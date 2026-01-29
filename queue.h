/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * queue.h: Thread-Safe FIFO Queue Declarations
 * * Includes synchronization for safe concurrent access.
 * * Defines the blocking interface using Mutex (exclusion) and Semaphores (signaling).
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <semaphore.h>

#include "config.h"

/* --- Data Structures --- */

/*
 * Represents a single work item passed between threads.
 * Includes metadata (producer_id, timestamp) for the required analysis report.
 */
typedef struct {
    int data;           // The payload value
    int priority;       // 0-9 (Higher values retrieved first)
    int producer_id;    // Traceability for logs
    long timestamp;     // Creation time (used to calculate latency)
} Message;

/*
 * The Thread-Safe Circular Buffer.
 * combines the storage array with the synchronization primitives 
 * required to prevent race conditions and handle thread blocking.
 */
typedef struct {
    /* Queue Data */
    Message buffer[MAX_QUEUE_SIZE];  
    int front;                       // Read Index
    int rear;                        // Write Index
    int count;                       // Current occupancy
    int capacity;                    // Max size (runtime)
    
    /* Synchronization Primitives */
    pthread_mutex_t mutex;           // Critical Section Lock (Protects buffer/indices)
    sem_t slots_available;           // Counting Sem: How many empty spots left? (Producers wait)
    sem_t items_available;           // Counting Sem: How many items ready? (Consumers wait)
    
    /* Control Flags */
    int shutdown;                    // Set to 1 to signal all threads to exit
} Queue;

/* --- Lifecycle & Management --- */

/*
 * Initialises the queue and OS synchronization resources.
 * Returns: 0 on success, -1 if mutex/sem init fails.
 */
int queue_init(Queue *q, int capacity);

/*
 * Destroys the queue and cleans up OS resources.
 * Must be called on exit to prevent resource leaks.
 */
int queue_destroy(Queue *q);

/* --- Unsafe Operations (Internal/Debug) ---
 * WARNING: These do not lock the mutex. 
 * Use only for debugging/logging or inside safe wrappers.
 */

int queue_is_full(const Queue *q);
int queue_is_empty(const Queue *q);
int queue_get_count(const Queue *q);
int queue_get_capacity(const Queue *q);

/*
 * Prints current state to stdout.
 * Not thread-safe; use for snapshots.
 */
void queue_display(const Queue *q);

/* --- Thread-Safe Operations (Blocking) --- */

/*
 * Blocking Enqueue.
 * Logic:
 * 1. Decrement 'slots_available' (Blocks if queue is full).
 * 2. Acquire 'mutex'.
 * 3. Add item.
 * 4. Release 'mutex'.
 * 5. Increment 'items_available' (Signals a consumer).
 * Returns: 0 on success, -1 if shutdown.
 */
int queue_enqueue_safe(Queue *q, Message msg, int *was_blocked);

/*
 * Blocking Dequeue (Priority Aware).
 * Logic:
 * 1. Decrement 'items_available' (Blocks if queue is empty).
 * 2. Acquire 'mutex'.
 * 3. Remove highest priority item.
 * 4. Release 'mutex'.
 * 5. Increment 'slots_available' (Signals a producer).
 * Returns: 0 on success, -1 if shutdown.
 */
int queue_dequeue_safe(Queue *q, Message *msg, int *was_blocked);

/*
 * Signal for Shutdown.
 * Sets the shutdown flag and posts to all semaphores to wake sleeping threads.
 */
void queue_shutdown(Queue *q);

/* --- Helpers --- */

/*
 * Factory to create a message with the current timestamp.
 */
Message message_create(int data, int priority, int producer_id);

#endif /* QUEUE_H */
