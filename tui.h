/*
 * tui.h: Terminal User Interface Declarations
 * Interface for the ncurses-based animated dashboard.
 */

#ifndef TUI_H
#define TUI_H

#include "queue.h"
#include "producer.h"
#include "consumer.h"
#include "analytics.h"

/*
 * tui_init
 * --------
 * Initialises ncurses and sets up the dashboard windows.
 */
void tui_init(void);

/*
 * tui_cleanup
 * -----------
 * Restores the terminal to normal state via endwin().
 */
void tui_cleanup(void);

/*
 * tui_update
 * ----------
 * Redraws the complete dashboard frame.
 * Parameters:
 *   num_producers, num_consumers: Thread counts
 *   p_args, c_args: Arrays of thread arguments (to read stats)
 *   q: Pointer to the shared queue (to visualize buffer slots)
 *   time_remaining: Seconds left in the simulation
 *   analytics: Pointer to analytics data (throughput/occupancy history)
 */
void tui_update(int num_producers, int num_consumers,
                ProducerArgs *p_args, ConsumerArgs *c_args,
                Queue *q, int time_remaining, Analytics *analytics);

#endif /* TUI_H */
