# RCC C++20 Migration Status

## Current State (2025-11-12)
- New C++20 project scaffold created under `radioContainer/rcc1/`, leaving the Go implementation in `radioContainer/rcc/` untouched.
- CMake toolchain configured with shared `CompilerWarnings`/`Sanitizers`, `dts-common` integration, version header generation, and Catch2 smoke test.
- Core entry point (`rcc::Application`) and `ApiGateway` skeletons exist, but business logic, REST/SSE wiring, and adapters are still placeholders.
- Placeholder configuration (`config/default.yaml`) and initial documentation (`README.md`, `docs/cpp_design_notes.md`) describe the target architecture and module responsibilities.

## Outstanding Tasks
1. **API & Telemetry**
   - Implement REST router with command endpoints and SSE telemetry stream using `dts-common` servers.
   - Add `/health`, `/capabilities`, and rate limiting/auth middleware.

2. **Command & Radio Layers**
   - Build `CommandOrchestrator` with validation, per-radio serialization, and telemetry/audit hooks.
   - Implement `RadioManager` with discovery, active-radio selection, and resilience timers per architecture §8.3.

3. **Configuration & Auth**
   - Flesh out `ConfigManager` (YAML parsing, channel map derivation, timing profiles).
   - Implement `Authenticator` leveraging `dts-common` JWT/bearer validation and scope checks.

4. **Telemetry Hub & Audit**
   - Wire `TelemetryHub` to `dts::common::telemetry::EventBus`, implement standard event publishers.
   - Implement `AuditLogger` with structured logging and retention policies.

5. **Adapter Layer**
   - Define `IRadioAdapter` interface and complete `SilvusAdapter` with error normalization (ADR-003) and health probing.
   - Provide mock/fake adapter for tests.

6. **Testing & Tooling**
   - Add unit tests across modules (command flows, config derivation, adapter normalization).
   - Create integration tests mirroring Go coverage; update CI scripts if applicable.

7. **Documentation & Release Prep**
   - Update `CHANGELOG.md` and architecture implementation notes once core functionality lands.
   - Document build/run instructions and migration considerations in top-level README.

## Git Notes
- `radioContainer/` **is** a git repository. Current branch: `main`.
- New C++ code resides in `radioContainer/rcc1/` (uncommitted).
- To commit from another machine:
  ```bash
  cd radioContainer
  git add rcc1/ README.md rcc/README.md .github/ deploy/ rcc/Dockerfile
  git status    # review changes
  git commit -m "Scaffold C++20 RCC project and docs"
  git push origin main
  ```
- Adjust the `git add` paths as needed before committing.


