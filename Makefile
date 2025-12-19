# TCLC Makefile
# TCL Core Implementation - Build and Test Orchestration

.PHONY: all clean test test-c test-integration oracle oracle-all diff loop prompt help features deps check-tools harness

# Configuration
CC ?= clang
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O2
GO = go
TCLSH ?= tclsh

# Directories
CORE_DIR = core
HOST_DIR = hosts/go
SPEC_DIR = spec
HARNESS_DIR = harness
BUILD_DIR = build

# Harness binary
HARNESS = $(BUILD_DIR)/harness

# Default target
all: help

# Help
help:
	@echo "TCLC - TCL Core Implementation"
	@echo ""
	@echo "Build targets:"
	@echo "  make build          - Build the C core and Go host"
	@echo "  make harness        - Build the test harness"
	@echo "  make clean          - Remove build artifacts"
	@echo ""
	@echo "Test targets:"
	@echo "  make test           - Run all tests"
	@echo "  make test-c         - Run C unit tests only"
	@echo "  make test-integration - Run integration tests"
	@echo "  make diff FEATURE=x - Run differential tests for feature x"
	@echo ""
	@echo "Oracle targets:"
	@echo "  make oracle-all     - Generate oracle for all features"
	@echo "  make oracle FEATURE=x - Generate oracle for feature x"
	@echo ""
	@echo "Agent targets:"
	@echo "  make loop FEATURE=x - Run feedback loop for feature x"
	@echo "  make prompt FEATURE=x - Generate agent prompt for feature x"
	@echo ""
	@echo "Info targets:"
	@echo "  make features       - List all features and status"
	@echo "  make deps FEATURE=x - Show dependencies for feature x"
	@echo "  make check-tools    - Verify required tools are installed"

# Build harness
$(HARNESS): $(BUILD_DIR) $(wildcard $(HARNESS_DIR)/*.go)
	cd $(HARNESS_DIR) && $(GO) build -o ../$(HARNESS) .

harness: $(HARNESS)

# Build
build: $(BUILD_DIR)
	@echo "Building C core..."
	@if ls $(CORE_DIR)/*.c 1> /dev/null 2>&1; then \
		$(CC) $(CFLAGS) -c $(CORE_DIR)/*.c -I$(CORE_DIR); \
		mv *.o $(BUILD_DIR)/; \
	else \
		echo "No C source files yet in $(CORE_DIR)/"; \
	fi
	@echo "Building Go host..."
	@if [ -f "$(HOST_DIR)/go.mod" ]; then \
		cd $(HOST_DIR) && $(GO) build ./...; \
	else \
		echo "No Go module yet in $(HOST_DIR)/"; \
	fi

# Clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f *.o
	rm -f $(HARNESS_DIR)/results/*.json
	rm -f prompt.md

# C Unit Tests
test-c: $(BUILD_DIR)
	@echo "Running C unit tests..."
	@if [ -d "$(CORE_DIR)/test" ] && ls $(CORE_DIR)/test/*.c 1> /dev/null 2>&1; then \
		$(CC) $(CFLAGS) -I$(CORE_DIR) $(CORE_DIR)/test/*.c -lm -o $(BUILD_DIR)/test_runner && \
		$(BUILD_DIR)/test_runner; \
	else \
		echo "No C tests found in $(CORE_DIR)/test/"; \
	fi

# Integration Tests
test-integration:
	@echo "Running integration tests..."
	@if [ -f "$(HOST_DIR)/go.mod" ]; then \
		cd $(HOST_DIR) && $(GO) test -v ./...; \
	else \
		echo "No Go module yet in $(HOST_DIR)/"; \
	fi

# All Tests
test: test-c test-integration

# Oracle Generation - Generate expected outputs from real tclsh
oracle-all: $(HARNESS)
	@TCLSH=$(TCLSH) $(HARNESS) oracle

oracle: $(HARNESS)
ifndef FEATURE
	$(error FEATURE is required. Usage: make oracle FEATURE=lexer)
endif
	@TCLSH=$(TCLSH) $(HARNESS) oracle $(FEATURE)

# Differential Testing - Compare our implementation against oracle
diff: $(HARNESS)
ifndef FEATURE
	$(error FEATURE is required. Usage: make diff FEATURE=lexer)
endif
	@$(HARNESS) diff $(FEATURE)

# Agent Prompt Generation
prompt: $(HARNESS)
ifndef FEATURE
	$(error FEATURE is required. Usage: make prompt FEATURE=lexer)
endif
	@$(HARNESS) prompt $(FEATURE)

# Feedback Loop - Iterate until all tests pass
loop: $(HARNESS)
ifndef FEATURE
	$(error FEATURE is required. Usage: make loop FEATURE=lexer)
endif
	@TCLSH=$(TCLSH) $(HARNESS) loop $(FEATURE)

# Feature Information
features: $(HARNESS)
	@$(HARNESS) features

deps: $(HARNESS)
ifndef FEATURE
	$(error FEATURE is required. Usage: make deps FEATURE=subst)
endif
	@$(HARNESS) deps $(FEATURE)

# Check for required tools
check-tools:
	@echo "Checking required tools..."
	@which $(CC) > /dev/null 2>&1 && echo "✓ $(CC)" || echo "✗ $(CC) not found"
	@which $(GO) > /dev/null 2>&1 && echo "✓ $(GO)" || echo "✗ $(GO) not found"
	@which $(TCLSH) > /dev/null 2>&1 && echo "✓ $(TCLSH)" || echo "✗ $(TCLSH) not found (needed for oracle)"
