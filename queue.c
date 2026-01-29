/*
 * ELE430 Coursework: Producer-Consumer Model
 * Student: Kaung
 * Date: Jan 27, 2026
 *
 * queue.c: Thread-Safe FIFO Queue Implementation
 * * Implements blocking Enqueue/Dequeue operations using Semaphores.
 * * Manages Mutex locking to prevent race conditions on the buffer.
 *
 * ERROR HANDLING STRATEGY:
 * -----------------------
 * This file protects against:
 *   1. NULL pointer dereference      — all public functions check inputs
 *   2. Semaphore failures (EINTR)    — retried in a loop (signal-safe)
 *   3. Semaphore failures (other)    — logged to stderr, return error code
 *   4. sem_trywait EAGAIN vs error   — EAGAIN means "would block" (normal),
 *                                      other errors are unexpected failures
 *   5. sem_post overflow             — checked; overflow would corrupt the
 *                                      semaphore count invariant
 *   6. Mutex lock/unlock failures    — checked; failures indicate a corrupted
 *                                      mutex or deadlock (fatal for data safety)
 *   7. Shutdown race conditions      — re-checked after every blocking call
 */

#define _POSIX_C_SOURCE 200809L /* Required for clock_gettime */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "queue.h"
#include "config.h"
#include "utils.h"

/* --- Internal Helpers (Private) --- */

/*
 * Returns current system time in milliseconds.
 * Used to timestamp messages for latency analysis.
 *
 * Error handling: If clock_gettime fails (e.g. unsupported clock),
 * returns 0. This means aging won't apply but the system still works.
 */
static long get_current_time_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        /* Error handling: clock failure is non-fatal.
         * Returning 0 disables aging (all wait times become negative,
         * so boost stays at 0). The queue still functions correctly. */
        return 0;
    }

    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

/*
 * Calculates effective priority with aging.
 * For every AGING_INTERVAL_MS the item has waited, its effective
 * priority increases by 1, capped at PRIORITY_MAX.
 * This prevents low-priority items from starving indefinitely.
 *
 * Error handling: If now_ms is 0 (clock failure) or less than
 * the message timestamp, wait will be <= 0 and no boost is applied.
 * This is a safe degradation — priorities work normally without aging.
 */
static int effective_priority(const Message *msg, long now_ms)
{
    long wait = now_ms - msg->timestamp;
    int boost = 0;

    if (wait > 0 && AGING_INTERVAL_MS > 0)
        boost = (int)(wait / AGING_INTERVAL_MS);

    int eff = msg->priority + boost;
    if (eff > PRIORITY_MAX) eff = PRIORITY_MAX;

    DBG(DBG_TRACE, "Aging: pri=%d, wait=%ldms, boost=%d, effective=%d",
        msg->priority, wait, boost, eff);

    return eff;
}

/*
 * Priority Arbitration Logic.
 * Scans the buffer for the highest effective priority item.
 * Ties are broken by FIFO order (oldest timestamp wins).
 *
 * NOTE: Caller must hold the mutex!
 *
 * Error handling: Returns -1 if queue is NULL or empty.
 * Caller must check return value before using the index.
 */
