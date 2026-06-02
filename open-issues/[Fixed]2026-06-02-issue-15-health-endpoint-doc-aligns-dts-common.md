# Issue 15 - Health endpoint documentation aligned with dts-common

**Status:** Fixed.
**Observed:** 2026-04-19, rcc C++ port architecture review.
**Fixed:** 2026-06-02.

## Original Symptom

The Radio Control API v1 document specified a small service-local health shape:

```json
{ "status": "ok", "uptimeSec": 1234, "version": "1.0.0" }
```

The RCC implementation correctly returns the shared DTS health snapshot from
`dts-common`, matching the cross-service observability model.

## Fix

`docs/radio_control_api_open_api_v_1.md` and `docs/contract/openapi.yaml` now
document the canonical DTS health snapshot shape with `status`, `checks`,
`metrics`, `version`, and `timestamp`.

## Evidence

- `docs/radio_control_api_open_api_v_1.md`
- `docs/contract/openapi.yaml`
- `rcc/src/api/api_gateway.cpp`
- `libs/dts-common/include/dts/common/health/health_types.hpp`
