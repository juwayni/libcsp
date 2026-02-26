#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "csp.h"

void worker_done(void *arg) {
    csp_gochan_t *done = (csp_gochan_t *)arg;
    csp_gochan_send(done, (void *)1);
}

void bench_goroutine_creation(int n) {
    printf("Benchmarking goroutine creation with n=%d\n", n);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    csp_gochan_t *done = csp_gochan_new(n);
    for (int i = 0; i < n; i++) {
        csp_proc_create(0, worker_done, done);
    }
    printf("All goroutines created, waiting for completion...\n");
    for (int i = 0; i < n; i++) {
        csp_gochan_recv(done, NULL);
        if (i > 0 && i % 1000 == 0) printf("Completed %d/%d\n", i, n);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Goroutine creation (%d): %.3fs (%.3f us per op)\n", n, elapsed, (elapsed / n) * 1e6);
}

typedef struct {
    csp_gochan_t *c1;
    csp_gochan_t *c2;
    int n;
} ping_pong_args_t;

void ping_pong_worker(void *arg) {
    ping_pong_args_t *args = (ping_pong_args_t *)arg;
    for (int i = 0; i < args->n; i++) {
        csp_gochan_send(args->c1, (void *)1);
        csp_gochan_recv(args->c2, NULL);
    }
}

void bench_channel_ping_pong(int n) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    csp_gochan_t *c1 = csp_gochan_new(0);
    csp_gochan_t *c2 = csp_gochan_new(0);

    ping_pong_args_t args = {c1, c2, n};
    csp_proc_create(0, ping_pong_worker, &args);

    for (int i = 0; i < n; i++) {
        csp_gochan_recv(c1, NULL);
        csp_gochan_send(c2, (void *)1);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Channel Ping-Pong (%d): %.3fs (%.3f us per op)\n", n, elapsed, (elapsed / n) * 1e6);
}

void real_main(void *arg) {
    printf("--- libcsp Benchmarks ---\n");
    bench_goroutine_creation(100000);
    bench_channel_ping_pong(100000);
    printf("Benchmarks completed successfully\n");
    exit(0);
}

int main() {
    setenv("LIBCSP_PRODUCTION", "1", 1);
    // Preemption might add overhead, we keep it enabled for realistic benchmark
    setenv("LIBCSP_PREEMPT", "1", 1);

    csp_proc_create(0, real_main, NULL);

    extern _Thread_local csp_core_t *csp_this_core;
    extern void *csp_core_run(void *data);
    csp_core_run(csp_this_core);
    return 0;
}
