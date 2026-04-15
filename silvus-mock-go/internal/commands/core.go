package commands

import (
	"context"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

// CoreCommandHandler handles core radio commands (freq, power_dBm, supported_frequency_profiles)
type CoreCommandHandler struct {
	state  *state.RadioState
	config *config.Config
}

// NewCoreCommandHandler creates a new core command handler
func NewCoreCommandHandler(radioState *state.RadioState, cfg *config.Config) *CoreCommandHandler {
	return &CoreCommandHandler{
		state:  radioState,
		config: cfg,
	}
}

// Handle processes core commands
func (h *CoreCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	// This will be implemented by specific command handlers
	return nil, &CommandError{Code: ErrNotSupported, Message: "Not implemented"}
}

// GetName returns the command name
func (h *CoreCommandHandler) GetName() string {
	return "core"
}

// GetDescription returns the command description
func (h *CoreCommandHandler) GetDescription() string {
	return "Core radio commands"
}

// IsReadOnly returns false (core commands can modify state)
func (h *CoreCommandHandler) IsReadOnly() bool {
	return false
}

// RequiresBlackout returns true (frequency changes trigger blackout)
func (h *CoreCommandHandler) RequiresBlackout() bool {
	return true
}

// FreqCommandHandler handles frequency commands
type FreqCommandHandler struct {
	state  *state.RadioState
	config *config.Config
}

// NewFreqCommandHandler creates a new frequency command handler
func NewFreqCommandHandler(radioState *state.RadioState, cfg *config.Config) *FreqCommandHandler {
	return &FreqCommandHandler{
		state:  radioState,
		config: cfg,
	}
}

// Handle processes frequency commands
func (h *FreqCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	if len(params) == 0 {
		// Read frequency
		response := h.state.ExecuteCommand("getFreq", []string{})
		if response.Error != "" {
			return nil, &CommandError{Code: response.Error, Message: response.Error}
		}
		return response.Result, nil
	}

	// Set frequency
	response := h.state.ExecuteCommand("setFreq", params)
	if response.Error != "" {
		return nil, &CommandError{Code: response.Error, Message: response.Error}
	}
	return response.Result, nil
}

// GetName returns the command name
func (h *FreqCommandHandler) GetName() string {
	return "freq"
}

// GetDescription returns the command description
func (h *FreqCommandHandler) GetDescription() string {
	return "Set/read RF frequency in MHz"
}

// IsReadOnly returns false (frequency can be set)
func (h *FreqCommandHandler) IsReadOnly() bool {
	return false
}

// RequiresBlackout returns true (frequency changes trigger blackout)
func (h *FreqCommandHandler) RequiresBlackout() bool {
	return true
}

// PowerCommandHandler handles power commands
type PowerCommandHandler struct {
	state  *state.RadioState
	config *config.Config
}

// NewPowerCommandHandler creates a new power command handler
func NewPowerCommandHandler(radioState *state.RadioState, cfg *config.Config) *PowerCommandHandler {
	return &PowerCommandHandler{
		state:  radioState,
		config: cfg,
	}
}

// Handle processes power commands
func (h *PowerCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	if len(params) == 0 {
		// Read power
		response := h.state.ExecuteCommand("getPower", []string{})
		if response.Error != "" {
			return nil, &CommandError{Code: response.Error, Message: response.Error}
		}
		return response.Result, nil
	}

	// Set power
	response := h.state.ExecuteCommand("setPower", params)
	if response.Error != "" {
		return nil, &CommandError{Code: response.Error, Message: response.Error}
	}
	return response.Result, nil
}

// GetName returns the command name
func (h *PowerCommandHandler) GetName() string {
	return "power_dBm"
}

// GetDescription returns the command description
func (h *PowerCommandHandler) GetDescription() string {
	return "Set/read transmit power in dBm"
}

// IsReadOnly returns false (power can be set)
func (h *PowerCommandHandler) IsReadOnly() bool {
	return false
}

// RequiresBlackout returns false (power changes don't trigger blackout)
func (h *PowerCommandHandler) RequiresBlackout() bool {
	return false
}

// ProfilesCommandHandler handles frequency profiles commands
type ProfilesCommandHandler struct {
	state  *state.RadioState
	config *config.Config
}

// NewProfilesCommandHandler creates a new profiles command handler
func NewProfilesCommandHandler(radioState *state.RadioState, cfg *config.Config) *ProfilesCommandHandler {
	return &ProfilesCommandHandler{
		state:  radioState,
		config: cfg,
	}
}

// Handle processes frequency profiles commands
func (h *ProfilesCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	if len(params) > 0 {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "This command does not accept parameters"}
	}

	response := h.state.ExecuteCommand("getProfiles", []string{})
	if response.Error != "" {
		return nil, &CommandError{Code: response.Error, Message: response.Error}
	}
	return response.Result, nil
}

// GetName returns the command name
func (h *ProfilesCommandHandler) GetName() string {
	return "supported_frequency_profiles"
}

// GetDescription returns the command description
func (h *ProfilesCommandHandler) GetDescription() string {
	return "Get supported frequency profiles"
}

// IsReadOnly returns true (profiles are read-only)
func (h *ProfilesCommandHandler) IsReadOnly() bool {
	return true
}

// RequiresBlackout returns false (reading profiles doesn't trigger blackout)
func (h *ProfilesCommandHandler) RequiresBlackout() bool {
	return false
}

// MaintenanceCommandHandler handles maintenance commands
type MaintenanceCommandHandler struct {
	state  *state.RadioState
	config *config.Config
}

// NewMaintenanceCommandHandler creates a new maintenance command handler
func NewMaintenanceCommandHandler(radioState *state.RadioState, cfg *config.Config) *MaintenanceCommandHandler {
	return &MaintenanceCommandHandler{
		state:  radioState,
		config: cfg,
	}
}

// Handle processes maintenance commands
func (h *MaintenanceCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	// Get command name from context or determine from parameters
	commandName := ctx.Value("commandName").(string)

	response := h.state.ExecuteCommand(commandName, params)
	if response.Error != "" {
		return nil, &CommandError{Code: response.Error, Message: response.Error}
	}
	return response.Result, nil
}

// GetName returns the command name
func (h *MaintenanceCommandHandler) GetName() string {
	return "maintenance"
}

// GetDescription returns the command description
func (h *MaintenanceCommandHandler) GetDescription() string {
	return "Maintenance commands (zeroize, radio_reset, factory_reset)"
}

// IsReadOnly returns false (maintenance commands modify state)
func (h *MaintenanceCommandHandler) IsReadOnly() bool {
	return false
}

// RequiresBlackout returns false (maintenance commands handle their own blackouts)
func (h *MaintenanceCommandHandler) RequiresBlackout() bool {
	return false
}

// RegisterCoreCommands registers all core commands in the registry
func RegisterCoreCommands(registry *CommandRegistry, radioState *state.RadioState, cfg *config.Config) {
	registry.Register(NewFreqCommandHandler(radioState, cfg))
	registry.Register(NewPowerCommandHandler(radioState, cfg))
	registry.Register(NewProfilesCommandHandler(radioState, cfg))
	registry.Register(NewMaintenanceCommandHandler(radioState, cfg))
}
