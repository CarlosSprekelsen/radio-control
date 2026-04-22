#!/bin/bash
# UltraLYNX LXC Container Package Builder for Radio Control Container (RCC)
# Usage: sudo ./setup.sh [x86_64|arm64|arm32|all] (--debug|debug|--release|release)
#
# Output: packages/rcc-{x86_64,arm64,arm32}{-debug}-YYYYMMDDTHHMMSSZ.tar (UltraLYNX format)
#   Package contains: MANIFEST, rcc-{arch}_config, rcc-{arch}_rootfs.txz
#   Upload to UltraLYNX hub via WebUI "Containers" tab

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$PROJECT_ROOT/packages}"
CONFIG_SOURCE="${CONFIG_SOURCE:-$PROJECT_ROOT/rcc/config/production.yaml}"
MIRROR_DEFAULT="${MIRROR_DEFAULT:-http://archive.ubuntu.com/ubuntu}"
MIRROR_PORTS="${MIRROR_PORTS:-http://ports.ubuntu.com/ubuntu-ports}"
USE_APT_CACHE_NG="${USE_APT_CACHE_NG:-auto}"
APT_PROXY_URL="${APT_PROXY_URL:-}"
APT_CACHE_REQUIRED="${APT_CACHE_REQUIRED:-false}"
DEBOOTSTRAP_CACHE_DIR="${DEBOOTSTRAP_CACHE_DIR:-$PROJECT_ROOT/.cache/debootstrap}"
DISABLE_DEBOOTSTRAP_CACHE="${DISABLE_DEBOOTSTRAP_CACHE:-false}"
DEBOOTSTRAP_REUSE_BASE_TARBALL="${DEBOOTSTRAP_REUSE_BASE_TARBALL:-true}"
CONFIGURED_ROOTFS_CACHE_DIR="${CONFIGURED_ROOTFS_CACHE_DIR:-$PROJECT_ROOT/.cache/configured-rootfs}"
DISABLE_CONFIGURED_ROOTFS_CACHE="${DISABLE_CONFIGURED_ROOTFS_CACHE:-false}"
CONTAINER_IPV4="${CONTAINER_IPV4:-192.168.101.35}"
CONTAINER_NETMASK="${CONTAINER_NETMASK:-255.255.255.0}"
CONTAINER_GATEWAY="${CONTAINER_GATEWAY:-192.168.101.100}"
BUILD_STAMP="${BUILD_STAMP:-$(date -u +%Y%m%dT%H%M%SZ)}"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log() { echo -e "${GREEN}[BUILD]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

TOTAL_CORES="$(nproc 2>/dev/null || echo 1)"
DEFAULT_BUILD_JOBS="$TOTAL_CORES"
if (( TOTAL_CORES > 2 )); then DEFAULT_BUILD_JOBS=$((TOTAL_CORES - 2)); fi
MAX_BUILD_JOBS="${MAX_BUILD_JOBS:-$DEFAULT_BUILD_JOBS}"
(( MAX_BUILD_JOBS < 1 )) && MAX_BUILD_JOBS=1

# ── APT cache proxy helpers ───────────────────────────────────────────────────

setup_apt_proxy() {
    if [[ -z "$APT_PROXY_URL" ]]; then
        case "${USE_APT_CACHE_NG,,}" in
            true|yes|1|auto)
                APT_PROXY_URL="http://127.0.0.1:3142"
                ;;
        esac
    fi
    [[ -z "$APT_PROXY_URL" ]] && return 0
    if curl -s --connect-timeout 2 --max-time 3 "${APT_PROXY_URL%/}/acng-report.html" \
            -o /dev/null 2>/dev/null; then
        log "APT proxy: $APT_PROXY_URL"
        export http_proxy="$APT_PROXY_URL" https_proxy="$APT_PROXY_URL"
        export HTTP_PROXY="$APT_PROXY_URL" HTTPS_PROXY="$APT_PROXY_URL"
    else
        if [[ "${APT_CACHE_REQUIRED,,}" == "true" ]]; then
            error "APT proxy required but unreachable: $APT_PROXY_URL"
        fi
        warn "APT proxy unreachable ($APT_PROXY_URL), using direct downloads"
        APT_PROXY_URL=""
    fi
}
setup_apt_proxy

get_debootstrap_mirror() {
    echo "$1"
}

ensure_ports_mirror_remap() {
    [[ -n "$APT_PROXY_URL" ]] || return 0
    local config_file="/etc/apt-cacher-ng/acng.conf"
    [[ -f "$config_file" ]] || return 0
    if ! grep -q "Remap-ubuntuports\|ports\.ubuntu\.com" "$config_file"; then
        log "[apt-cache] Adding apt-cacher-ng remap for ports.ubuntu.com..."
        cat >> "$config_file" <<'EOF'

# Ubuntu ports mirror remap — required for arm32 (armhf) and arm64 debootstrap
Remap-ubuntuports: /ports.ubuntu.com/ubuntu-ports ; http://ports.ubuntu.com/ubuntu-ports
EOF
        systemctl restart apt-cacher-ng 2>/dev/null || service apt-cacher-ng restart 2>/dev/null || true
        sleep 2
        log "[apt-cache] apt-cacher-ng restarted with ports remap."
    fi
}

# Cleanup on exit
STAGING_DIR=""
cleanup() {
    [[ "${KEEP_STAGING:-0}" == "1" ]] && return
    [[ -n "$STAGING_DIR" && -d "$STAGING_DIR" ]] && rm -rf "$STAGING_DIR"
}
trap cleanup EXIT INT TERM

