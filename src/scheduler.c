#include "scheduler.h"
#include "worker.h"
#include "core.h"
#include "proc.h"
#include "proc_extra.h"
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>

csp_scheduler_t *csp_global_scheduler = NULL;
static pthread_once_t scheduler_init_once = PTHREAD_ONCE_INIT;

extern size_t csp_cpu_cores;
extern size_t csp_procs_num;
extern size_t csp_procs_size[];

extern csp_proc_t *csp_proc_new(int id, bool waited_by_parent);
extern void csp_sched_yield(void);
extern void csp_core_proc_exit(void);

extern void csp_async_preempt(void);

static void preemption_handler(int sig, siginfo_t *si, void *uc) {
    ucontext_t *ctx = (ucontext_t *)uc;
    if (!csp_this_core || !csp_this_core->running) return;

    csp_proc_t *proc = (csp_proc_t *)csp_this_core->running;
    if (!proc->extra) return;
    csp_proc_extra_t *extra = (csp_proc_extra_t *)proc->extra;

    if (extra->preemptible && !extra->in_critical_section) {
        extra->preemptible = false; // Disable until safely enqueued
        greg_t old_rip = ctx->uc_mcontext.gregs[REG_RIP];
        ctx->uc_mcontext.gregs[REG_RSP] -= 8;
        *(greg_t *)(ctx->uc_mcontext.gregs[REG_RSP]) = old_rip;
        ctx->uc_mcontext.gregs[REG_RIP] = (greg_t)csp_async_preempt;
    }
}

static void *preempter_loop(void *arg) {
    while (!atomic_load(&csp_global_scheduler->stop_preempter)) {
        usleep(10000); // 10ms
        for (int i = 0; i < csp_global_scheduler->num_workers; i++) {
            if (csp_global_scheduler->workers[i]->tid) {
                pthread_kill(csp_global_scheduler->workers[i]->tid, SIGALRM);
            }
        }
    }
    return NULL;
}

static void scheduler_init_internal(void) {
    int num_workers = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (num_workers <= 0) num_workers = 1;

    csp_global_scheduler = (csp_scheduler_t *)calloc(1, sizeof(csp_scheduler_t));
    csp_global_scheduler->num_workers = num_workers;
    csp_global_scheduler->workers = (csp_worker_t **)calloc(num_workers, sizeof(csp_worker_t *));
    csp_global_scheduler->global_runq = csp_grunq_new(18); // 2^18 (approx 256K)

    pthread_mutex_init(&csp_global_scheduler->lock, NULL);
    pthread_cond_init(&csp_global_scheduler->cond, NULL);

    // Set up signal alt stack for Worker 0 (main thread)
    stack_t ss;
    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    // Worker 0 is the main thread
    csp_global_scheduler->workers[0] = (csp_worker_t *)calloc(1, sizeof(csp_worker_t));
    csp_global_scheduler->workers[0]->id = 0;
    csp_global_scheduler->workers[0]->tid = pthread_self();
    csp_global_scheduler->workers[0]->core = csp_this_core;
    if (csp_this_core) csp_this_core->worker = csp_global_scheduler->workers[0];

    for (int i = 1; i < num_workers; i++) {
        csp_global_scheduler->workers[i] = csp_worker_new(i);
        csp_worker_start(csp_global_scheduler->workers[i]);
    }

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sa.sa_sigaction = preemption_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    atomic_store(&csp_global_scheduler->stop_preempter, false);
    if (getenv("LIBCSP_PREEMPT")) {
        pthread_t preempter;
        pthread_create(&preempter, NULL, preempter_loop, NULL);
        pthread_detach(preempter);
    }
}

void csp_scheduler_enqueue(csp_proc_t *proc) {
    while (!csp_grunq_try_push(csp_global_scheduler->global_runq, proc)) {
        sched_yield();
    }
    pthread_mutex_lock(&csp_global_scheduler->lock);
    if (csp_global_scheduler->idle_workers > 0) {
        pthread_cond_signal(&csp_global_scheduler->cond);
    }
    pthread_mutex_unlock(&csp_global_scheduler->lock);
}

void csp_scheduler_init(int num_workers) {
    pthread_once(&scheduler_init_once, scheduler_init_internal);
}

void csp_scheduler_submit(csp_proc_t *proc) {
    if (!csp_global_scheduler) {
        csp_scheduler_init(0);
    }

    while (atomic_load(&proc->yielding)) {
        sched_yield();
    }

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &mask, &oldmask);

    // Block preemption during submission to avoid deadlocks
    csp_proc_t *self = csp_this_core ? (csp_proc_t *)csp_this_core->running : NULL;
    csp_proc_extra_t *ex = self ? (csp_proc_extra_t *)self->extra : NULL;
    if (ex) ex->in_critical_section++;

    // Atomic CAS to ensure only one thread enqueues this process
    while (1) {
        uint64_t old_stat = csp_proc_stat_get(proc);
        if (old_stat == csp_proc_stat_runnable) break; // Already in queue or about to be
        if (csp_proc_stat_cas(proc, old_stat, csp_proc_stat_runnable)) {
            csp_scheduler_enqueue(proc);
            break;
        }
    }

    if (ex) ex->in_critical_section--;
    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
}

csp_proc_t *csp_scheduler_get_work(int worker_id) {
    if (!csp_global_scheduler) return NULL;

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &mask, &oldmask);

    csp_proc_t *proc = NULL;
    uint64_t expected;

    if (csp_grunq_try_pop(csp_global_scheduler->global_runq, &proc)) {
        goto found;
    }
    // Note: lrunq is not thread-safe for stealing. Rely on global queue for now.
    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    return NULL;

found:
    expected = csp_proc_stat_runnable;
    if (!csp_proc_stat_cas(proc, expected, csp_proc_stat_running)) {
        // Already picked by someone else? (Should not happen with grunq/lrunq exclusive pop)
        // But better be safe.
        pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
        return NULL;
    }
    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    return proc;
}

csp_proc_t *csp_proc_create(int stack_id, void (*func)(void *), void *arg) {
    csp_proc_t *proc = csp_proc_new(stack_id, false);
    proc->registers.caller_saved.rdi = (uintptr_t)arg;
    ((csp_proc_extra_t *)proc->extra)->preemptible = true;

    uintptr_t *stack = (uintptr_t *)proc->rbp;
    *(--stack) = (uintptr_t)csp_core_proc_exit;
    *(--stack) = (uintptr_t)func;
    proc->rsp = (uintptr_t)stack;

    csp_scheduler_submit(proc);
    return proc;
}
