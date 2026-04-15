package commands

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

// ExtensibleJSONRPCServer provides an extensible JSON-RPC server that can handle multiple command types
type ExtensibleJSONRPCServer struct {
	registry *CommandRegistry
	config   *config.Config
}

// Request represents a JSON-RPC 2.0 request
type Request struct {
	JSONRPC string      `json:"jsonrpc"`
	Method  string      `json:"method"`
	Params  []string    `json:"params,omitempty"`
	ID      interface{} `json:"id"`
}

// Response represents a JSON-RPC 2.0 response
type Response struct {
	JSONRPC string      `json:"jsonrpc"`
	Result  interface{} `json:"result,omitempty"`
	Error   interface{} `json:"error,omitempty"`
	ID      interface{} `json:"id"`
}

// NewExtensibleJSONRPCServer creates a new extensible JSON-RPC server
func NewExtensibleJSONRPCServer(cfg *config.Config, radioState *state.RadioState) *ExtensibleJSONRPCServer {
	registry := NewCommandRegistry()

	// Register core commands
	RegisterCoreCommands(registry, radioState, cfg)

	// Register optional commands (ICD §6.2)
	RegisterOptionalCommands(registry, radioState, cfg)

	// Register GPS commands (ICD §6.2)
	RegisterGPSCommands(registry, radioState, cfg)

	// Register any other optional commands here
	// RegisterOtherCommands(registry, radioState, cfg)

	return &ExtensibleJSONRPCServer{
		registry: registry,
		config:   cfg,
	}
}

// HandleRequest handles HTTP POST requests to the JSON-RPC endpoint
func (s *ExtensibleJSONRPCServer) HandleRequest(w http.ResponseWriter, r *http.Request) {
	// Set response headers
	w.Header().Set("Content-Type", "application/json")
	if s.config.Network.HTTP.ServerHeader != "" {
		w.Header().Set("Server", s.config.Network.HTTP.ServerHeader)
	}

	// Only accept POST requests
	if r.Method != http.MethodPost {
		s.writeErrorResponse(w, -32600, "Invalid Request", nil)
		return
	}

	// Parse JSON-RPC request
	var req Request
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		s.writeErrorResponse(w, -32700, "Parse error", nil)
		return
	}

	// Validate JSON-RPC version
	if req.JSONRPC != "2.0" {
		s.writeErrorResponse(w, -32600, "Invalid Request", req.ID)
		return
	}

	// Process the request
	response := s.processRequest(&req)

	// Write response
	if err := json.NewEncoder(w).Encode(response); err != nil {
		fmt.Printf("Failed to encode response: %v", err)
		return
	}
}

// processRequest processes a JSON-RPC request using the command registry
func (s *ExtensibleJSONRPCServer) processRequest(req *Request) *Response {
	// Find command handler
	handler, exists := s.registry.Get(req.Method)
	if !exists {
		return &Response{
			JSONRPC: "2.0",
			Error: map[string]interface{}{
				"code":    -32601, // Method not found
				"message": "Method not found",
			},
			ID: req.ID,
		}
	}

	// Create context with command information
	ctx := context.WithValue(context.Background(), "commandName", req.Method)

	// Execute command
	result, err := handler.Handle(ctx, req.Params)
	if err != nil {
		// Handle command errors
		if cmdErr, ok := err.(*CommandError); ok {
			return &Response{
				JSONRPC: "2.0",
				Error: map[string]interface{}{
					"code":    -32602, // Invalid params
					"message": cmdErr.Code,
				},
				ID: req.ID,
			}
		}

		// Handle other errors
		return &Response{
			JSONRPC: "2.0",
			Error: map[string]interface{}{
				"code":    -32603, // Internal error
				"message": "INTERNAL",
			},
			ID: req.ID,
		}
	}

	return &Response{
		JSONRPC: "2.0",
		Result:  result,
		ID:      req.ID,
	}
}

// writeErrorResponse writes an error response
func (s *ExtensibleJSONRPCServer) writeErrorResponse(w http.ResponseWriter, code int, message string, id interface{}) {
	response := &Response{
		JSONRPC: "2.0",
		Error: map[string]interface{}{
			"code":    code,
			"message": message,
		},
		ID: id,
	}

	w.WriteHeader(http.StatusBadRequest)
	json.NewEncoder(w).Encode(response)
}

// GetAvailableCommands returns a list of available commands
func (s *ExtensibleJSONRPCServer) GetAvailableCommands() []CommandInfo {
	commands := make([]CommandInfo, 0, len(s.registry.handlers))

	for _, handler := range s.registry.handlers {
		commands = append(commands, CommandInfo{
			Name:        handler.GetName(),
			Description: handler.GetDescription(),
			ReadOnly:    handler.IsReadOnly(),
			Blackout:    handler.RequiresBlackout(),
		})
	}

	return commands
}

// CommandInfo provides information about a command
type CommandInfo struct {
	Name        string `json:"name"`
	Description string `json:"description"`
	ReadOnly    bool   `json:"read_only"`
	Blackout    bool   `json:"requires_blackout"`
}

// AddCustomCommand allows adding custom commands at runtime
func (s *ExtensibleJSONRPCServer) AddCustomCommand(handler CommandHandler) {
	s.registry.Register(handler)
}

// RemoveCommand allows removing commands at runtime
func (s *ExtensibleJSONRPCServer) RemoveCommand(commandName string) {
	delete(s.registry.handlers, commandName)
}

// Example custom command implementation
type CustomCommandHandler struct {
	name        string
	description string
	readOnly    bool
	blackout    bool
	handlerFunc func(ctx context.Context, params []string) (interface{}, error)
}

// NewCustomCommandHandler creates a custom command handler
func NewCustomCommandHandler(name, description string, readOnly, blackout bool, handlerFunc func(ctx context.Context, params []string) (interface{}, error)) *CustomCommandHandler {
	return &CustomCommandHandler{
		name:        name,
		description: description,
		readOnly:    readOnly,
		blackout:    blackout,
		handlerFunc: handlerFunc,
	}
}

func (h *CustomCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	return h.handlerFunc(ctx, params)
}

func (h *CustomCommandHandler) GetName() string {
	return h.name
}

func (h *CustomCommandHandler) GetDescription() string {
	return h.description
}

func (h *CustomCommandHandler) IsReadOnly() bool {
	return h.readOnly
}

func (h *CustomCommandHandler) RequiresBlackout() bool {
	return h.blackout
}

// Example usage of custom commands
func ExampleCustomCommands(server *ExtensibleJSONRPCServer) {
	// Add a custom status command
	statusHandler := NewCustomCommandHandler(
		"custom_status",
		"Get custom system status",
		true,  // read-only
		false, // no blackout
		func(ctx context.Context, params []string) (interface{}, error) {
			return map[string]interface{}{
				"status":    "operational",
				"uptime":    "24h",
				"version":   "1.0.0",
				"timestamp": "2025-01-01T00:00:00Z",
			}, nil
		},
	)
	server.AddCustomCommand(statusHandler)

	// Add a custom reboot command
	rebootHandler := NewCustomCommandHandler(
		"custom_reboot",
		"Reboot the system",
		false, // not read-only
		true,  // requires blackout
		func(ctx context.Context, params []string) (interface{}, error) {
			// Simulate reboot
			return []string{""}, nil
		},
	)
	server.AddCustomCommand(rebootHandler)
}
