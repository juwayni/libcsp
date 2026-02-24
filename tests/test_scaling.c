#include <stdio.h>
#include <stdlib.h>
#define csp_without_prefix
#include <libcsp/csp.h>

void compute_task(void *arg) {
    int id = *(int *)arg;
    unsigned long long count = 0;
    for (int i = 0; i < 10000000; i++) {
        count += i;
    }
    printf("Task %d finished, result irrelevant\n", id);
    free(arg);
}

int main() {
    int n = 100;
    for (int i = 0; i < n; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        async(compute_task(id));
    }

    sched.hangup(2 * csp_timer_second);
    printf("Main finished\n");
    return 0;
}
