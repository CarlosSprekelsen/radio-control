# Issue 08 - Radio service port allocation and API contracts

**Status:** Fixed.
**Reviewed:** 2026-06-02.

## Resolution

The service now has explicit port allocation and API contracts:

- REST, monitor, and health: `8080` (`rcc/config/default.yaml`,
  `network.command_port`).
- SSE telemetry: `8081` (`rcc/config/default.yaml`, `telemetry.sse_port`).
- Discovery: UDP `9999`.
- Northbound OpenAPI contract:
  `services/radio-control/docs/contract/openapi.yaml`.
- Telemetry event schema:
  `services/radio-control/docs/contract/telemetry.schema.json`.
- Podman packaging and deployment expose and health-check the same ports.

The stale derived ICD references to `8002/8003` were corrected to `8080/8081`
in both `docs/swad-dts/specifications/icd-radio-silvus-derived.md` and the
service-local ICD draft.

## Residual Risk

The production endpoint address for a real StreamCaster must still be set per
deployment. The checked-in production-shape config uses
`http://172.20.0.2/streamscape_api` as a placeholder.
