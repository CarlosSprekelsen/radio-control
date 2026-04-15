# Silvus Radio Mock Emulator

A drop-in Silvus radio emulator that RCC talks to as if it were a real device. RCC remains unchanged and unaware of the mock.

## Features

- **ICD Parity**: Implements exact method names and shapes from ICD §6
  - `freq` (set/read RF frequency, MHz)
  - `power_dBm` (set/read TX power, 0-39 dBm)
  - `supported_frequency_profiles` (read-only frequency/bandwidth/antenna combinations)
- **Maintenance**: TCP :50000 supports `zeroize`, `radio_reset`, `factory_reset`
- **Timing**: All delays/backoffs read from CB-TIMING config (no literals in code)
- **Indistinguishability**: Same ports/paths/headers/latency as real device
- **Errors**: Vendor-style errors mapped to OpenAPI normalized set

## Quick Start

### Build and Run

```bash
# Build locally
make build

# Run with default config
make run

# Or build and run with Docker
make docker
make docker-run
```

### Test with RCC

```bash
# Start with Docker Compose (includes example RCC)
make docker-compose
```

## Configuration

### Default Configuration

The mock uses `config/default.yaml` with:
- HTTP server on port 80 (`/streamscape_api`)
- Maintenance TCP server on port 50000
- Frequency profiles from ICD examples
- Power range 0-39 dBm
- Timing values from CB-TIMING v0.3

### Environment Overrides

- `CBTIMING_CONFIG=/path/to/cb-timing.yaml` - Load CB-TIMING configuration
- `SILVUS_MOCK_MODE=normal|degraded|offline` - Operation mode
- `SILVUS_MOCK_SOFT_BOOT_TIME=5` - Override soft boot duration (seconds)

## API Usage

### Set Power
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","method":"power_dBm","params":["30"],"id":"1"}' \
  http://localhost:8080/streamscape_api
```

### Set Frequency
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","method":"freq","params":["2490"],"id":"x"}' \
  http://localhost:8080/streamscape_api
```

### Read Frequency Profiles
```bash
curl -X POST -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","method":"supported_frequency_profiles","id":"pf"}' \
  http://localhost:8080/streamscape_api
```

### Maintenance Commands (TCP :50000)
```bash
# Zeroize
echo '{"jsonrpc":"2.0","method":"zeroize","id":"1"}' | nc localhost 50000

# Radio Reset
echo '{"jsonrpc":"2.0","method":"radio_reset","id":"2"}' | nc localhost 50000

# Factory Reset
echo '{"jsonrpc":"2.0","method":"factory_reset","id":"3"}' | nc localhost 50000
```

## Docker Network Integration

For RCC integration, use the provided `docker-compose.test.yml`:

```yaml
services:
  silvus-mock:
    image: silvus-mock:latest
    networks:
      radionet:
        ipv4_address: 172.20.1.10
    environment:
      - CBTIMING_CONFIG=/etc/cb-timing.yaml
    volumes:
      - ./config/cb-timing.yaml:/etc/cb-timing.yaml:ro

  rcc:
    image: your-rcc:latest
    networks:
      radionet:
        ipv4_address: 172.20.0.1
    environment:
      - RADIO_SILVUS_ADDR=172.20.1.10
```

## Error Handling

The mock returns vendor-style errors that RCC normalizes:
- `INVALID_RANGE` - Invalid parameter values
- `BUSY` - Radio in soft-boot blackout
- `UNAVAILABLE` - Radio temporarily unavailable
- `INTERNAL` - Internal error

## Soft-Boot Behavior

After setting frequency via `freq`, the radio enters a soft-boot blackout period (configurable, default 5 seconds). During this time:
- Set commands return `BUSY` or `UNAVAILABLE`
- Read commands may return `UNAVAILABLE`
- Duration is configurable via `CBTIMING_CONFIG` or `SILVUS_MOCK_SOFT_BOOT_TIME`

## Development

### Project Structure
```
silvus-mock/
├── cmd/silvusmock/main.go           # Main application
├── internal/
│   ├── config/                      # Configuration management
│   ├── jsonrpc/                     # HTTP JSON-RPC server
│   ├── state/                       # Radio state management
│   └── maintenance/                 # TCP maintenance server
├── config/                          # Configuration files
├── Dockerfile                       # Container build
├── Makefile                         # Build targets
└── docker-compose.test.yml         # Test integration
```

### Adding Features

1. **New Methods**: Add to `internal/jsonrpc/server.go`
2. **Configuration**: Extend `internal/config/config.go`
3. **State**: Modify `internal/state/radio.go`
4. **Timing**: Update CB-TIMING config files

## Compliance

- **ICD**: Implements exact method signatures and responses
- **Architecture**: Error normalization per §8.5
- **CB-TIMING**: All timing values externalized to config
- **Packaging**: Docker image exposes :80 and :50000

## Troubleshooting

### Connection Issues
- Verify ports 80 and 50000 are available
- Check firewall rules for maintenance port access
- Ensure Docker network configuration is correct

### Configuration Issues
- Validate YAML syntax in config files
- Check environment variable overrides
- Verify CB-TIMING bounds are within reasonable ranges

### RCC Integration
- Ensure RCC points to correct IP (172.20.1.10 in test setup)
- Verify JSON-RPC request format matches ICD specifications
- Check logs for timing and error responses
