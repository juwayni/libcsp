#include "csp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

void test_wg_panic() {
    printf("Testing WaitGroup negative counter panic...\n");
    waitgroup_t wg;
    wg_init(&wg);
    wg_add(&wg, 1);
    wg_done(&wg);

    // This should panic
    pid_t pid = fork();
    if (pid == 0) {
        // Child
        wg_done(&wg);
        exit(0);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) {
            printf("WaitGroup negative counter panic OK\n");
        } else {
            printf("FAILED: WaitGroup did not panic as expected\n");
            exit(1);
        }
    }
}

int main() {
    test_wg_panic();
    return 0;
}
