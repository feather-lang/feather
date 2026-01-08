# Makefile for Feather + Raylib example
#
# Prerequisites:
#   - libfeather.so built (run `mise build:c-shared` from project root)
#   - Run `mise setup` to download raylib, or install system-wide

FEATHER_ROOT := ../../..
FEATHER_LIB := $(FEATHER_ROOT)/bin
FEATHER_INCLUDE := $(FEATHER_ROOT)/cmd/libfeather

CC := cc
CFLAGS := -Wall -Wextra -O2 -I$(FEATHER_INCLUDE) -I$(FEATHER_LIB)
LDFLAGS :=

# Platform-specific flags
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    # Use @executable_path for runtime library resolution
    LDFLAGS += -Wl,-rpath,@executable_path/../../../bin
    PLATFORM_LIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif
ifeq ($(UNAME),Linux)
    LDFLAGS += -Wl,-rpath,'$$ORIGIN/../../../bin'
    PLATFORM_LIBS := -lGL -lpthread -ldl
endif

# Link directly to libfeather.so (not -lfeather which expects .dylib on macOS)
FEATHER_SO := $(FEATHER_LIB)/libfeather.so

# Use local raylib if RAYLIB_PATH is set (by mise)
# Link statically to avoid runtime library path issues
ifdef RAYLIB_PATH
    CFLAGS += -I$(RAYLIB_PATH)/include
    LIBS := $(RAYLIB_PATH)/lib/libraylib.a $(FEATHER_SO) -lm $(PLATFORM_LIBS)
else
    LIBS := -lraylib $(FEATHER_SO) -lm $(PLATFORM_LIBS)
endif

TARGET := game
SRCS := main.c console.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

run: $(TARGET)
	./$(TARGET) demo.tcl

clean:
	rm -f $(TARGET)
