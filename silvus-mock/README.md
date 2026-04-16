# Silvus Mock (C++20, Standalone)

This is a standalone, stateless Silvus radio mock for ICD validation and integration testing.

- Implements Silvus ICD as defined in `docs/swad-dts/specifications/icd-radio-silvus-derived.md` (reference only)
- Northbound API per `docs/swad-dts/specifications/api-radio-spec.md` (reference only)
- Uses C++20, dts-common, and header-only Asio
- No authentication, no persistent state
- Built-in minimal web UI for status/controls

## Build

```sh
cd services/radio-control/silvus-mock
mkdir -p build && cd build
cmake .. && make
```

## Run

```sh
cd services/radio-control/silvus-mock/build
SILVUS_MOCK_HTTP_PORT=8080 SILVUS_MOCK_MAINT_PORT=50001 ./silvus-mock
```

The mock listens on `/streamscape_api` over HTTP, exposes `/status` for current radio state, and also exposes the maintenance JSON-RPC port over TCP.
## Web UI Access

Open the browser to:

- `http://localhost:8080/`
- `http://localhost:8080/index.html`

The UI provides live status, frequency/power controls, and JSON-RPC logging.

## Status API

- `http://localhost:8080/status` returns current radio state and ICD-derived parameters as JSON.
## References
- `docs/swad-dts/specifications/icd-radio-silvus-derived.md`
- `docs/swad-dts/specifications/api-radio-spec.md`
- `docs/swad-dts/designs/svc-radio-architecture.md`
- `CB-TIMING v0.3`

## Notes
- This mock is stateless and does not implement authentication.
- All protocol and API details are strictly referenced from the above documents.
