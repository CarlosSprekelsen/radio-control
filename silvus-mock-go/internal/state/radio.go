package state

import (
	"context"
	"fmt"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/silvus-mock/internal/config"
)

// RadioState represents the thread-safe state of the radio
type RadioState struct {
	mu                  sync.RWMutex
	currentFreq         string
	currentPower        int
	blackoutUntil       time.Time
	mode                string
	frequencyProfiles   []config.FrequencyProfile
	powerLimits         PowerLimits
	softBootDuration    time.Duration // Channel change blackout
	powerChangeDuration time.Duration // Power change blackout
	radioResetDuration  time.Duration // Radio reset blackout
	commandQueue        chan Command
	stopChan            chan struct{}
	wg                  sync.WaitGroup  // For graceful shutdown
	ctx                 context.Context // For cancellation
	cancel              context.CancelFunc
}

// PowerLimits holds power range limits
type PowerLimits struct {
	MinDBm int
	MaxDBm int
}

// Command represents a command to be processed by the radio state
type Command struct {
	Type      string
	Params    []string
	Response  chan CommandResponse
	Timestamp time.Time
}

// CommandResponse represents the response from a command
type CommandResponse struct {
	Result interface{} `json:"result,omitempty"`
	Error  string      `json:"error,omitempty"`
}

// NewRadioState creates a new radio state instance
func NewRadioState(cfg *config.Config) *RadioState {
	ctx, cancel := context.WithCancel(context.Background())

	rs := &RadioState{
		currentFreq:       "4700.0", // Default frequency
		currentPower:      30,       // Default power in dBm
		mode:              cfg.Mode,
		frequencyProfiles: cfg.Profiles.FrequencyProfiles,
		powerLimits: PowerLimits{
			MinDBm: cfg.Power.MinDBm,
			MaxDBm: cfg.Power.MaxDBm,
		},
		softBootDuration:    time.Duration(cfg.Timing.Blackout.SoftBootSec) * time.Second,    // Channel change blackout
		powerChangeDuration: time.Duration(cfg.Timing.Blackout.PowerChangeSec) * time.Second, // Power change blackout
		radioResetDuration:  time.Duration(cfg.Timing.Blackout.RadioResetSec) * time.Second,  // Radio reset blackout
		commandQueue:        make(chan Command, 100),
		stopChan:            make(chan struct{}),
		ctx:                 ctx,
		cancel:              cancel,
	}

	// Start the command processing worker with proper lifecycle management
	rs.wg.Add(1)
	go rs.commandWorker()

	return rs
}

// commandWorker processes commands in FIFO order
func (rs *RadioState) commandWorker() {
	defer rs.wg.Done()

	for {
		select {
		case cmd := <-rs.commandQueue:
			rs.processCommand(cmd)
		case <-rs.stopChan:
			return
		case <-rs.ctx.Done():
			return
		}
	}
}

// processCommand processes a single command
func (rs *RadioState) processCommand(cmd Command) {
	rs.mu.Lock()
	defer rs.mu.Unlock()

	// Check if we're in blackout
	// ICD §6.1.1: During soft-boot, avoid concurrent API calls
	// All commands (including reads) should return UNAVAILABLE during blackout
	if time.Now().Before(rs.blackoutUntil) {
		cmd.Response <- CommandResponse{
			Error: "UNAVAILABLE",
		}
		return
	}

	switch cmd.Type {
	case "setFreq":
		rs.handleSetFreq(cmd)
	case "getFreq":
		rs.handleGetFreq(cmd)
	case "setPower":
		rs.handleSetPower(cmd)
	case "getPower":
		rs.handleGetPower(cmd)
	case "getProfiles":
		rs.handleGetProfiles(cmd)
	case "zeroize":
		rs.handleZeroize(cmd)
	case "radioReset":
		rs.handleRadioReset(cmd)
	case "factoryReset":
		rs.handleFactoryReset(cmd)
	default:
		cmd.Response <- CommandResponse{
			Error: "INTERNAL",
		}
	}
}

// handleSetFreq handles frequency setting with soft-boot blackout
func (rs *RadioState) handleSetFreq(cmd Command) {
	if len(cmd.Params) != 1 {
		cmd.Response <- CommandResponse{
			Error: "INTERNAL",
		}
		return
	}

	freqStr := cmd.Params[0]
	if !rs.isValidFrequency(freqStr) {
		cmd.Response <- CommandResponse{
			Error: "INVALID_RANGE",
		}
		return
	}

	// Set frequency and enter soft-boot blackout
	rs.currentFreq = freqStr
	rs.blackoutUntil = time.Now().Add(rs.softBootDuration)

	cmd.Response <- CommandResponse{
		Result: []string{""},
	}
}

// handleGetFreq handles frequency reading
func (rs *RadioState) handleGetFreq(cmd Command) {
	// Note: processCommand already holds the write lock
	cmd.Response <- CommandResponse{
		Result: []string{rs.currentFreq},
	}
}

// handleSetPower handles power setting
func (rs *RadioState) handleSetPower(cmd Command) {
	if len(cmd.Params) != 1 {
		cmd.Response <- CommandResponse{
			Error: "INTERNAL",
		}
		return
	}

	powerStr := cmd.Params[0]
	power, err := strconv.Atoi(powerStr)
	if err != nil || power < rs.powerLimits.MinDBm || power > rs.powerLimits.MaxDBm {
		cmd.Response <- CommandResponse{
			Error: "INVALID_RANGE",
		}
		return
	}

	rs.currentPower = power
	cmd.Response <- CommandResponse{
		Result: []string{""},
	}
}

