package state

import (
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/silvus-mock/internal/config"
)

func TestNewRadioState(t *testing.T) {
	cfg := &config.Config{
		Mode: "normal",
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
	}

	rs := NewRadioState(cfg)
	defer rs.Close()

	// Test initial state
	if rs.currentFreq != "2490.0" {
		t.Errorf("Expected initial frequency 2490.0, got %s", rs.currentFreq)
	}
	if rs.currentPower != 30 {
		t.Errorf("Expected initial power 30, got %d", rs.currentPower)
	}
	if rs.mode != "normal" {
		t.Errorf("Expected mode 'normal', got '%s'", rs.mode)
	}
}

func TestExecuteCommandSetPower(t *testing.T) {
	tests := []struct {
		name      string
		params    []string
		wantErr   string
		wantPower int
	}{
		{
			name:      "valid power",
			params:    []string{"25"},
			wantErr:   "",
			wantPower: 25,
		},
		{
			name:      "power too low",
			params:    []string{"-1"},
			wantErr:   "INVALID_RANGE",
			wantPower: 30, // unchanged
		},
		{
			name:      "power too high",
			params:    []string{"50"},
			wantErr:   "INVALID_RANGE",
			wantPower: 30, // unchanged
		},
		{
			name:      "invalid format",
			params:    []string{"abc"},
			wantErr:   "INVALID_RANGE",
			wantPower: 30, // unchanged
		},
		{
			name:      "wrong param count",
			params:    []string{"25", "extra"},
			wantErr:   "INTERNAL",
			wantPower: 30, // unchanged
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			rs := createTestRadioState()
			defer rs.Close()

			// Wait for any blackout to clear
			time.Sleep(6 * time.Second)

			response := rs.ExecuteCommand("setPower", tt.params)

			if response.Error != tt.wantErr {
				t.Errorf("ExecuteCommand() error = %v, want %v", response.Error, tt.wantErr)
			}

			if tt.wantErr == "" && response.Result == nil {
				t.Error("Expected result for successful command")
			}

			// Check power was set correctly
			readResponse := rs.ExecuteCommand("getPower", []string{})
			if readResponse.Error != "" {
				t.Errorf("Failed to read power: %v", readResponse.Error)
			} else {
				result := readResponse.Result.([]string)
				if len(result) != 1 {
					t.Errorf("Expected single power value, got %v", result)
				} else {
					power := result[0]
					if tt.wantPower == 30 && power != "30" {
						// Power should be unchanged for error cases
						t.Errorf("Expected unchanged power 30, got %s", power)
					}
				}
			}
		})
	}
}

func TestExecuteCommandGetPower(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	response := rs.ExecuteCommand("getPower", []string{})

	if response.Error != "" {
		t.Errorf("ExecuteCommand() error = %v", response.Error)
	}

	if response.Result == nil {
		t.Error("Expected result for getPower")
	}

	result := response.Result.([]string)
	if len(result) != 1 {
		t.Errorf("Expected single power value, got %v", result)
	}
	if result[0] != "30" {
		t.Errorf("Expected power 30, got %s", result[0])
	}
}

func TestExecuteCommandSetFreq(t *testing.T) {

	tests := []struct {
		name    string
		params  []string
		wantErr string
	}{
		{
			name:    "valid single frequency",
			params:  []string{"4700"},
			wantErr: "",
		},
		{
			name:    "valid range frequency",
			params:  []string{"2220"},
			wantErr: "",
		},
		{
			name:    "invalid frequency",
			params:  []string{"9999"},
			wantErr: "INVALID_RANGE",
		},
		{
			name:    "invalid format",
			params:  []string{"abc"},
			wantErr: "INVALID_RANGE",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			rs := createTestRadioState()
			defer rs.Close()

			// Wait for any blackout to clear
			time.Sleep(6 * time.Second)

			response := rs.ExecuteCommand("setFreq", tt.params)

			if response.Error != tt.wantErr {
				t.Errorf("ExecuteCommand() error = %v, want %v", response.Error, tt.wantErr)
			}

			if tt.wantErr == "" && response.Result == nil {
				t.Error("Expected result for successful command")
			}
		})
	}
}

func TestExecuteCommandGetFreq(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	response := rs.ExecuteCommand("getFreq", []string{})

	if response.Error != "" {
		t.Errorf("ExecuteCommand() error = %v", response.Error)
	}

	if response.Result == nil {
		t.Error("Expected result for getFreq")
	}

	result := response.Result.([]string)
	if len(result) != 1 {
		t.Errorf("Expected single frequency value, got %v", result)
	}
	if result[0] != "2490.0" {
		t.Errorf("Expected frequency 2490.0, got %s", result[0])
	}
}

func TestExecuteCommandGetProfiles(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	response := rs.ExecuteCommand("getProfiles", []string{})

	if response.Error != "" {
		t.Errorf("ExecuteCommand() error = %v", response.Error)
	}

	if response.Result == nil {
		t.Error("Expected result for getProfiles")
	}

	result := response.Result.([]config.FrequencyProfile)
	if len(result) == 0 {
		t.Error("Expected at least one frequency profile")
	}
}

