# RCC C++20 Component Design Notes

## Overview
- C++20 service reimplements the Radio Control Container described in `../docs/radio_control_container_architecture_v1.md`.
- Follow container conventions established by `BioSensorContainer` and other DTS services.
- Reuse `dts-common` targets:
  - `dts-common-core` for utilities/logging/config helpers.
  - `dts-common-net` for REST router, SSE server, auth primitives, health checks.
- Top-level executable target: `radio-control-container` (`rcc` binary).
- Primary namespaces: `rcc::api`, `rcc::auth`, `rcc::command`, `rcc::radio`, `rcc::telemetry`, `rcc::adapter`, `rcc::config`, `rcc::audit`.

## API Gateway (`rcc::api`)
- Responsibilities: host HTTP/JSON endpoints per OpenAPI (`GET /radios`, `POST /radios/select`, `POST /radios/{id}/power`, `POST /radios/{id}/channel`, `GET /telemetry`, `GET /capabilities`, `/health`).
- Uses `dts::common::net::RestServer` and router.
- Converts HTTP requests into command layer calls; wraps responses in standard envelopes.
- SSE endpoint delegates to telemetry hub (`TelemetryHubEndpoint`).
- Applies rate limiting via `dts-common` middlewares; enforces auth scopes using `rcc::auth::ScopeChecker`.

## Auth (`rcc::auth`)
- Responsibilities: validate bearer tokens, enforce roles (`viewer`, `controller`).
- Components:
  - `TokenValidator` wraps `dts::common::auth::JwtValidator`.
  - `ScopeChecker` maps OpenAPI operations to required scopes; shared with API handlers.
  - `AuthContext` extracted per request and propagated.
- Provides middleware for REST server and SSE connections.

## Command Orchestrator (`rcc::command`)
- Responsibilities: normalize and execute commands (`selectRadio`, `setPower`, `setChannel`) and propagate events.
- Key classes:
  - `CommandOrchestrator` with methods `select_radio`, `set_power`, `set_channel`.
  - `CommandBudget` enforces per-radio serialization and timing budgets (leverages `asio::strand`).
- Interacts with `rcc::radio::RadioManager`, `rcc::config::ChannelMapper`, `rcc::telemetry::TelemetryHub`, `rcc::audit::AuditLogger`.
- Applies ADR-003 error normalization.

## Radio Manager (`rcc::radio`)
- Responsibilities: maintain inventory, discover radios, expose adapters, track active radio.
- Components:
  - `RadioDescriptor` (id, model, capabilities, transport).
  - `RadioInventory` thread-safe store.
  - `RadioManager` handles discovery, lifecycle (Normal/Recovering/Offline states), scheduling resilience probes per §8.3.
  - `ActiveRadioTracker` ensures single default target for commands.
- Relies on `asio::io_context` for timers.
- Publishes state changes via telemetry hub.

## Telemetry Hub (`rcc::telemetry`)
- Responsibilities: multiplex events to SSE clients, buffer for replay, forward command outcomes.
- Components:
  - `TelemetryHub` wraps `dts::common::telemetry::EventBus`.
  - `TelemetryPublisher` helper for standard events (`ready`, `state`, `channelChanged`, `powerChanged`, `fault`).
  - `TelemetrySerializer` ensures payload schemas match AsyncAPI spec.
- SSE endpoint uses `dts::common::telemetry::SSEServer`.
- Supports capability negotiation (`GET /capabilities`).

## Configuration Store (`rcc::config`)
- Responsibilities: load and validate container config (region, channel map, power limits, credentials).
- Components:
  - `ConfigManager` loads YAML via `yaml-cpp`; supports hot reload.
  - `ChannelMapper` derives 1-based channel index → frequency mapping per §13.
  - `TimingProfile` encapsulates probe cadences per CB-TIMING.
- Provides configuration snapshots to command/radio modules and telemetry.

## Audit Logging (`rcc::audit`)
- Responsibilities: append immutable log entries for control actions and auth events.
- Components:
  - `AuditLogger` using `dts::common::logging::StructuredLogger`.
  - `AuditRecord` struct capturing `actor`, `radioId`, `action`, `params`, `result`, `latency`.
  - Optional sink rotation integration with `dts::common` log sinks.

