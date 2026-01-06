# Makefile for Feather + Raylib example
#
# Prerequisites:
#   - libfeather.so built (run `mise build` from project root)
#   - raylib installed (apt install libraylib-dev, brew install raylib, etc.)

FEATHER_ROOT := ../../..
FEATHER_LIB := $(FEATHER_ROOT)/bin

CC := cc
CFLAGS := -Wall -Wextra -O2 -I$(FEATHER_LIB)
LDFLAGS := -L$(FEATHER_LIB) -Wl,-rpath,'$$ORIGIN/$(FEATHER_LIB)'
LIBS := -lfeather -lraylib -lm

# Platform-specific flags
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif
ifeq ($(UNAME),Linux)
    LIBS += -lGL -lpthread -ldl
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
