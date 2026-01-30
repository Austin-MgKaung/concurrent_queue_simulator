/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * cli.c: Command Line Interface Implementation
 * * Implements user interaction, argument parsing, and formatted reporting.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cli.h"
#include "config.h"
#include "utils.h"

/* --- Display Functions --- */

void print_separator(void)
{
    printf("-----------------------------------------------------------------------------\n");
}

void print_usage(const char *program_name)
{
    printf("\nELE430 Producer-Consumer Model - Usage\n");
    print_separator();
    printf("Usage: %s [-h] [-v] [-d <level>] [-s <seed>] [-a <ms>] [-p <sec>] [-c <sec>]\n", program_name);
    printf("       %*s <producers> <consumers> <queue_size> <timeout>\n", (int)strlen(program_name) + 7, "");
    printf("\nArguments:\n");
    printf("  -h, --help  - Show this help message and exit\n");
    printf("  -v          - Enable Visual Dashboard (Optional)\n");
    printf("  -d <level>  - Debug level 0-3: OFF, ERROR, INFO, TRACE (Optional)\n");
    printf("  -s <seed>   - RNG seed for reproducible runs (Optional)\n");
    printf("  -a <ms>     - Priority aging interval in ms (default: %d, 0=disabled)\n", AGING_INTERVAL_MS);
    printf("  -p <sec>    - Max producer sleep between writes (default: %d)\n", MAX_PRODUCER_WAIT);
    printf("  -c <sec>    - Max consumer sleep between reads (default: %d)\n", MAX_CONSUMER_WAIT);
    printf("  producers   - Number of producer threads  [%d to %d]\n", MIN_PRODUCERS, MAX_PRODUCERS);
    printf("  consumers   - Number of consumer threads  [%d to %d]\n", MIN_CONSUMERS, MAX_RUNTIME_CONSUMERS);
    printf("  queue_size  - Maximum queue capacity      [%d to %d]\n", MIN_QUEUE_SIZE, MAX_QUEUE_SIZE);
    printf("  timeout     - Runtime in seconds          [minimum %d]\n", MIN_TIMEOUT);
    printf("\nExample:\n  %s -v 5 3 10 60\n", program_name);
    printf("\nSignals:\n  Ctrl+C (SIGINT)  - Graceful shutdown\n  SIGTERM          - Graceful shutdown\n");
}

void print_startup_info(const RuntimeParams *params)
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
    {
        const char *dbg_names[] = {"OFF", "ERROR", "INFO", "TRACE"};
        int dl = params->debug_level;
        if (dl < 0) dl = 0;
        if (dl > 3) dl = 3;
        printf("  Debug Level:  %d (%s)\n", params->debug_level, dbg_names[dl]);
    }
    printf("  Mode:         %s\n", params->tui_enabled ? "VISUAL DASHBOARD" : "CONSOLE LOG");
    printf("  Producers:    %d\n", params->num_producers);
    printf("  Consumers:    %d\n", params->num_consumers);
    printf("  Queue Size:   %d\n", params->queue_size);
    printf("  Timeout:      %d seconds\n", params->timeout_seconds);
    printf("  Producer Wait: 0-%d s\n", params->max_producer_wait);
    printf("  Consumer Wait: 0-%d s\n", params->max_consumer_wait);
    if (params->aging_interval == 0)
        printf("  Aging:        Disabled\n");
    else
        printf("  Aging:        %d ms\n", params->aging_interval);
    printf("\n");
}

void print_compiled_defaults(void)
{
    print_separator();
    printf("COMPILED DEFAULTS (config.h)\n");
    print_separator();
    printf("  Max Producers:     %d\n", MAX_PRODUCERS);
    printf("  Max Consumers:     %d\n", MAX_CONSUMERS);
    printf("  Max Queue Size:    %d\n", MAX_QUEUE_SIZE);
    printf("  Max Producer Wait: %d seconds\n", MAX_PRODUCER_WAIT);
    printf("  Max Consumer Wait: %d seconds\n", MAX_CONSUMER_WAIT);
    printf("  Debug Max Level:   %d (compile-time gate)\n", DEBUG_MAX_LEVEL);
    printf("\n");
}

/* --- Input Handling --- */

/*
 * Safely converts a string to int using strtol.
 * Returns 0 on success, -1 if the string is not a valid integer.
 */
static int safe_strtoi(const char *str, int *out)
{
    char *endptr;
    long val;

    errno = 0;
    val = strtol(str, &endptr, 10);

    if (errno != 0 || endptr == str || *endptr != '\0') return -1;
    if (val < INT_MIN || val > INT_MAX) return -1;

    *out = (int)val;
    return 0;
}

