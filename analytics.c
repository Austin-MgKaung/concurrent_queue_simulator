/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * analytics.c: Metrics Collection & Analysis Implementation
 * * Implements the background sampling thread and statistical aggregation.
 * * Generates performance reports and sizing recommendations.
 *
 * ERROR HANDLING STRATEGY:
 * -----------------------
 * This file protects against:
 *   1. NULL pointer arguments         — all public functions check inputs
 *   2. Mutex init/destroy failures    — logged, returns error code
 *   3. Sampling thread creation fail  — logged, returns error (non-fatal)
 *   4. CSV file I/O errors            — fopen checked, fprintf errors tracked
 *   5. Division by zero               — guarded in all rate calculations
 *   6. Sample buffer overflow         — bounded by MAX_QUEUE_SAMPLES check
 *   7. Double stop_sampling           — guarded by sampling_active flag
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "analytics.h"
#include "utils.h"

/* --- Internal Helpers --- */

/*
 * Background Sampling Thread.
 * Periodically wakes up to record queue depth.
 * Runs independently of Producer/Consumer threads.
 *
 * Error handling:
 *   - NULL arg check on entry (defensive)
 *   - Mutex lock/unlock failures logged but non-fatal
 *     (sampling data may be slightly inconsistent but won't crash)
 *   - Sample buffer overflow prevented by MAX_QUEUE_SAMPLES bound
 */
static void *sampling_thread_func(void *arg)
{
    Analytics *analytics = (Analytics *)arg;
    QueueSample sample;
    int occupancy;

    if (analytics == NULL) {
        fprintf(stderr, "[ERROR] sampling_thread: NULL argument\n");
        return NULL;
    }

    DBG(DBG_INFO, "%s", "Analytics sampler started");

    while (analytics->sampling_active) {
        /* 1. Snapshot Queue State
         * queue_get_count reads without mutex — acceptable for sampling.
         * The value may be slightly stale but this is a monitoring thread,
         * not a decision-making thread. */
        occupancy = queue_get_count(analytics->queue_ptr);

        /* 2. Lock analytics mutex to safely update shared data */
        if (pthread_mutex_lock(&analytics->mutex) != 0) {
            /* Error handling: Mutex lock failed — skip this sample.
             * Missing one sample is better than corrupting the data. */
            fprintf(stderr, "[WARN] sampling_thread: mutex lock failed, "
                    "skipping sample\n");
            sleep(SAMPLE_INTERVAL_SEC);
            continue;
        }

        /* 3. Record Time-Series Data (bounded by MAX_QUEUE_SAMPLES) */
        if (analytics->num_samples < MAX_QUEUE_SAMPLES) {
            sample.timestamp = time_elapsed();
            sample.occupancy = occupancy;
            sample.capacity = analytics->queue_capacity;

            analytics->queue_samples[analytics->num_samples] = sample;
            analytics->num_samples++;
        }
        /* else: buffer full — silently stop recording new samples.
         * Existing samples are preserved for the report. */

        /* 4. Update Aggregates */
        analytics->queue_occupancy_sum += occupancy;

        if (occupancy > analytics->queue_max_occupancy) {
            analytics->queue_max_occupancy = occupancy;
        }

        if (occupancy < analytics->queue_min_occupancy) {
            analytics->queue_min_occupancy = occupancy;
        }

        if (occupancy >= analytics->queue_capacity) {
            analytics->queue_full_count++;
        }

        if (occupancy == 0) {
            analytics->queue_empty_count++;
        }

        if (pthread_mutex_unlock(&analytics->mutex) != 0) {
            /* Error handling: Mutex unlock failed — other threads may
             * deadlock on the analytics mutex. Log the error. */
            fprintf(stderr, "[ERROR] sampling_thread: mutex unlock failed\n");
        }

        DBG(DBG_TRACE, "Analytics sample: occupancy=%d/%d (%d samples)",
            occupancy, analytics->queue_capacity, analytics->num_samples);

        /* 5. Wait for next interval */
        sleep(SAMPLE_INTERVAL_SEC);
    }

    DBG(DBG_INFO, "%s", "Analytics sampler stopped");

    return NULL;
}

/* --- Public API: Lifecycle --- */

/*
 * Initialises the analytics subsystem.
 *
 * Error handling:
 *   - NULL pointer check on both arguments
 *   - Mutex init failure logged and returns error
 *   - memset ensures all counters start at known state (zero)
 *   - queue_min_occupancy set to capacity so first sample always
 *     becomes the new minimum (avoids false 0 minimum)
 */
