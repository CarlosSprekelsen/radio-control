#!/bin/bash
# Podman OCI Container Image Builder for Radio Control Container (RCC)
# Usage: ./setup.sh [x86_64|arm64] [--debug=true|false]
#
# Output: OCI container image (radio-control:x86_64, radio-control:arm64, or with -debug suffix)
#   Production: Minimal image, no SSH, no debug tools
#   Debug: Includes SSH and debug tools (development/testing only)
#   Save archive: $OUTPUT_DIR/radio-control-<arch>.oci.tar when SAVE_IMAGE=true
#
# Prerequisites:
#   podman, buildah (or podman build)
#   Cross-compilation tools if building for different architecture

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$PROJECT_ROOT/dist}"
CONFIG_SOURCE="${CONFIG_SOURCE:-$PROJECT_ROOT/rcc/config/default.yaml}"
IMAGE_NAME="radio-control"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log() { echo -e "${GREEN}[BUILD]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Parse arguments
DEBUG=false
ARCH=""
for arg in "$@"; do
    case "$arg" in
        --debug=*)
            val="${arg#--debug=}"
            if [[ "$val" == "true" || "$val" == "1" ]]; then
                DEBUG=true
            else
                DEBUG=false
            fi
            ;;
        x86_64|arm64)
            ARCH="$arg"
            ;;
        --help|-h)
            echo "Usage: $0 [x86_64|arm64] [--debug=true|false]"
            echo ""
            echo "Builds OCI-compliant container image for the Radio Control Container."
            echo "  x86_64: For local testing/validation"
            echo "  arm64:  For production deployment (ARM64 hardware)"
            echo ""
            echo "Options:"
            echo "  --debug=true   Include SSH and debug tools (development only)"
            echo "  --debug=false  Minimal production image (default)"
            exit 0
            ;;
        *)
            if [[ -z "$ARCH" ]]; then
                error "Unknown argument: $arg\nUsage: $0 [x86_64|arm64] [--debug=true|false]"
            else
                warn "Unknown argument: $arg (ignored)"
            fi
            ;;
    esac
done

# Default to arm64 if not specified
if [[ -z "$ARCH" ]]; then
    ARCH="arm64"
    log "No architecture specified, defaulting to arm64 (production)"
    log "For local testing on x86_64, use: $0 x86_64"
fi

# Set image tag
if [[ "$DEBUG" == "true" ]]; then
    IMAGE_TAG="${ARCH}-debug"
else
    IMAGE_TAG="${ARCH}"
fi

# Check for podman
if ! command -v podman >/dev/null 2>&1; then
    error "podman not found. Install: apt install podman"
fi

# Check for buildah (optional, podman build works without it)
if ! command -v buildah >/dev/null 2>&1; then
    warn "buildah not found (optional, podman build will be used)"
fi

# Test user configuration for image (used for SSH access during testing)
TEST_USER_SSH_KEY="${TEST_USER_SSH_KEY:-}"

# Architecture mapping
get_arch_vars() {
    local arch="$1"
    case "$arch" in
        x86_64)
            DEBOOTSTRAP_ARCH="amd64"
            BASE_IMAGE="docker.io/ubuntu:24.04"
            PLATFORM="linux/amd64"
            LIBDIR="x86_64-linux-gnu"
            CMAKE_SYSTEM_PROCESSOR="x86_64"
            ;;
        arm64)
            DEBOOTSTRAP_ARCH="arm64"
            BASE_IMAGE="docker.io/arm64v8/ubuntu:24.04"
            PLATFORM="linux/arm64/v8"
            LIBDIR="aarch64-linux-gnu"
            CMAKE_SYSTEM_PROCESSOR="aarch64"
            ;;
        *)
            error "Unsupported architecture: $arch"
            ;;
    esac
}

get_arch_vars "$ARCH"

# Determine binary path
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/rcc/build-${ARCH}}"
BINARY_PATH="$BUILD_DIR/src/radio-control-container"

