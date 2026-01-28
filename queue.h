/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * queue.h: FIFO Queue Data Structure Declarations
 * * Implements a circular buffer with priority-aware dequeue logic.
 * * Used to store messages passing between Producer and Consumer threads.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include "config.h"

/* --- Data Structures --- */

/*
 * Represents a single work item in the system.
 * Contains the payload, priority, and metadata for tracking/analysis.
 */
typedef struct {
    int data;           // The actual data value (0-9 as per spec)
    int priority;       // Priority level (0-9, higher = more important)
    int producer_id;    // ID of the producer that created this message
    long timestamp;     // Creation time (used for performance analytics)
} Message;

/*
 * The Circular Buffer implementation.
 * Stores messages and tracks current occupancy.
 */
typedef struct {
    Message buffer[MAX_QUEUE_SIZE];  // Static storage array
    int front;                       // Index of the first item (Head)
    int rear;                        // Index of the next empty slot (Tail)
    int count;                       // Current number of items
    int capacity;                    // Max capacity (set at runtime)
} Queue;

/* --- Initialization & Status --- */

/*
 * Initializes the queue pointers and capacity.
 * Returns: 0 on success, -1 on error.
 */
int queue_init(Queue *q, int capacity);

/*
 * Checks if the queue has reached its capacity.
 * Returns: 1 if full, 0 if space is available.
 */
int queue_is_full(const Queue *q);

/*
 * Checks if the queue contains no items.
 * Returns: 1 if empty, 0 if items exist.
 */
int queue_is_empty(const Queue *q);

/* --- Getters --- */

/*
 * Returns the current number of items in the queue.
 */
int queue_get_count(const Queue *q);

/*
 * Returns the maximum capacity of the queue.
 */
int queue_get_capacity(const Queue *q);

/* --- Core Operations --- */

/*
 * Adds a message to the rear of the queue.
 * Returns: 0 on success, -1 if queue is full.
 */
int queue_enqueue(Queue *q, Message msg);

/*
 * Removes the highest priority message.
 * Logic: Scans buffer for highest priority; if tied, returns oldest (FIFO).
 * Returns: 0 on success, -1 if queue is empty.
 */
int queue_dequeue(Queue *q, Message *msg);

/*
 * Returns the highest priority message without removing it.
 * Useful for debugging or state inspection.
 */
int queue_peek(const Queue *q, Message *msg);

/* --- Helpers --- */

/*
 * Prints current queue contents to stdout.
 * Used for debugging/instrumentation.
 */
void queue_display(const Queue *q);

/*
 * Helper to create a fully populated Message struct.
 * Automatically captures the current timestamp.
 */
Message message_create(int data, int priority, int producer_id);

#endif /* QUEUE_H */