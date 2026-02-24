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

#include "platform.h"
#include "common.h"
#include "core.h"
#include "mutex.h"
#include "proc.h"
#include "timer.h"
#include "chan.h"
#include "scheduler.h"
#include "csp_sched.h"
#include <stdatomic.h>

#define csp_timer_getclock() ({                                                \
  uint32_t high, low;                                                          \
  __asm__ __volatile__("rdtsc\n": "=d"(high), "=a"(low));                      \
  ((int64_t)high << 32) | low;                                                 \
})                                                                             \

#define csp_timer_heap_default_cap 64
#define csp_timer_heap_lte(heap, i, j)                                         \
  ((heap)->procs[i]->timer.when <= (heap)->procs[j]->timer.when)

extern int csp_sched_np;
extern _Thread_local csp_core_t *csp_this_core;

extern void csp_core_proc_exit(void);
extern void csp_proc_destroy(csp_proc_t *proc);
extern void csp_sched_yield(void);

typedef struct csp_timer_heap_t {
  size_t cap, len;
  csp_proc_t **procs;
  csp_timer_time_t time, clock;
  int64_t token;
  csp_mutex_t mutex;
} csp_timer_heap_t;

bool csp_timer_heap_init(csp_timer_heap_t *heap, size_t pid) {
  heap->cap = csp_timer_heap_default_cap;
  heap->len = 0;
  heap->time = csp_timer_now();
  heap->clock = csp_timer_getclock();
  heap->procs = (csp_proc_t **)malloc(sizeof(csp_proc_t *) * heap->cap);
  heap->token = (uint64_t)pid << 53;
  csp_mutex_init(&heap->mutex);
  return heap->procs != NULL;
}

void csp_timer_heap_shift_up(csp_timer_heap_t *heap, int64_t idx) {
  while (idx > 0) {
    int64_t father = (idx - 1) >> 1;
    if (csp_timer_heap_lte(heap, father, idx)) {
      return;
    }
    csp_swap(heap->procs[idx], heap->procs[father]);
    csp_swap(heap->procs[idx]->timer.idx, heap->procs[father]->timer.idx);
    idx = father;
  }
}

void csp_timer_heap_put(csp_timer_heap_t *heap, csp_proc_t *proc) {
  csp_mutex_lock(&heap->mutex);
  if (csp_unlikely(heap->len == heap->cap)) {
    size_t cap = heap->cap << 1;
    csp_proc_t **procs = (csp_proc_t **)realloc(heap->procs, sizeof(csp_proc_t *) * cap);
    if (csp_unlikely(procs == NULL)) {
      exit(EXIT_FAILURE);
    }
    heap->procs = procs;
    heap->cap = cap;
  }
  csp_proc_timer_token_set(proc, heap->token);
  heap->token++;
  heap->procs[heap->len] = proc;
  proc->timer.idx = heap->len++;
  csp_timer_heap_shift_up(heap, proc->timer.idx);
  csp_mutex_unlock(&heap->mutex);
}

void csp_timer_heap_del(csp_timer_heap_t *heap, csp_proc_t *proc) {
  int64_t idx = proc->timer.idx;
  if (idx == --heap->len) {
    return;
  }
  heap->procs[idx] = heap->procs[heap->len];
  heap->procs[idx]->timer.idx = idx;
  if (idx > 0 && csp_timer_heap_lte(heap, idx, (idx - 1) >> 1)) {
    csp_timer_heap_shift_up(heap, idx);
    return;
  }
  while (true) {
    int64_t son = (idx << 1) + 1;
    if (son >= heap->len) break;
    if (son + 1 < heap->len && csp_timer_heap_lte(heap, son + 1, son)) son++;
    if (csp_timer_heap_lte(heap, idx, son)) break;
    csp_swap(heap->procs[idx], heap->procs[son]);
    csp_swap(heap->procs[idx]->timer.idx, heap->procs[son]->timer.idx);
    idx = son;
  }
}

static int csp_timer_heap_get(csp_timer_heap_t *heap, csp_proc_t **start, csp_proc_t **end) {
  csp_mutex_lock(&heap->mutex);
  if (heap->len == 0) {
    csp_mutex_unlock(&heap->mutex);
    return 0;
  }
  csp_timer_time_t curr_time = csp_timer_now();
  int n = 0;
  csp_proc_t *head = NULL, *tail = NULL, *top;
  while (heap->len > 0 && (top = heap->procs[0])->timer.when <= curr_time) {
    csp_timer_heap_del(heap, top);
    csp_proc_timer_token_set(top, -1);
    if (tail == NULL) { head = tail = top; }
    else { tail->next = (struct csp_proc_s *)top; top->pre = (struct csp_proc_s *)tail; tail = top; }
    n++;
  }
  if (n > 0) { *start = head; *end = tail; }
  csp_mutex_unlock(&heap->mutex);
  return n;
}