int analytics_init(Analytics *analytics, Queue *queue,
                   int num_producers, int num_consumers)
{
    if (analytics == NULL || queue == NULL) {
        fprintf(stderr, "[ERROR] analytics_init: NULL argument "
                "(analytics=%p, queue=%p)\n", (void *)analytics, (void *)queue);
        return -1;
    }

    /* Clear all memory — ensures no stale data from previous runs */
    memset(analytics, 0, sizeof(Analytics));

    analytics->queue_ptr = queue;
    analytics->queue_capacity = queue_get_capacity(queue);
    analytics->num_producers = num_producers;
    analytics->num_consumers = num_consumers;

    /* Start min_occupancy at capacity so any real value becomes the new min */
    analytics->queue_min_occupancy = analytics->queue_capacity;
    analytics->start_time = time_elapsed();

    if (pthread_mutex_init(&analytics->mutex, NULL) != 0) {
        fprintf(stderr, "[ERROR] analytics_init: mutex init failed\n");
        return -1;
    }

    return 0;
}

/*
 * Cleans up analytics resources.
 *
 * Error handling:
 *   - Stops sampling thread if still active (prevents dangling thread)
 *   - Mutex destroy failure logged (may indicate mutex still locked)
 */
int analytics_destroy(Analytics *analytics)
{
    if (analytics == NULL) return -1;

    /* Safety: stop sampling if caller forgot to */
    if (analytics->sampling_active) {
        analytics_stop_sampling(analytics);
    }

    if (pthread_mutex_destroy(&analytics->mutex) != 0) {
        fprintf(stderr, "[ERROR] analytics_destroy: mutex destroy failed "
                "(may still be locked)\n");
        return -1;
    }
    return 0;
}

/* --- Public API: Sampling Control --- */

/*
 * Starts the background sampling thread.
 *
 * Error handling:
 *   - Double-start prevented by sampling_active check
 *   - pthread_create failure resets flag and returns error
 *   - Caller should treat failure as non-fatal (simulation works
 *     without sampling, just no occupancy history)
 */
int analytics_start_sampling(Analytics *analytics)
{
    if (analytics == NULL) return -1;

    /* Guard against double-start */
    if (analytics->sampling_active) return 0;

    analytics->sampling_active = 1;

    if (pthread_create(&analytics->sampling_thread, NULL,
                       sampling_thread_func, analytics) != 0) {
        fprintf(stderr, "[ERROR] analytics_start_sampling: pthread_create failed\n");
        analytics->sampling_active = 0;
        return -1;
    }

    return 0;
}

/*
 * Stops the background sampling thread.
 *
 * Error handling:
 *   - Double-stop prevented by sampling_active check
 *   - pthread_join failure logged (thread may have already exited)
 *   - Flag cleared before join so thread sees it and exits its loop
 */
void analytics_stop_sampling(Analytics *analytics)
{
    int result;

    if (analytics == NULL) return;

    /* Guard against double-stop */
    if (!analytics->sampling_active) return;

    /* Clear flag first — the sampling thread checks this in its loop */
    analytics->sampling_active = 0;

    result = pthread_join(analytics->sampling_thread, NULL);
    if (result != 0) {
        fprintf(stderr, "[WARN] analytics_stop_sampling: pthread_join failed "
                "(error=%d)\n", result);
    }
}

/* --- Public API: Event Recording --- */

/*
 * Thread-safe event counters.
 * Called from producer/consumer threads concurrently.
 *
 * Error handling:
 *   - NULL check on analytics pointer (may not be set)
 *   - Mutex lock/unlock failures logged
 *   - On lock failure, the count is silently skipped (non-fatal —
 *     slightly inaccurate count is better than a crash or deadlock)
 */

void analytics_record_produce(Analytics *analytics) {
    if (!analytics) return;
    if (pthread_mutex_lock(&analytics->mutex) != 0) {
        fprintf(stderr, "[WARN] analytics_record_produce: mutex lock failed\n");
        return;
    }
    analytics->total_produced++;
    if (pthread_mutex_unlock(&analytics->mutex) != 0) {
        fprintf(stderr, "[ERROR] analytics_record_produce: mutex unlock failed\n");
    }
}

void analytics_record_consume(Analytics *analytics) {
    if (!analytics) return;
    if (pthread_mutex_lock(&analytics->mutex) != 0) {
        fprintf(stderr, "[WARN] analytics_record_consume: mutex lock failed\n");
        return;
    }
    analytics->total_consumed++;
    if (pthread_mutex_unlock(&analytics->mutex) != 0) {
        fprintf(stderr, "[ERROR] analytics_record_consume: mutex unlock failed\n");
    }
}

