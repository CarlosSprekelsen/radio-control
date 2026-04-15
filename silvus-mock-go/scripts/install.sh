#!/bin/bash

# Silvus Radio Mock Emulator - Installation Script
# Installs binary, configuration, and systemd service

set -euo pipefail

# Configuration
BINARY_NAME="silvusmock"
BINARY_PATH="/usr/local/bin/${BINARY_NAME}"
SERVICE_NAME="silvus-mock"
SERVICE_USER="silvus-mock"
SERVICE_GROUP="silvus-mock"
CONFIG_DIR="/etc/silvus-mock"
LOG_DIR="/var/log/silvus-mock"
DATA_DIR="/var/lib/silvus-mock"
SYSTEMD_DIR="/etc/systemd/system"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Check if binary exists
check_binary() {
    if [[ ! -f "./${BINARY_NAME}" ]]; then
        log_error "Binary ${BINARY_NAME} not found in current directory"
        log_error "Please run 'make build' first"
        exit 1
    fi
}

# Create service user
create_service_user() {
    if ! id "${SERVICE_USER}" &>/dev/null; then
        log_info "Creating service user: ${SERVICE_USER}"
        useradd --system --no-create-home --shell /bin/false "${SERVICE_USER}"
    else
        log_info "Service user ${SERVICE_USER} already exists"
    fi
}

# Install binary
install_binary() {
    log_info "Installing binary to ${BINARY_PATH}"
    cp "./${BINARY_NAME}" "${BINARY_PATH}"
    chmod 755 "${BINARY_PATH}"
    chown root:root "${BINARY_PATH}"
}

# Create directories
create_directories() {
    log_info "Creating directories"
    
    mkdir -p "${CONFIG_DIR}"
    mkdir -p "${LOG_DIR}"
    mkdir -p "${DATA_DIR}"
    
    chown "${SERVICE_USER}:${SERVICE_GROUP}" "${LOG_DIR}"
    chown "${SERVICE_USER}:${SERVICE_GROUP}" "${DATA_DIR}"
    chmod 755 "${LOG_DIR}"
    chmod 755 "${DATA_DIR}"
}

# Install configuration
install_config() {
    log_info "Installing configuration files"
    
    # Copy default configuration
    if [[ -f "config/default.yaml" ]]; then
        cp "config/default.yaml" "${CONFIG_DIR}/default.yaml"
        chmod 644 "${CONFIG_DIR}/default.yaml"
        chown root:root "${CONFIG_DIR}/default.yaml"
    fi
    
    # Copy CB-TIMING example
    if [[ -f "config/cb-timing.example.yaml" ]]; then
        cp "config/cb-timing.example.yaml" "${CONFIG_DIR}/cb-timing.example.yaml"
        chmod 644 "${CONFIG_DIR}/cb-timing.example.yaml"
        chown root:root "${CONFIG_DIR}/cb-timing.example.yaml"
    fi
    
    # Create environment file
    cat > "${CONFIG_DIR}/env" << EOF
# Silvus Mock Environment Configuration
# Override default.yaml settings here

# Operating mode: normal, degraded, offline
# SILVUS_MOCK_MODE=normal

# HTTP settings
# SILVUS_MOCK_HTTP_DEV_MODE=false
# SILVUS_MOCK_HTTP_PORT=80

# Timing settings (seconds)
# SILVUS_MOCK_SOFT_BOOT_TIME=30
# SILVUS_MOCK_POWER_CHANGE_TIME=5
# SILVUS_MOCK_RADIO_RESET_TIME=60

# Logging
# SILVUS_MOCK_LOG_LEVEL=info
# SILVUS_MOCK_DEBUG=false
EOF
    
    chmod 600 "${CONFIG_DIR}/env"
    chown root:root "${CONFIG_DIR}/env"
}

# Install systemd service
install_systemd_service() {
    log_info "Installing systemd service"
    
    if [[ -f "packaging/systemd/silvus-mock.service" ]]; then
        cp "packaging/systemd/silvus-mock.service" "${SYSTEMD_DIR}/"
        chmod 644 "${SYSTEMD_DIR}/silvus-mock.service"
        chown root:root "${SYSTEMD_DIR}/silvus-mock.service"
    else
        log_error "systemd service file not found"
        exit 1
    fi
    
    # Reload systemd
    systemctl daemon-reload
}