# Parse arguments and flags
DEBUG=false
TARGET_ARCH="all"
for arg in "$@"; do
    case "$arg" in
        --debug|debug)
            DEBUG=true
            ;;
        --release|release)
            DEBUG=false
            ;;
        x86_64|arm64|arm32|all)
            TARGET_ARCH="$arg"
            ;;
        *)
            echo "Usage: sudo $0 [x86_64|arm64|arm32|all] [--debug|debug|--release|release]"
            exit 1
            ;;
    esac
done

# Require root (debootstrap, mknod, chroot all need it)
if [[ $EUID -ne 0 ]]; then
    error "Root required for debootstrap. Run: sudo $0 $TARGET_ARCH"
fi

# Preserve original user for ownership fixes
BUILD_USER="${SUDO_USER:-root}"
BUILD_GROUP="$(id -gn "$BUILD_USER" 2>/dev/null || echo root)"

run_as_build_user() {
    if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
        local build_user_home
        build_user_home="$(getent passwd "$SUDO_USER" | cut -d: -f6)"
        [[ -z "$build_user_home" ]] && build_user_home="/home/$SUDO_USER"
        sudo -u "$SUDO_USER" -H env HOME="$build_user_home" "$@"
    else
        "$@"
    fi
}

# Test user configuration for image (used for quick SSH access during testing)
TEST_USER_SSH_KEY="${TEST_USER_SSH_KEY:-}"

# Architecture mapping
get_arch_vars() {
    local arch="$1"
    case "$arch" in
        x86_64)
            DEBOOTSTRAP_ARCH="amd64"
            UBUNTU_MIRROR="$MIRROR_DEFAULT"
            ;;
        arm64)
            DEBOOTSTRAP_ARCH="arm64"
            UBUNTU_MIRROR="$MIRROR_PORTS"
            ;;
        arm32)
            DEBOOTSTRAP_ARCH="armhf"
            UBUNTU_MIRROR="$MIRROR_PORTS"
            ;;
    esac
}

host_arch() {
    local arch
    arch="$(dpkg --print-architecture 2>/dev/null || uname -m)"
    case "$arch" in
        x86_64|amd64) echo "amd64" ;;
        aarch64|arm64) echo "arm64" ;;
        armv7l|armhf) echo "armhf" ;;
        *) echo "$arch" ;;
    esac
}

# Check disk space
AVAIL_GB=$(df -BG "$PROJECT_ROOT" | tail -1 | awk '{print $4}' | sed 's/G//')
if [[ $AVAIL_GB -lt 5 ]]; then
    error "Insufficient disk space: ${AVAIL_GB}GB available, need 5GB minimum"
fi

# Check and install dependencies
log "Checking dependencies..."
MISSING=()
command -v debootstrap >/dev/null 2>&1 || MISSING+=("debootstrap")
command -v cmake >/dev/null 2>&1 || MISSING+=("cmake")
command -v make >/dev/null 2>&1 || MISSING+=("build-essential")
command -v git >/dev/null 2>&1 || MISSING+=("git")
command -v pkg-config >/dev/null 2>&1 || MISSING+=("pkg-config")
command -v tar >/dev/null 2>&1 || MISSING+=("tar")
command -v sha256sum >/dev/null 2>&1 || MISSING+=("coreutils")

if [[ ${#MISSING[@]} -gt 0 ]]; then
    log "Installing missing dependencies: ${MISSING[*]}..."
    apt-get update -qq && apt-get install -y "${MISSING[@]}" || error "Failed to install dependencies"
fi

TAR_CMD=$(command -v tar)
SHA256SUM_CMD=$(command -v sha256sum)

# Check and install QEMU for cross-arch builds
require_cross_tooling() {
    local target="$1"
    local host
    host="$(host_arch)"
    
    [[ "$target" == "$host" ]] && return 0
    
    local need_qemu=false
    if [[ "$target" == "armhf" ]]; then
        if [[ ! -e /proc/sys/fs/binfmt_misc/qemu-arm ]] && ! command -v qemu-arm-static >/dev/null 2>&1; then
            need_qemu=true
        fi
    fi
    if [[ "$target" == "arm64" ]]; then
        if [[ ! -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]] && ! command -v qemu-aarch64-static >/dev/null 2>&1; then
            need_qemu=true
        fi
    fi
    
    if [[ "$need_qemu" == "true" ]] || ! command -v update-binfmts >/dev/null 2>&1; then
        log "Installing QEMU and binfmt-support for cross-arch builds..."
        apt-get update -qq && apt-get install -y qemu-user-static binfmt-support || error "Failed to install QEMU"
    fi
    
    if [[ "$target" == "armhf" ]] && [[ ! -e /proc/sys/fs/binfmt_misc/qemu-arm ]]; then
        log "Enabling qemu-arm binfmt..."
        update-binfmts --enable qemu-arm >/dev/null 2>&1 || true
    fi
    if [[ "$target" == "arm64" ]] && [[ ! -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]]; then
        log "Enabling qemu-aarch64 binfmt..."
        update-binfmts --enable qemu-aarch64 >/dev/null 2>&1 || true
    fi
}

# Get Ubuntu/Debian codename
get_codename() {
    local codename=""
    codename=$(lsb_release -cs 2>/dev/null) || true
    [[ -z "$codename" ]] && codename=$(grep -oP 'VERSION_CODENAME=\K\w+' /etc/os-release 2>/dev/null) || true
    [[ -z "$codename" ]] && codename=$(grep -oP 'UBUNTU_CODENAME=\K\w+' /etc/os-release 2>/dev/null) || true
    [[ -z "$codename" ]] && codename="noble"
    echo "$codename"
}

# Check if target arch needs cross-compilation on this host
needs_cross_compile() {
    local target="$1"
    local host_arch
    host_arch="$(host_arch)"
    
    case "$target" in
        x86_64) [[ "$host_arch" != "amd64" ]] && return 0 ;;
        arm64)  [[ "$host_arch" != "arm64" ]] && return 0 ;;
        arm32)  [[ "$host_arch" != "armhf" ]] && return 0 ;;
    esac
    return 1
}

