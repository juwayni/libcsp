#include "runtime.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdatomic.h>

int runtime_num_goroutines() {
    return csp_global_scheduler ? atomic_load(&csp_global_scheduler->num_procs) : 0;
}

int runtime_num_workers() {
    return csp_global_scheduler ? csp_global_scheduler->num_workers : 0;
}

void runtime_dump() {
    printf("Runtime Stats:\n");
    printf("  Goroutines: %d\n", runtime_num_goroutines());
    printf("  Workers:    %d\n", runtime_num_workers());
}

void runtime_trace_enable(bool enable) {
    // Placeholder
}
