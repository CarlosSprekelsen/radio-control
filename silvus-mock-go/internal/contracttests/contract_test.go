package contracttests

import (
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

// loadGoldenFixture loads a JSON fixture from the fixtures directory
func loadGoldenFixture(t *testing.T, filename string, key string) map[string]interface{} {
	path := filepath.Join("fixtures", filename)
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("Failed to read fixture %s: %v", path, err)
	}

	var fixtures map[string]interface{}
	if err := json.Unmarshal(data, &fixtures); err != nil {
		t.Fatalf("Failed to unmarshal fixture %s: %v", path, err)
	}

	fixture, ok := fixtures[key].(map[string]interface{})
	if !ok {
		t.Fatalf("Fixture %s[%s] is not a map", filename, key)
	}

	return fixture
}

// TestContractCompliance runs all contract compliance tests
func TestContractCompliance(t *testing.T) {
	t.Run("HTTP_JSONRPC", func(t *testing.T) {
		t.Run("EnvelopeCompliance", TestHTTPEnvelopeCompliance)
		t.Run("MethodPOSTOnly", TestHTTPMethodPOSTOnly)
		t.Run("PathExactMatch", TestHTTPPathExactMatch)
		t.Run("CoreMethodsCompliance", TestHTTPCoreMethodsCompliance)
		t.Run("ErrorHandling", TestHTTPErrorHandling)
		t.Run("BlackoutBehavior", TestHTTPBlackoutBehavior)
	})

	t.Run("TCP_Maintenance", func(t *testing.T) {
		t.Run("MethodExistence", TestTCPMethodExistence)
		t.Run("JSONRPCCompliance", TestTCPJSONRPCCompliance)
		t.Run("LocalOnlyPolicy", TestTCPLocalOnlyPolicy)
		t.Run("RadioResetBlackoutInteraction", TestTCPRadioResetBlackoutInteraction)
		t.Run("UnknownMethod", TestTCPUnknownMethod)
	})

	t.Run("Validation", func(t *testing.T) {
		t.Run("JSONRPCEnvelope", TestJSONRPCEnvelopeValidation)
		t.Run("FrequencyProfile", TestFrequencyProfileValidation)
		t.Run("ErrorResponse", TestErrorResponseValidation)
	})
}

func TestJSONRPCEnvelopeValidation(t *testing.T) {
	tests := []struct {
		name    string
		json    string
		wantErr bool
	}{
		{
			name:    "valid_response",
			json:    `{"jsonrpc":"2.0","id":"test","result":["2490.0"]}`,
			wantErr: false,
		},
		{
			name:    "valid_error",
			json:    `{"jsonrpc":"2.0","id":"test","error":{"code":-32601,"message":"Method not found"}}`,
			wantErr: false,
		},
		{
			name:    "invalid_version",
			json:    `{"jsonrpc":"1.0","id":"test","result":["2490.0"]}`,
			wantErr: true,
		},
		{
			name:    "missing_id",
			json:    `{"jsonrpc":"2.0","result":["2490.0"]}`,
			wantErr: true,
		},
		{
			name:    "both_result_and_error",
			json:    `{"jsonrpc":"2.0","id":"test","result":["2490.0"],"error":{"code":-1,"message":"test"}}`,
			wantErr: true,
		},
		{
			name:    "neither_result_nor_error",
			json:    `{"jsonrpc":"2.0","id":"test"}`,
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := ValidateEnvelope([]byte(tt.json))
			if (err != nil) != tt.wantErr {
				t.Errorf("ValidateEnvelope() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

func TestFrequencyProfileValidation(t *testing.T) {
	tests := []struct {
		name    string
		profile map[string]interface{}
		wantErr bool
	}{
		{
			name: "valid_profile",
			profile: map[string]interface{}{
				"Frequencies": []interface{}{"2200:20:2380", "4700"},
				"Bandwidth":   "-1",
				"AntennaMask": "15",
			},
			wantErr: false,
		},
		{
			name: "valid_single_frequency",
			profile: map[string]interface{}{
				"Frequencies": []interface{}{"4700.0"},
				"Bandwidth":   "-1",
				"AntennaMask": "F",
			},
			wantErr: false,
		},
		{
			name: "missing_frequencies",
			profile: map[string]interface{}{
				"Bandwidth":   "-1",
				"AntennaMask": "15",
			},
			wantErr: true,
		},
		{
			name: "empty_frequencies",
			profile: map[string]interface{}{
				"Frequencies": []interface{}{},
				"Bandwidth":   "-1",
				"AntennaMask": "15",
			},
			wantErr: true,
		},
		{
			name: "invalid_frequency_format",
			profile: map[string]interface{}{
				"Frequencies": []interface{}{"2200:20"}, // Missing end
				"Bandwidth":   "-1",
				"AntennaMask": "15",
			},
			wantErr: true,
		},
		{
			name: "invalid_antenna_mask",
			profile: map[string]interface{}{
				"Frequencies": []interface{}{"4700.0"},
				"Bandwidth":   "-1",
				"AntennaMask": "G", // Invalid hex
			},
			wantErr: true,
		},
		{
			name: "valid_antenna_mask_dash_one",
			profile: map[string]interface{}{
				"Frequencies": []interface{}{"4700.0"},
				"Bandwidth":   "-1",
				"AntennaMask": "-1", // Special case
			},
			wantErr: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := ValidateFrequencyProfile(tt.profile)
			if (err != nil) != tt.wantErr {
				t.Errorf("ValidateFrequencyProfile() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

func TestErrorResponseValidation(t *testing.T) {
	tests := []struct {
		name    string
		json    string
		wantErr bool
	}{
		{
			name:    "valid_error",
			json:    `{"code":-32601,"message":"Method not found"}`,
			wantErr: false,
		},
		{
			name:    "valid_error_with_data",
			json:    `{"code":-32602,"message":"INVALID_RANGE","data":"Frequency out of range"}`,
			wantErr: false,
		},
		{
			name:    "missing_code",
			json:    `{"message":"Method not found"}`,
			wantErr: true,
		},
		{
			name:    "missing_message",
			json:    `{"code":-32601}`,
			wantErr: true,
		},
		{
			name:    "invalid_code_type",
			json:    `{"code":"invalid","message":"Method not found"}`,
			wantErr: true,
		},
		{
			name:    "invalid_message_type",
			json:    `{"code":-32601,"message":123}`,
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := ValidateErrorResponse([]byte(tt.json))
			if (err != nil) != tt.wantErr {
				t.Errorf("ValidateErrorResponse() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}
