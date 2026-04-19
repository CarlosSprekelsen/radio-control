#!/bin/bash
# Local workstation helper for a built RCC Podman image.
# Usage:
#   ./run-local.sh x86_64
#   ./run-local.sh x86_64 --debug=true
#   ./run-local.sh x86_64 --api-host-port=8080
#   ./run-local.sh x86_64 --ip=192.168.101.36/24
# This script never builds images. Use ./setup.sh first.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_SCRIPT="$SCRIPT_DIR/deploy.sh"
PODMAN_CMD_STRING="${PODMAN_CMD:-podman}"
read -r -a PODMAN_CMD <<<"$PODMAN_CMD_STRING"

ARCH_INPUT=""
DEBUG_MODE=false
NETWORK_MODE="bridge"
CONTAINER_IP="192.168.101.36/24"
CONTAINER_GATEWAY="192.168.101.100"
BRIDGE_SUBNET="192.168.101.0/24"
USE_DHCP=0
API_HOST_PORT="${API_HOST_PORT:-8080}"
SSE_HOST_PORT="${SSE_HOST_PORT:-8081}"
HEALTH_HOST_PORT="${HEALTH_HOST_PORT:-8080}"
DISCOVERY_HOST_PORT="${DISCOVERY_HOST_PORT:-9999}"
SSH_HOST_PORT="${SSH_HOST_PORT:-12222}"
CONTAINER_NAME=""
IMAGE_NAME="radio-control"
IMAGE_TAG=""
NETWORK_NAME="rcc-local"

log() { echo "[podman][run-local] $1"; }
warn() { echo "[podman][run-local][warn] $1" >&2; }
die() { echo "[podman][run-local][error] $1" >&2; exit 1; }
run_podman() { "${PODMAN_CMD[@]}" "$@"; }

usage() {
  cat <<'EOF'
Usage: ./run-local.sh [x86_64|arm64] [options]

Run a previously-built RCC Podman image locally.
This wrapper delegates to deploy.sh with --skip-build.

Options:
  --debug=true|false   Run the debug image variant (default: false)
  --network-mode MODE  Runtime network mode: bridge or host (default: bridge)
  --ip=ADDR/CIDR       Static bridge-mode container IP (default: 192.168.101.36/24)
  --gateway=IP         Bridge gateway override (default: 192.168.101.100)
  --subnet=CIDR        Bridge subnet override (default: 192.168.101.0/24)
  --dhcp               Use Podman bridge IPAM instead of a static IP
  --api-host-port N    Host REST API port (default: 8080)
  --sse-host-port N    Host SSE telemetry port (default: 8081)
  --health-host-port N Host health endpoint port (default: 8080)
  --discovery-host-port N
                       Host discovery UDP port (default: 9999)
  --ssh-host-port N    Host SSH port in debug bridge mode (default: 12222)
  --container-name N   Override container name
  --image-tag TAG      Override image tag
  --help, -h           Show this help
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

for arg in "$@"; do
  case "$arg" in
    x86_64|arm64)
      ARCH_INPUT="$arg"
      ;;
    --debug=*)
      val="${arg#--debug=}"
      if [[ "$val" == "true" || "$val" == "1" ]]; then
        DEBUG_MODE=true
      elif [[ "$val" == "false" || "$val" == "0" ]]; then
        DEBUG_MODE=false
      else
        die "Unsupported debug flag: $arg"
      fi
      ;;
    --network-mode=*)
      NETWORK_MODE="${arg#--network-mode=}"
      ;;
    --ip=*)
      CONTAINER_IP="${arg#--ip=}"
      ;;
    --gateway=*)
      CONTAINER_GATEWAY="${arg#--gateway=}"
      ;;
    --subnet=*)
      BRIDGE_SUBNET="${arg#--subnet=}"
      ;;
    --dhcp)
      USE_DHCP=1
      ;;
    --api-host-port=*)
      API_HOST_PORT="${arg#--api-host-port=}"
      ;;
    --sse-host-port=*)
      SSE_HOST_PORT="${arg#--sse-host-port=}"
      ;;
    --health-host-port=*)
      HEALTH_HOST_PORT="${arg#--health-host-port=}"
      ;;
    --discovery-host-port=*)
      DISCOVERY_HOST_PORT="${arg#--discovery-host-port=}"
      ;;
    --ssh-host-port=*)
      SSH_HOST_PORT="${arg#--ssh-host-port=}"
      ;;
    --container-name=*)
      CONTAINER_NAME="${arg#--container-name=}"
      ;;
    --image-tag=*)
      IMAGE_TAG="${arg#--image-tag=}"
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      die "Unknown argument: $arg"
      ;;
  esac
 done

