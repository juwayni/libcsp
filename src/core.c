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
#include <assert.h>
#include <stddef.h>
#include "core.h"
#include "csp_sched.h"

static_assert(offsetof(csp_core_t, running) == 0x40, "csp_core_t.running offset mismatch");

#define csp_core_anchor_load(reg, p)                                           \
  "mov (%"reg"),     %"p"rbp\n"                                                \
  "mov 0x08(%"reg"), %"p"rsp\n"                                                \
  "mov 0x10(%"reg"), %"p"rax\n"                                                \
  "mov %"p"rax,         (%"p"rsp)\n"                                           \
  "mov 0x18(%"reg"), %"p"rbx\n"                                                \
  "mov 0x20(%"reg"), %"p"r12\n"                                                \
  "mov 0x28(%"reg"), %"p"r13\n"                                                \
  "mov 0x30(%"reg"), %"p"r14\n"                                                \
  "mov 0x38(%"reg"), %"p"r15\n"

extern csp_proc_t *csp_sched_get(csp_core_t *this_core);
extern void csp_sched_put_proc(csp_proc_t *proc);
extern bool csp_core_pools_get(size_t pid, csp_core_t **core);
extern void csp_core_pools_put(csp_core_t *core);
extern void csp_proc_restore(csp_proc_t *proc);
extern void csp_proc_destroy(csp_proc_t *proc);

_Thread_local csp_core_t *csp_this_core;

csp_core_t *csp_core_new(size_t pid, csp_lrunq_t *lrunq, csp_grunq_t *grunq) {
  csp_core_t *core = (csp_core_t *)malloc(sizeof(csp_core_t));
  if (core == NULL) {
    return NULL;
  }

  core->pid = pid;
  core->lrunq = lrunq;
  core->grunq = grunq;
  core->running = NULL;

  csp_core_state_set(core, csp_core_state_inited);
  pthread_cond_init(&core->cond, NULL);
  pthread_mutex_init(&core->mutex, NULL);
  csp_cond_init(&core->pcond);

  return core;
}

__attribute__((naked,used)) void csp_core_anchor_save(void *anchor) {
  __asm__ __volatile__(
    "mov %rbp,   (%rdi)\n"
    "mov %rsp,   0x08(%rdi)\n"
    "mov (%rsp), %rax\n"
    "mov %rax,   0x10(%rdi)\n"
    "mov %rbx,   0x18(%rdi)\n"
    "mov %r12,   0x20(%rdi)\n"
    "mov %r13,   0x28(%rdi)\n"
    "mov %r14,   0x30(%rdi)\n"
    "mov %r15,   0x38(%rdi)\n"
    "retq\n"
  );
}

__attribute__((naked)) void csp_core_anchor_restore(void *anchor) {
  __asm__ __volatile__(
    csp_core_anchor_load("rdi", "")
    "retq\n"
  );
}

__attribute__((noinline)) void *csp_core_run(void *data) {
  csp_core_t *this_core = (csp_core_t *)data;
  csp_core_state_set(this_core, csp_core_state_running);
  csp_this_core = this_core;

  __asm__ __volatile__(
    "mov %0, %%rbx\n"
    "1:\n"
    "mov %%rbx, %%rdi\n"
    "call csp_core_anchor_save@plt\n"
    "mov %%rbx, %%rdi\n"
    "call csp_sched_get@plt\n"
    "test %%rax, %%rax\n"
    "jz 2f\n"
    "mov %%rax, 0x40(%%rbx)\n"
    "mov %%rax, %%rdi\n"
    "call csp_proc_restore@plt\n"
    "jmp 1b\n"
    "2:\n"
    ::"r"(this_core) :"rdi", "rbx", "memory"
  );

  return NULL;
}

void csp_core_init_main(csp_core_t *core) {
  core->tid = pthread_self();
  csp_this_core = core;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  pthread_setaffinity_np(core->tid, sizeof(cpuset), &cpuset);
}

bool csp_core_start(csp_core_t *core) {
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0 ||
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0 ||
    pthread_create(&core->tid, &attr, csp_core_run, core) != 0) {
    return false;
  }
  pthread_attr_destroy(&attr);
  return true;
}

__attribute__((naked)) void csp_core_yield(csp_proc_t *proc, void *anchor) {
  __asm__ __volatile__(
    csp_proc_save("rdi")
    "mov %rsi, %rdi\n"
    "call csp_core_anchor_restore@plt\n"
  );
}

