#!/bin/bash

# Silvus Radio Mock Emulator - Service Management Script
# Provides convenient commands for managing the service

set -euo pipefail

# Configuration
SERVICE_NAME="silvus-mock"
BINARY_NAME="silvusmock"
CONFIG_DIR="/etc/silvus-mock"
LOG_DIR="/var/log/silvus-mock"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

log_header() {
    echo -e "${BLUE}=== $1 ===${NC}"
}

# Show usage
show_usage() {
    echo "Silvus Radio Mock Emulator - Service Manager"
    echo ""
    echo "Usage: $0 <command>"
    echo ""
    echo "Commands:"
    echo "  start       Start the service"
    echo "  stop        Stop the service"
    echo "  restart     Restart the service"
    echo "  status      Show service status"
    echo "  logs        Show service logs (follow mode)"
    echo "  logs-last   Show last 50 log lines"
    echo "  test        Test service endpoints"
    echo "  config      Show configuration info"
    echo "  reload      Reload configuration"
    echo "  enable      Enable service at boot"
    echo "  disable     Disable service at boot"
    echo ""
    echo "Examples:"
    echo "  $0 start"
    echo "  $0 logs"
    echo "  $0 test"
}

# Check if running as root for system commands
check_root_for_system_commands() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This command requires root privileges (use sudo)"
        exit 1
    fi
}

# Start service
start_service() {
    log_header "Starting Service"
    check_root_for_system_commands
    
    if systemctl is-active --quiet "${SERVICE_NAME}"; then
        log_warn "Service is already running"
    else
        systemctl start "${SERVICE_NAME}"
        sleep 2
        
        if systemctl is-active --quiet "${SERVICE_NAME}"; then
            log_info "Service started successfully"
        else
            log_error "Service failed to start"
            log_error "Check logs with: $0 logs"
            exit 1
        fi
    fi
}

# Stop service
stop_service() {
    log_header "Stopping Service"
    check_root_for_system_commands
    
    if systemctl is-active --quiet "${SERVICE_NAME}"; then
        systemctl stop "${SERVICE_NAME}"
        log_info "Service stopped"
    else
        log_info "Service is not running"
    fi
}

# Restart service
restart_service() {
    log_header "Restarting Service"
    check_root_for_system_commands
    
    systemctl restart "${SERVICE_NAME}"
    sleep 2
    
    if systemctl is-active --quiet "${SERVICE_NAME}"; then
        log_info "Service restarted successfully"
    else
        log_error "Service failed to restart"
        log_error "Check logs with: $0 logs"
        exit 1
    fi
}

# Show service status
show_status() {
    log_header "Service Status"
    
    systemctl status "${SERVICE_NAME}" --no-pager
    
    echo ""
    log_info "Listening ports:"
    netstat -tlnp 2>/dev/null | grep "${BINARY_NAME}" || echo "  No ports found"
    
    echo ""
    log_info "Process information:"
    ps aux | grep "${BINARY_NAME}" | grep -v grep || echo "  Process not found"
}

# Show service logs
show_logs() {
    log_header "Service Logs (follow mode - Ctrl+C to exit)"
    
    if [[ $EUID -eq 0 ]]; then
        journalctl -u "${SERVICE_NAME}" -f
    else
        sudo journalctl -u "${SERVICE_NAME}" -f
    fi
}

# Show last logs
show_logs_last() {
    log_header "Last 50 Log Lines"
    
    if [[ $EUID -eq 0 ]]; then
        journalctl -u "${SERVICE_NAME}" -n 50 --no-pager
    else
        sudo journalctl -u "${SERVICE_NAME}" -n 50 --no-pager
    fi
}

# Test service endpoints
test_service() {
    log_header "Testing Service Endpoints"
    
    # Test HTTP endpoint
    log_info "Testing HTTP endpoint (port 80)..."
    if curl -s -f -m 5 "http://localhost:80/streamscape_api" > /dev/null 2>&1; then
        log_info "✓ HTTP endpoint is responding"
    else
        log_warn "✗ HTTP endpoint test failed"
    fi
    
    # Test maintenance endpoint
    log_info "Testing maintenance endpoint (port 50000)..."
    if timeout 5 bash -c 'echo "{\"jsonrpc\":\"2.0\",\"method\":\"freq\",\"id\":\"test\"}" | nc localhost 50000' > /dev/null 2>&1; then
        log_info "✓ Maintenance endpoint is responding"
    else
        log_warn "✗ Maintenance endpoint test failed"
    fi
    
    # Test specific JSON-RPC command
    log_info "Testing JSON-RPC command..."
    response=$(curl -s -X POST -H "Content-Type: application/json" \
        -d '{"jsonrpc":"2.0","method":"freq","id":"test"}' \
        http://localhost:80/streamscape_api 2>/dev/null || echo "FAILED")
    
    if [[ "$response" != "FAILED" ]]; then
        log_info "✓ JSON-RPC command successful"
        echo "  Response: $response"
    else
        log_warn "✗ JSON-RPC command failed"
    fi
}

# Show configuration info
show_config() {
    log_header "Configuration Information"
    
    echo "Service configuration:"
    echo "  Service name: ${SERVICE_NAME}"
    echo "  Binary: /usr/local/bin/${BINARY_NAME}"
    echo "  Config dir: ${CONFIG_DIR}"
    echo "  Log dir: ${LOG_DIR}"
    echo ""
    
    echo "Configuration files:"
    if [[ -f "${CONFIG_DIR}/default.yaml" ]]; then
        echo "  ✓ default.yaml"
    else
        echo "  ✗ default.yaml (not found)"
    fi
    
    if [[ -f "${CONFIG_DIR}/env" ]]; then
        echo "  ✓ env"
    else
        echo "  ✗ env (not found)"
    fi
    
    echo ""
    echo "Environment variables:"
    if [[ -f "${CONFIG_DIR}/env" ]]; then
        grep -v "^#" "${CONFIG_DIR}/env" | grep -v "^$" || echo "  (no custom environment variables set)"
    else
        echo "  (env file not found)"
    fi
}

# Reload configuration
reload_config() {
    log_header "Reloading Configuration"
    check_root_for_system_commands
    
    if systemctl is-active --quiet "${SERVICE_NAME}"; then
        log_info "Sending reload signal to service"
        systemctl reload "${SERVICE_NAME}" 2>/dev/null || {
            log_warn "Reload signal not supported, restarting service"
            restart_service
        }
    else
        log_warn "Service is not running, nothing to reload"
    fi
}

# Enable service
enable_service() {
    log_header "Enabling Service"
    check_root_for_system_commands
    
    systemctl enable "${SERVICE_NAME}"
    log_info "Service enabled at boot"
}

# Disable service
disable_service() {
    log_header "Disabling Service"
    check_root_for_system_commands
    
    systemctl disable "${SERVICE_NAME}"
    log_info "Service disabled at boot"
}

# Main function
main() {
    case "${1:-}" in
        start)
            start_service
            ;;
        stop)
            stop_service
            ;;
        restart)
            restart_service
            ;;
        status)
            show_status
            ;;
        logs)
            show_logs
            ;;
        logs-last)
            show_logs_last
            ;;
        test)
            test_service
            ;;
        config)
            show_config
            ;;
        reload)
            reload_config
            ;;
        enable)
            enable_service
            ;;
        disable)
            disable_service
            ;;
        *)
            show_usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
