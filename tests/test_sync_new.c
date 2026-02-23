#include "csp.h"
#include <stdio.h>
#include <assert.h>

mutex_t mu;
int counter = 0;
waitgroup_t wg;

void worker(void* arg) {
    for (int i = 0; i < 1000; i++) {
        mutex_lock(&mu);
        counter++;
        mutex_unlock(&mu);
    }
    wg_done(&wg);
}

int main() {
    // Set environment to use new scheduler
    setenv("LIBCSP_PRODUCTION", "1", 1);

    mutex_init(&mu);
    wg_init(&wg);

    int num_workers = 10;
    wg_add(&wg, num_workers);

    for (int i = 0; i < num_workers; i++) {
        csp_proc_create(0, worker, NULL);
    }

    wg_wait(&wg);

    printf("Final counter: %d\n", counter);
    assert(counter == num_workers * 1000);
    printf("Sync test passed!\n");

    return 0;
}
