# [Fixed] silvus-mock container crashes with SIGSEGV when host port 9080 in use

**Date:** 2026-04-19
**Severity:** Medium (blocks containerized bench setup)
**Component:** silvus-mock / deployment

## Problem
When running `silvus-mock-bench` container with `--network host`, it exits with code 139 (SIGSEGV) if port 9080 is already bound by a native `silvus-mock` process on the host. The binary throws `std::system_error: bind: Address already in use` which is uncaught and causes a segfault.

## Current Workaround
A native `silvus-mock` process (PID 439640) is already running on the host at `192.168.1.176:9080`. RCC in host-network mode connects to it successfully via `127.0.0.1:9080`. The container is stopped and not needed for current bench setup.

## Root Cause
The C++ silvus-mock binary does not gracefully handle bind failures — it throws an exception that is not caught, resulting in SIGSEGV instead of a clean exit.

## Proposed Fix
1. Add proper exception handling around `bind()` in silvus-mock main loop
2. Or: change bench setup to use a custom bridge network with explicit port mapping instead of `--network host`
3. Or: detect port conflict in `run-local.sh` / `e2e-validate.sh` and warn/kill conflicting process

## Fix Applied (2026-04-19)
- `main.cpp`: Wrapped `MaintenanceServer` construction and `HttpServer::serve()` in `try/catch(std::system_error)` blocks
- `http_server.cpp`: Wrapped `server.start()` and `io.run()` in `try/catch(std::system_error)`
- Both now print `bind: Address already in use` with a hint about checking for other instances or changing ports
- Process exits cleanly with code 1 instead of SIGSEGV
- Container image `localhost/silvus-mock:latest` rebuilt with fix

## Files Changed
- `services/radio-control/silvus-mock/src/main.cpp`
- `services/radio-control/silvus-mock/src/http_server.cpp`

## Impact
- Bench E2E scripts that start silvus-mock container will fail if developer already has native instance running
- Confusing error (SIGSEGV vs "port in use")
