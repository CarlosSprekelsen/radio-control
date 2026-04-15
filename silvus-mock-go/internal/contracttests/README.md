# Contract Tests - Silvus Mock Radio Emulator

This package contains contract compliance tests that validate the Silvus Mock Radio Emulator strictly adheres to the ICD (Interface Control Document) and CB-TIMING v0.3 specifications.

## Purpose

These tests ensure that:
1. The emulator is indistinguishable from a real Silvus radio device
2. All JSON-RPC envelopes conform to the 2.0 specification
3. Method behaviors match ICD requirements exactly
4. Error handling follows the defined error codes
5. Timing and blackout behavior comply with CB-TIMING v0.3
6. The server debugging team won't encounter unexpected behaviors

## Test Structure

### HTTP JSON-RPC Tests (`http_contract_test.go`)
- **Envelope Compliance**: Validates JSON-RPC 2.0 envelope structure
- **Method POST Only**: Ensures only POST requests are accepted
- **Path Exact Match**: Validates `/streamscape_api` path requirement
- **Core Methods Compliance**: Tests `freq`, `power_dBm`, `supported_frequency_profiles`
- **Error Handling**: Validates error responses and codes
- **Blackout Behavior**: Ensures UNAVAILABLE during soft-boot periods

### TCP Maintenance Tests (`tcp_contract_test.go`)
- **Method Existence**: Validates `zeroize`, `radio_reset`, `factory_reset`
- **JSON-RPC Compliance**: Ensures proper JSON-RPC over TCP
- **Local Only Policy**: Tests CIDR-based access restrictions
- **Blackout Interaction**: Validates radio reset blackout behavior

### Validation Utilities (`validator.go`)
- **JSON-RPC Envelope Validation**: Ensures proper 2.0 compliance
- **Frequency Profile Validation**: Validates profile object structure
- **Error Response Validation**: Ensures proper error object format

### Test Fixtures (`fixtures/`)
- **Golden Requests**: Standard request examples from ICD
- **Golden Responses**: Expected response formats
- **TCP Requests/Responses**: Maintenance command examples

## Running Contract Tests

```bash
# Run all contract tests
go test ./internal/contracttests -v

# Run specific test suites
go test ./internal/contracttests -v -run HTTP_JSONRPC
go test ./internal/contracttests -v -run TCP_Maintenance

# Run with detailed output
go test ./internal/contracttests -v -run TestContractCompliance
```

## Contract Validation

### JSON-RPC 2.0 Compliance
- `jsonrpc` field must be exactly `"2.0"`
- `id` field must be echoed unchanged
- `result` and `error` fields are mutually exclusive
- One of `result` or `error` must be present

### HTTP Endpoint Compliance
- Path must be exactly `/streamscape_api`
- Method must be POST only
- Content-Type must be `application/json`
- HTTP status must be 200 (even for JSON-RPC errors)

### Method Behavior Compliance
- `freq` READ: Returns `["<frequency>"]`
- `freq` SET: Returns `[""]` on success, triggers 30s blackout
- `power_dBm` READ: Returns `["<power>"]`
- `power_dBm` SET: Returns `[""]` on success, triggers 5s blackout
- `supported_frequency_profiles`: Returns array of profile objects

### Error Code Compliance
- `INVALID_RANGE`: Invalid frequency/power values
- `BUSY`: System busy (not used in current implementation)
- `UNAVAILABLE`: During blackout periods
- `INTERNAL`: Internal system errors
- Method not found: Standard JSON-RPC error

### Timing Compliance (CB-TIMING v0.3)
- Channel change blackout: 30 seconds
- Power change blackout: 5 seconds  
- Radio reset blackout: 60 seconds
- All commands return UNAVAILABLE during blackout

### TCP Maintenance Compliance
- Port 50000 for maintenance commands
- Local interfaces only (configurable CIDR)
- JSON-RPC over TCP with newline delimiters
- Success responses: `[""]`

## Integration with CI/CD

These contract tests should be run as part of the CI pipeline to ensure:
1. No regressions in contract compliance
2. Changes don't break ICD conformance
3. New features maintain backward compatibility

```bash
# In CI pipeline
go test ./internal/contracttests -v -failfast
```

## Adding New Contract Tests

When adding new functionality:

1. **Add Golden Fixtures**: Create request/response examples in `fixtures/`
2. **Extend Validators**: Add validation logic in `validator.go`
3. **Write Contract Tests**: Add tests in appropriate `*_contract_test.go` files
4. **Update Documentation**: Update this README with new test coverage

## References

- ICD §5: Access, JSON-RPC, data formats, errors
- ICD §6.1.1-6.1.3: Core method specifications
- ICD §5.4/§6.2: Maintenance TCP methods
- CB-TIMING v0.3 §5, §6.2: Timing and blackout specifications

## Failure Investigation

When contract tests fail:

1. **Check ICD Compliance**: Verify the behavior matches ICD requirements
2. **Review Error Messages**: Look for specific validation failures
3. **Compare with Golden Fixtures**: Ensure responses match expected formats
4. **Validate Timing**: Check blackout durations and behaviors
5. **Test Real Device**: Compare with actual Silvus radio behavior if available

Contract test failures indicate potential issues that could confuse the server debugging team or break RCC integration.
