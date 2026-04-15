package commands

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

// Helper to create a test config for extensible server
func createExtensibleTestConfig() *config.Config {
	return &config.Config{
		Mode: "normal",
		Power: config.PowerConfig{
			MinDBm: 0,
			MaxDBm: 39,
		},
		Timing: config.TimingConfig{
			Blackout: config.BlackoutConfig{
				SoftBootSec:    30,
				PowerChangeSec: 5,
				RadioResetSec:  60,
			},
		},
	}
}

// Helper to create an isolated RadioState for extensible server tests
func createExtensibleTestRadioState(cfg *config.Config) *state.RadioState {
	rs := state.NewRadioState(cfg)
	return rs
}

func TestExtensibleJSONRPCServerWithOptionalCommands(t *testing.T) {
	cfg := createExtensibleTestConfig()
	radioState := createExtensibleTestRadioState(cfg)
	defer radioState.Close()

	server := NewExtensibleJSONRPCServer(cfg, radioState)

	// Test optional commands
	tests := []struct {
		name           string
		method         string
		params         []string
		expectedResult interface{}
		expectedError  string
	}{
		{
			name:           "read_power_dBm",
			method:         "read_power_dBm",
			params:         []string{},
			expectedResult: []string{"28"}, // 30 - 2 = 28
			expectedError:  "",
		},
		{
			name:           "read_power_mw",
			method:         "read_power_mw",
			params:         []string{},
			expectedResult: nil, // Will check it's not empty
			expectedError:  "",
		},
		{
			name:           "max_link_distance",
			method:         "max_link_distance",
			params:         []string{},
			expectedResult: []string{"5000"},
			expectedError:  "",
		},
		{
			name:           "gps_coordinates",
			method:         "gps_coordinates",
			params:         []string{},
			expectedResult: nil, // Will check it's not empty
			expectedError:  "",
		},
		{
			name:           "gps_mode",
			method:         "gps_mode",
			params:         []string{},
			expectedResult: nil, // Will check it's not empty
			expectedError:  "",
		},
		{
			name:           "gps_time",
			method:         "gps_time",
			params:         []string{},
			expectedResult: nil, // Will be a timestamp, so we'll just check it's not empty
			expectedError:  "",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			req := Request{
				JSONRPC: "2.0",
				Method:  tt.method,
				Params:  tt.params,
				ID:      "test",
			}

			reqBody, err := json.Marshal(req)
			if err != nil {
				t.Fatalf("Failed to marshal request: %v", err)
			}

			httpReq := httptest.NewRequest("POST", "/streamscape_api", bytes.NewBuffer(reqBody))
			httpReq.Header.Set("Content-Type", "application/json")

			rr := httptest.NewRecorder()
			server.HandleRequest(rr, httpReq)

			if rr.Code != http.StatusOK {
				t.Errorf("Expected status %d, got %d", http.StatusOK, rr.Code)
			}

			var resp Response
			err = json.Unmarshal(rr.Body.Bytes(), &resp)
			if err != nil {
				t.Fatalf("Failed to unmarshal response: %v", err)
			}

			if tt.expectedError != "" {
				if resp.Error == nil {
					t.Errorf("Expected error '%s', got no error", tt.expectedError)
				} else {
					// Check if error is a string or an object
					if errorStr, ok := resp.Error.(string); ok {
						if errorStr != tt.expectedError {
							t.Errorf("Expected error '%s', got '%s'", tt.expectedError, errorStr)
						}
					} else if errorObj, ok := resp.Error.(map[string]interface{}); ok {
						if message, exists := errorObj["message"]; exists {
							if message != tt.expectedError {
								t.Errorf("Expected error '%s', got '%s'", tt.expectedError, message)
							}
						}
					}
				}
			} else {
				if resp.Error != nil {
					t.Errorf("Expected no error, got %v", resp.Error)
				}

				if tt.expectedResult != nil {
					// For commands that return specific values, check them
					if tt.method == "read_power_dBm" || tt.method == "max_link_distance" {
						resultSlice, ok := resp.Result.([]interface{})
						if !ok {
							t.Errorf("Result is not a slice of interfaces: %v", resp.Result)
						} else {
							expectedSlice, ok := tt.expectedResult.([]string)
							if !ok {
								t.Errorf("Expected result is not a slice of strings: %v", tt.expectedResult)
							} else {
								if len(resultSlice) != len(expectedSlice) {
									t.Errorf("Expected result length %d, got %d", len(expectedSlice), len(resultSlice))
								} else {
									for i := range resultSlice {
										if resultSlice[i] != expectedSlice[i] {
											t.Errorf("Expected result[%d] '%v', got '%v'", i, expectedSlice[i], resultSlice[i])
										}
									}
								}
							}
						}
					} else {
						// For other commands, just check result is not empty
						if resp.Result == nil {
							t.Errorf("Expected result for %s, got nil", tt.method)
						}
					}
				} else {
					// For commands with nil expected result, just check it's not empty
					if resp.Result == nil {
						t.Errorf("Expected result for %s, got nil", tt.method)
					}
				}
			}
		})
	}
}

