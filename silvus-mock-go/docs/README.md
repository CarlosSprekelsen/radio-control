# Silvus Radio Mock Emulator

A drop-in Silvus radio emulator that provides a black-box compatible interface for the Radio Control Container (RCC) system. This emulator implements the complete ICD (Interface Control Document) specification and is indistinguishable from a real Silvus radio device.

## Features

- **Full ICD Compliance**: Implements all core and optional commands from the Silvus radio ICD
- **CB-TIMING v0.3 Compliant**: Proper timing, blackouts, and backoff behavior
- **Dual Interface Support**: HTTP JSON-RPC (port 80) and TCP maintenance (port 50000)
- **Extensible Architecture**: Easy to add custom commands and behaviors
- **Production Ready**: systemd service, proper logging, and configuration management
- **Test Coverage**: Comprehensive unit tests with 75%+ coverage

## Quick Start

### Development Mode

```bash
# Clone and build
git clone <repository>
cd silvus-mock
make build

# Run in development mode (port 8080)
./silvusmock -config config/dev.yaml

# Test basic functionality
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"freq","id":"test"}' \
  http://localhost:8080/streamscape_api
```

### Production Installation

```bash
# Install as systemd service
sudo ./scripts/install.sh

# Manage service
sudo systemctl start silvus-mock
sudo systemctl status silvus-mock
```

## Supported Commands

### Core Commands (ICD §6.1)

| Method | Description | Parameters | Response |
|--------|-------------|------------|----------|
| `freq` | Set/read RF frequency (MHz) | `["<mhz>"]` or none | `["<mhz>"]` or `[""]` |
| `power_dBm` | Set/read transmit power (dBm) | `["<dbm>"]` or none | `["<dbm>"]` or `[""]` |
| `supported_frequency_profiles` | Get frequency profiles | none | Array of profile objects |

### Optional Commands (ICD §6.2)

| Method | Description | Parameters | Response |
|--------|-------------|------------|----------|
| `read_power_dBm` | Read actual output power | none | `["<dbm>"]` |
| `read_power_mw` | Read power in milliwatts | none | `["<mw>"]` |
| `max_link_distance` | Maximum link distance | none | `["<meters>"]` |
| `gps_coordinates` | GPS coordinates | `["lat","lon","alt"]` or none | GPS object or `[""]` |
| `gps_mode` | GPS operational mode | `["<enabled>"]` or none | GPS mode object or `[""]` |
| `gps_time` | GPS time | `["<unix_timestamp>"]` or none | `["<timestamp>"]` or `[""]` |

### Maintenance Commands (TCP Port 50000)

| Method | Description | Parameters | Response |
|--------|-------------|------------|----------|
| `zeroize` | Erase all settings/keys | none | `[""]` |
| `radio_reset` | Reboot radio | none | `[""]` |
| `factory_reset` | Factory defaults | none | `[""]` |

## Configuration

The emulator supports extensive configuration through YAML files and environment variables:

### Basic Configuration

```yaml
# config/default.yaml
network:
  http:
    port: 80                    # Production port
    devMode: false              # Use 8080 in dev
    serverHeader: ""            # Suppress server ID

timing:
  blackout:
    softBootSec: 30             # Channel change blackout
    powerChangeSec: 5           # Power change blackout
    radioResetSec: 60           # Radio reset blackout

profiles:
  frequencyProfiles:
    - frequencies: ["2200:20:2380", "4700"]
      bandwidth: "-1"
      antenna_mask: "15"
```

### Environment Variables

```bash
# Override configuration
export SILVUS_MOCK_MODE=degraded
export SILVUS_MOCK_SOFT_BOOT_TIME=15
export CBTIMING_CONFIG=/path/to/custom-timing.yaml
```

See [CONFIG.md](CONFIG.md) for complete configuration options.

## Testing with curl

### Basic Commands

```bash
# Read current frequency
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"freq","id":"test1"}' \
  http://localhost:8080/streamscape_api

# Set frequency (triggers 30s blackout)
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"freq","params":["4700"],"id":"test2"}' \
  http://localhost:8080/streamscape_api

# Read power
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"power_dBm","id":"test3"}' \
  http://localhost:8080/streamscape_api

# Set power
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"power_dBm","params":["25"],"id":"test4"}' \
  http://localhost:8080/streamscape_api

# Get frequency profiles
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"supported_frequency_profiles","id":"test5"}' \
  http://localhost:8080/streamscape_api
```

### Optional Commands

```bash
# Read actual power output
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"read_power_dBm","id":"test6"}' \
  http://localhost:8080/streamscape_api

# Read power in milliwatts
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"read_power_mw","id":"test7"}' \
  http://localhost:8080/streamscape_api

# Get maximum link distance
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"max_link_distance","id":"test8"}' \
  http://localhost:8080/streamscape_api

# GPS coordinates
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"gps_coordinates","id":"test9"}' \
  http://localhost:8080/streamscape_api
```

