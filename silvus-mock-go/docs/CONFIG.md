# Configuration Guide

This document describes all configuration options available for the Silvus Radio Mock emulator.

## Configuration Sources

Configuration is loaded in the following order (later sources override earlier ones):

1. **Default values** (hardcoded in application)
2. **YAML configuration files** (`config/default.yaml`, then `CBTIMING_CONFIG` if specified)
3. **Environment variables** (override YAML values)

## YAML Configuration

### Network Settings

```yaml
network:
  http:
    port: 80                    # HTTP server port (production: 80, dev: 8080)
    serverHeader: ""            # HTTP Server header (empty = suppressed)
    devMode: false              # Development mode (uses port 8080)
  maintenance:
    port: 50000                 # TCP maintenance server port
    allowedCidrs:               # CIDR blocks allowed to connect
      - "127.0.0.0/8"          # Localhost
      - "172.20.0.0/16"        # Radio network
```

### Frequency Profiles

```yaml
profiles:
  frequencyProfiles:
    - frequencies:
        - "2200:20:2380"       # Range format: start:step:end (MHz)
        - "4700"               # Single frequency (MHz)
      bandwidth: "-1"          # Bandwidth (-1 = all supported)
      antenna_mask: "15"       # Hex bitmask (15 = all antennas)
    - frequencies:
        - "4420:40:4700"       # Another frequency range
      bandwidth: "-1"
      antenna_mask: "3"
```

**Frequency Format Rules:**
- **Range**: `"<start_mhz>:<step_mhz>:<end_mhz>"` (e.g., `"2200:20:2380"`)
- **Single**: `"<frequency_mhz>"` (e.g., `"4700"`)
- **Units**: All frequencies in MHz
- **Validation**: Must match one of the configured profiles

### Power Configuration

```yaml
power:
  minDbm: 0                    # Minimum power (dBm)
  maxDbm: 39                   # Maximum power (dBm)
```

**Power Rules:**
- Range: 0-39 dBm (ICD §6.1.3)
- Accuracy: Typically 10-39 dBm
- Default: 30 dBm

### Timing Configuration (CB-TIMING v0.3)

```yaml
timing:
  blackout:
    softBootSec: 30            # Channel change blackout (CB-TIMING §6.2)
    powerChangeSec: 5          # Power change blackout (CB-TIMING §6.2)
    radioResetSec: 60          # Radio reset blackout (CB-TIMING §6.2)
  commands:
    setPower:
      timeoutSec: 10           # setPower timeout (CB-TIMING §5)
    setChannel:
      timeoutSec: 30           # setChannel timeout (CB-TIMING §5)
    selectRadio:
      timeoutSec: 5            # selectRadio timeout (CB-TIMING §5)
    read:
      timeoutSec: 5            # read timeout (CB-TIMING §5)
  backoff:
    busyBaseMs: 1000           # Base backoff delay for BUSY errors
```

**Timing Rules:**
- All blackout durations in seconds
- All command timeouts in seconds
- Must comply with CB-TIMING v0.3 specifications
- Read commands return `UNAVAILABLE` during blackout (ICD compliance)

### Operating Mode

```yaml
mode: "normal"                 # normal, degraded, offline
```

**Mode Descriptions:**
- `normal`: Full functionality
- `degraded`: Limited functionality (for testing)
- `offline`: Simulates radio offline state

## Environment Variables

Environment variables override YAML configuration values:

### Basic Configuration

```bash
# Operating mode
export SILVUS_MOCK_MODE=normal

# HTTP settings
export SILVUS_MOCK_HTTP_PORT=80
export SILVUS_MOCK_HTTP_SERVER_HEADER=""
export SILVUS_MOCK_HTTP_DEV_MODE=false

# Maintenance settings
export SILVUS_MOCK_MAINTENANCE_PORT=50000
export SILVUS_MOCK_MAINTENANCE_ALLOWED_CIDRS="127.0.0.0/8,172.20.0.0/16"

# Power limits
export SILVUS_MOCK_POWER_MIN_DBM=0
export SILVUS_MOCK_POWER_MAX_DBM=39
```

### Timing Configuration

