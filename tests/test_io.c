#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "csp.h"
#include "scheduler.h"
#include "netpoll.h"
#include "timer.h"

void writer(void *arg) {
    int fd = *(int *)arg;
    char *msg = "Hello from Go-class C runtime!";
    csp_hangup(500 * csp_timer_millisecond);
    printf("Writer sending: %s\n", msg); fflush(stdout);
    csp_write(fd, msg, strlen(msg));
    printf("Writer done\n"); fflush(stdout);
}

void reader(void *arg) {
    int fd = *(int *)arg;
    char buf[128];
    memset(buf, 0, sizeof(buf));
    printf("Reader waiting...\n"); fflush(stdout);
    ssize_t n = csp_read(fd, buf, sizeof(buf));
    printf("Reader received %ld bytes: %s\n", (long)n, buf); fflush(stdout);
    if (n > 0 && strcmp(buf, "Hello from Go-class C runtime!") == 0) {
        printf("SUCCESS: Non-blocking I/O worked\n"); fflush(stdout);
        exit(0);
    }
}

int main() {
    setenv("LIBCSP_PRODUCTION", "1", 1);
    csp_scheduler_init(4);

    int p[2];
    if (pipe(p) < 0) { perror("pipe"); return 1; }

    csp_netpoll_register(p[0]);
    csp_netpoll_register(p[1]);

    int fd_r = p[0];
    int fd_w = p[1];

    csp_proc_create(0, reader, &fd_r);
    csp_proc_create(0, writer, &fd_w);

    sleep(2);
    printf("FAILED: I/O test timed out\n"); fflush(stdout);
    return 1;
}
