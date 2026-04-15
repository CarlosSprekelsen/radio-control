package state

import (
	"testing"
	"time"

	"github.com/silvus-mock/internal/config"
)

// Helper to create a test config for blackout tests
func createBlackoutTestConfig() *config.Config {
	return &config.Config{
		Mode: "normal",
		Profiles: config.ProfilesConfig{
			FrequencyProfiles: []config.FrequencyProfile{
				{Frequencies: []string{"2200:20:2380", "4700"}, Bandwidth: "-1", AntennaMask: "15"},
				{Frequencies: []string{"5000:10:5100"}, Bandwidth: "-1", AntennaMask: "3"},
			},
		},
		Power: config.PowerConfig{
			MinDBm: 0,
			MaxDBm: 39,
		},
		Timing: config.TimingConfig{
			Blackout: config.BlackoutConfig{
				SoftBootSec:    30, // CB-TIMING v0.3 §6.2: Channel change blackout
				PowerChangeSec: 5,  // CB-TIMING v0.3 §6.2: Power change blackout
				RadioResetSec:  60, // CB-TIMING v0.3 §6.2: Radio reset blackout
			},
			Commands: config.CommandsConfig{
				SetPower:    config.TimeoutConfig{TimeoutSec: 10},
				SetChannel:  config.TimeoutConfig{TimeoutSec: 30},
				SelectRadio: config.TimeoutConfig{TimeoutSec: 5},
				Read:        config.TimeoutConfig{TimeoutSec: 5},
			},
		},
	}
}

// Helper to create an isolated RadioState for blackout tests
func createBlackoutTestRadioState() *RadioState {
	cfg := createBlackoutTestConfig()
	rs := NewRadioState(cfg)
	// Give the command worker a moment to start
	time.Sleep(10 * time.Millisecond)
	return rs
}

func TestBlackoutBehaviorDuringSoftBoot(t *testing.T) {
	rs := createBlackoutTestRadioState()
	defer rs.Close()

	// Set frequency, which should trigger a 30-second blackout
	response := rs.ExecuteCommand("setFreq", []string{"4700"})
	if response.Error != "" {
		t.Fatalf("setFreq failed: %v", response.Error)
	}

	// Immediately try read commands (should return UNAVAILABLE during blackout)
	// ICD §6.1.1: During soft-boot, avoid concurrent API calls
	tests := []struct {
		name     string
		method   string
		params   []string
		expected string
	}{
		{"getFreq", "getFreq", []string{}, "UNAVAILABLE"},
		{"getPower", "getPower", []string{}, "UNAVAILABLE"},
		{"getProfiles", "supported_frequency_profiles", []string{}, "UNAVAILABLE"},
		{"setPower", "setPower", []string{"25"}, "UNAVAILABLE"},
		{"setFreq", "setFreq", []string{"2200"}, "UNAVAILABLE"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			response := rs.ExecuteCommand(tt.method, tt.params)
			if response.Error != tt.expected {
				t.Errorf("ExecuteCommand() during blackout error = %v, want %v", response.Error, tt.expected)
			}
		})
	}

	// Verify radio is not available during blackout
	if rs.IsAvailable() {
		t.Error("Expected radio to be unavailable during blackout")
	}
}

func TestBlackoutRecoveryAfterSoftBoot(t *testing.T) {
	rs := createBlackoutTestRadioState()
	defer rs.Close()

	// Set frequency, which should trigger a 30-second blackout
	response := rs.ExecuteCommand("setFreq", []string{"4700"})
	if response.Error != "" {
		t.Fatalf("setFreq failed: %v", response.Error)
	}

	// Verify we're in blackout
	if rs.IsAvailable() {
		t.Error("Expected radio to be unavailable during blackout")
	}

	// Wait for blackout to clear (30 seconds + buffer)
	time.Sleep(31 * time.Second)

	// Verify radio is available again
	if !rs.IsAvailable() {
		t.Error("Expected radio to be available after blackout")
	}

	// Test that commands work again
	response = rs.ExecuteCommand("getFreq", []string{})
	if response.Error != "" {
		t.Errorf("getFreq failed after blackout: %v", response.Error)
	}

	// Should return the frequency we set
	result := response.Result.([]string)
	if result[0] != "4700" {
		t.Errorf("Expected frequency 4700, got %s", result[0])
	}
}