```bash
# Blackout durations
export SILVUS_MOCK_SOFT_BOOT_TIME=30
export SILVUS_MOCK_POWER_CHANGE_TIME=5
export SILVUS_MOCK_RADIO_RESET_TIME=60

# Command timeouts
export SILVUS_MOCK_SET_POWER_TIMEOUT=10
export SILVUS_MOCK_SET_CHANNEL_TIMEOUT=30
export SILVUS_MOCK_SELECT_RADIO_TIMEOUT=5
export SILVUS_MOCK_READ_TIMEOUT=5

# Backoff settings
export SILVUS_MOCK_BUSY_BACKOFF_MS=1000
```

### Advanced Configuration

```bash
# Custom timing configuration file
export CBTIMING_CONFIG=/path/to/custom-timing.yaml

# Logging
export SILVUS_MOCK_LOG_LEVEL=info
export SILVUS_MOCK_LOG_FILE=/var/log/silvus-mock.log

# Debug mode
export SILVUS_MOCK_DEBUG=true
```

## Configuration Files

### Default Configuration (`config/default.yaml`)

Production-ready configuration with ICD-compliant defaults:

```yaml
# Silvus Radio Mock Configuration
# Based on ICD and CB-TIMING specifications

network:
  http:
    port: 80
    serverHeader: ""
    devMode: false
  maintenance:
    port: 50000
    allowedCidrs:
      - "127.0.0.0/8"
      - "172.20.0.0/16"

profiles:
  frequencyProfiles:
    - frequencies: ["2200:20:2380", "4700"]
      bandwidth: "-1"
      antenna_mask: "15"
    - frequencies: ["4420:40:4700"]
      bandwidth: "-1"
      antenna_mask: "3"
    - frequencies: ["4700:20:4980"]
      bandwidth: "-1"
      antenna_mask: "12"

power:
  minDbm: 0
  maxDbm: 39

timing:
  blackout:
    softBootSec: 30
    powerChangeSec: 5
    radioResetSec: 60
  commands:
    setPower:
      timeoutSec: 10
    setChannel:
      timeoutSec: 30
    selectRadio:
      timeoutSec: 5
    read:
      timeoutSec: 5
  backoff:
    busyBaseMs: 1000

mode: "normal"
```

### Development Configuration (`config/dev.yaml`)

Development-specific overrides:

```yaml
# Development Configuration
# Overrides default.yaml for development use

network:
  http:
    devMode: true  # Use port 8080 for development
```

### Custom Timing Configuration (`config/cb-timing.example.yaml`)

Example CB-TIMING v0.3 compliant configuration:

```yaml
# CB-TIMING v0.3 Configuration Example
# Reference: CB-TIMING v0.3 Provisional - Edge Power

timing:
  # Telemetry & Heartbeat Parameters (§3)
  heartbeat:
    intervalSec: 15
    jitterSec: 2
    timeoutSec: 45
  
  # Health Probe Parameters (§4)
  probes:
    normal:
      intervalSec: 30
      backoffFactor: 1.0
      maxIntervalSec: 30
      jitterSec: 5
    recovering:
      intervalSec: 5
      backoffFactor: 1.5
      maxIntervalSec: 15
      jitterSec: 2
    offline:
      intervalSec: 10
      backoffFactor: 2.0
      maxIntervalSec: 300
      jitterSec: 10
  
  # Command Timeout Classes (§5)
  commands:
    setPower:
      timeoutSec: 10
      retryCount: 3
      backoffBaseMs: 1000
    setChannel:
      timeoutSec: 30
      retryCount: 2
      backoffBaseMs: 2000
    selectRadio:
      timeoutSec: 5
      retryCount: 3
      backoffBaseMs: 500
    getState:
      timeoutSec: 5
      retryCount: 2
      backoffBaseMs: 1000
  
  # Event Replay & Buffering (§6)
  buffering:
    bufferSize: 50
    retentionHours: 1
    eventIdScheme: "monotonic"
    resumeSupport: true
  
  # Blackout Periods (§6.2)
  blackout:
    channelChangeSec: 30
    powerChangeSec: 5
    radioResetSec: 60
  
  # Backoff & Retry Policies (§8)
  backoff:
    standard:
      baseDelayMs: 500
      jitterRangeMs: 250
      maxRetries: 5
      exponentialFactor: 2.0
      maxDelaySec: 30
    errorSpecific:
      busy:
        initialDelayMs: 1000
        maxRetries: 3
        backoffFactor: 1.5
      unavailable:
        initialDelayMs: 2000
        maxRetries: 5
        backoffFactor: 2.0
      internal:
        initialDelayMs: 500
        maxRetries: 3
        backoffFactor: 2.0
```

