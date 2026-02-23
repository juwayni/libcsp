#define _GNU_SOURCE
#include "worker.h"
#include "scheduler.h"
#include "core.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

extern void csp_core_init_main(csp_core_t *core);
extern void *csp_core_run(void *data);
extern csp_core_t *csp_core_pool_get(size_t pid);

csp_worker_t *csp_worker_new(int id) {
    csp_worker_t *worker = (csp_worker_t *)calloc(1, sizeof(csp_worker_t));
    worker->id = id;
    worker->core = csp_core_pool_get(id);
    worker->core->worker = worker;
    return worker;
}

void csp_worker_start(csp_worker_t *worker) {
    pthread_create(&worker->tid, NULL, csp_worker_loop, worker);
}

void *csp_worker_loop(void *arg) {
    csp_worker_t *worker = (csp_worker_t *)arg;
    csp_core_run(worker->core);
    return NULL;
}
