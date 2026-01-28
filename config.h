/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * config.h: Global configuration and constants.
 */

#ifndef CONFIG_H
#define CONFIG_H

/* --- System Limits --- */
#define MAX_PRODUCERS           10  // Max threads supported by the model
#define MAX_CONSUMERS           5   // Max threads supported by the model
#define MAX_QUEUE_SIZE          20  // Fixed buffer size

/* --- Timing (Seconds) --- */
#define MAX_PRODUCER_WAIT       2   // Max sleep between writes (per spec)
#define MAX_CONSUMER_WAIT       4   // Max sleep between reads (per spec)

/* --- Data Generation --- */
#define DATA_RANGE_MIN          0   
#define DATA_RANGE_MAX          9   
#define PRIORITY_MIN            0   
#define PRIORITY_MAX            9   // 9 is highest priority

/* --- Input Validation --- */
// Used to validate command line arguments.
#define MIN_PRODUCERS           1   
#define MIN_CONSUMERS           1   
#define MAX_RUNTIME_CONSUMERS   3   // Spec restricts runtime consumers to 3 (even if we support 5)
#define MIN_QUEUE_SIZE          1   
#define MIN_TIMEOUT             1   // Simulation minimum duration

/* --- Debugging --- */
#define DEBUG_MODE              0   // 0 = Clean output (for report), 1 = Verbose trace

#endif /* CONFIG_H */