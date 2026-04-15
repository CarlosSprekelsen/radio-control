package commands

import (
	"context"
	"testing"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

func TestCommandRegistry(t *testing.T) {
	registry := NewCommandRegistry()

	// Test empty registry
	if len(registry.List()) != 0 {
		t.Error("Expected empty registry")
	}

	// Test registering commands
	handler := &mockCommandHandler{name: "test_command"}
	registry.Register(handler)

	if len(registry.List()) != 1 {
		t.Error("Expected one command in registry")
	}

	// Test getting command
	retrieved, exists := registry.Get("test_command")
	if !exists {
		t.Error("Expected to find test_command")
	}
	if retrieved != handler {
		t.Error("Expected to get the same handler")
	}

	// Test getting non-existent command
	_, exists = registry.Get("non_existent")
	if exists {
		t.Error("Expected non_existent command to not exist")
	}
}

func TestCoreCommands(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	defer radioState.Close()

	registry := NewCommandRegistry()
	RegisterCoreCommands(registry, radioState, cfg)

	// Test that core commands are registered
	expectedCommands := []string{
		"freq",
		"power_dBm",
		"supported_frequency_profiles",
		"maintenance",
	}

	for _, cmd := range expectedCommands {
		handler, exists := registry.Get(cmd)
		if !exists {
			t.Errorf("Expected command %s to be registered", cmd)
		}
		if handler.GetName() != cmd {
			t.Errorf("Expected command name %s, got %s", cmd, handler.GetName())
		}
	}
}

func TestFreqCommandHandler(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	defer radioState.Close()

	handler := NewFreqCommandHandler(radioState, cfg)

	// Test command properties
	if handler.GetName() != "freq" {
		t.Errorf("Expected name 'freq', got %s", handler.GetName())
	}
	if handler.IsReadOnly() {
		t.Error("Expected freq command to not be read-only")
	}
	if !handler.RequiresBlackout() {
		t.Error("Expected freq command to require blackout")
	}

	// Test read frequency
	ctx := context.Background()
	result, err := handler.Handle(ctx, []string{})
	if err != nil {
		t.Errorf("Expected no error reading frequency, got %v", err)
	}
	if result == nil {
		t.Error("Expected frequency result")
	}

	// Test set frequency
	result, err = handler.Handle(ctx, []string{"4700"})
	if err != nil {
		t.Errorf("Expected no error setting frequency, got %v", err)
	}
	if result == nil {
		t.Error("Expected success result")
	}
}

func TestPowerCommandHandler(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	defer radioState.Close()

	handler := NewPowerCommandHandler(radioState, cfg)

	// Test command properties
	if handler.GetName() != "power_dBm" {
		t.Errorf("Expected name 'power_dBm', got %s", handler.GetName())
	}
	if handler.IsReadOnly() {
		t.Error("Expected power command to not be read-only")
	}
	if handler.RequiresBlackout() {
		t.Error("Expected power command to not require blackout")
	}

	// Test read power
	ctx := context.Background()
	result, err := handler.Handle(ctx, []string{})
	if err != nil {
		t.Errorf("Expected no error reading power, got %v", err)
	}
	if result == nil {
		t.Error("Expected power result")
	}

	// Test set power
	result, err = handler.Handle(ctx, []string{"25"})
	if err != nil {
		t.Errorf("Expected no error setting power, got %v", err)
	}
	if result == nil {
		t.Error("Expected success result")
	}
}

func TestProfilesCommandHandler(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	defer radioState.Close()

	handler := NewProfilesCommandHandler(radioState, cfg)

	// Test command properties
	if handler.GetName() != "supported_frequency_profiles" {
		t.Errorf("Expected name 'supported_frequency_profiles', got %s", handler.GetName())
	}
	if !handler.IsReadOnly() {
		t.Error("Expected profiles command to be read-only")
	}
	if handler.RequiresBlackout() {
		t.Error("Expected profiles command to not require blackout")
	}

	// Test get profiles
	ctx := context.Background()
	result, err := handler.Handle(ctx, []string{})
	if err != nil {
		t.Errorf("Expected no error getting profiles, got %v", err)
	}
	if result == nil {
		t.Error("Expected profiles result")
	}

	// Test with parameters (should error)
	result, err = handler.Handle(ctx, []string{"extra"})
	if err == nil {
		t.Error("Expected error with parameters")
	}
}

func TestGPSCommands(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	defer radioState.Close()

	registry := NewCommandRegistry()
	RegisterGPSCommands(registry, radioState, cfg)

	// Test that GPS commands are registered
	expectedCommands := []string{
		"gps_coordinates",
		"gps_mode",
		"gps_time",
	}

	for _, cmd := range expectedCommands {
		handler, exists := registry.Get(cmd)
		if !exists {
			t.Errorf("Expected GPS command %s to be registered", cmd)
		}
		if handler.GetName() != cmd {
			t.Errorf("Expected command name %s, got %s", cmd, handler.GetName())
		}
	}
}

func TestGPSCoordinatesHandler(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	defer radioState.Close()

	handler := NewGpsCoordinatesCommandHandler(radioState, cfg)

	// Test read coordinates
	ctx := context.Background()
	result, err := handler.Handle(ctx, []string{})
	if err != nil {
		t.Errorf("Expected no error reading coordinates, got %v", err)
	}
	if result == nil {
		t.Error("Expected coordinates result")
	}

	// Test set coordinates
	result, err = handler.Handle(ctx, []string{"40.7128", "-74.0060", "10.0"})
	if err != nil {
		t.Errorf("Expected no error setting coordinates, got %v", err)
	}
	if result == nil {
		t.Error("Expected success result")
	}

	// Test invalid coordinates
	result, err = handler.Handle(ctx, []string{"91.0", "-74.0060", "10.0"}) // Invalid latitude
	if err == nil {
		t.Error("Expected error for invalid latitude")
	}

	result, err = handler.Handle(ctx, []string{"40.7128", "181.0", "10.0"}) // Invalid longitude
	if err == nil {
		t.Error("Expected error for invalid longitude")
	}

	result, err = handler.Handle(ctx, []string{"40.7128", "-74.0060"}) // Missing altitude
	if err == nil {
		t.Error("Expected error for missing altitude")
	}
}

func TestGPSModeHandler(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	defer radioState.Close()

	handler := NewGpsModeCommandHandler(radioState, cfg)

	// Test read mode
	ctx := context.Background()
	result, err := handler.Handle(ctx, []string{})
	if err != nil {
		t.Errorf("Expected no error reading mode, got %v", err)
	}
	if result == nil {
		t.Error("Expected mode result")
	}

	// Test set mode
	result, err = handler.Handle(ctx, []string{"true"})
	if err != nil {
		t.Errorf("Expected no error setting mode, got %v", err)
	}
	if result == nil {
		t.Error("Expected success result")
	}

	// Test invalid mode
	result, err = handler.Handle(ctx, []string{"invalid"})
	if err == nil {
		t.Error("Expected error for invalid mode")
	}
}

func TestGPSTimeHandler(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	defer radioState.Close()

	handler := NewGpsTimeCommandHandler(radioState, cfg)

	// Test read time
	ctx := context.Background()
	result, err := handler.Handle(ctx, []string{})
	if err != nil {
		t.Errorf("Expected no error reading time, got %v", err)
	}
	if result == nil {
		t.Error("Expected time result")
	}

	// Test set time
	result, err = handler.Handle(ctx, []string{"1640995200"}) // 2022-01-01
	if err != nil {
		t.Errorf("Expected no error setting time, got %v", err)
	}
	if result == nil {
		t.Error("Expected success result")
	}

	// Test invalid time
	result, err = handler.Handle(ctx, []string{"invalid"})
	if err == nil {
		t.Error("Expected error for invalid time")
	}

	result, err = handler.Handle(ctx, []string{"1"}) // Too old
	if err == nil {
		t.Error("Expected error for time too old")
	}
}

func TestCustomCommandHandler(t *testing.T) {
	handler := NewCustomCommandHandler(
		"test_command",
		"Test command",
		true,
		false,
		func(ctx context.Context, params []string) (interface{}, error) {
			return map[string]string{"status": "ok"}, nil
		},
	)

	// Test properties
	if handler.GetName() != "test_command" {
		t.Errorf("Expected name 'test_command', got %s", handler.GetName())
	}
	if !handler.IsReadOnly() {
		t.Error("Expected custom command to be read-only")
	}
	if handler.RequiresBlackout() {
		t.Error("Expected custom command to not require blackout")
	}

	// Test execution
	ctx := context.Background()
	result, err := handler.Handle(ctx, []string{})
	if err != nil {
		t.Errorf("Expected no error, got %v", err)
	}
	if result == nil {
		t.Error("Expected result")
	}
}

func TestExtensibleServer(t *testing.T) {
	cfg := createTestConfig()
	radioState := createTestRadioState(cfg)
	defer radioState.Close()

	server := NewExtensibleJSONRPCServer(cfg, radioState)

	// Test available commands
	commands := server.GetAvailableCommands()
	if len(commands) == 0 {
		t.Error("Expected available commands")
	}

	// Test adding custom command
	customHandler := NewCustomCommandHandler(
		"custom_test",
		"Custom test command",
		true,
		false,
		func(ctx context.Context, params []string) (interface{}, error) {
			return "test result", nil
		},
	)

	server.AddCustomCommand(customHandler)

	// Verify command was added
	commands = server.GetAvailableCommands()
	found := false
	for _, cmd := range commands {
		if cmd.Name == "custom_test" {
			found = true
			break
		}
	}
	if !found {
		t.Error("Expected custom command to be added")
	}

	// Test removing command
	server.RemoveCommand("custom_test")

	// Verify command was removed
	commands = server.GetAvailableCommands()
	found = false
	for _, cmd := range commands {
		if cmd.Name == "custom_test" {
			found = true
			break
		}
	}
	if found {
		t.Error("Expected custom command to be removed")
	}
}

// Mock command handler for testing
type mockCommandHandler struct {
	name string
}

func (m *mockCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	return "mock result", nil
}

func (m *mockCommandHandler) GetName() string {
	return m.name
}

func (m *mockCommandHandler) GetDescription() string {
	return "Mock command"
}

func (m *mockCommandHandler) IsReadOnly() bool {
	return true
}

func (m *mockCommandHandler) RequiresBlackout() bool {
	return false
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
		},
		Mode: "normal",
	}
}

func createTestRadioState(cfg *config.Config) *state.RadioState {
	return state.NewRadioState(cfg)
}
