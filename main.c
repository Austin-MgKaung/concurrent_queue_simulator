/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * main.c: Entry Point & System Orchestration
 * * The completed model with full simulation lifecycle and TUI support.
 * * Orchestrates the full simulation lifecycle: Init -> Spawn -> Run -> Shutdown -> Report.
 *
 * ERROR HANDLING STRATEGY:
 * -----------------------
 * This file protects against:
 *   1. Thread creation failures      — logged, triggers immediate shutdown
 *   2. Thread join failures          — logged per-thread, continues joining others
 *   3. Signal handler safety         — uses write() (async-signal-safe) not printf
 *   4. Double shutdown               — guarded by shutdown_in_progress flag
 *   5. Resource leak on early exit   — cleanup_resources called on all exit paths
 *   6. Unused write() return         — cast to void to silence compiler warning
 */

#define _POSIX_C_SOURCE 200809L /* For sleep, sigaction */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "config.h"
#include "utils.h"
#include "cli.h"
#include "queue.h"
#include "analytics.h"
#include "producer.h"
#include "consumer.h"
#include "tui.h"

/* --- Global State --- */

static Queue shared_queue;
static Analytics analytics;
static RuntimeParams runtime_params;

/* Lifecycle Flags
 *
 * These flags are shared between the main thread, worker threads, and
 * signal handlers. POSIX requires volatile sig_atomic_t for variables
 * accessed from signal handlers — this guarantees atomic reads/writes
 * even on architectures where int is not naturally atomic.
 * 'volatile' prevents the compiler from caching the value in a register. */
static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t shutdown_in_progress = 0;

/* Thread Management */
static pthread_t producer_threads[MAX_PRODUCERS];
static ProducerArgs producer_args[MAX_PRODUCERS];
static int num_producers_created = 0;

static pthread_t consumer_threads[MAX_CONSUMERS];
static ConsumerArgs consumer_args[MAX_CONSUMERS];
static int num_consumers_created = 0;

/* Init Flags for Cleanup — track which resources need releasing */
static int queue_initialized = 0;
static int analytics_initialized = 0;

/* --- Local Prototypes --- */
static int create_producers(int num_producers);
static int create_consumers(int num_consumers);
static void wait_for_threads(void);
static void setup_signal_handlers(void);
static void signal_handler(int signum);
static void initiate_shutdown(void);
static void finalize_shutdown(void);
static void cleanup_resources(void);

/* --- Main Execution --- */

