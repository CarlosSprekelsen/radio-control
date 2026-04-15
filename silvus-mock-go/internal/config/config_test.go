package config

import (
	"os"
	"testing"
)

func TestGetDefaultConfig(t *testing.T) {
	cfg := getDefaultConfig()

	// Test network defaults
	if cfg.Network.HTTP.Port != 80 {
		t.Errorf("Expected HTTP port 80, got %d", cfg.Network.HTTP.Port)
	}
	if cfg.Network.Maintenance.Port != 50000 {
		t.Errorf("Expected maintenance port 50000, got %d", cfg.Network.Maintenance.Port)
	}

	// Test power defaults
	if cfg.Power.MinDBm != 0 || cfg.Power.MaxDBm != 39 {
		t.Errorf("Expected power range 0-39, got %d-%d", cfg.Power.MinDBm, cfg.Power.MaxDBm)
	}

	// Test timing defaults
	if cfg.Timing.Commands.SetPower.TimeoutSec != 10 {
		t.Errorf("Expected setPower timeout 10s, got %d", cfg.Timing.Commands.SetPower.TimeoutSec)
	}
	if cfg.Timing.Commands.SetChannel.TimeoutSec != 30 {
		t.Errorf("Expected setChannel timeout 30s, got %d", cfg.Timing.Commands.SetChannel.TimeoutSec)
	}

	// Test frequency profiles
	if len(cfg.Profiles.FrequencyProfiles) == 0 {
		t.Error("Expected at least one frequency profile")
	}

	// Test allowed CIDRs
	if len(cfg.Network.Maintenance.AllowedCIDRs) != 2 {
		t.Errorf("Expected 2 allowed CIDRs, got %d", len(cfg.Network.Maintenance.AllowedCIDRs))
	}
}

func TestLoadConfigFromFile(t *testing.T) {
	// Test loading from existing config file
	cfg := &Config{}
	err := loadFromFile(cfg, "config/default.yaml")
	if err != nil {
		t.Logf("Could not load config file (expected in some environments): %v", err)
		return
	}

	// Verify some expected values from default.yaml
	if cfg.Network.HTTP.Port != 8080 {
		t.Errorf("Expected HTTP port 8080 from config file, got %d", cfg.Network.HTTP.Port)
	}
}

func TestLoadConfigFromNonExistentFile(t *testing.T) {
	cfg := &Config{}
	err := loadFromFile(cfg, "non-existent-file.yaml")
	if err == nil {
		t.Error("Expected error when loading non-existent file")
	}
}

func TestApplyEnvOverrides(t *testing.T) {
	cfg := getDefaultConfig()

	// Test mode override
	os.Setenv("SILVUS_MOCK_MODE", "degraded")
	defer os.Unsetenv("SILVUS_MOCK_MODE")

	applyEnvOverrides(cfg)

	if cfg.Mode != "degraded" {
		t.Errorf("Expected mode 'degraded', got '%s'", cfg.Mode)
	}
}

func TestApplySoftBootTimeOverride(t *testing.T) {
	cfg := getDefaultConfig()
	originalTime := cfg.Timing.Blackout.SoftBootSec

	os.Setenv("SILVUS_MOCK_SOFT_BOOT_TIME", "10")
	defer os.Unsetenv("SILVUS_MOCK_SOFT_BOOT_TIME")

	applyEnvOverrides(cfg)

	if cfg.Timing.Blackout.SoftBootSec != 10 {
		t.Errorf("Expected soft boot time 10s, got %d", cfg.Timing.Blackout.SoftBootSec)
	}

	// Test invalid value
	cfg = getDefaultConfig()
	os.Setenv("SILVUS_MOCK_SOFT_BOOT_TIME", "invalid")
	defer os.Unsetenv("SILVUS_MOCK_SOFT_BOOT_TIME")

	applyEnvOverrides(cfg)

	if cfg.Timing.Blackout.SoftBootSec != originalTime {
		t.Errorf("Expected original time %ds for invalid env var, got %d", originalTime, cfg.Timing.Blackout.SoftBootSec)
	}
}

func TestValidateConfig(t *testing.T) {
	tests := []struct {
		name    string
		config  func() *Config
		wantErr bool
	}{
		{
			name: "valid config",
			config: func() *Config {
				return getDefaultConfig()
			},
			wantErr: false,
		},
		{
			name: "invalid mode",
			config: func() *Config {
				cfg := getDefaultConfig()
				cfg.Mode = "invalid"
				return cfg
			},
			wantErr: true,
		},
		{
			name: "invalid power range",
			config: func() *Config {
				cfg := getDefaultConfig()
				cfg.Power.MinDBm = 50
				cfg.Power.MaxDBm = 40
				return cfg
			},
			wantErr: true,
		},
		{
			name: "power out of range",
			config: func() *Config {
				cfg := getDefaultConfig()
				cfg.Power.MaxDBm = 50
				return cfg
			},
			wantErr: true,
		},
		{
			name: "invalid soft boot time",
			config: func() *Config {
				cfg := getDefaultConfig()
				cfg.Timing.Blackout.SoftBootSec = 100
				return cfg
			},
			wantErr: true,
		},
		{
			name: "invalid setPower timeout",
			config: func() *Config {
				cfg := getDefaultConfig()
				cfg.Timing.Commands.SetPower.TimeoutSec = 50
				return cfg
			},
			wantErr: true,
		},
		{
			name: "invalid setChannel timeout",
			config: func() *Config {
				cfg := getDefaultConfig()
				cfg.Timing.Commands.SetChannel.TimeoutSec = 100
				return cfg
			},
			wantErr: true,
		},
		{
			name: "empty frequency profiles",
			config: func() *Config {
				cfg := getDefaultConfig()
				cfg.Profiles.FrequencyProfiles = []FrequencyProfile{}
				return cfg
			},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := validateConfig(tt.config())
			if (err != nil) != tt.wantErr {
				t.Errorf("validateConfig() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

func TestContains(t *testing.T) {
	tests := []struct {
		slice []string
		item  string
		want  bool
	}{
		{[]string{"normal", "degraded", "offline"}, "normal", true},
		{[]string{"normal", "degraded", "offline"}, "invalid", false},
		{[]string{}, "test", false},
		{[]string{"single"}, "single", true},
	}

	for _, tt := range tests {
		t.Run("", func(t *testing.T) {
			if got := contains(tt.slice, tt.item); got != tt.want {
				t.Errorf("contains() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestLoadIntegration(t *testing.T) {
	// Test the full Load function
	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() failed: %v", err)
	}

	// Basic sanity checks
	if cfg.Mode == "" {
		t.Error("Expected non-empty mode")
	}
	if cfg.Network.HTTP.Port <= 0 {
		t.Error("Expected positive HTTP port")
	}
	if len(cfg.Profiles.FrequencyProfiles) == 0 {
		t.Error("Expected at least one frequency profile")
	}
}

func TestFrequencyProfileStructure(t *testing.T) {
	cfg := getDefaultConfig()

	for i, profile := range cfg.Profiles.FrequencyProfiles {
		if len(profile.Frequencies) == 0 {
			t.Errorf("Profile %d: expected at least one frequency", i)
		}
		if profile.Bandwidth == "" {
			t.Errorf("Profile %d: expected non-empty bandwidth", i)
		}
		if profile.AntennaMask == "" {
			t.Errorf("Profile %d: expected non-empty antenna mask", i)
		}

		// Test that frequencies are valid strings
		for j, freq := range profile.Frequencies {
			if freq == "" {
				t.Errorf("Profile %d, frequency %d: expected non-empty frequency", i, j)
			}
		}
	}
}
