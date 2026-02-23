#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csp.h"
#include "scheduler.h"
#include "chan.h"
#include "sync.h"
#include "timer.h"
#include "context.h"
#include <unistd.h>

typedef struct {
    int id;
    csp_gochan_t *jobs;
    csp_gochan_t *results;
    csp_sync_waitgroup_t *wg;
    csp_context_t *ctx;
} worker_arg_t;

void worker_proc(void *arg) {
    worker_arg_t *wa = (worker_arg_t *)arg;
    printf("[Worker %d] starting\n", wa->id); fflush(stdout);

    while (true) {
        csp_select_case_t cases[2];
        void *job_ptr = NULL;

        cases[0].ch = csp_context_done(wa->ctx);
        cases[0].op = CSP_RECV;
        cases[0].val = NULL;

        cases[1].ch = wa->jobs;
        cases[1].op = CSP_RECV;
        cases[1].val = &job_ptr;

        int chosen = csp_select(cases, 2);
        if (chosen == 0) {
            printf("[Worker %d] received cancellation\n", wa->id); fflush(stdout);
            break;
        } else {
            if (job_ptr == (void*)0xDEADBEEF) {
                printf("[Worker %d] jobs channel sentinel received\n", wa->id); fflush(stdout);
                break;
            }

            int job_val = (int)(uintptr_t)job_ptr;
            printf("[Worker %d] processing job %d\n", wa->id, job_val); fflush(stdout);
            csp_hangup(100 * csp_timer_millisecond); // Simulate work
            int res = job_val * 2;
            csp_gochan_send(wa->results, (void *)(uintptr_t)res);
        }
    }

    printf("[Worker %d] done\n", wa->id); fflush(stdout);
    csp_sync_waitgroup_done(wa->wg);
}

void ticker_proc(void *arg) {
    csp_ticker_t *ticker = csp_ticker_new(200 * csp_timer_millisecond);
    for (int i = 0; i < 5; i++) {
        csp_gochan_recv(ticker->ch, NULL);
        printf("[Ticker] tick %d\n", i); fflush(stdout);
    }
    csp_ticker_stop(ticker);
    printf("[Ticker] stopped\n"); fflush(stdout);
}

void real_main(void *arg) {
    printf("--- REAL MAIN STARTING ---\n"); fflush(stdout);

    csp_context_t *bg_ctx = csp_context_background();
    csp_context_t *cancel_ctx = csp_context_with_cancel(bg_ctx);

    csp_gochan_t *jobs = csp_gochan_new(10);
    csp_gochan_t *results = csp_gochan_new(10);
    csp_sync_waitgroup_t wg;
    csp_sync_waitgroup_init(&wg);

    int num_workers = 3;
    worker_arg_t args[3];
    for (int i = 0; i < num_workers; i++) {
        args[i].id = i;
        args[i].jobs = jobs;
        args[i].results = results;
        args[i].wg = &wg;
        args[i].ctx = cancel_ctx;
        csp_sync_waitgroup_add(&wg, 1);
        csp_proc_create(0, worker_proc, &args[i]);
    }

    csp_proc_create(0, ticker_proc, NULL);

    // Send 10 jobs
    for (int i = 0; i < 10; i++) {
        csp_gochan_send(jobs, (void *)(uintptr_t)i);
    }

    // Collect 10 results with timeout on each
    for (int i = 0; i < 10; i++) {
        csp_select_case_t cases[2];
        void *res_ptr = NULL;
        cases[0].ch = results;
        cases[0].op = CSP_RECV;
        cases[0].val = &res_ptr;

        cases[1].ch = csp_time_after(1000 * csp_timer_millisecond);
        cases[1].op = CSP_RECV;
        cases[1].val = NULL;

        int chosen = csp_select(cases, 2);
        if (chosen == 0) {
            printf("[Main] received result: %d\n", (int)(uintptr_t)res_ptr); fflush(stdout);
        } else {
            printf("[Main] timeout receiving result %d\n", i); fflush(stdout);
        }
    }

    printf("[Main] canceling context to stop workers\n"); fflush(stdout);
    csp_context_cancel(cancel_ctx);

    // Send sentinel to break workers waiting on jobs channel if they are not yet canceled
    for(int i=0; i<num_workers; i++) {
        csp_gochan_try_send(jobs, (void*)0xDEADBEEF);
    }

    csp_sync_waitgroup_wait(&wg);
    printf("--- GRAND EXAMPLE FINISHED ---\n"); fflush(stdout);

    // Final check on Atomics
    atomic_int_fast64_t counter;
    atomic_init(&counter, 0);
    atomic_fetch_add(&counter, 42);
    if (atomic_load(&counter) == 42) {
        printf("Atomics OK\n"); fflush(stdout);
    }

    exit(0);
}

int main() {
    printf("--- STARTING GRAND EXAMPLE WRAPPER ---\n"); fflush(stdout);
    setenv("LIBCSP_PRODUCTION", "1", 1);
    csp_scheduler_init(4);

    csp_proc_create(0, real_main, NULL);

    while(1) sleep(1);
    return 0;
}
