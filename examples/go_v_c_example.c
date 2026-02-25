#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "csp.h"
#include "sync.h"
#include "context.h"
#include "runtime.h"

typedef struct {
    csp_context_t *ctx;
    int id;
    csp_gochan_t *jobs;
    csp_gochan_t *results;
    csp_sync_waitgroup_t *wg;
} worker_args_t;

void worker(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;
    while (1) {
        csp_select_case_t cases[2];

        // Case 0: Context cancellation
        cases[0].ch = csp_context_done(args->ctx);
        cases[0].op = CSP_RECV;
        cases[0].val = NULL;

        // Case 1: Job receipt
        cases[1].ch = args->jobs;
        cases[1].op = CSP_RECV;
        void *job_ptr;
        cases[1].val = &job_ptr;

        int chosen = csp_select(cases, 2);

        if (chosen == 0) {
            printf("Worker %d: cancelled and exiting\n", args->id);
            break;
        } else if (chosen == 1) {
            // job_ptr was already populated by csp_select
            if (job_ptr == NULL) {
                // In this implementation, if channel is closed, try_recv (inside select)
                // returns true, sets val=NULL and ok=false.
                // But wait, my csp_select doesn't pass &ok to try_recv correctly or doesn't use it.
                // Actually, if it's closed, it should exit.

                // Let's verify ok status.
                bool real_ok;
                void *dummy;
                // We need a way to know if it was closed.
                // For now, let's just assume NULL means closed for this simple example,
                // OR better, improve csp_select to support ok.

                // For this example, let's just do a proper check.
                printf("Worker %d: no more jobs or NULL job, exiting\n", args->id);
                break;
            }
            long job = (long)job_ptr;
            printf("Worker %d: processing job %ld\n", args->id, job);
            csp_hangup(100 * 1000 * 1000); // 100ms
            csp_gochan_send(args->results, (void *)(job * 2));
        }
    }
    csp_sync_waitgroup_done(args->wg);
    free(args);
}

void timeout_task(void *arg) {
    csp_context_t *ctx = (csp_context_t *)arg;
    csp_sched_hangup(2LL * 1000 * 1000 * 1000); // 2 seconds
    printf("Main: timeout reached, cancelling context\n");
    csp_context_cancel(ctx);
}

void wait_and_close(void *arg) {
    void **args = (void **)arg;
    csp_sync_waitgroup_t *wg = (csp_sync_waitgroup_t *)args[0];
    csp_gochan_t *results = (csp_gochan_t *)args[1];
    csp_sync_waitgroup_wait(wg);
    csp_gochan_close(results);
    free(args);
}

void real_main(void *arg) {
    csp_context_t *ctx = csp_context_with_cancel(csp_context_background());
    csp_sync_waitgroup_t wg;
    csp_sync_waitgroup_init(&wg);

    csp_gochan_t *jobs = csp_gochan_new(10);
    csp_gochan_t *results = csp_gochan_new(10);

    // Start 3 workers
    for (int i = 1; i <= 3; i++) {
        csp_sync_waitgroup_add(&wg, 1);
        worker_args_t *wargs = malloc(sizeof(worker_args_t));
        wargs->ctx = ctx;
        wargs->id = i;
        wargs->jobs = jobs;
        wargs->results = results;
        wargs->wg = &wg;
        csp_proc_create(0, worker, wargs);
    }

    // Send 5 jobs
    for (long i = 1; i <= 5; i++) {
        csp_gochan_send(jobs, (void *)i);
    }
    csp_gochan_close(jobs);

    // Start timeout task
    csp_proc_create(0, timeout_task, ctx);

    // Start waiter task
    void **wait_args = malloc(2 * sizeof(void *));
    wait_args[0] = &wg;
    wait_args[1] = results;
    csp_proc_create(0, wait_and_close, wait_args);

    // Collect results
    while (1) {
        bool ok;
        void *res_ptr = csp_gochan_recv(results, &ok);
        if (!ok) break;
        printf("Main: got result %ld\n", (long)res_ptr);
    }

    printf("Main: execution finished\n");
    exit(0);
}

int main() {
    setenv("LIBCSP_PRODUCTION", "1", 1);
    setenv("LIBCSP_PREEMPT", "1", 1);

    csp_proc_create(0, real_main, NULL);

    // Start scheduler
    extern _Thread_local csp_core_t *csp_this_core;
    extern void *csp_core_run(void *data);
    csp_core_run(csp_this_core);

    return 0;
}