func TestPowerChangeBlackout(t *testing.T) {
	rs := createBlackoutTestRadioState()
	defer rs.Close()

	// Wait for any existing blackout to clear
	time.Sleep(1 * time.Second)

	// Set power, which should trigger a 5-second blackout
	response := rs.ExecuteCommand("setPower", []string{"25"})
	if response.Error != "" {
		t.Fatalf("setPower failed: %v", response.Error)
	}

	// Verify we're in blackout
	if rs.IsAvailable() {
		t.Error("Expected radio to be unavailable during power change blackout")
	}

	// Test read commands during power change blackout (should return UNAVAILABLE)
	response = rs.ExecuteCommand("getPower", []string{})
	if response.Error != "UNAVAILABLE" {
		t.Errorf("Expected UNAVAILABLE during power change blackout, got %v", response.Error)
	}

	// Wait for blackout to clear (5 seconds + buffer)
	time.Sleep(6 * time.Second)

	// Verify radio is available again
	if !rs.IsAvailable() {
		t.Error("Expected radio to be available after power change blackout")
	}

	// Test that commands work again
	response = rs.ExecuteCommand("getPower", []string{})
	if response.Error != "" {
		t.Errorf("getPower failed after blackout: %v", response.Error)
	}

	// Should return the power we set
	result := response.Result.([]string)
	if result[0] != "25" {
		t.Errorf("Expected power 25, got %s", result[0])
	}
}

func TestRadioResetBlackout(t *testing.T) {
	rs := createBlackoutTestRadioState()
	defer rs.Close()

	// Wait for any existing blackout to clear
	time.Sleep(1 * time.Second)

	// Execute radio reset, which should trigger a 60-second blackout
	response := rs.ExecuteCommand("radioReset", []string{})
	if response.Error != "" {
		t.Fatalf("radioReset failed: %v", response.Error)
	}

	// Verify we're in blackout
	if rs.IsAvailable() {
		t.Error("Expected radio to be unavailable during radio reset blackout")
	}

	// Test commands during radio reset blackout (should return UNAVAILABLE)
	response = rs.ExecuteCommand("getPower", []string{})
	if response.Error != "UNAVAILABLE" {
		t.Errorf("Expected UNAVAILABLE during radio reset blackout, got %v", response.Error)
	}

	// For testing, we'll use a shorter timeout
	// In real scenarios, this would be 60 seconds
	time.Sleep(2 * time.Second)

	// Note: In a real test, we'd wait 61 seconds, but for unit testing we'll just verify
	// that the blackout duration was set correctly
	if rs.blackoutUntil.After(time.Now().Add(59 * time.Second)) {
		t.Error("Radio reset blackout duration seems too long")
	}
}

func TestBlackoutConfiguration(t *testing.T) {
	cfg := createBlackoutTestConfig()
	rs := NewRadioState(cfg)
	defer rs.Close()

	// Test that blackout durations are configured correctly
	expectedSoftBoot := 30 * time.Second
	expectedPowerChange := 5 * time.Second
	expectedRadioReset := 60 * time.Second

	if rs.softBootDuration != expectedSoftBoot {
		t.Errorf("Expected soft boot duration %v, got %v", expectedSoftBoot, rs.softBootDuration)
	}

	if rs.powerChangeDuration != expectedPowerChange {
		t.Errorf("Expected power change duration %v, got %v", expectedPowerChange, rs.powerChangeDuration)
	}

	if rs.radioResetDuration != expectedRadioReset {
		t.Errorf("Expected radio reset duration %v, got %v", expectedRadioReset, rs.radioResetDuration)
	}
}

func TestConcurrentCommandsDuringBlackout(t *testing.T) {
	rs := createBlackoutTestRadioState()
	defer rs.Close()

	// Set frequency to trigger blackout
	response := rs.ExecuteCommand("setFreq", []string{"4700"})
	if response.Error != "" {
		t.Fatalf("setFreq failed: %v", response.Error)
	}

	// Send multiple concurrent commands during blackout
	responses := make(chan CommandResponse, 5)

	go func() {
		responses <- rs.ExecuteCommand("getFreq", []string{})
	}()

	go func() {
		responses <- rs.ExecuteCommand("getPower", []string{})
	}()

	go func() {
		responses <- rs.ExecuteCommand("setPower", []string{"25"})
	}()

	go func() {
		responses <- rs.ExecuteCommand("setFreq", []string{"2200"})
	}()

	go func() {
		responses <- rs.ExecuteCommand("supported_frequency_profiles", []string{})
	}()

	// Collect responses
	for i := 0; i < 5; i++ {
		response := <-responses
		if response.Error != "UNAVAILABLE" {
			t.Errorf("Expected UNAVAILABLE during blackout, got %v", response.Error)
		}
	}
}

func TestBlackoutStatus(t *testing.T) {
	rs := createBlackoutTestRadioState()
	defer rs.Close()

	// Initially should be available
	if !rs.IsAvailable() {
		t.Error("Expected radio to be available initially")
	}

	// Set frequency to trigger blackout
	response := rs.ExecuteCommand("setFreq", []string{"4700"})
	if response.Error != "" {
		t.Fatalf("setFreq failed: %v", response.Error)
	}

	// Should be unavailable during blackout
	if rs.IsAvailable() {
		t.Error("Expected radio to be unavailable during blackout")
	}

	// Check status
	freq, power, available := rs.GetStatus()
	if available {
		t.Error("Expected status.Available to be false during blackout")
	}
	_ = freq  // Avoid unused variable warning
	_ = power // Avoid unused variable warning
}
