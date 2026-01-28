/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * main.c: Entry Point & System Integration
 * * Completed System: Milestones 1-7.
 * * Orchestrates the full simulation lifecycle: Init -> Spawn -> Run -> Shutdown -> Report.
 */

#define _POSIX_C_SOURCE 200809L // For sleep

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "config.h"
#include "utils.h"
#include "queue.h"
#include "producer.h"
#include "consumer.h"

/* --- Data Structures --- */

typedef struct {
    int num_producers;
    int num_consumers;
    int queue_size;
    int timeout_seconds;
} RuntimeParams;

/* --- Global State --- */

// Central Communication Buffer
static Queue shared_queue;

// Global Control Flag (1=Run, 0=Stop)
// volatile ensures threads read the updated value immediately
static volatile int running = 1;

// Thread Management Arrays
static pthread_t producer_threads[MAX_PRODUCERS];
static ProducerArgs producer_args[MAX_PRODUCERS];

static pthread_t consumer_threads[MAX_CONSUMERS];
static ConsumerArgs consumer_args[MAX_CONSUMERS];

// Stored for summary reporting
static RuntimeParams runtime_params;

/* --- Function Prototypes --- */

static void print_usage(const char *program_name);
static int parse_arguments(int argc, char *argv[], RuntimeParams *params);
static int validate_parameters(const RuntimeParams *params);
static void print_startup_info(const RuntimeParams *params);
static void print_compiled_defaults(void);
static void print_separator(void);

// Thread Orchestration
static int create_producers(int num_producers);
static int create_consumers(int num_consumers);
static void wait_for_producers(int num_producers);
static void wait_for_consumers(int num_consumers);
static void print_summary(int num_producers, int num_consumers);

/* --- Main Execution --- */

