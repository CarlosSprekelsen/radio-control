#!/bin/bash

set -euo pipefail

PODMAN_CMD_STRING="${PODMAN_CMD:-podman}"
read -r -a PODMAN_CMD <<<"$PODMAN_CMD_STRING"

run_podman() { "${PODMAN_CMD[@]}" "$@"; }

usage() {
  cat <<'EOF'
Usage: ./remove.sh [container-name]

Options:
  --help, -h     Show this help

Environment:
  PODMAN_CMD     Full podman command prefix, e.g.
                 'podman --root /tmp/podroot --runroot /tmp/podrun --storage-driver vfs'
EOF
}

case "${1:-}" in
  --help|-h)
    usage
    exit 0
    ;;
esac

CONTAINER_NAME="${1:-radio-control}"

if run_podman container exists "$CONTAINER_NAME"; then
  run_podman rm -f "$CONTAINER_NAME"
  echo "[podman][remove] Removed container $CONTAINER_NAME"
else
  echo "[podman][remove] Container $CONTAINER_NAME not present"
fi
