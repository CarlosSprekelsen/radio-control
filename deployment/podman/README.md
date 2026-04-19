# Radio Control Container — Podman Packaging

Podman OCI image build and deployment scripts for the C++20 Radio Control Container (RCC).

Aligned with the DTS Container Packaging for TNN alignment tracker (`podman-container-packaging-alignment.md`): systemd as PID 1, non-root service user, `HEALTHCHECK` targeting `/api/v1/health`, and OCI archive export for alpha/demo delivery.

## Network Design Compliance

| Parameter | Value | Source |
|-----------|-------|--------|
| Internal Zone subnet | `192.168.101.0/24` | `docs/swad-dts/designs/sys-network-architecture.md` |
| RCC static IP | `192.168.101.36/24` | `sys-network-architecture.md` §Internal Zone |
| Gateway | `192.168.101.100` | Hub gateway / bridge |
| REST API / monitor / health | `8080` | `rcc/config/default.yaml` (`command_port`) |
| SSE telemetry | `8081` | `rcc/config/default.yaml` (`sse_port`) |
| Service discovery | `9999/udp` | Cross-cutting DTS standard |
| Debug SSH | `12222` (host) → `22` (container) | Standard DTS debug pattern; `root:debug123` |

## Files

| File | Purpose |
|------|---------|
| `setup.sh` | Build OCI image (auto-builds binary if missing). Supports `x86_64` and `arm64`, cross-compilation, and `--debug=true`. |
| `deploy.sh` | Deploy container with bridge network, static IP, port mappings, health checks, and optional systemd unit generation. |
| `run-local.sh` | Dev convenience wrapper around `deploy.sh --skip-build`. Validates image exists and (in debug mode) tests SSH. |
| `remove.sh` | Stop and remove container. |

## Prerequisites

- `podman` (and optionally `buildah`)
- CMake ≥ 3.20, GCC ≥ 12 or Clang ≥ 14 with C++20 support
- For cross-compilation: `gcc-aarch64-linux-gnu` + `g++-aarch64-linux-gnu` (arm64 on x86_64 host)
- Runtime libraries present or available: `libssl3t64`, `libyaml-cpp0.8`, `zlib1g`

## Quick Start (Local x86_64)

```bash
# 1. Build image
cd services/radio-control/deployment/podman
./setup.sh x86_64 --debug=true

# 2. Run locally (bridged to dts-internal with static IP 192.168.101.36/24)
./run-local.sh x86_64 --debug=true

# 3. Validate health
curl -fsS http://127.0.0.1:8080/api/v1/health

# 4. Open monitor page
# http://127.0.0.1:8080/monitor

# 5. SSH into debug container (optional)
sshpass -p debug123 ssh -o StrictHostKeyChecking=no -p 12222 root@127.0.0.1

# 6. Remove
./remove.sh radio-control-test-debug
```

## Build Options

```bash
# Production image (arm64, minimal)
./setup.sh arm64

# Debug image with SSH and tools
./setup.sh x86_64 --debug=true

# Export OCI archive for transfer
SAVE_IMAGE=true ./setup.sh arm64 --debug=true
# Output: ../../dist/radio-control-arm64-debug.oci.tar
```

## Deployment Options

```bash
# Bridge mode with defaults (recommended for bench)
./deploy.sh x86_64 --debug

# Host network mode (simpler, no port mapping)
./deploy.sh x86_64 --debug --network-mode host

# Custom ports to avoid collisions
./deploy.sh x86_64 --debug --api-host-port 18080 --sse-host-port 18081 --ssh-host-port 22222

# Generate systemd unit files for host systemd management
./deploy.sh x86_64 --systemd-output ./systemd
```

## Container Runtime Details

- **Init:** systemd (PID 1)
- **Service unit:** `radio-control-container.service` (enabled)
- **User:** `rcc` (UID/GID 1000, non-root, `nologin`)
- **Working directory:** `/app`
- **Config:** `/etc/rcc/config.yaml`
- **Read-write paths:** `/var/log/rcc`, `/var/lib/rcc`
- **Network:** systemd-networkd with DHCP on `eth0` (orchestrator can override with static assignment)

## Port Reference

| Container Port | Protocol | Purpose | Mapped in Bridge Mode |
|----------------|----------|---------|----------------------|
| 8080 | TCP | REST API, `/monitor`, `/api/v1/health` | `API_HOST_PORT` (default 8080) |
| 8081 | TCP | SSE telemetry stream | `SSE_HOST_PORT` (default 8081) |
| 9999 | UDP | Service discovery (`dts.announce`) | `DISCOVERY_HOST_PORT` (default 9999) |
| 22 | TCP | SSH (debug images only) | `SSH_HOST_PORT` (default 12222) |

## Validation Checklist

After `run-local.sh` or `deploy.sh` completes:

- [ ] Health endpoint returns 200: `curl -fsS http://127.0.0.1:8080/api/v1/health`
- [ ] Monitor page loads: `curl -fsS http://127.0.0.1:8080/monitor | grep -q "Radio Control Monitor"`
- [ ] Discovery responds: `echo '{"type":"dts.discover"}' | nc -u 127.0.0.1 9999 -w 2` (when container is on host/bridge network)
- [ ] Debug SSH reachable (debug builds): `sshpass -p debug123 ssh -o StrictHostKeyChecking=no -p 12222 root@127.0.0.1 echo ok`
- [ ] systemd shows service active: `podman exec <name> systemctl status radio-control-container`

## Alignment with TNN Packaging Requirements

| Requirement | Implementation |
|-------------|----------------|
| systemd as PID 1 | ✅ `CMD ["/usr/lib/systemd/systemd"]` |
| Application as systemd service | ✅ `radio-control-container.service` with `Restart=always` |
| HEALTHCHECK / health probe | ✅ `--health-cmd` polls `GET /api/v1/health` |
| Bind `0.0.0.0` | ✅ `network.bind_address: "0.0.0.0"` in default config |
| No networking self-configuration | ✅ systemd-networkd DHCP only; orchestrator assigns static IP |
| No device mapping inside image | ✅ No `--device` in image; orchestrator injects at runtime |
| Non-root service user | ✅ `rcc` user (UID 1000) |
| OCI archive export (alpha/demo) | ✅ `SAVE_IMAGE=true` → `podman save --format oci-archive` |

## Troubleshooting

**Image build fails with missing dts-common**
> The RCC CMake resolves `dts-common` via package/submodule/monorepo fallback. If building in the monorepo at `services/radio-control/`, ensure `../../libs/dts-common` or a system package is available.

**Health probe times out**
> The container needs time for systemd to start the service. `deploy.sh` waits up to 120s (60 attempts × 2s). Check logs: `podman logs <container-name>`.

**Port collision on 8080/8081/9999**
> Use `--api-host-port`, `--sse-host-port`, and `--discovery-host-port` overrides in `deploy.sh` or `run-local.sh`.

**SSH not reachable in debug mode**
> Ensure `--debug=true` was used for both `setup.sh` and `run-local.sh`. Debug SSH is only installed in debug images.
