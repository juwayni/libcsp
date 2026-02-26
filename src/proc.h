/*
 * Copyright (c) 2020, Yanhui Shi <lime.syh at gmail dot com>
 * All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef LIBCSP_PROC_H
#define LIBCSP_PROC_H

#include "platform.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#include <stddef.h>
#endif

#ifdef csp_enable_valgrind
#include <valgrind/valgrind.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define csp_proc                        __attribute__((used,noinline))
#define csp_proc_nchild_get(p)          atomic_load(&(p)->nchild)
#define csp_proc_nchild_incr(p)         atomic_fetch_add(&(p)->nchild, 1)
#define csp_proc_nchild_decr(p)         atomic_fetch_sub(&(p)->nchild, 1)
#define csp_proc_timer_token_get(p)     atomic_load(&((p)->timer.token))
#define csp_proc_timer_token_set(p, v)  atomic_store(&((p)->timer.token), (v))
#define csp_proc_timer_token_cas(p, old, new)                                  \
  atomic_compare_exchange_weak(&((p)->timer.token), &old, new)

#define csp_proc_stat_none              0
#define csp_proc_stat_netpoll_waiting   1
#define csp_proc_stat_netpoll_avail     2
#define csp_proc_stat_netpoll_timeout   3
#define csp_proc_stat_runnable          4
#define csp_proc_stat_running           5
#define csp_proc_stat_blocked           6

#define csp_proc_stat_get(proc)         atomic_load_explicit(&(proc)->stat, memory_order_acquire)
#define csp_proc_stat_set(proc, val)    atomic_store_explicit(&(proc)->stat, val, memory_order_release)
#define csp_proc_stat_cas(proc, oval, nval)                                    \
  atomic_compare_exchange_weak_explicit(&(proc)->stat, &(oval), nval,          \
                                        memory_order_acq_rel, memory_order_acquire)

#define csp_proc_is_normal   0
#define csp_proc_is_new      1
#define csp_proc_is_preempt  2

#define csp_proc_save(reg)                                                     \
  "stmxcsr   0x18(%"reg")\n"                                                   \
  "fstcw     0x1c(%"reg")\n"                                                   \
  "mov %rsp, 0x20(%"reg")\n"                                                   \
  "mov %rbp, 0x28(%"reg")\n"                                                   \
  "mov %rbx, 0x30(%"reg")\n"                                                   \
  "mov %r12, 0x38(%"reg")\n"                                                   \
  "mov %r13, 0x40(%"reg")\n"                                                   \
  "mov %r14, 0x48(%"reg")\n"                                                   \
  "mov %r15, 0x50(%"reg")\n"                                                   \

typedef struct csp_proc_s {
  uint64_t base;
  uint64_t borned_pid;
  uint64_t is_new;
  uint32_t mxcsr;
  uint32_t x87cw;
  uint64_t rsp;
  uint64_t rbp;
  union {
    struct { uint64_t rbx, r12, r13, r14, r15; } callee_saved;
    struct { uint64_t rdi, rsi, rdx, rcx, r8, r9; } caller_saved;
  } registers;
  struct { int64_t when, idx; atomic_int_fast64_t token; } timer;
  struct csp_proc_s *parent;
  struct csp_proc_s *pre, *next;
  atomic_uint_fast64_t nchild;
  atomic_uint_fast64_t stat;
#ifdef csp_enable_valgrind
  uint64_t valgrind_stack;
#endif
  void *extra;
  atomic_bool yielding;
  char _padding[64]; // Avoid false sharing
} csp_proc_t;

void csp_proc_nchild_set(size_t nchild);

#ifdef __cplusplus
}
#endif

#endif
