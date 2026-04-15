package jsonrpc

import (
	"encoding/json"
	"log"
	"net/http"
	"time"

	"github.com/silvus-mock/internal/commands"
	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

// Server handles JSON-RPC HTTP requests
type Server struct {
	config           *config.Config
	state            *state.RadioState
	extensibleServer *commands.ExtensibleJSONRPCServer
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

// ErrorResponse represents a JSON-RPC 2.0 error response
type ErrorResponse struct {
	Code    int         `json:"code"`
	Message string      `json:"message"`
	Data    interface{} `json:"data,omitempty"`
}

// NewServer creates a new JSON-RPC server
func NewServer(cfg *config.Config, radioState *state.RadioState) *Server {
	return &Server{
		config:           cfg,
		state:            radioState,
		extensibleServer: commands.NewExtensibleJSONRPCServer(cfg, radioState),
	}
}

// HandleRequest handles HTTP POST requests to /streamscape_api
func (s *Server) HandleRequest(w http.ResponseWriter, r *http.Request) {
	start := time.Now()

	// Delegate to extensible server for all command processing
	s.extensibleServer.HandleRequest(w, r)

	// Log request processing time (extract method from request if possible)
	var method string
	if r.Body != nil {
		var req Request
		if err := json.NewDecoder(r.Body).Decode(&req); err == nil {
			method = req.Method
		}
	}

	duration := time.Since(start)
	log.Printf("JSON-RPC request processed: method=%s, duration=%v", method, duration)
}

// processRequest processes a JSON-RPC request
func (s *Server) processRequest(req *Request) *Response {
	// Map JSON-RPC method to internal command
	var cmdType string
	var params []string

	switch req.Method {
	case "freq":
		if len(req.Params) > 0 {
			cmdType = "setFreq"
			params = req.Params
		} else {
			cmdType = "getFreq"
		}
	case "power_dBm":
		if len(req.Params) > 0 {
			cmdType = "setPower"
			params = req.Params
		} else {
			cmdType = "getPower"
		}
	case "supported_frequency_profiles":
		cmdType = "getProfiles"
	default:
		return &Response{
			JSONRPC: "2.0",
			Error:   "Method not found",
			ID:      req.ID,
		}
	}

	// Execute command directly (timeout handled in ExecuteCommand)
	cmdResponse := s.state.ExecuteCommand(cmdType, params)
	if cmdResponse.Error != "" {
		return &Response{
			JSONRPC: "2.0",
			Error:   cmdResponse.Error,
			ID:      req.ID,
		}
	}
	return &Response{
		JSONRPC: "2.0",
		Result:  cmdResponse.Result,
		ID:      req.ID,
	}
}

// getTimeoutForMethod returns the appropriate timeout for a method
func (s *Server) getTimeoutForMethod(method string) time.Duration {
	switch method {
	case "freq":
		return time.Duration(s.config.Timing.Commands.SetChannel.TimeoutSec) * time.Second
	case "power_dBm":
		return time.Duration(s.config.Timing.Commands.SetPower.TimeoutSec) * time.Second
	default:
		return time.Duration(s.config.Timing.Commands.Read.TimeoutSec) * time.Second
	}
}

// writeErrorResponse writes an error response
func (s *Server) writeErrorResponse(w http.ResponseWriter, code int, message string, id interface{}) {
	response := &Response{
		JSONRPC: "2.0",
		Error: &ErrorResponse{
			Code:    code,
			Message: message,
		},
		ID: id,
	}

	w.WriteHeader(http.StatusBadRequest)
	json.NewEncoder(w).Encode(response)
}
