#!/bin/bash

# Silvus Radio Mock Emulator - Uninstallation Script
# Removes binary, configuration, and systemd service

set -euo pipefail

# Configuration
BINARY_NAME="silvusmock"
BINARY_PATH="/usr/local/bin/${BINARY_NAME}"
SERVICE_NAME="silvus-mock"
SERVICE_USER="silvus-mock"
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

# Stop and disable service
stop_service() {
    log_info "Stopping and disabling service"
    
    if systemctl is-active --quiet "${SERVICE_NAME}"; then
        systemctl stop "${SERVICE_NAME}"
        log_info "Service stopped"
    else
        log_info "Service was not running"
    fi
    
    if systemctl is-enabled --quiet "${SERVICE_NAME}"; then
        systemctl disable "${SERVICE_NAME}"
        log_info "Service disabled"
    else
        log_info "Service was not enabled"
    fi
}

# Remove systemd service
remove_systemd_service() {
    log_info "Removing systemd service"
    
    if [[ -f "${SYSTEMD_DIR}/${SERVICE_NAME}.service" ]]; then
        rm -f "${SYSTEMD_DIR}/${SERVICE_NAME}.service"
        systemctl daemon-reload
        log_info "systemd service removed"
    else
        log_info "systemd service not found"
    fi
}

# Remove binary
remove_binary() {
    log_info "Removing binary"
    
    if [[ -f "${BINARY_PATH}" ]]; then
        # Remove capabilities before removing binary
        setcap -r "${BINARY_PATH}" 2>/dev/null || true
        rm -f "${BINARY_PATH}"
        log_info "Binary removed"
    else
        log_info "Binary not found"
    fi
}

# Remove configuration
remove_config() {
    log_info "Removing configuration"
    
    if [[ -d "${CONFIG_DIR}" ]]; then
        rm -rf "${CONFIG_DIR}"
        log_info "Configuration removed"
    else
        log_info "Configuration not found"
    fi
}

# Remove log directory
remove_logs() {
    log_info "Removing log directory"
    
    if [[ -d "${LOG_DIR}" ]]; then
        rm -rf "${LOG_DIR}"
        log_info "Log directory removed"
    else
        log_info "Log directory not found"
    fi
}

# Remove data directory
remove_data() {
    log_info "Removing data directory"
    
    if [[ -d "${DATA_DIR}" ]]; then
        rm -rf "${DATA_DIR}"
        log_info "Data directory removed"
    else
        log_info "Data directory not found"
    fi
}

# Remove service user
remove_service_user() {
    log_info "Removing service user"
    
    if id "${SERVICE_USER}" &>/dev/null; then
        userdel "${SERVICE_USER}" 2>/dev/null || true
        log_info "Service user removed"
    else
        log_info "Service user not found"
    fi
}

# Clean up systemd state
cleanup_systemd() {
    log_info "Cleaning up systemd state"
    
    # Remove any failed service state
    systemctl reset-failed "${SERVICE_NAME}" 2>/dev/null || true
    
    # Clean up any lingering systemd state
    rm -f "/var/lib/systemd/linger/${SERVICE_USER}" 2>/dev/null || true
}

# Show removal summary
show_summary() {
    log_info "Removal completed!"
    log_info ""
    log_info "Removed components:"
    log_info "  ✓ systemd service: ${SERVICE_NAME}"
    log_info "  ✓ binary: ${BINARY_PATH}"
    log_info "  ✓ configuration: ${CONFIG_DIR}"
    log_info "  ✓ logs: ${LOG_DIR}"
    log_info "  ✓ data: ${DATA_DIR}"
    log_info "  ✓ service user: ${SERVICE_USER}"
    log_info ""
    log_info "Note: Any custom configurations or log files have been permanently removed."
}

# Confirmation prompt
confirm_removal() {
    echo "This will completely remove the Silvus Radio Mock Emulator and all its data."
    echo ""
    echo "The following will be removed:"
    echo "  - systemd service: ${SERVICE_NAME}"
    echo "  - binary: ${BINARY_PATH}"
    echo "  - configuration: ${CONFIG_DIR}"
    echo "  - logs: ${LOG_DIR}"
    echo "  - data: ${DATA_DIR}"
    echo "  - service user: ${SERVICE_USER}"
    echo ""
    read -p "Are you sure you want to continue? (yes/no): " -r
    
    if [[ ! $REPLY =~ ^[Yy][Ee][Ss]$ ]]; then
        log_info "Removal cancelled"
        exit 0
    fi
}

# Main removal function
main() {
    log_info "Silvus Radio Mock Emulator Uninstaller"
    
    check_root
    confirm_removal
    
    stop_service
    remove_systemd_service
    remove_binary
    remove_config
    remove_logs
    remove_data
    remove_service_user
    cleanup_systemd
    show_summary
}

# Run main function
main "$@"