# Setup multiarch for cross-compilation libraries
setup_multiarch() {
    local arch="$1"
    local dpkg_arch=""
    
    if ! needs_cross_compile "$arch"; then
        log "[$arch] Native architecture, no multiarch needed"
        return 0
    fi
    
    case "$arch" in
        arm64) dpkg_arch="arm64" ;;
        arm32) dpkg_arch="armhf" ;;
        x86_64) dpkg_arch="amd64" ;;
        *) return 0 ;;
    esac
    
    local host_arch
    host_arch="$(host_arch)"
    
    if ! dpkg --print-foreign-architectures 2>/dev/null | grep -q "^${dpkg_arch}$"; then
        log "Adding $dpkg_arch architecture..."
        dpkg --add-architecture "$dpkg_arch"
    fi
    
    local codename
    codename=$(get_codename)
    log "Detected distribution codename: $codename"
    
    if [[ "$host_arch" == "amd64" ]] && [[ "$dpkg_arch" == "arm64" || "$dpkg_arch" == "armhf" ]]; then
        setup_ports_source "$codename"
    elif [[ "$host_arch" == "arm64" || "$host_arch" == "armhf" ]] && [[ "$dpkg_arch" == "amd64" ]]; then
        setup_archive_source "$codename"
    fi
    
    log "Updating package lists..."
    apt-get update || error "Failed to update package lists after adding $dpkg_arch"
}

# Setup ports.ubuntu.com source for ARM packages
setup_ports_source() {
    local codename="$1"
    local ports_deb822="/etc/apt/sources.list.d/ubuntu-ports.sources"
    
    [[ -f "$ports_deb822" ]] && return 0
    
    log "Adding Ubuntu ports repository for ARM packages..."
    
    cat > "$ports_deb822" << EOF
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports
Suites: ${codename} ${codename}-updates ${codename}-security
Components: main restricted universe multiverse
Architectures: arm64 armhf
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
    
    restrict_main_sources "amd64"
}

# Setup archive.ubuntu.com source for x86 packages
setup_archive_source() {
    local codename="$1"
    local archive_deb822="/etc/apt/sources.list.d/ubuntu-archive-amd64.sources"
    
    [[ -f "$archive_deb822" ]] && return 0
    
    log "Adding Ubuntu archive repository for x86 packages..."
    
    cat > "$archive_deb822" << EOF
Types: deb
URIs: http://archive.ubuntu.com/ubuntu
Suites: ${codename} ${codename}-updates ${codename}-security
Components: main restricted universe multiverse
Architectures: amd64 i386
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
    
    local host_arch
    host_arch="$(host_arch)"
    restrict_main_sources "$host_arch"
}

# Restrict main apt sources to specific architecture
restrict_main_sources() {
    local arch="$1"
    
    local deb822_sources="/etc/apt/sources.list.d/ubuntu.sources"
    if [[ -f "$deb822_sources" ]] && ! grep -q '^Architectures:' "$deb822_sources"; then
        log "Restricting ubuntu.sources to $arch..."
        sed -i "/^Types: deb\$/a Architectures: $arch" "$deb822_sources"
    fi
    
    local main_sources="/etc/apt/sources.list"
    if [[ -f "$main_sources" ]] && grep -q '^deb http' "$main_sources"; then
        if ! grep -q '\[arch=' "$main_sources"; then
            log "Restricting sources.list to $arch..."
            sed -i "s|^deb http|deb [arch=$arch] http|g" "$main_sources"
        fi
    fi
}

# Install libraries for target architecture
install_cross_libs() {
    local arch="$1"
    
    if ! needs_cross_compile "$arch"; then
        log "[$arch] Installing native development libraries..."
        apt-get install -y libssl-dev libyaml-cpp-dev nlohmann-json3-dev libasio-dev || \
            error "Failed to install development libraries"
        return 0
    fi
    
    local dpkg_arch=""
    
    case "$arch" in
        arm64) dpkg_arch="arm64" ;;
        arm32) dpkg_arch="armhf" ;;
        x86_64) dpkg_arch="amd64" ;;
        *) return 0 ;;
    esac
    
    if dpkg -l "libssl-dev:${dpkg_arch}" 2>/dev/null | grep -q "^ii"; then
        return 0
    fi
    
    log "Installing cross-compile libraries for $arch..."
    apt-get update -qq
    apt-get install -y \
        "libssl-dev:${dpkg_arch}" \
        "libyaml-cpp-dev:${dpkg_arch}" \
        "libstdc++-14-dev:${dpkg_arch}" 2>/dev/null || \
        apt-get install -y \
            "libssl-dev:${dpkg_arch}" \
            "libyaml-cpp-dev:${dpkg_arch}" \
            "libstdc++-dev:${dpkg_arch}" || \
        error "Failed to install cross-compile libraries for $arch"
}