int main(int argc, char *argv[])
{
    int elapsed = 0;
    char csv_filename[256];

    /* 1. Initialisation */
    time_start();

    /* 2. Setup — parse and validate CLI arguments
     * Error handling: parse_arguments and validate_parameters return -1
     * on failure. We print usage and exit immediately since we can't
     * run without valid parameters. */
    if (parse_arguments(argc, argv, &runtime_params) != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (runtime_params.help_requested) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (validate_parameters(&runtime_params) != 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* Seed RNG: use explicit seed if provided (-s), otherwise time-based */
    if (runtime_params.seed_set) {
        random_init_seed(runtime_params.seed);
    } else {
        random_init();
    }

    debug_level = runtime_params.debug_level;

    setup_signal_handlers();
    print_startup_info(&runtime_params);
    print_compiled_defaults();

    /* 3. System Initialisation
     * Error handling: Each init function can fail (mutex/semaphore creation).
     * On failure, we clean up any already-initialised resources and exit. */
    print_separator();
    printf("INITIALISATION\n");
    print_separator();

    if (queue_init(&shared_queue, runtime_params.queue_size, runtime_params.aging_interval) != 0) {
        fprintf(stderr, "[ERROR] Failed to initialise queue\n");
        return EXIT_FAILURE;
    }
    queue_initialized = 1;
    printf("  Queue initialized.\n");

    if (analytics_init(&analytics, &shared_queue,
                       runtime_params.num_producers, runtime_params.num_consumers) != 0) {
        fprintf(stderr, "[ERROR] Failed to initialise analytics\n");
        cleanup_resources();
        return EXIT_FAILURE;
    }
    analytics_initialized = 1;
    printf("  Analytics initialized.\n");

    /* 4. Thread Spawning
     * Error handling: If any thread fails to create, we shut down
     * immediately and join whatever threads were already created.
     * num_producers_created/num_consumers_created track exactly
     * how many threads need joining. */
    print_separator();
    printf("SIMULATION START\n");
    print_separator();

    if (create_producers(runtime_params.num_producers) != 0 ||
        create_consumers(runtime_params.num_consumers) != 0) {
        fprintf(stderr, "[ERROR] Thread creation failed\n");
        initiate_shutdown();
        finalize_shutdown();
        wait_for_threads();
        cleanup_resources();
        return EXIT_FAILURE;
    }

    if (analytics_start_sampling(&analytics) != 0) {
        fprintf(stderr, "[WARN] Analytics sampling thread failed to start\n");
        /* Non-fatal: simulation can run without sampling */
    }
    printf("  All threads active. Running for %d seconds...\n", runtime_params.timeout_seconds);

    /* 5. Runtime Loop (Monitor) */
    if (runtime_params.tui_enabled) {
        tui_init();
    } else {
        print_separator();
        printf("EXECUTION LOG\n");
        print_separator();
    }

    while (elapsed < runtime_params.timeout_seconds && running) {
        if (runtime_params.tui_enabled) {
            /* TUI MODE — refresh at 100ms intervals */
            int remaining = runtime_params.timeout_seconds - (int)time_elapsed();
            if (remaining < 0) remaining = 0;

            tui_update(runtime_params.num_producers, runtime_params.num_consumers,
                       producer_args, consumer_args, &shared_queue, remaining, &analytics);

            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 100000000L;
            nanosleep(&ts, NULL);

            /* Sync elapsed time from wall clock */
            if ((int)time_elapsed() > elapsed) elapsed = (int)time_elapsed();

        } else {
            /* LOG MODE — update once per second */
            sleep(1);
            elapsed++;
            if (elapsed % 10 == 0 && running) {
                printf("[%06.2f] --- %d seconds remaining ---\n",
                       time_elapsed(), runtime_params.timeout_seconds - elapsed);
            }
        }
    }

    if (runtime_params.tui_enabled) {
        tui_cleanup();
    }

    /* 6. Shutdown */
    if (!runtime_params.tui_enabled) {
        print_separator();
        printf("SHUTDOWN\n");
        print_separator();
    }

    if (!shutdown_in_progress) {
        initiate_shutdown();
        DBG(DBG_INFO, "%s", "Shutdown initiated: Timeout");
    } else {
        DBG(DBG_INFO, "%s", "Shutdown initiated: Signal");
    }

    /* Stop subsystems that need pthread_join (not safe in signal context) */
    finalize_shutdown();

    if (!runtime_params.tui_enabled) printf("  Waiting for threads to finish...\n");
    wait_for_threads();
    if (!runtime_params.tui_enabled) printf("  All threads joined.\n");

    /* 7. Reporting */
    analytics_finalise(&analytics);

    print_separator();
    printf("THREAD SUMMARY\n");
    print_separator();

    print_thread_summary(num_producers_created, num_consumers_created,
                         producer_args, consumer_args, &shared_queue);

    print_separator();
    printf("ANALYTICS REPORT\n");
    print_separator();
    analytics_print_summary(&analytics);
    analytics_print_recommendations(&analytics);

    generate_csv_filename(csv_filename, sizeof(csv_filename), &runtime_params);
    if (analytics_export_csv(&analytics, csv_filename) != 0) {
        fprintf(stderr, "[WARN] CSV export failed\n");
        /* Non-fatal: report was already printed to stdout */
    }

    /* 8. Cleanup */
    cleanup_resources();

    printf("\n[Execution Complete. Exit: SUCCESS]\n\n");
    return EXIT_SUCCESS;
}

/* --- Logic Implementations --- */

/*
 * Creates producer threads.
 *
 * Error handling: If producer_init_args or pthread_create fails,
 * we return -1 immediately. The caller (main) will then shut down
 * and join the threads that were already created successfully.
 * num_producers_created tracks exactly how many need joining.
 */
static int create_producers(int num_producers)
{
    int i;
    for (i = 0; i < num_producers; i++) {
        if (producer_init_args(&producer_args[i], i + 1, &shared_queue, &running) != 0) {
            fprintf(stderr, "[ERROR] Producer %d: init_args failed\n", i + 1);
            return -1;
        }
        producer_args[i].quiet_mode = runtime_params.tui_enabled;
        producer_args[i].analytics = &analytics;

        if (pthread_create(&producer_threads[i], NULL, producer_thread, &producer_args[i]) != 0) {
            fprintf(stderr, "[ERROR] Producer %d: pthread_create failed\n", i + 1);
            return -1;
        }
        num_producers_created++;
    }
    return 0;
}

/*
 * Creates consumer threads.
 *
 * Error handling: Same strategy as create_producers.
 */
static int create_consumers(int num_consumers)
{
    int i;
    for (i = 0; i < num_consumers; i++) {
        if (consumer_init_args(&consumer_args[i], i + 1, &shared_queue, &running) != 0) {
            fprintf(stderr, "[ERROR] Consumer %d: init_args failed\n", i + 1);
            return -1;
        }
        consumer_args[i].quiet_mode = runtime_params.tui_enabled;
        consumer_args[i].analytics = &analytics;

        if (pthread_create(&consumer_threads[i], NULL, consumer_thread, &consumer_args[i]) != 0) {
            fprintf(stderr, "[ERROR] Consumer %d: pthread_create failed\n", i + 1);
            return -1;
        }
        num_consumers_created++;
    }
    return 0;
}

/*
 * Joins all created threads.
 *
 * Error handling: pthread_join can fail if:
 *   - Thread ID is invalid (ESRCH) — thread already exited and was joined
 *   - Deadlock detected (EDEADLK) — thread is trying to join itself
 *   - Not joinable (EINVAL) — thread was created detached
 * We log the error but continue joining remaining threads to avoid
 * leaking thread resources. One failed join should not prevent others.
 */
static void wait_for_threads(void)
{
    int i, result;

    for (i = 0; i < num_producers_created; i++) {
        result = pthread_join(producer_threads[i], NULL);
        if (result != 0) {
            fprintf(stderr, "[ERROR] pthread_join(producer %d) failed "
                    "(error=%d)\n", i + 1, result);
            /* Continue joining other threads — don't leak resources */
        }
    }

    for (i = 0; i < num_consumers_created; i++) {
        result = pthread_join(consumer_threads[i], NULL);
        if (result != 0) {
            fprintf(stderr, "[ERROR] pthread_join(consumer %d) failed "
                    "(error=%d)\n", i + 1, result);
        }
    }
}

/*
 * Installs signal handlers for graceful shutdown.
 *
 * Error handling: If sigaction fails, we log via perror but continue.
 * The simulation will still work — it just won't respond to that
 * particular signal gracefully (it will use the default handler).
 */
static void setup_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("[WARN] sigaction(SIGINT) failed");
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("[WARN] sigaction(SIGTERM) failed");
    }
}

