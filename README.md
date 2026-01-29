# ELE430 Producer-Consumer Model

## Author Information
- **Name:** Kaung
- **Date:** January 2026

## Overview

A multi-threaded producer-consumer simulation built in C using POSIX threads.
Producers generate random data with priorities and push to a bounded queue.
Consumers dequeue the highest-priority item first. The system demonstrates
thread synchronization, graceful shutdown, and performance analysis.

## Features

| Feature | Description |
|---|---|
| Multi-threaded | Up to 10 producers and 3 consumers running concurrently |
| Thread-safe queue | Mutex for buffer protection, semaphores for blocking |
| Priority dequeue | Consumers always take the highest-priority item first |
| Priority aging | Low-priority items gain priority over time to prevent starvation. Configurable at runtime (`-a <ms>`, 0 to disable) |
| Graceful shutdown | Timeout-based or Ctrl+C (SIGINT/SIGTERM), all threads joined cleanly |
| Live TUI dashboard | ncurses visual mode with color-coded queue, throughput bars, sparkline |
| Debug logging | 4 levels (OFF, ERROR, INFO, TRACE) controlled at runtime |
| Analytics | Background sampling, final report, optimization recommendations |
| CSV export | Queue occupancy over time, importable into Excel/Python |
| Reproducible runs | `-s <seed>` flag for deterministic testing |
| Input validation | All CLI arguments validated with `strtol` (rejects non-numeric input) |
| Help flag | `-h` / `--help` displays usage and exits cleanly |
| Test bench | 72 automated tests covering all corner cases |
| CI pipeline | GitHub Actions runs the full test suite and valgrind memory check on every push |
| Memory safety | Valgrind leak check integrated into CI (`make valgrind`) |

## Setup on a Fresh Machine (Linux Mint / Ubuntu / Debian)

Choose **one** of the two routes below depending on how you received the project.

### Route A: From GitHub (Clone)

```bash
# 1. Install git (if not already installed)
sudo apt-get update && sudo apt-get install -y git

# 2. Clone the repository
git clone https://github.com/Austin-MgKaung/concurrent_queue_simulator.git
cd concurrent_queue_simulator

# 3. Install build dependencies
make deps

# 4. Build
make rebuild

# 5. Run
./model 5 3 10 30
```

### Route B: From ZIP File

```bash
# 1. Extract the zip (replace with your actual filename)
unzip concurrent_queue_simulator.zip
cd concurrent_queue_simulator

# 2. Install build dependencies (gcc, make, ncurses)
make deps

# 3. Make the test bench executable (zip may lose permissions)
chmod +x test_bench.sh

# 4. Build
make rebuild

# 5. Run
./model 5 3 10 30
```

Both routes are identical after step 2 — `make deps` handles all system packages.

You should see `Build complete.` with zero warnings.

### Run the Test Suite (Optional)

```bash
make bench
```

Runs 72 automated tests. You should see `All tests passed.`

## Usage

```
./model [-h] [-v] [-d <level>] [-s <seed>] [-a <ms>] <producers> <consumers> <queue_size> <timeout>
```

### Parameters

| Parameter | Description | Valid Range |
|---|---|---|
| `producers` | Number of producer threads | 1 to 10 |
| `consumers` | Number of consumer threads | 1 to 3 |
| `queue_size` | Maximum queue capacity | 1 to 20 |
| `timeout` | Simulation duration in seconds | 1 or more |

### Optional Flags

| Flag | Description |
|---|---|
| `-h`, `--help` | Show usage information and exit |
| `-v` | Enable the live ncurses TUI dashboard |
| `-d <level>` | Debug verbosity: 0=OFF, 1=ERROR, 2=INFO, 3=TRACE |
| `-s <seed>` | Set RNG seed for reproducible/deterministic runs |
| `-a <ms>` | Priority aging interval in milliseconds (default: 500, 0=disabled) |

Flags can appear in any order before the positional arguments.

## Examples

### Basic console run
```bash
./model 3 2 10 15
```

### Visual dashboard mode
```bash
./model -v 5 3 20 60
```
What you'll see:
- Color-coded queue slots (red=high priority, yellow=medium, green=low)
- Live producer/consumer statistics updating in real time
- Throughput bars and queue occupancy sparkline
- Press Ctrl+C to stop

### Debug mode (see thread lifecycle)
```bash
./model -d 2 2 2 5 10
```
Debug output goes to stderr. Filter with:
```bash
./model -d 3 2 2 5 10 2>&1 | grep "^\[DBG"
```

### Reproducible run (same output every time)
```bash
./model -s 42 3 2 10 10
```

### Disable priority aging
```bash
./model -a 0 5 3 10 30
```
Items are dequeued strictly by their original priority (no aging boost).

### Faster aging (250ms interval)
```bash
./model -a 250 5 3 10 30
```
Low-priority items get boosted twice as fast as the default (500ms).

### Show help
```bash
./model --help
```

### Graceful shutdown via signal
Start any run, then press Ctrl+C. The program will:
1. Stop all threads cleanly
2. Print the full summary report
3. Export the CSV trace
4. Release all resources

### Stress test (maximum threads)
```bash
./model 10 3 20 30
```

## Make Targets

