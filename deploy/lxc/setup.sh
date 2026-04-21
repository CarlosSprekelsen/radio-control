#!/bin/bash
# UltraLYNX LXC Container Package Builder for Radio Control Container
# Usage: sudo ./setup.sh [arm32|arm64|amd64] [--debug|--release]
#
# Output: packages/rcc-{arm32,arm64,amd64}{-debug}-YYYYMMDDTHHMMSSZ.tar
#   Package contains: MANIFEST, {name}_config, {name}_rootfs.txz
#   Cross-arch packaging expects a prebuilt binary unless packaging natively.
#
# Caching (speeds up repeated builds):
#   .cache/debootstrap/         - debootstrap package cache
#   .cache/debootstrap-tarball/ - base rootfs tarball (saves ~5-10 min)
#   Set DISABLE_DEBOOTSTRAP_CACHE=true to force fresh debootstrap.

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

# Caching configuration
DEBOOTSTRAP_CACHE_DIR="${DEBOOTSTRAP_CACHE_DIR:-$PROJECT_ROOT/.cache/debootstrap}"
DISABLE_DEBOOTSTRAP_CACHE="${DISABLE_DEBOOTSTRAP_CACHE:-false}"
USE_APT_CACHE_NG="${USE_APT_CACHE_NG:-auto}"
APT_PROXY_URL="${APT_PROXY_URL:-}"

TARGET_ARCH="arm32"
DEBUG_MODE="false"

# Test user configuration for image (used for quick SSH access during testing)
# You may provide `TEST_USER_SSH_KEY` env var to inject a public key. Password is always 'ubuntu'.
TEST_USER_SSH_KEY="${TEST_USER_SSH_KEY:-}"

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