int main(int argc, char *argv[])
{
    int result;
    
    // 1. Initialization Phase
    random_init();
    
    result = parse_arguments(argc, argv, &runtime_params);
    if (result != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    result = validate_parameters(&runtime_params);
    if (result != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // 2. Logging Phase
    print_startup_info(&runtime_params);
    print_compiled_defaults();
    
    // 3. Queue Setup
    print_separator();
    printf("INITIALISATION\n");
    print_separator();
    
    result = queue_init(&shared_queue, runtime_params.queue_size);
    if (result != 0) {
        fprintf(stderr, "Error: Queue init failed\n");
        return EXIT_FAILURE;
    }
    printf("  Queue initialized (capacity: %d)\n", runtime_params.queue_size);
    printf("  Sync primitives: Mutex + 2 Semaphores\n");
    
    // 4. Thread Spawning Phase
    print_separator();
    printf("SIMULATION START\n");
    print_separator();
    
    printf("\n  Creating %d Producer(s) and %d Consumer(s)...\n",
           runtime_params.num_producers, runtime_params.num_consumers);
    
    running = 1; // Enable workload loops
    
    // Start Producers
    if (create_producers(runtime_params.num_producers) != 0) {
        fprintf(stderr, "Error: Failed to create producers\n");
        queue_destroy(&shared_queue);
        return EXIT_FAILURE;
    }
    
    // Start Consumers
    if (create_consumers(runtime_params.num_consumers) != 0) {
        fprintf(stderr, "Error: Failed to create consumers\n");
        // Emergency cleanup
        running = 0;
        queue_shutdown(&shared_queue);
        wait_for_producers(runtime_params.num_producers);
        queue_destroy(&shared_queue);
        return EXIT_FAILURE;
    }
    
    printf("  All threads active.\n");
    
    // 5. Runtime Phase
    // The main thread simply waits while worker threads do the heavy lifting
    printf("\n  Running simulation for %d seconds...\n\n", runtime_params.timeout_seconds);
    
    print_separator();
    printf("EXECUTION LOG\n");
    print_separator();
    
    sleep(runtime_params.timeout_seconds);
    
    // 6. Shutdown Phase
    print_separator();
    printf("SHUTDOWN\n");
    print_separator();
    
    printf("  Timeout reached. Stopping system...\n");
    
    // A: Signal Stop
    running = 0;
    
    // B: Wake sleepers
    queue_shutdown(&shared_queue);
    
    // C: Join threads
    wait_for_producers(runtime_params.num_producers);
    wait_for_consumers(runtime_params.num_consumers);
    
    printf("  All threads joined.\n");
    
    // 7. Audit & Reporting
    print_separator();
    printf("SUMMARY\n");
    print_separator();
    
    print_summary(runtime_params.num_producers, runtime_params.num_consumers);
    
    // 8. Cleanup
    print_separator();
    printf("CLEANUP\n");
    print_separator();
    
    result = queue_destroy(&shared_queue);
    printf("  Queue destroyed: %s\n", result == 0 ? "OK" : "FAILED");
    printf("  Resources released.\n");
    
    // Final Status Check
    printf("\n");
    print_separator();
    printf("MILESTONE STATUS\n");
    print_separator();
    printf("  [x] Milestone 1: Config & Parsing\n");
    printf("  [x] Milestone 2: Utilities\n");
    printf("  [x] Milestone 3: Queue Structure\n");
    printf("  [x] Milestone 4: Synchronization\n");
    printf("  [x] Milestone 5: Producers\n");
    printf("  [x] Milestone 6: Consumers\n");
    printf("  [x] Milestone 7: Timeout & Cleanup\n");
    printf("  [x] Milestone 8: Analytics (Balance Check)\n");
    printf("\n");
    
    return EXIT_SUCCESS;
}

/* --- Helpers --- */

static void print_separator(void)
{
    printf("-----------------------------------------------------------------------------\n");
}

static void print_usage(const char *program_name)
{
    printf("\nELE430 Producer-Consumer Model - Usage\n");
    print_separator();
    printf("Usage: %s <producers> <consumers> <queue_size> <timeout>\n", program_name);
    printf("\nArguments:\n");
    printf("  producers   - Number of producer threads  [%d to %d]\n", MIN_PRODUCERS, MAX_PRODUCERS);
    printf("  consumers   - Number of consumer threads  [%d to %d]\n", MIN_CONSUMERS, MAX_RUNTIME_CONSUMERS);
    printf("  queue_size  - Maximum queue capacity      [%d to %d]\n", MIN_QUEUE_SIZE, MAX_QUEUE_SIZE);
    printf("  timeout     - Runtime in seconds          [minimum %d]\n", MIN_TIMEOUT);
    printf("\nExample:\n  %s 5 3 10 60\n", program_name);
}

static int parse_arguments(int argc, char *argv[], RuntimeParams *params)
{
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
    if (params->num_producers < MIN_PRODUCERS || params->num_producers > MAX_PRODUCERS) is_valid = 0;
    if (params->num_consumers < MIN_CONSUMERS || params->num_consumers > MAX_RUNTIME_CONSUMERS) is_valid = 0;
    if (params->queue_size < MIN_QUEUE_SIZE || params->queue_size > MAX_QUEUE_SIZE) is_valid = 0;
    if (params->timeout_seconds < MIN_TIMEOUT) is_valid = 0;
    
    if (!is_valid) fprintf(stderr, "Error: Invalid parameters provided.\n");
    return is_valid ? 0 : -1;
}

static void print_startup_info(const RuntimeParams *params)
{
    char hostname[256], timestamp[64];
    get_hostname(hostname, sizeof(hostname));
    get_timestamp(timestamp, sizeof(timestamp));
    
    printf("\nELE430 Producer-Consumer Model\n");
    print_separator();
    printf("SYSTEM INFORMATION\n");
    print_separator();
    printf("  User:         %s\n", get_username());
    printf("  Hostname:     %s\n", hostname);
    printf("  Date/Time:    %s\n", timestamp);
    printf("\n");
    
    print_separator();
    printf("RUNTIME PARAMETERS\n");
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
    printf("  Debug Mode:        %s\n", DEBUG_MODE ? "ENABLED" : "DISABLED");
    printf("\n");
}

/* --- Thread Orchestration --- */

static int create_producers(int num_producers)
{
    int i, res;
    for (i = 0; i < num_producers; i++) {
        res = producer_init_args(&producer_args[i], i + 1, &shared_queue, &running);
        if (res != 0) return -1;
        
        res = pthread_create(&producer_threads[i], NULL, producer_thread, &producer_args[i]);
        if (res != 0) return -1;
    }
    return 0;
}

static int create_consumers(int num_consumers)
{
    int i, res;
    for (i = 0; i < num_consumers; i++) {
        res = consumer_init_args(&consumer_args[i], i + 1, &shared_queue, &running);
        if (res != 0) return -1;
        
        res = pthread_create(&consumer_threads[i], NULL, consumer_thread, &consumer_args[i]);
        if (res != 0) return -1;
    }
    return 0;
}

static void wait_for_producers(int num_producers)
{
    int i;
    for (i = 0; i < num_producers; i++) {
        pthread_join(producer_threads[i], NULL);
    }
}

static void wait_for_consumers(int num_consumers)
{
    int i;
    for (i = 0; i < num_consumers; i++) {
        pthread_join(consumer_threads[i], NULL);
    }
}

static void print_summary(int num_producers, int num_consumers)
{
    int i;
    int total_produced = 0, total_consumed = 0;
    int blocked_p = 0, blocked_c = 0;
    int items_in_queue = queue_get_count(&shared_queue);
    
    printf("\n  Queue State: %d/%d items\n\n", items_in_queue, queue_get_capacity(&shared_queue));
    
    printf("  Producer Stats:\n");
    for (i = 0; i < num_producers; i++) {
        producer_print_stats(&producer_args[i]);
        total_produced += producer_args[i].stats.messages_produced;
        blocked_p += producer_args[i].stats.times_blocked;
    }
    printf("    -> Total Produced: %d | Total Blocked: %d\n\n", total_produced, blocked_p);
    
    printf("  Consumer Stats:\n");
    for (i = 0; i < num_consumers; i++) {
        consumer_print_stats(&consumer_args[i]);
        total_consumed += consumer_args[i].stats.messages_consumed;
        blocked_c += consumer_args[i].stats.times_blocked;
    }
    printf("    -> Total Consumed: %d | Total Blocked: %d\n\n", total_consumed, blocked_c);
    
    // Correctness Proof
    printf("  Balance Check:\n");
    printf("    Produced (%d) == Consumed (%d) + Queue (%d)\n", 
           total_produced, total_consumed, items_in_queue);
           
    if (total_produced == total_consumed + items_in_queue) {
        printf("    Result: PASS\n");
    } else {
        printf("    Result: FAIL (Data Lost/Created?)\n");
    }
    printf("\n");
}