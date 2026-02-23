#include "csp.h"
#include <stdio.h>
#include <unistd.h>

void worker(void* arg) {
    int id = (int)(uintptr_t)arg;
    printf("Worker %d started\n", id);
    for (int i = 0; i < 5; i++) {
        printf("Worker %d: %d\n", id, i);
        csp_sched_yield();
    }
    printf("Worker %d finished\n", id);
}

int main() {
    printf("Main started\n");
    for (int i = 0; i < 3; i++) {
        csp_proc_create(0, worker, (void*)(uintptr_t)i);
    }
    printf("Main sleeping\n");
    sleep(1);
    printf("Main finished\n");
    return 0;
}
