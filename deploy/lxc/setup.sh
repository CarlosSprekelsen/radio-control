#!/bin/bash
# UltraLYNX LXC Container Package Builder for Radio Control Container
# Usage: sudo ./setup.sh [arm32|arm64|amd64] [--debug|--release]
#
# Output: packages/rcc-{arm32,arm64,amd64}{-debug}-YYYYMMDDTHHMMSSZ.tar
#   Package contains: MANIFEST, {name}_config, {name}_rootfs.txz
#   Cross-arch packaging expects a prebuilt binary unless packaging natively.

[ -n "${BASH_VERSION:-}" ] || exec /bin/bash "$0" "$@"

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PACKAGES_DIR="${PACKAGES_DIR:-$PROJECT_ROOT/packages}"
CONFIG_SOURCE="${CONFIG_SOURCE:-$PROJECT_ROOT/rcc/config/default.yaml}"
BINARY_SOURCE="${BINARY_SOURCE:-}"
BUILD_STAMP="${BUILD_STAMP:-$(date -u +%Y%m%dT%H%M%SZ)}"
UBUNTU_RELEASE="${UBUNTU_RELEASE:-jammy}"
MIRROR_DEFAULT="${MIRROR_DEFAULT:-http://archive.ubuntu.com/ubuntu}"
MIRROR_PORTS="${MIRROR_PORTS:-http://ports.ubuntu.com/ubuntu-ports}"

TARGET_ARCH="arm32"
DEBUG_MODE="false"

STAGING_DIR=""
ROOTFS_DIR=""
RESOLVED_BINARY=""

log() {
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] [rcc-lxc] $1"
}

error() {
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] [rcc-lxc][error] $1" >&2
  exit 1
}

cleanup() {
  [[ -n "$STAGING_DIR" && -d "$STAGING_DIR" ]] && rm -rf "$STAGING_DIR"
  [[ -n "$ROOTFS_DIR" && -d "$ROOTFS_DIR" ]] && rm -rf "$ROOTFS_DIR"
}
trap cleanup EXIT INT TERM

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      arm32|arm64|amd64)
        TARGET_ARCH="$1"
        ;;
      --debug|debug)
        DEBUG_MODE="true"
        ;;
      --release|release)
        DEBUG_MODE="false"
        ;;
      *)
        error "Unknown argument: $1. Usage: sudo ./setup.sh [arm32|arm64|amd64] [--debug|--release]"
        ;;
    esac
    shift
  done
}

host_arch() {
  case "$(dpkg --print-architecture 2>/dev/null || uname -m)" in
    amd64|x86_64) echo "amd64" ;;
    arm64|aarch64) echo "arm64" ;;
    armhf|armv7l) echo "arm32" ;;
    *) echo "unknown" ;;
  esac
}

arch_vars() {
  case "$1" in
    amd64)
      DEBOOTSTRAP_ARCH="amd64"
      LXC_ARCH_STRING="linux64"
      UBUNTU_MIRROR="$MIRROR_DEFAULT"
      ;;
    arm64)
      DEBOOTSTRAP_ARCH="arm64"
      LXC_ARCH_STRING="linux64"
      UBUNTU_MIRROR="$MIRROR_PORTS"
      ;;
    arm32)
      DEBOOTSTRAP_ARCH="armhf"
      LXC_ARCH_STRING="linux32"
      UBUNTU_MIRROR="$MIRROR_PORTS"
      ;;
    *)
      error "Unsupported architecture: $1"
      ;;
  esac
}

ensure_host_packages() {
  local missing=()
  local package
  for package in "$@"; do
    dpkg -s "$package" >/dev/null 2>&1 || missing+=("$package")
  done

  if (( ${#missing[@]} > 0 )); then
    log "Installing host packages: ${missing[*]}"
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${missing[@]}"
  fi
}

ensure_root() {
  [[ $EUID -eq 0 ]] || error "Run as root. Example: sudo ./setup.sh $TARGET_ARCH"
}

qemu_static_path() {
  case "$1" in
    arm32) command -v qemu-arm-static ;;
    arm64) command -v qemu-aarch64-static ;;
    *) return 1 ;;
  esac
}

resolve_binary() {
  local candidate
  local cmake_args=()

  if [[ -n "$BINARY_SOURCE" ]]; then
    [[ -f "$BINARY_SOURCE" ]] || error "BINARY_SOURCE does not exist: $BINARY_SOURCE"
    RESOLVED_BINARY="$BINARY_SOURCE"
    return 0
  fi

  for candidate in \
    "$PROJECT_ROOT/rcc/build/src/radio-control-container" \
    "$PROJECT_ROOT/rcc/build/radio-control-container"; do
    if [[ -f "$candidate" ]]; then
      RESOLVED_BINARY="$candidate"
      return 0
    fi
  done

  if [[ "$(host_arch)" != "$TARGET_ARCH" ]]; then
    error "No prebuilt binary found for $TARGET_ARCH. Build it first or set BINARY_SOURCE=/path/to/radio-control-container"
  fi

  ensure_host_packages build-essential cmake pkg-config git libssl-dev libyaml-cpp-dev nlohmann-json3-dev libfmt-dev libasio-dev

  if [[ -d "$PROJECT_ROOT/dts-common" ]]; then
    cmake_args+=("-DDTS_COMMON_SOURCE_DIR=$PROJECT_ROOT/dts-common")
  fi

  log "Building native radio-control-container binary..."
  cmake -S "$PROJECT_ROOT/rcc" -B "$PROJECT_ROOT/rcc/build" -DCMAKE_BUILD_TYPE=Release -DRCC_BUILD_TESTS=OFF "${cmake_args[@]}"
  cmake --build "$PROJECT_ROOT/rcc/build" --parallel

  for candidate in \
    "$PROJECT_ROOT/rcc/build/src/radio-control-container" \
    "$PROJECT_ROOT/rcc/build/radio-control-container"; do
    if [[ -f "$candidate" ]]; then
      RESOLVED_BINARY="$candidate"
      return 0
    fi
  done

  error "Build did not produce a radio-control-container binary"
}

