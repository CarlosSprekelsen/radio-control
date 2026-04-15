package config

import (
	"fmt"
	"os"
	"strconv"

	"gopkg.in/yaml.v2"
)

// Config represents the complete configuration for the Silvus mock
type Config struct {
	Network  NetworkConfig  `yaml:"network"`
	Profiles ProfilesConfig `yaml:"profiles"`
	Power    PowerConfig    `yaml:"power"`
	Timing   TimingConfig   `yaml:"timing"`
	Mode     string         `yaml:"mode"`
}

// NetworkConfig holds network-related settings
type NetworkConfig struct {
	HTTP        HTTPConfig        `yaml:"http"`
	Maintenance MaintenanceConfig `yaml:"maintenance"`
}

// HTTPConfig holds HTTP server settings
type HTTPConfig struct {
	Port         int    `yaml:"port"`
	ServerHeader string `yaml:"serverHeader"`
	DevMode      bool   `yaml:"devMode"`
}

// MaintenanceConfig holds maintenance TCP server settings
type MaintenanceConfig struct {
	Port         int      `yaml:"port"`
	AllowedCIDRs []string `yaml:"allowedCidrs"`
}

// ProfilesConfig holds frequency profile settings
type ProfilesConfig struct {
	FrequencyProfiles []FrequencyProfile `yaml:"frequencyProfiles"`
}

// FrequencyProfile represents a frequency profile from ICD
type FrequencyProfile struct {
	Frequencies []string `yaml:"frequencies"`
	Bandwidth   string   `yaml:"bandwidth"`
	AntennaMask string   `yaml:"antenna_mask"`
}

// PowerConfig holds power-related settings
type PowerConfig struct {
	MinDBm int `yaml:"minDbm"`
	MaxDBm int `yaml:"maxDbm"`
}

// TimingConfig holds all timing-related settings
type TimingConfig struct {
	Blackout BlackoutConfig `yaml:"blackout"`
	Commands CommandsConfig `yaml:"commands"`
	Backoff  BackoffConfig  `yaml:"backoff"`
}

// BlackoutConfig holds soft-boot blackout settings
type BlackoutConfig struct {
	SoftBootSec    int `yaml:"softBootSec"`    // Channel change blackout (CB-TIMING v0.3 §6.2: 30s)
	PowerChangeSec int `yaml:"powerChangeSec"` // Power change blackout (CB-TIMING v0.3 §6.2: 5s)
	RadioResetSec  int `yaml:"radioResetSec"`  // Radio reset blackout (CB-TIMING v0.3 §6.2: 60s)
}

// CommandsConfig holds command timeout settings
type CommandsConfig struct {
	SetPower    TimeoutConfig `yaml:"setPower"`    // CB-TIMING v0.3 §5: 10s
	SetChannel  TimeoutConfig `yaml:"setChannel"`  // CB-TIMING v0.3 §5: 30s
	SelectRadio TimeoutConfig `yaml:"selectRadio"` // CB-TIMING v0.3 §5: 5s
	Read        TimeoutConfig `yaml:"read"`        // CB-TIMING v0.3 §5: 5s
}

// TimeoutConfig holds timeout settings for a command
type TimeoutConfig struct {
	TimeoutSec int `yaml:"timeoutSec"`
}

// BackoffConfig holds backoff settings
type BackoffConfig struct {
	BusyBaseMs int `yaml:"busyBaseMs"`
}

// Load loads configuration from file and environment variables
func Load() (*Config, error) {
	// Load default configuration
	cfg := getDefaultConfig()

	// Load from default config file
	if err := loadFromFile(cfg, "config/default.yaml"); err != nil {
		// If default config doesn't exist, continue with defaults
		fmt.Printf("Warning: Could not load default config: %v\n", err)
	}

	// Load from config file if CBTIMING_CONFIG is set
	if cbTimingConfig := os.Getenv("CBTIMING_CONFIG"); cbTimingConfig != "" {
		if err := loadFromFile(cfg, cbTimingConfig); err != nil {
			return nil, fmt.Errorf("failed to load CB-TIMING config from %s: %v", cbTimingConfig, err)
		}
	}

	// Override with environment variables
	applyEnvOverrides(cfg)

	// Validate configuration
	if err := validateConfig(cfg); err != nil {
		return nil, fmt.Errorf("configuration validation failed: %v", err)
	}

	return cfg, nil
}

