#include <stdio.h>
#define csp_without_prefix
#include <libcsp/csp.h>

void task(void *arg) {
    printf("Task running\n");
}

int main() {
    for (int i = 0; i < 4; i++) {
        async(task(NULL));
    }
    csp_sched_hangup(100 * csp_timer_millisecond);
    return 0;
}
