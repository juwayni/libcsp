#include <sched.h>
#include "chan.h"
#include "core.h"
#include "scheduler.h"
#include "proc_extra.h"
#include <stdlib.h>
#include <string.h>

extern _Thread_local csp_core_t *csp_this_core;

csp_gochan_t *csp_gochan_new(size_t capacity) {
    csp_gochan_t *ch = (csp_gochan_t *)calloc(1, sizeof(csp_gochan_t));
    ch->capacity = capacity;
    if (capacity > 0) {
        ch->buffer = calloc(capacity, sizeof(void *));
    }
    pthread_mutex_init(&ch->lock, NULL);
    return ch;
}

void csp_gochan_close(csp_gochan_t *ch) {
    CSP_CRITICAL_START();
    pthread_mutex_lock(&ch->lock);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->lock);
        CSP_CRITICAL_END();
        return;
    }
    ch->closed = true;
    while (ch->recv_q) {
        csp_proc_t *p = (csp_proc_t *)ch->recv_q;
        ch->recv_q = (struct csp_proc_s *)p->next;
        csp_proc_extra_t *ex = (csp_proc_extra_t *)p->extra;
        if (ex) {
            ex->chan_ok = false;
            ex->chan_val = NULL;
        }
        csp_scheduler_submit(p);
    }
    while (ch->send_q) {
        csp_proc_t *p = (csp_proc_t *)ch->send_q;
        ch->send_q = (struct csp_proc_s *)p->next;
        csp_scheduler_submit(p);
    }
    pthread_mutex_unlock(&ch->lock);
    CSP_CRITICAL_END();
}

bool csp_gochan_send(csp_gochan_t *ch, void *val) {
    CSP_CRITICAL_START();
    pthread_mutex_lock(&ch->lock);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->lock);
        CSP_CRITICAL_END();
        return false;
    }

    if (ch->recv_q) {
        csp_proc_t *p = (csp_proc_t *)ch->recv_q;
        ch->recv_q = (struct csp_proc_s *)p->next;
        csp_proc_extra_t *ex = (csp_proc_extra_t *)p->extra;
        if (ex) {
            ex->chan_ok = true;
            ex->chan_val = val;
        }
        pthread_mutex_unlock(&ch->lock);
        csp_scheduler_submit(p);
        CSP_CRITICAL_END();
        return true;
    }

    if (ch->capacity > 0 && ch->size < ch->capacity) {
        ch->buffer[ch->head] = val;
        ch->head = (ch->head + 1) % ch->capacity;
        ch->size++;
        pthread_mutex_unlock(&ch->lock);
        CSP_CRITICAL_END();
        return true;
    }

    csp_proc_t *self = (csp_proc_t *)csp_this_core->running;
    csp_proc_stat_set(self, csp_proc_stat_blocked);
    self->next = (struct csp_proc_s *)ch->send_q;
    ch->send_q = (struct csp_proc_s *)self;
    csp_proc_extra_t *self_ex = (csp_proc_extra_t *)self->extra;
    if (self_ex) self_ex->chan_val = val;

    pthread_mutex_unlock(&ch->lock);
    csp_core_yield(self, &csp_this_core->anchor);
    CSP_CRITICAL_END();
    return true;
}

void *csp_gochan_recv(csp_gochan_t *ch, bool *ok) {
    CSP_CRITICAL_START();
    pthread_mutex_lock(&ch->lock);
    if (ch->size > 0) {
        void *val = ch->buffer[ch->tail];
        ch->tail = (ch->tail + 1) % ch->capacity;
        ch->size--;
        if (ch->send_q) {
            csp_proc_t *p = (csp_proc_t *)ch->send_q;
            ch->send_q = (struct csp_proc_s *)p->next;
            csp_proc_extra_t *ex = (csp_proc_extra_t *)p->extra;
            void *sval = ex ? ex->chan_val : NULL;
            ch->buffer[ch->head] = sval;
            ch->head = (ch->head + 1) % ch->capacity;
            ch->size++;
            csp_scheduler_submit(p);
        }
        pthread_mutex_unlock(&ch->lock);
        if (ok) *ok = true;
        CSP_CRITICAL_END();
        return val;
    }

    if (ch->closed) {
        pthread_mutex_unlock(&ch->lock);
        if (ok) *ok = false;
        CSP_CRITICAL_END();
        return NULL;
    }

    if (ch->send_q && ch->capacity == 0) {
        csp_proc_t *p = (csp_proc_t *)ch->send_q;
        ch->send_q = (struct csp_proc_s *)p->next;
        csp_proc_extra_t *ex = (csp_proc_extra_t *)p->extra;
        void *val = ex ? ex->chan_val : NULL;
        pthread_mutex_unlock(&ch->lock);
        csp_scheduler_submit(p);
        if (ok) *ok = true;
        CSP_CRITICAL_END();
        return val;
    }

    csp_proc_t *self = (csp_proc_t *)csp_this_core->running;
    csp_proc_stat_set(self, csp_proc_stat_blocked);
    self->next = (struct csp_proc_s *)ch->recv_q;
    ch->recv_q = (struct csp_proc_s *)self;
    pthread_mutex_unlock(&ch->lock);

    csp_core_yield(self, &csp_this_core->anchor);

    csp_proc_extra_t *self_ex = (csp_proc_extra_t *)self->extra;
    void *val = self_ex ? self_ex->chan_val : NULL;
    if (ok) *ok = self_ex ? self_ex->chan_ok : false;
    CSP_CRITICAL_END();
    return val;
}