/*
 * Signal handler — called asynchronously when SIGINT/SIGTERM arrives.
 *
 * CRITICAL: This function runs in signal context. Only async-signal-safe
 * functions may be called here (POSIX.1-2008 §2.4.3). Specifically:
 *   - write() is safe; printf/fprintf are NOT (they use internal locks
 *     that could deadlock if the signal interrupts a printf call)
 *   - Writes to volatile sig_atomic_t are safe
 *   - sem_post() is async-signal-safe
 *   - pthread_join, pthread_mutex_lock, fprintf are NOT safe here
 *
 * The handler only sets flags and wakes blocked threads via sem_post.
 * The main thread detects the flags and performs the actual cleanup
 * (thread joining, analytics stop) after the main loop exits.
 *
 * Error handling: write() can fail (broken pipe, etc). We cast the
 * return to void since there's nothing we can do about it in a
 * signal handler — the shutdown will proceed regardless.
 */
static void signal_handler(int signum)
{
    (void)signum;
    if (!shutdown_in_progress) {
        shutdown_in_progress = 1;
        running = 0;

        /* Wake all blocked threads so they can see the stop flag.
         * queue_shutdown only sets a flag and calls sem_post,
         * both of which are async-signal-safe. */
        if (queue_initialized) queue_shutdown(&shared_queue);

        if (!runtime_params.tui_enabled) {
            const char msg[] = "\n[SIGNAL] Shutting down...\n";
            /* Error handling: cast to void to acknowledge we're
             * intentionally ignoring write's return value.
             * In signal context, there's no recovery from write failure. */
            (void)write(STDOUT_FILENO, msg, sizeof(msg) - 1);
        }
    }
}

/*
 * Initiates system shutdown from the main thread.
 *
 * NOT called from signal context — only from main() for the timeout
 * path or early-exit (thread creation failure). The signal handler
 * performs its own minimal shutdown (flag + sem_post only).
 *
 * Error handling:
 *   - shutdown_in_progress flag prevents double-shutdown. Without this,
 *     a race between signal and timeout could queue_shutdown twice.
 *   - Each subsystem is only shut down if it was successfully initialised,
 *     checked via the queue_initialized/analytics_initialized flags.
 */
static void initiate_shutdown(void)
{
    /* Guard against double shutdown (e.g. signal already fired) */
    if (shutdown_in_progress) return;
    shutdown_in_progress = 1;

    /* Set the global stop flag — all thread loops check this */
    running = 0;

    /* Wake all blocked threads so they can see the stop flag */
    if (queue_initialized) queue_shutdown(&shared_queue);
}

/*
 * Stops subsystems that require non-signal-safe calls (pthread_join).
 * Must be called from the main thread after the main loop exits,
 * regardless of whether shutdown was triggered by signal or timeout.
 */
static void finalize_shutdown(void)
{
    /* Stop the background sampling thread (calls pthread_join internally) */
    if (analytics_initialized) analytics_stop_sampling(&analytics);
}

/*
 * Releases all allocated resources.
 *
 * Error handling: Each destroy function checks its own initialisation
 * state internally. We also check our init flags to avoid calling
 * destroy on uninitialised resources. Destroy functions log their
 * own errors internally (see queue_destroy, analytics_destroy).
 */
static void cleanup_resources(void)
{
    print_separator();
    printf("CLEANUP\n");
    print_separator();

    if (analytics_initialized) {
        if (analytics_destroy(&analytics) != 0) {
            fprintf(stderr, "[WARN] analytics_destroy reported errors\n");
        }
    }

    if (queue_initialized) {
        if (queue_destroy(&shared_queue) != 0) {
            fprintf(stderr, "[WARN] queue_destroy reported errors\n");
        }
    }

    printf("  Resources released.\n");
}
