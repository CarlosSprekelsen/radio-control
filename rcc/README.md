# Radio Control Container (C++20)

This project implements the Radio Control Container using C++20, following the architecture defined in `../docs/radio_control_container_architecture_v1.md` and leveraging shared infrastructure from `dts-common`.

## Prerequisites

- CMake ≥ 3.20
- GCC ≥ 12 or Clang ≥ 14 with C++20 support
- `dts-common` (present in `../../libs/dts-common` or installed system-wide)
- Dependencies: OpenSSL, yaml-cpp, nlohmann-json, fmt, Catch2

On Ubuntu/Debian:

```bash
sudo apt-get install build-essential cmake libssl-dev libyaml-cpp-dev nlohmann-json3-dev libfmt-dev libasio-dev
```

Install Catch2 v3 (package or source) and ensure it is discoverable via CMake.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

To run tests:

```bash
cmake --build build --target test
ctest --test-dir build --output-on-failure
```

### Options

- `-DRCC_BUILD_TESTS=OFF` to skip tests
- `-DRCC_ENABLE_SANITIZERS=ON` to enable ASAN/UBSAN
- `-DRCC_ENABLE_TSAN=ON` to enable ThreadSanitizer
- `-DRCC_ENABLE_LTO=ON` to enable link-time optimization (Release builds)

## Running

```bash
./build/radio-control-container config/default.yaml
```

A default configuration lives in `config/default.yaml`; replace with deployment-specific values.

## Project Structure

- `src/` — application sources (API, command orchestrator, telemetry, adapters, etc.)
- `include/` — public headers
- `config/` — default configuration templates
- `docs/` — design notes and references
- `test/` — unit and integration tests

## Status

The C++20 port is complete and operational. All cross-cutting DTS features are implemented:
- REST API (`/api/v1/*`) with JWT bearer authentication
- SSE telemetry stream (`/api/v1/telemetry`)
- Health endpoint (`/api/v1/health`) compliant with DTS schema
- Service discovery (`dts.announce` on UDP 9999)
- Embedded monitor page (`/monitor`)
- `RadioManager`, `CommandOrchestrator`, `SilvusAdapter`, `TelemetryHub`, `AuditLogger`

See `open-issues/` for known runtime robustness gaps (async adapter, probing state machine) tracked for future sprints.
