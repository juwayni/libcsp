# Nim example for libcsp goroutines
# Compile with: nim c -d:danger --mm:orc -L:.libs -l:csp examples/nim_example.nim

import os

# FFI Bindings
type
  CspGoChan = ptr object
  CspProcFunc = proc(arg: pointer) {.noconv.}

proc csp_proc_create(id: cint, fn: CspProcFunc, arg: pointer): pointer {.importc, header: "csp.h".}
proc csp_gochan_new(cap: csize_t): CspGoChan {.importc, header: "csp.h".}
proc csp_gochan_send(ch: CspGoChan, val: pointer): bool {.importc, header: "csp.h".}
proc csp_gochan_recv(ch: CspGoChan, ok: ptr bool): pointer {.importc, header: "csp.h".}
proc csp_core_run(data: pointer): pointer {.importc, header: "csp.h".}

# Global core variable from libcsp
var csp_this_core {.importc, header: "csp.h".}: pointer

proc nim_worker(arg: pointer) {.noconv.} =
  let ch = cast[CspGoChan](arg)
  echo "[Nim Goroutine] Sending message to channel..."
  discard csp_gochan_send(ch, cast[pointer](42))
  echo "[Nim Goroutine] Done."

proc real_main(arg: pointer) {.noconv.} =
  echo "--- Nim libcsp Example ---"
  let ch = csp_gochan_new(1)

  discard csp_proc_create(0, nim_worker, ch)

  echo "[Nim Main] Waiting for message..."
  let val = cast[int](csp_gochan_recv(ch, nil))
  echo "[Nim Main] Received value: ", val

  echo "--- Nim Example Complete ---"
  quit(0)

# Entry point
when isMainModule:
  putEnv("LIBCSP_PRODUCTION", "1")
  discard csp_proc_create(0, real_main, nil)
  discard csp_core_run(csp_this_core)