# Check and install compilers
check_compiler() {
    local arch="$1"
    
    if ! needs_cross_compile "$arch"; then
        for cxx in g++-14 g++-13 g++-12 g++ clang++-14 clang++; do
            command -v "$cxx" >/dev/null 2>&1 && return 0
        done
        log "Installing g++-14..."
        apt-get update -qq && apt-get install -y g++-14 || apt-get install -y g++-13 || apt-get install -y g++-12 || error "Failed to install g++"
        return 0
    fi
    
    case "$arch" in
        arm64)
            for ver in 14 15 13 12; do
                command -v "aarch64-linux-gnu-g++-$ver" >/dev/null 2>&1 && return 0
            done
            command -v aarch64-linux-gnu-g++ >/dev/null 2>&1 && return 0
            log "Installing cross-compiler for arm64..."
            apt-get update -qq && apt-get install -y gcc-14-aarch64-linux-gnu g++-14-aarch64-linux-gnu 2>/dev/null || \
                apt-get install -y gcc-13-aarch64-linux-gnu g++-13-aarch64-linux-gnu 2>/dev/null || \
                apt-get install -y gcc-12-aarch64-linux-gnu g++-12-aarch64-linux-gnu || error "Failed to install arm64 cross-compiler"
            ;;
        arm32)
            for ver in 14 15 13 12; do
                command -v "arm-linux-gnueabihf-g++-$ver" >/dev/null 2>&1 && return 0
            done
            command -v arm-linux-gnueabihf-g++ >/dev/null 2>&1 && return 0
            log "Installing cross-compiler for arm32..."
            apt-get update -qq && apt-get install -y gcc-14-arm-linux-gnueabihf g++-14-arm-linux-gnueabihf 2>/dev/null || \
                apt-get install -y gcc-13-arm-linux-gnueabihf g++-13-arm-linux-gnueabihf 2>/dev/null || \
                apt-get install -y gcc-12-arm-linux-gnueabihf g++-12-arm-linux-gnueabihf || error "Failed to install arm32 cross-compiler"
            ;;
    esac
}

# Validate and setup for requested architectures
if [[ "$TARGET_ARCH" == "all" ]]; then
    for a in x86_64 arm64 arm32; do
        setup_multiarch "$a"
        check_compiler "$a"
        install_cross_libs "$a"
        get_arch_vars "$a"
        require_cross_tooling "$DEBOOTSTRAP_ARCH"
    done
else
    setup_multiarch "$TARGET_ARCH"
    check_compiler "$TARGET_ARCH"
    install_cross_libs "$TARGET_ARCH"
    get_arch_vars "$TARGET_ARCH"
    require_cross_tooling "$DEBOOTSTRAP_ARCH"
fi

log "Host: $(uname -m), Target: $TARGET_ARCH"

# Create output directory
mkdir -p "$OUTPUT_DIR" || error "Cannot create output dir: $OUTPUT_DIR"

# Initialize submodules if needed
if [[ -f "$PROJECT_ROOT/.gitmodules" ]] && [[ ! -f "$PROJECT_ROOT/rcc/dts-common/CMakeLists.txt" ]]; then
    warn "Initializing submodules..."
    cd "$PROJECT_ROOT"
    git submodule update --init --recursive || error "Submodule init failed"
fi

# Create default config if missing
if [[ ! -f "$CONFIG_SOURCE" ]]; then
    warn "Creating default config..."
    mkdir -p "$(dirname "$CONFIG_SOURCE")"
    cat > "$CONFIG_SOURCE" << 'EOF'
server:
  host: 0.0.0.0
  port: 8080
  tls: false

radio:
  default_baud: 115200
  poll_interval_ms: 1000

telemetry:
  buffer_size: 100
  max_clients: 10

logging:
  level: info
  output: stdout
EOF
fi