# Check if binary exists, build if needed
if [[ ! -f "$BINARY_PATH" ]]; then
    log "Binary not found at $BINARY_PATH, building..."
    
    if [[ ! -d "$BUILD_DIR" ]]; then
        log "Creating build directory: $BUILD_DIR"
        mkdir -p "$BUILD_DIR"
    fi
    
    cd "$PROJECT_ROOT/rcc"
    
    # Determine if cross-compilation is needed
    HOST_ARCH="$(uname -m)"
    NEEDS_CROSS=false
    
    case "$HOST_ARCH" in
        x86_64|amd64)
            if [[ "$ARCH" == "arm64" ]]; then
                NEEDS_CROSS=true
            fi
            ;;
        aarch64)
            if [[ "$ARCH" == "x86_64" ]]; then
                NEEDS_CROSS=true
            fi
            ;;
    esac
    
    if [[ "$NEEDS_CROSS" == "true" ]]; then
        log "Cross-compilation required: $HOST_ARCH -> $ARCH"
        
        case "$ARCH" in
            arm64)
                if ! command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
                    error "Cross-compiler not found. Install: apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
                fi
                ;;
            x86_64)
                if ! command -v x86_64-linux-gnu-gcc >/dev/null 2>&1; then
                    error "Cross-compiler not found. Install: apt install gcc-x86-64-linux-gnu g++-x86-64-linux-gnu"
                fi
                ;;
        esac
    fi
    
    cd "$BUILD_DIR"
    
    # Configure CMake
    CMAKE_ARGS=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DRCC_BUILD_TESTS=OFF"
        "-DRCC_ENABLE_LTO=ON"
    )
    
    if [[ "$NEEDS_CROSS" == "true" ]]; then
        case "$ARCH" in
            arm64)
                CMAKE_ARGS+=(
                    "-DCMAKE_SYSTEM_NAME=Linux"
                    "-DCMAKE_SYSTEM_PROCESSOR=aarch64"
                    "-DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc"
                    "-DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++"
                    "-DCMAKE_FIND_ROOT_PATH=/usr/aarch64-linux-gnu"
                    "-DPKG_CONFIG_EXECUTABLE=/usr/bin/aarch64-linux-gnu-pkg-config"
                )
                ;;
            x86_64)
                CMAKE_ARGS+=(
                    "-DCMAKE_SYSTEM_NAME=Linux"
                    "-DCMAKE_SYSTEM_PROCESSOR=x86_64"
                    "-DCMAKE_C_COMPILER=x86_64-linux-gnu-gcc"
                    "-DCMAKE_CXX_COMPILER=x86_64-linux-gnu-g++"
                    "-DCMAKE_FIND_ROOT_PATH=/usr/x86_64-linux-gnu"
                    "-DPKG_CONFIG_EXECUTABLE=/usr/bin/x86_64-linux-gnu-pkg-config"
                )
                ;;
        esac
    else
        CMAKE_ARGS+=(
            "-DCMAKE_C_COMPILER=gcc"
            "-DCMAKE_CXX_COMPILER=g++"
        )
    fi
    
    log "Configuring CMake..."
    cmake "${CMAKE_ARGS[@]}" "$PROJECT_ROOT/rcc" || error "CMake configuration failed"
    
    log "Building binary..."
    cmake --build . --parallel "$(nproc)" || error "Build failed"
    
    if [[ ! -f "$BINARY_PATH" ]]; then
        error "Binary not found after build: $BINARY_PATH"
    fi
    
    log "Binary built successfully: $BINARY_PATH"
else
    log "Using existing binary: $BINARY_PATH"
fi

# Validate binary
log "Validating binary..."
if [[ ! -x "$BINARY_PATH" ]]; then
    error "Binary is not executable: $BINARY_PATH"
fi

# Check binary architecture
if command -v file >/dev/null 2>&1; then
    BINARY_INFO="$(file "$BINARY_PATH")"
    case "$ARCH" in
        x86_64)
            if [[ "$BINARY_INFO" != *"x86-64"* ]] && [[ "$BINARY_INFO" != *"x86_64"* ]] && [[ "$BINARY_INFO" != *"64-bit"* ]]; then
                warn "Binary architecture may not match: expected x86_64, got: $BINARY_INFO"
            fi
            ;;
        arm64)
            if [[ "$BINARY_INFO" != *"aarch64"* ]] && [[ "$BINARY_INFO" != *"ARM aarch64"* ]]; then
                warn "Binary architecture may not match: expected ARM64, got: $BINARY_INFO"
            fi
            ;;
    esac