## Validation Rules

### Configuration Validation

The emulator validates configuration on startup:

1. **Port ranges**: HTTP port 1-65535, Maintenance port 1-65535
2. **Power limits**: 0 ≤ minDbm ≤ maxDbm ≤ 50
3. **Timing values**: All timeouts and blackouts must be positive integers
4. **Frequency profiles**: At least one profile must be configured
5. **CIDR blocks**: Must be valid CIDR notation
6. **Operating mode**: Must be one of: normal, degraded, offline

### Validation Errors

Common validation errors and fixes:

```bash
# Error: Invalid power range
# Fix: Ensure minDbm ≤ maxDbm
power:
  minDbm: 10
  maxDbm: 5  # ERROR: min > max

# Error: No frequency profiles
# Fix: Add at least one frequency profile
profiles:
  frequencyProfiles: []  # ERROR: empty array

# Error: Invalid CIDR
# Fix: Use proper CIDR notation
network:
  maintenance:
    allowedCidrs:
      - "invalid-cidr"  # ERROR: not valid CIDR
```

## Configuration Examples

### Production Configuration

```yaml
# Production deployment
network:
  http:
    port: 80
    devMode: false
    serverHeader: ""  # Suppress server identification

timing:
  blackout:
    softBootSec: 30    # CB-TIMING compliant
    powerChangeSec: 5
    radioResetSec: 60

mode: "normal"
```

### Testing Configuration

```yaml
# Testing with reduced blackouts
timing:
  blackout:
    softBootSec: 5     # Faster testing
    powerChangeSec: 1
    radioResetSec: 10

mode: "degraded"       # Simulate degraded state
```

### High-Latency Network Configuration

```yaml
# For high-latency networks (CB-TIMING §12.1)
timing:
  commands:
    setPower:
      timeoutSec: 20   # 2x normal timeout
    setChannel:
      timeoutSec: 60   # 2x normal timeout
  backoff:
    busyBaseMs: 2000   # 2x normal backoff
```

### Low-Power Mode Configuration

```yaml
# For low-power mode (CB-TIMING §12.1)
timing:
  commands:
    setPower:
      timeoutSec: 30   # 3x normal timeout
    setChannel:
      timeoutSec: 90   # 3x normal timeout
  backoff:
    busyBaseMs: 3000   # 3x normal backoff
```

## Troubleshooting Configuration

### Debug Configuration Loading

```bash
# Enable debug mode to see configuration loading
export SILVUS_MOCK_DEBUG=true
./silvusmock -config /path/to/config.yaml
```

### Validate Configuration

```bash
# Test configuration without starting server
./silvusmock -config /path/to/config.yaml -validate-only
```

### Configuration Override Testing

```bash
# Test environment variable overrides
export SILVUS_MOCK_MODE=degraded
export SILVUS_MOCK_SOFT_BOOT_TIME=10
./silvusmock -config config/default.yaml
```

## Best Practices

1. **Use environment variables** for deployment-specific overrides
2. **Keep YAML files** in version control with sensible defaults
3. **Validate configuration** before deploying to production
4. **Use separate config files** for different environments (dev, test, prod)
5. **Document custom configurations** for team members
6. **Test configuration changes** in development environment first

## Security Considerations

1. **CIDR restrictions**: Limit maintenance port access to trusted networks
2. **Server headers**: Suppress server identification in production
3. **Port binding**: Use non-privileged ports when possible
4. **Configuration files**: Secure file permissions (600) for sensitive configs
5. **Environment variables**: Avoid logging sensitive environment variables