// handleGetPower handles power reading
func (rs *RadioState) handleGetPower(cmd Command) {
	// Note: processCommand already holds the write lock
	cmd.Response <- CommandResponse{
		Result: []string{strconv.Itoa(rs.currentPower)},
	}
}

// handleGetProfiles handles frequency profiles reading
func (rs *RadioState) handleGetProfiles(cmd Command) {
	// Note: processCommand already holds the write lock
	cmd.Response <- CommandResponse{
		Result: rs.frequencyProfiles,
	}
}

// handleZeroize handles zeroize operation
func (rs *RadioState) handleZeroize(cmd Command) {
	// Reset to defaults
	rs.currentFreq = "2490.0"
	rs.currentPower = 30
	rs.blackoutUntil = time.Time{}

	cmd.Response <- CommandResponse{
		Result: []string{""},
	}
}

// handleRadioReset handles radio reset operation
func (rs *RadioState) handleRadioReset(cmd Command) {
	// Enter radio reset blackout (CB-TIMING v0.3 §6.2: 60s)
	rs.blackoutUntil = time.Now().Add(rs.radioResetDuration)

	cmd.Response <- CommandResponse{
		Result: []string{""},
	}
}

// handleFactoryReset handles factory reset operation
func (rs *RadioState) handleFactoryReset(cmd Command) {
	// Reset to factory defaults
	rs.currentFreq = "2490.0"
	rs.currentPower = 30
	// Note: factory reset requires radio_reset to take effect per ICD

	cmd.Response <- CommandResponse{
		Result: []string{""},
	}
}

// isValidFrequency checks if a frequency is valid according to profiles
func (rs *RadioState) isValidFrequency(freqStr string) bool {
	freq, err := strconv.ParseFloat(freqStr, 64)
	if err != nil {
		return false
	}

	// Check against all frequency profiles
	for _, profile := range rs.frequencyProfiles {
		if rs.frequencyMatchesProfile(freq, profile) {
			return true
		}
	}
	return false
}

// frequencyMatchesProfile checks if a frequency matches a profile
func (rs *RadioState) frequencyMatchesProfile(freq float64, profile config.FrequencyProfile) bool {
	for _, freqRange := range profile.Frequencies {
		if rs.frequencyInRange(freq, freqRange) {
			return true
		}
	}
	return false
}

// frequencyInRange checks if a frequency is within a range
func (rs *RadioState) frequencyInRange(freq float64, freqRange string) bool {
	// Handle single frequency (e.g., "4700")
	if !strings.Contains(freqRange, ":") {
		rangeFreq, err := strconv.ParseFloat(freqRange, 64)
		if err != nil {
			return false
		}
		return freq == rangeFreq
	}

	// Handle range format (e.g., "2200:20:2380")
	parts := strings.Split(freqRange, ":")
	if len(parts) != 3 {
		return false
	}

	start, err1 := strconv.ParseFloat(parts[0], 64)
	step, err2 := strconv.ParseFloat(parts[1], 64)
	end, err3 := strconv.ParseFloat(parts[2], 64)

	if err1 != nil || err2 != nil || err3 != nil {
		return false
	}

	if step <= 0 {
		return false
	}

	// Check if frequency is within range with step tolerance
	for f := start; f <= end; f += step {
		if freq == f {
			return true
		}
	}

	// Also check if frequency is within the range bounds (for decimal frequencies)
	return freq >= start && freq <= end
}

// IsAvailable checks if the radio is available (not in blackout)
func (rs *RadioState) IsAvailable() bool {
	rs.mu.RLock()
	defer rs.mu.RUnlock()
	return time.Now().After(rs.blackoutUntil)
}

// GetStatus returns the current radio status
func (rs *RadioState) GetStatus() (string, int, bool) {
	rs.mu.RLock()
	defer rs.mu.RUnlock()
	return rs.currentFreq, rs.currentPower, rs.IsAvailable()
}

// ExecuteCommand executes a command and returns the response
func (rs *RadioState) ExecuteCommand(cmdType string, params []string) CommandResponse {
	response := make(chan CommandResponse, 1)
	cmd := Command{
		Type:      cmdType,
		Params:    params,
		Response:  response,
		Timestamp: time.Now(),
	}

	// Add backpressure handling and timeout
	select {
	case rs.commandQueue <- cmd:
		// Command queued successfully
		select {
		case resp := <-response:
			return resp
		case <-time.After(30 * time.Second):
			return CommandResponse{Error: "INTERNAL"}
		case <-rs.ctx.Done():
			return CommandResponse{Error: "UNAVAILABLE"}
		}
	case <-time.After(5 * time.Second):
		// Command queue full or system busy
		return CommandResponse{Error: "BUSY"}
	case <-rs.ctx.Done():
		// System shutting down
		return CommandResponse{Error: "UNAVAILABLE"}
	}
}

// Close shuts down the radio state gracefully
func (rs *RadioState) Close() error {
	// Cancel context to stop all operations
	rs.cancel()

	// Close stop channel
	close(rs.stopChan)

	// Wait for goroutines to finish with timeout
	done := make(chan struct{})
	go func() {
		rs.wg.Wait()
		close(done)
	}()

	select {
	case <-done:
		// Clean shutdown
		return nil
	case <-time.After(10 * time.Second):
		// Force shutdown after timeout
		return fmt.Errorf("shutdown timeout")
	}
}