void analytics_record_producer_block(Analytics *analytics) {
    if (!analytics) return;
    if (pthread_mutex_lock(&analytics->mutex) != 0) {
        fprintf(stderr, "[WARN] analytics_record_producer_block: mutex lock failed\n");
        return;
    }
    analytics->total_producer_blocks++;
    if (pthread_mutex_unlock(&analytics->mutex) != 0) {
        fprintf(stderr, "[ERROR] analytics_record_producer_block: mutex unlock failed\n");
    }
}

void analytics_record_consumer_block(Analytics *analytics) {
    if (!analytics) return;
    if (pthread_mutex_lock(&analytics->mutex) != 0) {
        fprintf(stderr, "[WARN] analytics_record_consumer_block: mutex lock failed\n");
        return;
    }
    analytics->total_consumer_blocks++;
    if (pthread_mutex_unlock(&analytics->mutex) != 0) {
        fprintf(stderr, "[ERROR] analytics_record_consumer_block: mutex unlock failed\n");
    }
}

/* --- Public API: Reporting --- */

/*
 * Freezes metrics and calculates final timing.
 *
 * Error handling:
 *   - Stops sampling thread if still active (prevents data changes
 *     while we read the final values)
 *   - NULL check on input
 */
void analytics_finalise(Analytics *analytics)
{
    if (!analytics) return;

    if (analytics->sampling_active) {
        analytics_stop_sampling(analytics);
    }

    analytics->end_time = time_elapsed();
    analytics->total_runtime = analytics->end_time - analytics->start_time;
}

/*
 * Prints a formatted performance report.
 *
 * Error handling:
 *   - Division by zero guarded for all rate calculations
 *     (num_samples == 0, total_runtime == 0, queue_capacity == 0)
 *   - NULL check on input
 */
void analytics_print_summary(const Analytics *analytics)
{
    double avg_occupancy, percent_full, percent_empty, utilisation;
    double p_rate, c_rate;

    if (!analytics) return;

    /* Calculate derived metrics with division-by-zero guards */
    if (analytics->num_samples > 0) {
        avg_occupancy = (double)analytics->queue_occupancy_sum / analytics->num_samples;
        percent_full = (double)analytics->queue_full_count / analytics->num_samples * 100.0;
        percent_empty = (double)analytics->queue_empty_count / analytics->num_samples * 100.0;

        /* Guard against queue_capacity == 0 (should never happen but defensive) */
        if (analytics->queue_capacity > 0) {
            utilisation = avg_occupancy / analytics->queue_capacity * 100.0;
        } else {
            utilisation = 0.0;
        }
    } else {
        avg_occupancy = 0.0; percent_full = 0.0;
        percent_empty = 0.0; utilisation = 0.0;
    }

    /* Guard against zero runtime for rate calculations */
    if (analytics->total_runtime > 0) {
        p_rate = analytics->total_produced / analytics->total_runtime;
        c_rate = analytics->total_consumed / analytics->total_runtime;
    } else {
        p_rate = 0.0; c_rate = 0.0;
    }

    printf("\nANALYTICS SUMMARY\n");
    printf("------------------------------------------------------------\n");

    printf("CONFIGURATION\n");
    printf("  Producers:        %-5d Consumers:        %-5d\n",
           analytics->num_producers, analytics->num_consumers);
    printf("  Queue Capacity:   %-5d Runtime:          %.2f sec\n",
           analytics->queue_capacity, analytics->total_runtime);

    printf("\nQUEUE METRICS\n");
    printf("  Avg Occupancy:    %.2f items (%.1f%% Utilisation)\n", avg_occupancy, utilisation);
    printf("  Peak Occupancy:   %d items\n", analytics->queue_max_occupancy);
    printf("  Time Full:        %.1f%%\n", percent_full);
    printf("  Time Empty:       %.1f%%\n", percent_empty);

    printf("\nTHROUGHPUT\n");
    printf("  Produced:         %d (%.2f msg/sec)\n", analytics->total_produced, p_rate);
    printf("  Consumed:         %d (%.2f msg/sec)\n", analytics->total_consumed, c_rate);

    printf("\nBLOCKING EVENTS\n");
    printf("  Producer Blocks:  %d (Queue Full)\n", analytics->total_producer_blocks);
    printf("  Consumer Blocks:  %d (Queue Empty)\n", analytics->total_consumer_blocks);
    printf("------------------------------------------------------------\n");
}

/*
 * Analyses data to suggest optimal queue size or thread counts.
 *
 * Error handling:
 *   - Division by zero guarded for utilisation calculation
 *   - num_samples == 0 guard prevents division in threshold checks
 */
