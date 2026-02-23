#ifndef LIBCSP_SCHEDULER_H
#define LIBCSP_SCHEDULER_H

#include "platform.h"
#include "proc.h"
#include "runq.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct csp_scheduler_s {
    int num_workers;
    struct csp_worker_s **workers;
    csp_grunq_t *global_runq;
    _Alignas(64) atomic_int num_procs;
    _Alignas(64) pthread_mutex_t lock;
    pthread_cond_t cond;
    _Alignas(64) int idle_workers;

    pthread_t preempter_tid;
    atomic_bool stop_preempter;
} csp_scheduler_t;

extern csp_scheduler_t *csp_global_scheduler;

void csp_scheduler_init(int num_workers);
void csp_scheduler_stop(void);
void csp_scheduler_submit(csp_proc_t *proc);
csp_proc_t *csp_scheduler_get_work(int worker_id);

csp_proc_t *csp_proc_create(int stack_id, void (*func)(void *), void *arg);

#ifdef __cplusplus
}
#endif

#endif
