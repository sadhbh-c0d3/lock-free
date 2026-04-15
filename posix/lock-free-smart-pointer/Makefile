# ================================================
# Makefile for POSIXARC (Atomic Shared Pointer)
# Binaries → bin/, GDB support included
# ================================================

CC      := gcc
CFLAGS  := -std=c11 -O3 -Wall -Wextra -pthread
INCLUDES := -Iinclude

SRC_DIR  := example
BIN_DIR  := bin
BUILD_DIR := build

TARGET   := $(BIN_DIR)/posixarc_test
SOURCE   := $(SRC_DIR)/main.c
OBJECT   := $(BUILD_DIR)/main.o

# Default target
all: $(TARGET)

# Create directories
$(BIN_DIR) $(BUILD_DIR):
	mkdir -p $@

# Compile
$(OBJECT): $(SOURCE) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link
$(TARGET): $(OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# Clean
clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)

# Run normally
run: $(TARGET)
	./$(TARGET)

# Phony targets
.PHONY: all clean run gdb