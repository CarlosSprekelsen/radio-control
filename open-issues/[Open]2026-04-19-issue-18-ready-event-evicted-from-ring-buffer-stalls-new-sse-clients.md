# Issue 18 — `ready` snapshot evicted from ring buffer; new SSE clients never see it

**Status:** Open.
**Severity:** High (blocks APK "connected" state).
**Observed:** 2026-04-19, TT→rcc bench validation (apk-radio-controller against radio-control-bench container).

## Symptom

The Radio Controller APK connects to `GET /api/v1/telemetry`, receives the retry hint plus an unbroken stream of `heartbeat` events with increasing sequence IDs, but **never a `ready` event**. Concrete capture:

```
retry: 1000

id: 109
event: heartbeat
data: {"correlationId":"…","ts":"2026-04-19T08:47:21.252Z"}

id: 366   ← jump: events 110-365 evicted
event: heartbeat
…
```

The APK's connection coordinator waits for `ready` before marking the connection `connected` and stays stuck in `waitingForReady`, which the UI renders as "Connection error".

## Root Cause

[application.cpp](rcc/src/core/application.cpp) calls `TelemetryHub::publishReady(snapshot)` **once** at container startup. That event lands in the EventBus ring buffer as sequence id N. The heartbeat timer (default 5s) then steadily publishes `heartbeat` events at seq N+1, N+2, … The ring buffer (`cfg.telemetry.event_buffer_size = 512`) fills up in ~42 minutes of heartbeats, at which point `ready` is overwritten.

After eviction, any new SSE client connecting with no `Last-Event-ID` calls `EventBus::replay_since(0, …)`, which walks the ring — but `ready` is gone. There is no "initial snapshot on connect" mechanism: the SSE client-connect path (`SSEConnection` in dts-common) replays only what's in the ring, it does not synthesise a welcome message.

## Why it matters

- Every long-running rcc instance becomes un-connectable by new APKs after ~hour-scale uptime.
- The failure mode is silent: container looks healthy, curl looks healthy, APK looks stuck. Debugging path is non-obvious (cost me ~2h to isolate; captured here so the next person doesn't pay the same tax).
- Violates the SSE v1 spec intent: `ready` is described as the initial state handshake; treating it as an evictable ring entry was not the intent.

## Fix (sketch)

Preferred (server-side, minimal): hold the last-published `ready` snapshot in `TelemetryHub` as a sticky "welcome payload", and have the SSE server call a new `onClientConnect(socket)` hook that writes that frame before starting the normal replay. The hook avoids touching the ring buffer or EventBus semantics.

Rough shape:

```cpp
// TelemetryHub
std::optional<nlohmann::json> lastReady_;
void publishReady(json snapshot) {
    lastReady_ = snapshot;
    impl_->publishEvent("ready", {{"snapshot", snapshot}});
}
std::optional<nlohmann::json> lastReadySnapshot() const { return lastReady_; }

// SSEConnection — new hook, called after headers, before replay
if (auto welcome = hub.lastReadySnapshot()) {
    writeEvent("ready", {{"snapshot", *welcome}}, /*id=*/0);  // id=0 = not part of replay stream
}
```

Alternatives:
- Re-publish `ready` to the bus on every client connect (pollutes all existing subscribers with duplicate events — rejected).
- Dedicate a separate "pinned events" ring (overkill for one event).
- Just bump buffer size (masks the bug — rejected).

## Related

- apk-radio-controller [Open] issue 02 — APK-side defensive handling of the same bug.
- Must-fix: telemetry hub signature already accepts the snapshot; adding `lastReady_` is additive.
- Monitor page (rcc) consumes the same SSE stream and will benefit from the fix.
