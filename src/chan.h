#ifndef LIBCSP_CHAN_H
#define LIBCSP_CHAN_H

#include "platform.h"
#include "rbq.h"

#ifdef __plusplus
extern "C" {
#endif

// Forward declarations
struct csp_proc_s;

// Production-grade Go-class channel
typedef struct csp_gochan_s {
    size_t capacity;
    size_t size;
    size_t head;
    size_t tail;
    void **buffer;
    bool closed;
    pthread_mutex_t lock;
    struct csp_proc_s *send_q;
    struct csp_proc_s *recv_q;
} csp_gochan_t;

csp_gochan_t *csp_gochan_new(size_t capacity);
void csp_gochan_close(csp_gochan_t *ch);
bool csp_gochan_send(csp_gochan_t *ch, void *val);
void *csp_gochan_recv(csp_gochan_t *ch, bool *ok);
bool csp_gochan_try_send(csp_gochan_t *ch, void *val);
bool csp_gochan_try_recv(csp_gochan_t *ch, void **val, bool *ok);

typedef enum { CSP_RECV, CSP_SEND, CSP_DEFAULT } csp_select_op_t;
typedef struct {
    csp_gochan_t *ch;
    csp_select_op_t op;
    void **val; // For RECV: where to store. For SEND: what to send (needs to be void* val, so maybe void** is fine if we cast)
} csp_select_case_t;

int csp_select(csp_select_case_t *cases, int n);

#define SELECT_START { \
    csp_select_case_t _cases[32]; \
    int _n_cases = 0;

#define RECV_CASE(chan, pval) \
    _cases[_n_cases].ch = (csp_gochan_t*)(chan); \
    _cases[_n_cases].op = CSP_RECV; \
    _cases[_n_cases].val = (void**)(pval); \
    _n_cases++;

#define SEND_CASE(chan, val) \
    _cases[_n_cases].ch = (csp_gochan_t*)(chan); \
    _cases[_n_cases].op = CSP_SEND; \
    _cases[_n_cases].val = (void**)(val); \
    _n_cases++;

#define DEFAULT_CASE \
    _cases[_n_cases].ch = NULL; \
    _cases[_n_cases].op = CSP_DEFAULT; \
    _cases[_n_cases].val = NULL; \
    _n_cases++;

#define SELECT_END(idx) \
    idx = csp_select(_cases, _n_cases); \
}

// Directional macros
#define chan_send_only(type) csp_gochan_t*
#define chan_recv_only(type) csp_gochan_t*

// Original libcsp macros
#define csp_chan_t(I)                     csp_chan_t_ ## I
#define csp_chan_new(I)                   csp_chan_new_ ## I
#define csp_chan_try_push(c, item)        ((c)->try_push((c)->rbq, (item)))
#define csp_chan_push(c, item)            ((c)->push((c)->rbq, (item)))
#define csp_chan_try_pop(c, item)         ((c)->try_pop((c)->rbq, (item)))
#define csp_chan_pop(c, item)             ((c)->pop((c)->rbq, (item)))
#define csp_chan_try_pushm(c, items, n)   ((c)->try_pushm((c)->rbq, (items), n))
#define csp_chan_pushm(c, items, n)       ((c)->pushm((c)->rbq, (items), n))
#define csp_chan_try_popm(c, items, n)    ((c)->try_popm((c)->rbq, (items), n))
#define csp_chan_popm(c, items, n)        ((c)->popm((c)->rbq, (items), n))
#define csp_chan_destroy(c)                                                    \
  do { (c)->destroy((c)->rbq); free(c); } while (0)                            \

#define csp_chan_declare(K, T, I)                                              \
  csp_ ## K ## rbq_declare(T, I);                                              \
  typedef struct {                                                             \
    void *rbq;                                                                 \
    bool (*try_push)(void *rbq, T item);                                       \
    bool (*try_pushm)(void *rbq, T *items, size_t n);                          \
    bool (*try_pop)(void *rbq, T *item);                                       \
    size_t (*try_popm)(void *rbq, T *items, size_t n);                         \
    void (*push)(void *rbq, T item);                                           \
    void (*pushm)(void *rbq, T *items, size_t n);                              \
    void (*pop)(void *rbq, T *item);                                           \
    void (*popm)(void *rbq, T *items, size_t n);                               \
    void (*destroy)(void *rbq);                                                \
  } csp_chan_t(I);                                                             \
  csp_chan_t(I) *csp_chan_new(I)(size_t cap_exp);                              \

#define csp_chan_define(K, T, I)                                               \
  csp_ ## K ## rbq_define(T, I);                                               \
  csp_chan_t(I) *csp_chan_new(I)(size_t cap_exp) {                             \
    csp_chan_t(I) *chan = (csp_chan_t(I) *)malloc(sizeof(csp_chan_t(I)));      \
    if (chan == NULL) {                                                        \
      return NULL;                                                             \
    }                                                                          \
    chan->rbq = csp_ ## K ## rbq_new(I)(cap_exp);                              \
    if (chan->rbq == NULL) {                                                   \
      free(chan);                                                              \
      return NULL;                                                             \
    }                                                                          \
    chan->try_push  = csp_ ## K ## rbq_try_push(I);                            \
    chan->try_pushm = csp_ ## K ## rbq_try_pushm(I);                           \
    chan->try_pop   = csp_ ## K ## rbq_try_pop(I);                             \
    chan->try_popm  = csp_ ## K ## rbq_try_popm(I);                            \
    chan->push      = csp_ ## K ## rbq_push(I);                                \
    chan->pushm     = csp_ ## K ## rbq_pushm(I);                               \
    chan->pop       = csp_ ## K ## rbq_pop(I);                                 \
    chan->popm      = csp_ ## K ## rbq_popm(I);                                \
    chan->destroy   = csp_ ## K ## rbq_destroy(I);                             \
    return chan;                                                               \
  }                                                                            \

#ifdef __plusplus
}
#endif

#endif
