#define _GNU_SOURCE
#include "sync.h"
#include "core.h"
#include "scheduler.h"
#include "proc_extra.h"
#include <stdlib.h>
#include <stdio.h>

extern void csp_core_yield(csp_proc_t *proc, void *anchor);

void csp_sync_mutex_init(csp_sync_mutex_t *mutex) {
    mutex->locked = 0;
    mutex->waiters_head = mutex->waiters_tail = NULL;
    pthread_mutex_init(&mutex->lock, NULL);
    pthread_cond_init(&mutex->cond, NULL);
}

void csp_sync_mutex_lock(csp_sync_mutex_t *mutex) {
    CSP_CRITICAL_START();
    pthread_mutex_lock(&mutex->lock);
    if (!mutex->locked) {
        mutex->locked = 1;
        pthread_mutex_unlock(&mutex->lock);
        CSP_CRITICAL_END();
        return;
    }

    csp_proc_t *self = (csp_proc_t *)csp_this_core->running;
    if (self) {
        csp_proc_stat_set(self, csp_proc_stat_blocked);
        self->next = NULL;
        if (mutex->waiters_tail) {
            mutex->waiters_tail->next = (struct csp_proc_s *)self;
            mutex->waiters_tail = (struct csp_proc_s *)self;
        } else {
            mutex->waiters_head = mutex->waiters_tail = (struct csp_proc_s *)self;
        }
        pthread_mutex_unlock(&mutex->lock);
        csp_core_yield(self, &csp_this_core->anchor);
        CSP_CRITICAL_END();
    } else {
        while (mutex->locked) {
            pthread_cond_wait(&mutex->cond, &mutex->lock);
        }
        mutex->locked = 1;
        pthread_mutex_unlock(&mutex->lock);
        CSP_CRITICAL_END();
    }
}

void csp_sync_mutex_unlock(csp_sync_mutex_t *mutex) {
    CSP_CRITICAL_START();
    pthread_mutex_lock(&mutex->lock);
    if (mutex->waiters_head) {
        csp_proc_t *proc = (csp_proc_t *)mutex->waiters_head;
        mutex->waiters_head = (struct csp_proc_s *)proc->next;
        if (!mutex->waiters_head) mutex->waiters_tail = NULL;
        pthread_mutex_unlock(&mutex->lock);
        csp_scheduler_submit(proc);
    } else {
        mutex->locked = 0;
        pthread_cond_signal(&mutex->cond);
        pthread_mutex_unlock(&mutex->lock);
    }
    CSP_CRITICAL_END();
}

void csp_sync_waitgroup_init(csp_sync_waitgroup_t *wg) {
    atomic_store(&wg->counter, 0);
    wg->waiters_head = wg->waiters_tail = NULL;
    pthread_mutex_init(&wg->lock, NULL);
    pthread_cond_init(&wg->cond, NULL);
}

void csp_sync_waitgroup_add(csp_sync_waitgroup_t *wg, int delta) {
    CSP_CRITICAL_START();
    long val = atomic_fetch_add(&wg->counter, delta) + delta;
    if (val < 0) {
        fprintf(stderr, "panic: negative WaitGroup counter\n");
        abort();
    }
    if (val == 0) {
        pthread_mutex_lock(&wg->lock);
        csp_proc_t *q = (csp_proc_t *)wg->waiters_head;
        wg->waiters_head = wg->waiters_tail = NULL;
        pthread_cond_broadcast(&wg->cond);
        pthread_mutex_unlock(&wg->lock);
        while (q) {
            csp_proc_t *next = (csp_proc_t *)q->next;
            csp_scheduler_submit(q);
            q = next;
        }
    }
    CSP_CRITICAL_END();
}

void csp_sync_waitgroup_done(csp_sync_waitgroup_t *wg) {
    csp_sync_waitgroup_add(wg, -1);
}

void csp_sync_waitgroup_wait(csp_sync_waitgroup_t *wg) {
    if (atomic_load(&wg->counter) == 0) return;

    CSP_CRITICAL_START();
    pthread_mutex_lock(&wg->lock);
    if (atomic_load(&wg->counter) == 0) {
        pthread_mutex_unlock(&wg->lock);
        CSP_CRITICAL_END();
        return;
    }
    csp_proc_t *self = (csp_proc_t *)csp_this_core->running;
    if (self) {
        csp_proc_stat_set(self, csp_proc_stat_blocked);
        self->next = NULL;
        if (wg->waiters_tail) {
            wg->waiters_tail->next = (struct csp_proc_s *)self;
            wg->waiters_tail = (struct csp_proc_s *)self;
        } else {
            wg->waiters_head = wg->waiters_tail = (struct csp_proc_s *)self;
        }
        pthread_mutex_unlock(&wg->lock);
        csp_core_yield(self, &csp_this_core->anchor);
        CSP_CRITICAL_END();
    } else {
        while (atomic_load(&wg->counter) > 0) {
            pthread_cond_wait(&wg->cond, &wg->lock);
        }
        pthread_mutex_unlock(&wg->lock);
        CSP_CRITICAL_END();
    }
}
