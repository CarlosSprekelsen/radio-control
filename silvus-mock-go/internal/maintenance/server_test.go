package maintenance

import (
	"encoding/json"
	"net"
	"testing"
	"time"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

func TestNewServer(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	if server.config != cfg {
		t.Error("Expected server config to be set")
	}
	if server.state != radioState {
		t.Error("Expected server state to be set")
	}
	if server.stopChan == nil {
		t.Error("Expected stopChan to be initialized")
	}
}

func TestProcessMaintenanceRequest(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Wait for any blackout to clear
	time.Sleep(6 * time.Second)

	tests := []struct {
		name         string
		method       string
		expectedType string
	}{
		{"zeroize", "zeroize", "zeroize"},
		{"radio reset", "radio_reset", "radioReset"},
		{"factory reset", "factory_reset", "factoryReset"},
		{"invalid method", "invalid_method", ""},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			req := &Request{
				JSONRPC: "2.0",
				Method:  tt.method,
				ID:      "test-1",
			}

			response := server.processMaintenanceRequest(req)

			if response.JSONRPC != "2.0" {
				t.Errorf("Expected JSONRPC 2.0, got %s", response.JSONRPC)
			}
			if response.ID != "test-1" {
				t.Errorf("Expected ID 'test-1', got %v", response.ID)
			}

			if tt.method == "invalid_method" {
				if response.Error != "Method not found" {
					t.Errorf("Expected 'Method not found' error, got %v", response.Error)
				}
			} else {
				if response.Error != nil {
					t.Errorf("Expected no error for valid method, got %v", response.Error)
				}
				if response.Result == nil {
					t.Error("Expected result for valid method")
				}
			}
		})
	}
}

func TestIsAllowedConnection(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Test with mock connection
	tests := []struct {
		name        string
		remoteAddr  string
		expectAllow bool
	}{
		{"localhost IPv4", "127.0.0.1:12345", true},
		{"localhost IPv6", "[::1]:12345", false}, // Not in allowed CIDRs
		{"radio network", "172.20.1.10:12345", true},
		{"outside network", "192.168.1.1:12345", false},
		{"invalid address", "invalid", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create a mock connection
			conn := &mockConn{remoteAddr: tt.remoteAddr}

			allowed := server.isAllowedConnection(conn)
			if allowed != tt.expectAllow {
				t.Errorf("Expected allowed=%v for %s, got %v", tt.expectAllow, tt.remoteAddr, allowed)
			}
		})
	}
}

func TestWriteErrorResponse(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Create a mock connection
	conn := &mockConn{}

	// Write error response
	server.writeErrorResponse(conn, -32600, "Invalid Request", "test-id")

	// Check that data was written
	if len(conn.writtenData) == 0 {
		t.Error("Expected error response to be written")
	}

	// Parse the written JSON
	var response Response
	err := json.Unmarshal(conn.writtenData, &response)
	if err != nil {
		t.Fatalf("Failed to unmarshal error response: %v", err)
	}

	// Check response structure
	if response.JSONRPC != "2.0" {
		t.Errorf("Expected JSONRPC 2.0, got %s", response.JSONRPC)
	}
	if response.ID != "test-id" {
		t.Errorf("Expected ID 'test-id', got %v", response.ID)
	}
	if response.Result != nil {
		t.Error("Expected no result in error response")
	}

	// Check error object
	errorObj, ok := response.Error.(map[string]interface{})
	if !ok {
		t.Fatalf("Expected error to be an object, got %T", response.Error)
	}
	if errorObj["code"] != float64(-32600) {
		t.Errorf("Expected error code -32600, got %v", errorObj["code"])
	}
	if errorObj["message"] != "Invalid Request" {
		t.Errorf("Expected error message 'Invalid Request', got %v", errorObj["message"])
	}
}

func TestServerClose(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Test that close doesn't panic
	err := server.Close()
	if err != nil {
		t.Errorf("Close() returned error: %v", err)
	}

	// Test closing already closed server (should handle gracefully)
	err = server.Close()
	if err != nil {
		t.Errorf("Close() on already closed server returned error: %v", err)
	}
}

