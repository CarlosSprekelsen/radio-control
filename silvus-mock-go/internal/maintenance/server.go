package maintenance

import (
	"encoding/json"
	"fmt"
	"log"
	"net"
	"strings"
	"sync"
	"time"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

// Server handles maintenance TCP connections
type Server struct {
	config            *config.Config
	state             *state.RadioState
	listener          net.Listener
	stopChan          chan struct{}
	activeConnections map[string]net.Conn
	connectionsMutex  sync.RWMutex
	maxConnections    int
	connectionTimeout time.Duration
}

// Request represents a JSON-RPC request over TCP
type Request struct {
	JSONRPC string      `json:"jsonrpc"`
	Method  string      `json:"method"`
	Params  []string    `json:"params,omitempty"`
	ID      interface{} `json:"id"`
}

// Response represents a JSON-RPC response over TCP
type Response struct {
	JSONRPC string      `json:"jsonrpc"`
	Result  interface{} `json:"result,omitempty"`
	Error   interface{} `json:"error,omitempty"`
	ID      interface{} `json:"id"`
}

// NewServer creates a new maintenance server
func NewServer(cfg *config.Config, radioState *state.RadioState) *Server {
	return &Server{
		config:            cfg,
		state:             radioState,
		stopChan:          make(chan struct{}),
		activeConnections: make(map[string]net.Conn),
		maxConnections:    10, // Limit concurrent connections
		connectionTimeout: 30 * time.Second,
	}
}

// ListenAndServe starts the maintenance TCP server
func (s *Server) ListenAndServe() error {
	listener, err := net.Listen("tcp", fmt.Sprintf(":%d", s.config.Network.Maintenance.Port))
	if err != nil {
		return fmt.Errorf("failed to listen on port %d: %v", s.config.Network.Maintenance.Port, err)
	}
	s.listener = listener

	log.Printf("Maintenance server listening on port %d", s.config.Network.Maintenance.Port)

	for {
		select {
		case <-s.stopChan:
			return nil
		default:
			conn, err := listener.Accept()
			if err != nil {
				if strings.Contains(err.Error(), "use of closed network connection") {
					return nil
				}
				log.Printf("Failed to accept connection: %v", err)
				continue
			}

			// Check if connection is from allowed CIDR
			if !s.isAllowedConnection(conn) {
				log.Printf("Rejected connection from %s (not in allowed CIDRs)", conn.RemoteAddr())
				conn.Close()
				continue
			}

			// Handle connection in goroutine
			go s.handleConnection(conn)
		}
	}
}

// handleConnection handles a single TCP connection
func (s *Server) handleConnection(conn net.Conn) {
	defer conn.Close()

	// Set connection timeout
	conn.SetDeadline(time.Now().Add(30 * time.Second))

	// Read JSON-RPC request
	var req Request
	decoder := json.NewDecoder(conn)
	if err := decoder.Decode(&req); err != nil {
		log.Printf("Failed to decode JSON-RPC request: %v", err)
		s.writeErrorResponse(conn, -32700, "Parse error", nil)
		return
	}

	// Validate JSON-RPC version
	if req.JSONRPC != "2.0" {
		s.writeErrorResponse(conn, -32600, "Invalid Request", req.ID)
		return
	}

	// Process maintenance command
	response := s.processMaintenanceRequest(&req)

	// Write response
	encoder := json.NewEncoder(conn)
	if err := encoder.Encode(response); err != nil {
		log.Printf("Failed to encode response: %v", err)
		return
	}

	log.Printf("Maintenance command processed: method=%s, client=%s", req.Method, conn.RemoteAddr())
}

// processMaintenanceRequest processes maintenance commands
func (s *Server) processMaintenanceRequest(req *Request) *Response {
	var cmdType string
	var params []string

	switch req.Method {
	case "zeroize":
		cmdType = "zeroize"
	case "radio_reset":
		cmdType = "radioReset"
	case "factory_reset":
		cmdType = "factoryReset"
	default:
		return &Response{
			JSONRPC: "2.0",
			Error:   "Method not found",
			ID:      req.ID,
		}
	}

	// Execute command
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

// isAllowedConnection checks if the connection is from an allowed CIDR
func (s *Server) isAllowedConnection(conn net.Conn) bool {
	clientAddr := conn.RemoteAddr()
	host, _, err := net.SplitHostPort(clientAddr.String())
	if err != nil {
		return false
	}

	clientIP := net.ParseIP(host)
	if clientIP == nil {
		return false
	}

	// Check against allowed CIDRs
	for _, cidrStr := range s.config.Network.Maintenance.AllowedCIDRs {
		_, network, err := net.ParseCIDR(cidrStr)
		if err != nil {
			log.Printf("Invalid CIDR in config: %s", cidrStr)
			continue
		}
		if network.Contains(clientIP) {
			return true
		}
	}

	return false
}

// writeErrorResponse writes an error response
func (s *Server) writeErrorResponse(conn net.Conn, code int, message string, id interface{}) {
	response := &Response{
		JSONRPC: "2.0",
		Error: map[string]interface{}{
			"code":    code,
			"message": message,
		},
		ID: id,
	}

	encoder := json.NewEncoder(conn)
	encoder.Encode(response)
}

// Close shuts down the maintenance server
func (s *Server) Close() error {
	select {
	case <-s.stopChan:
		// Already closed
		return nil
	default:
		close(s.stopChan)
	}

	if s.listener != nil {
		return s.listener.Close()
	}
	return nil
}
