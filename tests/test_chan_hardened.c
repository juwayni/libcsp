#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "csp.h"
#include "scheduler.h"
#include "chan.h"

void test_close_semantics() {
    printf("Testing channel close semantics...\n");
    csp_gochan_t *ch = csp_gochan_new(2);
    csp_gochan_send(ch, (void*)1);
    csp_gochan_send(ch, (void*)2);

    csp_gochan_close(ch);
    // Double close should not crash
    csp_gochan_close(ch);

    bool ok;
    void *val;
    val = csp_gochan_recv(ch, &ok);
    assert(val == (void*)1 && ok == true);

    val = csp_gochan_recv(ch, &ok);
    assert(val == (void*)2 && ok == true);

    val = csp_gochan_recv(ch, &ok);
    assert(val == NULL && ok == false);

    // Send on closed channel should return false
    bool sent = csp_gochan_send(ch, (void*)3);
    assert(sent == false);

    printf("Channel close semantics OK\n");
}

void test_select_fairness() {
    printf("Testing select fairness...\n");
    csp_gochan_t *ch1 = csp_gochan_new(10);
    csp_gochan_t *ch2 = csp_gochan_new(10);

    for (int i = 0; i < 10; i++) {
        csp_gochan_send(ch1, (void*)(uintptr_t)i);
        csp_gochan_send(ch2, (void*)(uintptr_t)i);
    }

    int count1 = 0, count2 = 0;
    for (int i = 0; i < 20; i++) {
        csp_select_case_t cases[2];
        void *val;
        cases[0].ch = ch1; cases[0].op = CSP_RECV; cases[0].val = &val;
        cases[1].ch = ch2; cases[1].op = CSP_RECV; cases[1].val = &val;

        int chosen = csp_select(cases, 2);
        if (chosen == 0) count1++;
        else count2++;
    }

    printf("Select distribution: ch1=%d, ch2=%d\n", count1, count2);
    // With 20 samples, it should be somewhat balanced. 0 and 20 would be highly suspicious.
    assert(count1 > 0 && count2 > 0);
    printf("Select fairness OK\n");
}

int main() {
    setenv("LIBCSP_PRODUCTION", "1", 1);
    csp_scheduler_init(1);

    test_close_semantics();
    test_select_fairness();

    printf("ALL CHAN HARDENED TESTS PASSED\n");
    return 0;
}
