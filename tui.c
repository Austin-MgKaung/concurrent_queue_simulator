/*
 * ELE430 Coursework: Producer-Consumer Model
 * tui.c: ncurses-based Full Dashboard
 *
 * Renders a bordered, colour-coded dashboard with:
 *   - Header with runtime/remaining timers
 *   - Queue buffer visualisation with R/W pointers and priority legend
 *   - Producer/Consumer stats tables side-by-side
 *   - Throughput bar gauges
 *   - Queue occupancy sparkline (last 20 samples)
 */

#define _POSIX_C_SOURCE 200809L
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include "tui.h"
#include "utils.h"

/* --- Colour Pair IDs --- */
#define CP_RED     1
#define CP_YELLOW  2
#define CP_GREEN   3
#define CP_CYAN    4
#define CP_WHITE   5
#define CP_GRAY    6
#define CP_BAR     7

/* --- Sparkline characters (UTF-8) --- */
static const char *spark_chars[] = {
    " ", "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83",
    "\xe2\x96\x84", "\xe2\x96\x85", "\xe2\x96\x86",
    "\xe2\x96\x87", "\xe2\x96\x88"
};

/* Helper: check if a circular-buffer slot is occupied */
static int is_valid_slot(const Queue *q, int index)
{
    if (q->count == 0) return 0;
    if (q->count == q->capacity) return 1;
    if (q->front < q->rear) return (index >= q->front && index < q->rear);
    return (index >= q->front || index < q->rear);
}

/* ------------------------------------------------------------------ */

void tui_init(void)
{
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_RED,    COLOR_RED,     -1);
        init_pair(CP_YELLOW, COLOR_YELLOW,  -1);
        init_pair(CP_GREEN,  COLOR_GREEN,   -1);
        init_pair(CP_CYAN,   COLOR_CYAN,    -1);
        init_pair(CP_WHITE,  COLOR_WHITE,   -1);
        init_pair(CP_GRAY,   COLOR_WHITE,   -1); /* fallback */
        init_pair(CP_BAR,    COLOR_CYAN,    -1);
    }
}

void tui_cleanup(void)
{
    endwin();
}

/* ------------------------------------------------------------------ */

