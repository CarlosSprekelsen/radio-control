# Silvus Mock C++ Test Coverage Gap After Go Deletion

**Date:** 2026-04-19
**Issue:** 16
**Severity:** Medium
**Status:** Open

## Context

The Go-based `silvus-mock-go/` has been deleted. The C++ `silvus-mock/` is now the sole Silvus radio mock. Functional parity for RCC integration is verified — all JSON-RPC methods consumed by `SilvusAdapter` (`freq`, `power_dBm`, `supported_frequency_profiles`, `read_power_dBm`, `zeroize`, `radio_reset`, `factory_reset`) are implemented and respond correctly.

## Gap

The C++ mock currently has **zero automated tests**. The deleted Go mock contained:

- HTTP contract tests (`internal/contracttests/http_contract_test.go`)
- TCP maintenance contract tests (`internal/contracttests/tcp_contract_test.go`)
- Blackout behavior unit tests (`internal/state/blackout_test.go`)
- JSON-RPC envelope validators and golden fixtures
- Configurability via YAML/env (C++ mock is fully hardcoded)

## What is acceptable

For RCC E2E and integration testing, the C++ mock is sufficient because:
- RCC only consumes the HTTP JSON-RPC surface (`/streamscape_api`)
- All required methods are implemented with correct response shapes
- Blackout behavior (soft-boot after freq/power change, reset blackout) is present

## What should be added (priority order)

1. **Smoke test script** that starts the mock and exercises `freq`, `power_dBm`, `profiles`, `status`, and a blackout sequence via curl.
2. **CMake CTest target** that runs the smoke test as part of the RCC build.
3. **YAML configuration** to replace hardcoded frequency profiles, power limits, and blackout durations.
4. **Unit tests** for the `SilvusMock` state machine (blackout logic, parameter validation).

## Acceptance Criteria

- [ ] `ctest --test-dir build` in `silvus-mock/` runs at least one validation test.
- [ ] No regression in RCC integration tests (`test_silvus_adapter_contract`).

## References

- `silvus-mock/` — C++ implementation (no tests)
- `silvus-mock-go/` — deleted Go implementation (had extensive tests)
