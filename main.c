/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * main.c: Entry Point & Initialization
 * * Handles command line parsing, parameter validation, and 
 * system logging before starting the simulation threads.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "utils.h"
#include "queue.h"

/* --- Data Structures --- */

/*
 * Container for parsed command line arguments.
 * Allows easy passing of runtime configuration to setup functions.
 */
typedef struct {
    int num_producers;      
    int num_consumers;      
    int queue_size;         
    int timeout_seconds;    
} RuntimeParams;

/* --- Function Prototypes --- */

static void print_usage(const char *program_name);
static int parse_arguments(int argc, char *argv[], RuntimeParams *params);
static int validate_parameters(const RuntimeParams *params);
static void print_startup_info(const RuntimeParams *params);
static void print_compiled_defaults(void);
static void print_separator(void);

// Milestone 3 Verification
static void test_queue(int queue_size);

/* --- Main Execution --- */

int main(int argc, char *argv[])
{
    RuntimeParams params;
    int result;
    
    // Step 1: Initialize RNG
    // Essential to ensure random priority generation works in tests.
    random_init();
    
    // Step 2: Parse Command Line
    result = parse_arguments(argc, argv, &params);
    if (result != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Step 3: Validate Inputs
    // Ensures arguments comply with the spec limits defined in config.h.
    result = validate_parameters(&params);
    if (result != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Step 4: System Logging
    // Prints the "Run Summary" required for the coursework report.
    print_startup_info(&params);
    print_compiled_defaults();
    
    // Step 5: Test Queue Functionality (Milestone 3)
    // Runs a single-threaded functional test to verify Priority FIFO logic.
    print_separator();
    printf("QUEUE TEST (Milestone 3)\n");
    print_separator();
    test_queue(params.queue_size);
    
    // Step 6: Status Report
    printf("\n");
    print_separator();
    printf("MILESTONE STATUS\n");
    print_separator();
    printf("  [x] Milestone 1: Configuration and argument parsing\n");
    printf("  [x] Milestone 2: Utility functions\n");
    printf("  [x] Milestone 3: Queue data structure\n");
    printf("  [ ] Milestone 4: Add synchronisation\n");
    printf("  [ ] Milestone 5: Producer threads\n");
    printf("  [ ] Milestone 6: Consumer threads\n");
    printf("  [ ] Milestone 7: Timeout and cleanup\n");
    printf("\n");
    
    return EXIT_SUCCESS;
}

/* --- Helper Implementations --- */

static void print_separator(void)
{
    printf("-----------------------------------------------------------------------------\n");
}

static void print_usage(const char *program_name)
{
    printf("\n");
    printf("ELE430 Producer-Consumer Model - Usage\n");
    print_separator();
    printf("Usage: %s <producers> <consumers> <queue_size> <timeout>\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  producers   - Number of producer threads  [%d to %d]\n", 
           MIN_PRODUCERS, MAX_PRODUCERS);
    printf("  consumers   - Number of consumer threads  [%d to %d]\n", 
           MIN_CONSUMERS, MAX_RUNTIME_CONSUMERS);
    printf("  queue_size  - Maximum queue capacity      [%d to %d]\n", 
           MIN_QUEUE_SIZE, MAX_QUEUE_SIZE);
    printf("  timeout     - Runtime in seconds          [minimum %d]\n", 
           MIN_TIMEOUT);
    printf("\n");
    printf("Example:\n");
    printf("  %s 5 3 10 60\n", program_name);
    printf("\n");
}

static int parse_arguments(int argc, char *argv[], RuntimeParams *params)
{
    // Spec requires exactly 4 arguments + program name
    if (argc != 5) {
        fprintf(stderr, "Error: Expected 4 arguments, received %d\n", argc - 1);
        return -1;
    }
    
    params->num_producers = atoi(argv[1]);
    params->num_consumers = atoi(argv[2]);
    params->queue_size = atoi(argv[3]);
    params->timeout_seconds = atoi(argv[4]);
    
    return 0;
}

static int validate_parameters(const RuntimeParams *params)
{
    int is_valid = 1; 
    
    // Check bounds against config.h limits
    
    if (params->num_producers < MIN_PRODUCERS || 
        params->num_producers > MAX_PRODUCERS) {
        fprintf(stderr, "Error: producers must be between %d and %d (got %d)\n",
                MIN_PRODUCERS, MAX_PRODUCERS, params->num_producers);
        is_valid = 0;
    }
    
    if (params->num_consumers < MIN_CONSUMERS || 
        params->num_consumers > MAX_RUNTIME_CONSUMERS) {
        fprintf(stderr, "Error: consumers must be between %d and %d (got %d)\n",
                MIN_CONSUMERS, MAX_RUNTIME_CONSUMERS, params->num_consumers);
        is_valid = 0;
    }
    
    if (params->queue_size < MIN_QUEUE_SIZE || 
        params->queue_size > MAX_QUEUE_SIZE) {
        fprintf(stderr, "Error: queue_size must be between %d and %d (got %d)\n",
                MIN_QUEUE_SIZE, MAX_QUEUE_SIZE, params->queue_size);
        is_valid = 0;
    }
    
    if (params->timeout_seconds < MIN_TIMEOUT) {
        fprintf(stderr, "Error: timeout must be at least %d second(s) (got %d)\n",
                MIN_TIMEOUT, params->timeout_seconds);
        is_valid = 0;
    }
    
    return is_valid ? 0 : -1;
}

static void print_startup_info(const RuntimeParams *params)
{
    char hostname[256];
    char timestamp[64];
    
    get_hostname(hostname, sizeof(hostname));
    get_timestamp(timestamp, sizeof(timestamp));
    
    printf("\n");
    printf("ELE430 Producer-Consumer Model\n");
    
    print_separator();
    printf("SYSTEM INFORMATION\n");
    print_separator();
    printf("  User:         %s\n", get_username());
    printf("  Hostname:     %s\n", hostname);
    printf("  Date/Time:    %s\n", timestamp);
    printf("\n");
    
    print_separator();
    printf("RUNTIME PARAMETERS (Command Line)\n");
    print_separator();
    printf("  Producers:    %d\n", params->num_producers);
    printf("  Consumers:    %d\n", params->num_consumers);
    printf("  Queue Size:   %d\n", params->queue_size);
    printf("  Timeout:      %d seconds\n", params->timeout_seconds);
    printf("\n");
}

static void print_compiled_defaults(void)
{
    print_separator();
    printf("COMPILED DEFAULTS (config.h)\n");
    print_separator();
    printf("  Max Producers:     %d\n", MAX_PRODUCERS);
    printf("  Max Consumers:     %d\n", MAX_CONSUMERS);
    printf("  Max Queue Size:    %d\n", MAX_QUEUE_SIZE);
    printf("  Max Producer Wait: %d seconds\n", MAX_PRODUCER_WAIT);
    printf("  Max Consumer Wait: %d seconds\n", MAX_CONSUMER_WAIT);
    printf("  Data Range:        %d to %d\n", DATA_RANGE_MIN, DATA_RANGE_MAX);
    printf("  Priority Range:    %d to %d\n", PRIORITY_MIN, PRIORITY_MAX);
    printf("  Debug Mode:        %s\n", DEBUG_MODE ? "ENABLED" : "DISABLED");
    printf("\n");
}

/* --- Milestone 3 Test Logic --- */

/*
 * Unit test for the Queue structure.
 * Validates: initialization, enqueue, priority-based dequeue, and overflow handling.
 * Note: Runs in the main thread (no concurrency yet).
 */
static void test_queue(int queue_size)
{
    Queue q;
    Message msg;
    int result;
    int i;
    
    printf("\nTesting queue with capacity %d...\n\n", queue_size);
    
    // Test 1: Initialization
    result = queue_init(&q, queue_size);
    if (result != 0) {
        printf("FAILED: Could not initialise queue\n");
        return;
    }
    printf("  [PASS] Queue initialised\n");
    
    if (queue_is_empty(&q)) {
        printf("  [PASS] Queue is empty after init\n");
    } else {
        printf("  [FAIL] Queue should be empty after init\n");
    }
    
    // Test 2: Priority Ordering
    printf("\n  Adding messages with different priorities...\n");
    
    // We add them in mixed order: 2, 7, 1, 9, 5
    // Expected Dequeue: 9 (High), 7, 5, 2, 1 (Low)
    
    // P1: pri=2
    msg = message_create(5, 2, 1);
    result = queue_enqueue(&q, msg);
    printf("    Enqueued: P1, priority=2, data=5 %s\n", result == 0 ? "[OK]" : "[FAIL]");
    
    // P2: pri=7
    msg = message_create(3, 7, 2);
    result = queue_enqueue(&q, msg);
    printf("    Enqueued: P2, priority=7, data=3 %s\n", result == 0 ? "[OK]" : "[FAIL]");
    
    // P1: pri=1
    msg = message_create(8, 1, 1);
    result = queue_enqueue(&q, msg);
    printf("    Enqueued: P1, priority=1, data=8 %s\n", result == 0 ? "[OK]" : "[FAIL]");
    
    // P3: pri=9 (Should come out first)
    msg = message_create(2, 9, 3);
    result = queue_enqueue(&q, msg);
    printf("    Enqueued: P3, priority=9, data=2 %s\n", result == 0 ? "[OK]" : "[FAIL]");
    
    // P2: pri=5
    msg = message_create(6, 5, 2);
    result = queue_enqueue(&q, msg);
    printf("    Enqueued: P2, priority=5, data=6 %s\n", result == 0 ? "[OK]" : "[FAIL]");
    
    printf("\n  Queue count: %d/%d\n", queue_get_count(&q), queue_get_capacity(&q));
    
    // Visual verification
    printf("\n  Current queue state:\n");
    queue_display(&q);
    
    // Test 3: Dequeue Logic
    printf("\n  Dequeuing (Expected: 9, 7, 5, 2, 1)...\n");
    
    while (!queue_is_empty(&q)) {
        result = queue_dequeue(&q, &msg);
        if (result == 0) {
            printf("    Dequeued: P%d, priority=%d, data=%d\n", 
                   msg.producer_id, msg.priority, msg.data);
        } else {
            printf("    [FAIL] Dequeue failed\n");
        }
    }
    
    printf("\n");
    if (queue_is_empty(&q)) {
        printf("  [PASS] Queue is empty after dequeuing all items\n");
    } else {
        printf("  [FAIL] Queue should be empty\n");
    }
    
    // Test 4: Underflow
    result = queue_dequeue(&q, &msg);
    if (result == -1) {
        printf("  [PASS] Dequeue from empty queue returns error\n");
    } else {
        printf("  [FAIL] Dequeue from empty queue should return error\n");
    }
    
    // Test 5: Overflow / Saturation
    printf("\n  Testing queue full condition...\n");
    
    for (i = 0; i < queue_size + 2; i++) {
        msg = message_create(i, i % 10, 1);
        result = queue_enqueue(&q, msg);
        
        if (i < queue_size) {
            if (result != 0) {
                printf("    [FAIL] Enqueue %d should succeed\n", i);
            }
        } else {
            // We expect failure here because i >= capacity
            if (result == -1) {
                printf("    [PASS] Enqueue %d correctly rejected (queue full)\n", i);
            } else {
                printf("    [FAIL] Enqueue %d should fail (queue full)\n", i);
            }
        }
    }
    
    printf("\n  Queue test complete!\n");
}