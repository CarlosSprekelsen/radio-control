# Issue 10 — Silvus adapter blocks the REST worker; no per-radio serialization

**Status:** Open.
**Observed:** 2026-04-19, rcc C++ port architecture review.

## Symptom

[services/radio-control/rcc/src/adapter/silvus_adapter.cpp](services/radio-control/rcc/src/adapter/silvus_adapter.cpp) `sendHttpPost` creates a brand-new `asio::io_context` per call and blocks on `asio::read_until` / `asio::read`. It is invoked from the REST server's worker thread via `Orchestrator::setPower` / `setChannel`. A single slow or unresponsive radio therefore stalls every concurrent REST request, including `GET /health` and unrelated radios.

[services/radio-control/rcc/src/command/orchestrator.cpp](services/radio-control/rcc/src/command/orchestrator.cpp) has no per-radio lock, queue or strand — the only synchronisation is the mutex inside each adapter, which serializes *within* an adapter but does not enforce the architecture requirement that at most one in-flight control command per radio is dispatched.

## Why it matters

- Breaks Quality Scenario 1 (§10): <100 ms p95 command latency under 5 concurrent clients.
- Violates Cross-cutting §"Concurrency & Serialization": per-radio FIFO, recovering gate, independence across radios.
- Undermines the power-aware event-first posture: a stuck radio starves healthy ones.

## Fix (sketch)

1. Share the application's `asio::io_context` with the adapter and use async HTTP (resolver + socket async ops) so the REST worker thread isn't blocked; OR keep sync HTTP but run adapter calls on a dedicated per-radio strand/thread so the REST loop stays responsive.
2. Add a per-radio serializer in `RadioManager` / `Orchestrator`: an `asio::strand` or `std::mutex` keyed by `radioId`, plus a bounded FIFO for queued commands (§ Concurrency & Serialization).
3. Honour the "recovering gate" — while a radio is in `Recovering` state, reject new control commands with `BUSY` instead of blocking.

## Related

- Issue 11 (error taxonomy) — the normalized codes needed to signal `BUSY` / `UNAVAILABLE` assume the adapter isn't blocking callers indefinitely.
- Issue 12 (duty-cycled probing + soft-boot) — the recovering state is also what gates probe cadence.
