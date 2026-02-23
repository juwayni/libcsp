#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>

typedef struct csp_proc_t {
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
  struct csp_proc_t *parent;
  struct csp_proc_t *pre, *next;
  atomic_uint_fast64_t nchild;
  atomic_uint_fast64_t stat;
  void *extra;
} csp_proc_t;

int main() {
    printf("%zu\n", sizeof(csp_proc_t));
    return 0;
}
