# Radio Control Container (C++20)

This project reimplements the Radio Control Container using C++20 while following the architecture defined in `../docs/radio_control_container_architecture_v1.md` and leveraging shared infrastructure from `dts-common`.

## Prerequisites
- CMake ≥ 3.20
- GCC ≥ 12 or Clang ≥ 14 with C++20 support
- `dts-common` (present in `../dts-common` or installed system-wide)
- Dependencies: OpenSSL, yaml-cpp, nlohmann-json, fmt, Catch2

On Ubuntu/Debian:

```
sudo apt-get install build-essential cmake libssl-dev libyaml-cpp-dev nlohmann-json3-dev libfmt-dev libasio-dev
```

Install Catch2 v3 (package or source) and ensure it is discoverable via CMake.

## Building

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

To run tests:

```
cmake --build build --target test
ctest --test-dir build --output-on-failure
```

### Options
- `-DRCC_BUILD_TESTS=OFF` to skip tests
- `-DRCC_ENABLE_SANITIZERS=ON` to enable ASAN/UBSAN
- `-DRCC_ENABLE_TSAN=ON` to enable ThreadSanitizer
- `-DRCC_ENABLE_LTO=ON` to enable link-time optimization (Release builds)

## Running

```
./build/radio-control-container
```

A placeholder configuration lives in `config/default.yaml`; replace with deployment-specific values.

## Project Structure
- `src/` — application sources (API, command orchestrator, telemetry, adapters, etc.)
- `include/` — public headers
- `config/` — default configuration templates
- `docs/` — design notes and references
- `test/` — unit and integration tests

## Status

The codebase is scaffolded and ready for incremental implementation of the architecture components. Consult `docs/cpp_design_notes.md` for the component breakdown and integration plan.


