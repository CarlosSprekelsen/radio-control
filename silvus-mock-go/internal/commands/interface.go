package commands

import (
	"context"
	"time"
)

// CommandHandler defines the interface for command handlers
type CommandHandler interface {
	// Handle processes a command and returns the response
	Handle(ctx context.Context, params []string) (interface{}, error)

	// GetName returns the command name
	GetName() string

	// GetDescription returns a human-readable description
	GetDescription() string

	// IsReadOnly returns true if the command only reads data
	IsReadOnly() bool

	// RequiresBlackout returns true if the command triggers a blackout period
	RequiresBlackout() bool
}

// CommandRegistry manages available commands
type CommandRegistry struct {
	handlers map[string]CommandHandler
}

// NewCommandRegistry creates a new command registry
func NewCommandRegistry() *CommandRegistry {
	return &CommandRegistry{
		handlers: make(map[string]CommandHandler),
	}
}

// Register adds a command handler to the registry
func (r *CommandRegistry) Register(handler CommandHandler) {
	r.handlers[handler.GetName()] = handler
}

// Get returns a command handler by name
func (r *CommandRegistry) Get(name string) (CommandHandler, bool) {
	handler, exists := r.handlers[name]
	return handler, exists
}

// List returns all registered command names
func (r *CommandRegistry) List() []string {
	names := make([]string, 0, len(r.handlers))
	for name := range r.handlers {
		names = append(names, name)
	}
	return names
}

// CommandContext provides context for command execution
type CommandContext struct {
	RadioID   string
	Timestamp time.Time
	UserID    string
	SessionID string
}

// CommandResponse represents the result of a command execution
type CommandResponse struct {
	Result interface{} `json:"result,omitempty"`
	Error  string      `json:"error,omitempty"`
}

// CommandError represents a command-specific error
type CommandError struct {
	Code    string `json:"code"`
	Message string `json:"message"`
	Details string `json:"details,omitempty"`
}

func (e *CommandError) Error() string {
	return e.Message
}

// Common error codes
const (
	ErrInvalidRange  = "INVALID_RANGE"
	ErrBusy          = "BUSY"
	ErrUnavailable   = "UNAVAILABLE"
	ErrInternal      = "INTERNAL"
	ErrNotSupported  = "NOT_SUPPORTED"
	ErrInvalidParams = "INVALID_PARAMS"
)
