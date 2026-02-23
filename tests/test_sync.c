#include <stdio.h>
#define csp_without_prefix
#include <libcsp/csp.h>
#include <libcsp/sync.h>

csp_sync_mutex_t mu;
int counter = 0;

typedef struct {
    csp_sync_waitgroup_t *wg;
} worker_args;

void worker(void *arg) {
    worker_args *args = (worker_args *)arg;
    for (int i = 0; i < 1000; i++) {
        csp_sync_mutex_lock(&mu);
        counter++;
        csp_sync_mutex_unlock(&mu);
    }
    csp_sync_waitgroup_done(args->wg);
}

int main() {
    csp_sync_mutex_init(&mu);
    csp_sync_waitgroup_t wg;
    csp_sync_waitgroup_init(&wg);

    worker_args args[10];
    for (int i = 0; i < 10; i++) {
        args[i].wg = &wg;
        csp_sync_waitgroup_add(&wg, 1);
        async(worker(&args[i]));
    }

    csp_sync_waitgroup_wait(&wg);
    printf("Counter: %d (expected 10000)\n", counter);
    if (counter == 10000) {
        printf("Sync test passed!\n");
    } else {
        printf("Sync test failed!\n");
    }
    return 0;
}
