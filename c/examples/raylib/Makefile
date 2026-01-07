# Makefile for Feather + Raylib example
#
# Prerequisites:
#   - libfeather.so built (run `mise build` from project root)
#   - Run `mise setup` to download raylib, or install system-wide

FEATHER_ROOT := ../../..
FEATHER_LIB := $(FEATHER_ROOT)/bin

CC := cc
CFLAGS := -Wall -Wextra -O2 -I$(FEATHER_LIB)
LDFLAGS := -L$(FEATHER_LIB)

# Platform-specific flags
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    LDFLAGS += -Wl,-rpath,@loader_path/$(FEATHER_LIB)
    PLATFORM_LIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif
ifeq ($(UNAME),Linux)
    LDFLAGS += -Wl,-rpath,'$$ORIGIN/$(FEATHER_LIB)'
    PLATFORM_LIBS := -lGL -lpthread -ldl
endif

# Use local raylib if RAYLIB_PATH is set (by mise)
# Link statically to avoid runtime library path issues
ifdef RAYLIB_PATH
    CFLAGS += -I$(RAYLIB_PATH)/include
    LIBS := $(RAYLIB_PATH)/lib/libraylib.a -lfeather -lm $(PLATFORM_LIBS)
else
    LIBS := -lraylib -lfeather -lm $(PLATFORM_LIBS)
endif

TARGET := game
SRCS := main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

run: $(TARGET)
	./$(TARGET) demo.tcl

clean:
	rm -f $(TARGET)
