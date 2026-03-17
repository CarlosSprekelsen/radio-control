\# Radio Control Telemetry \– Human\-Readable (SSE v1)

\> **Scope**: Northbound telemetry stream from the **Radio Control Container (RCC)** to the **Radio Control App**.\
\> **Transport**: HTTP Server\-Sent Events \(SSE\).\
\> **Version**: `v1` \(payloads frozen; additive extensions only\).

\---

\## 0\. Overview
\- **Base URL**: `http://<edge\-hub>/api/v1`\
\- **Telemetry Endpoint**: `GET /telemetry` \(SSE\)\
\- **Content Type**: `text/event\-stream; charset\=utf\-8`\
\- **Auth**: `Authorization: Bearer <token>` \(short\-lived\).\
\- **Backwards compatibility**: New event types/fields may be added; existing fields remain stable.

\### 0\.1 Changelog \(v1\)
\- `1\.0\.0` \– Initial freeze: `ready`, `state`, `channelChanged`, `powerChanged`, `fault`, `heartbeat`.

\---

\## 1\. Connect \(SSE\)
\### 1\.1 Request
```
GET /api/v1/telemetry HTTP/1.1
Host: <edge-hub>
Accept: text/event-stream
Cache-Control: no-cache
Authorization: Bearer <token>
```
\### 1\.2 Successful Response Headers
```
HTTP/1.1 200 OK
Content-Type: text/event-stream; charset=utf-8
Cache-Control: no-cache
Connection: keep-alive
```
\### 1\.3 Reconnect \(Resume\)
Clients \*should\* send `Last\-Event\-ID` on reconnect to resume from the last processed event ID. The server \*may\* replay up to the last **N** buffered events per client, where N is defined in **CB-TIMING v0.3**.

Event IDs are monotonic per radio, starting from 1. The system maintains separate event streams per radio to ensure proper ordering and resume semantics.

```
GET /api/v1/telemetry
Last-Event-ID: 42
```

\---

\## 2\. Event Stream Semantics
Each event consists of optional `id:`, required `event:` name, and `data:` JSON payload on a single line \(wrapped by the client as needed\). A blank line terminates the event.

\### 2\.1 Common Envelope
\- All payloads are **JSON objects**.\
\- Timestamps are **ISO\-8601 UTC** strings unless otherwise noted.\
\- Numeric units are explicit in field names \(e\.g\., `frequencyMhz`, `powerDbm`\).

\### 2\.2 Core Event Types
\#### a\) `ready`
Emitted once per connection with a snapshot of current state.
```
event: ready
data: {"snapshot":{"activeRadioId":"silvus-01","radios":[{"id":"silvus-01","model":"Silvus-XXXX","status":"online","state":{"powerDbm":30,"frequencyMhz":2412}}]}}
```

\#### b\) `state`
Periodic state or change\-driven update for a radio.
```
id: 41
event: state
data: {"radioId":"silvus-01","powerDbm":30,"frequencyMhz":2412,"status":"online","ts":"2025-10-02T08:20:15Z"}
```

\#### c\) `channelChanged`
Acknowledged change to frequency/channel.
```
id: 42
event: channelChanged
data: {"radioId":"silvus-01","frequencyMhz":2422,"channelIndex":3,"ts":"2025-10-02T08:20:20Z"}
```

\#### d\) `powerChanged`
Acknowledged change to power setting.
```
id: 43
event: powerChanged
data: {"radioId":"silvus-01","powerDbm":28,"ts":"2025-10-02T08:20:25Z"}
```

\#### e\) `fault`
Fault or exceptional condition, with normalized code and human message.
```
id: 44
event: fault
data: {"radioId":"silvus-01","code":"UNAVAILABLE","message":"Radio applying frequency change","details":{"retryMs":1000},"ts":"2025-10-02T08:20:26Z"}
```

\#### f\) `heartbeat`
Lightweight keepalive at a low cadence.
```
event: heartbeat
data: {"ts":"2025-10-02T08:20:30Z"}
```

\---

