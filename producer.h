/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * producer.h: Producer Thread Declarations
 * * Defines the workload logic for Producer threads.
 * * Producers generate random data/priorities and push to the shared queue.
 */

#ifndef PRODUCER_H
#define PRODUCER_H

#include <pthread.h>
#include <signal.h>
#include "queue.h"
#include "analytics.h"

/* --- Data Structures --- */

/*
 * Metrics tracked per producer.
 * Required for the final report to demonstrate throughput and blocking behavior.
 */
typedef struct {
    int messages_produced;      // Successful writes to queue
    int times_blocked;          // Count of times the thread had to wait for space
} ProducerStats;

/*
 * Thread Arguments Container.
 * Passed via pthread_create to give the thread its context.
 */
typedef struct {
    int id;                     // Identification (1..N)
    Queue *queue;               // Reference to the shared buffer
    volatile sig_atomic_t *running; // Pointer to the global stop flag
    ProducerStats stats;        // Local performance counters
    int quiet_mode;            // Flag for quiet mode (TUI integration)
    Analytics *analytics;      // Pointer to shared analytics (may be NULL)
} ProducerArgs;

/* --- Function Prototypes --- */

/*
 * The Main Producer Loop.
 * Logic:
 * 1. Generate random data & priority (using config.h ranges).
 * 2. Write to Queue (Blocks if full).
 * 3. Log activity.
 * 4. Sleep random interval (0..MAX_PRODUCER_WAIT).
 * Returns: NULL on exit.
 */
void *producer_thread(void *arg);

/*
 * Helper to populate the argument struct before spawning threads.
 * Returns: 0 on success.
 */
int producer_init_args(ProducerArgs *args, int id, Queue *queue, volatile sig_atomic_t *running);

/*
 * Prints the final usage statistics for this thread.
 * Called during system shutdown.
 */
void producer_print_stats(const ProducerArgs *args);

#endif /* PRODUCER_H */