# Detect and configure apt-cacher-ng if available
detect_apt_proxy() {
  local proxy=""
  
  # Use explicit proxy if set
  if [[ -n "$APT_PROXY_URL" ]]; then
    echo "$APT_PROXY_URL"
    return 0
  fi
  
  # Auto-detect if enabled
  if [[ "$USE_APT_CACHE_NG" == "auto" ]]; then
    # Check common apt-cacher-ng ports
    if curl -sf --max-time 1 "http://localhost:3142" >/dev/null 2>&1; then
      proxy="http://localhost:3142"
    elif curl -sf --max-time 1 "http://127.0.0.1:3142" >/dev/null 2>&1; then
      proxy="http://127.0.0.1:3142"
    fi
  fi
  
  echo "$proxy"
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

  # Always rebuild: deployment means shipping new code, not cached old code.
  for candidate in \
    "$PROJECT_ROOT/rcc/build/src/radio-control-container" \
    "$PROJECT_ROOT/rcc/build/radio-control-container"; do
    if [[ -f "$candidate" ]]; then
      log "Removing cached binary — forcing rebuild for deployment"
      rm -f "$candidate"
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

  # Setup caching
  local cache_dir="$DEBOOTSTRAP_CACHE_DIR/$target_arch"
  local tarball_dir="$PROJECT_ROOT/.cache/debootstrap-tarball"
  local tarball_name="rcc-${UBUNTU_RELEASE}-${DEBOOTSTRAP_ARCH}.tar.gz"
  local tarball_path="$tarball_dir/$tarball_name"
  local apt_proxy
  apt_proxy="$(detect_apt_proxy)"
  
  mkdir -p "$cache_dir" "$tarball_dir"
  
  local debootstrap_args=(
    --arch="$DEBOOTSTRAP_ARCH"
    --variant=minbase
  )
  
  # Add cache directory if not disabled
  if [[ "$DISABLE_DEBOOTSTRAP_CACHE" != "true" ]]; then
    debootstrap_args+=(--cache-dir="$cache_dir")
  fi
  
  # Add apt proxy if detected
  if [[ -n "$apt_proxy" ]]; then
    log "Using apt proxy: $apt_proxy"
    debootstrap_args+=(--aptopt="Acquire::http::Proxy \"$apt_proxy\";")
  fi

  if [[ "$(host_arch)" == "$target_arch" ]]; then
    # Native build - try to use cached tarball
    if [[ "$DISABLE_DEBOOTSTRAP_CACHE" != "true" && -f "$tarball_path" ]]; then
      log "Using cached debootstrap tarball: $tarball_path"
      debootstrap "${debootstrap_args[@]}" --unpack-tarball="$tarball_path" "$UBUNTU_RELEASE" "$ROOTFS_DIR" "$UBUNTU_MIRROR"
    else
      log "Running debootstrap (this may take 10-30 min on first run)..."
      debootstrap "${debootstrap_args[@]}" "$UBUNTU_RELEASE" "$ROOTFS_DIR" "$UBUNTU_MIRROR"
      # Cache the tarball for next time
      if [[ "$DISABLE_DEBOOTSTRAP_CACHE" != "true" ]]; then
        log "Creating tarball cache: $tarball_path"
        tar -czf "$tarball_path" -C "$ROOTFS_DIR" . 2>/dev/null || log "Warning: Failed to create tarball cache"
      fi
    fi
  else
    # Cross-arch build
    local qemu_binary
    qemu_binary="$(qemu_static_path "$target_arch")"
    [[ -n "$qemu_binary" ]] || error "Missing qemu static binary for $target_arch"
    
    if [[ "$DISABLE_DEBOOTSTRAP_CACHE" != "true" && -f "$tarball_path" ]]; then
      log "Using cached debootstrap tarball: $tarball_path"
      debootstrap "${debootstrap_args[@]}" --foreign --unpack-tarball="$tarball_path" "$UBUNTU_RELEASE" "$ROOTFS_DIR" "$UBUNTU_MIRROR"
    else
      log "Running cross-arch debootstrap (this may take 10-30 min on first run)..."
      debootstrap "${debootstrap_args[@]}" --foreign "$UBUNTU_RELEASE" "$ROOTFS_DIR" "$UBUNTU_MIRROR"
      if [[ "$DISABLE_DEBOOTSTRAP_CACHE" != "true" ]]; then
        log "Creating tarball cache: $tarball_path"
        tar -czf "$tarball_path" -C "$ROOTFS_DIR" . 2>/dev/null || log "Warning: Failed to create tarball cache"
      fi
    fi
    
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

  # Configure apt proxy in chroot if detected
  if [[ -n "$apt_proxy" ]]; then
    echo "Acquire::http::Proxy \"$apt_proxy\";" > "$ROOTFS_DIR/etc/apt/apt.conf.d/01proxy"
  fi

  # Enable universe repository for additional packages
  chroot "$ROOTFS_DIR" /bin/bash -lc 'apt-get update && apt-get install -y --no-install-recommends software-properties-common && add-apt-repository -y universe && apt-get update'
  chroot "$ROOTFS_DIR" /bin/bash -lc 'DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates libssl-dev libyaml-cpp-dev libfmt-dev nlohmann-json3-dev libasio-dev nginx-light && apt-get clean && rm -rf /var/lib/apt/lists/*'

  if [[ "$DEBUG_MODE" == "true" ]]; then
    chroot "$ROOTFS_DIR" /bin/bash -lc 'apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends curl iproute2 net-tools procps openssh-server && apt-get clean && rm -rf /var/lib/apt/lists/*'

    # Generate SSH host keys
    chroot "$ROOTFS_DIR" /bin/bash -c 'if command -v ssh-keygen >/dev/null 2>&1; then ssh-keygen -A || true; fi' 2>/dev/null || true

    # Configure SSH daemon for debug access (BCC reference pattern)
    mkdir -p "$ROOTFS_DIR/etc/ssh"
    cat > "$ROOTFS_DIR/etc/ssh/sshd_config" <<'SSH_EOF'
Port 22
AddressFamily any
ListenAddress 0.0.0.0
ListenAddress ::
PermitRootLogin yes
PasswordAuthentication yes
PubkeyAuthentication yes
X11Forwarding no
Subsystem sftp /usr/lib/openssh/sftp-server
SSH_EOF

    # Create test user 'ubuntu' with password 'ubuntu' for debug access
    # Add to sudo group if it exists (BCC/GPSD reference pattern)
    chroot "$ROOTFS_DIR" /bin/bash -c '
      if ! id -u ubuntu >/dev/null 2>&1; then
        if getent group sudo >/dev/null 2>&1; then
          useradd -m -s /bin/bash -G sudo ubuntu || useradd -m -s /bin/bash ubuntu || true
        else
          useradd -m -s /bin/bash ubuntu || true
        fi
      fi
      echo "ubuntu:ubuntu" | chpasswd || true
      mkdir -p /home/ubuntu/.ssh || true
      chown ubuntu:ubuntu /home/ubuntu/.ssh || true
      chmod 700 /home/ubuntu/.ssh || true
    ' 2>/dev/null || true

    # If TEST_USER_SSH_KEY provided on the host, write it into authorized_keys
    if [[ -n "${TEST_USER_SSH_KEY:-}" ]]; then
      log "Installing provided SSH key for 'ubuntu' user"
      mkdir -p "$ROOTFS_DIR/home/ubuntu/.ssh"
      echo "${TEST_USER_SSH_KEY}" > "$ROOTFS_DIR/home/ubuntu/.ssh/authorized_keys"
      chown 1000:1000 "$ROOTFS_DIR/home/ubuntu/.ssh/authorized_keys" 2>/dev/null || true
      chmod 600 "$ROOTFS_DIR/home/ubuntu/.ssh/authorized_keys" 2>/dev/null || true
    fi

    # Set root password for debug
    chroot "$ROOTFS_DIR" /bin/bash -c 'echo "root:debug123" | chpasswd' 2>/dev/null || true

    # Mark debug mode
    echo 'DEBUG_MODE=true' > "$ROOTFS_DIR/etc/environment.rcc"
  fi
}

