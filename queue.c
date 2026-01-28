/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * queue.c: Thread-Safe FIFO Queue Implementation
 * * Implements blocking Enqueue/Dequeue operations using Semaphores.
 * * Manages Mutex locking to prevent race conditions on the buffer.
 */

#define _POSIX_C_SOURCE 200809L // Required for clock_gettime

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "queue.h"
#include "config.h"

/* --- Internal Helpers (Private) --- */

/*
 * Returns current system time in milliseconds.
 * Used to timestamp messages for latency analysis.
 */
static long get_current_time_ms(void)
{
    struct timespec ts;
    
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }
    
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

/*
 * Priority Arbitration Logic.
 * Scans the buffer for the highest priority item.
 * NOTE: Caller must hold the mutex!
 */
static int find_highest_priority_index(const Queue *q)
{
    int highest_priority;
    int highest_index;
    long oldest_timestamp;
    int i, current_index;
    
    if (q == NULL || q->count == 0) return -1;
    
    highest_index = q->front;
    highest_priority = q->buffer[q->front].priority;
    oldest_timestamp = q->buffer[q->front].timestamp;
    
    for (i = 0; i < q->count; i++) {
        current_index = (q->front + i) % q->capacity;
        
        if (q->buffer[current_index].priority > highest_priority) {
            highest_priority = q->buffer[current_index].priority;
            highest_index = current_index;
            oldest_timestamp = q->buffer[current_index].timestamp;
        }
        else if (q->buffer[current_index].priority == highest_priority) {
            // FIFO fallback for equal priorities
            if (q->buffer[current_index].timestamp < oldest_timestamp) {
                highest_index = current_index;
                oldest_timestamp = q->buffer[current_index].timestamp;
            }
        }
    }
    
    return highest_index;
}

/*
 * Low-level write to buffer.
 * NOTE: Caller must hold the mutex!
 */
static int internal_enqueue(Queue *q, Message msg)
{
    if (q->count >= q->capacity) return -1;
    
    q->buffer[q->rear] = msg;
    q->rear = (q->rear + 1) % q->capacity;
    q->count++;
    
    return 0;
}

/*
 * Low-level read from buffer with shift.
 * NOTE: Caller must hold the mutex!
 */
static int internal_dequeue(Queue *q, Message *msg)
{
    int highest_index, current_index, next_index;
    
    if (q->count == 0) return -1;
    
    highest_index = find_highest_priority_index(q);
    if (highest_index < 0) return -1;
    
    *msg = q->buffer[highest_index];
    
    // Shift elements to fill gap
    if (highest_index == q->front) {
        q->front = (q->front + 1) % q->capacity;
    } else {
        current_index = highest_index;
        while (current_index != q->front) {
            next_index = (current_index - 1 + q->capacity) % q->capacity;
            q->buffer[current_index] = q->buffer[next_index];
            current_index = next_index;
        }
        q->front = (q->front + 1) % q->capacity;
    }
    
    q->count--;
    return 0;
}

/* --- Public API: Lifecycle --- */

int queue_init(Queue *q, int capacity)
{
    int result;
    
    if (q == NULL) return -1;
    if (capacity < MIN_QUEUE_SIZE || capacity > MAX_QUEUE_SIZE) return -1;
    
    // Data setup
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    q->capacity = capacity;
    q->shutdown = 0;
    memset(q->buffer, 0, sizeof(q->buffer));
    
    // 1. Initialize Mutex
    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        return -1;
    }
    
    // 2. Init 'Slots' Semaphore (Full capacity initially)
    if (sem_init(&q->slots_available, 0, capacity) != 0) {
        pthread_mutex_destroy(&q->mutex);
        return -1;
    }
    
    // 3. Init 'Items' Semaphore (0 initially)
    if (sem_init(&q->items_available, 0, 0) != 0) {
        pthread_mutex_destroy(&q->mutex);
        sem_destroy(&q->slots_available);
        return -1;
    }
    
    return 0;
}

int queue_destroy(Queue *q)
{
    if (q == NULL) return -1;
    
    pthread_mutex_destroy(&q->mutex);
    sem_destroy(&q->slots_available);
    sem_destroy(&q->items_available);
    
    return 0;
}

/* --- Public API: Unsafe Diagnostics --- */

int queue_is_full(const Queue *q) {
    if (!q) return 0;
    return (q->count >= q->capacity);
}

int queue_is_empty(const Queue *q) {
    if (!q) return 1;
    return (q->count == 0);
}

int queue_get_count(const Queue *q) {
    if (!q) return 0;
    return q->count;
}

int queue_get_capacity(const Queue *q) {
    if (!q) return 0;
    return q->capacity;
}

void queue_display(const Queue *q)
{
    int i, index;
    if (!q) return;
    
    printf("Queue Status: %d/%d items (Shutdown=%d)\n", 
           q->count, q->capacity, q->shutdown);
    
    for (i = 0; i < q->count; i++) {
        index = (q->front + i) % q->capacity;
        printf("    [%d] Prod:%d Pri:%d Data:%d\n",
               index, q->buffer[index].producer_id, 
               q->buffer[index].priority, q->buffer[index].data);
    }
}

/* --- Public API: Thread-Safe Operations --- */

/*
 * Blocking Enqueue.
 * Uses Semaphore to wait for space, Mutex for data safety.
 */
int queue_enqueue_safe(Queue *q, Message msg)
{
    int result;
    
    if (q == NULL) return -1;
    if (q->shutdown) return -1;
    
    // 1. Wait for Space (Blocks if full)
    result = sem_wait(&q->slots_available);
    
    // Handle signal interruptions (common in threaded apps)
    if (result != 0 && errno == EINTR) {
        if (q->shutdown) return -1;
    }
    
    if (q->shutdown) {
        sem_post(&q->slots_available); // Return the token
        return -1;
    }
    
    // 2. Critical Section
    pthread_mutex_lock(&q->mutex);
    
    internal_enqueue(q, msg);
    
    pthread_mutex_unlock(&q->mutex);
    
    // 3. Signal Consumers
    sem_post(&q->items_available);
    
    return 0;
}

/*
 * Blocking Dequeue.
 * Uses Semaphore to wait for data, Mutex for data safety.
 */
int queue_dequeue_safe(Queue *q, Message *msg)
{
    int result;
    
    if (q == NULL || msg == NULL) return -1;
    if (q->shutdown) return -1;
    
    // 1. Wait for Data (Blocks if empty)
    result = sem_wait(&q->items_available);
    
    if (result != 0 && errno == EINTR) {
        if (q->shutdown) return -1;
    }
    
    if (q->shutdown) {
        sem_post(&q->items_available); // Return the token
        return -1;
    }
    
    // 2. Critical Section
    pthread_mutex_lock(&q->mutex);
    
    internal_dequeue(q, msg);
    
    pthread_mutex_unlock(&q->mutex);
    
    // 3. Signal Producers
    sem_post(&q->slots_available);
    
    return 0;
}

/*
 * Initiates Shutdown.
 * Sets flag and spams semaphores to wake any sleeping threads.
 */
void queue_shutdown(Queue *q)
{
    int i;
    if (q == NULL) return;
    
    q->shutdown = 1;
    
    // Wake up everyone so they can see the shutdown flag and exit
    // We post enough times to cover worst-case all threads waiting
    for (i = 0; i < MAX_PRODUCERS + MAX_CONSUMERS; i++) {
        sem_post(&q->slots_available);
        sem_post(&q->items_available);
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