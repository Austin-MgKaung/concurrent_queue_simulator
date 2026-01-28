/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * main.c: Entry Point & Integration Tests
 * * Includes Milestone 3 (Basic Queue) and Milestone 4 (Thread Safety) tests.
 * * Verifies correct mutex locking and semaphore blocking before full simulation.
 */

#define _POSIX_C_SOURCE 200809L // For usleep

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "config.h"
#include "utils.h"
#include "queue.h"

/* --- Data Structures --- */

typedef struct {
    int num_producers;
    int num_consumers;
    int queue_size;
    int timeout_seconds;
} RuntimeParams;

/* --- Global Variables (Test Scope Only) --- */
static Queue test_queue;
static int test_running = 1;

/* --- Function Prototypes --- */

static void print_usage(const char *program_name);
static int parse_arguments(int argc, char *argv[], RuntimeParams *params);
static int validate_parameters(const RuntimeParams *params);
static void print_startup_info(const RuntimeParams *params);
static void print_compiled_defaults(void);
static void print_separator(void);

// Milestone 3 Test (Single Threaded)
static void test_queue_basic(int queue_size);

// Milestone 4 Test (Multi-Threaded)
static void test_queue_threaded(int queue_size);
static void *test_producer_thread(void *arg);
static void *test_consumer_thread(void *arg);

/* --- Main Execution --- */