fi

# Check library dependencies
log "Checking library dependencies..."
if command -v ldd >/dev/null 2>&1; then
    MISSING_LIBS="$(ldd "$BINARY_PATH" 2>/dev/null | grep "not found" || true)"
    if [[ -n "$MISSING_LIBS" ]]; then
        warn "Missing libraries detected:"
        echo "$MISSING_LIBS"
        warn "These libraries must be available in the container runtime"
    else
        log "All library dependencies resolved"
    fi
fi

# Create temporary directory for container build context
TEMP_DIR="$(mktemp -d)"
trap "rm -rf '$TEMP_DIR'" EXIT INT TERM

log "Creating container build context in $TEMP_DIR"

# Copy binary and config
mkdir -p "$TEMP_DIR/app" "$TEMP_DIR/etc/rcc"
cp "$BINARY_PATH" "$TEMP_DIR/app/radio-control-container"
chmod 755 "$TEMP_DIR/app/radio-control-container"

if [[ ! -f "$CONFIG_SOURCE" ]]; then
    warn "Config file not found: $CONFIG_SOURCE"
    warn "Creating minimal config..."
    mkdir -p "$(dirname "$CONFIG_SOURCE")"
    cat > "$CONFIG_SOURCE" << 'EOF'
# Minimal RCC configuration
container:
  id: "rcc-container"
  deployment: "default"
  soldier_id: "operator-001"
network:
  bind_address: "0.0.0.0"
  command_port: 8080
telemetry:
  sse_port: 8081
  heartbeat_interval_sec: 30
  event_buffer_size: 512
  event_retention_hours: 24
  max_sse_clients: 8
  client_idle_timeout_sec: 60
security:
  token_secret: ""
  allow_unauthenticated_dev_access: false
  allowed_roles:
    - viewer
    - controller
  token_ttl_sec: 300
serviceDiscovery:
  enabled: true
  port: 9999
  ttl: 60
  startupBurstCount: 1
  startupBurstSpacingMs: 1000
  bindAddress: "0.0.0.0"
timing:
  normal_probe_sec: 30
  recovering_probe_sec: 10
  offline_probe_sec: 60
radios: []
EOF
fi
cp "$CONFIG_SOURCE" "$TEMP_DIR/etc/rcc/config.yaml"

# Copy dts-common libraries if they exist
BUILD_DIR_FROM_BINARY="$(cd "$(dirname "$(dirname "$BINARY_PATH")")" && pwd)"
log "Looking for dts-common libraries in: $BUILD_DIR_FROM_BINARY"
mkdir -p "$TEMP_DIR/usr/lib/$LIBDIR" "$TEMP_DIR/var/lib/rcc"
touch "$TEMP_DIR/usr/lib/$LIBDIR/.keep" "$TEMP_DIR/var/lib/rcc/.keep"
mapfile -d '' DTS_COMMON_LIBS < <(
    find "$BUILD_DIR_FROM_BINARY" \
        \( -type f -o -type l \) \
        \( -name 'libdts-common*.so*' -o -name 'libdts*.so*' \) \
        -print0 | sort -z
)
LIB_COUNT=0
for lib in "${DTS_COMMON_LIBS[@]}"; do
    log "Copying $(basename "$lib")"
    cp -a "$lib" "$TEMP_DIR/var/lib/rcc/" || warn "Failed to copy $lib"
    cp -a "$lib" "$TEMP_DIR/usr/lib/$LIBDIR/" || warn "Failed to copy $lib"
    LIB_COUNT=$((LIB_COUNT + 1))
done
if [[ $LIB_COUNT -eq 0 ]]; then
    log "No dts-common libraries found (this is OK if statically linked)"
    rm -f "$TEMP_DIR/usr/lib/$LIBDIR/.keep" "$TEMP_DIR/var/lib/rcc/.keep"
fi