| Target | What it does |
|---|---|
| `make` | Build the executable |
| `make rebuild` | Clean and rebuild from scratch |
| `make clean` | Remove all build artifacts and CSV files |
| `make deps` | Install required system packages (Ubuntu/Debian) |
| `make test` | Quick test run (5P, 3C, Q10, 30s) |
| `make visual` | Quick test in TUI mode (5P, 3C, Q20, 60s) |
| `make bench` | Run the full 72-test suite |
| `make valgrind` | Run valgrind memory leak check |
| `make sanitize` | Build and run with AddressSanitizer (catches buffer overflows) |

## File Structure

```
concurrent_queue_simulator/
├── main.c / main            Entry point. Orchestrates: init -> spawn -> run -> shutdown -> report
├── queue.c / queue.h        Thread-safe bounded queue (mutex + semaphores, priority dequeue + aging)
├── producer.c / producer.h  Producer thread logic (generate data -> enqueue -> sleep)
├── consumer.c / consumer.h  Consumer thread logic (dequeue highest priority -> log -> sleep)
├── analytics.c / analytics.h Background sampling thread, metrics aggregation, CSV export
├── tui.c / tui.h            ncurses live dashboard (queue visualization, throughput bars, sparkline)
├── cli.c / cli.h            CLI argument parsing, validation, report formatting
├── utils.c / utils.h        Timing, RNG, system info, debug macro (DBG)
├── config.h                 All compile-time constants (limits, timing, debug levels)
├── makefile                 Build automation with deps/test/bench targets
├── test_bench.sh            72 automated tests (CLI, boundaries, signals, priority, stress)
└── .github/workflows/test.yml  GitHub Actions CI pipeline
```

## How It Works

### Architecture

```
  Producer 1 ──┐                          ┌── Consumer 1
  Producer 2 ──┤   ┌──────────────────┐   ├── Consumer 2
  Producer 3 ──┼──>│  Bounded Queue   │──>┼── Consumer 3
  ...         ─┤   │ (mutex + sems)   │   │
  Producer N ──┘   │ priority dequeue │   │
                   └──────────────────┘   │
                          │               │
                   Analytics Sampler      │
                   (background thread)    │
```

1. **Producers** generate random data with a random priority (0-9) and enqueue it.
   If the queue is full, the producer blocks until a slot opens.

2. **Consumers** dequeue the item with the highest effective priority.
   If the queue is empty, the consumer blocks until an item arrives.
   Priority aging prevents low-priority items from waiting forever.

3. **Synchronization**: A mutex protects the buffer. Two counting semaphores
   (`slots_available` and `items_available`) handle blocking without busy-waiting.

4. **Shutdown**: When the timeout expires or Ctrl+C is pressed, the `running` flag
   is set to 0 and all semaphores are posted to wake blocked threads. Every thread
   exits its loop, gets joined, and all resources are destroyed.

### Priority Aging

Items gain +1 effective priority for every aging interval they wait in the queue,
capped at priority 9. This prevents starvation of low-priority items.
The default interval is 500ms (set in `config.h` via `AGING_INTERVAL_MS`).
Override at runtime with `-a <ms>`, or disable entirely with `-a 0`.

### Debug Levels

| Level | Name | What it logs |
|---|---|---|
| 0 | OFF | Nothing (default) |
| 1 | ERROR | Errors only |
| 2 | INFO | Thread lifecycle (start/stop), shutdown events |
| 3 | TRACE | Per-operation detail: enqueue/dequeue internals, aging calculations, sleep durations |

Set `DEBUG_MAX_LEVEL` to 0 in `config.h` and rebuild for zero-overhead production builds.

## Test Suite

The test bench (`test_bench.sh`) covers 72 tests across 16 categories:

| Category | Tests | What it verifies |
|---|---|---|
| CLI Validation | 7 | Bad args, missing args, unknown flags rejected |
| Parameter Validation | 7 | Every boundary violation detected |
| Boundary Parameters | 5 | Min (1,1,1,1) and max (10,3,20) values work |
| Normal Operation | 8 | Balance check, thread counts, lifecycle, CSV, cleanup |
| Debug Levels | 8 | Each level produces correct output, no leaks between levels |
| Signal Handling | 5 | SIGINT clean exit, balance PASS, resources released |
| Priority Ordering | 3 | Dequeue events traced, aging fires |
| Blocking Behaviour | 3 | Producers block on full, consumers block on empty |
| Reproducibility | 1 | Same seed produces identical results |
| Combined Flags | 2 | Flag order independence |
| Timeout | 1 | Wall-clock time matches requested duration |
| Stress Test | 4 | 10P+3C, all threads start, balance PASS, cleanup |
| Queue Size 1 | 3 | Extreme boundary, blocking triggered, balance PASS |
| Help Flag | 3 | `-h` and `--help` exit with success, display usage |
| Input Validation | 4 | Non-numeric arguments rejected (`abc`, `3x`, bad `-d`, bad `-s`) |
| Aging Interval | 8 | `-a 0` disables aging, `-a 250` custom interval, bad values rejected |

## Notes for Reviewers

- Every system call return value is checked with a meaningful error message.
- The signal handler only uses async-signal-safe functions (`write`, `sem_post`, flag writes).
- `volatile sig_atomic_t` is used for flags shared with the signal handler (POSIX-correct).
- Lock ordering is consistent (semaphore first, then mutex) to prevent deadlocks.
- The balance check (`produced == consumed + remaining`) verifies no data is lost or duplicated.
- All CLI input is validated with `strtol` (not `atoi`) to reject non-numeric arguments.
- Thread sleep uses 200ms `nanosleep` chunks for responsive shutdown (exits within 200ms of signal).
- Valgrind memory leak check runs in CI to verify zero leaks on every push.
