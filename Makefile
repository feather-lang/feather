# TCLC Makefile
# TCL Core Implementation - Build and Test Orchestration

.PHONY: all clean test test-c test-integration oracle oracle-all diff diff-all loop prompt help features deps check-tools harness libs build build-all build-go

# Configuration
CC ?= clang
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O2
GO = go
TCLSH ?= tclsh

# GLib-2.0 flags (for C host)
GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0 2>/dev/null)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0 2>/dev/null)

# Directories
CORE_DIR = core
HOST_C_DIR = hosts/c
HOST_GO_DIR = hosts/go
SPEC_DIR = spec
HARNESS_DIR = harness
BUILD_DIR = build
BIN_DIR = bin

# Source files
CORE_SRCS = $(CORE_DIR)/lexer.c $(CORE_DIR)/parser.c $(CORE_DIR)/subst.c \
            $(CORE_DIR)/eval.c $(CORE_DIR)/builtins.c $(CORE_DIR)/builtin_expr.c \
            $(CORE_DIR)/builtin_global.c $(CORE_DIR)/builtin_upvar.c $(CORE_DIR)/builtin_uplevel.c \
            $(CORE_DIR)/builtin_string.c $(CORE_DIR)/builtin_dict.c $(CORE_DIR)/builtin_exec.c \
            $(CORE_DIR)/builtin_file.c $(CORE_DIR)/builtin_break.c $(CORE_DIR)/builtin_list.c \
            $(CORE_DIR)/builtin_error.c $(CORE_DIR)/builtin_proc.c $(CORE_DIR)/builtin_var.c \
            $(CORE_DIR)/builtin_array.c $(CORE_DIR)/builtin_info.c $(CORE_DIR)/builtin_control.c \
            $(CORE_DIR)/builtin_subst.c $(CORE_DIR)/builtin_chan.c $(CORE_DIR)/builtin_regexp.c \
            $(CORE_DIR)/builtin_event.c
HOST_C_SRCS = $(HOST_C_DIR)/host.c $(HOST_C_DIR)/object.c $(HOST_C_DIR)/vars.c \
              $(HOST_C_DIR)/arena.c $(HOST_C_DIR)/channel.c $(HOST_C_DIR)/main.c

# Object files
CORE_OBJS = $(patsubst $(CORE_DIR)/%.c,$(BUILD_DIR)/%.o,$(CORE_SRCS))
HOST_C_OBJS = $(patsubst $(HOST_C_DIR)/%.c,$(BUILD_DIR)/host_%.o,$(HOST_C_SRCS))

# Libraries
LIBTCLC_STATIC = $(BUILD_DIR)/libtclc.a
LIBTCLC_DYNAMIC = $(BUILD_DIR)/libtclc.dylib

# Binaries
TCLC = $(BIN_DIR)/tclc
TCLGO = $(BIN_DIR)/tclgo
HARNESS = $(BIN_DIR)/harness

# Default target
all: help