## Adapter Layer (`rcc::adapter`)
- Responsibilities: abstract vendor-specific protocols behind `IRadioAdapter`.
- Interfaces:
  - `class IRadioAdapter` with async methods (`connect`, `set_power`, `set_channel`, `get_state`, `refresh_capabilities`, `probe_health`).
  - `AdapterFactory` resolves adapter instances from config/inventory.
- `SilvusAdapter`: implements Silvus ICD (JSON-RPC/HTTP) leveraging `dts::common::net::HttpClient`.
- Implements error normalization to container codes; publishes diagnostic metadata.
- Provides simulation/test adapter (`FakeAdapter`) for integration tests.

## Cross-Cutting Utilities
- `rcc::common::Clock`, `CorrelationId`, `MetricsFacade` referencing `dts-common`.
- Asio integration: single-threaded `io_context` + optional thread pool for adapters.
- Health endpoints: `GET /health` (liveness/readiness) reusing `dts::common::health`.
- Metrics: integrate with `dts::common::metrics::Registry`.

## Testing Strategy
- Unit tests use Catch2 (via `dts-common` testing setup) located under `test/`.
- Categories:
  - API handler tests (mock orchestrator/auth).
  - Command orchestrator tests (mock adapters, channel mapper).
  - Adapter conformance tests for Silvus mapping.
  - Config derivation tests (channel map scenarios, timing profile parsing).
- Integration test harness simulates SSE clients and command flows using `asio::io_context`.

## Build & Packaging
- Root `CMakeLists.txt` mirrors BioSensorContainer: options for tests, sanitizers, LTO.
- Targets:
  - `radio-control-container` executable (src/main.cpp).
  - Static libraries per module (`rcc-api`, `rcc-command`, etc.) to enable unit test linking.
- Link dependencies: `dts-common-core`, `dts-common-net`, `Threads::Threads`, `OpenSSL`, `yaml-cpp`, `nlohmann_json`.
- Install layout: binary under `bin/`, configs under `etc/rcc`, shared libs optional.

# RCC C++20 Component Design Notes

## Overview
- C++20 service reimplements the Radio Control Container described in `radio_control_container_architecture_v1.md`.
- Follow container conventions established by `BioSensorContainer` and other DTS services.
- Reuse `dts-common` targets:
  - `dts-common-core` for utilities/logging/config helpers.
  - `dts-common-net` for REST router, SSE server, auth primitives, health checks.
- Top-level executable target: `radio-control-container` (`rcc` binary).
- Primary namespaces: `rcc::api`, `rcc::auth`, `rcc::command`, `rcc::radio`, `rcc::telemetry`, `rcc::adapter`, `rcc::config`, `rcc::audit`.

## API Gateway (`rcc::api`)
- Responsibilities: host HTTP/JSON endpoints per OpenAPI (`GET /radios`, `POST /radios/select`, `POST /radios/{id}/power`, `POST /radios/{id}/channel`, `GET /telemetry`, `GET /capabilities`, `/health`).
- Uses `dts::common::net::RestServer` and router.
- Converts HTTP requests into command layer calls; wraps responses in standard envelopes.
- SSE endpoint delegates to telemetry hub (`TelemetryHubEndpoint`).
- Applies rate limiting via `dts-common` middlewares; enforces auth scopes using `rcc::auth::ScopeChecker`.

## Auth (`rcc::auth`)
- Responsibilities: validate bearer tokens, enforce roles (`viewer`, `controller`).
- Components:
  - `TokenValidator` wraps `dts::common::auth::JwtValidator`.
  - `ScopeChecker` maps OpenAPI operations to required scopes; shared with API handlers.
  - `AuthContext` extracted per request and propagated.
- Provides middleware for REST server and SSE connections.

## Command Orchestrator (`rcc::command`)
- Responsibilities: normalize and execute commands (`selectRadio`, `setPower`, `setChannel`) and propagate events.
- Key classes:
  - `CommandOrchestrator` with methods `select_radio`, `set_power`, `set_channel`.
  - `CommandBudget` enforces per-radio serialization and timing budgets (leverages `asio::strand`).
  - `ResultEnvelope` normalizes adapter results to container codes (`OK`, `INVALID_RANGE`, etc.).
