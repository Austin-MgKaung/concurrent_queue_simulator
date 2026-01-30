/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * analytics.h: Metrics Collection & Analysis
 * * Defines structures for tracking queue occupancy, throughput, and blocking.
 * * Includes interface for CSV export and optimization recommendations.
 */

#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <pthread.h>
#include "config.h"
#include "queue.h"

/* --- Constants --- */

// Buffer size for time-series data (1 sample/sec)
// Enough for 10 minutes of runtime.
#define MAX_QUEUE_SAMPLES       600
#define SAMPLE_INTERVAL_SEC     1

/* --- Data Structures --- */

/*
 * A snapshot of the queue state at a specific timestamp.
 * Used to generate the "Queue Occupancy vs Time" graph.
 */
typedef struct {
    double timestamp;           // Time since start (seconds)
    int occupancy;              // Number of items in queue
    int capacity;               // Max capacity at that moment
} QueueSample;

/*
 * Central storage for all performance metrics.
 * Thread-safe: Protected by its own mutex.
 */
typedef struct {
    /* Time-Series Data */
    QueueSample queue_samples[MAX_QUEUE_SAMPLES];
    int num_samples;
    
    /* Aggregated Queue Stats */
    int queue_capacity;
    int queue_max_occupancy;    // Highest peak observed
    int queue_min_occupancy;    // Lowest trough observed
    long long queue_occupancy_sum; // For calculating average
    int queue_full_count;       // Samples where Queue == Capacity
    int queue_empty_count;      // Samples where Queue == 0
    
    /* Throughput Stats */
    int total_produced;
    int total_consumed;
    
    /* Bottleneck Stats */
    int total_producer_blocks;
    int total_consumer_blocks;

    /* Message Latency Stats (time spent in queue) */
    long long total_latency_ms;     // Sum of all message latencies
    long max_latency_ms;            // Worst-case latency
    long min_latency_ms;            // Best-case latency
    int latency_count;              // Number of latency samples
    
    /* Timing Context */
    double start_time;
    double end_time;
    double total_runtime;
    
    /* System Config (for report context) */
    int num_producers;
    int num_consumers;
    
    /* Synchronization */
    pthread_mutex_t mutex;      // Protects updates to this struct
    
    /* Sampling Agent */
    volatile int sampling_active;
    pthread_t sampling_thread;
    Queue *queue_ptr;           // Target queue to monitor
    
} Analytics;

/* --- Lifecycle Functions --- */

/*
 * Initialises the analytics subsystem.
 * Must be called before starting worker threads.
 * Returns: 0 on success.
 */
int analytics_init(Analytics *analytics, Queue *queue, 
                   int num_producers, int num_consumers);

/*
 * Cleans up resources (mutex, threads).
 * Returns: 0 on success.
 */
int analytics_destroy(Analytics *analytics);

/* --- Background Sampling --- */

/*
 * Spawns a dedicated thread that wakes up every SAMPLE_INTERVAL_SEC
 * to record the current queue depth.
 */
int analytics_start_sampling(Analytics *analytics);

/*
 * Stops the sampling thread gracefully.
 */
void analytics_stop_sampling(Analytics *analytics);

/* --- Event Recording (Thread-Safe) --- */

// Called by Producer threads
void analytics_record_produce(Analytics *analytics);
void analytics_record_producer_block(Analytics *analytics);

// Called by Consumer threads
void analytics_record_consume(Analytics *analytics);
void analytics_record_consumer_block(Analytics *analytics);
void analytics_record_latency(Analytics *analytics, long latency_ms);

/* --- Reporting & Export --- */

/*
 * Freezes metrics and calculates averages/rates.
 * Call this after the simulation stops but before printing.
 */
void analytics_finalise(Analytics *analytics);

/*
 * Prints a formatted performance report to stdout.
 * Includes throughput rates and bottleneck identification.
 */
void analytics_print_summary(const Analytics *analytics);

/*
 * Analyzes data to suggest optimal Queue Size or Thread Counts.
 */
void analytics_print_recommendations(const Analytics *analytics);

/*
 * Writes time-series data to a CSV file (e.g., "trace.csv").
 * This file can be opened in Excel/Python for graphing.
 */
int analytics_export_csv(const Analytics *analytics, const char *filename);

#endif /* ANALYTICS_H */