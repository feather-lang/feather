# hosts/c/Makefile - Build the C host with GLib support
#
# Usage:
#   make           - Build tclc binary
#   make clean     - Remove build artifacts
#   make test      - Build and run a simple test

.PHONY: all clean test

# Configuration
CC ?= clang
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O2

# GLib flags via pkg-config
GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)

# Directories (relative to this Makefile)
ROOT_DIR = ../..
CORE_DIR = $(ROOT_DIR)/core
BUILD_DIR = $(ROOT_DIR)/build
BIN_DIR = $(ROOT_DIR)/bin

# Source files
HOST_SRCS = host.c object.c vars.c arena.c channel.c main.c
CORE_SRCS = $(wildcard $(CORE_DIR)/*.c)

# Object files
HOST_OBJS = $(patsubst %.c,$(BUILD_DIR)/host_%.o,$(HOST_SRCS))
CORE_OBJS = $(patsubst $(CORE_DIR)/%.c,$(BUILD_DIR)/%.o,$(CORE_SRCS))

# Library
LIBTCLC = $(BUILD_DIR)/libtclc.a

# Binary
TCLC = $(BIN_DIR)/tclc

# Default target
all: $(TCLC)

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile core source files
$(BUILD_DIR)/%.o: $(CORE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(CORE_DIR) -c -o $@ $<

# Create static library from core
$(LIBTCLC): $(CORE_OBJS)
	ar rcs $@ $^

# Compile host source files with GLib
$(BUILD_DIR)/host_%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GLIB_CFLAGS) -I$(CORE_DIR) -c -o $@ $<

# Link tclc executable
$(TCLC): $(HOST_OBJS) $(LIBTCLC) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(HOST_OBJS) $(LIBTCLC) $(GLIB_LIBS)

# Clean
clean:
	rm -f $(HOST_OBJS)
	rm -f $(TCLC)

# Full clean (including core objects)
distclean: clean
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)

# Simple test
test: $(TCLC)
	@echo 'puts "Hello from tclc with GLib!"' | $(TCLC)
	@echo "Test passed!"

# Show configuration
info:
	@echo "CC:          $(CC)"
	@echo "CFLAGS:      $(CFLAGS)"
	@echo "GLIB_CFLAGS: $(GLIB_CFLAGS)"
	@echo "GLIB_LIBS:   $(GLIB_LIBS)"
	@echo "HOST_SRCS:   $(HOST_SRCS)"
	@echo "HOST_OBJS:   $(HOST_OBJS)"
	@echo "CORE_OBJS:   $(CORE_OBJS)"