write_nginx_config() {
  cat > "$ROOTFS_DIR/etc/nginx/sites-available/rcc" <<'NGX_EOF'
server {
  listen 80 default_server;
  server_name _;

  proxy_http_version 1.1;
  proxy_set_header Host $host;
  proxy_set_header X-Real-IP $remote_addr;
  proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

  location = / {
    proxy_pass http://127.0.0.1:8084/monitor;
  }

  location = /monitor {
    proxy_pass http://127.0.0.1:8084/monitor;
  }
}
NGX_EOF
  mkdir -p "$ROOTFS_DIR/etc/nginx/sites-enabled"
  rm -f "$ROOTFS_DIR/etc/nginx/sites-enabled/default"
  ln -sf /etc/nginx/sites-available/rcc "$ROOTFS_DIR/etc/nginx/sites-enabled/default"
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
mkdir -p /var/log/rcc /var/log/nginx

# Detect first non-lo interface
INTERFACE=""
for iface in /sys/class/net/*; do
  name="$(basename "$iface")"
  [ "$name" = "lo" ] && continue
  INTERFACE="$name"
  break
done

if [ -z "$INTERFACE" ]; then
  echo "$(date): FATAL - no network interface found" >> "$LOG_FILE"
  exit 1
fi

# --- Unified network readiness loop ---
STATIC_IP_CIDR="${STATIC_IP:-192.168.101.36/24}"
STATIC_GW="${STATIC_GATEWAY:-192.168.101.100}"
NET_TIMEOUT=90
NET_ELAPSED=0
NET_READY=false

while [ "$NET_ELAPSED" -lt "$NET_TIMEOUT" ]; do
  ip link set "$INTERFACE" up 2>/dev/null || true

  if ! ip addr show "$INTERFACE" 2>/dev/null | grep -q "inet ${STATIC_IP_CIDR%/*}/"; then
    ip addr add "$STATIC_IP_CIDR" dev "$INTERFACE" 2>/dev/null || true
  fi

  if ! ip route show default 2>/dev/null | grep -q "via $STATIC_GW"; then
    ip route replace default via "$STATIC_GW" dev "$INTERFACE" 2>/dev/null || true
  fi

  if ping -c1 -W1 "$STATIC_GW" >/dev/null 2>&1; then
    NET_READY=true
    echo "$(date): Network ready after ${NET_ELAPSED}s" >> "$LOG_FILE"
    break
  fi

  NET_ELAPSED=$((NET_ELAPSED + 1))
  sleep 1
done

if [ "$NET_READY" = "false" ]; then
  echo "$(date): WARNING - Network not ready after ${NET_TIMEOUT}s, starting service anyway" >> "$LOG_FILE"
fi

# Source DEBUG_MODE if set
[ -f /etc/environment.rcc ] && . /etc/environment.rcc

# Start SSH in debug mode
if [ "${DEBUG_MODE:-}" = "true" ] && [ -x /usr/sbin/sshd ]; then
  echo "$(date): Starting SSH server (debug mode)..." >> "$LOG_FILE"
  mkdir -p /run/sshd
  chmod 755 /run/sshd
  /usr/sbin/sshd >> "$LOG_FILE" 2>&1 &
fi

# Start nginx front-door
if command -v nginx >/dev/null 2>&1; then
  echo "$(date): Starting nginx front-door..." >> "$LOG_FILE"
  nginx >> "$LOG_FILE" 2>&1 &
fi

(
  while true; do
    /usr/local/bin/radio-control-container --config /etc/rcc/config.yaml >>"$LOG_FILE" 2>&1 || true
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

  # INVISIO identity must be stable across rebuilds; only the tar filename carries BUILD_STAMP.
  cat > "$STAGING_DIR/MANIFEST" <<EOF
name=$package_member_prefix
config=$config_name
rootfs=$rootfs_name
EOF

  log "Creating $output_path"
  (cd "$STAGING_DIR" && tar -cf "$output_path" MANIFEST "$config_name" "$rootfs_name")

  if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
    chown "$SUDO_USER:$(id -gn "$SUDO_USER" 2>/dev/null || echo "$SUDO_USER")" "$output_path"
  fi

  # Stable filename copy for build automation (orchestrator expects <prefix>.tar)
  local stable_output="$PACKAGES_DIR/${package_member_prefix}.tar"
  cp -f "$output_path" "$stable_output"
  if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
    chown "$SUDO_USER:$(id -gn "$SUDO_USER" 2>/dev/null || echo "$SUDO_USER")" "$stable_output"
  fi
  log "Stable package copy: $stable_output"

  log "Package ready: $output_path"
  
  # Print cache status
  local cache_dir="$DEBOOTSTRAP_CACHE_DIR/$TARGET_ARCH"
  local tarball_path="$PROJECT_ROOT/.cache/debootstrap-tarball/rcc-${UBUNTU_RELEASE}-$(arch_vars "$TARGET_ARCH" >/dev/null; echo "$DEBOOTSTRAP_ARCH").tar.gz"
  log "Cache locations:"
  log "  - Debootstrap cache: $cache_dir"
  log "  - Tarball cache: $tarball_path"
}

main() {
  parse_args "$@"
  ensure_root
  resolve_binary
  prepare_rootfs "$TARGET_ARCH"
  write_nginx_config
  install_payload "$RESOLVED_BINARY"
  create_package
}

main "$@"