struct { int len; csp_timer_heap_t *heaps; } csp_timer_heaps;

bool csp_timer_heaps_init(void) {
  csp_timer_heaps.heaps = (csp_timer_heap_t *)malloc(sizeof(csp_timer_heap_t) * csp_sched_np);
  if (csp_timer_heaps.heaps == NULL) return false;
  for (int i = 0; i < csp_sched_np; i++) {
    if(!csp_timer_heap_init(&csp_timer_heaps.heaps[i], i)) {
      csp_timer_heaps.len = i;
      return false;
    }
  }
  csp_timer_heaps.len = csp_sched_np;
  return true;
}

void csp_timer_heaps_destroy(void) {
  for (int i = 0; i < csp_timer_heaps.len; i++) free(csp_timer_heaps.heaps[i].procs);
  free(csp_timer_heaps.heaps);
}

void csp_timer_put(size_t pid, csp_proc_t *proc) {
  csp_timer_heap_put(&csp_timer_heaps.heaps[pid % csp_timer_heaps.len], proc);
}

int csp_timer_poll(csp_proc_t **start, csp_proc_t **end) {
  int total = 0;
  csp_proc_t *head, *tail;
  for (int i = 0; i < csp_timer_heaps.len; i++) {
    int n = csp_timer_heap_get(&csp_timer_heaps.heaps[i], &head, &tail);
    if (n > 0) {
      if (total != 0) { (*end)->next = (struct csp_proc_s *)head; head->pre = (struct csp_proc_s *)*end; *end = tail; }
      else { *start = head; *end = tail; }
      total += n;
    }
  }
  return total;
}

bool csp_timer_cancel(csp_timer_t timer) {
  csp_timer_heap_t *heap = &csp_timer_heaps.heaps[timer.ctx->borned_pid % csp_timer_heaps.len];
  csp_mutex_lock(&heap->mutex);
  if (!csp_proc_timer_token_cas(timer.ctx, timer.token, -1)) {
    csp_mutex_unlock(&heap->mutex);
    return false;
  }
  csp_timer_heap_del(heap, timer.ctx);
  csp_mutex_unlock(&heap->mutex);
  csp_proc_destroy(timer.ctx);
  return true;
}

csp_proc void csp_timer_anchor(csp_timer_time_t when) {}

// PRODUCTION EXTENSIONS
typedef struct {
    csp_gochan_t *ch;
    csp_timer_duration_t duration;
    bool periodic;
    atomic_bool stopped;
} timer_task_arg_t;

static void timer_task(void *arg) {
    timer_task_arg_t *ta = (timer_task_arg_t *)arg;
    while (!atomic_load(&ta->stopped)) {
        csp_csp_sched_hangup(ta->duration);
        if (atomic_load(&ta->stopped) || ta->ch->closed) break;
        if (ta->periodic) {
            csp_gochan_try_send(ta->ch, NULL);
        } else {
            csp_gochan_send(ta->ch, NULL);
        }
        if (!ta->periodic) break;
    }
    if (!ta->periodic) free(ta);
}

csp_gochan_t *csp_time_after(csp_timer_duration_t d) {
    csp_gochan_t *ch = csp_gochan_new(1);
    timer_task_arg_t *ta = (timer_task_arg_t *)malloc(sizeof(timer_task_arg_t));
    ta->ch = ch; ta->duration = d; ta->periodic = false;
    atomic_init(&ta->stopped, false);
    csp_proc_create(0, timer_task, ta);
    return ch;
}

csp_ticker_t *csp_ticker_new(csp_timer_duration_t d) {
    csp_ticker_t *ticker = (csp_ticker_t *)malloc(sizeof(csp_ticker_t));
    ticker->ch = csp_gochan_new(1);
    timer_task_arg_t *ta = (timer_task_arg_t *)malloc(sizeof(timer_task_arg_t));
    ta->ch = ticker->ch;
    ta->duration = d;
    ta->periodic = true;
    atomic_init(&ta->stopped, false);
    ticker->ta = ta;
    csp_proc_create(0, timer_task, ta);
    return ticker;
}

void csp_ticker_stop(csp_ticker_t *ticker) {
    if (ticker) {
        timer_task_arg_t *ta = (timer_task_arg_t *)ticker->ta;
        atomic_store(&ta->stopped, true);
    }
}
