#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "csp.h"
#include "sync.h"
#include "context.h"
#include "runtime.h"
#include "scheduler.h"

typedef struct {
    long id;
    csp_gochan_t *jobs;
    csp_gochan_t *results;
} worker_args_t;

void worker(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;
    printf("[Worker %ld] starting\n", args->id);

    while (1) {
        bool ok;
        void *job = csp_gochan_recv(args->jobs, &ok);
        if (!ok) {
            printf("[Worker %ld] jobs channel closed, exiting\n", args->id);
            break;
        }
        printf("[Worker %ld] processing job %ld\n", args->id, (long)job);
        usleep(100000); // Simulate work
        csp_gochan_send(args->results, job);
    }
}

void ticker_proc(void *arg) {
    csp_gochan_t *ticker_ch = (csp_gochan_t *)arg;
    int count = 0;
    while (count < 5) {
        csp_gochan_recv(ticker_ch, NULL);
        count++;
        printf("Tick %d\n", count);
    }
    printf("Ticker proc done\n");
}

typedef struct {
    csp_sync_waitgroup_t *wg;
    csp_sync_mutex_t *mu;
    _Atomic int *counter;
} task_args_t;

void task(void *arg) {
    task_args_t *args = (task_args_t *)arg;
    csp_sync_mutex_lock(args->mu);
    atomic_fetch_add(args->counter, 1);
    csp_sync_mutex_unlock(args->mu);
    csp_sync_waitgroup_done(args->wg);
}

void cancel_task(void *a) {
    csp_context_t *c = (csp_context_t *)a;
    printf("Cancellable goroutine waiting...\n");
    csp_gochan_recv(csp_context_done(c), NULL);
    printf("Cancellable goroutine saw cancellation!\n");
}

void real_main(void *arg) {
    printf("--- REAL MAIN STARTING ---\n");

    // 1. Worker Pool
    csp_gochan_t *jobs = csp_gochan_new(0);
    csp_gochan_t *results = csp_gochan_new(10);

    worker_args_t wargs[3];
    for (int i = 0; i < 3; i++) {
        wargs[i].id = i;
        wargs[i].jobs = jobs;
        wargs[i].results = results;
        csp_proc_create(0, worker, &wargs[i]);
    }

    for (int i = 0; i < 5; i++) {
        csp_gochan_send(jobs, (void*)(long)i);
    }
    csp_gochan_close(jobs);

    for (int i = 0; i < 5; i++) {
        void *res = csp_gochan_recv(results, NULL);
        printf("Main got result: %ld\n", (long)res);
    }

    // 2. WaitGroup & Mutex & Atomics
    csp_sync_waitgroup_t wg;
    csp_sync_waitgroup_init(&wg);
    csp_sync_mutex_t mu;
    csp_sync_mutex_init(&mu);
    _Atomic int counter = 0;

    task_args_t targs = {&wg, &mu, &counter};

    for (int i = 0; i < 10; i++) {
        csp_sync_waitgroup_add(&wg, 1);
        csp_proc_create(0, task, &targs);
    }
    csp_sync_waitgroup_wait(&wg);
    printf("Counter: %d (expected 10)\n", atomic_load(&counter));

    // 3. Ticker & Select & Timeout
    csp_ticker_t *ticker = ticker_new(200000000); // 200ms
    csp_proc_create(0, ticker_proc, ticker->ch);

    csp_gochan_t *timeout = time_after(1500000000); // 1.5s
    bool running = true;
    while (running) {
        csp_select_case_t cases[2];
        cases[0].op = CSP_RECV;
        cases[0].ch = ticker->ch;
        cases[0].val = NULL;

        cases[1].op = CSP_RECV;
        cases[1].ch = timeout;
        cases[1].val = NULL;

        int chosen = csp_select(cases, 2);
        if (chosen == 0) {
            printf("Main: select got tick\n");
        } else {
            printf("Main: select got timeout, stopping\n");
            running = false;
        }
    }
    ticker_stop(ticker);

    // 4. Context Cancellation
    csp_context_t *ctx = csp_context_background();
    csp_context_t *cancel_ctx = csp_context_with_cancel(ctx);

    csp_proc_create(0, cancel_task, cancel_ctx);

    usleep(500000); // 0.5s
    printf("Main cancelling context...\n");
    csp_context_cancel(cancel_ctx);
    usleep(100000);

    printf("--- GRAND EXAMPLE COMPLETE ---\n");
    exit(0);
}

int main() {
    printf("--- STARTING GRAND EXAMPLE WRAPPER ---\n");
    setenv("LIBCSP_PRODUCTION", "1", 1);
    setenv("LIBCSP_PREEMPT", "1", 1);

    csp_proc_create(0, real_main, NULL);

    extern _Thread_local csp_core_t *csp_this_core;
    extern void *csp_core_run(void *data);
    csp_core_run(csp_this_core);

    return 0;
}