bool csp_core_block_prologue(csp_core_t *this_core) {
  return false;
}

__attribute__((used))
static void csp_core_block_epilogue_inner(csp_core_t *this_core) {
  while (!csp_grunq_try_push(this_core->grunq, (csp_proc_t *)this_core->running));
  this_core->running = NULL;
  pthread_mutex_lock(&this_core->mutex);
  csp_core_pools_put(this_core);
  pthread_cond_wait(&this_core->cond, &this_core->mutex);
  pthread_mutex_unlock(&this_core->mutex);
  csp_core_anchor_restore(&this_core->anchor);
  __builtin_unreachable();
}

__attribute__((naked))
void csp_core_block_epilogue(csp_core_t *core, csp_proc_t *proc) {
  __asm__ __volatile__(
    csp_proc_save("rsi")
    "call csp_core_block_epilogue_inner@plt\n"
  );
}

__attribute__((naked))
void csp_core_proc_exit_inner(csp_proc_t *proc, void *anchor) {
  __asm__ __volatile__(
    "mov %rdi, %r12\n" // Preserve 'proc' in r12 (which will be restored anyway, but wait...)
    // Wait! r12 is restored by anchor_load. So I'll use r11 (caller-saved).
    "mov %rdi, %r11\n"
    csp_core_anchor_load("rsi", "")
    "mov %r11, %rdi\n"
    "sub $8, %rsp\n"   // Align stack for call
    "call csp_proc_destroy@plt\n"
    "add $8, %rsp\n"
    "retq\n"
  );
}

void csp_core_proc_exit(void) {
  csp_proc_t *running = (csp_proc_t *)csp_this_core->running, *parent = (csp_proc_t *)running->parent;
  if (parent != NULL && csp_proc_nchild_decr(parent) == 0x01) {
    csp_sched_put_proc(parent);
  }
  csp_this_core->running = NULL;
  csp_core_proc_exit_inner(running, &csp_this_core->anchor);
}

__attribute__((noreturn)) void csp_core_proc_exit_and_run(csp_proc_t *to_run) {
  csp_core_t *this_core = csp_this_core;
  csp_proc_t *running = (csp_proc_t *)this_core->running;
  this_core->running = (struct csp_proc_s *)to_run;

  __asm__ __volatile__ (
    "mov %0, %%r12\n"
    "mov %1, %%rbp\n"
    "mov %2, %%rsp\n"
    "sub $8, %%rsp\n"
    "call csp_proc_destroy@plt\n"
    "add $8, %%rsp\n"
    "mov %%r12, %%rdi\n"
    "call csp_proc_restore@plt\n"
    :
    :"m"(to_run), "r"(this_core->anchor.rbp), "r"(this_core->anchor.rsp),
     "D"(running)
    :"rbp", "rsp", "r12", "memory"
  );
  __builtin_unreachable();
}

void csp_core_destroy(csp_core_t *core) {
  if (core != NULL) {
    pthread_mutex_destroy(&core->mutex);
    pthread_cond_destroy(&core->cond);
    free(core);
  }
}

extern void csp_scheduler_submit(csp_proc_t *proc);
extern void csp_core_anchor_restore(void *anchor);

__attribute__((naked))
void csp_preempt_switch_and_submit(uintptr_t rsp, uintptr_t rbp, csp_proc_t *proc, void *anchor) {
    __asm__ __volatile__(
        "mov %rdi, %rsp\n"
        "mov %rsi, %rbp\n"
        "sub $16, %rsp\n"
        "push %rcx\n"
        "mov %rdx, %rdi\n"
        "call csp_scheduler_submit@plt\n"
        "pop %rdi\n"
        "call csp_core_anchor_restore@plt\n"
    );
}

void csp_preempt_helper(uintptr_t sp) {
    csp_core_t *core = csp_this_core;
    csp_proc_t *proc = (csp_proc_t *)core->running;

    proc->rsp = sp;
    proc->is_new = 2; // Preempted
    __asm__ __volatile__("stmxcsr %0" : "=m"(proc->mxcsr));
    __asm__ __volatile__("fstcw %0" : "=m"(proc->x87cw));

    core->running = NULL;

    csp_preempt_switch_and_submit(core->anchor.rsp, core->anchor.rbp, proc, &core->anchor);
}