int parse_arguments(int argc, char *argv[], RuntimeParams *params)
{
    int tmp;
    int arg_idx = 1;

    /* Initialize defaults */
    params->tui_enabled = 0;
    params->debug_level = 0;
    params->seed_set = 0;
    params->seed = 0;
    params->help_requested = 0;
    params->aging_interval = AGING_INTERVAL_MS;
    params->max_producer_wait = MAX_PRODUCER_WAIT;
    params->max_consumer_wait = MAX_CONSUMER_WAIT;

    /* Check for not enough arguments first */
    if (argc < 2) return -1;

    /* Parse optional flags before positional arguments */
    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
            params->help_requested = 1;
            return 0;
        } else if (strcmp(argv[arg_idx], "-v") == 0) {
            params->tui_enabled = 1;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-s") == 0) {
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "Error: -s requires a seed argument\n");
                return -1;
            }
            if (safe_strtoi(argv[arg_idx + 1], &tmp) != 0) {
                fprintf(stderr, "Error: -s requires a numeric argument\n");
                return -1;
            }
            params->seed = (unsigned int)tmp;
            params->seed_set = 1;
            arg_idx += 2;
        } else if (strcmp(argv[arg_idx], "-d") == 0) {
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "Error: -d requires a level argument (0-3)\n");
                return -1;
            }
            if (safe_strtoi(argv[arg_idx + 1], &tmp) != 0) {
                fprintf(stderr, "Error: -d requires a numeric argument\n");
                return -1;
            }
            params->debug_level = tmp;
            if (params->debug_level < 0 || params->debug_level > DBG_TRACE) {
                fprintf(stderr, "Error: Debug level must be 0-3\n");
                return -1;
            }
            arg_idx += 2;
        } else if (strcmp(argv[arg_idx], "-a") == 0) {
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "Error: -a requires an interval in ms\n");
                return -1;
            }
            if (safe_strtoi(argv[arg_idx + 1], &tmp) != 0 || tmp < 0) {
                fprintf(stderr, "Error: -a requires a non-negative numeric argument\n");
                return -1;
            }
            params->aging_interval = tmp;
            arg_idx += 2;
        } else if (strcmp(argv[arg_idx], "-p") == 0) {
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "Error: -p requires a max wait in seconds\n");
                return -1;
            }
            if (safe_strtoi(argv[arg_idx + 1], &tmp) != 0 || tmp < 0) {
                fprintf(stderr, "Error: -p requires a non-negative numeric argument\n");
                return -1;
            }
            params->max_producer_wait = tmp;
            arg_idx += 2;
        } else if (strcmp(argv[arg_idx], "-c") == 0) {
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "Error: -c requires a max wait in seconds\n");
                return -1;
            }
            if (safe_strtoi(argv[arg_idx + 1], &tmp) != 0 || tmp < 0) {
                fprintf(stderr, "Error: -c requires a non-negative numeric argument\n");
                return -1;
            }
            params->max_consumer_wait = tmp;
            arg_idx += 2;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[arg_idx]);
            return -1;
        }
    }

    /* Check if we have the correct number of remaining arguments (4 required) */
    if (argc - arg_idx != 4) {
        fprintf(stderr, "Error: Expected 4 numeric arguments, received %d\n", argc - arg_idx);
        return -1;
    }

    if (safe_strtoi(argv[arg_idx], &params->num_producers) != 0 ||
        safe_strtoi(argv[arg_idx + 1], &params->num_consumers) != 0 ||
        safe_strtoi(argv[arg_idx + 2], &params->queue_size) != 0 ||
        safe_strtoi(argv[arg_idx + 3], &params->timeout_seconds) != 0) {
        fprintf(stderr, "Error: All arguments must be valid integers\n");
        return -1;
    }

    return 0;
}

int validate_parameters(const RuntimeParams *params)
{
    int is_valid = 1;
    if (params->num_producers < MIN_PRODUCERS || params->num_producers > MAX_PRODUCERS) is_valid = 0;
    if (params->num_consumers < MIN_CONSUMERS || params->num_consumers > MAX_RUNTIME_CONSUMERS) is_valid = 0;
    if (params->queue_size < MIN_QUEUE_SIZE || params->queue_size > MAX_QUEUE_SIZE) is_valid = 0;
    if (params->timeout_seconds < MIN_TIMEOUT) is_valid = 0;
    
    if (!is_valid) fprintf(stderr, "Error: Invalid parameters provided.\n");
    return is_valid ? 0 : -1;
}

/* --- Reporting --- */

void print_thread_summary(int num_producers, int num_consumers, 
                          ProducerArgs *p_args, ConsumerArgs *c_args,
                          Queue *q)
{
    int i;
    int total_produced = 0, total_consumed = 0;
    int blocked_p = 0, blocked_c = 0;
    int items_in_queue = queue_get_count(q);
    
    printf("\n  Queue Final State: %d/%d items\n\n", items_in_queue, queue_get_capacity(q));
    
    printf("  Producer Statistics:\n");
    for (i = 0; i < num_producers; i++) {
        producer_print_stats(&p_args[i]);
        total_produced += p_args[i].stats.messages_produced;
        blocked_p += p_args[i].stats.times_blocked;
    }
    printf("    -> Total Produced: %d | Total Blocked: %d\n\n", total_produced, blocked_p);
    
    printf("  Consumer Statistics:\n");
    for (i = 0; i < num_consumers; i++) {
        consumer_print_stats(&c_args[i]);
        total_consumed += c_args[i].stats.messages_consumed;
        blocked_c += c_args[i].stats.times_blocked;
    }
    printf("    -> Total Consumed: %d | Total Blocked: %d\n\n", total_consumed, blocked_c);
    
    printf("  Balance Check:\n");
    printf("    Produced (%d) == Consumed (%d) + Queue (%d)\n", 
           total_produced, total_consumed, items_in_queue);
           
    if (total_produced == total_consumed + items_in_queue) {
        printf("    Result: PASS\n");
    } else {
        printf("    Result: FAIL (Data Discrepancy)\n");
    }
    printf("\n");
}

void generate_csv_filename(char *buffer, size_t size, const RuntimeParams *params)
{
    snprintf(buffer, size, "queue_occupancy_p%d_c%d_q%d.csv",
             params->num_producers,
             params->num_consumers,
             params->queue_size);
}