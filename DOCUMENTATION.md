# libcsp: Production-Grade Go-Class Runtime for C

`libcsp` is a high-performance, production-grade concurrency runtime for C that implements a **preemptive M:N scheduler**, similar to the Go runtime. It provides lightweight processes (goroutines), type-safe channels, and a full suite of synchronization and context primitives.

## Table of Contents
1. [Core Architecture](#core-architecture)
2. [Getting Started](#getting-started)
3. [M:N Scheduler](#mn-scheduler)
4. [Asynchronous Preemption](#asynchronous-preemption)
5. [Channels and Select](#channels-and-select)
6. [Sync Package](#sync-package)
7. [Context Package](#context-package)
8. [Time Package](#time-package)
9. [Runtime Introspection](#runtime-introspection)

---

## Core Architecture

`libcsp` manages a pool of OS threads (Workers) and multiplexes a large number of lightweight processes (Goroutines) onto them.

- **Goroutines**: Stack-allocated, lightweight processes with ~2KB initial overhead.
- **Workers**: OS threads that execute goroutines. By default, N workers are created, where N is the number of CPU cores.
- **Work Stealing**: Each worker has a local run queue. If a worker becomes idle, it steals work from other workers or the global queue.

---

## Getting Started

### Compilation
To use `libcsp`, you must compile with `-fno-stack-protector` (required for manual stack management) and link against `libcsp.so`, `lpthread`, and `lrt`.

```bash
gcc -O3 -fno-stack-protector -I/path/to/libcsp/src my_program.c -L. -lcsp -lpthread -lrt
```

### Initialization
The runtime is initialized automatically via a constructor. To enable production mode, set the following environment variables:

```bash
export LIBCSP_PRODUCTION=1  # Enables M:N Scheduler
export LIBCSP_PREEMPT=1     # Enables Asynchronous Preemption
```

---

## M:N Scheduler

The scheduler distributes goroutines across OS threads.

- **`csp_proc_create(stack_id, func, arg)`**: Creates and schedules a new goroutine.
  - `stack_id`: Index into the pre-configured stack size array (default sizes: 64KB, etc.).
  - `func`: The function to execute.
  - `arg`: Argument passed to the function.

---

## Asynchronous Preemption

`libcsp` supports true asynchronous preemption. Long-running, CPU-bound goroutines are automatically paused and moved to the back of the queue to ensure fairness.

- **Mechanism**: Uses `SIGALRM` and a background preempter thread.
- **Safety**: Preemption is automatically disabled during critical sections (e.g., inside channel operations or mutex locks).

---

## Channels and Select

Channels are the primary mechanism for communication and synchronization.

### Channel API
- **`csp_gochan_new(capacity)`**: Create a new channel. `capacity=0` for unbuffered.
- **`csp_gochan_send(ch, val)`**: Send a value. Blocks if full/unbuffered. Returns `false` if closed.
- **`csp_gochan_recv(ch, ok)`**: Receive a value. Blocks if empty. `ok` is set to `false` if closed.
- **`csp_gochan_close(ch)`**: Closes the channel. Waiters are unblocked with `ok=false`.
- **`csp_gochan_try_send/recv`**: Non-blocking variants.

### Select
The `select` statement allows a goroutine to wait on multiple channel operations.

```c
csp_select_case_t cases[2];
cases[0] = (csp_select_case_t){ .ch = ch1, .op = CSP_RECV, .val = &v1 };
cases[1] = (csp_select_case_t){ .ch = ch2, .op = CSP_SEND, .val = (void**)v2 };

int chosen = csp_select(cases, 2);
```

---

## Sync Package

Provides high-level synchronization primitives that park goroutines rather than blocking OS threads.

### Mutex
- **`csp_sync_mutex_init(mu)`**
- **`csp_sync_mutex_lock(mu)`**
- **`csp_sync_mutex_unlock(mu)`**

### WaitGroup
- **`csp_sync_waitgroup_init(wg)`**
- **`csp_sync_waitgroup_add(wg, delta)`**
- **`csp_sync_waitgroup_done(wg)`**
- **`csp_sync_waitgroup_wait(wg)`**

### Atomics
Wrappers around `stdatomic.h` for safe cross-worker operations.
- `atomic_add`, `atomic_load`, `atomic_store`, etc.

---

## Context Package

Used for cancellation propagation and deadlines.

- **`csp_context_background()`**: Root context.
- **`csp_context_with_cancel(parent)`**: Creates a cancellable child context.
- **`csp_context_cancel(ctx)`**: Cancels the context and all its children.
- **`csp_context_done(ctx)`**: Returns a channel that is closed when the context is cancelled.

---

## Time Package

- **`time_after(duration)`**: Returns a channel that receives the current time after `duration` (nanoseconds).
- **`ticker_new(interval)`**: Returns a ticker that sends on its channel every `interval`.
- **`ticker_stop(ticker)`**: Stops a ticker. Does **not** close the channel.

---

## Runtime Introspection

- **`runtime_num_goroutines()`**: Total active goroutines.
- **`runtime_num_workers()`**: Number of active worker threads.
- **`runtime_dump()`**: Dumps runtime state to stdout for debugging.

---

## Side-by-Side Comparison: Go vs. libcsp

Below is a comparison of a common worker pool pattern implemented in both Go and libcsp (C).

### Go Implementation (`examples/go_v_c_example.go`)
```go
func worker(ctx context.Context, id int, jobs <-chan int, results chan<- int, wg *sync.WaitGroup) {
	defer wg.Done()
	for {
		select {
		case <-ctx.Done():
			return
		case job, ok := <-jobs:
			if !ok { return }
			results <- job * 2
		}
	}
}
```

### libcsp C Implementation (`examples/go_v_c_example.c`)
```c
void worker(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;
    while (1) {
        csp_select_case_t cases[2];
        cases[0] = (csp_select_case_t){ .ch = csp_context_done(args->ctx), .op = CSP_RECV };
        cases[1] = (csp_select_case_t){ .ch = args->jobs, .op = CSP_RECV, .val = &job_ptr };

        int chosen = csp_select(cases, 2);
        if (chosen == 0) break; // Cancelled
        if (job_ptr == NULL) break; // Closed

        csp_gochan_send(args->results, (void *)(job * 2));
    }
    csp_sync_waitgroup_done(args->wg);
}
```

---

## Foreign Function Interface (FFI)

`libcsp` is designed to be highly portable and can be used to bring Go-class goroutines to other languages that support C FFI.

### Nim Example
Nim's `--mm:orc` memory management is highly compatible with `libcsp`'s stack-switching architecture.

**`examples/nim_example.nim`**
```nim
import os

type
  CspGoChan = ptr object
  CspProcFunc = proc(arg: pointer) {.noconv.}

# Import C symbols from libcsp
proc csp_proc_create(id: cint, fn: CspProcFunc, arg: pointer): pointer {.importc, header: "csp.h".}
proc csp_gochan_new(cap: csize_t): CspGoChan {.importc, header: "csp.h".}
proc csp_gochan_send(ch: CspGoChan, val: pointer): bool {.importc, header: "csp.h".}
proc csp_gochan_recv(ch: CspGoChan, ok: ptr bool): pointer {.importc, header: "csp.h".}

proc nim_worker(arg: pointer) {.noconv.} =
  let ch = cast[CspGoChan](arg)
  discard csp_gochan_send(ch, cast[pointer](42))

# Compile with: nim c -d:danger --mm:orc -L:.libs -l:csp examples/nim_example.nim
```

### Zig Example
Zig can import `libcsp` directly using `@cImport`, making it a powerful choice for systems programming with goroutines.

**`examples/zig_example.zig`**
```zig
const std = @import("std");
const c = @cImport({
    @cInclude("csp.h");
});

fn zig_worker(arg: ?*anyopaque) callconv(.C) void {
    const ch = @ptrCast(*c.csp_gochan_t, arg);
    _ = c.csp_gochan_send(ch, @intToPtr(*anyopaque, 1234));
}

// Compile with: zig build-exe examples/zig_example.zig -I. -L.libs -lcsp -lpthread -fno-stack-check
```