\## 3\. Data Model \(Payload Schemas\)
\### 3\.1 `Radio` \(snapshot items\)
```
{
  "id": "string",
  "model": "string",
  "status": "online|offline|recovering",
  "state": {
    "powerDbm": 0,
    "frequencyMhz": 0
  }
}
```
\### 3\.2 `state` Event
```
{
  "radioId": "string",
  "powerDbm": 0,
  "frequencyMhz": 0,
  "status": "online|offline|recovering",
  "ts": "YYYY-MM-DDThh:mm:ssZ"
}
```
\### 3\.3 `channelChanged` Event
```
{
  "radioId": "string",
  "frequencyMhz": 0,
  "channelIndex": 0,
  "ts": "YYYY-MM-DDThh:mm:ssZ"
}
```
\### 3\.4 `powerChanged` Event
```
{
  "radioId": "string",
  "powerDbm": 0,
  "ts": "YYYY-MM-DDThh:mm:ssZ"
}
```
\### 3\.5 `fault` Event
```
{
  "radioId": "string",
  "code": "INVALID_RANGE|BUSY|UNAVAILABLE|INTERNAL",
  "message": "string",
  "details": {"retryMs": 0},
  "ts": "YYYY-MM-DDThh:mm:ssZ"
}
```
\### 3\.6 `ready` Event
```
{
  "snapshot": {
    "activeRadioId": "string",
    "radios": [<Radio>, ...]
  }
}
```

\---

\## 4\. Timing, Rate & Ordering
\- **Event ordering**: Monotonic per radio; global order best\-effort across radios. Use `id:` to detect gaps.\
\- **Heartbeat**: interval and jitter defined in **CB-TIMING v0.3**.\
\- **State cadence**: change\-driven; background tick rate defined in **CB-TIMING v0.3**.\
\- **Backoff guidance**: on `fault.code \= BUSY|UNAVAILABLE` use policies defined in **CB-TIMING v0.3**.

\---

\## 5\. Error Codes \(Normalized\)
| Code | HTTP mapping \(for related command APIs\) | Client guidance |
|\-\-\-|\-\-\-|\-\-\-|
| `INVALID_RANGE` | 400 | Fix parameters; do not retry unchanged. |
| `BUSY` | 503 | Retry with backoff. |
| `UNAVAILABLE` | 503 | Radio soft\-boot/applying change; retry with backoff. |
| `INTERNAL` | 500 | Retry with jittered backoff; log for diagnostics. |

> **Note**: Error codes must match the OpenAPI specification exactly. See Architecture §8.5 for normalization rules.

\---

\## 6\. Client Integration Notes
\- Treat `ready` as the initial state; do not issue UI confirmations until the corresponding `channelChanged`/`powerChanged` arrives.\
\- Use `Last\-Event\-ID` to avoid duplicate UI updates on reconnect.\
\- Buffer a small local queue to coalesce rapid changes and prevent UI thrash.\
\- Treat missing `channelIndex` as unknown; display `frequencyMhz` as authoritative.

\---

\## 7\. Examples
\### 7\.1 `curl` \(observe stream\)
```
curl -N -H "Authorization: Bearer $TOKEN" -H "Accept: text/event-stream" \
  http://edge-hub.local/api/v1/telemetry
```
\### 7\.2 Minimal JavaScript SSE Client
```
const es = new EventSource("/api/v1/telemetry", { withCredentials: false });
es.addEventListener("ready", e => console.log(JSON.parse(e.data)));
es.addEventListener("state", e => renderState(JSON.parse(e.data)));
es.addEventListener("channelChanged", e => ackChan(JSON.parse(e.data)));
es.addEventListener("powerChanged", e => ackPwr(JSON.parse(e.data)));
es.addEventListener("fault", e => showFault(JSON.parse(e.data)));
```

\---

\## 8\. Security & Privacy
\- Tokens must be validated on connect; unauthenticated requests are rejected.\
\- Do not include secrets or raw vendor error strings in events; map to normalized codes and redact sensitive data.\
\- In air\-gapped deployments, bind the API to local interfaces and enforce OS\-level firewall rules.

\---

\## 9\. Versioning & Extensions
\- New event types must be optional; clients ignore unknown `event:` names.\
\- New fields must be optional with safe defaults.\
\- Breaking changes require a new base path \(e\.g\., `/api/v2/telemetry`\).

\---

\## 10\. Compliance Checklist \(IV\&V\)
\- \[ \] Emits `ready` within **\<1 s** of connect.\
\- \[ \] Emits `channelChanged`/`powerChanged` after successful control actions.\
\- \[ \] Supports `Last\-Event\-ID` resume.\
\- \[ \] Heartbeat at configured cadence.\
\- \[ \] Normalizes faults to standard codes.

