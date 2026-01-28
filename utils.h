/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * utils.h: Utility Function Declarations
 * * Provides helper functions for system logging (hostname, time)
 * and simulation mechanics (random delays, data generation).
 */

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h> 

/* --- System Information ---
 * These functions retrieve environment details required for the 
 * "run summary" specified in the assignment text.
 */

/*
 * Retrieves the username of the effective user ID.
 * Returns: Pointer to a static string (do not free).
 * Warning: Not thread-safe if called concurrently with environment changes.
 */
const char* get_username(void);

/*
 * Writes the machine's hostname into the provided buffer.
 * Params:
 * buffer - Destination array for the hostname string.
 * size   - Size of the buffer (should be at least 256 bytes).
 * Returns: 0 on success, -1 on error.
 */
int get_hostname(char *buffer, size_t size);

/*
 * Formats the current system time as "Day Mon DD HH:MM:SS YYYY".
 * Params:
 * buffer - Destination array for the timestamp.
 * size   - Size of the buffer.
 * Returns: Pointer to the buffer on success, NULL on failure.
 */
char* get_timestamp(char *buffer, size_t size);


/* --- Simulation & Randomness ---
 * Wrappers for random number generation used to emulate 
 * unpredictable workload and processing times.
 */

/*
 * Seeds the random number generator using the current time.
 * Logic: Must be called exactly once in main() before threads are spawned.
 */
void random_init(void);

/*
 * Generates a pseudo-random integer within the specified bounds.
 * Params:
 * min - Lower bound (inclusive).
 * max - Upper bound (inclusive).
 * Returns: Integer in range [min, max].
 */
int random_range(int min, int max);

/*
 * Blocks the calling thread for a random duration.
 * Logic: Simulates the "work" done by Producers (generating data) 
 * and Consumers (processing data).
 * Params:
 * max_seconds - The upper limit for the sleep interval.
 */
void sleep_random(int max_seconds);

#endif /* UTILS_H */