void analytics_print_recommendations(const Analytics *analytics)
{
    double avg_occupancy, utilisation;
    const char *action;
    const char *reason;
    int recommended_size;

    if (!analytics) return;

    if (analytics->num_samples > 0 && analytics->queue_capacity > 0) {
        avg_occupancy = (double)analytics->queue_occupancy_sum / analytics->num_samples;
        utilisation = avg_occupancy / analytics->queue_capacity * 100.0;
    } else {
        utilisation = 0.0;
    }

    /* Recommendation Logic — driven by blocking frequency and utilisation */

    if (analytics->num_samples > 0 &&
        analytics->total_producer_blocks > 0 &&
        (double)analytics->queue_full_count / analytics->num_samples > 0.1) {

        /* Scenario: Bottleneck at Queue (Too Small) */
        recommended_size = analytics->queue_capacity * 2;
        if (recommended_size > MAX_QUEUE_SIZE) recommended_size = MAX_QUEUE_SIZE;

        action = "INCREASE Queue Size";
        reason = "High producer blocking frequency (Queue Full)";

    } else if (analytics->num_samples > 0 &&
               analytics->total_consumer_blocks > 0 &&
               (double)analytics->queue_empty_count / analytics->num_samples > 0.3) {

        /* Scenario: Bottleneck at Production (Queue Empty) */
        recommended_size = analytics->queue_capacity;
        action = "ADD Producers (or Maintain Size)";
        reason = "High consumer starvation (Queue Empty)";

    } else if (utilisation < 30.0) {

        /* Scenario: Oversized Queue */
        recommended_size = (int)(analytics->queue_capacity * 0.7);
        if (recommended_size < MIN_QUEUE_SIZE) recommended_size = MIN_QUEUE_SIZE;

        action = "DECREASE Queue Size";
        reason = "Low utilisation (<30%)";

    } else {
        /* Scenario: Optimal */
        recommended_size = analytics->queue_capacity;
        action = "MAINTAIN Current Size";
        reason = "Balanced utilisation (30-70%)";
    }

    printf("\nOPTIMIZATION RECOMMENDATION\n");
    printf("------------------------------------------------------------\n");
    printf("  Current Size:     %d\n", analytics->queue_capacity);
    printf("  Suggested Size:   %d\n", recommended_size);
    printf("  Action:           %s\n", action);
    printf("  Rationale:        %s\n", reason);
    printf("------------------------------------------------------------\n\n");
}

/*
 * Exports time-series data to a CSV file.
 *
 * Error handling:
 *   - NULL pointer checks on both arguments
 *   - fopen failure reported via perror (includes OS error message)
 *   - fprintf failures tracked — if any write fails, we report the
 *     number of failed writes so the user knows the file is incomplete
 *   - File is always closed even if writes fail (prevents fd leak)
 */
int analytics_export_csv(const Analytics *analytics, const char *filename)
{
    FILE *fp;
    int i;
    double util;
    int write_errors = 0;

    if (!analytics || !filename) {
        fprintf(stderr, "[ERROR] analytics_export_csv: NULL argument\n");
        return -1;
    }

    fp = fopen(filename, "w");
    if (!fp) {
        /* Error handling: fopen failed — could be permissions, disk full,
         * invalid path. perror prints the OS-specific reason. */
        perror("[ERROR] analytics_export_csv: fopen failed");
        return -1;
    }

    /* Write header */
    if (fprintf(fp, "Time,Occupancy,Capacity,Utilisation\n") < 0) {
        fprintf(stderr, "[ERROR] analytics_export_csv: failed writing header\n");
        fclose(fp);
        return -1;
    }

    /* Write data rows */
    for (i = 0; i < analytics->num_samples; i++) {
        /* Guard against capacity == 0 in utilisation calculation */
        if (analytics->queue_samples[i].capacity > 0) {
            util = (double)analytics->queue_samples[i].occupancy /
                   analytics->queue_samples[i].capacity * 100.0;
        } else {
            util = 0.0;
        }

        if (fprintf(fp, "%.2f,%d,%d,%.1f\n",
                    analytics->queue_samples[i].timestamp,
                    analytics->queue_samples[i].occupancy,
                    analytics->queue_samples[i].capacity,
                    util) < 0) {
            /* Error handling: fprintf failed — likely disk full.
             * Count the error but continue trying remaining rows. */
            write_errors++;
        }
    }

    /* Error handling: Always close the file to prevent fd leak */
    if (fclose(fp) != 0) {
        perror("[ERROR] analytics_export_csv: fclose failed");
        return -1;
    }

    if (write_errors > 0) {
        fprintf(stderr, "[WARN] analytics_export_csv: %d write errors occurred "
                "(file may be incomplete)\n", write_errors);
        return -1;
    }

    printf("  Trace exported to: %s (%d samples)\n", filename, analytics->num_samples);
    return 0;
}
