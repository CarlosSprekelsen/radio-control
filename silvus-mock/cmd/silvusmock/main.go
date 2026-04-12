package main

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/silvus-mock/internal/config"
	"github.com/silvus-mock/internal/jsonrpc"
	"github.com/silvus-mock/internal/maintenance"
	"github.com/silvus-mock/internal/state"
)

func main() {
	log.Println("Starting Silvus Mock Radio Emulator...")

	// Load configuration
	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("Failed to load configuration: %v", err)
	}

	// Setup logging - use stdout for now
	log.SetFlags(log.LstdFlags | log.Lshortfile)

	log.Printf("Starting Silvus Mock Radio Emulator with config: %+v", cfg)

	// Initialize radio state
	radioState := state.NewRadioState(cfg)

	// Create JSON-RPC HTTP server
	jsonrpcServer := jsonrpc.NewServer(cfg, radioState)
	httpMux := http.NewServeMux()
	httpMux.HandleFunc("/streamscape_api", jsonrpcServer.HandleRequest)

	// Determine HTTP port based on dev mode
	httpPort := cfg.Network.HTTP.Port
	if cfg.Network.HTTP.DevMode {
		httpPort = 8081
		log.Println("Development mode: using port 8081")
	}

	httpServer := &http.Server{
		Addr:         fmt.Sprintf(":%d", httpPort),
		Handler:      httpMux,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
	}

	// Create maintenance TCP server
	maintenanceServer := maintenance.NewServer(cfg, radioState)

	// Start HTTP server
	go func() {
		log.Printf("Starting HTTP server on port %d", httpPort)
		if err := httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("HTTP server failed: %v", err)
		}
	}()

	// Start maintenance TCP server
	go func() {
		log.Printf("Starting maintenance TCP server on port %d", cfg.Network.Maintenance.Port)
		if err := maintenanceServer.ListenAndServe(); err != nil {
			log.Fatalf("Maintenance server failed: %v", err)
		}
	}()

	// Wait for interrupt signal
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("Shutting down servers...")

	// Shutdown HTTP server
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := httpServer.Shutdown(ctx); err != nil {
		log.Printf("HTTP server shutdown error: %v", err)
	}

	// Shutdown maintenance server
	if err := maintenanceServer.Close(); err != nil {
		log.Printf("Maintenance server shutdown error: %v", err)
	}

	// Shutdown radio state
	if err := radioState.Close(); err != nil {
		log.Printf("Radio state shutdown error: %v", err)
	}

	log.Println("Servers stopped")
}
