package jsonrpc

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

func TestServerHandleRequest(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Wait for any blackout to clear
	time.Sleep(6 * time.Second)

	tests := []struct {
		name           string
		method         string
		params         []string
		expectedError  string
		expectedResult interface{}
	}{
		{
			name:           "set power valid",
			method:         "power_dBm",
			params:         []string{"25"},
			expectedError:  "",
			expectedResult: []string{""},
		},
		{
			name:           "set power invalid",
			method:         "power_dBm",
			params:         []string{"50"},
			expectedError:  "INVALID_RANGE",
			expectedResult: nil,
		},
		{
			name:           "get power",
			method:         "power_dBm",
			params:         nil,
			expectedError:  "",
			expectedResult: []string{"30"},
		},
		{
			name:           "set frequency valid",
			method:         "freq",
			params:         []string{"4700"},
			expectedError:  "",
			expectedResult: []string{""},
		},
		{
			name:           "set frequency invalid",
			method:         "freq",
			params:         []string{"9999"},
			expectedError:  "INVALID_RANGE",
			expectedResult: nil,
		},
		{
			name:           "get frequency",
			method:         "freq",
			params:         nil,
			expectedError:  "",
			expectedResult: []string{"2490.0"},
		},
		{
			name:           "get profiles",
			method:         "supported_frequency_profiles",
			params:         nil,
			expectedError:  "",
			expectedResult: nil, // Will check structure separately
		},
		{
			name:           "invalid method",
			method:         "invalid_method",
			params:         nil,
			expectedError:  "Method not found",
			expectedResult: nil,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create JSON-RPC request
			req := Request{
				JSONRPC: "2.0",
				Method:  tt.method,
				ID:      "test-1",
			}
			if tt.params != nil {
				req.Params = tt.params
			}

			reqBody, err := json.Marshal(req)
			if err != nil {
				t.Fatalf("Failed to marshal request: %v", err)
			}

			// Create HTTP request
			httpReq := httptest.NewRequest("POST", "/streamscape_api", bytes.NewBuffer(reqBody))
			httpReq.Header.Set("Content-Type", "application/json")

			// Create response recorder
			rr := httptest.NewRecorder()

			// Handle request
			server.HandleRequest(rr, httpReq)

			// Check status code
			if rr.Code != http.StatusOK {
				t.Errorf("Expected status 200, got %d", rr.Code)
			}

			// Parse response
			var response Response
			err = json.Unmarshal(rr.Body.Bytes(), &response)
			if err != nil {
				t.Fatalf("Failed to unmarshal response: %v", err)
			}

			// Check JSON-RPC version
			if response.JSONRPC != "2.0" {
				t.Errorf("Expected JSONRPC 2.0, got %s", response.JSONRPC)
			}

			// Check ID
			if response.ID != "test-1" {
				t.Errorf("Expected ID 'test-1', got %v", response.ID)
			}

			// Check error
			if tt.expectedError != "" {
				if response.Error != tt.expectedError {
					t.Errorf("Expected error '%s', got %v", tt.expectedError, response.Error)
				}
				if response.Result != nil {
					t.Error("Expected no result when error is present")
				}
			} else {
				if response.Error != nil {
					t.Errorf("Expected no error, got %v", response.Error)
				}
				if response.Result == nil {
					t.Error("Expected result when no error")
				}
			}

			// Check result for specific cases
			if tt.expectedResult != nil && response.Result != nil {
				resultBytes, _ := json.Marshal(response.Result)
				expectedBytes, _ := json.Marshal(tt.expectedResult)
				if string(resultBytes) != string(expectedBytes) {
					t.Errorf("Expected result %v, got %v", tt.expectedResult, response.Result)
				}
			}
		})
	}
}

func TestServerHandleRequestInvalidJSON(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Create invalid JSON request
	httpReq := httptest.NewRequest("POST", "/streamscape_api", bytes.NewBufferString("invalid json"))
	httpReq.Header.Set("Content-Type", "application/json")

	rr := httptest.NewRecorder()
	server.HandleRequest(rr, httpReq)

	// Should return error
	if rr.Code != http.StatusBadRequest {
		t.Errorf("Expected status 400 for invalid JSON, got %d", rr.Code)
	}
}

func TestServerHandleRequestWrongMethod(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Create GET request (should be POST)
	httpReq := httptest.NewRequest("GET", "/streamscape_api", nil)

	rr := httptest.NewRecorder()
	server.HandleRequest(rr, httpReq)

	// Should return error
	if rr.Code != http.StatusBadRequest {
		t.Errorf("Expected status 400 for wrong method, got %d", rr.Code)
	}
}

func TestServerHandleRequestWrongJSONRPCVersion(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Create request with wrong JSON-RPC version
	req := Request{
		JSONRPC: "1.0",
		Method:  "power_dBm",
		ID:      "test-1",
	}

	reqBody, err := json.Marshal(req)
	if err != nil {
		t.Fatalf("Failed to marshal request: %v", err)
	}

	httpReq := httptest.NewRequest("POST", "/streamscape_api", bytes.NewBuffer(reqBody))
	httpReq.Header.Set("Content-Type", "application/json")

	rr := httptest.NewRecorder()
	server.HandleRequest(rr, httpReq)

	// Should return error
	if rr.Code != http.StatusBadRequest {
		t.Errorf("Expected status 400 for wrong JSON-RPC version, got %d", rr.Code)
	}
}

