# Issue 13 — Audit log is stdout-only; no rotated local file

**Status:** Open.
**Observed:** 2026-04-19, rcc C++ port architecture review.

## Symptom

[audit_logger.cpp](services/radio-control/rcc/src/audit/audit_logger.cpp) emits records via `dts::common::core::getLogger().info("[AUDIT] …")` — i.e. into the generic log stream that goes to stdout / journald. It does not write an append-only file and has no rotation policy.

Architecture §"Commissioning (Air-Gapped Profile)" and §"Observability & Logs" require a "minimal local audit file (rotated)" with retention configured via CB-TIMING v0.3. API §9 compliance checklist requires "Audit log of control actions (server-side)".

Note: the separate must-fix work in Orchestrator (actually calling `AuditLogger::record`) is a prerequisite — without that wiring, even stdout audit records aren't produced. This issue covers the *persistence* half.

## Why it matters

- In the air-gapped deployment, journald may not survive container rebuilds the way a persistent audit file on a mounted volume would.
- "Who set which radio to which power at which time" is a contractual obligation for the security officer stakeholder.
- Rotation bounds the disk footprint on the Edge Hub.

## Fix (sketch)

1. Back `AuditLogger` with an append-only file writer under `/var/log/rcc/audit.log` (path from config).
2. Add size-based rotation (e.g. 10 × 1 MiB) with CB-TIMING v0.3 parameters.
3. Keep the journald log as a secondary destination for operator visibility.
4. Decide whether to checksum each record (minor, optional).

## Related

- Must-fix in current pass: call `AuditLogger::record` from `Orchestrator::{selectRadio,setPower,setChannel}`. That work unblocks this issue but is separate.
