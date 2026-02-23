#ifndef LIBCSP_CORE_H
#define LIBCSP_CORE_H

#include "platform.h"
#include "cond.h"
#include "proc.h"
#include "runq.h"

#ifdef __cplusplus
extern "C" {
#endif

#define csp_core_state_set(c, s)    atomic_store(&(c)->state, (s))
#define csp_core_state_get(c)       atomic_load(&(c)->state)
#define csp_core_state_cas(c, o, n)                                            \
  atomic_compare_exchange_weak(&(c)->state, &(o), n)                           \

#define csp_core_wakeup(core) do {                                             \
  pthread_mutex_lock(&(core)->mutex);                                          \
  pthread_cond_signal(&(core)->cond);                                          \
  pthread_mutex_unlock(&(core)->mutex);                                        \
} while (0)                                                                    \

typedef enum {
  csp_core_state_inited,
  csp_core_state_running,
} csp_core_state_t;

typedef struct csp_core_s {
  /*
   * anchor saves the full callee-saved context of the scheduler thread.
   * rbp (0x00), rsp (0x08), rip (0x10), rbx (0x18), r12 (0x20), r13 (0x28), r14 (0x30), r15 (0x38)
   */
  struct { int64_t rbp, rsp, rip, rbx, r12, r13, r14, r15; } anchor;

  /* Current process running on the core. Offset: 0x40 */
  struct csp_proc_s *running;

  /* Id of the thread the core runs on. Offset: 0x48 */
  pthread_t tid;

  /* The id of cpu processor with which the core binds. Offset: 0x50 */
  size_t pid;

  /* State of the core. Offset: 0x58 */
  _Atomic csp_core_state_t state;

  /* The local runq. Offset: 0x60 */
  csp_lrunq_t *lrunq;

  /* The global runq. Offset: 0x68 */
  csp_grunq_t *grunq;

  /* Sync primitives. Offset: 0x70+ */
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  /* proc-level conditional variable. */
  csp_cond_t pcond;

  /* NEW FIELDS FOR M:N SCHEDULER */
  void *worker;

  _Alignas(64) char _padding[64]; // Avoid false sharing
} csp_core_t;

extern _Thread_local csp_core_t *csp_this_core;

bool csp_core_block_prologue(csp_core_t *core);
void csp_core_block_epilogue(csp_core_t *core, struct csp_proc_s *proc)
__attribute__((naked));

void csp_core_yield(struct csp_proc_s *proc, void *anchor) __attribute__((naked));

#ifdef __cplusplus
}
#endif

#endif
