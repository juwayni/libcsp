#include "context.h"
#include "scheduler.h"
#include <stdlib.h>

csp_context_t *csp_context_background() {
    csp_context_t *ctx = (csp_context_t *)calloc(1, sizeof(csp_context_t));
    ctx->done = csp_gochan_new(0); // Go background context done channel is always nil/never closed
    return ctx;
}

static void context_propagate(void *arg) {
    csp_context_t *ctx = (csp_context_t *)arg;
    csp_gochan_recv(ctx->parent->done, NULL);
    csp_context_cancel(ctx);
}

csp_context_t *csp_context_with_cancel(csp_context_t *parent) {
    csp_context_t *ctx = (csp_context_t *)calloc(1, sizeof(csp_context_t));
    ctx->done = csp_gochan_new(0);
    ctx->parent = parent;
    if (parent && parent->done) {
        csp_proc_create(0, context_propagate, ctx);
    }
    return ctx;
}

void csp_context_cancel(csp_context_t *ctx) {
    if (ctx && !atomic_exchange(&ctx->canceled, true)) {
        csp_gochan_close(ctx->done);
    }
}

csp_gochan_t *csp_context_done(csp_context_t *ctx) {
    return ctx ? ctx->done : NULL;
}
