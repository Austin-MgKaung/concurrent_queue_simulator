# ELE430 Coursework: Producer-Consumer Model
# Student: Kaung
# Date: Jan 27, 2026
#
# Makefile: Build Automation
# * Manages compilation, linking with pthread, and testing shortcuts.

# --- Compiler Settings ---
CC = gcc
# -pedantic and -Wall are crucial for catching potential logic errors early
# -pthread is required for POSIX threads (producer/consumer)
CFLAGS = -Wall -Wextra -pedantic -std=c99 -pthread
LDFLAGS = -pthread

# --- File Definitions ---
TARGET = model

# Source files (update this list as we add queue.c, producer.c, etc.)
SRCS = main.c utils.c
OBJS = $(SRCS:.c=.o)

# Header dependencies (recompile if these change)
HDRS = config.h utils.h

# --- Build Rules ---

# Default target: build the executable
all: $(TARGET)

# Linking: Combines object files into the final binary
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@echo "Build complete."

# Compilation: Turns .c files into .o files
# Depends on HDRS so we recompile if config.h changes
%.o: %.c $(HDRS)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# --- Utility Targets ---

# Cleans up build artifacts to force a fresh start
clean:
	@echo "Cleaning..."
	rm -f $(OBJS) $(TARGET)

# Shortcut for a clean rebuild
rebuild: clean all

# Quick test with valid arguments (Producers=5, Consumers=3, Queue=10, Time=30)
test: $(TARGET)
	@echo "Running test configuration..."
	./$(TARGET) 5 3 10 30

# Debug build: Adds symbols (-g) and enables verbose logs (DEBUG_MODE)
debug: CFLAGS += -g -DDEBUG_MODE=1
debug: rebuild
	@echo "Debug build complete."

.PHONY: all clean rebuild test debug