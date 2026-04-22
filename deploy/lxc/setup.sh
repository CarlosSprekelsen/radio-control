#!/bin/bash

[ -n "${BASH_VERSION:-}" ] || exec /bin/bash "$0" "$@"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/build-package.sh" "$@"