# Determine library package names based on architecture
case "$ARCH" in
    x86_64)
        LIBSSL_PKG="libssl3t64"
        LIBYAML_PKG="libyaml-cpp0.8"
        ;;
    arm64)
        LIBSSL_PKG="libssl3t64"
        LIBYAML_PKG="libyaml-cpp0.8"
        ;;
esac

# Create Containerfile
log "Creating Containerfile for $ARCH..."
cat > "$TEMP_DIR/Containerfile" << EOF
FROM $BASE_IMAGE

ARG DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \\
    ca-certificates \\
    curl \\
    $LIBSSL_PKG \\
    $LIBYAML_PKG \\
    zlib1g \\
    systemd \\
    systemd-sysv \\
    networkd-dispatcher \\
    && rm -rf /var/lib/apt/lists/* \\
    && apt-get clean

# Install debug tools if DEBUG_MODE is set
ARG DEBUG_MODE=false
RUN if [ "\$DEBUG_MODE" = "true" ]; then \\
    apt-get update && apt-get install -y --no-install-recommends \\
    openssh-server \\
    net-tools \\
    curl \\
    vim \\
    strace \\
    tcpdump \\
    iproute2 \\
    htop \\
    && rm -rf /var/lib/apt/lists/* \\
    && apt-get clean; \\
    fi

# Create non-root user and group.
RUN groupadd -o -r rcc -g 1000 && \\
    useradd -o -r -u 1000 -g rcc -d /app -s /usr/sbin/nologin rcc

# Create application directories
RUN mkdir -p /app /etc/rcc /var/log/rcc /var/lib/rcc && \\
    chown -R rcc:rcc /app /var/log/rcc /var/lib/rcc /etc/rcc

# Copy application files
COPY app/radio-control-container /app/radio-control-container
COPY etc/rcc/config.yaml /etc/rcc/config.yaml

# Copy dts-common libraries (if they exist)
COPY usr/lib/$LIBDIR/ /usr/lib/$LIBDIR/
COPY var/lib/rcc/ /var/lib/rcc/
RUN rm -f /usr/lib/$LIBDIR/.keep /var/lib/rcc/.keep 2>/dev/null || true

# Set ownership
RUN chown -R rcc:rcc /app /var/log/rcc /var/lib/rcc /etc/rcc && \\
    chmod 755 /app/radio-control-container

# Create systemd service
RUN printf '%s\n' \\
    '[Unit]' \\
    'Description=Radio Control Container' \\
    'After=network-online.target' \\
    'Wants=network-online.target' \\
    '' \\
    '[Service]' \\
    'Type=simple' \\
    'User=rcc' \\
    'Group=rcc' \\
    'WorkingDirectory=/app' \\
    'ExecStart=/app/radio-control-container /etc/rcc/config.yaml' \\
    'Restart=always' \\
    'RestartSec=5' \\
    'StandardOutput=journal' \\
    'StandardError=journal' \\
    'SyslogIdentifier=rcc' \\
    '' \\
    '# Security settings' \\
    'NoNewPrivileges=true' \\
    'PrivateTmp=true' \\
    'ProtectSystem=strict' \\
    'ProtectHome=true' \\
    'ReadWritePaths=/var/log/rcc /var/lib/rcc' \\
    '' \\
    '[Install]' \\
    'WantedBy=multi-user.target' \\
    > /etc/systemd/system/radio-control-container.service

# Enable service
RUN systemctl enable radio-control-container.service

# Configure network for DHCP (systemd-networkd)
RUN mkdir -p /etc/systemd/network && \\
    printf '%s\n' \\
    '[Match]' \\
    'Name=eth0' \\
    '' \\
    '[Network]' \\
    'DHCP=yes' \\
    > /etc/systemd/network/10-eth0.network

# Enable systemd-networkd
RUN systemctl enable systemd-networkd.service

# Create debug SSH root login for debug mode
ARG DEBUG_MODE=false
RUN if [ "\$DEBUG_MODE" = "true" ]; then \\
    sed -i 's/^#\\?PermitRootLogin .*/PermitRootLogin yes/' /etc/ssh/sshd_config && \\
    sed -i 's/^#\\?PasswordAuthentication .*/PasswordAuthentication yes/' /etc/ssh/sshd_config && \\
    sed -i 's/^#\\?UsePAM .*/UsePAM no/' /etc/ssh/sshd_config && \\
    echo 'root:debug123' | chpasswd; \\
    fi