- Interacts with `rcc::radio::RadioManager`, `rcc::config::ChannelMapper`, `rcc::telemetry::TelemetryHub`, `rcc::audit::AuditLogger`.
- Applies ADR-003 error normalization.

## Radio Manager (`rcc::radio`)
- Responsibilities: maintain inventory, discover radios, expose adapters, track active radio.
- Components:
  - `RadioDescriptor` (id, model, capabilities, transport).
  - `RadioInventory` thread-safe store.
  - `RadioManager` handles discovery, lifecycle (Normal/Recovering/Offline states), scheduling resilience probes per §8.3.
  - `ActiveRadioTracker` ensures single default target for commands.
- Relies on `asio::io_context` for timers.
- Publishes state changes via telemetry hub.

## Telemetry Hub (`rcc::telemetry`)
- Responsibilities: multiplex events to SSE clients, buffer for replay, forward command outcomes.
- Components:
  - `TelemetryHub` wraps `dts::common::telemetry::EventBus`.
  - `TelemetryPublisher` helper for standard events (`ready`, `state`, `channelChanged`, `powerChanged`, `fault`).
  - `TelemetrySerializer` ensures payload schemas match AsyncAPI spec.
- SSE endpoint uses `dts::common::telemetry::SSEServer`.
- Supports capability negotiation (`GET /capabilities`).

## Configuration Store (`rcc::config`)
- Responsibilities: load and validate container config (region, channel map, power limits, credentials).
- Components:
  - `ConfigManager` loads YAML via `yaml-cpp`; supports hot reload.
  - `ChannelMapper` derives 1-based channel index → frequency mapping per §13.
  - `TimingProfile` encapsulates probe cadences per CB-TIMING.
- Provides configuration snapshots to command/radio modules and telemetry.

## Audit Logging (`rcc::audit`)
- Responsibilities: append immutable log entries for control actions and auth events.
- Components:
  - `AuditLogger` using `dts::common::logging::StructuredLogger`.
  - `AuditRecord` struct capturing `actor`, `radioId`, `action`, `params`, `result`, `latency`.
  - Optional sink rotation integration with `dts::common` log sinks.

## Adapter Layer (`rcc::adapter`)
- Responsibilities: abstract vendor-specific protocols behind `IRadioAdapter`.
- Interfaces:
  - `class IRadioAdapter` with async methods (`connect`, `set_power`, `set_channel`, `get_state`, `refresh_capabilities`, `probe_health`).
  - `AdapterFactory` resolves adapter instances from config/inventory.
- `SilvusAdapter`: implements Silvus ICD (JSON-RPC/HTTP) leveraging `dts::common::net::HttpClient`.
- Implements error normalization to container codes; publishes diagnostic metadata.
- Provides simulation/test adapter (`FakeAdapter`) for integration tests.

## Cross-Cutting Utilities
- `rcc::common::Clock`, `CorrelationId`, `MetricsFacade` referencing `dts-common`.
- Asio integration: single-threaded `io_context` + optional thread pool for adapters.
- Health endpoints: `GET /health` (liveness/readiness) reusing `dts::common::health`.
- Metrics: integrate with `dts::common::metrics::Registry`.

## Testing Strategy
- Unit tests use `Catch2` (via `dts-common` testing setup) located under `test/`.
- Categories:
  - API handler tests (mock orchestrator/auth).
  - Command orchestrator tests (mock adapters, channel mapper).
  - Adapter conformance tests for Silvus mapping.
  - Config derivation tests (channel map scenarios, timing profile parsing).
- Integration test harness simulates SSE clients and command flows using `asio::io_context`.

## Build & Packaging
- Root `CMakeLists.txt` mirrors BioSensorContainer: options for tests, sanitizers, LTO.
- Targets:
  - `radio-control-container` executable (src/main.cpp).
  - Static libraries per module (`rcc-api`, `rcc-command`, etc.) to enable unit test linking.
- Link dependencies: `dts-common-core`, `dts-common-net`, `Threads::Threads`, `OpenSSL`, `yaml-cpp`, `nlohmann_json`.
- Install layout: binary under `bin/`, configs under `etc/rcc`, shared libs optional.


