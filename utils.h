/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * utils.h: Utility Function Declarations
 * * Helper functions for system identification, random number generation,
 * * and execution timing.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h> /* For size_t */
#include <stdio.h>
#include "config.h"

/* --- Debug System --- */

extern int debug_level;

#define DBG(level, fmt, ...) \
    do { \
        if ((level) <= DEBUG_MAX_LEVEL && debug_level >= (level)) \
            fprintf(stderr, "[DBG:%d] [%06.2f] " fmt "\n", \
                    (level), time_elapsed(), ##__VA_ARGS__); \
    } while (0)

/* --- System Information --- */

/*
 * Retrieves the current user's login name.
 * Note: Returns a pointer to internal static memory; do not free.
 */
const char* get_username(void);

/*
 * Retrieves the machine's hostname.
 * Populates 'buffer' with up to 'size' characters.
 * Returns 0 on success, -1 on failure.
 */
int get_hostname(char *buffer, size_t size);

/*
 * Formats the current system time as "Day Mon DD HH:MM:SS YYYY".
 * Used for logging run start/end times.
 * Returns pointer to buffer on success, NULL on error.
 */
char* get_timestamp(char *buffer, size_t size);

/* --- Randomization (Simulation) --- */

/*
 * Seeds the random number generator.
 * Must be called exactly once at program startup.
 */
void random_init(void);

/*
 * Seeds the RNG with a specific value for reproducible runs.
 * Used by the -s flag for deterministic testing.
 */
void random_init_seed(unsigned int seed);

/*
 * Generates a pseudo-random integer between min and max (inclusive).
 * Used for data generation and priority assignment.
 */
int random_range(int min, int max);

/*
 * Suspends execution for a random duration [0..max_seconds].
 * Used to simulate variable processing work.
 */
void sleep_random(int max_seconds);

/* --- Time Tracking --- */

/*
 * Records the application start time.
 * Used as the epoch for relative log timestamps.
 */
void time_start(void);

/*
 * Returns the number of seconds elapsed since time_start() was called.
 * Used for timestamping log entries (e.g., [05.23]).
 */
double time_elapsed(void);

#endif /* UTILS_H */