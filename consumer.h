/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * consumer.h: Consumer Thread Declarations
 * * Defines the workload logic for Consumer threads.
 * * Consumers retrieve high-priority data from the queue and simulate processing time.
 */

#ifndef CONSUMER_H
#define CONSUMER_H

#include <pthread.h>
#include <signal.h>
#include "queue.h"
#include "analytics.h"

/* --- Data Structures --- */

/*
 * Metrics tracked per consumer.
 * Required for the final report to demonstrate throughput and load balancing.
 */
typedef struct {
    int messages_consumed;      // Total items successfully processed
    int times_blocked;          // Count of times the thread waited for data
} ConsumerStats;

/*
 * Thread Arguments Container.
 * Passed via pthread_create to give the thread its context.
 */
typedef struct {
    int id;                     // Identification (1..N)
    Queue *queue;               // Reference to the shared buffer
    volatile sig_atomic_t *running; // Pointer to the global stop flag
    ConsumerStats stats;        // Local performance counters
    int quiet_mode;             // Flag for quiet mode (TUI integration)
    int max_wait;               // Max sleep between reads (seconds)
    Analytics *analytics;       // Pointer to shared analytics (may be NULL)
} ConsumerArgs;

/* --- Function Prototypes --- */

/*
 * The Main Consumer Loop.
 * Logic:
 * 1. Dequeue highest priority item (Blocks if empty).
 * 2. Log retrieval details (Consumer ID, Producer ID, Data, Priority).
 * 3. Sleep random interval (0..MAX_CONSUMER_WAIT).
 * Returns: NULL on exit.
 */
void *consumer_thread(void *arg);

/*
 * Helper to populate the argument struct before spawning threads.
 * Returns: 0 on success.
 */
int consumer_init_args(ConsumerArgs *args, int id, Queue *queue, volatile sig_atomic_t *running);

/*
 * Prints the final usage statistics for this thread.
 * Called during system shutdown.
 */
void consumer_print_stats(const ConsumerArgs *args);

#endif /* CONSUMER_H */