func TestServerHeaders(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Create valid request
	req := Request{
		JSONRPC: "2.0",
		Method:  "power_dBm",
		ID:      "test-1",
	}

	reqBody, err := json.Marshal(req)
	if err != nil {
		t.Fatalf("Failed to marshal request: %v", err)
	}

	httpReq := httptest.NewRequest("POST", "/streamscape_api", bytes.NewBuffer(reqBody))
	httpReq.Header.Set("Content-Type", "application/json")

	rr := httptest.NewRecorder()
	server.HandleRequest(rr, httpReq)

	// Check Content-Type header
	if rr.Header().Get("Content-Type") != "application/json" {
		t.Errorf("Expected Content-Type application/json, got %s", rr.Header().Get("Content-Type"))
	}

	// Server header should be empty (suppressed)
	if rr.Header().Get("Server") != "" {
		t.Errorf("Expected empty Server header, got %s", rr.Header().Get("Server"))
	}
}

func TestServerWithCustomServerHeader(t *testing.T) {
	cfg := createTestConfig()
	cfg.Network.HTTP.ServerHeader = "Custom-Server"
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	// Create valid request
	req := Request{
		JSONRPC: "2.0",
		Method:  "power_dBm",
		ID:      "test-1",
	}

	reqBody, err := json.Marshal(req)
	if err != nil {
		t.Fatalf("Failed to marshal request: %v", err)
	}

	httpReq := httptest.NewRequest("POST", "/streamscape_api", bytes.NewBuffer(reqBody))
	httpReq.Header.Set("Content-Type", "application/json")

	rr := httptest.NewRecorder()
	server.HandleRequest(rr, httpReq)

	// Check custom Server header
	if rr.Header().Get("Server") != "Custom-Server" {
		t.Errorf("Expected Server header 'Custom-Server', got %s", rr.Header().Get("Server"))
	}
}

func TestProcessRequestMethodRouting(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	tests := []struct {
		method       string
		params       []string
		expectedType string
	}{
		{"power_dBm", []string{"25"}, "setPower"},
		{"power_dBm", nil, "getPower"},
		{"freq", []string{"4700"}, "setFreq"},
		{"freq", nil, "getFreq"},
		{"supported_frequency_profiles", nil, "getProfiles"},
	}

	for _, tt := range tests {
		t.Run(tt.method, func(t *testing.T) {
			req := &Request{
				JSONRPC: "2.0",
				Method:  tt.method,
				Params:  tt.params,
				ID:      "test",
			}

			response := server.processRequest(req)

			if response.JSONRPC != "2.0" {
				t.Errorf("Expected JSONRPC 2.0, got %s", response.JSONRPC)
			}
			if response.ID != "test" {
				t.Errorf("Expected ID 'test', got %v", response.ID)
			}
		})
	}
}

func TestGetTimeoutForMethod(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	tests := []struct {
		method   string
		expected int // seconds
	}{
		{"freq", 30},                        // setChannel timeout
		{"power_dBm", 10},                   // setPower timeout
		{"supported_frequency_profiles", 5}, // read timeout
		{"unknown", 5},                      // default read timeout
	}

	for _, tt := range tests {
		t.Run(tt.method, func(t *testing.T) {
			timeout := server.getTimeoutForMethod(tt.method)
			expectedDuration := time.Duration(tt.expected) * time.Second
			if timeout != expectedDuration {
				t.Errorf("Expected timeout %v for method %s, got %v", expectedDuration, tt.method, timeout)
			}
		})
	}
}

func TestWriteErrorResponse(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	server := NewServer(cfg, radioState)
	defer radioState.Close()

	rr := httptest.NewRecorder()
	server.writeErrorResponse(rr, -32700, "Parse error", "test-id")

	// Check status code
	if rr.Code != http.StatusBadRequest {
		t.Errorf("Expected status 400, got %d", rr.Code)
	}

	// Parse response
	var response Response
	err := json.Unmarshal(rr.Body.Bytes(), &response)
	if err != nil {
		t.Fatalf("Failed to unmarshal error response: %v", err)
	}

	// Check error structure
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
	if errorObj["code"] != float64(-32700) {
		t.Errorf("Expected error code -32700, got %v", errorObj["code"])
	}
	if errorObj["message"] != "Parse error" {
		t.Errorf("Expected error message 'Parse error', got %v", errorObj["message"])
	}
}

// Helper functions
func createTestConfig() *config.Config {
	return &config.Config{
		Network: config.NetworkConfig{
			HTTP: config.HTTPConfig{
				Port:         8080,
				ServerHeader: "",
			},
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
			Commands: config.CommandsConfig{
				SetPower:    config.TimeoutConfig{TimeoutSec: 10},
				SetChannel:  config.TimeoutConfig{TimeoutSec: 30},
				SelectRadio: config.TimeoutConfig{TimeoutSec: 5},
				Read:        config.TimeoutConfig{TimeoutSec: 5},
			},
		},
		Mode: "normal",
	}
}

func createTestRadioState(cfg *config.Config) *state.RadioState {
	return state.NewRadioState(cfg)
}