#=============================================================================
# Compile function
#=============================================================================
compile_binary() {
    local ARCH="$1"
    local BUILD_DIR="$2"
    local BINARY_PATH="$3"
    
    [[ -f "$BINARY_PATH" ]] && { log "[$ARCH] Removing old binary for fresh build..."; rm -f "$BINARY_PATH"; }
    
    log "[$ARCH] Compiling..."
    cd "$PROJECT_ROOT/rcc" || error "Cannot access project root"
    
    local CXX_COMPILER=""
    local C_COMPILER=""
    
    if ! needs_cross_compile "$ARCH"; then
        for cxx in g++-14 g++-13 g++-12 g++ clang++-14 clang++; do
            if command -v "$cxx" >/dev/null 2>&1; then
                CXX_COMPILER="$cxx"
                C_COMPILER="${cxx//++/cc}"
                [[ "$C_COMPILER" == "$cxx" ]] && C_COMPILER="${cxx//clang++/clang}"
                [[ "$C_COMPILER" == "$cxx" ]] && C_COMPILER="${cxx//g++/gcc}"
                break
            fi
        done
    else
        case "$ARCH" in
            arm64)
                for ver in 14 15 13 12; do
                    if command -v "aarch64-linux-gnu-g++-$ver" >/dev/null 2>&1; then
                        CXX_COMPILER="aarch64-linux-gnu-g++-$ver"
                        C_COMPILER="aarch64-linux-gnu-gcc-$ver"
                        break
                    fi
                done
                [[ -z "$CXX_COMPILER" ]] && command -v aarch64-linux-gnu-g++ >/dev/null 2>&1 && {
                    CXX_COMPILER="aarch64-linux-gnu-g++"
                    C_COMPILER="aarch64-linux-gnu-gcc"
                }
                ;;
            arm32)
                for ver in 14 15 13 12; do
                    if command -v "arm-linux-gnueabihf-g++-$ver" >/dev/null 2>&1; then
                        CXX_COMPILER="arm-linux-gnueabihf-g++-$ver"
                        C_COMPILER="arm-linux-gnueabihf-gcc-$ver"
                        break
                    fi
                done
                [[ -z "$CXX_COMPILER" ]] && command -v arm-linux-gnueabihf-g++ >/dev/null 2>&1 && {
                    CXX_COMPILER="arm-linux-gnueabihf-g++"
                    C_COMPILER="arm-linux-gnueabihf-gcc"
                }
                ;;
        esac
    fi
    
    [[ -z "$CXX_COMPILER" ]] && error "[$ARCH] No compiler found"
    log "[$ARCH] Using $CXX_COMPILER"
    
    [[ -f "$BUILD_DIR/CMakeCache.txt" ]] && rm -rf "$BUILD_DIR"
    
    mkdir -p "$BUILD_DIR" || error "Cannot create build dir: $BUILD_DIR"
    cd "$BUILD_DIR" || error "Cannot cd to build dir"
    
    local CMAKE_ARGS=(
        "$PROJECT_ROOT/rcc"
        "-DCMAKE_BUILD_TYPE=Release"
        "-DRCC_BUILD_TESTS=OFF"
        "-DRCC_ENABLE_LTO=ON"
        "-DCMAKE_C_COMPILER=$C_COMPILER"
        "-DCMAKE_CXX_COMPILER=$CXX_COMPILER"
    )
    
    if needs_cross_compile "$ARCH"; then
        CMAKE_ARGS+=("-DCMAKE_SYSTEM_NAME=Linux")
        CMAKE_ARGS+=(
            "-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER"
            "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY"
            "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH"
            "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH"
        )
        
        case "$ARCH" in
            arm64)
                CMAKE_ARGS+=(
                    "-DCMAKE_SYSTEM_PROCESSOR=aarch64"
                    "-DCMAKE_FIND_ROOT_PATH=/usr/lib/aarch64-linux-gnu;/usr/share"
                    "-DOPENSSL_ROOT_DIR=/usr/lib/aarch64-linux-gnu"
                    "-DOPENSSL_INCLUDE_DIR=/usr/include/aarch64-linux-gnu"
                )
                ;;
            arm32)
                CMAKE_ARGS+=(
                    "-DCMAKE_SYSTEM_PROCESSOR=arm"
                    "-DCMAKE_FIND_ROOT_PATH=/usr/lib/arm-linux-gnueabihf;/usr/share"
                    "-DOPENSSL_ROOT_DIR=/usr/lib/arm-linux-gnueabihf"
                    "-DOPENSSL_INCLUDE_DIR=/usr/include/arm-linux-gnueabihf"
                )
                ;;
        esac
    fi
    
    log "[$ARCH] Running CMake..."
    if ! run_as_build_user cmake "${CMAKE_ARGS[@]}" 2>&1 | tee cmake.log; then
        error "[$ARCH] CMake failed. Check: $BUILD_DIR/cmake.log"
    fi
    
    log "[$ARCH] Building..."
    if ! run_as_build_user cmake --build . --parallel "$MAX_BUILD_JOBS" 2>&1 | tee build.log; then
        error "[$ARCH] Build failed. Check: $BUILD_DIR/build.log"
    fi
    
    cd "$PROJECT_ROOT"
    
    [[ ! -f "$BINARY_PATH" ]] && error "[$ARCH] Binary not created: $BINARY_PATH"
    [[ ! -x "$BINARY_PATH" ]] && error "[$ARCH] Binary not executable: $BINARY_PATH"
    
    log "[$ARCH] Compiled: $BINARY_PATH"
}

