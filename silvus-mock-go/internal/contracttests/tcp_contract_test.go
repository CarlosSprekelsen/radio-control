package contracttests

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"strings"
	"testing"
	"time"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/maintenance"
	"github.com/silvus-mock/internal/state"
)

// TestTCPServer wraps the TCP maintenance server for contract testing
type TestTCPServer struct {
	listener net.Listener
	server   *maintenance.Server
	radioState *state.RadioState
	config   *config.Config
	port     int
}

// NewTestTCPServer creates a test TCP server
func NewTestTCPServer(t *testing.T) *TestTCPServer {
	// Create test configuration
	cfg := &config.Config{
		Network: config.NetworkConfig{
			HTTP: config.HTTPConfig{
				Port:         8080,
				ServerHeader: "",
				DevMode:      true,
			},
			Maintenance: config.MaintenanceConfig{
				Port:         0, // Let system assign port
				AllowedCIDRs: []string{"127.0.0.0/8", "172.20.0.0/16"},
			},
		},
		Profiles: config.ProfilesConfig{
			FrequencyProfiles: []config.FrequencyProfile{
				{
					Frequencies: []string{"2200:20:2380", "4700"},
					Bandwidth:   "-1",
					AntennaMask: "15",
				},
			},
		},
		Power: config.PowerConfig{
			MinDBm: 0,
			MaxDBm: 39,
		},
		Timing: config.TimingConfig{
			Blackout: config.BlackoutConfig{
				SoftBootSec:    1, // 1 second for unit tests
				PowerChangeSec: 1,
				RadioResetSec:  1,
			},
			Commands: config.CommandsConfig{
				SetPower:    config.TimeoutConfig{TimeoutSec: 10},
				SetChannel:  config.TimeoutConfig{TimeoutSec: 30},
				SelectRadio: config.TimeoutConfig{TimeoutSec: 5},
				Read:        config.TimeoutConfig{TimeoutSec: 5},
			},
			Backoff: config.BackoffConfig{
				BusyBaseMs: 1000,
			},
		},
		Mode: "normal",
	}

	radioState := state.NewRadioState(cfg)
	server := maintenance.NewServer(cfg, radioState)

	// Start listener on any available port
	listener, err := net.Listen("tcp", ":0")
	if err != nil {
		t.Fatalf("Failed to create TCP listener: %v", err)
	}

	// Get the assigned port
	port := listener.Addr().(*net.TCPAddr).Port

	// Start server in goroutine
	go func() {
		if err := server.ListenAndServe(); err != nil {
			t.Logf("TCP server error: %v", err)
		}
	}()

	return &TestTCPServer{
		listener:   listener,
		server:     server,
		radioState: radioState,
		config:     cfg,
		port:       port,
	}
}

// Close shuts down the test TCP server
func (ts *TestTCPServer) Close() {
	ts.listener.Close()
}

// SendJSONRPC sends a JSON-RPC request over TCP and returns the response
func (ts *TestTCPServer) SendJSONRPC(t *testing.T, request map[string]interface{}) map[string]interface{} {
	conn, err := net.Dial("tcp", fmt.Sprintf("localhost:%d", ts.port))
	if err != nil {
		t.Fatalf("Failed to connect to TCP server: %v", err)
	}
	defer conn.Close()

	// Set timeout
	conn.SetDeadline(time.Now().Add(5 * time.Second))

	// Send JSON-RPC request
	jsonData, err := json.Marshal(request)
	if err != nil {
		t.Fatalf("Failed to marshal request: %v", err)
	}

	// Send with newline delimiter
	if _, err := conn.Write(append(jsonData, '\n')); err != nil {
		t.Fatalf("Failed to send request: %v", err)
	}

	// Read response
	reader := bufio.NewReader(conn)
	responseLine, err := reader.ReadString('\n')
	if err != nil {
		t.Fatalf("Failed to read response: %v", err)
	}

	// Parse response
	var response map[string]interface{}
	if err := json.Unmarshal([]byte(strings.TrimSpace(responseLine)), &response); err != nil {
		t.Fatalf("Failed to unmarshal response: %v", err)
	}

	return response
}

func TestTCPMethodExistence(t *testing.T) {
	server := NewTestTCPServer(t)
	defer server.Close()

	tests := []struct {
		name     string
		fixture  string
		key      string
		expected string
	}{
		{
			name:     "zeroize",
			fixture:  "tcp_requests.json",
			key:      "zeroize",
			expected: "zeroize_success",
		},
		{
			name:     "radio_reset",
			fixture:  "tcp_requests.json",
			key:      "radio_reset",
			expected: "radio_reset_success",
		},
		{
			name:     "factory_reset",
			fixture:  "tcp_requests.json",
			key:      "factory_reset",
			expected: "factory_reset_success",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			request := loadGoldenFixture(t, tt.fixture, tt.key)
			expected := loadGoldenFixture(t, "tcp_responses.json", tt.expected)

			response := server.SendJSONRPC(t, request)

			// Validate JSON-RPC envelope
			responseJSON, _ := json.Marshal(response)
			if err := ValidateEnvelope(responseJSON); err != nil {
				t.Errorf("Invalid JSON-RPC envelope: %v", err)
			}

			// Compare with expected response
			expectedJSON, _ := json.Marshal(expected)
			if err := CompareEnvelopes(expectedJSON, responseJSON); err != nil {
				t.Errorf("Response envelope mismatch: %v", err)
			}

			// Validate result structure (should be [""])
			result, ok := response["result"].([]interface{})
			if !ok {
				t.Errorf("Expected result to be array")
				return
			}
			if len(result) != 1 || result[0] != "" {
				t.Errorf("Expected result [\"\"], got %v", result)
			}
		})
	}
}