### Error Testing

```bash
# Invalid frequency (should return INVALID_RANGE)
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"freq","params":["9999"],"id":"test10"}' \
  http://localhost:8080/streamscape_api

# Invalid power (should return INVALID_RANGE)
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"power_dBm","params":["50"],"id":"test11"}' \
  http://localhost:8080/streamscape_api

# Command during blackout (should return UNAVAILABLE)
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"freq","params":["4700"],"id":"test12"}' \
  http://localhost:8080/streamscape_api
# Immediately after (during 30s blackout):
curl -X POST -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"freq","id":"test13"}' \
  http://localhost:8080/streamscape_api
```

### Maintenance Commands (TCP)

```bash
# Connect to maintenance port
nc localhost 50000

# Send zeroize command
{"jsonrpc":"2.0","method":"zeroize","id":"maint1"}

# Send radio reset
{"jsonrpc":"2.0","method":"radio_reset","id":"maint2"}

# Send factory reset
{"jsonrpc":"2.0","method":"factory_reset","id":"maint3"}
```

## RCC Integration

The emulator is designed to work seamlessly with the Radio Control Container:

### Docker Compose Integration

```yaml
# docker-compose.yml
version: '3.8'
services:
  silvus-mock:
    build: .
    ports:
      - "80:80"
      - "50000:50000"
    environment:
      - SILVUS_MOCK_MODE=normal
    networks:
      - radio-network

  rcc:
    image: radio-control-container:latest
    depends_on:
      - silvus-mock
    environment:
      - RADIO_ENDPOINT=http://silvus-mock:80/streamscape_api
    networks:
      - radio-network

networks:
  radio-network:
    driver: bridge
```

### End-to-End Testing

```bash
# Start both services
docker-compose up -d

# Test RCC -> Silvus Mock communication
curl -X POST http://localhost:8002/api/v1/radios/silvus-01/power \
  -H "Content-Type: application/json" \
  -d '{"powerDbm": 25}'

# Verify power was set
curl -X GET http://localhost:8002/api/v1/radios/silvus-01/power
```

## Error Codes

The emulator returns standardized error codes per ICD §8 and OpenAPI §2.2:

| Error Code | HTTP Status | Description | Client Action |
|------------|-------------|-------------|---------------|
| `INVALID_RANGE` | 400 | Parameter out of valid range | Fix parameters, don't retry |
| `BUSY` | 503 | Radio busy with another operation | Retry with backoff |
| `UNAVAILABLE` | 503 | Radio in blackout/soft-boot | Wait and retry |
| `INTERNAL` | 500 | Internal error | Retry with backoff, log for diagnostics |

## Timing Behavior

The emulator implements CB-TIMING v0.3 specifications:

- **Channel Change Blackout**: 30 seconds (after `freq` command)
- **Power Change Blackout**: 5 seconds (after `power_dBm` command)
- **Radio Reset Blackout**: 60 seconds (after `radio_reset` command)
- **Command Timeouts**: 5-30 seconds based on command type
- **Read During Blackout**: Returns `UNAVAILABLE` (ICD compliance)

## Development

### Building

```bash
# Build binary
make build

# Run tests
make test

# Run with coverage
make test-coverage

# Build Docker image
make docker
```

### Adding Custom Commands

The emulator supports custom commands through the extensible command system:

```go
// Register custom command
customHandler := commands.NewCustomCommandHandler(
    "custom_status",
    "Get custom system status",
    true,  // read-only
    false, // no blackout
    func(ctx context.Context, params []string) (interface{}, error) {
        return map[string]interface{}{
            "status": "operational",
            "uptime": "24h",
        }, nil
    },
)
server.AddCustomCommand(customHandler)
```

## Troubleshooting

### Common Issues

1. **Permission denied on port 80**
   ```bash
   # Use development mode
   ./silvusmock -config config/dev.yaml
   
   # Or set capabilities for production
   sudo setcap 'cap_net_bind_service=+ep' /usr/local/bin/silvusmock
   ```

2. **Blackout behavior not working**
   - Check timing configuration in `config/default.yaml`
   - Verify CB-TIMING values are correct
   - Ensure commands return `UNAVAILABLE` during blackout

3. **RCC integration issues**
   - Verify radio endpoint URL is correct
   - Check network connectivity between RCC and emulator
   - Review RCC logs for adapter errors

### Logs

```bash
# View service logs
sudo journalctl -u silvus-mock -f

# View application logs
tail -f /var/log/silvus-mock/silvus-mock.log
```

## License

[Add your license information here]

## Contributing

[Add contribution guidelines here]

## Support

[Add support information here]
