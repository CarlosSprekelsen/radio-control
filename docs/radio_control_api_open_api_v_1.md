# Radio Control API – OpenAPI v1

> **Scope**: Northbound API between the Android **Radio Control App** and the **Radio Control Container (RCC)**.
> **Focus**: Minimal, stable contract for selecting a radio, setting **channel** and **power**, and receiving **telemetry**.

---

## 0. Overview
- **API Version**: `v1`
- **Base URL**: `http://<edge-hub>/api/v1`
- **Content‑Type**: `application/json; charset=utf-8`
- **Authentication**: Bearer token (short‑lived). Optional mTLS (deployment‑specific).
- **Compatibility**: Backward‑compatible additions only. Breaking changes require `v2`.

### 0.1 Changelog (v1)
- `1.0.0` — Initial freeze: radios listing, select radio, set/get power, set/get channel, SSE telemetry, health endpoints, unified error envelope.

---

## 1. Authentication & Authorization

### 1.1 Auth
- Send `Authorization: Bearer <token>` header on every request (except `/health`).

### 1.2 Roles & Scopes
- `viewer`: read‑only (list radios, get state, subscribe to telemetry)
- `controller`: all `viewer` privileges **plus** control actions (select radio, set power, set channel)

> **403** if role lacks permission.

---

## 2. Error Model (Unified Envelope)
All endpoints return **200 OK** with a success body **or** an error body with an appropriate HTTP status.

### 2.1 Success
```json
{
  "result": "ok",
  "data": { /* endpoint‑specific payload */ },
  "correlationId": "9c3b3a8e-..."
}
```

### 2.2 Error
```json
{
  "result": "error",
  "code": "INVALID_RANGE",
  "message": "Power must be between 0 and 39 dBm",
  "details": { /* optional */ },
  "correlationId": "9c3b3a8e-..."
}
```

**Standard codes**
- `BAD_REQUEST` → HTTP 400 (malformed JSON, trailing data, or structural validation failure)
- `INVALID_RANGE` → HTTP 400 (semantic validation failure: parameter value outside allowed range)
- `UNAUTHORIZED` → HTTP 401
- `FORBIDDEN` → HTTP 403
- `NOT_FOUND` → HTTP 404
- `BUSY` → HTTP 503 (retry with backoff)
- `UNAVAILABLE` → HTTP 503 (radio rebooting/soft‑boot)
- `INTERNAL` → HTTP 500

> **Distinction**: `BAD_REQUEST` indicates the request structure is invalid (JSON parse error, unknown fields, trailing data). `INVALID_RANGE` indicates the request structure is valid but parameter values fail semantic validation (e.g., power outside 0-39 dBm range). Both return HTTP 400, but with different error codes to guide client remediation.

> Error mapping normalizes vendor/adapter errors to the codes above. See Architecture §8.5 for normalization rules.

---

## 3. Resources

### 3.1 GET `/capabilities`
Lists negotiated API/transport capabilities.

**Response 200**
```json
{
  "result": "ok",
  "data": {
    "telemetry": ["sse"],
    "commands": ["http-json"],
    "version": "1.0.0"
  }
}
```

---

### 3.2 GET `/radios`
List known radios and current selection/state snapshot.

**Response 200**
```json
{
  "result": "ok",
  "data": {
    "activeRadioId": "silvus-01",
    "items": [
      {
        "id": "silvus-01",
        "model": "Silvus-XXXX",
        "status": "online",
        "capabilities": {
          "minPowerDbm": 0,
          "maxPowerDbm": 39,
          "channels": [
            {"index": 1, "frequencyMhz": 2412},
            {"index": 2, "frequencyMhz": 2417},
            {"index": 3, "frequencyMhz": 2422}
          ]
        },
        "state": {
          "powerDbm": 30,
          "frequencyMhz": 2412
        }
      }
    ]
  }
}
```

---

### 3.3 POST `/radios/select`
Select the active radio for subsequent operations.

