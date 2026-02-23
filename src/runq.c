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
#include "proc.h"
#include "runq.h"

csp_mmrbq_define(csp_proc_t *, proc);

csp_lrunq_t *csp_lrunq_new() {
  csp_lrunq_t *lrunq = (csp_lrunq_t *)malloc(sizeof(csp_lrunq_t));
  if (lrunq != NULL) {
    lrunq->head = lrunq->tail = NULL;
    lrunq->len = 0;
    lrunq->poped_times = 0;
  }
  return lrunq;
}

void csp_lrunq_push(csp_lrunq_t *lrunq, csp_proc_t *proc) {
  if (lrunq->tail != NULL) {
    lrunq->tail->next = proc;
    proc->pre = lrunq->tail;
    lrunq->tail = proc;
  } else {
    lrunq->head = lrunq->tail = proc;
  }
  lrunq->len++;
}

void csp_lrunq_push_front(csp_lrunq_t *lrunq, csp_proc_t *proc) {
  if (lrunq->head != NULL) {
    proc->next = lrunq->head;
    lrunq->head->pre = proc;
    lrunq->head = proc;
  } else {
    lrunq->head = lrunq->tail = proc;
  }
  lrunq->len++;
}

int csp_lrunq_try_pop_front(csp_lrunq_t *lrunq, csp_proc_t **proc) {
  if (lrunq->len == 0) {
    return csp_lrunq_failed;
  }

  if (csp_unlikely(++lrunq->poped_times >= 61)) {
    lrunq->poped_times = 0;
    return csp_lrunq_missed;
  }

  *proc = lrunq->head;
  if (--lrunq->len > 0) {
    lrunq->head = (*proc)->next;
    lrunq->head->pre = NULL;
    (*proc)->next = NULL;
  } else {
    lrunq->head = lrunq->tail = NULL;
  }
  return csp_lrunq_ok;
}

void csp_lrunq_popm_front(csp_lrunq_t *lrunq, size_t n, csp_proc_t **start,
  csp_proc_t **end
) {
  if (lrunq->len == 0 || n == 0) {
    *start = *end = NULL;
    return;
  }

  if (n > lrunq->len) {
    n = lrunq->len;
  }

  *start = lrunq->head;
  for (size_t i = 1; i < n; i++) {
    lrunq->head = lrunq->head->next;
  }

  *end = lrunq->head;
  if (lrunq->len > n) {
    lrunq->head = lrunq->head->next;
    lrunq->head->pre = NULL;
    (*end)->next = NULL;
  } else {
    lrunq->head = lrunq->tail = NULL;
  }
  lrunq->len -= n;
}

void csp_lrunq_destroy(csp_lrunq_t *lrunq) {
  free(lrunq);
}
