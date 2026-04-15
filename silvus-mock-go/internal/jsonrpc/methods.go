package jsonrpc

import (
	"strconv"
	"strings"
)

// MethodHandler handles specific JSON-RPC methods
type MethodHandler struct {
	server *Server
}

// NewMethodHandler creates a new method handler
func NewMethodHandler(server *Server) *MethodHandler {
	return &MethodHandler{
		server: server,
	}
}

// ValidateFrequency validates a frequency string against supported profiles
func (mh *MethodHandler) ValidateFrequency(freqStr string) bool {
	freq, err := strconv.ParseFloat(freqStr, 64)
	if err != nil {
		return false
	}

	// Check against all frequency profiles
	for _, profile := range mh.server.config.Profiles.FrequencyProfiles {
		if mh.frequencyMatchesProfile(freq, profile) {
			return true
		}
	}
	return false
}

// frequencyMatchesProfile checks if a frequency matches a profile
func (mh *MethodHandler) frequencyMatchesProfile(freq float64, profile interface{}) bool {
	// This is a simplified version - in practice, you'd need to parse the profile structure
	// For now, we'll implement basic range checking
	return true // Placeholder - implement proper validation
}

// ValidatePower validates a power value against limits
func (mh *MethodHandler) ValidatePower(powerStr string) bool {
	power, err := strconv.Atoi(powerStr)
	if err != nil {
		return false
	}

	return power >= mh.server.config.Power.MinDBm && power <= mh.server.config.Power.MaxDBm
}

// ParseFrequencyRange parses a frequency range string
func (mh *MethodHandler) ParseFrequencyRange(freqRange string) (start, step, end float64, err error) {
	parts := strings.Split(freqRange, ":")
	if len(parts) != 3 {
		err = &ParseError{Message: "Invalid frequency range format"}
		return
	}

	start, err1 := strconv.ParseFloat(parts[0], 64)
	step, err2 := strconv.ParseFloat(parts[1], 64)
	end, err3 := strconv.ParseFloat(parts[2], 64)

	if err1 != nil || err2 != nil || err3 != nil {
		err = &ParseError{Message: "Invalid frequency values"}
		return
	}

	if step <= 0 {
		err = &ParseError{Message: "Step must be positive"}
		return
	}

	return start, step, end, nil
}

// ParseError represents a parsing error
type ParseError struct {
	Message string
}

func (e *ParseError) Error() string {
	return e.Message
}