static int find_highest_priority_index(const Queue *q)
{
    int highest_priority;
    int highest_index;
    long oldest_timestamp;
    int i, current_index;
    long now_ms;

    if (q == NULL || q->count == 0) return -1;

    now_ms = get_current_time_ms();

    highest_index = q->front;
    highest_priority = effective_priority(&q->buffer[q->front], now_ms);
    oldest_timestamp = q->buffer[q->front].timestamp;

    for (i = 0; i < q->count; i++) {
        current_index = (q->front + i) % q->capacity;
        int eff = effective_priority(&q->buffer[current_index], now_ms);

        if (eff > highest_priority) {
            highest_priority = eff;
            highest_index = current_index;
            oldest_timestamp = q->buffer[current_index].timestamp;
        }
        else if (eff == highest_priority) {
            /* FIFO fallback for equal effective priorities */
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
 *
 * Error handling: Returns -1 if buffer is full.
 * This should never happen if semaphores are working correctly,
 * but we check defensively to prevent buffer overflow.
 */
static int internal_enqueue(Queue *q, Message msg)
{
    if (q->count >= q->capacity) {
        /* Error handling: This indicates a semaphore count mismatch.
         * Should never occur in normal operation. */
        fprintf(stderr, "[ERROR] internal_enqueue: buffer overflow prevented "
                "(count=%d, capacity=%d)\n", q->count, q->capacity);
        return -1;
    }

    q->buffer[q->rear] = msg;
    q->rear = (q->rear + 1) % q->capacity;
    q->count++;

    return 0;
}

/*
 * Low-level read from buffer with gap-filling shift.
 * NOTE: Caller must hold the mutex!
 *
 * Error handling: Returns -1 if buffer is empty or priority scan fails.
 * This should never happen if semaphores are working correctly.
 */
static int internal_dequeue(Queue *q, Message *msg)
{
    int highest_index, current_index, next_index;

    if (q->count == 0) {
        /* Error handling: Dequeue from empty buffer.
         * Indicates semaphore count mismatch. */
        fprintf(stderr, "[ERROR] internal_dequeue: underflow prevented "
                "(count=0)\n");
        return -1;
    }

    highest_index = find_highest_priority_index(q);
    if (highest_index < 0) {
        /* Error handling: Priority scan failed despite count > 0.
         * This would indicate memory corruption. */
        fprintf(stderr, "[ERROR] internal_dequeue: priority scan failed\n");
        return -1;
    }

    *msg = q->buffer[highest_index];

    /* Shift elements toward front to fill the gap left by removal */
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

/*
 * Initialises the queue and all OS synchronisation resources.
 *
 * Error handling: Uses cascading cleanup — if any init step fails,
 * all previously initialised resources are destroyed before returning.
 * This prevents resource leaks on partial initialisation.
 */
int queue_init(Queue *q, int capacity)
{
    if (q == NULL) return -1;
    if (capacity < MIN_QUEUE_SIZE || capacity > MAX_QUEUE_SIZE) {
        fprintf(stderr, "[ERROR] queue_init: capacity %d out of range [%d, %d]\n",
                capacity, MIN_QUEUE_SIZE, MAX_QUEUE_SIZE);
        return -1;
    }

    /* Data setup */
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    q->capacity = capacity;
    q->shutdown = 0;
    memset(q->buffer, 0, sizeof(q->buffer));

    /* 1. Initialise Mutex — protects buffer/indices in critical sections */
    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        fprintf(stderr, "[ERROR] queue_init: pthread_mutex_init failed\n");
        return -1;
    }

    /* 2. Init 'Slots' Semaphore (starts at capacity — all slots free)
     * Error handling: destroy mutex if sem_init fails */
    if (sem_init(&q->slots_available, 0, capacity) != 0) {
        fprintf(stderr, "[ERROR] queue_init: sem_init(slots) failed\n");
        pthread_mutex_destroy(&q->mutex);
        return -1;
    }

    /* 3. Init 'Items' Semaphore (starts at 0 — no items yet)
     * Error handling: destroy mutex + slots semaphore if this fails */
    if (sem_init(&q->items_available, 0, 0) != 0) {
        fprintf(stderr, "[ERROR] queue_init: sem_init(items) failed\n");
        pthread_mutex_destroy(&q->mutex);
        sem_destroy(&q->slots_available);
        return -1;
    }

    return 0;
}

/*
 * Destroys the queue and releases OS resources.
 *
 * Error handling: Checks each destroy call. Failures here typically
 * mean the resource was already destroyed or never initialised.
 * We log but continue cleanup to release as much as possible.
 */
int queue_destroy(Queue *q)
{
    int errors = 0;

    if (q == NULL) return -1;

    if (pthread_mutex_destroy(&q->mutex) != 0) {
        fprintf(stderr, "[ERROR] queue_destroy: mutex destroy failed "
                "(may still be locked)\n");
        errors++;
    }
    if (sem_destroy(&q->slots_available) != 0) {
        fprintf(stderr, "[ERROR] queue_destroy: slots semaphore destroy failed\n");
        errors++;
    }
    if (sem_destroy(&q->items_available) != 0) {
        fprintf(stderr, "[ERROR] queue_destroy: items semaphore destroy failed\n");
        errors++;
    }

    return (errors > 0) ? -1 : 0;
}

/* --- Public API: Unsafe Diagnostics --- */

/*
 * NOTE: These functions read queue state WITHOUT holding the mutex.
 * They are safe for approximate reads (display, logging) but not
 * for decisions that affect queue operations. The values may be
 * stale by the time the caller uses them.
 */

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
 * Blocking Enqueue with accurate block detection.
 *
 * Uses sem_trywait first to detect if the thread would block.
 * If trywait fails with EAGAIN (semaphore at 0), we fall back
 * to blocking sem_wait and set was_blocked = 1.
 *
 * ERROR HANDLING:
 *   - sem_trywait EAGAIN: Normal — means "would block", not an error
 *   - sem_trywait other:  Unexpected failure — log and return error
 *   - sem_wait EINTR:     Signal interrupted the wait — retry in loop
 *   - sem_wait other:     Unexpected failure — log and return error
 *   - mutex lock fail:    Fatal for data integrity — log and return error
 *   - sem_post fail:      Semaphore overflow — log (should never happen)
 */
int queue_enqueue_safe(Queue *q, Message msg, int *was_blocked)
{
    int result;
    int blocked = 0;

    if (q == NULL) return -1;
    if (q->shutdown) return -1;

    /* 1. Try non-blocking acquire to detect if we would block */
    result = sem_trywait(&q->slots_available);
    if (result != 0) {
        if (errno == EAGAIN) {
            /* Normal: semaphore is 0, queue is full, we will block */
            blocked = 1;
        } else {
            /* Error handling: Unexpected sem_trywait failure.
             * Could be EINVAL (invalid semaphore) or other system error. */
            fprintf(stderr, "[ERROR] queue_enqueue: sem_trywait failed "
                    "(errno=%d: %s)\n", errno, strerror(errno));
            return -1;
        }

        /* Fall back to blocking wait, retrying on signal interrupts */
        do {
            result = sem_wait(&q->slots_available);
        } while (result != 0 && errno == EINTR && !q->shutdown);

        if (result != 0) {
            /* Error handling: sem_wait failed with non-EINTR error,
             * or EINTR during shutdown. Either way we cannot proceed. */
            if (errno != EINTR) {
                fprintf(stderr, "[ERROR] queue_enqueue: sem_wait failed "
                        "(errno=%d: %s)\n", errno, strerror(errno));
            }
            return -1;
        }

        if (q->shutdown) {
            /* Error handling: Acquired semaphore but shutdown was signalled.
             * Return the token to avoid leaking a semaphore count. */
            sem_post(&q->slots_available);
            return -1;
        }
    }

    /* Re-check shutdown after acquiring semaphore (could have changed) */
    if (q->shutdown) {
        sem_post(&q->slots_available);
        return -1;
    }

    if (was_blocked) *was_blocked = blocked;

    /* 2. Critical Section — mutex protects buffer/indices */
    if (pthread_mutex_lock(&q->mutex) != 0) {
        /* Error handling: Mutex lock failure is critical.
         * Could indicate deadlock or corrupted mutex.
         * Return the semaphore token and abort this operation. */
        fprintf(stderr, "[ERROR] queue_enqueue: mutex lock failed\n");
        sem_post(&q->slots_available);
        return -1;
    }

    result = internal_enqueue(q, msg);

    if (result == 0) {
        DBG(DBG_TRACE, "Enqueue: pri=%d, slot=%d, count=%d/%d, was_blocked=%d",
            msg.priority, (q->rear - 1 + q->capacity) % q->capacity,
            q->count, q->capacity, blocked);
    }

    if (pthread_mutex_unlock(&q->mutex) != 0) {
        /* Error handling: Mutex unlock failure.
         * The data was already written, but other threads may deadlock.
         * Log the error — there's no safe recovery from this. */
        fprintf(stderr, "[ERROR] queue_enqueue: mutex unlock failed\n");
    }

    if (result != 0) {
        /* Error handling: internal_enqueue failed (buffer overflow).
         * Return the slots token since we didn't actually add an item. */
        sem_post(&q->slots_available);
        return -1;
    }

    /* 3. Signal Consumers — one new item is available */
    if (sem_post(&q->items_available) != 0) {
        /* Error handling: sem_post failed — likely SEM_VALUE_MAX overflow.
         * This would desynchronise the semaphore count. Log the error. */
        fprintf(stderr, "[ERROR] queue_enqueue: sem_post(items) failed "
                "(errno=%d: %s)\n", errno, strerror(errno));
    }

    return 0;
}

/*
 * Blocking Dequeue with accurate block detection.
 *
 * Same pattern as enqueue: sem_trywait to detect blocking,
 * then blocking sem_wait if needed.
 *
 * ERROR HANDLING: Same strategy as queue_enqueue_safe (see above).
 */
int queue_dequeue_safe(Queue *q, Message *msg, int *was_blocked)
{
    int result;
    int blocked = 0;

    if (q == NULL || msg == NULL) return -1;
    if (q->shutdown) return -1;

    /* 1. Try non-blocking acquire to detect if we would block */
    result = sem_trywait(&q->items_available);
    if (result != 0) {
        if (errno == EAGAIN) {
            /* Normal: semaphore is 0, queue is empty, we will block */
            blocked = 1;
        } else {
            /* Error handling: Unexpected sem_trywait failure */
            fprintf(stderr, "[ERROR] queue_dequeue: sem_trywait failed "
                    "(errno=%d: %s)\n", errno, strerror(errno));
            return -1;
        }

        /* Fall back to blocking wait, retrying on signal interrupts */
        do {
            result = sem_wait(&q->items_available);
        } while (result != 0 && errno == EINTR && !q->shutdown);

        if (result != 0) {
            if (errno != EINTR) {
                fprintf(stderr, "[ERROR] queue_dequeue: sem_wait failed "
                        "(errno=%d: %s)\n", errno, strerror(errno));
            }
            return -1;
        }

        if (q->shutdown) {
            sem_post(&q->items_available);
            return -1;
        }
    }

    /* Re-check shutdown after acquiring semaphore */
    if (q->shutdown) {
        sem_post(&q->items_available);
        return -1;
    }

    if (was_blocked) *was_blocked = blocked;

    /* 2. Critical Section — mutex protects buffer/indices */
    if (pthread_mutex_lock(&q->mutex) != 0) {
        /* Error handling: Mutex lock failure — return semaphore token */
        fprintf(stderr, "[ERROR] queue_dequeue: mutex lock failed\n");
        sem_post(&q->items_available);
        return -1;
    }

    result = internal_dequeue(q, msg);

    if (result == 0) {
        DBG(DBG_TRACE, "Dequeue: pri=%d, data=%d, from P%d, count=%d/%d",
            msg->priority, msg->data, msg->producer_id,
            q->count, q->capacity);
    }

    if (pthread_mutex_unlock(&q->mutex) != 0) {
        /* Error handling: Mutex unlock failure — no safe recovery */
        fprintf(stderr, "[ERROR] queue_dequeue: mutex unlock failed\n");
    }

    if (result != 0) {
        /* Error handling: internal_dequeue failed (underflow).
         * Return the items token since we didn't actually remove an item. */
        sem_post(&q->items_available);
        return -1;
    }

    /* 3. Signal Producers — one slot is now free */
    if (sem_post(&q->slots_available) != 0) {
        /* Error handling: sem_post failed — semaphore count corruption */
        fprintf(stderr, "[ERROR] queue_dequeue: sem_post(slots) failed "
                "(errno=%d: %s)\n", errno, strerror(errno));
    }

    return 0;
}

/*
 * Initiates Shutdown.
 * Sets the shutdown flag and wakes all blocked threads by posting
 * to both semaphores enough times to cover worst-case (all threads waiting).
 *
 * Error handling: sem_post failures during shutdown are non-fatal.
 * We continue posting to wake as many threads as possible.
 * The shutdown flag itself is what ultimately stops the threads.
 */
void queue_shutdown(Queue *q)
{
    int i;
    if (q == NULL) return;

    /* Set flag first — threads check this after waking from sem_wait */
    q->shutdown = 1;

    /* Wake up all potentially sleeping threads.
     * We post enough times to cover worst-case: all threads waiting.
     * sem_post errors here are logged but non-fatal — the shutdown
     * flag will eventually stop threads even if some posts fail. */
    for (i = 0; i < MAX_PRODUCERS + MAX_CONSUMERS; i++) {
        if (sem_post(&q->slots_available) != 0 && errno != EOVERFLOW) {
            fprintf(stderr, "[WARN] queue_shutdown: sem_post(slots) failed\n");
        }
        if (sem_post(&q->items_available) != 0 && errno != EOVERFLOW) {
            fprintf(stderr, "[WARN] queue_shutdown: sem_post(items) failed\n");
        }
    }
}

/*
 * Factory to create a message with the current timestamp.
 *
 * Error handling: If get_current_time_ms returns 0 (clock failure),
 * the message still works — aging just won't apply to this message.
 */
Message message_create(int data, int priority, int producer_id)
{
    Message msg;
    msg.data = data;
    msg.priority = priority;
    msg.producer_id = producer_id;
    msg.timestamp = get_current_time_ms();
    return msg;
}
