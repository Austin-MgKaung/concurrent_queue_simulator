# ELE430 Producer-Consumer Model

## Author Information
- **Name:** [Your Name]
- **Student ID:** [Your ID]
- **Date:** January 2026

## Description
A C program that models a Producer-Consumer system with multiple producers,
multiple consumers, and a shared priority-aware FIFO queue.

## Files
- `config.h` - Compile-time configuration parameters
- `utils.h` / `utils.c` - Utility functions (random, system info)
- `main.c` - Program entry point and argument handling
- `Makefile` - Build instructions
- `README.md` - This file

## Compilation

### Using Make (Recommended)
```bash
make            # Build the program
make clean      # Remove build files
make test       # Build and run with test parameters
```

### Manual Compilation
```bash
gcc -Wall -Wextra -pedantic -std=c99 -o model main.c utils.c -lpthread
```

## Usage
```bash
./model <num_producers> <num_consumers> <queue_size> <timeout_seconds>
```

### Parameters
| Parameter | Description | Valid Range |
|-----------|-------------|-------------|
| num_producers | Number of producer threads | 1 to 10 |
| num_consumers | Number of consumer threads | 1 to 3 |
| queue_size | Maximum queue capacity | 1 to 20 |
| timeout_seconds | Runtime duration | 1 or more |

### Example
```bash
./model 5 3 10 60
```
Runs 5 producers, 3 consumers, queue size 10, for 60 seconds.

## Generating Log Files
```bash
./model 5 3 10 60 > logfile1.txt
./model 8 2 15 45 > logfile2.txt
```

## Compile-Time Configuration
Edit `config.h` to modify default parameters:
- `MAX_PRODUCER_WAIT` - Max seconds between producer writes (default: 2)
- `MAX_CONSUMER_WAIT` - Max seconds between consumer reads (default: 4)
- `DEBUG_MODE` - Set to 1 for verbose output (default: 0)

## Current Status
- [x] Milestone 1: Configuration and argument parsing
- [ ] Milestone 3: Queue data structure
- [ ] Milestone 4: Synchronisation
- [ ] Milestone 5: Producer threads
- [ ] Milestone 6: Consumer threads
- [ ] Milestone 7: Timeout and cleanup
- [ ] Milestone 8: Analytics