void tui_update(int num_producers, int num_consumers,
                ProducerArgs *p_args, ConsumerArgs *c_args,
                Queue *q, int time_remaining, Analytics *analytics)
{
    int width, row;
    int i;
    double elapsed = time_elapsed();

    /* Determine terminal width (clamp minimum) */
    int term_rows, term_cols;
    getmaxyx(stdscr, term_rows, term_cols);
    (void)term_rows;
    width = (term_cols > 80) ? term_cols : 80;

    erase();

    /* ============================================================
     * 1. HEADER
     * ============================================================ */
    row = 0;
    attron(A_BOLD | COLOR_PAIR(CP_CYAN));
    mvprintw(row, 1, " ELE430 SYSTEM MONITOR");
    attroff(A_BOLD | COLOR_PAIR(CP_CYAN));

    attron(COLOR_PAIR(CP_WHITE));
    mvprintw(row, width - 40, "Runtime: ");
    attroff(COLOR_PAIR(CP_WHITE));
    attron(A_BOLD | COLOR_PAIR(CP_WHITE));
    printw("%6.1fs", elapsed);
    attroff(A_BOLD | COLOR_PAIR(CP_WHITE));

    attron(COLOR_PAIR(CP_WHITE));
    mvprintw(row, width - 20, "Remaining: ");
    attroff(COLOR_PAIR(CP_WHITE));
    attron(A_BOLD | COLOR_PAIR(CP_WHITE));
    printw("%3ds", time_remaining);
    attroff(A_BOLD | COLOR_PAIR(CP_WHITE));

    /* horizontal rule */
    row++;
    mvhline(row, 0, ACS_HLINE, width);

    /* ============================================================
     * 2. QUEUE BUFFER
     * ============================================================ */
    row++;
    attron(A_BOLD | COLOR_PAIR(CP_CYAN));
    mvprintw(row, 1, " SHARED QUEUE BUFFER (%d/%d)", q->count, q->capacity);
    attroff(A_BOLD | COLOR_PAIR(CP_CYAN));
    row++;

    /* Draw slots */
    move(row, 2);
    for (i = 0; i < q->capacity; i++) {
        if (is_valid_slot(q, i)) {
            int p = q->buffer[i].priority;
            int cp = (p >= 7) ? CP_RED : (p >= 4) ? CP_YELLOW : CP_GREEN;
            attron(A_BOLD | COLOR_PAIR(cp));
            printw("[%d]", p);
            attroff(A_BOLD | COLOR_PAIR(cp));
        } else {
            attron(A_DIM);
            printw("[ ]");
            attroff(A_DIM);
        }
    }
    row++;

    /* R/W pointers */
    {
        int col_base = 2;
        int r_col = col_base + q->front * 3 + 1;
        int w_col = col_base + q->rear  * 3 + 1;

        if (r_col == w_col) {
            attron(A_BOLD | COLOR_PAIR(CP_YELLOW));
            mvprintw(row, r_col, "X");
            attroff(A_BOLD | COLOR_PAIR(CP_YELLOW));
        } else {
            attron(A_BOLD | COLOR_PAIR(CP_CYAN));
            mvprintw(row, r_col, "R");
            attroff(A_BOLD | COLOR_PAIR(CP_CYAN));
            attron(A_BOLD | COLOR_PAIR(CP_GREEN));
            mvprintw(row, w_col, "W");
            attroff(A_BOLD | COLOR_PAIR(CP_GREEN));
        }
    }
    row++;

    /* Legend */
    mvprintw(row, 2, "Key: ");
    attron(A_BOLD | COLOR_PAIR(CP_RED));    printw("High(7-9)");  attroff(A_BOLD | COLOR_PAIR(CP_RED));
    printw("  ");
    attron(A_BOLD | COLOR_PAIR(CP_YELLOW)); printw("Med(4-6)");   attroff(A_BOLD | COLOR_PAIR(CP_YELLOW));
    printw("  ");
    attron(A_BOLD | COLOR_PAIR(CP_GREEN));  printw("Low(0-3)");   attroff(A_BOLD | COLOR_PAIR(CP_GREEN));
    printw("  ");
    attron(A_DIM); printw("[ ] Empty"); attroff(A_DIM);
    row++;

    /* divider */
    mvhline(row, 0, ACS_HLINE, width);
    row++;

    /* ============================================================
     * 3. PRODUCER / CONSUMER TABLES (side by side)
     * ============================================================ */
    {
        int col_left = 2;
        int col_right = width / 2 + 1;
        int max_rows = (num_producers > num_consumers) ? num_producers : num_consumers;

        /* Section titles */
        attron(A_BOLD | COLOR_PAIR(CP_CYAN));
        mvprintw(row, col_left, " PRODUCERS");
        mvprintw(row, col_right, " CONSUMERS");
        attroff(A_BOLD | COLOR_PAIR(CP_CYAN));

        /* Vertical divider hint */
        mvvline(row, width / 2, ACS_VLINE, max_rows + 2);
        row++;

        /* Column headers */
        attron(A_UNDERLINE);
        mvprintw(row, col_left,  "  ID   Produced   Blocked");
        mvprintw(row, col_right, "  ID   Consumed   Blocked");
        attroff(A_UNDERLINE);
        row++;

        /* Data rows */
        for (i = 0; i < max_rows; i++) {
            if (i < num_producers) {
                mvprintw(row + i, col_left, "  P%-3d", p_args[i].id);
                attron(A_BOLD | COLOR_PAIR(CP_WHITE));
                mvprintw(row + i, col_left + 7, "%-10d", p_args[i].stats.messages_produced);
                attroff(A_BOLD | COLOR_PAIR(CP_WHITE));
                int b = p_args[i].stats.times_blocked;
                if (b > 0) attron(COLOR_PAIR(CP_RED));
                mvprintw(row + i, col_left + 18, "%-7d", b);
                if (b > 0) attroff(COLOR_PAIR(CP_RED));
            }
            if (i < num_consumers) {
                mvprintw(row + i, col_right, "  C%-3d", c_args[i].id);
                attron(A_BOLD | COLOR_PAIR(CP_WHITE));
                mvprintw(row + i, col_right + 7, "%-10d", c_args[i].stats.messages_consumed);
                attroff(A_BOLD | COLOR_PAIR(CP_WHITE));
                int b = c_args[i].stats.times_blocked;
                if (b > 0) attron(COLOR_PAIR(CP_RED));
                mvprintw(row + i, col_right + 18, "%-7d", b);
                if (b > 0) attroff(COLOR_PAIR(CP_RED));
            }
        }
        row += max_rows;
    }

    /* divider */
    mvhline(row, 0, ACS_HLINE, width);
    row++;

    /* ============================================================
     * 4. THROUGHPUT BARS
     * ============================================================ */
    {
        attron(A_BOLD | COLOR_PAIR(CP_CYAN));
        mvprintw(row, 1, " THROUGHPUT");
        attroff(A_BOLD | COLOR_PAIR(CP_CYAN));
        row++;

        double prod_rate = 0.0, cons_rate = 0.0;
        if (elapsed > 0.5) {
            int total_p = 0, total_c = 0;
            for (i = 0; i < num_producers; i++) total_p += p_args[i].stats.messages_produced;
            for (i = 0; i < num_consumers; i++) total_c += c_args[i].stats.messages_consumed;
            prod_rate = total_p / elapsed;
            cons_rate = total_c / elapsed;
        }

        /* Scale: find max rate, bar width = 30 chars */
        double max_rate = (prod_rate > cons_rate) ? prod_rate : cons_rate;
        if (max_rate < 1.0) max_rate = 1.0;
        int bar_width = 30;

        /* Produced/s bar */
        {
            int filled = (int)(prod_rate / max_rate * bar_width);
            if (filled > bar_width) filled = bar_width;
            mvprintw(row, 2, "Produced/s: ");
            attron(A_BOLD | COLOR_PAIR(CP_GREEN));
            for (i = 0; i < filled; i++) addch(ACS_CKBOARD);
            attroff(A_BOLD | COLOR_PAIR(CP_GREEN));
            attron(A_DIM);
            for (i = filled; i < bar_width; i++) addch(ACS_CKBOARD);
            attroff(A_DIM);
            printw(" %.1f", prod_rate);
        }

        /* Consumed/s bar */
        {
            int filled = (int)(cons_rate / max_rate * bar_width);
            if (filled > bar_width) filled = bar_width;
            int right_col = width / 2 + 1;
            mvprintw(row, right_col, "Consumed/s: ");
            attron(A_BOLD | COLOR_PAIR(CP_CYAN));
            for (i = 0; i < filled; i++) addch(ACS_CKBOARD);
            attroff(A_BOLD | COLOR_PAIR(CP_CYAN));
            attron(A_DIM);
            for (i = filled; i < bar_width; i++) addch(ACS_CKBOARD);
            attroff(A_DIM);
            printw(" %.1f", cons_rate);
        }
        row++;
    }

    /* divider */
    mvhline(row, 0, ACS_HLINE, width);
    row++;

    /* ============================================================
     * 5. QUEUE OCCUPANCY SPARKLINE (last 20 samples)
     * ============================================================ */
    {
        attron(A_BOLD | COLOR_PAIR(CP_CYAN));
        mvprintw(row, 1, " QUEUE OCCUPANCY (last 20 samples)");
        attroff(A_BOLD | COLOR_PAIR(CP_CYAN));
        row++;

        int n = analytics->num_samples;
        int start = (n > 20) ? n - 20 : 0;
        int count = n - start;

        move(row, 2);
        for (i = 0; i < count; i++) {
            int occ = analytics->queue_samples[start + i].occupancy;
            int cap = analytics->queue_samples[start + i].capacity;
            int level = 0;
            if (cap > 0) level = occ * 8 / cap;
            if (level < 0) level = 0;
            if (level > 8) level = 8;

            /* colour by fill level */
            int cp = (level >= 7) ? CP_RED : (level >= 4) ? CP_YELLOW : CP_GREEN;
            attron(A_BOLD | COLOR_PAIR(cp));
            addstr(spark_chars[level]);
            attroff(A_BOLD | COLOR_PAIR(cp));
        }
        /* pad remaining with spaces if fewer than 20 samples */
        for (i = count; i < 20; i++) {
            attron(A_DIM);
            addstr(spark_chars[0]);
            attroff(A_DIM);
        }
        row++;
    }

    /* bottom rule */
    mvhline(row, 0, ACS_HLINE, width);
    row++;

    /* Footer */
    attron(A_BOLD | COLOR_PAIR(CP_RED));
    mvprintw(row, 2, "[Ctrl+C to Stop]");
    attroff(A_BOLD | COLOR_PAIR(CP_RED));

    refresh();
}
