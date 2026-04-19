# Radio Control Container

This repository bundles the components that make up the Radio Controller stack.

## Projects

### rcc
- C++20 implementation of the Radio Control Container API and orchestration services.
- Contains the production service, tests, and supporting tooling.
- Replaces the earlier Go implementation (fully ported and deleted).

### silvus-mock
- C++20 Silvus-compatible mock radio for integration and contract testing.
- Implements the Silvus ICD JSON-RPC surface over HTTP and TCP maintenance.
- Includes an embedded web UI for manual bench testing.

## Documentation

Shared documentation lives in `docs/`. The `rcc` package refers to these files directly; if you move documentation make sure to update those references.

## Getting Started

Clone the repo and work within the subproject you need:

```bash
# Build and test RCC
cd rcc
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Refer to each subproject's own README for detailed setup and workflow guidance.