func TestHandleConnection(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Create a mock connection with valid request
	req := Request{
		JSONRPC: "2.0",
		Method:  "zeroize",
		ID:      "test-1",
	}
	reqData, _ := json.Marshal(req)

	conn := &mockConn{
		remoteAddr: "127.0.0.1:12345",
		readData:   reqData,
	}

	// Handle connection (this should not block in test)
	done := make(chan bool, 1)
	go func() {
		server.handleConnection(conn)
		done <- true
	}()

	// Wait for completion with timeout
	select {
	case <-done:
		// Success
	case <-time.After(1 * time.Second):
		t.Error("handleConnection timed out")
	}

	// Check that response was written
	if len(conn.writtenData) == 0 {
		t.Error("Expected response to be written")
	}

	// Parse response
	var response Response
	err := json.Unmarshal(conn.writtenData, &response)
	if err != nil {
		t.Fatalf("Failed to unmarshal response: %v", err)
	}

	if response.JSONRPC != "2.0" {
		t.Errorf("Expected JSONRPC 2.0, got %s", response.JSONRPC)
	}
	if response.Error != nil {
		t.Errorf("Expected no error, got %v", response.Error)
	}
}

func TestHandleConnectionInvalidJSON(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Create a mock connection with invalid JSON
	conn := &mockConn{
		remoteAddr: "127.0.0.1:12345",
		readData:   []byte("invalid json"),
	}

	// Handle connection
	done := make(chan bool, 1)
	go func() {
		server.handleConnection(conn)
		done <- true
	}()

	select {
	case <-done:
		// Success
	case <-time.After(1 * time.Second):
		t.Error("handleConnection timed out")
	}

	// Check that error response was written
	if len(conn.writtenData) == 0 {
		t.Error("Expected error response to be written")
	}
}

func TestHandleConnectionWrongJSONRPCVersion(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Create a mock connection with wrong JSON-RPC version
	req := Request{
		JSONRPC: "1.0",
		Method:  "zeroize",
		ID:      "test-1",
	}
	reqData, _ := json.Marshal(req)

	conn := &mockConn{
		remoteAddr: "127.0.0.1:12345",
		readData:   reqData,
	}

	// Handle connection
	done := make(chan bool, 1)
	go func() {
		server.handleConnection(conn)
		done <- true
	}()

	select {
	case <-done:
		// Success
	case <-time.After(1 * time.Second):
		t.Error("handleConnection timed out")
	}

	// Check that error response was written
	if len(conn.writtenData) == 0 {
		t.Error("Expected error response to be written")
	}
}

func TestCIDRFiltering(t *testing.T) {
	cfg := createTestConfig()
	// Add more specific CIDR for testing
	cfg.Network.Maintenance.AllowedCIDRs = []string{
		"127.0.0.0/8",
		"172.20.0.0/16",
		"10.0.0.0/8",
	}

	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	tests := []struct {
		name        string
		remoteAddr  string
		expectAllow bool
	}{
		{"localhost", "127.0.0.1:12345", true},
		{"radio network", "172.20.1.10:12345", true},
		{"private network", "10.1.1.1:12345", true},
		{"public network", "8.8.8.8:12345", false},
		{"other private", "192.168.1.1:12345", false},
		{"invalid IP", "invalid:12345", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			conn := &mockConn{remoteAddr: tt.remoteAddr}
			allowed := server.isAllowedConnection(conn)
			if allowed != tt.expectAllow {
				t.Errorf("Expected allowed=%v for %s, got %v", tt.expectAllow, tt.remoteAddr, allowed)
			}
		})
	}
}

// Mock connection for testing
type mockConn struct {
	remoteAddr  string
	readData    []byte
	writtenData []byte
	readPos     int
}

func (m *mockConn) Read(b []byte) (n int, err error) {
	if m.readPos >= len(m.readData) {
		return 0, nil // EOF
	}

	n = copy(b, m.readData[m.readPos:])
	m.readPos += n
	return n, nil
}

func (m *mockConn) Write(b []byte) (n int, err error) {
	m.writtenData = append(m.writtenData, b...)
	return len(b), nil
}

func (m *mockConn) Close() error {
	return nil
}

func (m *mockConn) LocalAddr() net.Addr {
	return &mockAddr{"local"}
}

func (m *mockConn) RemoteAddr() net.Addr {
	return &mockAddr{m.remoteAddr}
}

func (m *mockConn) SetDeadline(t time.Time) error {
	return nil
}

func (m *mockConn) SetReadDeadline(t time.Time) error {
	return nil
}

func (m *mockConn) SetWriteDeadline(t time.Time) error {
	return nil
}

type mockAddr struct {
	addr string
}

func (m *mockAddr) Network() string {
	return "tcp"
}

func (m *mockAddr) String() string {
	return m.addr
}

// Helper functions
func createTestConfig() *config.Config {
	return &config.Config{
		Network: config.NetworkConfig{
			Maintenance: config.MaintenanceConfig{
				Port:         50000,
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
				SoftBootSec: 5,
			},
		},
		Mode: "normal",
	}
}

func createTestRadioState(cfg *config.Config) *state.RadioState {
	return state.NewRadioState(cfg)
}