prepare_rootfs() {
  local target_arch="$1"
  arch_vars "$target_arch"

  ROOTFS_DIR="$(mktemp -d "${TMPDIR:-/tmp}/rcc-rootfs-${target_arch}.XXXXXX")"
  log "Creating rootfs at $ROOTFS_DIR"

  ensure_host_packages debootstrap tar xz-utils ca-certificates qemu-user-static binfmt-support

  if [[ "$(host_arch)" == "$target_arch" ]]; then
    debootstrap --arch="$DEBOOTSTRAP_ARCH" --variant=minbase "$UBUNTU_RELEASE" "$ROOTFS_DIR" "$UBUNTU_MIRROR"
  else
    local qemu_binary
    qemu_binary="$(qemu_static_path "$target_arch")"
    [[ -n "$qemu_binary" ]] || error "Missing qemu static binary for $target_arch"
    debootstrap --foreign --arch="$DEBOOTSTRAP_ARCH" --variant=minbase "$UBUNTU_RELEASE" "$ROOTFS_DIR" "$UBUNTU_MIRROR"
    mkdir -p "$ROOTFS_DIR/usr/bin"
    cp "$qemu_binary" "$ROOTFS_DIR/usr/bin/"
    chroot "$ROOTFS_DIR" /debootstrap/debootstrap --second-stage
  fi

  cp /etc/resolv.conf "$ROOTFS_DIR/etc/resolv.conf"
  cat > "$ROOTFS_DIR/usr/sbin/policy-rc.d" <<'EOF'
#!/bin/sh
exit 101
EOF
  chmod +x "$ROOTFS_DIR/usr/sbin/policy-rc.d"

  chroot "$ROOTFS_DIR" /bin/bash -lc 'apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates libssl-dev libyaml-cpp-dev libfmt-dev nlohmann-json3-dev libasio-dev && apt-get clean && rm -rf /var/lib/apt/lists/*'

  if [[ "$DEBUG_MODE" == "true" ]]; then
    chroot "$ROOTFS_DIR" /bin/bash -lc 'apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends curl iproute2 net-tools procps && apt-get clean && rm -rf /var/lib/apt/lists/*'
  fi
}

install_payload() {
  local binary_path="$1"

  [[ -f "$CONFIG_SOURCE" ]] || error "Config file not found: $CONFIG_SOURCE"

  mkdir -p "$ROOTFS_DIR/usr/local/bin" "$ROOTFS_DIR/etc/rcc" "$ROOTFS_DIR/var/log/rcc"
  install -m 0755 "$binary_path" "$ROOTFS_DIR/usr/local/bin/radio-control-container"
  install -m 0644 "$CONFIG_SOURCE" "$ROOTFS_DIR/etc/rcc/config.yaml"

  cat > "$ROOTFS_DIR/sbin/init" <<'EOF'
#!/bin/sh
exec /bin/sh -c '/etc/rc.local; exec /bin/sh'
EOF
  chmod +x "$ROOTFS_DIR/sbin/init"

  cat > "$ROOTFS_DIR/etc/rc.local" <<'EOF'
#!/bin/sh
set -eu

LOG_FILE="/var/log/rcc/radio-control-container.log"
mkdir -p /var/log/rcc

(
  while true; do
    /usr/local/bin/radio-control-container /etc/rcc/config.yaml >>"$LOG_FILE" 2>&1 || true
    sleep 5
  done
) &

while true; do
  sleep 3600
done
EOF
  chmod +x "$ROOTFS_DIR/etc/rc.local"
}

create_package() {
  local package_member_prefix="rcc-${TARGET_ARCH}"
  local debug_suffix=""
  [[ "$DEBUG_MODE" == "true" ]] && debug_suffix="-debug"
  package_member_prefix+="$debug_suffix"

  local package_name="${package_member_prefix}-${BUILD_STAMP}"
  local rootfs_name="${package_member_prefix}_rootfs.txz"
  local config_name="${package_member_prefix}_config"
  local output_path="$PACKAGES_DIR/${package_name}.tar"

  STAGING_DIR="$(mktemp -d "${TMPDIR:-/tmp}/rcc-package-${TARGET_ARCH}.XXXXXX")"
  mkdir -p "$PACKAGES_DIR"

  log "Creating $rootfs_name"
  (cd "$ROOTFS_DIR" && tar -cJf "$STAGING_DIR/$rootfs_name" .)

  cat > "$STAGING_DIR/$config_name" <<EOF
# Distribution configuration
lxc.include = /usr/share/lxc/config/common.conf
lxc.arch = $LXC_ARCH_STRING
# Container specific configuration
lxc.rootfs.path =
lxc.uts.name =
EOF

  cat > "$STAGING_DIR/MANIFEST" <<EOF
name=$package_name
config=$config_name
rootfs=$rootfs_name
EOF

  log "Creating $output_path"
  (cd "$STAGING_DIR" && tar -cf "$output_path" MANIFEST "$config_name" "$rootfs_name")

  if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
    chown "$SUDO_USER:$(id -gn "$SUDO_USER" 2>/dev/null || echo "$SUDO_USER")" "$output_path"
  fi

  log "Package ready: $output_path"
}

main() {
  parse_args "$@"
  ensure_root
  resolve_binary
  prepare_rootfs "$TARGET_ARCH"
  install_payload "$RESOLVED_BINARY"
  create_package
}

main "$@"