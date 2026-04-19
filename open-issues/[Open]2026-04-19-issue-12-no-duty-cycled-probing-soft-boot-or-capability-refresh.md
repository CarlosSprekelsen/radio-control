# Issue 12 — No duty-cycled probing, no soft-boot UNAVAILABLE handling, no capability re-ingest on re-attach

**Status:** Open.
**Observed:** 2026-04-19, rcc C++ port architecture review.

## Symptom

Three related architecture requirements are unimplemented in the rcc C++ port:

1. **Duty-cycled probing (§8 Power-Aware Operating Modes).**
   [silvus_adapter.cpp](services/radio-control/rcc/src/adapter/silvus_adapter.cpp) has a `refresh_state()` method but nothing schedules it. There is no Normal/Recovering/Offline state machine, no probe cadence from CB-TIMING v0.3, no bounded backoff. The container therefore never updates radio state on its own — only operator-triggered commands move state.

2. **Soft-boot UNAVAILABLE window (API §3.8, Telemetry §5).**
   After `POST /radios/{id}/channel`, the radio may soft-boot. The spec requires subsequent calls to briefly return `UNAVAILABLE` so the client backs off. [silvus_adapter.cpp](services/radio-control/rcc/src/adapter/silvus_adapter.cpp) `set_channel()` does not arm any recovering window — it returns `Ok` and accepts further commands immediately.

3. **Capability refresh on re-attach (§5.6 "Loss & Re-attach").**
   [radio_manager.cpp](services/radio-control/rcc/src/radio/radio_manager.cpp) `start()` connects each adapter once and never reconnects. On link loss there is no loss event published, no re-connect loop, and no re-ingest of `supported_frequency_profiles`. Capabilities cached at startup remain in use indefinitely.

## Why it matters

- Defeats the "event-first, power-aware" posture the architecture depends on for battery life.
- Breaks the Android App retry story for freq changes: without `UNAVAILABLE`, the UI cannot distinguish "radio is rebooting, wait" from "command rejected, fix params".
- After a radio swap or region config change, the channel derivation becomes stale without the user noticing.

## Fix (sketch)

1. Add a `RadioHealthMonitor` that owns the Normal/Recovering/Offline state machine per radio, runs `refresh_state()` on an `asio::steady_timer` at CB-TIMING cadences, and publishes state-transition events via `TelemetryHub`.
2. On `set_channel()` success, arm a short "soft-boot" window during which the radio is `Recovering`; new commands to that radio return `BUSY`/`UNAVAILABLE` until the window clears.
3. On connect failure / probe failure, transition to `Offline`, publish a fault, and retry with exponential backoff up to a CB-TIMING ceiling. On reconnect, re-call `supported_frequency_profiles` and publish a `state` event plus any derived channel-set change.

## Related

- Issue 10 (adapter serialization) — the recovering gate belongs in the same orchestration layer.
- Issue 11 (error normalisation) — `BUSY` vs `UNAVAILABLE` semantics assume this state machine exists.
- Issue 14 (channel/region derivation) — capability refresh is the trigger for recomputing the channel map.
