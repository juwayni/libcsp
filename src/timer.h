#ifndef LIBCSP_TIMER_H
#define LIBCSP_TIMER_H

#include "platform.h"
#include "proc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define csp_timer_nanosecond    ((csp_timer_duration_t)1)
#define csp_timer_microsecond   (csp_timer_nanosecond * 1000)
#define csp_timer_millisecond   (csp_timer_microsecond * 1000)
#define csp_timer_second        (csp_timer_millisecond * 1000)
#define csp_timer_minute        (csp_timer_second * 60)
#define csp_timer_hour          (csp_timer_minute * 60)

#define csp_timer_now() ({                                                     \
  struct timespec ts;                                                          \
  clock_gettime(CLOCK_MONOTONIC, &ts);                                         \
  (csp_timer_time_t)(ts.tv_sec * csp_timer_second + ts.tv_nsec);               \
})                                                                             \

#define csp_timer_at(when, task) ({                                            \
  csp_soft_mbarr();                                                            \
  csp_timer_t timer;                                                           \
  csp_timer_anchor(when);                                                      \
  task;                                                                        \
  __asm__ __volatile__("mov %%rax, %0\n" :"=r"(timer.ctx) :: "rax", "memory"); \
  timer.token = csp_proc_timer_token_get(timer.ctx);                           \
  csp_soft_mbarr();                                                            \
  timer;                                                                       \
})                                                                             \

#define csp_timer_after(duration, task)                                        \
  csp_timer_at(csp_timer_now() + (duration), (task))                           \

typedef int64_t csp_timer_time_t;
typedef int64_t csp_timer_duration_t;
typedef struct { csp_proc_t *ctx; int64_t token; } csp_timer_t;

bool csp_timer_cancel(csp_timer_t timer);
void csp_timer_anchor(csp_timer_time_t when);

// PRODUCTION EXTENSIONS
struct csp_gochan_s;
typedef struct csp_ticker_s {
    struct csp_gochan_s *ch;
    void *ta;
} csp_ticker_t;

struct csp_gochan_s *csp_time_after(csp_timer_duration_t d);
csp_ticker_t *csp_ticker_new(csp_timer_duration_t d);
void csp_ticker_stop(csp_ticker_t *ticker);

#ifdef __cplusplus
}
#endif

#endif
