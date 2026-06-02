# [Fixed] radio-control: adopt TimeAuthority for outbound telemetry; verify discovery participation

## Status

Open. Deviation from design (discovery + trusted-clock).

## Owner

`services/radio-control`.

## Deviation

`docs/swad-dts/designs/xcut-service-discovery.md` lists `radio-service` as a
discovery participant announcing `rest` + `sse`, and
`docs/swad-dts/designs/xcut-trusted-clock-design.md` lists radio outbound events
among tactical telemetry that should be EUD-time-aligned. A survey of
`services/radio-control/src` found **no** `DiscoveryResponder`/`DiscoveryClient`
and **no** `TimeAuthority`/`TrustedClock` usage.

## Required work

1. Confirm whether radio-control already announces `radio-service` (if via a
   non-src path, document it; if not implemented, add a `DiscoveryResponder`
   using `dts-common`, matching the camera/biometric/LRF pattern).
2. Stamp outbound tactical SSE telemetry (radio alerts, status, heartbeat) with
   `dts::common::core::TimeAuthority::instance().nowIso8601Ms()` instead of the
   host clock. Leave logs/health/retry timers on host/monotonic time per the
   adoption rules.
3. If radio-control depends on `location-service` for OWN position, consume its
   `time`/`timeQuality` via the same `DiscoverySseSubscriber.onAnnouncement`
   path LRF uses; otherwise observe peer announcements opportunistically.
4. Link the dts-common Makefile/CMake source list so `trusted_clock.cpp` and
   `time_authority.cpp` are compiled (see the lrf-control Makefile fix for the
   pattern).

## Acceptance criteria

- radio-service announce verified or added.
- Outbound radio telemetry timestamps come from `TimeAuthority`.
- Build + test pass.

## Cross-references

- `docs/swad-dts/designs/xcut-trusted-clock-design.md`
- `services/lrf-control/open-issues/[Open]2026-06-01-issue-46-lrf-lock-clock-to-eud-time.md`
