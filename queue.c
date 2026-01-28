/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * queue.c: FIFO Queue Data Structure Implementation
 * * Implements the circular buffer logic defined in queue.h.
 * * Handles priority-based retrieval and buffer memory management.
 */

#define _POSIX_C_SOURCE 200809L // Required for clock_gettime

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "queue.h"
#include "config.h"

/* --- Internal Helpers --- */

/*
 * returns current system time in milliseconds.
 * Used to timestamp messages for latency analysis.
 */
static long get_current_time_ms(void)
{
    struct timespec ts;
    
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0; // Fallback on error
    }
    
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

/*
 * Scans the buffer to find the highest priority item.
 * Arbitration Rule:
 * 1. Highest 'priority' value wins.
 * 2. Tie-break: Smallest 'timestamp' (Oldest) wins (FIFO).
 */
static int find_highest_priority_index(const Queue *q)
{
    int highest_priority;
    int highest_index;
    long oldest_timestamp;
    int i;
    int current_index;
    
    if (q == NULL || q->count == 0) {
        return -1;
    }
    
    // Initialize search with the head of the queue
    highest_index = q->front;
    highest_priority = q->buffer[q->front].priority;
    oldest_timestamp = q->buffer[q->front].timestamp;
    
    // Linear scan of active items
    for (i = 0; i < q->count; i++) {
        current_index = (q->front + i) % q->capacity;
        
        if (q->buffer[current_index].priority > highest_priority) {
            // Found a strictly higher priority item
            highest_priority = q->buffer[current_index].priority;
            highest_index = current_index;
            oldest_timestamp = q->buffer[current_index].timestamp;
        }
        else if (q->buffer[current_index].priority == highest_priority) {
            // Priority tie: Enforce FIFO by checking timestamps
            if (q->buffer[current_index].timestamp < oldest_timestamp) {
                highest_index = current_index;
                oldest_timestamp = q->buffer[current_index].timestamp;
            }
        }
    }
    
    return highest_index;
}

/* --- Public API Implementation --- */

int queue_init(Queue *q, int capacity)
{
    if (q == NULL) {
        fprintf(stderr, "Error: queue_init - NULL pointer\n");
        return -1;
    }
    
    // Bounds check against spec limits
    if (capacity < MIN_QUEUE_SIZE || capacity > MAX_QUEUE_SIZE) {
        fprintf(stderr, "Error: queue_init - capacity %d invalid\n", capacity);
        return -1;
    }
    
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    q->capacity = capacity;
    
    // Zero out memory to prevent stale data leaks
    memset(q->buffer, 0, sizeof(q->buffer));
    
    if (DEBUG_MODE) {
        printf("[DEBUG] Queue initialized. Capacity: %d\n", capacity);
    }
    
    return 0;
}

int queue_is_full(const Queue *q)
{
    if (q == NULL) return 0;
    return (q->count >= q->capacity);
}

int queue_is_empty(const Queue *q)
{
    if (q == NULL) return 1;
    return (q->count == 0);
}

int queue_get_count(const Queue *q)
{
    if (q == NULL) return 0;
    return q->count;
}

int queue_get_capacity(const Queue *q)
{
    if (q == NULL) return 0;
    return q->capacity;
}

/*
 * Standard FIFO Enqueue.
 * Adds item to q->rear and advances the pointer.
 */
int queue_enqueue(Queue *q, Message msg)
{
    if (q == NULL) return -1;
    
    if (queue_is_full(q)) {
        if (DEBUG_MODE) {
            printf("[DEBUG] Enqueue failed: Full (%d/%d)\n", q->count, q->capacity);
        }
        return -1;
    }
    
    // Write data
    q->buffer[q->rear] = msg;
    
    // Circular increment
    q->rear = (q->rear + 1) % q->capacity;
    q->count++;
    
    if (DEBUG_MODE) {
        printf("[DEBUG] Enqueued (P%d Pri=%d Data=%d)\n", 
               msg.producer_id, msg.priority, msg.data);
    }
    
    return 0;
}

/*
 * Priority Dequeue.
 * Removes the most important item.
 * NOTE: Requires memory shifting to maintain contiguous buffer state.
 */
int queue_dequeue(Queue *q, Message *msg)
{
    int highest_index;
    int current_index;
    int next_index;
    
    if (q == NULL || msg == NULL) return -1;
    
    if (queue_is_empty(q)) {
        return -1;
    }
    
    // Step 1: Identify target item
    highest_index = find_highest_priority_index(q);
    
    if (highest_index < 0) {
        return -1; // Should not happen given !empty check
    }
    
    // Step 2: Extract data
    *msg = q->buffer[highest_index];
    
    if (DEBUG_MODE) {
        printf("[DEBUG] Dequeued index %d (P%d Pri=%d)\n", 
               highest_index, msg->producer_id, msg->priority);
    }
    
    // Step 3: Remove item and Close Gap
    // Strategy: Shift elements from Front towards the removed index.
    
    if (highest_index == q->front) {
        // Optimization: If removing head, just advance pointer
        q->front = (q->front + 1) % q->capacity;
    }
    else {
        // Complex case: Middle removal
        // Shift predecessors forward one slot to fill the gap
        current_index = highest_index;
        
        while (current_index != q->front) {
            // Calculate index of the item strictly before current
            next_index = (current_index - 1 + q->capacity) % q->capacity;
            
            // Move it into the current slot
            q->buffer[current_index] = q->buffer[next_index];
            
            current_index = next_index;
        }
        
        // Adjust front pointer to reflect the shift
        q->front = (q->front + 1) % q->capacity;
    }
    
    q->count--;
    return 0;
}

/*
 * Non-destructive read of highest priority item.
 */
int queue_peek(const Queue *q, Message *msg)
{
    int highest_index;
    
    if (q == NULL || msg == NULL) return -1;
    if (queue_is_empty(q)) return -1;
    
    highest_index = find_highest_priority_index(q);
    
    if (highest_index < 0) return -1;
    
    *msg = q->buffer[highest_index];
    return 0;
}

void queue_display(const Queue *q)
{
    int i, index;
    
    if (q == NULL) {
        printf("Queue: NULL\n");
        return;
    }
    
    printf("Queue Status: %d/%d items (F:%d R:%d)\n", 
           q->count, q->capacity, q->front, q->rear);
           
    if (q->count == 0) {
        printf("  [Empty]\n");
        return;
    }
    
    // Dump contents for debugging
    for (i = 0; i < q->count; i++) {
        index = (q->front + i) % q->capacity;
        printf("    [%d] Prod:%d Pri:%d Data:%d Time:%ld\n",
               index,
               q->buffer[index].producer_id,
               q->buffer[index].priority,
               q->buffer[index].data,
               q->buffer[index].timestamp);
    }
}

Message message_create(int data, int priority, int producer_id)
{
    Message msg;
    
    msg.data = data;
    msg.priority = priority;
    msg.producer_id = producer_id;
    msg.timestamp = get_current_time_ms();
    
    return msg;
}