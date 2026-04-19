# Issue 11 — Silvus adapter error normalisation is coarser than ADR-003

**Status:** Open.
**Observed:** 2026-04-19, rcc C++ port architecture review.

## Symptom

[silvus_adapter.cpp](services/radio-control/rcc/src/adapter/silvus_adapter.cpp):

- `connect()` / `refresh_state()`: *any* HTTP non-200 or JSON `error` field → `CommandResultCode::Unavailable`.
- `set_power()` / `set_channel()`: *any* radio `error` field → `CommandResultCode::InvalidRange`.

This flattens every vendor-specific failure into two buckets and loses the distinction between `INVALID_RANGE`, `BUSY`, `UNAVAILABLE`, and `INTERNAL` that the Android client must be able to act on (retry vs. fix parameters vs. stop).

[orchestrator.cpp](services/radio-control/rcc/src/command/orchestrator.cpp) then remaps any non-OK adapter result to `CommandResult::Code::InternalError`, making the loss of fidelity irreversible before the gateway even sees it. (The second half of this — orchestrator flattening — is a must-fix in the current pass; this issue tracks the vendor-side mapping table that is deeper work.)

## Why it matters

- Violates ADR-003: "Normalize vendor errors to container error codes via adapter layer" with a documented mapping table.
- Breaks CB-TIMING backoff semantics: clients retry `BUSY`/`UNAVAILABLE` with backoff, but won't retry `INVALID_RANGE` — flattening all three to the same code gives the wrong retry behaviour.
- Vendor diagnostic payload is not preserved in a `details` field as required by architecture §"Vendor Error Format Ambiguity".

## Fix (sketch)

1. Add a Silvus-specific normaliser that inspects the vendor error object (string vs object form, known codes, HTTP status class) and returns the mapped `CommandResultCode` + original payload as opaque `details`.
2. Add a table-driven test in `test_silvus_adapter_contract.cpp` that covers the five rows of the architecture §"Error Model & Normalization" table.
3. Cross-check against TPN Silvus ICD when it lands.

## Related

- Issue 10 (async adapter) — per-radio `BUSY` depends on a working recovering gate.
- Issue 12 (soft-boot) — the `UNAVAILABLE` code is specifically the post-freq-change path.
