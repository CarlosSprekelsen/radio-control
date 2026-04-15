package commands

import (
	"context"
	"strconv"
	"time"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

// GPSCommandHandler handles GPS-related commands
type GPSCommandHandler struct {
	state      *state.RadioState
	config     *config.Config
	location   GPSCoordinates
	lastUpdate time.Time
}

// GPSCoordinates represents GPS location data
type GPSCoordinates struct {
	Latitude  float64   `json:"latitude"`
	Longitude float64   `json:"longitude"`
	Altitude  float64   `json:"altitude"`
	Accuracy  float64   `json:"accuracy"`
	Timestamp time.Time `json:"timestamp"`
}

// GPSMode represents GPS operational mode
type GPSMode struct {
	Enabled    bool   `json:"enabled"`
	LockStatus string `json:"lock_status"`
	Satellites int    `json:"satellites"`
}

// NewGPSCommandHandler creates a new GPS command handler
func NewGPSCommandHandler(radioState *state.RadioState, cfg *config.Config) *GPSCommandHandler {
	return &GPSCommandHandler{
		state:  radioState,
		config: cfg,
		location: GPSCoordinates{
			Latitude:  40.7128, // Default to NYC
			Longitude: -74.0060,
			Altitude:  10.0,
			Accuracy:  3.0,
			Timestamp: time.Now(),
		},
		lastUpdate: time.Now(),
	}
}

// Handle processes GPS commands
func (h *GPSCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	// Get subcommand from context
	subcommand := ctx.Value("subcommand").(string)

	switch subcommand {
	case "coordinates":
		return h.handleCoordinates(params)
	case "mode":
		return h.handleMode(params)
	case "time":
		return h.handleTime(params)
	default:
		return nil, &CommandError{Code: ErrNotSupported, Message: "Unknown GPS subcommand: " + subcommand}
	}
}

// handleCoordinates handles GPS coordinates commands
func (h *GPSCommandHandler) handleCoordinates(params []string) (interface{}, error) {
	if len(params) == 0 {
		// Read coordinates
		h.updateLocation() // Simulate GPS update
		return []GPSCoordinates{h.location}, nil
	}

	// Set coordinates (simulate GPS update)
	if len(params) != 3 {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "Expected 3 parameters: latitude, longitude, altitude"}
	}

	lat, err := strconv.ParseFloat(params[0], 64)
	if err != nil {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "Invalid latitude: " + params[0]}
	}

	lon, err := strconv.ParseFloat(params[1], 64)
	if err != nil {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "Invalid longitude: " + params[1]}
	}

	alt, err := strconv.ParseFloat(params[2], 64)
	if err != nil {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "Invalid altitude: " + params[2]}
	}

	// Validate ranges
	if lat < -90 || lat > 90 {
		return nil, &CommandError{Code: ErrInvalidRange, Message: "Latitude must be between -90 and 90"}
	}
	if lon < -180 || lon > 180 {
		return nil, &CommandError{Code: ErrInvalidRange, Message: "Longitude must be between -180 and 180"}
	}

	h.location = GPSCoordinates{
		Latitude:  lat,
		Longitude: lon,
		Altitude:  alt,
		Accuracy:  3.0,
		Timestamp: time.Now(),
	}
	h.lastUpdate = time.Now()

	return []string{""}, nil // Success response
}

// handleMode handles GPS mode commands
func (h *GPSCommandHandler) handleMode(params []string) (interface{}, error) {
	if len(params) == 0 {
		// Read GPS mode
		mode := GPSMode{
			Enabled:    true,
			LockStatus: "locked",
			Satellites: 8,
		}
		return []GPSMode{mode}, nil
	}

	// Set GPS mode
	if len(params) != 1 {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "Expected 1 parameter: enabled"}
	}

	enabled, err := strconv.ParseBool(params[0])
	if err != nil {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "Invalid boolean value: " + params[0]}
	}

	// Simulate GPS mode change
	_ = enabled // In real implementation, this would control GPS hardware

	return []string{""}, nil // Success response
}

// handleTime handles GPS time commands
func (h *GPSCommandHandler) handleTime(params []string) (interface{}, error) {
	if len(params) == 0 {
		// Read GPS time
		gpsTime := time.Now().Unix()
		return []string{strconv.FormatInt(gpsTime, 10)}, nil
	}

	// Set GPS time
	if len(params) != 1 {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "Expected 1 parameter: unix_timestamp"}
	}

	timestamp, err := strconv.ParseInt(params[0], 10, 64)
	if err != nil {
		return nil, &CommandError{Code: ErrInvalidParams, Message: "Invalid timestamp: " + params[0]}
	}

	// Validate timestamp (reasonable range)
	if timestamp < 946684800 || timestamp > 4102444800 { // 2000-2100
		return nil, &CommandError{Code: ErrInvalidRange, Message: "Timestamp out of reasonable range"}
	}

	// Simulate GPS time setting
	_ = timestamp // In real implementation, this would set GPS time

	return []string{""}, nil // Success response
}

