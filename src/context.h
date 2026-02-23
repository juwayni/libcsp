#ifndef LIBCSP_CONTEXT_H
#define LIBCSP_CONTEXT_H

#include "platform.h"
#include "chan.h"
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct csp_context_s {
    csp_gochan_t *done;
    struct csp_context_s *parent;
    atomic_bool canceled;
} csp_context_t;

csp_context_t *csp_context_background();
csp_context_t *csp_context_with_cancel(csp_context_t *parent);
void csp_context_cancel(csp_context_t *ctx);
csp_gochan_t *csp_context_done(csp_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
