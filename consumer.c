/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * consumer.c: Consumer Thread Implementation
 * * Implements the workload lifecycle: Dequeue (Priority) -> Log -> Sleep.
 * * Uses global time_elapsed() for synchronized logging.
 * * Supports 'quiet_mode' for TUI integration.
 *
 * ERROR HANDLING STRATEGY:
 * -----------------------
 * This file protects against:
 *   1. NULL argument to thread        — checked on entry, exits immediately
 *   2. Dequeue failure (shutdown)     — normal exit path, not an error
 *   3. Dequeue failure (other)        — logged, thread exits cleanly
 *   4. Blocking detection             — reported via was_blocked from queue
 *   5. Analytics NULL pointer         — checked before every analytics call
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "consumer.h"
#include "config.h"
#include "utils.h"

/* --- Public API --- */

/*
 * Populates a ConsumerArgs struct before thread creation.
 *
 * Error handling: Validates all pointers and ID range.
 * Returns -1 if any validation fails, preventing the caller
 * from spawning a thread with invalid context.
 */
int consumer_init_args(ConsumerArgs *args, int id, Queue *queue, volatile sig_atomic_t *running)
{
    if (args == NULL) {
        fprintf(stderr, "[ERROR] consumer_init_args: NULL args pointer\n");
        return -1;
    }

    if (queue == NULL || running == NULL) {
        fprintf(stderr, "[ERROR] consumer_init_args: NULL dependency "
                "(queue=%p, running=%p)\n", (void *)queue, (void *)running);
        return -1;
    }

    if (id < 1 || id > MAX_CONSUMERS) {
        fprintf(stderr, "[ERROR] consumer_init_args: ID %d out of range [1, %d]\n",
                id, MAX_CONSUMERS);
        return -1;
    }

    args->id = id;
    args->queue = queue;
    args->running = running;
    args->quiet_mode = 0;
    args->max_wait = MAX_CONSUMER_WAIT;
    args->analytics = NULL;

    args->stats.messages_consumed = 0;
    args->stats.times_blocked = 0;

    return 0;
}

/*
 * The Main Consumer Thread Logic.
 * Executed concurrently by pthread_create.
 *
 * Error handling:
 *   - NULL arg check on entry (defensive — should never happen if
 *     consumer_init_args succeeded, but guards against misuse)
 *   - Dequeue return value checked every iteration:
 *     * result != 0 during shutdown is normal (thread exits cleanly)
 *     * result != 0 while running is unexpected (logged as error)
 *   - Blocking detection uses the accurate was_blocked output from
 *     queue_dequeue_safe (no more racy pre-checks)
 */
void *consumer_thread(void *arg)
{
    ConsumerArgs *args;
    Message msg;
    int result;
    int sleep_time;
    int was_blocked;

    args = (ConsumerArgs *)arg;

    /* Error handling: Guard against NULL argument.
     * This would indicate a bug in main.c thread setup. */
    if (args == NULL) {
        fprintf(stderr, "[ERROR] consumer_thread: NULL argument\n");
        return NULL;
    }

    if (!args->quiet_mode) {
        printf("[%06.2f] Consumer %d: Started\n", time_elapsed(), args->id);
    }

    DBG(DBG_INFO, "Consumer %d: Context Loaded", args->id);

    /* Main Lifecycle Loop
     * Continues until the main thread sets the global 'running' flag to 0. */
    while (*(args->running)) {

        /* Step 1: Dequeue (Blocking Operation)
         * was_blocked is set by queue_dequeue_safe using sem_trywait.
         * This gives us accurate block detection without race conditions. */
        was_blocked = 0;
        long wait_time_ms = 0;
        result = queue_dequeue_safe(args->queue, &msg, &was_blocked, &wait_time_ms);

        /* Step 2: Record blocking if it occurred */
        if (was_blocked) {
            args->stats.times_blocked++;
            /* Error handling: analytics pointer may be NULL if analytics
             * was not initialised. Check before calling. */
            if (args->analytics) {
                analytics_record_consumer_block(args->analytics);
                analytics_record_consumer_wait(args->analytics, wait_time_ms);
            }
            if (!args->quiet_mode) {
                printf("[%06.2f] Consumer %d: BLOCKED (queue was empty)\n",
                       time_elapsed(), args->id);
            }
        }

        /* Error handling: Check dequeue result */
        if (result != 0) {
            if (*(args->running)) {
                /* Error handling: Dequeue failed while still running.
                 * This is unexpected — could be a queue error, not shutdown.
                 * Log it so the issue is visible in the output. */
                if (!args->quiet_mode) {
                    fprintf(stderr, "[%06.2f] Consumer %d: Dequeue failed "
                            "(unexpected)\n", time_elapsed(), args->id);
                }
            }
            /* Whether shutdown or error, exit the loop cleanly */
            break;
        }

        /* Step 3: Success Logging */
        args->stats.messages_consumed++;
        if (args->analytics) {
            analytics_record_consume(args->analytics);
            /* Record how long this message waited in the queue */
            long latency = queue_get_time_ms() - msg.timestamp;
            if (latency >= 0)
                analytics_record_latency(args->analytics, latency);
        }

        DBG(DBG_TRACE, "Consumer %d: Read pri=%d, data=%d from P%d, queue=%d/%d",
            args->id, msg.priority, msg.data, msg.producer_id,
            queue_get_count(args->queue), queue_get_capacity(args->queue));

        if (!args->quiet_mode) {
            printf("[%06.2f] Consumer %d: Read (pri=%d, data=%d) from P%d | Queue: %d/%d\n",
                   time_elapsed(), args->id,
                   msg.priority, msg.data, msg.producer_id,
                   queue_get_count(args->queue), queue_get_capacity(args->queue));
        }

        /* Step 4: Simulated Processing Time
         * Responsive sleep: wake every second to check shutdown flag.
         * This ensures threads exit promptly (within 1s) when stopped. */
        if (*(args->running)) {
            sleep_time = random_range(0, args->max_wait);

            DBG(DBG_TRACE, "Consumer %d: Sleeping for %d s", args->id, sleep_time);

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
        printf("[%06.2f] Consumer %d: Stopped (Total: %d, Blocked: %d)\n",
               time_elapsed(), args->id,
               args->stats.messages_consumed, args->stats.times_blocked);
    }
    DBG(DBG_INFO, "Consumer %d: Exiting thread", args->id);

    return NULL;
}

void consumer_print_stats(const ConsumerArgs *args)
{
    if (args == NULL) return;

    printf("    Consumer %d: %d messages consumed, %d times blocked\n",
           args->id, args->stats.messages_consumed, args->stats.times_blocked);
}