# Set capabilities for port 80 binding
set_capabilities() {
    log_info "Setting capabilities for port 80 binding"
    setcap 'cap_net_bind_service=+ep' "${BINARY_PATH}"
    
    # Verify capabilities
    if getcap "${BINARY_PATH}" | grep -q "cap_net_bind_service"; then
        log_info "Capabilities set successfully"
    else
        log_warn "Failed to set capabilities - you may need to run as root for port 80"
    fi
}

# Enable and start service
enable_service() {
    log_info "Enabling and starting service"
    
    systemctl enable "${SERVICE_NAME}"
    
    if systemctl is-active --quiet "${SERVICE_NAME}"; then
        log_info "Stopping existing service"
        systemctl stop "${SERVICE_NAME}"
    fi
    
    systemctl start "${SERVICE_NAME}"
    
    # Wait a moment for startup
    sleep 2
    
    if systemctl is-active --quiet "${SERVICE_NAME}"; then
        log_info "Service started successfully"
    else
        log_error "Service failed to start"
        log_error "Check logs with: journalctl -u ${SERVICE_NAME} -f"
        exit 1
    fi
}

# Show status
show_status() {
    log_info "Service status:"
    systemctl status "${SERVICE_NAME}" --no-pager
    
    log_info "Service is listening on:"
    netstat -tlnp | grep "${BINARY_NAME}" || true
    
    log_info "Configuration files:"
    echo "  Binary: ${BINARY_PATH}"
    echo "  Config: ${CONFIG_DIR}/"
    echo "  Logs: ${LOG_DIR}/"
    echo "  Data: ${DATA_DIR}/"
    echo "  Service: ${SYSTEMD_DIR}/${SERVICE_NAME}.service"
}

# Test installation
test_installation() {
    log_info "Testing installation..."
    
    # Test HTTP endpoint
    if curl -s -f "http://localhost:80/streamscape_api" > /dev/null 2>&1; then
        log_info "HTTP endpoint is responding"
    else
        log_warn "HTTP endpoint test failed (this may be normal during startup)"
    fi
    
    # Test maintenance endpoint
    if timeout 5 bash -c 'echo "{\"jsonrpc\":\"2.0\",\"method\":\"zeroize\",\"id\":\"test\"}" | nc localhost 50000' > /dev/null 2>&1; then
        log_info "Maintenance endpoint is responding"
    else
        log_warn "Maintenance endpoint test failed (this may be normal during startup)"
    fi
}

# Main installation function
main() {
    log_info "Installing Silvus Radio Mock Emulator"
    
    check_root
    check_binary
    create_service_user
    install_binary
    create_directories
    install_config
    install_systemd_service
    set_capabilities
    enable_service
    show_status
    test_installation
    
    log_info "Installation completed successfully!"
    log_info ""
    log_info "Useful commands:"
    log_info "  Service status: sudo systemctl status ${SERVICE_NAME}"
    log_info "  View logs: sudo journalctl -u ${SERVICE_NAME} -f"
    log_info "  Stop service: sudo systemctl stop ${SERVICE_NAME}"
    log_info "  Start service: sudo systemctl start ${SERVICE_NAME}"
    log_info "  Restart service: sudo systemctl restart ${SERVICE_NAME}"
    log_info ""
    log_info "Configuration:"
    log_info "  Edit config: sudo nano ${CONFIG_DIR}/env"
    log_info "  Edit defaults: sudo nano ${CONFIG_DIR}/default.yaml"
    log_info ""
    log_info "Testing:"
    log_info "  HTTP test: curl -X POST -H 'Content-Type: application/json' \\"
    log_info "    -d '{\"jsonrpc\":\"2.0\",\"method\":\"freq\",\"id\":\"test\"}' \\"
    log_info "    http://localhost:80/streamscape_api"
}

# Run main function
main "$@"
