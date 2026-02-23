#ifndef LIBCSP_SYNC_H
#define LIBCSP_SYNC_H

#include "platform.h"
#include "proc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    atomic_int locked;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct csp_proc_s *waiters_head;
    struct csp_proc_s *waiters_tail;
} csp_sync_mutex_t;

void csp_sync_mutex_init(csp_sync_mutex_t *m);
void csp_sync_mutex_lock(csp_sync_mutex_t *m);
bool csp_sync_mutex_try_lock(csp_sync_mutex_t *m);
void csp_sync_mutex_unlock(csp_sync_mutex_t *m);

typedef struct {
    atomic_long counter;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct csp_proc_s *waiters_head;
    struct csp_proc_s *waiters_tail;
} csp_sync_waitgroup_t;

void csp_sync_waitgroup_init(csp_sync_waitgroup_t *wg);
void csp_sync_waitgroup_add(csp_sync_waitgroup_t *wg, int delta);
void csp_sync_waitgroup_done(csp_sync_waitgroup_t *wg);
void csp_sync_waitgroup_wait(csp_sync_waitgroup_t *wg);

#ifdef __cplusplus
}
#endif

#endif
