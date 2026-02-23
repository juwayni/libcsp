#ifndef LIBCSP_RUNTIME_H
#define LIBCSP_RUNTIME_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int runtime_num_goroutines();
int runtime_num_workers();
void runtime_dump();
void runtime_trace_enable(bool enable);

#ifdef __cplusplus
}
#endif

#endif
