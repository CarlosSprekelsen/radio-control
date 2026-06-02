# Issue 18 - `ready` snapshot evicted from ring buffer

**Status:** Fixed.
**Original observation:** 2026-04-19, bench validation with APK radio
controller against the radio-control container.
**Reviewed:** 2026-06-02.

## Original Symptom

The APK waited for an SSE `ready` event before marking the radio-control stream
connected. Long-running containers could evict the startup-only `ready` event
from the ring buffer, leaving late clients with heartbeats but no initial
snapshot.

## Resolution

`rcc/src/telemetry/telemetry_hub.cpp` now stores the last ready payload via
`rememberReadySnapshot()` and periodically re-emits it from the heartbeat timer
(`kReadyReemitPeriod`). This keeps a fresh `ready` event in the SSE ring buffer
for late-joining APK clients without changing `dts-common` replay semantics or
broadcasting duplicate ready events on every client connection.

The implementation was already present during the 2026-06-02 review, so the old
open issue file was stale rather than an active live-integration blocker.

## Evidence

- `TelemetryHub::publishReady()` stores and publishes the snapshot payload.
- `TelemetryHub::Impl::scheduleHeartbeat()` re-emits the stored ready payload
  every `kReadyReemitPeriod` heartbeats.
- Existing SSE integration tests cover server startup, heartbeat, state, fault,
  and event delivery paths.

## Residual Risk

There is no dedicated regression test that shrinks the ring, evicts the first
ready event, and asserts a later ready re-emission. Add one when the local build
toolchain is available.
