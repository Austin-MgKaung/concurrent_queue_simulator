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

/* --- Data Structures --- */

/*
 * Container for parsed command line arguments.
 * Used to pass runtime configuration to the simulation setup.
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

/* --- Main Execution --- */

int main(int argc, char *argv[])
{
    RuntimeParams params;
    int result;
    
    // Step 1: Initialize RNG
    // Must occur before any threads start to ensure random sequences.
    random_init();
    
    // Step 2: Parse Command Line
    result = parse_arguments(argc, argv, &params);
    if (result != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Step 3: Validate Inputs
    // Ensures user inputs are within the bounds defined in config.h (the Spec).
    result = validate_parameters(&params);
    if (result != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Step 4: System Logging
    // Prints the "Run Summary" required by the assignment criteria.
    print_startup_info(&params);
    print_compiled_defaults();
    
    // Step 5: Model Execution (Milestones 3-7)
    print_separator();
    printf("MODEL STATUS\n");
    print_separator();
    printf("Initialisation complete.\n");
    printf("\n");
    printf("[Milestone 1 Complete]\n");
    printf("Next steps:\n");
    printf("  - Implement Queue (FIFO) structure\n");
    printf("  - Add Synchronization (Mutex/Semaphores)\n");
    printf("  - Create Worker Threads\n");
    
    // Step 6: Cleanup
    // (Future: Join threads and destroy mutex/semaphores)
    
    return EXIT_SUCCESS;
}

/* --- Helper Implementations --- */

static void print_separator(void)
{
    printf("-----------------------------------------------------------------------------\n");
}

/*
 * Displays usage instructions and valid parameter ranges.
 * Called when arguments are missing or invalid.
 */
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

/*
 * Parses argv strings into integers.
 * Returns 0 on success, -1 if arg count is wrong.
 */
static int parse_arguments(int argc, char *argv[], RuntimeParams *params)
{
    // We expect exactly 4 arguments + program name = 5
    if (argc != 5) {
        fprintf(stderr, "Error: Expected 4 arguments, received %d\n", argc - 1);
        return -1;
    }
    
    // atoi returns 0 on failure, which is handled by validate_parameters
    params->num_producers = atoi(argv[1]);
    params->num_consumers = atoi(argv[2]);
    params->queue_size = atoi(argv[3]);
    params->timeout_seconds = atoi(argv[4]);
    
    return 0;
}

/*
 * Checks if parameters match the constraints in config.h.
 * Returns -1 if any parameter is out of bounds.
 */
static int validate_parameters(const RuntimeParams *params)
{
    int is_valid = 1; 
    
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

/*
 * Prints the dynamic run configuration and system details.
 */
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

/*
 * Prints the static configuration from config.h.
 * Useful for verifying what settings the model was built with.
 */
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