**Request**
```json
{ "id": "silvus-01" }
```

**Responses**
- **200**
```json
{ "result": "ok", "data": { "activeRadioId": "silvus-01" } }
```
- **404** `NOT_FOUND` if radio id is unknown.

---

### 3.4 GET `/radios/{id}`
Get a single radio’s details and current state.

**Response 200**
```json
{
  "result": "ok",
  "data": {
    "id": "silvus-01",
    "model": "Silvus-XXXX",
    "status": "online",
    "capabilities": {
      "minPowerDbm": 0,
      "maxPowerDbm": 39,
      "channels": [
        {"index": 1, "frequencyMhz": 2412},
        {"index": 2, "frequencyMhz": 2417},
        {"index": 3, "frequencyMhz": 2422}
      ]
    },
    "state": {
      "powerDbm": 30,
      "frequencyMhz": 2412
    }
  }
}
```

---

### 3.5 GET `/radios/{id}/power`
Return current **TX power setting** for a radio (dBm).

**Response 200**
```json
{ "result": "ok", "data": { "powerDbm": 30 } }
```

---

### 3.6 POST `/radios/{id}/power`
Set **TX power** for a radio (dBm).

**Request**
```json
{ "powerDbm": 28 }
```

**Rules**
- Range: **0..39** (accuracy typically 10..39).
- Request is idempotent.

**Responses**
- **200**
```json
{ "result": "ok", "data": { "powerDbm": 28 } }
```
- **400** `INVALID_RANGE`
- **503** `BUSY` or `UNAVAILABLE` if the adapter/radio is temporarily unavailable; client should retry with backoff.

---

### 3.7 GET `/radios/{id}/channel`
Return the current channel as **frequency** in MHz and (if available) UI channel index.

**Response 200**
```json
{ "result": "ok", "data": { "frequencyMhz": 2412, "channelIndex": 1 } }
```

**Note**: The `channelIndex` may be `null` if the current frequency is not in the derived channel set per Architecture §13.

---

### 3.8 POST `/radios/{id}/channel`
Set radio channel by **UI channel index** **or** by **frequency**.

**Request (by index)**
```json
{ "channelIndex": 3 }
```

**Request (by frequency)**
```json
{ "frequencyMhz": 2422 }
```

**Request (both provided)**
```json
{ "channelIndex": 3, "frequencyMhz": 2422 }
```

**Rules**
- Frequency must be within the radio's allowed ranges.
- If both `channelIndex` and `frequencyMhz` are provided, **frequency takes precedence** per Architecture §13.
- Setting frequency may cause a **soft‑boot**; subsequent calls may briefly return `UNAVAILABLE`.

**Responses**
- **200**
```json
{ "result": "ok", "data": { "frequencyMhz": 2422, "channelIndex": 3 } }
```
- **400** `INVALID_RANGE` (illegal frequency/index)
- **503** `UNAVAILABLE` (radio applying change)

---

### 3.9 GET `/telemetry`  (Server‑Sent Events)
Subscribes to the live telemetry/event stream.

**Request headers**
```
GET /api/v1/telemetry
Accept: text/event-stream
Cache-Control: no-cache
```

**Events**
- `ready` — Initial snapshot after connect
- `state` — Periodic/changed state (power, channel, faults)
- `powerChanged` — Acknowledged power change
- `channelChanged` — Acknowledged channel change
- `fault` — Fault/event notifications

**Examples**
```
event: ready
data: {"snapshot":{"activeRadioId":"silvus-01","powerDbm":30,"frequencyMhz":2412}}

id: 42
event: channelChanged
data: {"radioId":"silvus-01","frequencyMhz":2422,"channelIndex":3}
```

> Supports `Last-Event-ID` header for resume. See **Radio Control Telemetry SSE v1** for complete event schemas, buffering, and resume semantics.

---

### 3.10 GET `/health`
Liveness/readiness probe.

**Response 200**
```json
{ "status": "ok", "uptimeSec": 1234, "version": "1.0.0" }
```

