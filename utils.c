/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * utils.c: Utility Function Implementations
 * * Implements system logging, random number generation, and high-precision timing.
 * * Uses CLOCK_MONOTONIC for reliable elapsed time tracking.
 */

#define _POSIX_C_SOURCE 200809L // Required for gethostname, struct passwd, clock_gettime

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>

#include "utils.h"

/* --- Debug Level (Runtime) --- */

int debug_level = 0;

/* --- Static State (Internal) --- */

static struct timespec program_start_time;
static int time_initialized = 0;

/* --- System Information Functions --- */

/*
 * Wrapper for getpwuid to retrieve the current username.
 * Note: Returns a pointer to static memory managed by the OS library.
 */
const char* get_username(void)
{
    struct passwd *pw;
    
    // Get password entry for current effective user ID
    pw = getpwuid(getuid());
    
    if (pw == NULL) {
        return "unknown";
    }
    
    return pw->pw_name;
}

/*
 * Retrieves the network hostname.
 * Includes buffer safety checks to prevent overflows during logging.
 */
int get_hostname(char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) {
        return -1;
    }
    
    if (gethostname(buffer, size) != 0) {
        // Fallback if the syscall fails
        strncpy(buffer, "unknown", size - 1);
        buffer[size - 1] = '\0'; 
        return -1;
    }
    
    return 0;
}

/*
 * Formats the current time for the run log.
 * Format: "Day Mon DD HH:MM:SS YYYY"
 */
char* get_timestamp(char *buffer, size_t size)
{
    time_t now;
    struct tm *timeinfo;
    
    if (buffer == NULL || size == 0) {
        return NULL;
    }
    
    time(&now);
    timeinfo = localtime(&now);
    
    if (timeinfo == NULL) {
        strncpy(buffer, "unknown", size - 1);
        buffer[size - 1] = '\0';
        return buffer;
    }
    
    if (strftime(buffer, size, "%a %b %d %H:%M:%S %Y", timeinfo) == 0) {
        strncpy(buffer, "unknown", size - 1);
        buffer[size - 1] = '\0';
    }
    
    return buffer;
}

/* --- Random Number Functions (Simulation) --- */

/*
 * Seeds the RNG. 
 * Must be called exactly once in main to ensure unique run sequences.
 */
void random_init(void)
{
    srand((unsigned int)time(NULL));
}

/*
 * Generates a random integer in [min, max].
 * Used for both priority generation and sleep times.
 */
int random_range(int min, int max)
{
    // Safety swap: ensure min is actually smaller than max
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    
    return min + (rand() % (max - min + 1));
}

/*
 * Simulates processing time for Producers/Consumers.
 * Sleeps for a random duration up to max_seconds.
 */
void sleep_random(int max_seconds)
{
    int sleep_time;
    
    if (max_seconds <= 0) {
        return;
    }
    
    sleep_time = random_range(0, max_seconds);
    sleep(sleep_time);
}

/* --- Time Tracking Functions --- */

/*
 * Records the 'Epoch' for this run.
 * Uses CLOCK_MONOTONIC to ensure timing is immune to system clock changes.
 */
void time_start(void)
{
    clock_gettime(CLOCK_MONOTONIC, &program_start_time);
    time_initialized = 1;
}

/*
 * Calculates seconds elapsed since time_start().
 * Returns: double precision float (e.g., 5.002 seconds).
 */
double time_elapsed(void)
{
    struct timespec current_time;
    double elapsed;
    
    // Auto-init safety check
    if (!time_initialized) {
        time_start();
        return 0.0;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    // Calculate difference (Seconds + Nanoseconds)
    elapsed = (current_time.tv_sec - program_start_time.tv_sec);
    elapsed += (current_time.tv_nsec - program_start_time.tv_nsec) / 1000000000.0;
    
    return elapsed;
}