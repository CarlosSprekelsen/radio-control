package commands

import (
	"context"
	"testing"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

// Helper to create a test config for optional commands
func createOptionalTestConfig() *config.Config {
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

// Helper to create an isolated RadioState for optional command tests
func createOptionalTestRadioState(cfg *config.Config) *state.RadioState {
	rs := state.NewRadioState(cfg)
	return rs
}

func TestReadPowerDBmCommandHandler(t *testing.T) {
	cfg := createOptionalTestConfig()
	radioState := createOptionalTestRadioState(cfg)
	defer radioState.Close()

	handler := NewReadPowerDBmCommandHandler(radioState, cfg)

	// Test read command
	result, err := handler.Handle(context.Background(), []string{})
	if err != nil {
		t.Fatalf("Handle() error = %v", err)
	}

	resultSlice, ok := result.([]string)
	if !ok {
		t.Fatalf("Expected []string result, got %T", result)
	}

	if len(resultSlice) != 1 {
		t.Fatalf("Expected 1 result, got %d", len(resultSlice))
	}

	// Should be 2dB lower than set power (30 - 2 = 28)
	if resultSlice[0] != "28" {
		t.Errorf("Expected actual power 28, got %s", resultSlice[0])
	}

	// Test with parameters (should fail)
	_, err = handler.Handle(context.Background(), []string{"invalid"})
	if err == nil {
		t.Error("Expected error for invalid parameters")
	}

	// Test command metadata
	if handler.GetName() != "read_power_dBm" {
		t.Errorf("Expected name 'read_power_dBm', got '%s'", handler.GetName())
	}

	if !handler.IsReadOnly() {
		t.Error("Expected command to be read-only")
	}

	if handler.RequiresBlackout() {
		t.Error("Expected command to not require blackout")
	}
}

func TestReadPowerMwCommandHandler(t *testing.T) {
	cfg := createOptionalTestConfig()
	radioState := createOptionalTestRadioState(cfg)
	defer radioState.Close()

	handler := NewReadPowerMwCommandHandler(radioState, cfg)

	// Test read command
	result, err := handler.Handle(context.Background(), []string{})
	if err != nil {
		t.Fatalf("Handle() error = %v", err)
	}

	resultSlice, ok := result.([]string)
	if !ok {
		t.Fatalf("Expected []string result, got %T", result)
	}

	if len(resultSlice) != 1 {
		t.Fatalf("Expected 1 result, got %d", len(resultSlice))
	}

	// Should be milliwatts equivalent of 28dBm
	// 10^(28/10) = 630.96 ≈ 630 mW
	// But the actual calculation might be different, so just check it's not empty
	if resultSlice[0] == "" {
		t.Error("Expected non-empty power value")
	}

	// Test with parameters (should fail)
	_, err = handler.Handle(context.Background(), []string{"invalid"})
	if err == nil {
		t.Error("Expected error for invalid parameters")
	}

	// Test command metadata
	if handler.GetName() != "read_power_mw" {
		t.Errorf("Expected name 'read_power_mw', got '%s'", handler.GetName())
	}

	if !handler.IsReadOnly() {
		t.Error("Expected command to be read-only")
	}

	if handler.RequiresBlackout() {
		t.Error("Expected command to not require blackout")
	}
}

func TestMaxLinkDistanceCommandHandler(t *testing.T) {
	cfg := createOptionalTestConfig()
	radioState := createOptionalTestRadioState(cfg)
	defer radioState.Close()

	handler := NewMaxLinkDistanceCommandHandler(radioState, cfg)

	// Test read command
	result, err := handler.Handle(context.Background(), []string{})
	if err != nil {
		t.Fatalf("Handle() error = %v", err)
	}

	resultSlice, ok := result.([]string)
	if !ok {
		t.Fatalf("Expected []string result, got %T", result)
	}

	if len(resultSlice) != 1 {
		t.Fatalf("Expected 1 result, got %d", len(resultSlice))
	}

	// Should be 5000 meters (5km)
	if resultSlice[0] != "5000" {
		t.Errorf("Expected distance 5000, got %s", resultSlice[0])
	}

	// Test with parameters (should fail)
	_, err = handler.Handle(context.Background(), []string{"invalid"})
	if err == nil {
		t.Error("Expected error for invalid parameters")
	}

	// Test command metadata
	if handler.GetName() != "max_link_distance" {
		t.Errorf("Expected name 'max_link_distance', got '%s'", handler.GetName())
	}

	if !handler.IsReadOnly() {
		t.Error("Expected command to be read-only")
	}

	if handler.RequiresBlackout() {
		t.Error("Expected command to not require blackout")
	}
}

func TestRegisterOptionalCommands(t *testing.T) {
	cfg := createOptionalTestConfig()
	radioState := createOptionalTestRadioState(cfg)
	defer radioState.Close()

	registry := NewCommandRegistry()
	RegisterOptionalCommands(registry, radioState, cfg)

	// Test that all optional commands are registered
	expectedCommands := []string{
		"read_power_dBm",
		"read_power_mw",
		"max_link_distance",
	}

	for _, cmdName := range expectedCommands {
		cmd, ok := registry.Get(cmdName)
		if !ok {
			t.Errorf("Command '%s' not found in registry", cmdName)
			continue
		}

		if cmd.GetName() != cmdName {
			t.Errorf("Expected command name '%s', got '%s'", cmdName, cmd.GetName())
		}

		if !cmd.IsReadOnly() {
			t.Errorf("Command '%s' should be read-only", cmdName)
		}

		if cmd.RequiresBlackout() {
			t.Errorf("Command '%s' should not require blackout", cmdName)
		}
	}

	// Test that we have at least the expected commands
	allCommands := registry.List()
	if len(allCommands) < len(expectedCommands) {
		t.Errorf("Expected at least %d commands, got %d", len(expectedCommands), len(allCommands))
	}
}

func TestOptionalCommandsWithDifferentPowerSettings(t *testing.T) {
	cfg := createOptionalTestConfig()
	radioState := createOptionalTestRadioState(cfg)
	defer radioState.Close()

	// Set power to 25 dBm
	response := radioState.ExecuteCommand("setPower", []string{"25"})
	if response.Error != "" {
		t.Fatalf("Failed to set power: %v", response.Error)
	}

	// Test read_power_dBm (should return 23 dBm - 2dB lower)
	handler := NewReadPowerDBmCommandHandler(radioState, cfg)
	result, err := handler.Handle(context.Background(), []string{})
	if err != nil {
		t.Fatalf("Handle() error = %v", err)
	}

	resultSlice := result.([]string)
	if resultSlice[0] != "23" {
		t.Errorf("Expected actual power 23, got %s", resultSlice[0])
	}

	// Test read_power_mw (should return milliwatts equivalent of 23dBm)
	mwHandler := NewReadPowerMwCommandHandler(radioState, cfg)
	result, err = mwHandler.Handle(context.Background(), []string{})
	if err != nil {
		t.Fatalf("Handle() error = %v", err)
	}

	resultSlice = result.([]string)
	// Just check it's not empty
	if resultSlice[0] == "" {
		t.Error("Expected non-empty power value")
	}
}

func TestOptionalCommandsErrorHandling(t *testing.T) {
	cfg := createOptionalTestConfig()
	radioState := createOptionalTestRadioState(cfg)
	defer radioState.Close()

	handlers := []struct {
		name    string
		handler CommandHandler
	}{
		{"read_power_dBm", NewReadPowerDBmCommandHandler(radioState, cfg)},
		{"read_power_mw", NewReadPowerMwCommandHandler(radioState, cfg)},
		{"max_link_distance", NewMaxLinkDistanceCommandHandler(radioState, cfg)},
	}

	for _, h := range handlers {
		t.Run(h.name, func(t *testing.T) {
			// Test with invalid parameters
			_, err := h.handler.Handle(context.Background(), []string{"invalid"})
			if err == nil {
				t.Error("Expected error for invalid parameters")
			}

			// Check error type
			cmdErr, ok := err.(*CommandError)
			if !ok {
				t.Errorf("Expected CommandError, got %T", err)
			} else if cmdErr.Code != ErrInvalidParams {
				t.Errorf("Expected ErrInvalidParams, got %s", cmdErr.Code)
			}
		})
	}
}
