#!/bin/bash
# Podman deployment helper for Radio Control Container (RCC).
# Builds the image, then starts a health-checked container.
# Usage: ./deploy.sh x86_64 --debug
#        ./deploy.sh arm64 --skip-build --network-mode host

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SETUP_SCRIPT="$SCRIPT_DIR/setup.sh"
PODMAN_CMD_STRING="${PODMAN_CMD:-podman}"
read -r -a PODMAN_CMD <<<"$PODMAN_CMD_STRING"

ARCH=""
IMAGE_NAME="radio-control"
CONTAINER_NAME=""
IMAGE_TAG=""
SKIP_BUILD=0
SKIP_RUN=0
NETWORK_MODE="bridge"
BRIDGE_NETWORK_NAME="dts-internal"
CONTAINER_IP="192.168.101.36/24"
CONTAINER_GATEWAY="192.168.101.100"
BRIDGE_SUBNET="192.168.101.0/24"
USE_DHCP=0
API_PORT=8080
API_HOST_PORT="8080"
SSE_PORT=8081
SSE_HOST_PORT="8081"
SSH_HOST_PORT="12222"
HEALTH_PORT=8080
HEALTH_HOST_PORT="${HEALTH_HOST_PORT:-8080}"
DISCOVERY_PORT=9999
DISCOVERY_HOST_PORT="${DISCOVERY_HOST_PORT:-9999}"
DEBUG_MODE=0
SYSTEMD_OUTPUT=""
RUNTIME_ROOT=""

log() { echo "[podman][deploy] $1"; }
warn() { echo "[podman][deploy][warn] $1" >&2; }
die() { echo "[podman][deploy][error] $1" >&2; exit 1; }
run_podman() { "${PODMAN_CMD[@]}" "$@"; }

usage() {
  cat <<'EOF'
Usage: ./deploy.sh [x86_64|arm64] [options]

Options:
  --debug                 Build and run the debug image variant
  --skip-build            Reuse an existing image tag instead of rebuilding
  --skip-run              Prepare runtime state only, do not start container
  --container-name NAME   Override container name
  --image-tag TAG         Override image tag
  --runtime-root DIR      Runtime root for generated state files
  --systemd-output DIR    Generate podman systemd unit files into DIR

Network options:
  --network-mode MODE     bridge or host (default: bridge)
  --ip ADDR/CIDR          Static bridge-mode container IP
  --gateway IP            Bridge gateway override
  --subnet CIDR           Bridge subnet override
  --dhcp                  Use Podman bridge IPAM instead of a static IP
  --api-host-port PORT    Host REST API port (default: 8080)
  --sse-host-port PORT    Host SSE telemetry port (default: 8081)
  --health-host-port PORT Host health endpoint port (default: 8080)
  --discovery-host-port PORT
                          Host discovery UDP port (default: 9999)
  --ssh-host-port PORT    Host SSH port in debug bridge mode (default: 12222)

Environment:
  PODMAN_CMD              Full podman command prefix
  PROJECT_ROOT            Override project root
EOF
}

infer_arch() {
  local machine
  machine="$(uname -m)"
  case "$machine" in
    x86_64|amd64) echo "x86_64" ;;
    aarch64|arm64) echo "arm64" ;;
    *) die "Unsupported host arch '$machine'; pass x86_64 or arm64 explicitly" ;;
  esac
}

