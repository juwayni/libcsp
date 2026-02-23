#include <stdio.h>
#include <stdlib.h>
#include "csp.h"
#include "scheduler.h"

void producer(void *arg) {
    csp_gochan_t *c = (csp_gochan_t *)arg;
    for (int i = 0; i < 5; i++) {
        csp_hangup(100 * csp_timer_millisecond);
        printf("Producer %p pushing %d\n", arg, i);
        fflush(stdout);
        csp_gochan_send(c, (void*)(uintptr_t)i);
    }
}

void select_task(void *arg) {
    csp_gochan_t *c1 = ((csp_gochan_t**)arg)[0];
    csp_gochan_t *c2 = ((csp_gochan_t**)arg)[1];

    for (int i = 0; i < 10; i++) {
        void *val_ptr = NULL;
        int selected;
        SELECT_START
            RECV_CASE(c1, &val_ptr)
            RECV_CASE(c2, &val_ptr)
        SELECT_END(selected)

        printf("Selected %d, val %d\n", selected, (int)(uintptr_t)val_ptr);
        fflush(stdout);
    }

    printf("SUCCESS: test_select passed\n");
    exit(0);
}

int main() {
    setenv("LIBCSP_PRODUCTION", "1", 1);
    csp_scheduler_init(4);

    csp_gochan_t *c1 = csp_gochan_new(4);
    csp_gochan_t *c2 = csp_gochan_new(4);

    csp_proc_create(0, producer, c1);
    csp_proc_create(0, producer, c2);

    csp_gochan_t *chans[2] = {c1, c2};
    csp_proc_create(0, select_task, chans);

    while(1) sleep(1);
    return 0;
}
