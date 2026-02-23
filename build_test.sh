#!/bin/bash
gcc -O3 -Isrc -I. -D_GNU_SOURCE \
    src/core.c src/corepool.c src/mem.c src/monitor.c src/netpoll.c src/proc.c src/rand.c src/runq.c src/sched.c src/timer.c \
    src/scheduler.c src/worker.c src/sync.c src/chan.c src/context.c src/runtime.c \
    tests/manual_config.c tests/test_simple_original.c \
    -pthread -lm -o simple_new_runtime
