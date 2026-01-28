/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * consumer.c: Consumer Thread Implementation
 * * Implements the workload lifecycle: Dequeue (Priority) -> Log -> Sleep.
 * * Tracks consumption statistics and responsiveness to shutdown signals.
 */

#define _POSIX_C_SOURCE 200809L // For time handling

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "consumer.h"
#include "config.h"
#include "utils.h"

/* --- Internal Helpers --- */

/*
 * Static start time reference.
 * Consistent with producer.c to ensure log timestamps are synchronized.
 */
static time_t start_time = 0;

static double get_elapsed_seconds(void)
{
    if (start_time == 0) {
        start_time = time(NULL);
    }
    return difftime(time(NULL), start_time);
}

/* --- Public API --- */

int consumer_init_args(ConsumerArgs *args, int id, Queue *queue, volatile int *running)
{
    // Validate pointers
    if (args == NULL) {
        fprintf(stderr, "Error: consumer_init_args - NULL args\n");
        return -1;
    }
    
    if (queue == NULL || running == NULL) {
        fprintf(stderr, "Error: consumer_init_args - Missing dependencies\n");
        return -1;
    }
    
    if (id < 1 || id > MAX_CONSUMERS) {
        fprintf(stderr, "Error: consumer_init_args - ID %d out of range\n", id);
        return -1;
    }
    
    // Initialize context
    args->id = id;
    args->queue = queue;
    args->running = running;
    
    // Reset counters
    args->stats.messages_consumed = 0;
    args->stats.times_blocked = 0;
    
    return 0;
}

/*
 * The Main Consumer Thread Logic.
 * Executed concurrently by pthread_create.
 */
void *consumer_thread(void *arg)
{
    ConsumerArgs *args;
    Message msg;
    int result;
    int sleep_time;
    
    args = (ConsumerArgs *)arg;
    
    if (args == NULL) return NULL;
    
    printf("[%06.2f] Consumer %d: Started\n", get_elapsed_seconds(), args->id);
    
    if (DEBUG_MODE) {
        printf("[DEBUG] Consumer %d: Context Loaded\n", args->id);
    }
    
    // Main Lifecycle Loop
    // Continues until the main thread sets the global 'running' flag to 0.
    while (*(args->running)) {
        
        // Step 1: Pre-Dequeue Check (Statistics)
        // We peek at the queue state to detect if we *would* block.
        // This provides the "Times Blocked" metric for the report.
        if (queue_is_empty(args->queue)) {
            args->stats.times_blocked++;
            printf("[%06.2f] Consumer %d: BLOCKED (queue empty: %d/%d)\n",
                   get_elapsed_seconds(), args->id,
                   queue_get_count(args->queue), queue_get_capacity(args->queue));
        }
        
        // Step 2: Dequeue (Blocking Operation)
        // This call blocks if the queue is empty.
        // It guarantees retrieving the HIGHEST PRIORITY item available.
        result = queue_dequeue_safe(args->queue, &msg);
        
        // Handle shutdown signal
        if (result != 0) {
            if (*(args->running)) {
                fprintf(stderr, "[%06.2f] Consumer %d: Dequeue failed\n", 
                        get_elapsed_seconds(), args->id);
            }
            break; // Exit loop
        }
        
        // Step 3: Success Logging
        args->stats.messages_consumed++;
        
        printf("[%06.2f] Consumer %d: Read (pri=%d, data=%d) from P%d | Queue: %d/%d\n",
               get_elapsed_seconds(), args->id,
               msg.priority, msg.data, msg.producer_id,
               queue_get_count(args->queue), queue_get_capacity(args->queue));
        
        // Step 4: Simulated Processing Time
        // Random sleep [0..MAX_CONSUMER_WAIT].
        if (*(args->running)) {
            sleep_time = random_range(0, MAX_CONSUMER_WAIT);
            
            // Responsive Sleep Loop:
            // Checks for shutdown signal every second to avoid hanging
            // during long sleep intervals.
            while (sleep_time > 0 && *(args->running)) {
                sleep(1);
                sleep_time--;
            }
        }
    }
    
    // Cleanup & Exit
    printf("[%06.2f] Consumer %d: Stopped (Total: %d, Blocked: %d)\n",
           get_elapsed_seconds(), args->id,
           args->stats.messages_consumed, args->stats.times_blocked);
    
    return NULL;
}

void consumer_print_stats(const ConsumerArgs *args)
{
    if (args == NULL) return;
    
    printf("  Consumer %d: %d messages consumed, %d times blocked\n",
           args->id, args->stats.messages_consumed, args->stats.times_blocked);
}