#ifndef LIBCSP_PROC_EXTRA_H
#define LIBCSP_PROC_EXTRA_H

#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>

typedef struct {
    bool preemptible;
    atomic_int in_critical_section;
    void *chan_val; // For returning values from blocking channel operations
    bool chan_ok;
} csp_proc_extra_t;

extern _Thread_local struct csp_core_s *csp_this_core;

#define CSP_CRITICAL_START() do { \
    if (csp_this_core && csp_this_core->running) { \
        csp_proc_t *_p = (csp_proc_t *)csp_this_core->running; \
        if (_p->extra) ((csp_proc_extra_t *)_p->extra)->in_critical_section++; \
    } \
} while(0)

#define CSP_CRITICAL_END() do { \
    if (csp_this_core && csp_this_core->running) { \
        csp_proc_t *_p = (csp_proc_t *)csp_this_core->running; \
        if (_p->extra) ((csp_proc_extra_t *)_p->extra)->in_critical_section--; \
    } \
} while(0)

static inline csp_proc_extra_t *csp_proc_extra_new() {
    return (csp_proc_extra_t *)calloc(1, sizeof(csp_proc_extra_t));
}

#endif
