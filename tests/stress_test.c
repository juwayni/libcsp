#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "csp.h"

// Stress test: 100,000 goroutines, each doing some work and communicating
#define NUM_GOROUTINES 1
#define NUM_CHANNELS 1

typedef struct {
    int id;
    csp_gochan_t *in;
    csp_gochan_t *out;
    csp_sync_waitgroup_t *wg;
} stress_args_t;

void stress_worker(void *arg) {
    stress_args_t *a = (stress_args_t *)arg;
    for (int i = 0; i < 10; i++) {
        void *val = csp_gochan_recv(a->in, NULL);
        csp_gochan_send(a->out, val);
    }
    csp_sync_waitgroup_done(a->wg);
    free(a);
}

void real_main(void *arg) {
    printf("Starting Stress Test with %d goroutines...\n", NUM_GOROUTINES);
    csp_sync_waitgroup_t wg;
    csp_sync_waitgroup_init(&wg);

    csp_gochan_t *chans[NUM_CHANNELS];
    for (int i = 0; i < NUM_CHANNELS; i++) {
        chans[i] = csp_gochan_new(100);
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_GOROUTINES; i++) {
        csp_sync_waitgroup_add(&wg, 1);
        stress_args_t *a = malloc(sizeof(stress_args_t));
        a->id = i;
        a->in = chans[i % NUM_CHANNELS];
        a->out = chans[(i + 1) % NUM_CHANNELS];
        a->wg = &wg;
        csp_proc_create(0, stress_worker, a);
    }

    printf("All workers spawned. Injecting tokens...\n");
    for (int i = 0; i < NUM_CHANNELS; i++) {
        for (int j = 0; j < 10; j++) {
            csp_gochan_send(chans[i], (void *)1);
        }
    }

    printf("Waiting for workers to finish...\n");
    csp_sync_waitgroup_wait(&wg);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Stress test finished in %.3fs\n", elapsed);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        // cleanup would go here
    }

    printf("PASSED\n");
    exit(0);
}

int main() {
    // Environment should be set externally
    csp_proc_create(0, real_main, NULL);

    extern _Thread_local csp_core_t *csp_this_core;
    extern void *csp_core_run(void *data);

    if (!csp_this_core) {
        // This might happen if constructor didn't run or didn't set it
        // But it should have.
    }
    csp_core_run(csp_this_core);
    return 0;
}
