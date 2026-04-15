package contracttests

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/jsonrpc"
	"github.com/silvus-mock/internal/state"
)

// TestServer wraps the emulator for contract testing
type TestServer struct {
	server   *httptest.Server
	radioState *state.RadioState
	config   *config.Config
}

// NewTestServer creates a test server with development configuration
func NewTestServer(t *testing.T) *TestServer {
	// Create test configuration with tight timing for unit tests
	cfg := &config.Config{
		Network: config.NetworkConfig{
			HTTP: config.HTTPConfig{
				Port:         8080,
				ServerHeader: "",
				DevMode:      true,
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
				{
					Frequencies: []string{"4420:40:4700"},
					Bandwidth:   "-1",
					AntennaMask: "3",
				},
				{
					Frequencies: []string{"4700:20:4980"},
					Bandwidth:   "-1",
					AntennaMask: "12",
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
	jsonrpcServer := jsonrpc.NewServer(cfg, radioState)

	// Create HTTP mux
	mux := http.NewServeMux()
	mux.HandleFunc("/streamscape_api", jsonrpcServer.HandleRequest)

	server := httptest.NewServer(mux)

	return &TestServer{
		server:     server,
		radioState: radioState,
		config:     cfg,
	}
}

// Close shuts down the test server
func (ts *TestServer) Close() {
	ts.server.Close()
}

// PostJSON sends a JSON-RPC request to the test server
func (ts *TestServer) PostJSON(t *testing.T, request map[string]interface{}) (*http.Response, []byte) {
	jsonData, err := json.Marshal(request)
	if err != nil {
		t.Fatalf("Failed to marshal request: %v", err)
	}

	resp, err := http.Post(ts.server.URL+"/streamscape_api", "application/json", bytes.NewBuffer(jsonData))
	if err != nil {
		t.Fatalf("Failed to send request: %v", err)
	}

	body, err := io.ReadAll(resp.Body)
	resp.Body.Close()
	if err != nil {
		t.Fatalf("Failed to read response body: %v", err)
	}

	return resp, body
}


func TestHTTPEnvelopeCompliance(t *testing.T) {
	server := NewTestServer(t)
	defer server.Close()

	tests := []struct {
		name     string
		fixture  string
		key      string
		expected map[string]interface{}
	}{
		{
			name:     "freq_read",
			fixture:  "golden_requests.json",
			key:      "freq_read",
			expected: loadGoldenFixture(t, "golden_responses.json", "freq_read_success"),
		},
		{
			name:     "power_read", 
			fixture:  "golden_requests.json",
			key:      "power_read",
			expected: loadGoldenFixture(t, "golden_responses.json", "power_read_success"),
		},
		{
			name:     "profiles_read",
			fixture:  "golden_requests.json", 
			key:      "profiles_read",
			expected: loadGoldenFixture(t, "golden_responses.json", "profiles_success"),
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			request := loadGoldenFixture(t, tt.fixture, tt.key)
			
			resp, body := server.PostJSON(t, request)
			
			// Assert HTTP 200
			if resp.StatusCode != http.StatusOK {
				t.Errorf("Expected HTTP 200, got %d", resp.StatusCode)
			}

			// Assert Content-Type
			contentType := resp.Header.Get("Content-Type")
			if contentType != "application/json" {
				t.Errorf("Expected Content-Type application/json, got %s", contentType)
			}

			// Validate JSON-RPC envelope
			if err := ValidateEnvelope(body); err != nil {
				t.Errorf("Invalid JSON-RPC envelope: %v", err)
			}

			// Compare with expected response
			expectedJSON, err := json.Marshal(tt.expected)
			if err != nil {
				t.Fatalf("Failed to marshal expected response: %v", err)
			}

			if err := CompareEnvelopes(expectedJSON, body); err != nil {
				t.Errorf("Response envelope mismatch: %v", err)
			}
		})
	}
}

func TestHTTPMethodPOSTOnly(t *testing.T) {
	server := NewTestServer(t)
	defer server.Close()

	request := loadGoldenFixture(t, "golden_requests.json", "freq_read")

	// Test GET request (should fail)
	resp, err := http.Get(server.server.URL + "/streamscape_api")
	if err != nil {
		t.Fatalf("Failed to send GET request: %v", err)
	}
	resp.Body.Close()

	// GET should return method not allowed or similar error
	if resp.StatusCode == http.StatusOK {
		t.Errorf("GET request should not return 200 OK")
	}

	// Test POST request (should succeed)
	resp, body := server.PostJSON(t, request)
	if resp.StatusCode != http.StatusOK {
		t.Errorf("POST request should return 200 OK, got %d", resp.StatusCode)
	}

	// Validate response
	if err := ValidateEnvelope(body); err != nil {
		t.Errorf("Invalid JSON-RPC envelope: %v", err)
	}
}

func TestHTTPPathExactMatch(t *testing.T) {
	server := NewTestServer(t)
	defer server.Close()

	request := loadGoldenFixture(t, "golden_requests.json", "freq_read")
	jsonData, _ := json.Marshal(request)

	// Test wrong path
	resp, err := http.Post(server.server.URL+"/wrong_path", "application/json", bytes.NewBuffer(jsonData))
	if err != nil {
		t.Fatalf("Failed to send request to wrong path: %v", err)
	}
	resp.Body.Close()

	if resp.StatusCode == http.StatusOK {
		t.Errorf("Wrong path should not return 200 OK")
	}

	// Test correct path
	resp, body := server.PostJSON(t, request)
	if resp.StatusCode != http.StatusOK {
		t.Errorf("Correct path should return 200 OK, got %d", resp.StatusCode)
	}

	if err := ValidateEnvelope(body); err != nil {
		t.Errorf("Invalid JSON-RPC envelope: %v", err)
	}
}

func TestHTTPCoreMethodsCompliance(t *testing.T) {
	// Don't reuse server across tests to avoid blackout interference

	tests := []struct {
		name     string
		request  string
		expected string
		validate func(t *testing.T, response map[string]interface{})
	}{
		{
			name:     "freq_set",
			request:  "freq_set",
			expected: "freq_set_success",
			validate: func(t *testing.T, response map[string]interface{}) {
				result, ok := response["result"]
				if !ok {
					t.Errorf("Expected result field")
					return
				}
				
				// Result should be an array or slice
				resultSlice, ok := result.([]interface{})
				if !ok {
					// Try to handle case where result is []string
					if resultArray, ok := result.([]string); ok {
						if len(resultArray) == 0 {
							return // Empty slice is correct for set operations
						}
						if len(resultArray) == 1 && resultArray[0] == "" {
							return // Single empty string is also correct
						}
						t.Errorf("Expected result to be empty slice or [\"\"], got %v", resultArray)
						return
					}
					t.Errorf("Expected result to be array, got %T: %v", result, result)
					return
				}
				
				// For set operations, we expect either empty slice or [""
				if len(resultSlice) == 0 {
					return // Empty slice is correct
				}
				if len(resultSlice) == 1 && resultSlice[0] == "" {
					return // Single empty string is also correct
				}
				t.Errorf("Expected result to be empty slice or [\"\"], got %v", resultSlice)
			},
		},
		{
			name:     "power_set",
			request:  "power_set", 
			expected: "power_set_success",
			validate: func(t *testing.T, response map[string]interface{}) {
				result, ok := response["result"]
				if !ok {
					t.Errorf("Expected result field")
					return
				}
				
				// Result should be an array or slice
				resultSlice, ok := result.([]interface{})
				if !ok {
					// Try to handle case where result is []string
					if resultArray, ok := result.([]string); ok {
						if len(resultArray) == 0 {
							return // Empty slice is correct for set operations
						}
						if len(resultArray) == 1 && resultArray[0] == "" {
							return // Single empty string is also correct
						}
						t.Errorf("Expected result to be empty slice or [\"\"], got %v", resultArray)
						return
					}
					t.Errorf("Expected result to be array, got %T: %v", result, result)
					return
				}
				
				// For set operations, we expect either empty slice or [""
				if len(resultSlice) == 0 {
					return // Empty slice is correct
				}
				if len(resultSlice) == 1 && resultSlice[0] == "" {
					return // Single empty string is also correct
				}
				t.Errorf("Expected result to be empty slice or [\"\"], got %v", resultSlice)
			},
		},
		{
			name:     "profiles_structure",
			request:  "profiles_read",
			expected: "profiles_success", 
			validate: func(t *testing.T, response map[string]interface{}) {
				result, ok := response["result"]
				if !ok {
					t.Errorf("Expected result field")
					return
				}
				
				// Result should be an array or slice
				resultSlice, ok := result.([]interface{})
				if !ok {
					t.Errorf("Expected result to be array, got %T: %v", result, result)
					return
				}
				
				for i, profile := range resultSlice {
					profileMap, ok := profile.(map[string]interface{})
					if !ok {
						t.Errorf("Profile %d is not a map", i)
						continue
					}
					
					if err := ValidateFrequencyProfile(profileMap); err != nil {
						t.Errorf("Profile %d validation failed: %v", i, err)
					}
				}
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create fresh server for each test to avoid blackout interference
			server := NewTestServer(t)
			defer server.Close()
			
			request := loadGoldenFixture(t, "golden_requests.json", tt.request)
			
			resp, body := server.PostJSON(t, request)
			
			if resp.StatusCode != http.StatusOK {
				t.Errorf("Expected HTTP 200, got %d", resp.StatusCode)
				return
			}

			var response map[string]interface{}
			if err := json.Unmarshal(body, &response); err != nil {
				t.Errorf("Failed to unmarshal response: %v", err)
				return
			}

			tt.validate(t, response)
		})
	}
}

func TestHTTPErrorHandling(t *testing.T) {
	server := NewTestServer(t)
	defer server.Close()

	tests := []struct {
		name     string
		request  string
		expected string
	}{
		{
			name:     "unknown_method",
			request:  "unknown_method",
			expected: "method_not_found",
		},
		{
			name:     "invalid_freq",
			request:  "invalid_freq", 
			expected: "invalid_range",
		},
		{
			name:     "invalid_power",
			request:  "invalid_power",
			expected: "invalid_range", 
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			request := loadGoldenFixture(t, "golden_requests.json", tt.request)
			
			resp, body := server.PostJSON(t, request)
			
			// HTTP should still be 200 for JSON-RPC errors
			if resp.StatusCode != http.StatusOK {
				t.Errorf("Expected HTTP 200 for JSON-RPC error, got %d", resp.StatusCode)
			}

			var response map[string]interface{}
			if err := json.Unmarshal(body, &response); err != nil {
				t.Errorf("Failed to unmarshal response: %v", err)
				return
			}

			// Should have error field, not result
			if _, hasError := response["error"]; !hasError {
				t.Errorf("Expected error field in response")
			}
			if _, hasResult := response["result"]; hasResult {
				t.Errorf("Should not have result field when error is present")
			}

			// Validate error structure
			errorData, _ := json.Marshal(response["error"])
			if err := ValidateErrorResponse(errorData); err != nil {
				t.Errorf("Invalid error response: %v", err)
			}
		})
	}
}

func TestHTTPBlackoutBehavior(t *testing.T) {
	server := NewTestServer(t)
	defer server.Close()

	// Trigger a blackout by setting frequency
	setRequest := loadGoldenFixture(t, "golden_requests.json", "freq_set")
	resp, _ := server.PostJSON(t, setRequest)
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("Failed to set frequency: %d", resp.StatusCode)
	}

	// Immediately try to read during blackout
	readRequest := loadGoldenFixture(t, "golden_requests.json", "freq_read")
	resp, body := server.PostJSON(t, readRequest)

	if resp.StatusCode != http.StatusOK {
		t.Errorf("Expected HTTP 200 during blackout, got %d", resp.StatusCode)
	}

	var response map[string]interface{}
	if err := json.Unmarshal(body, &response); err != nil {
		t.Errorf("Failed to unmarshal response: %v", err)
		return
	}

	// Should have error field with UNAVAILABLE
	if _, hasError := response["error"]; !hasError {
		t.Errorf("Expected error field during blackout")
	}

	errorData, _ := json.Marshal(response["error"])
	if err := ValidateErrorResponse(errorData); err != nil {
		t.Errorf("Invalid error response: %v", err)
	}

	// Wait for blackout to end
	time.Sleep(2 * time.Second)

	// Try read again - should succeed
	resp, body = server.PostJSON(t, readRequest)
	if resp.StatusCode != http.StatusOK {
		t.Errorf("Expected HTTP 200 after blackout, got %d", resp.StatusCode)
	}

	if err := json.Unmarshal(body, &response); err != nil {
		t.Errorf("Failed to unmarshal response: %v", err)
		return
	}

	// Should have result field now
	if _, hasResult := response["result"]; !hasResult {
		t.Errorf("Expected result field after blackout")
	}
}