bool csp_gochan_try_send(csp_gochan_t *ch, void *val) {
    CSP_CRITICAL_START();
    pthread_mutex_lock(&ch->lock);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->lock);
        CSP_CRITICAL_END();
        return false;
    }

    if (ch->recv_q) {
        csp_proc_t *p = (csp_proc_t *)ch->recv_q;
        ch->recv_q = (struct csp_proc_s *)p->next;
        csp_proc_extra_t *ex = (csp_proc_extra_t *)p->extra;
        if (ex) {
            ex->chan_ok = true;
            ex->chan_val = val;
        }
        pthread_mutex_unlock(&ch->lock);
        csp_scheduler_submit(p);
        CSP_CRITICAL_END();
        return true;
    }

    if (ch->capacity > 0 && ch->size < ch->capacity) {
        ch->buffer[ch->head] = val;
        ch->head = (ch->head + 1) % ch->capacity;
        ch->size++;
        pthread_mutex_unlock(&ch->lock);
        CSP_CRITICAL_END();
        return true;
    }

    pthread_mutex_unlock(&ch->lock);
    CSP_CRITICAL_END();
    return false;
}

bool csp_gochan_try_recv(csp_gochan_t *ch, void **val, bool *ok) {
    CSP_CRITICAL_START();
    pthread_mutex_lock(&ch->lock);
    if (ch->size > 0) {
        void *v = ch->buffer[ch->tail];
        ch->tail = (ch->tail + 1) % ch->capacity;
        ch->size--;
        if (ch->send_q) {
            csp_proc_t *p = (csp_proc_t *)ch->send_q;
            ch->send_q = (struct csp_proc_s *)p->next;
            csp_proc_extra_t *ex = (csp_proc_extra_t *)p->extra;
            void *sval = ex ? ex->chan_val : NULL;
            ch->buffer[ch->head] = sval;
            ch->head = (ch->head + 1) % ch->capacity;
            ch->size++;
            csp_scheduler_submit(p);
        }
        pthread_mutex_unlock(&ch->lock);
        if (val) *val = v;
        if (ok) *ok = true;
        CSP_CRITICAL_END();
        return true;
    }

    if (ch->closed) {
        pthread_mutex_unlock(&ch->lock);
        if (val) *val = NULL;
        if (ok) *ok = false;
        CSP_CRITICAL_END();
        return true;
    }

    if (ch->send_q && ch->capacity == 0) {
        csp_proc_t *p = (csp_proc_t *)ch->send_q;
        ch->send_q = (struct csp_proc_s *)p->next;
        csp_proc_extra_t *ex = (csp_proc_extra_t *)p->extra;
        void *v = ex ? ex->chan_val : NULL;
        pthread_mutex_unlock(&ch->lock);
        csp_scheduler_submit(p);
        if (val) *val = v;
        if (ok) *ok = true;
        CSP_CRITICAL_END();
        return true;
    }

    pthread_mutex_unlock(&ch->lock);
    CSP_CRITICAL_END();
    return false;
}

int csp_select(csp_select_case_t *cases, int n) {
    int poll_order_stack[32];
    int *poll_order = poll_order_stack;
    if (n > 32) poll_order = malloc(n * sizeof(int));

    for (int i = 0; i < n; i++) poll_order[i] = i;

    // Shuffle for fairness
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = poll_order[i];
        poll_order[i] = poll_order[j];
        poll_order[j] = temp;
    }

    int default_idx = -1;
    int final_chosen = -1;

    while (true) {
        for (int i = 0; i < n; i++) {
            int idx = poll_order[i];
            if (cases[idx].op == CSP_DEFAULT) {
                default_idx = idx;
                continue;
            }

            csp_gochan_t *ch = cases[idx].ch;
            if (cases[idx].op == CSP_RECV) {
                void *val;
                bool ok;
                if (csp_gochan_try_recv(ch, &val, &ok)) {
                    if (cases[idx].val) *cases[idx].val = val;
                    final_chosen = idx;
                    goto out;
                }
            } else if (cases[idx].op == CSP_SEND) {
                if (csp_gochan_try_send(ch, (void *)cases[idx].val)) {
                    final_chosen = idx;
                    goto out;
                }
            }
        }

        if (default_idx != -1) {
            final_chosen = default_idx;
            goto out;
        }

        csp_sched_yield();
    }

out:
    if (n > 32) free(poll_order);
    return final_chosen;
}
