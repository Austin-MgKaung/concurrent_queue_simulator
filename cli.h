/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * cli.h: Command Line Interface Declarations
 * * Handles argument parsing, input validation, and user reporting.
 * * Isolates UI logic from the main system orchestration.
 */

#ifndef CLI_H
#define CLI_H

#include "producer.h"
#include "consumer.h"
#include "analytics.h"

/* --- Data Structures --- */

/*
 * Container for runtime settings.
 * Defined here so parsing functions can populate it directly.
 */
typedef struct {
    int num_producers;
    int num_consumers;
    int queue_size;
    int timeout_seconds;
    int tui_enabled;      // -v flag for visualization
    int debug_level;      // -d flag for debug verbosity
    int seed_set;         // 1 if -s was provided
    unsigned int seed;    // RNG seed for reproducible runs
    int help_requested;   // -h/--help flag
    int aging_interval;   // -a flag: aging interval in ms (0 = disabled)
} RuntimeParams;

/* --- UI / Display Functions --- */

void print_separator(void);
void print_usage(const char *program_name);
void print_startup_info(const RuntimeParams *params);
void print_compiled_defaults(void);

/* --- Input Handling --- */

/*
 * Parses argv into the params structure.
 * Returns 0 on success, -1 on failure.
 */
int parse_arguments(int argc, char *argv[], RuntimeParams *params);

/*
 * Checks if parameters are within config.h limits.
 * Returns 0 on success, -1 on failure.
 */
int validate_parameters(const RuntimeParams *params);

/* --- Reporting --- */

/*
 * Generates the "Thread Summary" section of the final report.
 * Requires pointers to the thread argument arrays to read stats.
 */
void print_thread_summary(int num_producers, int num_consumers, 
                          ProducerArgs *p_args, ConsumerArgs *c_args,
                          Queue *q);

/*
 * Generates the CSV filename string based on current parameters.
 */
void generate_csv_filename(char *buffer, size_t size, const RuntimeParams *params);

#endif /* CLI_H */