if [[ -z "$ARCH_INPUT" ]]; then
  ARCH_INPUT="$(infer_arch)"
  log "No architecture specified, defaulting to host architecture: $ARCH_INPUT"
fi

case "$ARCH_INPUT" in
  x86_64|amd64) DEPLOY_ARCH="x86_64" ;;
  arm64) DEPLOY_ARCH="arm64" ;;
  *) die "Unsupported architecture: $ARCH_INPUT" ;;
 esac

if [[ -z "$IMAGE_TAG" ]]; then
  IMAGE_TAG="${IMAGE_NAME}:${DEPLOY_ARCH}"
  if [[ "$DEBUG_MODE" == "true" ]]; then
    IMAGE_TAG="${IMAGE_TAG}-debug"
  fi
fi

if [[ -z "$CONTAINER_NAME" ]]; then
  CONTAINER_NAME="radio-control-test"
  if [[ "$DEBUG_MODE" == "true" ]]; then
    CONTAINER_NAME="radio-control-test-debug"
  fi
fi

if [[ "$DEPLOY_ARCH" != "$(infer_arch)" ]]; then
  die "run-local.sh is for same-host architecture runs only (host=$(infer_arch), requested=$DEPLOY_ARCH). Build the target image with setup.sh, but do not run it on this host."
fi

if ! run_podman image exists "$IMAGE_TAG"; then
  die "Image '$IMAGE_TAG' is not available locally. Build it first with ./setup.sh ${DEPLOY_ARCH} $( [[ "$DEBUG_MODE" == "true" ]] && echo '--debug=true' )"
fi

if [[ ! -x "$DEPLOY_SCRIPT" ]]; then
  die "deploy.sh not found or not executable: $DEPLOY_SCRIPT"
fi

deploy_args=(
  "$DEPLOY_ARCH"
  --skip-build
  --network-mode "$NETWORK_MODE"
  --api-host-port "$API_HOST_PORT"
  --sse-host-port "$SSE_HOST_PORT"
  --health-host-port "$HEALTH_HOST_PORT"
  --discovery-host-port "$DISCOVERY_HOST_PORT"
  --ssh-host-port "$SSH_HOST_PORT"
)

if [[ "$DEBUG_MODE" == "true" ]]; then
  deploy_args+=(--debug)
fi

if [[ -n "$CONTAINER_NAME" ]]; then
  deploy_args+=(--container-name "$CONTAINER_NAME")
fi

if [[ -n "$IMAGE_TAG" ]]; then
  deploy_args+=(--image-tag "$IMAGE_TAG")
fi

if [[ "$USE_DHCP" -eq 1 ]]; then
  deploy_args+=(--dhcp)
else
  deploy_args+=(--ip "$CONTAINER_IP")
fi

if [[ -n "$CONTAINER_GATEWAY" ]]; then
  deploy_args+=(--gateway "$CONTAINER_GATEWAY")
fi

if [[ -n "$BRIDGE_SUBNET" ]]; then
  deploy_args+=(--subnet "$BRIDGE_SUBNET")
fi

log "Delegating to deploy.sh"
"$DEPLOY_SCRIPT" "${deploy_args[@]}"

if [[ "$DEBUG_MODE" == "true" && "$NETWORK_MODE" == "bridge" ]]; then
  if command -v sshpass >/dev/null 2>&1; then
    log "Validating SSH access on host port $SSH_HOST_PORT"
    sshpass -p debug123 ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5 -p "$SSH_HOST_PORT" root@127.0.0.1 "echo ssh-ok" >/dev/null
    log "SSH access validated for root@127.0.0.1:$SSH_HOST_PORT"
  else
    warn "sshpass not installed; skip SSH validation. Use: ssh root@127.0.0.1 -p $SSH_HOST_PORT"
  fi
fi

log "Use 'podman logs $CONTAINER_NAME' to view container logs."