func TestSoftBootBlackout(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	// Set frequency to trigger soft boot
	response := rs.ExecuteCommand("setFreq", []string{"4700"})
	if response.Error != "" {
		t.Fatalf("Failed to set frequency: %v", response.Error)
	}

	// Should be in blackout now
	if rs.IsAvailable() {
		t.Error("Expected radio to be unavailable during blackout")
	}

	// Commands should return BUSY during blackout
	busyResponse := rs.ExecuteCommand("setPower", []string{"25"})
	if busyResponse.Error != "BUSY" {
		t.Errorf("Expected BUSY during blackout, got %v", busyResponse.Error)
	}

	// Wait for blackout to clear (plus small buffer)
	time.Sleep(6 * time.Second)

	// Should be available again
	if !rs.IsAvailable() {
		t.Error("Expected radio to be available after blackout")
	}

	// Commands should work again
	response = rs.ExecuteCommand("setPower", []string{"25"})
	if response.Error != "" {
		t.Errorf("Expected successful command after blackout, got %v", response.Error)
	}
}

func TestIsValidFrequency(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	tests := []struct {
		freq      string
		wantValid bool
	}{
		{"4700", true},  // exact match
		{"2220", true},  // within range
		{"2200", true},  // start of range
		{"2380", true},  // end of range
		{"2490", false}, // outside range
		{"9999", false}, // way outside
		{"abc", false},  // invalid format
		{"", false},     // empty
	}

	for _, tt := range tests {
		t.Run(tt.freq, func(t *testing.T) {
			valid := rs.isValidFrequency(tt.freq)
			if valid != tt.wantValid {
				t.Errorf("isValidFrequency(%s) = %v, want %v", tt.freq, valid, tt.wantValid)
			}
		})
	}
}

func TestFrequencyInRange(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	tests := []struct {
		freq      float64
		freqRange string
		wantValid bool
	}{
		{4700, "4700", true},          // exact single frequency
		{2220, "2200:20:2380", true},  // within range
		{2200, "2200:20:2380", true},  // start of range
		{2380, "2200:20:2380", true},  // end of range
		{2490, "2200:20:2380", false}, // outside range
		{4700, "2200:20:2380", false}, // outside range
		{2220, "4700", false},         // wrong single frequency
	}

	for _, tt := range tests {
		t.Run("", func(t *testing.T) {
			valid := rs.frequencyInRange(tt.freq, tt.freqRange)
			if valid != tt.wantValid {
				t.Errorf("frequencyInRange(%.1f, %s) = %v, want %v", tt.freq, tt.freqRange, valid, tt.wantValid)
			}
		})
	}
}

func TestConcurrentAccess(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	const numGoroutines = 10
	const numCommands = 5

	var wg sync.WaitGroup
	errors := make(chan error, numGoroutines*numCommands)

	// Start multiple goroutines doing concurrent operations
	for i := 0; i < numGoroutines; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()

			for j := 0; j < numCommands; j++ {
				// Mix of read and write operations
				if j%2 == 0 {
					response := rs.ExecuteCommand("getPower", []string{})
					if response.Error != "" {
						errors <- fmt.Errorf("goroutine %d: getPower failed: %v", id, response.Error)
						return
					}
				} else {
					response := rs.ExecuteCommand("setPower", []string{"25"})
					if response.Error != "" && response.Error != "BUSY" {
						errors <- fmt.Errorf("goroutine %d: setPower failed: %v", id, response.Error)
						return
					}
				}
			}
		}(i)
	}

	wg.Wait()
	close(errors)

	// Check for errors
	for err := range errors {
		if err != nil {
			t.Error(err)
		}
	}
}

func TestMaintenanceCommands(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	// Wait for any blackout to clear
	time.Sleep(6 * time.Second)

	// Test zeroize
	response := rs.ExecuteCommand("zeroize", []string{})
	if response.Error != "" {
		t.Errorf("Zeroize failed: %v", response.Error)
	}

	// Check that values were reset
	freqResponse := rs.ExecuteCommand("getFreq", []string{})
	if freqResponse.Error != "" {
		t.Errorf("Failed to read frequency after zeroize: %v", freqResponse.Error)
	} else {
		result := freqResponse.Result.([]string)
		if result[0] != "2490.0" {
			t.Errorf("Expected frequency 2490.0 after zeroize, got %s", result[0])
		}
	}

	powerResponse := rs.ExecuteCommand("getPower", []string{})
	if powerResponse.Error != "" {
		t.Errorf("Failed to read power after zeroize: %v", powerResponse.Error)
	} else {
		result := powerResponse.Result.([]string)
		if result[0] != "30" {
			t.Errorf("Expected power 30 after zeroize, got %s", result[0])
		}
	}

	// Test radio reset
	response = rs.ExecuteCommand("radioReset", []string{})
	if response.Error != "" {
		t.Errorf("Radio reset failed: %v", response.Error)
	}

	// Should be in blackout after reset
	if rs.IsAvailable() {
		t.Error("Expected radio to be unavailable after reset")
	}

	// Wait for radio reset blackout to clear
	time.Sleep(6 * time.Second)

	// Test factory reset
	response = rs.ExecuteCommand("factoryReset", []string{})
	if response.Error != "" {
		t.Errorf("Factory reset failed: %v", response.Error)
	}
}

func TestGetStatus(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	freq, power, available := rs.GetStatus()

	if freq != "2490.0" {
		t.Errorf("Expected frequency 2490.0, got %s", freq)
	}
	if power != 30 {
		t.Errorf("Expected power 30, got %d", power)
	}
	if !available {
		t.Error("Expected radio to be available initially")
	}
}

func TestInvalidCommand(t *testing.T) {
	rs := createTestRadioState()
	defer rs.Close()

	response := rs.ExecuteCommand("invalidCommand", []string{})
	if response.Error != "INTERNAL" {
		t.Errorf("Expected INTERNAL error for invalid command, got %v", response.Error)
	}
}

// Helper function to create a test radio state
func createTestRadioState() *RadioState {
	cfg := &config.Config{
		Mode: "normal",
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
	}

	return NewRadioState(cfg)
}