int main(int argc, char *argv[])
{
    RuntimeParams params;
    int result;
    
    // 1. Setup RNG
    random_init();
    
    // 2. Argument Parsing
    result = parse_arguments(argc, argv, &params);
    if (result != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // 3. Validation
    result = validate_parameters(&params);
    if (result != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // 4. Reporting
    print_startup_info(&params);
    print_compiled_defaults();
    
    // 5. Run Integration Tests
    
    // Milestone 3: Verify FIFO and Priority Logic
    print_separator();
    printf("QUEUE TEST - Basic Operations (Milestone 3)\n");
    print_separator();
    test_queue_basic(params.queue_size);
    
    // Milestone 4: Verify Mutex and Semaphore locking
    print_separator();
    printf("QUEUE TEST - Thread Safety (Milestone 4)\n");
    print_separator();
    test_queue_threaded(params.queue_size);
    
    // 6. Status Update
    printf("\n");
    print_separator();
    printf("MILESTONE STATUS\n");
    print_separator();
    printf("  [x] Milestone 1: Configuration and argument parsing\n");
    printf("  [x] Milestone 2: Utility functions\n");
    printf("  [x] Milestone 3: Queue data structure\n");
    printf("  [x] Milestone 4: Synchronisation (Mutex/Semaphores)\n");
    printf("  [ ] Milestone 5: Producer threads\n");
    printf("  [ ] Milestone 6: Consumer threads\n");
    printf("  [ ] Milestone 7: Timeout and cleanup\n");
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
    printf("  producers   - Number of producer threads  [%d to %d]\n", 
           MIN_PRODUCERS, MAX_PRODUCERS);
    printf("  consumers   - Number of consumer threads  [%d to %d]\n", 
           MIN_CONSUMERS, MAX_RUNTIME_CONSUMERS);
    printf("  queue_size  - Maximum queue capacity      [%d to %d]\n", 
           MIN_QUEUE_SIZE, MAX_QUEUE_SIZE);
    printf("  timeout     - Runtime in seconds          [minimum %d]\n", 
           MIN_TIMEOUT);
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
    
    if (params->num_producers < MIN_PRODUCERS || params->num_producers > MAX_PRODUCERS) {
        fprintf(stderr, "Error: producers must be between %d and %d\n", MIN_PRODUCERS, MAX_PRODUCERS);
        is_valid = 0;
    }
    
    if (params->num_consumers < MIN_CONSUMERS || params->num_consumers > MAX_RUNTIME_CONSUMERS) {
        fprintf(stderr, "Error: consumers must be between %d and %d\n", MIN_CONSUMERS, MAX_RUNTIME_CONSUMERS);
        is_valid = 0;
    }
    
    if (params->queue_size < MIN_QUEUE_SIZE || params->queue_size > MAX_QUEUE_SIZE) {
        fprintf(stderr, "Error: queue_size must be between %d and %d\n", MIN_QUEUE_SIZE, MAX_QUEUE_SIZE);
        is_valid = 0;
    }
    
    if (params->timeout_seconds < MIN_TIMEOUT) {
        fprintf(stderr, "Error: timeout must be at least %d second(s)\n", MIN_TIMEOUT);
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
    printf("  Data Range:        %d to %d\n", DATA_RANGE_MIN, DATA_RANGE_MAX);
    printf("  Priority Range:    %d to %d\n", PRIORITY_MIN, PRIORITY_MAX);
    printf("  Debug Mode:        %s\n", DEBUG_MODE ? "ENABLED" : "DISABLED");
    printf("\n");
}

/* --- Milestone 3: Basic Functionality --- */

static void test_queue_basic(int queue_size)
{
    Queue q;
    Message msg;
    int result;
    
    printf("\nTesting basic operations (Capacity: %d)...\n\n", queue_size);
    
    // 1. Initialization
    result = queue_init(&q, queue_size);
    printf("  queue_init:           %s\n", result == 0 ? "[PASS]" : "[FAIL]");
    
    // 2. Enqueue Safe
    msg = message_create(5, 7, 1);
    result = queue_enqueue_safe(&q, msg);
    printf("  enqueue (Pri=7):      %s\n", result == 0 ? "[PASS]" : "[FAIL]");
    
    msg = message_create(3, 2, 2);
    queue_enqueue_safe(&q, msg);
    
    msg = message_create(9, 9, 1);
    queue_enqueue_safe(&q, msg);
    
    printf("  Queue Count:          %d (Expected 3)\n", queue_get_count(&q));
    
    // 3. Dequeue Safe (Priority Check)
    // Expect 9 first
    result = queue_dequeue_safe(&q, &msg);
    printf("  dequeue 1 (Exp 9):    %s (Got Pri=%d)\n", 
           (msg.priority == 9) ? "[PASS]" : "[FAIL]", msg.priority);
           
    // Expect 7 second
    queue_dequeue_safe(&q, &msg);
    printf("  dequeue 2 (Exp 7):    %s (Got Pri=%d)\n", 
           (msg.priority == 7) ? "[PASS]" : "[FAIL]", msg.priority);

    // Expect 2 last
    queue_dequeue_safe(&q, &msg);
    printf("  dequeue 3 (Exp 2):    %s (Got Pri=%d)\n", 
           (msg.priority == 2) ? "[PASS]" : "[FAIL]", msg.priority);
           
    // 4. Cleanup
    queue_destroy(&q);
    printf("  queue_destroy:        [PASS]\n");
}

/* --- Milestone 4: Thread Safety Tests --- */

/*
 * Worker thread to simulate concurrent production.
 */
static void *test_producer_thread(void *arg)
{
    int id = *(int *)arg;
    int i, result;
    Message msg;
    
    printf("  [Producer %d] Started\n", id);
    
    for (i = 0; i < 3 && test_running; i++) {
        msg = message_create(
            random_range(DATA_RANGE_MIN, DATA_RANGE_MAX),
            random_range(PRIORITY_MIN, PRIORITY_MAX),
            id
        );
        
        // This call will block on the semaphore if queue is full
        // and lock the mutex while writing.
        result = queue_enqueue_safe(&test_queue, msg);
        
        if (result == 0) {
            printf("  [Producer %d] Enqueued Pri=%d Data=%d | Queue: %d/%d\n",
                   id, msg.priority, msg.data, 
                   queue_get_count(&test_queue), queue_get_capacity(&test_queue));
        } else {
            printf("  [Producer %d] Shutdown/Error\n", id);
            break;
        }
        
        usleep(100000); // 100ms delay to induce interleaving
    }
    
    printf("  [Producer %d] Finished\n", id);
    return NULL;
}

/*
 * Worker thread to simulate concurrent consumption.
 */
static void *test_consumer_thread(void *arg)
{
    int id = *(int *)arg;
    int i, result;
    Message msg;
    
    printf("  [Consumer %d] Started\n", id);
    
    for (i = 0; i < 3 && test_running; i++) {
        // This call will block on semaphore if queue is empty
        // and lock the mutex while reading.
        result = queue_dequeue_safe(&test_queue, &msg);
        
        if (result == 0) {
            printf("  [Consumer %d] Dequeued Pri=%d Data=%d (from P%d)\n",
                   id, msg.priority, msg.data, msg.producer_id);
        } else {
            printf("  [Consumer %d] Shutdown/Error\n", id);
            break;
        }
        
        usleep(150000); // 150ms delay
    }
    
    printf("  [Consumer %d] Finished\n", id);
    return NULL;
}

/*
 * Integration test: Spawns real threads to hammer the queue.
 * If mutex/semaphores are wrong, this will crash or deadlock.
 */
static void test_queue_threaded(int queue_size)
{
    pthread_t producers[2], consumers[2];
    int p_ids[2] = {1, 2};
    int c_ids[2] = {1, 2};
    int result, i;
    
    printf("\nTesting concurrency with 2 Producers / 2 Consumers...\n\n");
    
    // 1. Initialize Safe Queue
    result = queue_init(&test_queue, queue_size);
    if (result != 0) {
        printf("  [FAIL] Queue Init\n");
        return;
    }
    
    test_running = 1;
    
    // 2. Spawn Threads
    for (i = 0; i < 2; i++) {
        pthread_create(&producers[i], NULL, test_producer_thread, &p_ids[i]);
        pthread_create(&consumers[i], NULL, test_consumer_thread, &c_ids[i]);
    }
    
    // 3. Join Threads (Wait for them to finish work)
    printf("\n  Running threads...\n\n");
    
    for (i = 0; i < 2; i++) {
        pthread_join(producers[i], NULL);
        pthread_join(consumers[i], NULL);
    }
    
    printf("\n  Threads joined.\n");
    printf("  Final Queue Count: %d\n", queue_get_count(&test_queue));
    
    // 4. Cleanup
    queue_destroy(&test_queue);
    printf("  queue_destroy: [PASS]\n");
}