# Silvus Mock C++ Test Coverage Gap After Go Deletion

**Date:** 2026-04-19
**Issue:** 16
**Severity:** Medium
**Status:** Open, partially mitigated 2026-06-02.

## Context

The Go-based `silvus-mock-go/` has been deleted. The C++ `silvus-mock/` is now
the sole Silvus radio mock. Functional parity for RCC integration is verified
for the reduced ICD methods consumed by `SilvusAdapter`: `freq`, `power_dBm`,
`enable_max_power`, `supported_frequency_profiles`, `read_power_dBm`,
`read_power_mw`, `zeroize`, `radio_reset`, and `factory_reset`.

## Remaining Gap

The C++ mock has a reduced CMake/CTest contract test, but it still does not
match the deleted Go mock's breadth of coverage. The deleted Go mock contained:

- HTTP contract tests
- TCP maintenance contract tests
- blackout behavior unit tests
- JSON-RPC envelope validators and golden fixtures
- configurability via YAML/env

2026-06-02 progress: `silvus-mock/test/silvus_mock_contract_test.cpp` directly
exercises the reduced ICD response shapes and validation rules. The remaining
gap is broader process coverage: executable smoke tests, TCP maintenance
coverage, configurable fixtures, and state-machine edge cases.

## What is acceptable

For RCC E2E and integration testing, the C++ mock is sufficient because:

- RCC only consumes the HTTP JSON-RPC surface (`/streamscape_api`)
- all required methods are implemented with correct response shapes
- blackout behavior (soft boot after `freq` change, reset blackout) is present

## What should be added

1. Smoke test script that starts the mock and exercises `freq`, `power_dBm`,
   profiles, status, and a blackout sequence via curl.
2. CMake CTest target that runs the smoke test as part of the RCC build.
3. YAML configuration to replace hardcoded frequency profiles, power limits, and
   blackout durations.
4. Unit tests for the `SilvusMock` state machine.

## Acceptance Criteria

- [x] `ctest --test-dir build` in `silvus-mock/` runs at least one validation
  test target.
- [ ] No regression in RCC integration tests (`test_silvus_adapter_contract`).

## References

- `silvus-mock/` - C++ implementation and reduced contract test
- `silvus-mock-go/` - deleted Go implementation with broader coverage