#=============================================================================
# Build image using debootstrap
#=============================================================================
build_image() {
    local ARCH="$1"
    local BINARY_PATH="$2"
    
    [[ ! -f "$BINARY_PATH" ]] && { warn "[$ARCH] Binary missing, skipping"; return 1; }
    
    get_arch_vars "$ARCH"
    
    local base_tmp="${TMPDIR:-/var/tmp}"
    STAGING_DIR=$(mktemp -d "$base_tmp/rcc-lxc.XXXXXX")
    log "[$ARCH] Staging dir: $STAGING_DIR"
    
    local rootfs="$STAGING_DIR/rootfs"
    mkdir -p "$rootfs"
    
    log "[$ARCH] Creating minimal rootfs via debootstrap..."
    local INCLUDE_PKGS=(ca-certificates libssl3t64 libyaml-cpp0.8)
    if [[ "$DEBUG" == "true" ]]; then
        INCLUDE_PKGS+=(openssh-server net-tools curl vim strace tcpdump iproute2 htop binutils file procps)
    fi
    local INCLUDE_CSV
    INCLUDE_CSV=$(IFS=,; echo "${INCLUDE_PKGS[*]}")

    local pkg_hash
    pkg_hash="$(printf '%s' "$INCLUDE_CSV" | sha256sum | awk '{print substr($1,1,12)}')"
    local configured_rootfs_tarball="$CONFIGURED_ROOTFS_CACHE_DIR/noble-${DEBOOTSTRAP_ARCH}-${pkg_hash}.tar.gz"

    if [[ "${DISABLE_CONFIGURED_ROOTFS_CACHE,,}" != "true" ]] && [[ -f "$configured_rootfs_tarball" ]]; then
        log "[$ARCH] Restoring configured rootfs from cache (skipping debootstrap)..."
        tar -xzf "$configured_rootfs_tarball" -C "$rootfs" \
            || error "[$ARCH] Failed to extract configured rootfs cache: $configured_rootfs_tarball"
        log "[$ARCH] Configured rootfs restored."
    else
        if [[ "$ARCH" == "arm32" || "$ARCH" == "arm64" ]]; then
            ensure_ports_mirror_remap
        fi

        local mirror_url direct_mirror_url
        direct_mirror_url="$UBUNTU_MIRROR"
        mirror_url="$(get_debootstrap_mirror "$UBUNTU_MIRROR")"
        log "[$ARCH] Running debootstrap (noble, $mirror_url)..."

        local tarball_dir="$PROJECT_ROOT/.cache/debootstrap-tarballs"
        local base_tarball="$tarball_dir/noble-${DEBOOTSTRAP_ARCH}-${pkg_hash}.tar.gz"

        local DEBOOTSTRAP_ARGS=(
            --arch="$DEBOOTSTRAP_ARCH"
            --variant=minbase
            --components=main,universe
            --include="$INCLUDE_CSV"
        )
        if [[ "${DISABLE_DEBOOTSTRAP_CACHE,,}" != "true" ]]; then
            mkdir -p "$DEBOOTSTRAP_CACHE_DIR/$DEBOOTSTRAP_ARCH"
            DEBOOTSTRAP_ARGS+=(--cache-dir="$DEBOOTSTRAP_CACHE_DIR/$DEBOOTSTRAP_ARCH")
        fi

        _run_debootstrap() {
            env -u http_proxy -u https_proxy -u HTTP_PROXY -u HTTPS_PROXY \
                debootstrap "${DEBOOTSTRAP_ARGS[@]}" "$@"
        }

        if [[ "${DEBOOTSTRAP_REUSE_BASE_TARBALL,,}" == "true" ]]; then
            mkdir -p "$tarball_dir"
            if [[ -f "$base_tarball" ]]; then
                log "[$ARCH] Reusing debootstrap tarball: $base_tarball"
                _run_debootstrap --unpack-tarball="$base_tarball" noble "$rootfs" "$mirror_url" \
                    || error "[$ARCH] debootstrap --unpack-tarball failed"
            else
                log "[$ARCH] Creating debootstrap tarball cache (first run — two passes): $base_tarball"
                _run_debootstrap --make-tarball="$base_tarball" noble "$rootfs" "$mirror_url" \
                    || error "[$ARCH] debootstrap --make-tarball failed"
                [[ -f "$base_tarball" ]] || error "[$ARCH] debootstrap did not produce tarball at $base_tarball"
                log "[$ARCH] Tarball created; installing rootfs from tarball..."
                _run_debootstrap --unpack-tarball="$base_tarball" noble "$rootfs" "$mirror_url" \
                    || error "[$ARCH] debootstrap --unpack-tarball (second pass) failed"
            fi
        else
            _run_debootstrap noble "$rootfs" "$mirror_url" || error "[$ARCH] debootstrap failed"
        fi

        chroot "$rootfs" /bin/bash -c 'apt-get clean && rm -rf /var/lib/apt/lists/*' 2>/dev/null || true

        if [[ "${DISABLE_CONFIGURED_ROOTFS_CACHE,,}" != "true" ]]; then
            log "[$ARCH] Saving configured rootfs cache: $configured_rootfs_tarball"
            mkdir -p "$CONFIGURED_ROOTFS_CACHE_DIR"
            tar -czf "${configured_rootfs_tarball}.tmp" -C "$rootfs" . \
                && mv "${configured_rootfs_tarball}.tmp" "$configured_rootfs_tarball" \
                || warn "[$ARCH] Failed to save configured rootfs cache (non-fatal)"
        fi
    fi
    
    # Create app directories
    log "[$ARCH] Installing application..."
    mkdir -p "$rootfs"/app
    mkdir -p "$rootfs"/etc/rcc
    mkdir -p "$rootfs"/var/log/rcc
    mkdir -p "$rootfs"/var/lib/rcc
    
    # Copy binary
    cp "$BINARY_PATH" "$rootfs/app/radio-control-container"
    chmod 755 "$rootfs/app/radio-control-container"
    
    # Copy config
    cp "$CONFIG_SOURCE" "$rootfs/etc/rcc/config.yaml"
    
    # Copy dts-common shared libraries
    BUILD_DIR_FROM_BINARY="$(cd "$(dirname "$(dirname "$BINARY_PATH")")" && pwd)"
    log "[$ARCH] Looking for dts-common libraries in: $BUILD_DIR_FROM_BINARY"
    shopt -s nullglob
    for pattern in \
        "$BUILD_DIR_FROM_BINARY/_deps/dts-common*/libdts-common"*.so* \
        "$BUILD_DIR_FROM_BINARY/dts-common/libdts-common"*.so* \
        "$BUILD_DIR_FROM_BINARY"/libdts-common*.so*; do
        for lib in $pattern; do
            if [[ -f "$lib" ]]; then
                log "[$ARCH] Copying $(basename "$lib") to /var/lib/rcc"
                cp -a "$lib" "$rootfs/var/lib/rcc/" || warn "[$ARCH] Failed to copy $lib"
            fi
        done
    done
    shopt -u nullglob

    # Update dynamic linker cache
    log "[$ARCH] Updating dynamic linker cache..."
    chroot "$rootfs" /sbin/ldconfig 2>/dev/null || true
    
    # Create rcc user (UID/GID 1000)
    log "[$ARCH] Creating service user..."
    chroot "$rootfs" /bin/bash -c '
        groupadd -r -g 1000 rcc 2>/dev/null || true
        useradd -r -u 1000 -g 1000 -d /app -s /bin/false rcc 2>/dev/null || true
        chown -R rcc:rcc /app /var/log/rcc /var/lib/rcc /etc/rcc
    '
    
    # Generate SSH host keys when debug enabled
    if [[ "$DEBUG" == "true" ]]; then
        log "[$ARCH] Generating SSH host keys for debug mode..."
        chroot "$rootfs" /bin/bash -c '
            if command -v ssh-keygen >/dev/null 2>&1; then
                ssh-keygen -A || true
            fi
        '
        echo 'export DEBUG_MODE=true' > "$rootfs/etc/environment.rcc"
    fi

    # Configure RCC environment variables
    log "[$ARCH] Configuring RCC environment variables..."
    cat >> "$rootfs/etc/environment.rcc" <<'ENVEOF'
export RCC_CONTAINER_ID="rcc-dev-001"
export RCC_SOLDIER_ID="soldier-001"
ENVEOF
    printf 'export CONTAINER_IPV4="%s"\nexport CONTAINER_GATEWAY="%s"\n' \
        "${CONTAINER_IPV4}" "${CONTAINER_GATEWAY}" >> "$rootfs/etc/environment.rcc"

    # Create test user 'ubuntu' for SSH access
    log "[$ARCH] Creating test user 'ubuntu' for SSH access..."
    chroot "$rootfs" /bin/bash -c "set -e
        if ! id -u ubuntu >/dev/null 2>&1; then
            if getent group sudo >/dev/null 2>&1; then
                useradd -m -s /bin/bash -G sudo ubuntu || useradd -m -s /bin/bash ubuntu || true
            else
                useradd -m -s /bin/bash ubuntu || true
            fi
        fi
        echo 'ubuntu:ubuntu' | chpasswd || true
        mkdir -p /home/ubuntu/.ssh
        chown ubuntu:ubuntu /home/ubuntu/.ssh || true
        chmod 700 /home/ubuntu/.ssh || true
    "

    # Install SSH key if provided
    if [[ -n "${TEST_USER_SSH_KEY}" ]]; then
        log "[$ARCH] Installing provided SSH key for 'ubuntu' user"
        mkdir -p "$rootfs/home/ubuntu/.ssh"
        echo "${TEST_USER_SSH_KEY}" > "$rootfs/home/ubuntu/.ssh/authorized_keys"
        chown 1000:1000 "$rootfs/home/ubuntu/.ssh/authorized_keys" 2>/dev/null || true
        chmod 600 "$rootfs/home/ubuntu/.ssh/authorized_keys" || true
    fi

    # Configure container networking
    log "[$ARCH] Configuring container networking (static IP inside container)..."
    mkdir -p "$rootfs/etc/network/interfaces.d"
    cat > "$rootfs/etc/network/interfaces.d/eth0" <<EOF
auto eth0
iface eth0 inet static
    address ${CONTAINER_IPV4}
    netmask ${CONTAINER_NETMASK}
    gateway ${CONTAINER_GATEWAY}
EOF

    if [[ ! -f "$rootfs/etc/network/interfaces" ]]; then
        cat > "$rootfs/etc/network/interfaces" << 'EOF'
source /etc/network/interfaces.d/*
EOF
    fi
    
    # Create /etc/rc.local init script (replaces systemd service)
    log "[$ARCH] Creating /etc/rc.local init script..."
    tee "$rootfs/etc/rc.local" >/dev/null <<'EOF'
#!/bin/sh
# Radio Control Container startup script
# Runs as root to set up, then starts service as rcc user

mkdir -p /var/run /var/log/rcc

# Source environment variables
if [ -f /etc/environment.rcc ]; then
    . /etc/environment.rcc
fi

# Export for all child processes
export RCC_CONTAINER_ID
export RCC_SOLDIER_ID
export CONTAINER_IPV4
export CONTAINER_GATEWAY

# Bring up network
if command -v ifup >/dev/null 2>&1; then
    ifup eth0 2>/dev/null || true
fi

# Update library cache
/sbin/ldconfig 2>/dev/null || true

# Start SSH in debug mode
if [ "$DEBUG_MODE" = "true" ] && command -v sshd >/dev/null 2>&1; then
    mkdir -p /run/sshd
    /usr/sbin/sshd
fi

# Drop privileges and start radio-control-container
cd /app || exit 1
exec su -s /bin/sh rcc -c '
    export RCC_CONTAINER_ID="'"$RCC_CONTAINER_ID"'"
    export RCC_SOLDIER_ID="'"$RCC_SOLDIER_ID"'"
    export LD_LIBRARY_PATH=/var/lib/rcc:$LD_LIBRARY_PATH
    exec /app/radio-control-container
'
EOF
    chmod +x "$rootfs/etc/rc.local"

    # Create /sbin/init fallback
    log "[$ARCH] Creating /sbin/init fallback..."
    tee "$rootfs/sbin/init" >/dev/null <<'EOF'
#!/bin/sh
# Fallback init for UltraLYNX containers
exec /etc/rc.local
EOF
    chmod +x "$rootfs/sbin/init"

    # Create LXC config file
    log "[$ARCH] Creating LXC config file (rcc-${ARCH}_config)..."
    local CONFIG_NAME="rcc-${ARCH}_config"
    tee "$STAGING_DIR/$CONFIG_NAME" >/dev/null <<EOF
lxc.uts.name = rcc-${ARCH}
lxc.arch = $DEBOOTSTRAP_ARCH
lxc.rootfs.path = /var/lib/lxc/rcc-${ARCH}/rootfs

# Network (configured inside container via /etc/network/interfaces)
lxc.net.0.type = veth
lxc.net.0.link = lxcbr0
lxc.net.0.flags = up
lxc.net.0.name = eth0

# Security
lxc.apparmor.profile = unconfined
lxc.cgroup.devices.allow = a
lxc.cap.drop = 

# Mounts
lxc.mount.auto = cgroup:mixed proc:mixed sys:mixed
lxc.mount.entry = /dev dev none bind,create=dir 0 0
EOF
    
    # Create rootfs archive
    log "[$ARCH] Creating rcc-${ARCH}_rootfs.txz..."
    local ROOTFS_NAME="rcc-${ARCH}_rootfs.txz"
    tar -C "$rootfs" -cJf "$STAGING_DIR/$ROOTFS_NAME" .
    
    # Create package
    local output_suffix=""
    [[ "$DEBUG" == "true" ]] && output_suffix="-debug"

    # Stable container name (used in MANIFEST — INVISIO treats this as the identity;
    # it must NOT vary per build or INVISIO registers a new container every upload).
    local package_prefix="rcc-${ARCH}${output_suffix}"
    # Timestamped tar filename preserves build history on the host filesystem.
    local package_name="${package_prefix}-${BUILD_STAMP}"

    cat > "$STAGING_DIR/MANIFEST" <<EOF
name=$package_prefix
config=$CONFIG_NAME
rootfs=$ROOTFS_NAME
EOF

    # Create final package (UltraLYNX format: uncompressed tar)
    local output="$OUTPUT_DIR/${package_name}.tar"
    log "[$ARCH] Creating UltraLYNX package (uncompressed tar)..."
    (cd "$STAGING_DIR" && "$TAR_CMD" -cf "$output" MANIFEST "$CONFIG_NAME" "$ROOTFS_NAME")
    chown "$BUILD_USER:$BUILD_GROUP" "$output"

    # Create stable filename copy for build automation
    local stable_name="${package_prefix}.tar"
    local stable_output="$OUTPUT_DIR/${stable_name}"
    cp -f "$output" "$stable_output"
    chown "$BUILD_USER:$BUILD_GROUP" "$stable_output"
    log "[$ARCH] Stable package copy: $stable_output"
    
    # Cleanup
    rm -rf "$rootfs"
    rm -rf "$STAGING_DIR"
    STAGING_DIR=""
    
    log "[$ARCH] Package: $output ($(du -h "$output" | cut -f1))"
    log "[$ARCH] Done"
}

#=============================================================================
# Main build loop
#=============================================================================
log "Output: $OUTPUT_DIR"

FAILED_ARCHS=()

build_arch() {
    local arch="$1"
    local build_dir="$PROJECT_ROOT/rcc/build-$arch"
    local binary="$build_dir/src/radio-control-container"
    
    if compile_binary "$arch" "$build_dir" "$binary" && build_image "$arch" "$binary"; then
        log "$arch complete"
        return 0
    else
        return 1
    fi
}

if [[ "$TARGET_ARCH" == "all" ]]; then
    for arch in x86_64 arm64 arm32; do
        build_arch "$arch" || FAILED_ARCHS+=("$arch")
    done
else
    build_arch "$TARGET_ARCH" || FAILED_ARCHS+=("$TARGET_ARCH")
fi

# Fix ownership
log "Fixing ownership to $BUILD_USER..."
chown -R "$BUILD_USER:$BUILD_GROUP" "$OUTPUT_DIR" 2>/dev/null || true

log "========================================"
log "Build Complete"
log "========================================"
log ""

if [[ "$TARGET_ARCH" == "all" ]]; then
    log "Deploy:"
    log "  Upload rcc-{arm32|arm64|x86_64}{-debug}-${BUILD_STAMP}.tar via UltraLYNX WebUI"
else
    suffix=""
    [[ "$DEBUG" == "true" ]] && suffix="-debug"
    log "Deploy:"
    log "  1. Upload rcc-${TARGET_ARCH}${suffix}-${BUILD_STAMP}.tar via UltraLYNX WebUI"
    log "  2. Go to Containers tab -> Upload -> Select .tar file"
    log "  3. Start container from WebUI"
fi

if [[ ${#FAILED_ARCHS[@]} -gt 0 ]]; then
    warn "Failed architectures: ${FAILED_ARCHS[*]}"
fi

ls -lh "$OUTPUT_DIR"/*.tar 2>/dev/null || warn "No packages created"

if [[ ${#FAILED_ARCHS[@]} -gt 0 ]]; then
    exit 1
fi
