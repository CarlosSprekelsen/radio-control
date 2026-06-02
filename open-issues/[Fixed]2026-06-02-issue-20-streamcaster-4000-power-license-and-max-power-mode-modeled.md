# [Fixed] radio-control: StreamCaster 4000 power ceiling and max-power mode modeled

## Status

Fixed 2026-06-02.

## Owner

`services/radio-control`.

## Deviation

The Silvus adapter now accepts the documented `power_dBm` command range of
`0..39 dBm` when converting the northbound watts value to radio JSON-RPC, but
the vendor ICD states that the usable upper limit can vary by radio model and
installed license. The derived ICD also notes that `power_dBm` is ignored when
`enable_max_power=1`.

Original code did not read or model:

- StreamCaster 4000 model/license-specific maximum TX power.
- `enable_max_power` state and whether manual `power_dBm` writes are currently
  effective.
- Any resulting UI/API indication that a set-power request was accepted by the
  radio but will not affect actual output due to max-power mode.

## Why It Matters

The container can avoid rejecting valid 39 dBm requests, but it still cannot
tell whether a specific StreamCaster 4000 unit/license should expose the full
range or whether manual power control is temporarily bypassed by max-power mode.

## Required Work

1. Confirm StreamCaster 4000 licensed TX power limits in lab and document the
   allowed northbound range for DTS.
2. Decide whether RCC must read `enable_max_power` during capability ingest or
   refresh.
3. If needed, expose an adapter capability/fault indicating that manual
   `power_dBm` is currently ignored.
4. Add contract tests using captured StreamCaster 4000 JSON-RPC traces.

## Resolution

Implemented in the RCC C++ Silvus adapter and mock:

- `radios[].min_power_dbm` and `radios[].max_power_dbm` config fields now
  allow deployment/lab-confirmed StreamCaster 4000 license ceilings to narrow
  the vendor default `0..39 dBm` range without code changes.
- `SilvusAdapter` probes `enable_max_power` during connect, refresh, and before
  a manual power set. If the radio reports `enable_max_power=1`, RCC returns
  `BUSY` and does not send `power_dBm`, preventing a misleading "accepted but
  ignored" result.
- `/api/v1/radios` capability payload now reports `powerRangeSource`,
  `enableMaxPower`, and `manualPowerControl` so clients can distinguish normal
  manual power control from max-power ownership.
- The C++ Silvus mock implements `enable_max_power`; when enabled, direct
  `power_dBm` writes return success but leave the power unchanged, matching the
  vendor behavior described by the derived ICD.
- Unit/contract tests cover configured power ceilings, max-power-mode refusal,
  and mock ignored-write behavior.

No page-level vendor trace or lab capture was available in this workspace. That
auditability gap remains tracked by issue 21; the runtime behavior is now
adaptable to the lab-confirmed StreamCaster 4000 ceiling once known.

## Acceptance Criteria

- [x] StreamCaster 4000 power ceiling can be supplied from lab/deployment config.
- [x] RCC rejects values outside the configured radio/license profile, or the
  vendor default `0..39 dBm` when no narrower profile is configured.
- [x] Clients can distinguish "power set applied" from "max-power mode owns
  output" via `manualPowerControl=false`, `enableMaxPower=true`, and `BUSY`
  responses to manual power writes.

## Cross-References

- `services/radio-control/docs/StreamCaster API Manual (SILVUS).pdf`
- `docs/swad-dts/specifications/icd-radio-silvus-derived.md`
- `services/radio-control/open-issues/[Open]2026-06-02-issue-21-derived-silvus-icd-needs-page-level-vendor-traceability.md`
