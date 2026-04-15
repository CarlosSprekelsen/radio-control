package commands

import (
	"context"
	"math"
	"strconv"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

// ReadPowerDBmCommandHandler handles read_power_dBm command
// ICD §6.2: Read actual transmitted total output power in dBm
type ReadPowerDBmCommandHandler struct {
	state  *state.RadioState
	config *config.Config
}

// NewReadPowerDBmCommandHandler creates a new read power dBm command handler
func NewReadPowerDBmCommandHandler(radioState *state.RadioState, cfg *config.Config) *ReadPowerDBmCommandHandler {
	return &ReadPowerDBmCommandHandler{
		state:  radioState,
		config: cfg,
	}
}

// Handle processes read_power_dBm command
func (h *ReadPowerDBmCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	if len(params) > 0 {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "This command does not accept parameters"}
	}

	// Read actual transmitted power (simulate slight variation from set power)
	response := h.state.ExecuteCommand("getPower", []string{})
	if response.Error != "" {
		return nil, &CommandError{Code: response.Error, Message: response.Error}
	}

	// Simulate actual power measurement (slightly different from set power)
	result := response.Result.([]string)
	setPower, _ := strconv.Atoi(result[0])
	actualPower := setPower - 2 // Simulate 2dB lower actual output

	return []string{strconv.Itoa(actualPower)}, nil
}

func (h *ReadPowerDBmCommandHandler) GetName() string {
	return "read_power_dBm"
}

func (h *ReadPowerDBmCommandHandler) GetDescription() string {
	return "Read actual transmitted total output power in dBm"
}

func (h *ReadPowerDBmCommandHandler) IsReadOnly() bool {
	return true
}

func (h *ReadPowerDBmCommandHandler) RequiresBlackout() bool {
	return false
}

// ReadPowerMwCommandHandler handles read_power_mw command
// ICD §6.2: Read actual transmitted total output power in milliwatts
type ReadPowerMwCommandHandler struct {
	state  *state.RadioState
	config *config.Config
}

// NewReadPowerMwCommandHandler creates a new read power mw command handler
func NewReadPowerMwCommandHandler(radioState *state.RadioState, cfg *config.Config) *ReadPowerMwCommandHandler {
	return &ReadPowerMwCommandHandler{
		state:  radioState,
		config: cfg,
	}
}

// Handle processes read_power_mw command
func (h *ReadPowerMwCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	if len(params) > 0 {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "This command does not accept parameters"}
	}

	// Read actual transmitted power in dBm first
	response := h.state.ExecuteCommand("getPower", []string{})
	if response.Error != "" {
		return nil, &CommandError{Code: response.Error, Message: response.Error}
	}

	// Convert dBm to milliwatts
	result := response.Result.([]string)
	setPower, _ := strconv.Atoi(result[0])
	actualPower := setPower - 2 // Simulate 2dB lower actual output

	// Convert dBm to milliwatts: mW = 10^(dBm/10)
	milliwatts := int(10.0 * math.Pow(10.0, float64(actualPower)/10.0))

	return []string{strconv.Itoa(milliwatts)}, nil
}

func (h *ReadPowerMwCommandHandler) GetName() string {
	return "read_power_mw"
}

func (h *ReadPowerMwCommandHandler) GetDescription() string {
	return "Read actual transmitted total output power in milliwatts"
}

func (h *ReadPowerMwCommandHandler) IsReadOnly() bool {
	return true
}

func (h *ReadPowerMwCommandHandler) RequiresBlackout() bool {
	return false
}

// MaxLinkDistanceCommandHandler handles max_link_distance command
// ICD §6.2: Read maximum link distance
type MaxLinkDistanceCommandHandler struct {
	state  *state.RadioState
	config *config.Config
}

// NewMaxLinkDistanceCommandHandler creates a new max link distance command handler
func NewMaxLinkDistanceCommandHandler(radioState *state.RadioState, cfg *config.Config) *MaxLinkDistanceCommandHandler {
	return &MaxLinkDistanceCommandHandler{
		state:  radioState,
		config: cfg,
	}
}

// Handle processes max_link_distance command
func (h *MaxLinkDistanceCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	if len(params) > 0 {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "This command does not accept parameters"}
	}

	// Return a simulated maximum link distance in meters
	// This is a static value for the emulator
	maxDistance := 5000 // 5km maximum link distance

	return []string{strconv.Itoa(maxDistance)}, nil
}

func (h *MaxLinkDistanceCommandHandler) GetName() string {
	return "max_link_distance"
}

func (h *MaxLinkDistanceCommandHandler) GetDescription() string {
	return "Read maximum link distance in meters"
}

func (h *MaxLinkDistanceCommandHandler) IsReadOnly() bool {
	return true
}

func (h *MaxLinkDistanceCommandHandler) RequiresBlackout() bool {
	return false
}

// RegisterOptionalCommands registers optional commands in the registry
func RegisterOptionalCommands(registry *CommandRegistry, radioState *state.RadioState, cfg *config.Config) {
	registry.Register(NewReadPowerDBmCommandHandler(radioState, cfg))
	registry.Register(NewReadPowerMwCommandHandler(radioState, cfg))
	registry.Register(NewMaxLinkDistanceCommandHandler(radioState, cfg))
}
