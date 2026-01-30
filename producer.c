/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * producer.c: Producer Thread Implementation
 * * Implements the workload lifecycle: Generate Data -> Enqueue -> Sleep.
 * * Uses global time_elapsed() for synchronized logging.
 * * Supports 'quiet_mode' for TUI integration.
 *
 * ERROR HANDLING STRATEGY:
 * -----------------------
 * This file protects against:
 *   1. NULL argument to thread        — checked on entry, exits immediately
 *   2. Enqueue failure (shutdown)     — normal exit path, not an error
 *   3. Enqueue failure (other)        — logged, thread exits cleanly
 *   4. Blocking detection             — reported via was_blocked from queue
 *   5. Analytics NULL pointer         — checked before every analytics call
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "producer.h"
#include "config.h"
#include "utils.h"

/* --- Public API --- */

/*
 * Populates a ProducerArgs struct before thread creation.
 *
 * Error handling: Validates all pointers and ID range.
 * Returns -1 if any validation fails, preventing the caller
 * from spawning a thread with invalid context.
 */
int producer_init_args(ProducerArgs *args, int id, Queue *queue, volatile sig_atomic_t *running)
{
    if (args == NULL) {
        fprintf(stderr, "[ERROR] producer_init_args: NULL args pointer\n");
        return -1;
    }

    if (queue == NULL || running == NULL) {
        fprintf(stderr, "[ERROR] producer_init_args: NULL dependency "
                "(queue=%p, running=%p)\n", (void *)queue, (void *)running);
        return -1;
    }

    if (id < 1 || id > MAX_PRODUCERS) {
        fprintf(stderr, "[ERROR] producer_init_args: ID %d out of range [1, %d]\n",
                id, MAX_PRODUCERS);
        return -1;
    }

    args->id = id;
    args->queue = queue;
    args->running = running;
    args->quiet_mode = 0;
    args->max_wait = MAX_PRODUCER_WAIT;
    args->analytics = NULL;

    args->stats.messages_produced = 0;
    args->stats.times_blocked = 0;

    return 0;
}

/*
 * The Main Producer Thread Logic.
 * Executed concurrently by pthread_create.
 *
 * Error handling:
 *   - NULL arg check on entry (defensive — should never happen if
 *     producer_init_args succeeded, but guards against misuse)
 *   - Enqueue return value checked every iteration:
 *     * result != 0 during shutdown is normal (thread exits cleanly)
 *     * result != 0 while running is unexpected (logged as error)
 *   - Blocking detection uses the accurate was_blocked output from
 *     queue_enqueue_safe (no more racy pre-checks)
 */
void *producer_thread(void *arg)
{
    ProducerArgs *args;
    Message msg;
    int data;
    int priority;
    int result;
    int sleep_time;
    int was_blocked;

    args = (ProducerArgs *)arg;

    /* Error handling: Guard against NULL argument.
     * This would indicate a bug in main.c thread setup. */
    if (args == NULL) {
        fprintf(stderr, "[ERROR] producer_thread: NULL argument\n");
        return NULL;
    }

    if (!args->quiet_mode) {
        printf("[%06.2f] Producer %d: Started\n", time_elapsed(), args->id);
    }

    DBG(DBG_INFO, "Producer %d: Context Loaded", args->id);

    /* Main Lifecycle Loop
     * Continues until the main thread sets the global 'running' flag to 0. */
    while (*(args->running)) {

        /* Step 1: Data Generation */
        data = random_range(DATA_RANGE_MIN, DATA_RANGE_MAX);
        priority = random_range(PRIORITY_MIN, PRIORITY_MAX);
        msg = message_create(data, priority, args->id);

        DBG(DBG_TRACE, "Producer %d: Generated data=%d, pri=%d", args->id, data, priority);

        /* Step 2: Enqueue (Blocking Operation)
         * was_blocked is set by queue_enqueue_safe using sem_trywait.
         * This gives us accurate block detection without race conditions. */
        was_blocked = 0;
        result = queue_enqueue_safe(args->queue, msg, &was_blocked);

        /* Step 3: Record blocking if it occurred */
        if (was_blocked) {
            args->stats.times_blocked++;
            /* Error handling: analytics pointer may be NULL if analytics
             * was not initialised. Check before calling. */
            if (args->analytics) analytics_record_producer_block(args->analytics);
            if (!args->quiet_mode) {
                printf("[%06.2f] Producer %d: BLOCKED (queue was full)\n",
                       time_elapsed(), args->id);
            }
        }

        /* Error handling: Check enqueue result */
        if (result != 0) {
            if (*(args->running)) {
                /* Error handling: Enqueue failed while still running.
                 * This is unexpected — could be a queue error, not shutdown.
                 * Log it so the issue is visible in the output. */
                if (!args->quiet_mode) {
                    fprintf(stderr, "[%06.2f] Producer %d: Enqueue failed "
                            "(unexpected)\n", time_elapsed(), args->id);
                }
            }
            /* Whether shutdown or error, exit the loop cleanly */
            break;
        }

        /* Step 4: Success Logging */
        args->stats.messages_produced++;
        if (args->analytics) analytics_record_produce(args->analytics);

        if (!args->quiet_mode) {
            printf("[%06.2f] Producer %d: Wrote (pri=%d, data=%d) | Queue: %d/%d\n",
                   time_elapsed(), args->id,
                   priority, data,
                   queue_get_count(args->queue), queue_get_capacity(args->queue));
        }

        /* Step 5: Simulated Processing Time
         * Responsive sleep: wake every second to check shutdown flag.
         * This ensures threads exit promptly (within 1s) when stopped. */
        if (*(args->running)) {
            sleep_time = random_range(0, args->max_wait);

            DBG(DBG_TRACE, "Producer %d: Sleeping for %d s", args->id, sleep_time);

            {
                int remaining_ms = sleep_time * 1000;
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = 200000000L; /* 200ms chunks for responsive shutdown */
                while (remaining_ms > 0 && *(args->running)) {
                    nanosleep(&ts, NULL);
                    remaining_ms -= 200;
                }
            }
        }
    }

    /* Cleanup & Exit */
    if (!args->quiet_mode) {
        printf("[%06.2f] Producer %d: Stopped (produced %d, blocked %d)\n",
               time_elapsed(), args->id,
               args->stats.messages_produced, args->stats.times_blocked);
    }
    DBG(DBG_INFO, "Producer %d: Exiting thread", args->id);

    return NULL;
}

void producer_print_stats(const ProducerArgs *args)
{
    if (args == NULL) return;

    printf("    Producer %d: %d messages produced, %d times blocked\n",
           args->id, args->stats.messages_produced, args->stats.times_blocked);
}
