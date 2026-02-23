#include <stdio.h>
#include "src/proc.h"
#include "src/core.h"

int main() {
    printf("sizeof(csp_proc_t) = %zu\n", sizeof(csp_proc_t));
    printf("sizeof(csp_core_t) = %zu\n", sizeof(csp_core_t));
    return 0;
}
