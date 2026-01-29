# ELE430 Coursework: Producer-Consumer Model
# Student: Kaung
# Date: Jan 27, 2026
#
# Makefile: Build Automation
# * Manages compilation, linking with pthread, and testing shortcuts.

# --- Compiler Settings ---
CC = gcc
# -Werror: Treat all warnings as errors (Demonstrates code quality)
# -D_POSIX_C_SOURCE: Required for sleep/time functions
CFLAGS = -Wall -Wextra -pedantic -std=c99 -pthread -D_POSIX_C_SOURCE=200809L -Werror
LDFLAGS = -pthread -lncursesw

# --- File Definitions ---
TARGET = model

# Source files
# Added cli.c (Argument Parsing) and tui.c (Visualization)
SRCS = main.c utils.c cli.c queue.c producer.c consumer.c analytics.c tui.c

# Object files (generated from source files)
OBJS = $(SRCS:.c=.o)

# Header files (dependencies)
# Added cli.h and tui.h
HDRS = config.h utils.h cli.h queue.h producer.h consumer.h analytics.h tui.h

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

# Cleans up build artifacts and CSV traces
clean:
	@echo "Cleaning..."
	rm -f $(OBJS) $(TARGET) *.csv

# Shortcut for a clean rebuild
rebuild: clean all

# Quick test with valid arguments (Producers=5, Consumers=3, Queue=10, Time=30)
test: $(TARGET)
	@echo "Running test configuration..."
	./$(TARGET) 5 3 10 30

# Visual test shortcut (Enables TUI mode)
visual: $(TARGET)
	@echo "Running visual mode..."
	./$(TARGET) -v 5 3 20 60

# Install required system packages (Ubuntu/Debian)
deps:
	@echo "Installing dependencies..."
	sudo apt-get update
	sudo apt-get install -y gcc make libncursesw5-dev
	@echo "Dependencies installed."

# Run the full test bench (57 tests)
bench: $(TARGET)
	@echo "Running test bench..."
	./test_bench.sh

# Memory leak check with valgrind
valgrind: $(TARGET)
	@echo "Running valgrind memory check..."
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
		./$(TARGET) 1 1 5 2

.PHONY: all clean rebuild test visual deps bench valgrind