# Help
help:
	@echo "TCLC - TCL Core Implementation"
	@echo ""
	@echo "Build targets:"
	@echo "  make build          - Build bin/tclc (C host)"
	@echo "  make build-go       - Build bin/tclgo (Go host)"
	@echo "  make build-all      - Build all binaries and libraries"
	@echo "  make libs           - Build libraries only (build/libtclc.a, build/libtclc.dylib)"
	@echo "  make harness        - Build bin/harness (test harness)"
	@echo "  make clean          - Remove build artifacts"
	@echo ""
	@echo "Test targets:"
	@echo "  make test           - Run all tests"
	@echo "  make test-c         - Run C unit tests only"
	@echo "  make test-integration - Run integration tests"
	@echo "  make diff FEATURE=x - Run differential tests for feature x"
	@echo "  make diff-all       - Run differential tests for all features"
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
$(HARNESS): $(BIN_DIR)/.stamp $(wildcard $(HARNESS_DIR)/*.go)
	cd $(HARNESS_DIR) && $(GO) build -o ../$(HARNESS) .

harness: $(HARNESS)

# Directory creation (use stamp files to avoid circular dependency)
$(BUILD_DIR)/.stamp:
	mkdir -p $(BUILD_DIR)
	touch $(BUILD_DIR)/.stamp

$(BIN_DIR)/.stamp:
	mkdir -p $(BIN_DIR)
	touch $(BIN_DIR)/.stamp

# Core object files
$(BUILD_DIR)/%.o: $(CORE_DIR)/%.c $(CORE_DIR)/tclc.h $(CORE_DIR)/internal.h $(BUILD_DIR)/.stamp
	$(CC) $(CFLAGS) -I$(CORE_DIR) -c -o $@ $<

# Host C object files (require glib-2.0)
$(BUILD_DIR)/host_%.o: $(HOST_C_DIR)/%.c $(CORE_DIR)/tclc.h $(BUILD_DIR)/.stamp
	$(CC) $(CFLAGS) $(GLIB_CFLAGS) -I$(CORE_DIR) -c -o $@ $<

# Static library
$(LIBTCLC_STATIC): $(CORE_OBJS)
	ar rcs $@ $^

# Dynamic library
$(LIBTCLC_DYNAMIC): $(CORE_OBJS)
	$(CC) $(CFLAGS) -dynamiclib -o $@ $^

# Library targets
libs: $(LIBTCLC_STATIC) $(LIBTCLC_DYNAMIC)

# Build tclc interpreter (links statically, requires glib-2.0)
$(TCLC): $(BIN_DIR)/.stamp $(HOST_C_OBJS) $(LIBTCLC_STATIC)
	$(CC) $(CFLAGS) -o $@ $(HOST_C_OBJS) $(LIBTCLC_STATIC) $(GLIB_LIBS)

# Build Go host
$(TCLGO): $(BIN_DIR)/.stamp $(LIBTCLC_STATIC)
	@if [ -f "$(HOST_GO_DIR)/go.mod" ]; then \
		cd $(HOST_GO_DIR) && CGO_LDFLAGS="-L../../$(BUILD_DIR)" $(GO) build -o ../../$(TCLGO) .; \
	else \
		echo "No Go module yet in $(HOST_GO_DIR)/"; \
		exit 1; \
	fi

# Build target (all binaries)
build: $(TCLC)
	@echo "Built $(TCLC)"

# Build all including Go host
build-all: $(TCLC) $(TCLGO) libs
	@echo "Built $(TCLC), $(TCLGO), and libraries"

# Build Go host only
build-go: $(TCLGO)
	@echo "Built $(TCLGO)"

# Clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	rm -f *.o
	rm -f $(HARNESS_DIR)/results/*.json
	rm -f prompt.md

# C Unit Tests
test-c: $(BUILD_DIR)/.stamp
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
diff: $(HARNESS) $(TCLC)
ifndef FEATURE
	$(error FEATURE is required. Usage: make diff FEATURE=lexer)
endif
	@TCLC_INTERP=$(TCLC) $(HARNESS) diff $(FEATURE)

# Run differential tests for all features
diff-all: $(HARNESS) $(TCLC)
	@TCLC_INTERP=$(TCLC) $(HARNESS) diff

# Agent Prompt Generation
prompt: $(HARNESS)
ifndef FEATURE
	$(error FEATURE is required. Usage: make prompt FEATURE=lexer)
endif
	@$(HARNESS) prompt $(FEATURE)

# Feedback Loop - Iterate until all tests pass
loop: $(HARNESS) $(TCLC)
ifndef FEATURE
	$(error FEATURE is required. Usage: make loop FEATURE=lexer)
endif
	@TCLSH=$(TCLSH) TCLC_INTERP=$(TCLC) $(HARNESS) loop $(FEATURE)

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
