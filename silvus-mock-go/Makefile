# Silvus Radio Mock Makefile

.PHONY: build docker run test clean help

# Default target
all: build

# Build the application
build:
	@echo "Building silvus-mock..."
	go build -o silvusmock ./cmd/silvusmock

# Build for Docker
docker:
	@echo "Building Docker image..."
	docker build -t silvus-mock .

# Run locally with default config
run: build
	@echo "Running silvus-mock locally..."
	./silvusmock

# Run with Docker
docker-run:
	@echo "Running silvus-mock in Docker..."
	docker run --rm -p 8080:80 -p 50000:50000 \
		-e CBTIMING_CONFIG=/etc/cb-timing.yaml \
		-v $(PWD)/config/cb-timing.example.yaml:/etc/cb-timing.yaml:ro \
		silvus-mock

# Run with Docker Compose for testing with RCC
docker-compose:
	@echo "Starting services with Docker Compose..."
	docker-compose -f docker-compose.test.yml up --build

# Run tests
test:
	@echo "Running tests..."
	go test ./...

# Clean build artifacts
clean:
	@echo "Cleaning..."
	rm -f silvusmock
	docker rmi silvus-mock 2>/dev/null || true

# Show help
help:
	@echo "Available targets:"
	@echo "  build        - Build the application locally"
	@echo "  docker       - Build Docker image"
	@echo "  run          - Run locally with default config"
	@echo "  docker-run   - Run in Docker container"
	@echo "  docker-compose - Start with Docker Compose for testing"
	@echo "  test         - Run tests"
	@echo "  clean        - Clean build artifacts"
	@echo "  help         - Show this help"
