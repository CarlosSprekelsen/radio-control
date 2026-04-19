# Issue 15 — /api/v1/health shape diverges from API v1 spec (doc fix, not code)

**Status:** Open (docs).
**Observed:** 2026-04-19, rcc C++ port architecture review.

## Symptom

API v1 §3.10 specifies `GET /health` as:

```json
{ "status": "ok", "uptimeSec": 1234, "version": "1.0.0" }
```

[api_gateway.cpp](services/radio-control/rcc/src/api/api_gateway.cpp) returns the cross-service `dts::common::health::HealthSnapshot` shape — flat `{status, checks, metrics, version, timestamp}` — which is what every other DTS service emits (biometric-control, camera-recorder, gpsd-proxy, lrf-control).

## Why it matters

- Cross-service consistency is worth more than matching a single-service spec. Telco observability tooling keys off the DTS shape.
- The API v1 document predates the DTS health schema work in dts-common.

## Fix

Update the Radio Control API v1 spec §3.10 to reference the DTS health schema (point to `libs/dts-common/include/dts/common/health/health_types.hpp`) rather than the inline minimal JSON. Update the IV&V checklist accordingly.

No code change required.

## Related

- Cross-cutting: every DTS service uses the same dts-common health schema. The API spec was written before that convergence.