func TestTCPJSONRPCCompliance(t *testing.T) {
	server := NewTestTCPServer(t)
	defer server.Close()

	// Test with a simple request
	request := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      "test-compliance-1",
		"method":  "zeroize",
		"params":  []interface{}{},
	}

	response := server.SendJSONRPC(t, request)

	// Validate envelope structure
	responseJSON, _ := json.Marshal(response)
	if err := ValidateEnvelope(responseJSON); err != nil {
		t.Errorf("Invalid JSON-RPC envelope: %v", err)
	}

	// Check jsonrpc version
	if response["jsonrpc"] != "2.0" {
		t.Errorf("Expected jsonrpc '2.0', got '%v'", response["jsonrpc"])
	}

	// Check id echo
	if response["id"] != "test-compliance-1" {
		t.Errorf("Expected id 'test-compliance-1', got '%v'", response["id"])
	}

	// Check result structure
	if _, hasResult := response["result"]; !hasResult {
		t.Errorf("Expected result field")
	}
	if _, hasError := response["error"]; hasError {
		t.Errorf("Should not have error field for successful request")
	}
}

func TestTCPLocalOnlyPolicy(t *testing.T) {
	// Create server with restrictive CIDR list
	cfg := &config.Config{
		Network: config.NetworkConfig{
			HTTP: config.HTTPConfig{
				Port:         8080,
				ServerHeader: "",
				DevMode:      true,
			},
			Maintenance: config.MaintenanceConfig{
				Port:         0,
				AllowedCIDRs: []string{"192.168.1.0/24"}, // Only allow 192.168.1.x
			},
		},
		Profiles: config.ProfilesConfig{
			FrequencyProfiles: []config.FrequencyProfile{
				{
					Frequencies: []string{"2200:20:2380"},
					Bandwidth:   "-1",
					AntennaMask: "15",
				},
			},
		},
		Power: config.PowerConfig{
			MinDBm: 0,
			MaxDBm: 39,
		},
		Timing: config.TimingConfig{
			Blackout: config.BlackoutConfig{
				SoftBootSec:    1,
				PowerChangeSec: 1,
				RadioResetSec:  1,
			},
			Commands: config.CommandsConfig{
				SetPower:    config.TimeoutConfig{TimeoutSec: 10},
				SetChannel:  config.TimeoutConfig{TimeoutSec: 30},
				SelectRadio: config.TimeoutConfig{TimeoutSec: 5},
				Read:        config.TimeoutConfig{TimeoutSec: 5},
			},
			Backoff: config.BackoffConfig{
				BusyBaseMs: 1000,
			},
		},
		Mode: "normal",
	}

	radioState := state.NewRadioState(cfg)
	server := maintenance.NewServer(cfg, radioState)

	// Start listener
	listener, err := net.Listen("tcp", ":0")
	if err != nil {
		t.Fatalf("Failed to create TCP listener: %v", err)
	}
	defer listener.Close()

	port := listener.Addr().(*net.TCPAddr).Port

	// Start server
	go func() {
		server.ListenAndServe()
	}()

	// Give server time to start
	time.Sleep(100 * time.Millisecond)

	// Try to connect from localhost (should be rejected)
	conn, err := net.Dial("tcp", fmt.Sprintf("localhost:%d", port))
	if err != nil {
		t.Fatalf("Failed to connect to TCP server: %v", err)
	}
	defer conn.Close()

	// Send a request
	request := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      "test-local-only",
		"method":  "zeroize",
		"params":  []interface{}{},
	}

	jsonData, _ := json.Marshal(request)
	conn.Write(append(jsonData, '\n'))

	// Set timeout for response
	conn.SetDeadline(time.Now().Add(2 * time.Second))

	// Try to read response
	reader := bufio.NewReader(conn)
	_, err = reader.ReadString('\n')
	
	// Connection should be closed or timeout (indicating rejection)
	if err == nil {
		t.Errorf("Expected connection to be rejected, but got response")
	}
}

func TestTCPRadioResetBlackoutInteraction(t *testing.T) {
	server := NewTestTCPServer(t)
	defer server.Close()

	// Send radio_reset via TCP
	resetRequest := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      "tcp-reset-test",
		"method":  "radio_reset",
		"params":  []interface{}{},
	}

	response := server.SendJSONRPC(t, resetRequest)

	// Validate successful reset
	if response["result"] == nil {
		t.Errorf("Expected successful radio_reset response")
	}

	// Verify result is [""]
	result, ok := response["result"].([]interface{})
	if !ok || len(result) != 1 || result[0] != "" {
		t.Errorf("Expected result [\"\"], got %v", result)
	}

	// Note: In a real integration test, we would also test HTTP calls
	// returning UNAVAILABLE during the blackout period, but that would
	// require coordination between HTTP and TCP servers which is beyond
	// the scope of unit contract tests. That should be covered in
	// integration tests.
}

func TestTCPUnknownMethod(t *testing.T) {
	server := NewTestTCPServer(t)
	defer server.Close()

	// Send unknown method
	request := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      "test-unknown-tcp",
		"method":  "nonexistent_method",
		"params":  []interface{}{},
	}

	response := server.SendJSONRPC(t, request)

	// Should have error field
	if _, hasError := response["error"]; !hasError {
		t.Errorf("Expected error field for unknown method")
	}

	if _, hasResult := response["result"]; hasResult {
		t.Errorf("Should not have result field when error is present")
	}

	// Validate error structure
	errorData, _ := json.Marshal(response["error"])
	if err := ValidateErrorResponse(errorData); err != nil {
		t.Errorf("Invalid error response: %v", err)
	}
}