**Response 503**
```json
{ "status": "degraded", "reason": "adapter.unavailable" }
```

---

## 4. Data Models

### 4.1 Radio
```json
{
  "id": "string",
  "model": "string",
  "status": "online|offline|recovering",
  "capabilities": {
    "minPowerDbm": 0,
    "maxPowerDbm": 39,
    "channels": [
      {"index": 1, "frequencyMhz": 2412},
      {"index": 2, "frequencyMhz": 2417},
      {"index": 3, "frequencyMhz": 2422}
    ]
  },
  "state": {
    "powerDbm": 30,
    "frequencyMhz": 2412
  }
}
```

> **Note**: The `channels` array is derived from radio capabilities and regional constraints per Architecture §13. Channel indices are 1-based.

### 4.2 Events
- **`ready`**
```json
{ "snapshot": { "activeRadioId": "silvus-01", "powerDbm": 30, "frequencyMhz": 2412 } }
```
- **`state`**
```json
{ "radioId": "silvus-01", "powerDbm": 30, "frequencyMhz": 2412 }
```
- **`powerChanged`**
```json
{ "radioId": "silvus-01", "powerDbm": 28 }
```
- **`channelChanged`**
```json
{ "radioId": "silvus-01", "frequencyMhz": 2422, "channelIndex": 3 }
```
- **`fault`**
```json
{ "radioId": "silvus-01", "code": "UNAVAILABLE", "message": "Radio applying frequency change" }
```

---

## 5. Validation Rules (Normative)
- **Power**: 0–39 dBm (accuracy typically 10–39 dBm). Reject out‑of‑range with `INVALID_RANGE`.
- **Channel/Frequency**: must be within the derived channel set (see Architecture §13) or within allowed ranges derived from the radio/region configuration. When both `channelIndex` and `frequencyMhz` are provided, frequency takes precedence.
- **Idempotency**: Repeat submits of the same desired state return `200` and current state.
- **Backoff Guidance**: On `BUSY`/`UNAVAILABLE`, use backoff policies defined in **CB-TIMING v0.3**.

---

## 6. Examples (cURL)

### 6.1 List radios
```bash
curl -s -H "Authorization: Bearer $TOKEN" \
  http://edge-hub.local/api/v1/radios | jq
```

### 6.2 Select radio
```bash
curl -s -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"id":"silvus-01"}' \
  http://edge-hub.local/api/v1/radios/select | jq
```

### 6.3 Set power (dBm)
```bash
curl -s -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"powerDbm":28}' \
  http://edge-hub.local/api/v1/radios/silvus-01/power | jq
```

### 6.4 Set channel (by index)
```bash
curl -s -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"channelIndex":3}' \
  http://edge-hub.local/api/v1/radios/silvus-01/channel | jq
```

### 6.5 Telemetry (SSE)
```bash
curl -N -H "Authorization: Bearer $TOKEN" \
  -H "Accept: text/event-stream" \
  http://edge-hub.local/api/v1/telemetry
```

---

## 7. Versioning & Extensions
- Additive changes (new optional fields, new endpoints) **must not** break clients.
- New transports (e.g., WebSocket `/telemetry/ws`, MQTT topics) may be added without changing payload schemas.
- Breaking changes require a new base path (e.g., `/api/v2`).

---

## 8. Non‑Goals (For clarity)
- Southbound vendor protocols (Silvus JSON‑RPC, etc.) are **out of scope** here and covered in the separate **ICD**.
- Certificate lifecycle, provisioning, and radio maintenance are deployment concerns outside this API.

---

## 9. Compliance Checklist (for IV&V)
- [ ] Endpoints implemented exactly as defined
- [ ] Role enforcement (`viewer`, `controller`)
- [ ] Unified error envelope & HTTP status codes
- [ ] SSE supports `Last-Event-ID` resume
- [ ] Idempotent control actions
- [ ] Audit log of control actions (server‑side)

