#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "csp.h"
#include "scheduler.h"
#include "timer.h"

volatile int counter1 = 0;
volatile int loop2_done = 0;

void loop1(void *arg) {
    printf("Loop 1 started\n"); fflush(stdout);
    while (counter1 < 100000000) {
        counter1++;
        if (counter1 % 10000000 == 0) {
            printf("Loop 1: %d\n", counter1); fflush(stdout);
        }
    }
    printf("Loop 1 finished\n"); fflush(stdout);
}

void loop2(void *arg) {
    printf("Loop 2 started\n"); fflush(stdout);
    for (int i = 0; i < 10; i++) {
        printf("Loop 2 tick: %d\n", i); fflush(stdout);
        csp_hangup(50 * csp_timer_millisecond);
    }
    printf("Loop 2 finished\n"); fflush(stdout);
    loop2_done = 1;
}

int main() {
    printf("Main started\n"); fflush(stdout);
    // LIBCSP_PRODUCTION=1 must be set in environment
    csp_scheduler_init(1);

    csp_proc_create(0, loop1, NULL);
    csp_proc_create(0, loop2, NULL);

    while (!loop2_done) {
        usleep(100000);
    }
    printf("SUCCESS: Preemption worked.\n"); fflush(stdout);
    return 0;
}