func TestExtensibleJSONRPCServerInvalidCommand(t *testing.T) {
	cfg := createExtensibleTestConfig()
	radioState := createExtensibleTestRadioState(cfg)
	defer radioState.Close()

	server := NewExtensibleJSONRPCServer(cfg, radioState)

	req := Request{
		JSONRPC: "2.0",
		Method:  "invalid_command",
		Params:  []string{},
		ID:      "test",
	}

	reqBody, err := json.Marshal(req)
	if err != nil {
		t.Fatalf("Failed to marshal request: %v", err)
	}

	httpReq := httptest.NewRequest("POST", "/streamscape_api", bytes.NewBuffer(reqBody))
	httpReq.Header.Set("Content-Type", "application/json")

	rr := httptest.NewRecorder()
	server.HandleRequest(rr, httpReq)

	if rr.Code != http.StatusOK {
		t.Errorf("Expected status %d, got %d", http.StatusOK, rr.Code)
	}

	var resp Response
	err = json.Unmarshal(rr.Body.Bytes(), &resp)
	if err != nil {
		t.Fatalf("Failed to unmarshal response: %v", err)
	}

	if resp.Error == nil {
		t.Error("Expected error for invalid command, got no error")
	} else {
		// Check if error is a string or an object
		if errorStr, ok := resp.Error.(string); ok {
			if errorStr != "Method not found" {
				t.Errorf("Expected error 'Method not found', got '%s'", errorStr)
			}
		} else if errorObj, ok := resp.Error.(map[string]interface{}); ok {
			if message, exists := errorObj["message"]; exists {
				if message != "Method not found" {
					t.Errorf("Expected error 'Method not found', got '%s'", message)
				}
			}
		}
	}
}

func TestExtensibleJSONRPCServerGetAvailableCommands(t *testing.T) {
	cfg := createExtensibleTestConfig()
	radioState := createExtensibleTestRadioState(cfg)
	defer radioState.Close()

	server := NewExtensibleJSONRPCServer(cfg, radioState)

	commands := server.GetAvailableCommands()

	// Check that we have core commands
	expectedCoreCommands := []string{
		"freq",
		"power_dBm",
		"supported_frequency_profiles",
	}

	// Check that we have optional commands
	expectedOptionalCommands := []string{
		"read_power_dBm",
		"read_power_mw",
		"max_link_distance",
	}

	// Check that we have GPS commands
	expectedGPSCommands := []string{
		"gps_coordinates",
		"gps_mode",
		"gps_time",
	}

	allExpected := append(append(expectedCoreCommands, expectedOptionalCommands...), expectedGPSCommands...)

	for _, expected := range allExpected {
		found := false
		for _, cmd := range commands {
			if cmd.Name == expected {
				found = true
				break
			}
		}
		if !found {
			t.Errorf("Expected command '%s' not found in available commands", expected)
		}
	}

	if len(commands) < len(allExpected) {
		t.Errorf("Expected at least %d commands, got %d", len(allExpected), len(commands))
	}
}

func TestExtensibleJSONRPCServerAddCustomCommand(t *testing.T) {
	cfg := createExtensibleTestConfig()
	radioState := createExtensibleTestRadioState(cfg)
	defer radioState.Close()

	server := NewExtensibleJSONRPCServer(cfg, radioState)

	// Add a custom command
	customHandler := NewCustomCommandHandler(
		"custom_status",
		"Get custom system status",
		true,  // read-only
		false, // no blackout
		func(ctx context.Context, params []string) (interface{}, error) {
			return []string{"operational", "24h"}, nil
		},
	)
	server.AddCustomCommand(customHandler)

	// Test the custom command
	req := Request{
		JSONRPC: "2.0",
		Method:  "custom_status",
		Params:  []string{},
		ID:      "test",
	}

	reqBody, err := json.Marshal(req)
	if err != nil {
		t.Fatalf("Failed to marshal request: %v", err)
	}

	httpReq := httptest.NewRequest("POST", "/streamscape_api", bytes.NewBuffer(reqBody))
	httpReq.Header.Set("Content-Type", "application/json")

	rr := httptest.NewRecorder()
	server.HandleRequest(rr, httpReq)

	if rr.Code != http.StatusOK {
		t.Errorf("Expected status %d, got %d", http.StatusOK, rr.Code)
	}

	var resp Response
	err = json.Unmarshal(rr.Body.Bytes(), &resp)
	if err != nil {
		t.Fatalf("Failed to unmarshal response: %v", err)
	}

	if resp.Error != nil {
		t.Errorf("Expected no error, got %v", resp.Error)
	}

	if resp.Result == nil {
		t.Error("Expected result for custom command, got nil")
	}

	// Verify the custom command is in available commands
	commands := server.GetAvailableCommands()
	found := false
	for _, cmd := range commands {
		if cmd.Name == "custom_status" {
			found = true
			break
		}
	}
	if !found {
		t.Error("Custom command not found in available commands")
	}
}
