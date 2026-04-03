# Issue 08 -- Radio service architecture: Missing port allocation and API contracts

## Status

Open (CDR documentation gap -- deferred service awaiting ICD)

## Owner

Radio Service / `services/radio-control`

## Summary

The Radio Service architecture document lacks explicit port allocation and
formal API endpoint definitions. This contrasts with the mature services which
fully document their port pattern and REST/SSE endpoint tables.

## Required Work

1. Define the base `apiPort` and resulting service port bindings.
2. Inline formal endpoint definitions for operations such as `setPower`,
   `setChannel`, and `selectRadio`.
3. Replace abstract southbound vendor protocol references with concrete
   interface mapping where ICD clearance permits.

## Context

The service is deferred pending vendor ICD availability. These gaps are
acceptable temporarily but must be closed before CDR.

## Traceability

- `docs/swad-dts/designs/sys-dts-architecture.md` v2.5
- ADR-003