// updateLocation simulates GPS location updates
func (h *GPSCommandHandler) updateLocation() {
	// Simulate small random movement
	now := time.Now()
	if now.Sub(h.lastUpdate) > time.Second {
		// Add small random variation to simulate GPS drift
		h.location.Latitude += (float64(now.UnixNano()%100) - 50) / 1000000.0
		h.location.Longitude += (float64(now.UnixNano()%100) - 50) / 1000000.0
		h.location.Timestamp = now
		h.lastUpdate = now
	}
}

// GetName returns the command name
func (h *GPSCommandHandler) GetName() string {
	return "gps"
}

// GetDescription returns the command description
func (h *GPSCommandHandler) GetDescription() string {
	return "GPS commands (coordinates, mode, time)"
}

// IsReadOnly returns false (GPS can be configured)
func (h *GPSCommandHandler) IsReadOnly() bool {
	return false
}

// RequiresBlackout returns false (GPS commands don't trigger blackout)
func (h *GPSCommandHandler) RequiresBlackout() bool {
	return false
}

// GpsCoordinatesCommandHandler handles specific GPS coordinates commands
type GpsCoordinatesCommandHandler struct {
	*GPSCommandHandler
}

// NewGpsCoordinatesCommandHandler creates a new GPS coordinates command handler
func NewGpsCoordinatesCommandHandler(radioState *state.RadioState, cfg *config.Config) *GpsCoordinatesCommandHandler {
	return &GpsCoordinatesCommandHandler{
		GPSCommandHandler: NewGPSCommandHandler(radioState, cfg),
	}
}

// Handle processes GPS coordinates commands
func (h *GpsCoordinatesCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	return h.handleCoordinates(params)
}

// GetName returns the command name
func (h *GpsCoordinatesCommandHandler) GetName() string {
	return "gps_coordinates"
}

// GetDescription returns the command description
func (h *GpsCoordinatesCommandHandler) GetDescription() string {
	return "Get/set GPS coordinates (latitude, longitude, altitude)"
}

// GpsModeCommandHandler handles specific GPS mode commands
type GpsModeCommandHandler struct {
	*GPSCommandHandler
}

// NewGpsModeCommandHandler creates a new GPS mode command handler
func NewGpsModeCommandHandler(radioState *state.RadioState, cfg *config.Config) *GpsModeCommandHandler {
	return &GpsModeCommandHandler{
		GPSCommandHandler: NewGPSCommandHandler(radioState, cfg),
	}
}

// Handle processes GPS mode commands
func (h *GpsModeCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	return h.handleMode(params)
}

// GetName returns the command name
func (h *GpsModeCommandHandler) GetName() string {
	return "gps_mode"
}

// GetDescription returns the command description
func (h *GpsModeCommandHandler) GetDescription() string {
	return "Get/set GPS operational mode"
}

// GpsTimeCommandHandler handles specific GPS time commands
type GpsTimeCommandHandler struct {
	*GPSCommandHandler
}

// NewGpsTimeCommandHandler creates a new GPS time command handler
func NewGpsTimeCommandHandler(radioState *state.RadioState, cfg *config.Config) *GpsTimeCommandHandler {
	return &GpsTimeCommandHandler{
		GPSCommandHandler: NewGPSCommandHandler(radioState, cfg),
	}
}

// Handle processes GPS time commands
func (h *GpsTimeCommandHandler) Handle(ctx context.Context, params []string) (interface{}, error) {
	return h.handleTime(params)
}

// GetName returns the command name
func (h *GpsTimeCommandHandler) GetName() string {
	return "gps_time"
}

// GetDescription returns the command description
func (h *GpsTimeCommandHandler) GetDescription() string {
	return "Get/set GPS time (Unix timestamp)"
}

// RegisterGPSCommands registers GPS commands in the registry
func RegisterGPSCommands(registry *CommandRegistry, radioState *state.RadioState, cfg *config.Config) {
	registry.Register(NewGpsCoordinatesCommandHandler(radioState, cfg))
	registry.Register(NewGpsModeCommandHandler(radioState, cfg))
	registry.Register(NewGpsTimeCommandHandler(radioState, cfg))
}
