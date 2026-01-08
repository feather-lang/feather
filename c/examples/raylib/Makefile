# Makefile for Feather + Raylib example
#
# Prerequisites:
#   - Run `mise run:raylib` from project root (builds everything and runs)
#   - Or `mise build:raylib` to just build

FEATHER_ROOT := ../../..
FEATHER_INCLUDE := $(FEATHER_ROOT)/cmd/libfeather

CC := cc
CFLAGS := -Wall -Wextra -O2 -I$(FEATHER_INCLUDE)
LDFLAGS :=

# Platform-specific flags
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    # Use @executable_path for runtime library resolution (local directory)
    LDFLAGS += -Wl,-rpath,@executable_path
    PLATFORM_LIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif
ifeq ($(UNAME),Linux)
    LDFLAGS += -Wl,-rpath,'$$ORIGIN'
    PLATFORM_LIBS := -lGL -lpthread -ldl
endif

# Link to local libfeather.so (copied by mise build:raylib)
FEATHER_SO := ./libfeather.so

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
