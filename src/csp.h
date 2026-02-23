#ifndef LIBCSP_H
#define LIBCSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "chan.h"
#include "mutex.h"
#include "netpoll.h"
#include "csp_sched.h"
#include "timer.h"
#include "sync.h"
#include "context.h"
#include "runtime.h"

#define csp_async   csp_sched_async
#define csp_sync    csp_sched_sync
#define csp_block   csp_sched_block
#define csp_yield   csp_sched_yield
#define csp_hangup  csp_sched_hangup

/* Channel */
#define chan_t              csp_gochan_t
#define chan_new            csp_gochan_new
#define chan_send           csp_gochan_send
#define chan_recv           csp_gochan_recv
#define chan_close          csp_gochan_close

/* Time */
#define time_after          csp_time_after
#define ticker_new          csp_ticker_new
#define ticker_stop         csp_ticker_stop

/* Sync */
#define mutex_t             csp_sync_mutex_t
#define mutex_init          csp_sync_mutex_init
#define mutex_lock          csp_sync_mutex_lock
#define mutex_unlock        csp_sync_mutex_unlock

#define waitgroup_t         csp_sync_waitgroup_t
#define wg_init             csp_sync_waitgroup_init
#define wg_add              csp_sync_waitgroup_add
#define wg_done             csp_sync_waitgroup_done
#define wg_wait             csp_sync_waitgroup_wait

#ifdef __cplusplus
}
#endif

#endif
