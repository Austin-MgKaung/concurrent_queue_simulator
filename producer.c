/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * producer.c: Producer Thread Implementation
 * * Implements the workload lifecycle: Generate Data -> Enqueue -> Sleep.
 * * Uses global time_elapsed() for synchronized logging.
 */

#define _POSIX_C_SOURCE 200809L // For time/sleep functions

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "producer.h"
#include "config.h"
#include "utils.h"

/* --- Public API --- */

int producer_init_args(ProducerArgs *args, int id, Queue *queue, volatile int *running)
{
    // Validate pointers
    if (args == NULL) {
        fprintf(stderr, "Error: producer_init_args - NULL args\n");
        return -1;
    }
    
    if (queue == NULL || running == NULL) {
        fprintf(stderr, "Error: producer_init_args - Missing dependencies\n");
        return -1;
    }
    
    if (id < 1 || id > MAX_PRODUCERS) {
        fprintf(stderr, "Error: producer_init_args - ID %d out of range\n", id);
        return -1;
    }
    
    // Initialize context
    args->id = id;
    args->queue = queue;
    args->running = running;
    
    // Reset counters
    args->stats.messages_produced = 0;
    args->stats.times_blocked = 0;
    
    return 0;
}

/*
 * The Main Producer Thread Logic.
 * Executed concurrently by pthread_create.
 */
void *producer_thread(void *arg)
{
    ProducerArgs *args;
    Message msg;
    int data;
    int priority;
    int result;
    int sleep_time;
    int queue_count_before;
    
    args = (ProducerArgs *)arg;
    
    if (args == NULL) {
        fprintf(stderr, "Error: producer_thread - NULL argument\n");
        return NULL;
    }
    
    printf("[%06.2f] Producer %d: Started\n", time_elapsed(), args->id);
    
    if (DEBUG_MODE) {
        printf("[DEBUG] Producer %d: Context Loaded\n", args->id);
    }
    
    // Main Lifecycle Loop
    // Continues until the main thread sets the global 'running' flag to 0.
    while (*(args->running)) {
        
        // Step 1: Data Generation
        data = random_range(DATA_RANGE_MIN, DATA_RANGE_MAX);
        priority = random_range(PRIORITY_MIN, PRIORITY_MAX);
        
        msg = message_create(data, priority, args->id);
        
        // Step 2: Pre-Enqueue Check (Statistics)
        // Check if queue is full to track "Times Blocked" metric
        queue_count_before = queue_get_count(args->queue);
        
        if (queue_is_full(args->queue)) {
            args->stats.times_blocked++;
            printf("[%06.2f] Producer %d: BLOCKED (queue full: %d/%d)\n",
                   time_elapsed(), args->id,
                   queue_count_before, queue_get_capacity(args->queue));
        }
        
        // Step 3: Enqueue (Blocking Operation)
        // This call sleeps on the semaphore if the queue is full.
        result = queue_enqueue_safe(args->queue, msg);
        
        // Handle shutdown signal
        if (result != 0) {
            if (*(args->running)) {
                fprintf(stderr, "[%06.2f] Producer %d: Enqueue failed\n", 
                        time_elapsed(), args->id);
            }
            break; // Exit loop
        }
        
        // Step 4: Success Logging
        args->stats.messages_produced++;
        
        printf("[%06.2f] Producer %d: Wrote (pri=%d, data=%d) | Queue: %d/%d\n",
               time_elapsed(), args->id,
               priority, data,
               queue_get_count(args->queue), queue_get_capacity(args->queue));
        
        // Step 5: Simulated Processing Time
        if (*(args->running)) {
            sleep_time = random_range(0, MAX_PRODUCER_WAIT);
            
            if (DEBUG_MODE) {
                printf("[DEBUG] Producer %d: Sleeping for %d s\n", args->id, sleep_time);
            }
            
            // Responsive Sleep Loop:
            // Wake up every second to check for shutdown signal
            while (sleep_time > 0 && *(args->running)) {
                sleep(1);
                sleep_time--;
            }
        }
    }
    
    // Cleanup & Exit
    printf("[%06.2f] Producer %d: Stopped (produced %d, blocked %d)\n",
           time_elapsed(), args->id,
           args->stats.messages_produced, args->stats.times_blocked);
    
    return NULL;
}

void producer_print_stats(const ProducerArgs *args)
{
    if (args == NULL) return;
    
    printf("    Producer %d: %d messages produced, %d times blocked\n",
           args->id, args->stats.messages_produced, args->stats.times_blocked);
}