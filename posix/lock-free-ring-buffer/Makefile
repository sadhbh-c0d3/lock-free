# ================================================
# Makefile for POSIX Ring Buffer Example
# Outputs binaries to bin/ directory
# ================================================

# Compiler and flags
CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -pthread
INCLUDES := -Iinclude

# Directories
SRC_DIR  := example
BIN_DIR  := bin
BUILD_DIR := build

# Files
TARGET   := $(BIN_DIR)/ringb_test
SOURCE   := $(SRC_DIR)/main.c
OBJECT   := $(BUILD_DIR)/main.o

# Default target
all: $(TARGET)

# Create necessary directories
$(BIN_DIR) $(BUILD_DIR):
	mkdir -p $@

# Compile object file
$(OBJECT): $(SOURCE) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link the final executable
$(TARGET): $(OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# Clean build artifacts
clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Phony targets
.PHONY: all clean run