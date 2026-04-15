package contracttests

import (
	"encoding/json"
	"fmt"
	"strings"
)

// JSONRPCEnvelope validates JSON-RPC 2.0 envelope structure
type JSONRPCEnvelope struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      interface{}     `json:"id"`
	Method  string          `json:"method,omitempty"`
	Params  json.RawMessage `json:"params,omitempty"`
	Result  json.RawMessage `json:"result,omitempty"`
	Error   json.RawMessage `json:"error,omitempty"`
}

// ValidateEnvelope validates JSON-RPC 2.0 envelope compliance
func ValidateEnvelope(data []byte) error {
	var envelope JSONRPCEnvelope
	if err := json.Unmarshal(data, &envelope); err != nil {
		return fmt.Errorf("invalid JSON: %w", err)
	}

	// Check jsonrpc version
	if envelope.JSONRPC != "2.0" {
		return fmt.Errorf("jsonrpc must be '2.0', got '%s'", envelope.JSONRPC)
	}

	// Check id is present
	if envelope.ID == nil {
		return fmt.Errorf("id field is required")
	}

	// Check mutual exclusivity of result and error
	hasResult := len(envelope.Result) > 0
	hasError := len(envelope.Error) > 0

	if hasResult && hasError {
		return fmt.Errorf("both result and error cannot be present")
	}

	if !hasResult && !hasError {
		return fmt.Errorf("either result or error must be present")
	}

	return nil
}

// ValidateFrequencyProfile validates the structure of a frequency profile object
func ValidateFrequencyProfile(profile map[string]interface{}) error {
	// Check required fields (note: JSON field names are case-sensitive)
	frequencies, ok := profile["Frequencies"].([]interface{})
	if !ok {
		return fmt.Errorf("Frequencies field must be an array")
	}

	if len(frequencies) == 0 {
		return fmt.Errorf("frequencies array cannot be empty")
	}

	// Validate frequency format (range or single)
	for i, freq := range frequencies {
		freqStr, ok := freq.(string)
		if !ok {
			return fmt.Errorf("frequency %d must be a string", i)
		}

		// Check for range format (e.g., "2200:20:2380") or single format (e.g., "4700")
		if strings.Contains(freqStr, ":") {
			parts := strings.Split(freqStr, ":")
			if len(parts) != 3 {
				return fmt.Errorf("frequency range format must be 'start:step:end', got '%s'", freqStr)
			}
		}
		// Single frequency format is just a string number, which is valid
	}

	_, ok = profile["Bandwidth"].(string)
	if !ok {
		return fmt.Errorf("Bandwidth field must be a string")
	}

	antennaMask, ok := profile["AntennaMask"].(string)
	if !ok {
		return fmt.Errorf("AntennaMask field must be a string")
	}

	// Validate antenna_mask is hex (1-F)
	if antennaMask != "-1" && (len(antennaMask) == 0 || !isValidHexMask(antennaMask)) {
		return fmt.Errorf("antenna_mask must be valid hex (1-F) or '-1', got '%s'", antennaMask)
	}

	return nil
}

// isValidHexMask checks if a string is a valid hex mask (1-F)
func isValidHexMask(mask string) bool {
	if len(mask) == 0 {
		return false
	}

	for _, char := range mask {
		if !((char >= '1' && char <= '9') || (char >= 'A' && char <= 'F')) {
			return false
		}
	}
	return true
}

// ValidateArrayStringResult validates that a result is an array of strings
func ValidateArrayStringResult(result json.RawMessage) error {
	var arr []string
	if err := json.Unmarshal(result, &arr); err != nil {
		return fmt.Errorf("result must be array of strings: %w", err)
	}
	return nil
}

// ValidateArrayObjectResult validates that a result is an array of objects
func ValidateArrayObjectResult(result json.RawMessage) error {
	var arr []interface{}
	if err := json.Unmarshal(result, &arr); err != nil {
		return fmt.Errorf("result must be array of objects: %w", err)
	}
	return nil
}

// ValidateErrorResponse validates JSON-RPC error structure
func ValidateErrorResponse(errorData json.RawMessage) error {
	var errorObj map[string]interface{}
	if err := json.Unmarshal(errorData, &errorObj); err != nil {
		return fmt.Errorf("error must be an object: %w", err)
	}

	code, hasCode := errorObj["code"]
	if !hasCode {
		return fmt.Errorf("error object must have 'code' field")
	}

	message, hasMessage := errorObj["message"]
	if !hasMessage {
		return fmt.Errorf("error object must have 'message' field")
	}

	// Validate code is numeric
	if _, ok := code.(float64); !ok {
		return fmt.Errorf("error code must be numeric")
	}

	// Validate message is string
	if _, ok := message.(string); !ok {
		return fmt.Errorf("error message must be string")
	}

	return nil
}

// CompareEnvelopes compares two JSON-RPC envelopes for structural equality
func CompareEnvelopes(expected, actual []byte) error {
	// Validate both envelopes first
	if err := ValidateEnvelope(expected); err != nil {
		return fmt.Errorf("expected envelope invalid: %w", err)
	}
	if err := ValidateEnvelope(actual); err != nil {
		return fmt.Errorf("actual envelope invalid: %w", err)
	}

	var expEnv, actEnv JSONRPCEnvelope
	if err := json.Unmarshal(expected, &expEnv); err != nil {
		return fmt.Errorf("failed to unmarshal expected: %w", err)
	}
	if err := json.Unmarshal(actual, &actEnv); err != nil {
		return fmt.Errorf("failed to unmarshal actual: %w", err)
	}

	// Compare jsonrpc version
	if expEnv.JSONRPC != actEnv.JSONRPC {
		return fmt.Errorf("jsonrpc version mismatch: expected '%s', got '%s'", expEnv.JSONRPC, actEnv.JSONRPC)
	}

	// Compare id (allowing for different types but same value)
	if fmt.Sprintf("%v", expEnv.ID) != fmt.Sprintf("%v", actEnv.ID) {
		return fmt.Errorf("id mismatch: expected '%v', got '%v'", expEnv.ID, actEnv.ID)
	}

	// Compare method if present
	if expEnv.Method != actEnv.Method {
		return fmt.Errorf("method mismatch: expected '%s', got '%s'", expEnv.Method, actEnv.Method)
	}

	// Compare result or error
	if len(expEnv.Result) > 0 {
		if len(actEnv.Result) == 0 {
			return fmt.Errorf("expected result but got none")
		}
		// For result comparison, we'll do semantic comparison in specific tests
	} else if len(expEnv.Error) > 0 {
		if len(actEnv.Error) == 0 {
			return fmt.Errorf("expected error but got none")
		}
		// For error comparison, we'll do semantic comparison in specific tests
	}

	return nil
}
