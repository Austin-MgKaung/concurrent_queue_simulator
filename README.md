#+#+#+#+------------------------------+----------------------------------------------------+
# ELE430 Producer-Consumer Model

## Author Information
- **Name:** Kaung
- **Student ID:**
- **Date:** January 2026

## Overview (What this program demonstrates)
This program simulates a multi-producer / multi-consumer system using a
bounded, thread-safe queue with priority and aging. It is designed to
show correct synchronization, graceful shutdown, and performance reporting.

If you are reviewing this project, the fastest way to see all features
is to run the Quick Start and then the Feature Tour below.

## Key Features
- Multi-producer / multi-consumer pthread model with bounded queue.
- Thread-safe queue using mutex + semaphores with blocking enqueue/dequeue.
- Priority-aware dequeue with aging to prevent starvation.
- Accurate blocking detection for producers/consumers.
- Graceful shutdown on timeout or SIGINT/SIGTERM.
- Background analytics sampler (queue occupancy, throughput, blocking).
- Final summary report with optimization recommendations.
- CSV export of queue occupancy over time.
- Optional ncurses TUI dashboard (`-v`) with live stats and sparkline.
- Debug logging with runtime levels (`-d 0..3`).

## Quick Start (Recommended)
```bash
make
./model 5 3 10 30
```
You should see:
- startup system info
- producers/consumers logging activity
- final thread summary
- analytics report + CSV export

## Feature Tour (for full appreciation)

### 1) Live TUI dashboard (visual mode)
```bash
./model -v 5 3 10 30
```
What to notice:
- queue slots with color-coded priorities
- producer/consumer counters updating live
- throughput bars and occupancy sparkline

### 2) Debug logging levels
```bash
./model -d 1 2 2 5 10   # ERROR
./model -d 2 2 2 5 10   # INFO
./model -d 3 2 2 5 10   # TRACE
```
Tip: pipe to `grep` to focus on debug lines:
```bash
./model -d 3 2 2 5 10 2>&1 | grep -E "^\[DBG"
```

### 3) Graceful shutdown by signal
Start any run, then press `Ctrl+C` (SIGINT).
The program should:
- announce shutdown
- stop threads cleanly
- still print the final summary/report

### 4) Analytics + CSV export
Each run exports a CSV file named:
```
queue_occupancy_p<producers>_c<consumers>_q<queue>.csv
```
You can open it in Excel/Python to plot occupancy vs time.

## Usage
```bash
./model [-v] [-d <level>] <num_producers> <num_consumers> <queue_size> <timeout_seconds>
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
./model -v -d 2 5 3 10 60
```

## Build Instructions

### Using Make (Recommended)
```bash
make
make clean
make test
make visual
```

### Manual Compilation (if needed)
```bash
gcc -Wall -Wextra -pedantic -std=c99 -pthread -D_POSIX_C_SOURCE=200809L \
  -o model main.c utils.c cli.c queue.c producer.c consumer.c analytics.c tui.c \
  -lncursesw
```

## Files (Quick Map)
- `main.c`       - Orchestrates lifecycle: init -> spawn -> run -> shutdown -> report
- `queue.c/.h`   - Thread-safe bounded queue with priority + aging
- `producer.c/.h` / `consumer.c/.h` - Worker thread logic + per-thread stats
- `analytics.c/.h` - Sampling thread + aggregated metrics + CSV export
- `tui.c/.h`     - ncurses dashboard
- `cli.c/.h`     - CLI parsing, validation, report formatting
- `utils.c/.h`   - timing, randomization, system info, debug macros
- `config.h`     - compile-time limits and ranges

## Notes for Reviewers
- Blocking behavior is measured accurately using `sem_trywait` then fallback to `sem_wait`.
- Queue dequeue is priority-aware with aging (`AGING_INTERVAL_MS`) to prevent starvation.
- The shutdown path is signal-safe and avoids unsafe stdio in handlers.