set_arch() {
  local arch="$1"
  [[ -z "$ARCH" ]] || die "Architecture specified more than once"
  ARCH="$arch"
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      x86_64|arm64)
        set_arch "$1"
        ;;
      --arch)
        [[ $# -ge 2 ]] || die "--arch requires a value"
        case "$2" in
          x86_64|arm64)
            set_arch "$2"
            ;;
          *) die "Unsupported arch: $2 (use x86_64 or arm64)" ;;
        esac
        shift
        ;;
      --debug)
        DEBUG_MODE=1
        ;;
      --skip-build)
        SKIP_BUILD=1
        ;;
      --skip-run)
        SKIP_RUN=1
        ;;
      --container-name)
        [[ $# -ge 2 ]] || die "--container-name requires a value"
        CONTAINER_NAME="$2"
        shift
        ;;
      --image-tag)
        [[ $# -ge 2 ]] || die "--image-tag requires a value"
        IMAGE_TAG="$2"
        shift
        ;;
      --runtime-root)
        [[ $# -ge 2 ]] || die "--runtime-root requires a value"
        RUNTIME_ROOT="$2"
        shift
        ;;
      --systemd-output)
        [[ $# -ge 2 ]] || die "--systemd-output requires a value"
        SYSTEMD_OUTPUT="$2"
        shift
        ;;
      --network-mode)
        [[ $# -ge 2 ]] || die "--network-mode requires a value"
        case "$2" in
          bridge|host) NETWORK_MODE="$2" ;;
          *) die "Unsupported network mode: $2 (use bridge or host)" ;;
        esac
        shift
        ;;
      --ip)
        [[ $# -ge 2 ]] || die "--ip requires ADDR/CIDR"
        CONTAINER_IP="$2"
        shift
        ;;
      --gateway)
        [[ $# -ge 2 ]] || die "--gateway requires IP"
        CONTAINER_GATEWAY="$2"
        shift
        ;;
      --subnet)
        [[ $# -ge 2 ]] || die "--subnet requires CIDR"
        BRIDGE_SUBNET="$2"
        shift
        ;;
      --dhcp)
        USE_DHCP=1
        ;;
      --api-host-port)
        [[ $# -ge 2 ]] || die "--api-host-port requires a value"
        API_HOST_PORT="$2"
        shift
        ;;
      --sse-host-port)
        [[ $# -ge 2 ]] || die "--sse-host-port requires a value"
        SSE_HOST_PORT="$2"
        shift
        ;;
      --health-host-port)
        [[ $# -ge 2 ]] || die "--health-host-port requires a value"
        HEALTH_HOST_PORT="$2"
        shift
        ;;
      --discovery-host-port)
        [[ $# -ge 2 ]] || die "--discovery-host-port requires a value"
        DISCOVERY_HOST_PORT="$2"
        shift
        ;;
      --ssh-host-port)
        [[ $# -ge 2 ]] || die "--ssh-host-port requires a value"
        SSH_HOST_PORT="$2"
        shift
        ;;
      --help|-h)
        usage
        exit 0
        ;;
      *)
        die "Unknown argument: $1"
        ;;
    esac
    shift
  done

  [[ -n "$ARCH" ]] || ARCH="$(infer_arch)"
  [[ -n "$IMAGE_TAG" ]] || IMAGE_TAG="${IMAGE_NAME}:${ARCH}"
  if [[ "$DEBUG_MODE" -eq 1 ]] && [[ "$IMAGE_TAG" != *-debug ]]; then
    IMAGE_TAG="${IMAGE_TAG}-debug"
  fi
  if [[ -z "$CONTAINER_NAME" ]]; then
    CONTAINER_NAME="radio-control"
    if [[ "$DEBUG_MODE" -eq 1 ]]; then
      CONTAINER_NAME="radio-control-debug"
    fi
  fi
  [[ -n "$RUNTIME_ROOT" ]] || RUNTIME_ROOT="$PROJECT_ROOT/.podman-runtime/$CONTAINER_NAME"
}

ensure_prereqs() {
  command -v "${PODMAN_CMD[0]}" >/dev/null 2>&1 || die "podman not found. Install: apt install podman"
  [[ -f "$SETUP_SCRIPT" ]] || die "setup.sh not found: $SETUP_SCRIPT"
}

prepare_runtime_layout() {
  mkdir -p "$RUNTIME_ROOT"
  if [[ -n "$SYSTEMD_OUTPUT" ]]; then
    mkdir -p "$SYSTEMD_OUTPUT"
  fi
}

generate_systemd_units() {
  [[ -n "$SYSTEMD_OUTPUT" ]] || return 0

  local podman_bin
  podman_bin="$(command -v "${PODMAN_CMD[0]}")"
  local run_script="$SYSTEMD_OUTPUT/run-$CONTAINER_NAME.sh"
  local unit_file="$SYSTEMD_OUTPUT/container-$CONTAINER_NAME.service"

  mkdir -p "$SYSTEMD_OUTPUT"

  cat > "$run_script" <<RUNEOF
#!/bin/bash
set -euo pipefail

exec ${podman_bin} run \\
  --name "$CONTAINER_NAME" \\
  --replace \\
  --pull=never \\
  --restart=no \\
  --health-cmd "curl -fsS --max-time 5 http://127.0.0.1:${HEALTH_PORT}/api/v1/health >/dev/null || exit 1" \\
  --health-on-failure=kill \\
  --health-start-period=40s \\
  --health-interval=5s \\
  --health-retries=3 \\
  --health-timeout=5s \\
  --systemd=true \\
  ${NETWORK_ARGS[@]} \\
  -p "${API_HOST_PORT}:${API_PORT}" \\
  -p "${SSE_HOST_PORT}:${SSE_PORT}" \\
  -p "${DISCOVERY_HOST_PORT}:${DISCOVERY_PORT}/udp" \\
$( [[ "$DEBUG_MODE" -eq 1 ]] && echo "  -p \"${SSH_HOST_PORT}:22\" \\" )
  "$IMAGE_TAG"
RUNEOF
  chmod +x "$run_script"

  cat > "$unit_file" <<UNITEOF
[Unit]
Description=Radio Control Container (RCC -- Podman)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=$run_script
ExecStop=${podman_bin} stop -t 10 $CONTAINER_NAME
ExecStopPost=-${podman_bin} rm -f $CONTAINER_NAME
Restart=on-failure
RestartSec=5
StartLimitBurst=10
StartLimitIntervalSec=600
TimeoutStartSec=120
TimeoutStopSec=30

[Install]
WantedBy=multi-user.target
UNITEOF

  log "Generated systemd unit: $unit_file"
  log "Generated run script: $run_script"
}

build_image() {
  [[ "$SKIP_BUILD" -eq 0 ]] || return 0
  log "Building image $IMAGE_TAG for $ARCH"
  "$SETUP_SCRIPT" "$ARCH" --debug="$( [[ "$DEBUG_MODE" -eq 1 ]] && echo true || echo false )"
}

ensure_bridge_network() {
  [[ "$NETWORK_MODE" == "bridge" ]] || return 0
  if run_podman network inspect "$BRIDGE_NETWORK_NAME" >/dev/null 2>&1; then
    log "Bridge network '$BRIDGE_NETWORK_NAME' already exists"
    return 0
  fi
  log "Creating bridge network '$BRIDGE_NETWORK_NAME' (subnet=$BRIDGE_SUBNET, gateway=$CONTAINER_GATEWAY)"
  run_podman network create --driver bridge --subnet "$BRIDGE_SUBNET" --gateway "$CONTAINER_GATEWAY" "$BRIDGE_NETWORK_NAME"
}

collect_network_args() {
  NETWORK_ARGS=()
  if [[ "$NETWORK_MODE" == "host" ]]; then
    NETWORK_ARGS=(--network host)
  else
    NETWORK_ARGS=(--network "$BRIDGE_NETWORK_NAME")
    if [[ "$USE_DHCP" -eq 0 ]]; then
      NETWORK_ARGS+=(--ip "${CONTAINER_IP%%/*}")
    fi
  fi
}

run_container() {
  [[ "$SKIP_RUN" -eq 0 ]] || return 0
  ensure_bridge_network
  collect_network_args

  local run_args=(
    run -d
    --name "$CONTAINER_NAME"
    --replace
    --pull=never
    --restart=always
    --health-cmd "curl -fsS --max-time 5 http://127.0.0.1:${HEALTH_PORT}/api/v1/health >/dev/null || exit 1"
    --health-on-failure=kill
    --health-start-period=40s
    --health-interval=5s
    --health-retries=3
    --health-timeout=5s
    --systemd=true
    "${NETWORK_ARGS[@]}"
  )

  if [[ "$NETWORK_MODE" == "bridge" ]]; then
    run_args+=(
      -p "${API_HOST_PORT}:${API_PORT}"
      -p "${SSE_HOST_PORT}:${SSE_PORT}"
      -p "${DISCOVERY_HOST_PORT}:${DISCOVERY_PORT}/udp"
    )
    if [[ "$DEBUG_MODE" -eq 1 ]]; then
      run_args+=( -p "${SSH_HOST_PORT}:22" )
    fi
  fi

  run_args+=("$IMAGE_TAG")
  run_podman "${run_args[@]}" >/dev/null
}

wait_for_health() {
  [[ "$SKIP_RUN" -eq 0 ]] || return 0
  local attempts=60
  local delay=2
  local url="http://127.0.0.1:${HEALTH_HOST_PORT}/api/v1/health"
  for ((i=1; i<=attempts; i++)); do
    if curl -fsS --max-time 2 "$url" >/dev/null 2>&1; then
      log "Health endpoint is ready: $url"
      return 0
    fi
    sleep "$delay"
  done
  warn "Container failed health probe; recent podman logs follow"
  run_podman logs "$CONTAINER_NAME" || true
  die "Timed out waiting for container health"
}

print_summary() {
  log "Container name: $CONTAINER_NAME"
  log "Image tag: $IMAGE_TAG"
  log "Network mode: $NETWORK_MODE"
  if [[ "$NETWORK_MODE" == "bridge" ]]; then
    log "REST API / monitor:  http://127.0.0.1:${API_HOST_PORT}"
    log "SSE telemetry:       http://127.0.0.1:${SSE_HOST_PORT}"
    log "Health URL:          http://127.0.0.1:${HEALTH_HOST_PORT}/api/v1/health"
    log "Discovery UDP:       127.0.0.1:${DISCOVERY_HOST_PORT}"
    if [[ "$DEBUG_MODE" -eq 1 ]]; then
      log "SSH: sshpass -p debug123 ssh -o StrictHostKeyChecking=no -p ${SSH_HOST_PORT} root@127.0.0.1"
    fi
  else
    log "REST API / monitor:  http://127.0.0.1:${API_PORT}"
    log "SSE telemetry:       http://127.0.0.1:${SSE_PORT}"
  fi
}

main() {
  parse_args "$@"
  ensure_prereqs
  prepare_runtime_layout
  build_image
  generate_systemd_units
  run_container
  wait_for_health
  print_summary
}

main "$@"
