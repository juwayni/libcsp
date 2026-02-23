#ifndef LIBCSP_WORKER_H
#define LIBCSP_WORKER_H

#include "platform.h"
#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct csp_worker_s {
    int id;
    pthread_t tid;
    csp_core_t *core;
} csp_worker_t;

csp_worker_t *csp_worker_new(int id);
void csp_worker_start(csp_worker_t *worker);
void *csp_worker_loop(void *arg);

#ifdef __cplusplus
}
#endif

#endif