// getDefaultConfig returns the default configuration
func getDefaultConfig() *Config {
	return &Config{
		Network: NetworkConfig{
			HTTP: HTTPConfig{
				Port:         80,
				ServerHeader: "",
				DevMode:      false,
			},
			Maintenance: MaintenanceConfig{
				Port:         50000,
				AllowedCIDRs: []string{"127.0.0.0/8", "172.20.0.0/16"},
			},
		},
		Profiles: ProfilesConfig{
			FrequencyProfiles: []FrequencyProfile{
				{
					Frequencies: []string{"2200:20:2380", "4700"},
					Bandwidth:   "-1",
					AntennaMask: "15",
				},
				{
					Frequencies: []string{"4420:40:4700"},
					Bandwidth:   "-1",
					AntennaMask: "3",
				},
				{
					Frequencies: []string{"4700:20:4980"},
					Bandwidth:   "-1",
					AntennaMask: "12",
				},
			},
		},
		Power: PowerConfig{
			MinDBm: 0,
			MaxDBm: 39,
		},
		Timing: TimingConfig{
			Blackout: BlackoutConfig{
				SoftBootSec:    30, // CB-TIMING v0.3 §6.2: Channel change blackout
				PowerChangeSec: 5,  // CB-TIMING v0.3 §6.2: Power change blackout
				RadioResetSec:  60, // CB-TIMING v0.3 §6.2: Radio reset blackout
			},
			Commands: CommandsConfig{
				SetPower:    TimeoutConfig{TimeoutSec: 10}, // CB-TIMING v0.3 §5
				SetChannel:  TimeoutConfig{TimeoutSec: 30}, // CB-TIMING v0.3 §5
				SelectRadio: TimeoutConfig{TimeoutSec: 5},  // CB-TIMING v0.3 §5
				Read:        TimeoutConfig{TimeoutSec: 5},  // CB-TIMING v0.3 §5
			},
			Backoff: BackoffConfig{
				BusyBaseMs: 1000,
			},
		},
		Mode: "normal",
	}
}

// loadFromFile loads configuration from a YAML file
func loadFromFile(cfg *Config, filename string) error {
	data, err := os.ReadFile(filename)
	if err != nil {
		return err
	}

	return yaml.Unmarshal(data, cfg)
}

// applyEnvOverrides applies environment variable overrides
func applyEnvOverrides(cfg *Config) {
	if mode := os.Getenv("SILVUS_MOCK_MODE"); mode != "" {
		cfg.Mode = mode
	}

	if softBootTime := os.Getenv("SILVUS_MOCK_SOFT_BOOT_TIME"); softBootTime != "" {
		if sec, err := strconv.Atoi(softBootTime); err == nil {
			cfg.Timing.Blackout.SoftBootSec = sec
		}
	}
}

// validateConfig validates the configuration
func validateConfig(cfg *Config) error {
	// Validate mode
	validModes := []string{"normal", "degraded", "offline"}
	if !contains(validModes, cfg.Mode) {
		return fmt.Errorf("invalid mode %s, must be one of: %v", cfg.Mode, validModes)
	}

	// Validate power range
	if cfg.Power.MinDBm < 0 || cfg.Power.MaxDBm > 39 || cfg.Power.MinDBm > cfg.Power.MaxDBm {
		return fmt.Errorf("invalid power range: min=%d, max=%d (must be 0-39)", cfg.Power.MinDBm, cfg.Power.MaxDBm)
	}

	// Validate timing values (CB-TIMING bounds)
	if cfg.Timing.Blackout.SoftBootSec <= 0 || cfg.Timing.Blackout.SoftBootSec > 60 {
		return fmt.Errorf("soft boot time %d seconds is outside reasonable range [1, 60]", cfg.Timing.Blackout.SoftBootSec)
	}

	if cfg.Timing.Commands.SetPower.TimeoutSec <= 0 || cfg.Timing.Commands.SetPower.TimeoutSec > 30 {
		return fmt.Errorf("setPower timeout %d seconds is outside reasonable range [1, 30]", cfg.Timing.Commands.SetPower.TimeoutSec)
	}

	if cfg.Timing.Commands.SetChannel.TimeoutSec <= 0 || cfg.Timing.Commands.SetChannel.TimeoutSec > 60 {
		return fmt.Errorf("setChannel timeout %d seconds is outside reasonable range [1, 60]", cfg.Timing.Commands.SetChannel.TimeoutSec)
	}

	// Validate frequency profiles
	if len(cfg.Profiles.FrequencyProfiles) == 0 {
		return fmt.Errorf("at least one frequency profile must be configured")
	}

	return nil
}

// contains checks if a string slice contains a specific string
func contains(slice []string, item string) bool {
	for _, s := range slice {
		if s == item {
			return true
		}
	}
	return false
}