# Copy SSH key if provided (for debug mode)
ARG TEST_USER_SSH_KEY=""
RUN if [ "\$DEBUG_MODE" = "true" ] && [ -n "\$TEST_USER_SSH_KEY" ]; then \\
    mkdir -p /root/.ssh && \\
    echo "\$TEST_USER_SSH_KEY" > /root/.ssh/authorized_keys && \\
    chmod 600 /root/.ssh/authorized_keys; \\
    fi

# Enable SSH in debug mode
RUN if [ "\$DEBUG_MODE" = "true" ]; then \\
    systemctl enable ssh.service; \\
    fi

# Expose service ports
EXPOSE 8080 8081 9999/udp 22

HEALTHCHECK --interval=30s --timeout=10s --start-period=30s --retries=3 \
    CMD curl -fsS http://127.0.0.1:8080/api/v1/health || exit 1

# Set systemd as init
CMD ["/usr/lib/systemd/systemd"]
EOF

# Build container image
FULL_IMAGE_NAME="${IMAGE_NAME}:${IMAGE_TAG}"
log "Building container image: $FULL_IMAGE_NAME (platform: $PLATFORM)"

BUILDAH_ARGS=(
    "build"
    "-f" "$TEMP_DIR/Containerfile"
    "-t" "$FULL_IMAGE_NAME"
    "--platform" "$PLATFORM"
    "--build-arg" "DEBUG_MODE=$DEBUG"
)

if [[ -n "${TEST_USER_SSH_KEY}" ]]; then
    BUILDAH_ARGS+=("--build-arg" "TEST_USER_SSH_KEY=${TEST_USER_SSH_KEY}")
fi

BUILDAH_ARGS+=("$TEMP_DIR")

if command -v buildah >/dev/null 2>&1; then
    buildah "${BUILDAH_ARGS[@]}" || error "Container build failed"
else
    podman build "${BUILDAH_ARGS[@]}" || error "Container build failed"
fi

log "Container image built successfully: $FULL_IMAGE_NAME"

# Validate image
log "Validating image..."
if ! podman image exists "$FULL_IMAGE_NAME" 2>/dev/null; then
    error "Image validation failed: $FULL_IMAGE_NAME not found"
fi

IMAGE_SIZE="$(podman images --format "{{.Size}}" "$FULL_IMAGE_NAME" 2>/dev/null || echo "unknown")"
log "Image size: $IMAGE_SIZE"

# Optionally save image to file
SAVE_IMAGE="${SAVE_IMAGE:-false}"
if [[ "$SAVE_IMAGE" == "true" ]]; then
    OUTPUT_FILE="$OUTPUT_DIR/radio-control-${ARCH}"
    [[ "$DEBUG" == "true" ]] && OUTPUT_FILE="${OUTPUT_FILE}-debug"
    OUTPUT_FILE="${OUTPUT_FILE}.oci.tar"

    log "Saving OCI archive to: $OUTPUT_FILE"
    mkdir -p "$OUTPUT_DIR"
    podman save --format oci-archive -o "$OUTPUT_FILE" "$FULL_IMAGE_NAME" || error "Failed to save OCI archive"
    chown "$(id -u):$(id -g)" "$OUTPUT_FILE" 2>/dev/null || true
    log "OCI archive saved: $OUTPUT_FILE ($(du -h "$OUTPUT_FILE" | cut -f1))"
fi

log "Image: $FULL_IMAGE_NAME"
log "Architecture: $ARCH"
log "Debug mode: $DEBUG"
log ""
log "Next steps:"
if [[ "$ARCH" == "x86_64" ]]; then
    log "  Local testing:"
    log "    ./run-local.sh x86_64"
else
    log "  Production deployment:"
    log "    podman save -o radio-control-${ARCH}.tar $FULL_IMAGE_NAME"
    log "    # Transfer to ARM64 host and load:"
    log "    podman load -i radio-control-${ARCH}.tar"
fi
