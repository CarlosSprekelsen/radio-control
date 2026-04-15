package main

import (
	"fmt"
	"log"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/state"
)

func main() {
	log.Println("Testing radio state directly...")

	// Load configuration
	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("Failed to load configuration: %v", err)
	}

	// Initialize radio state
	radioState := state.NewRadioState(cfg)

	// Test read power command
	log.Println("Testing read power...")
	response := radioState.ExecuteCommand("getPower", []string{})
	fmt.Printf("Response: %+v\n", response)

	// Test set power command
	log.Println("Testing set power...")
	response = radioState.ExecuteCommand("setPower", []string{"25"})
	fmt.Printf("Response: %+v\n", response)

	// Test read power again
	log.Println("Testing read power again...")
	response = radioState.ExecuteCommand("getPower", []string{})
	fmt.Printf("Response: %+v\n", response)

	log.Println("Test completed.")
}
