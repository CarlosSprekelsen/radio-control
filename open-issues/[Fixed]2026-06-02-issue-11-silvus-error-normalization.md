# Issue 11 - Silvus adapter error normalization

**Status:** Fixed.
**Observed:** 2026-04-19, rcc C++ port architecture review.
**Fixed:** 2026-06-02.

## Original Symptom

`rcc/src/adapter/silvus_adapter.cpp` flattened Silvus JSON-RPC failures too
coarsely. Reads mapped most errors to `Unavailable`, while control methods
mapped most errors to `InvalidRange`, losing distinctions the Android client
needs for retry behavior.

## Fix

The Silvus adapter now normalizes vendor `error` values, accepting both
documented string errors and structured JSON-RPC error objects:

- range/parameter failures map to `CommandResultCode::InvalidRange`
- busy/recovering/reboot indications map to `CommandResultCode::Busy`
- unavailable/disconnected/timeout indications map to
  `CommandResultCode::Unavailable`
- unknown failures map to `CommandResultCode::InternalError`

The original vendor error payload is preserved in `CommandResult::vendor_payload`
and propagated through RCC command results into the REST error envelope
`details` field for diagnostics.

## Evidence

- `rcc/src/adapter/silvus_adapter.cpp`
- `rcc/src/command/orchestrator.cpp`
- `rcc/src/api/api_gateway.cpp`
- `rcc/test/unit/test_silvus_adapter.cpp`

## Residual Notes

This fixes deterministic adapter-side normalization for known Silvus and mock
error forms. Future lab captures from the StreamCaster 4000 should be added to
the mapping table if they reveal new vendor spellings or numeric codes.
