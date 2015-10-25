/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <rig-config.h>

#include <ncurses.h>
#include <locale.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <clib.h>
#include <uv.h>

#include <rut.h>

#include "rig-logs.h"
#include "rig-curses-debug.h"

struct curses_state
{
    rut_shell_t *shell;

    rut_closure_t *redraw_closure;

    int screen_width;
    int screen_height;

    int current_page;

    //WINDOW *main_window;
    WINDOW *titlebar_window;
    WINDOW *header_window;

    WINDOW *log0_window;
    WINDOW *log1_window;

    /* While scrolling we refer to a snapshot of
     * the logs at the point where scrolling
     * started...*/
    struct rig_log *log0_scroll_snapshot;
    struct rig_log *log1_scroll_snapshot;

    int hscroll_pos;
    int vscroll_pos;

} curses_state;

enum {
    RIG_DEFAULT_COLOR,
    RIG_HEADER_COLOR,
    RIG_ERROR_COLOR,
    RIG_CRITICAL_COLOR,
    RIG_WARNING_COLOR,
};

#define PAGE_COUNT 1

static int real_stdin;
static int real_stdout;
static int real_stderr;

static void
destroy_windows (void)
{
    struct curses_state *state = &curses_state;

    //if (state->main_window)
    //    delwin(state->main_window);
    if (state->titlebar_window)
        delwin(state->titlebar_window);
    //if (state->headerbar_window)
    //    delwin(state->headerbar_window);
    if (state->log0_window)
        delwin(state->log0_window);
    if (state->log1_window)
        delwin(state->log1_window);
}

static void
print_message(WINDOW *log_window,
              c_log_level_flags_t level,
              const char *message,
              int hscroll_pos,
              int vscroll_pos,
              int max_lines,
              int *pos,
              int *n_lines)
{
    const char *run;
    const char *next_run;
    C_GNUC_UNUSED int log_win_width;
    int log_win_height;
    chtype color_pair;

    getmaxyx(log_window, log_win_height, log_win_width);

    switch(level)
    {
    case C_LOG_LEVEL_ERROR:
        color_pair = COLOR_PAIR(RIG_ERROR_COLOR);
        break;
    case C_LOG_LEVEL_CRITICAL:
        color_pair = COLOR_PAIR(RIG_CRITICAL_COLOR);
        break;
    case C_LOG_LEVEL_WARNING:
        color_pair = COLOR_PAIR(RIG_WARNING_COLOR);
        break;
    default:
        color_pair = COLOR_PAIR(RIG_DEFAULT_COLOR);
    }

    wattrset(log_window, color_pair);
    wbkgdset(log_window, color_pair);

    for (run = message;
         *n_lines < max_lines && run && run[0] != '\0';
         run = next_run)
    {
        char buf[1024];
        int len = -1;

        next_run = strstr(run, "\n");
        if (next_run) {
            len = MIN(next_run - run, sizeof(buf) - 1);
            next_run++;
        }

        if ((*pos)++ >= vscroll_pos) {
            const char *ptr;
            int cursor_y = log_win_height - 1 - *n_lines;
            int j;

            if (len == -1)
                len = MIN(strlen(run), sizeof(buf) - 1);

            /* Copy string so we can NULL terminate
             *
             * N.B. we are only doing this because the docs for
              * mvwaddnstr() imply we have to pass a len of -1 if
             * we want it to stop when it reaches the edge of the
             * window, otherwise we'd just pass len to
             * mvwaddnstr()
             */
            strncpy(buf, run, len);
            buf[len] = '\0';

            if (hscroll_pos) {
                for (j = 0, ptr = buf;
                     j < hscroll_pos && ptr < buf + len;
                     j++)
                    ptr = c_utf8_next_char(ptr);
            } else
                ptr = buf;

            wmove(log_window, cursor_y, 0);
            while (*ptr) {
                int y;

                waddnstr(log_window, ptr, 1);
                y = getcury(log_window);
                if (y > cursor_y)
                    break;

                ptr = c_utf8_next_char(ptr);
            }
            (*n_lines)++;
        }
    }
}

static void
print_synchronised_logs(struct rig_log *log0,
                        WINDOW *log0_window,
                        struct rig_log *log1,
                        WINDOW *log1_window,
                        int hscroll_pos,
                        int vscroll_pos)
{
    int height;
    C_GNUC_UNUSED int log0_width;
    int max_lines;
    int pos = 0;
    int n_lines = 0;
    struct rig_log_entry *entry0;
    struct rig_log_entry *entry1;

    getmaxyx(log0_window, height, log0_width);

    max_lines = height - 1;

    werase(log0_window);
    werase(log1_window);

    wattrset(log0_window, COLOR_PAIR (RIG_DEFAULT_COLOR));
    wbkgd(log0_window, COLOR_PAIR (RIG_DEFAULT_COLOR));

    wattrset(log1_window, COLOR_PAIR (RIG_DEFAULT_COLOR));
    wbkgd(log1_window, COLOR_PAIR (RIG_DEFAULT_COLOR));

    mvwprintw(log0_window, 0, 0, log0->title);
    mvwprintw(log1_window, 0, 0, log1->title);

    entry0 = rut_container_of(log0->entries.next, entry0, link);
    entry1 = rut_container_of(log1->entries.next, entry1, link);

    while (&entry0->link != &log0->entries || &entry1->link != &log1->entries) {
        for (;
             (
              &entry0->link != &log0->entries &&
              (&entry1->link == &log1->entries ||
               entry0->timestamp >= entry1->timestamp)
             );
             entry0 = rut_container_of(entry0->link.next, entry0, link)) {

            print_message(log0_window, entry0->log_level, entry0->message,
                          hscroll_pos, vscroll_pos,
                          max_lines, &pos, &n_lines);

            if (n_lines >= max_lines)
                goto done;
        }

        for (;
             (
              &entry1->link != &log1->entries &&
              (&entry0->link == &log0->entries ||
               entry1->timestamp > entry0->timestamp)
              );
             entry1 = rut_container_of(entry1->link.next, entry1, link)) {

            print_message(log1_window, entry1->log_level, entry1->message,
                          hscroll_pos, vscroll_pos,
                          max_lines, &pos, &n_lines);

            if (n_lines >= max_lines)
                goto done;
        }
    }

done:

    wnoutrefresh(log0_window);
    wnoutrefresh(log1_window);
}

static void
print_log(struct rig_log *log, WINDOW *log_window)
{
    struct curses_state *state = &curses_state;
    C_GNUC_UNUSED int log_win_width;
    int log_win_height;
    struct rig_log_entry *entry;
    int max_lines;
    int pos = 0;
    int n_lines = 0;

    getmaxyx(log_window, log_win_height, log_win_width);
    max_lines = log_win_height - 1;

    wattrset(log_window, COLOR_PAIR (RIG_DEFAULT_COLOR));
    wbkgd(log_window, COLOR_PAIR (RIG_DEFAULT_COLOR));
    //scrollok(state->log0_window, true);

    werase(log_window);
    mvwprintw(log_window, 0, 0, log->title);

    c_list_for_each(entry, &log->entries, link) {

        print_message(log_window, entry->log_level, entry->message,
                      state->hscroll_pos, state->vscroll_pos,
                      max_lines, &pos, &n_lines);

        if (n_lines >= max_lines)
            break;
    }

    wnoutrefresh(log_window);
}

static void
get_logs(struct rig_log **log0, struct rig_log **log1)
{
    *log0 = rig_logs_get_frontend_log();
    *log1 = rig_logs_get_simulator_log();
}

static void
redraw_cb(void *user_data)
{
    rut_shell_t *shell = user_data;
    struct curses_state *state = &curses_state;
    struct rig_log *log0;
    struct rig_log *log1;
    int log_win_height;

    rut_poll_shell_remove_idle_FIXME(shell, state->redraw_closure);
    state->redraw_closure = NULL;

    destroy_windows();

    getmaxyx(stdscr, state->screen_height, state->screen_width);
    log_win_height = state->screen_height - 1;

    werase(stdscr);

    state->titlebar_window =
        subwin(stdscr, 1, state->screen_width, 0, 0);

    wattrset(state->titlebar_window, COLOR_PAIR (RIG_HEADER_COLOR));
    wbkgd(state->titlebar_window, COLOR_PAIR (RIG_HEADER_COLOR));
    werase(state->titlebar_window);
    mvwprintw(state->titlebar_window, 0, 0,
              "     Rig version %s       ← Page %d/%d →",
              RIG_VERSION_STR,
              state->current_page, PAGE_COUNT);

#if 0
    state->headerbar_window =
        subwin(stdscr, 1, state->screen_width, 1, 0);

    wattrset(state->titlebar_window, COLOR_PAIR (RIG_HEADER_COLOR));
    wbkgd(state->titlebar_window, COLOR_PAIR (RIG_HEADER_COLOR));
    werase(state->titlebar_window);
    mvwprintw(state->titlebar_window, 0, 0,
              "     Rig version %d       ← Page %d/%d →",
              RIG_VERSION_STR,
              state->current_page, PAGE_COUNT);
#endif

#if 0
    state->main_window =
        subwin(stdscr,
               state->screen_height - 1,
               state->screen_width, 1, 0);
#endif

    get_logs(&log0, &log1);

    rig_logs_lock();

    if (log0 && log1) {
        int log0_win_width = state->screen_width / 2;
        int log1_win_width = state->screen_width - log0_win_width - 1;

        state->log0_window =
            subwin(stdscr, log_win_height, log0_win_width, 1, 0);

        state->log1_window =
            subwin(stdscr,
                   log_win_height,
                   log1_win_width,
                   1, log0_win_width + 1);

        print_synchronised_logs(state->vscroll_pos ? state->log0_scroll_snapshot : log0,
                                state->log0_window,
                                state->vscroll_pos ? state->log1_scroll_snapshot : log1,
                                state->log1_window,
                                state->hscroll_pos,
                                state->vscroll_pos);
    } else if (log0) {
        int log0_win_width = state->screen_width;

        state->log0_window =
            subwin(stdscr, log_win_height, log0_win_width, 1, 0);

        print_log(state->vscroll_pos ? state->log0_scroll_snapshot : log0,
                  state->log0_window);
    } else if (log1) {
        int log1_win_width = state->screen_width;

        state->log1_window =
            subwin(stdscr, log_win_height, log1_win_width, 1, 0);

        print_log(state->vscroll_pos ? state->log1_scroll_snapshot : log1,
                  state->log1_window);
    }

    rig_logs_unlock();

    redrawwin(stdscr); /* make whole window invalid */
    wrefresh(stdscr);
}

/* NB: make sure to hold the log_lock when calling. */
static void
queue_redraw(rut_shell_t *shell)
{
    if (curses_state.redraw_closure)
        return;

    curses_state.redraw_closure =
        rut_poll_shell_add_idle_FIXME(shell,
                                redraw_cb,
                                shell,
                                NULL /* destroy */);
}

static void
deinit_curses(void)
{
    destroy_windows();
    endwin();

    dup2(real_stdin, 0);
    dup2(real_stdout, 1);
    dup2(real_stderr, 2);

    rig_logs_fini();
}

/* XXX: called with logs locked */
static void
log_cb(struct rig_log *log)
{
    struct curses_state *state = &curses_state;

    if (state->shell)
        queue_redraw(state->shell);
}

static void
init_once(void)
{
    struct curses_state *state = &curses_state;
    int nullfd;
    FILE *infd, *outfd;
    SCREEN *screen;

    rig_logs_init(log_cb);

    state->current_page = 0;

    nullfd = open("/dev/null", O_RDWR|O_CLOEXEC);

    real_stdin = dup(0);
    real_stdout = dup(1);
    real_stderr = dup(2);

    dup2(nullfd, 0);
    dup2(nullfd, 1);
    dup2(nullfd, 2);

    /* XXX: we're assuming we'll get a utf8 locale */
    setlocale(LC_ALL, "");

    infd = fdopen(real_stdin, "r");
    outfd = fdopen(real_stdout, "w");

    screen = newterm(NULL, outfd, infd);
    set_term(screen);

    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE); /* enable arrow keys etc */

    cbreak(); /* don't buffer input up to \n */

    noecho();
    //curs_set(0); /* don't display the cursor */

    start_color();
    use_default_colors();

    init_pair(RIG_DEFAULT_COLOR, COLOR_WHITE, COLOR_BLACK);
    init_pair(RIG_HEADER_COLOR, COLOR_WHITE, COLOR_GREEN);
    init_pair(RIG_ERROR_COLOR, COLOR_RED, COLOR_YELLOW);
    init_pair(RIG_CRITICAL_COLOR, COLOR_RED, COLOR_YELLOW);
    init_pair(RIG_WARNING_COLOR, COLOR_YELLOW, COLOR_BLACK);
#if 0
    init_pair(RIG_KEY_LABEL_COLOR, COLOR_WHITE, COLOR_GREEN);
#endif

    atexit(deinit_curses);
}

void
rig_curses_init(void)
{
    static uv_once_t once = UV_ONCE_INIT;

    uv_once(&once, init_once);
}

static void
freeze_logs(struct curses_state *state)
{
    struct rig_log *log0, *log1;

    get_logs(&log0, &log1);

    if (log0)
        state->log0_scroll_snapshot = rig_logs_copy_log(log0);
    if (log1)
        state->log1_scroll_snapshot = rig_logs_copy_log(log1);
}

static void
thaw_logs(struct curses_state *state)
{
    if (state->log0_scroll_snapshot) {
        rig_logs_free_copy(state->log0_scroll_snapshot);
        state->log0_scroll_snapshot = NULL;
    }
    if (state->log1_scroll_snapshot) {
        rig_logs_free_copy(state->log1_scroll_snapshot);
        state->log1_scroll_snapshot = NULL;
    }
}

static void
handle_input_cb(void *user_data, int fd, int revents)
{
    struct curses_state *state = &curses_state;
    rut_shell_t *shell = user_data;
    int key = wgetch(stdscr);

    switch (key){
    case 'q':
    case 'Q':
        rut_shell_quit(shell);
        break;
    case KEY_RIGHT:
        state->hscroll_pos += 10;
        queue_redraw(shell);
        break;
    case KEY_LEFT:
        if (state->hscroll_pos)
            state->hscroll_pos -= 10;
        queue_redraw(shell);
        break;
    case KEY_UP:
        if (state->vscroll_pos == 0)
            freeze_logs(state);

        state->vscroll_pos += 1;

        queue_redraw(shell);
        break;
    case KEY_DOWN:
        if (state->vscroll_pos)
            state->vscroll_pos -= 1;

        if (state->vscroll_pos == 0)
            thaw_logs(state);

        queue_redraw(shell);
        break;
    case KEY_PPAGE:
        if (state->vscroll_pos == 0)
            freeze_logs(state);

        state->vscroll_pos += (state->screen_height - 10);
        queue_redraw(shell);
        break;
    case KEY_NPAGE:
        state->vscroll_pos -= (state->screen_height - 10);
        if (state->vscroll_pos < 0)
            state->vscroll_pos = 0;

        if (state->vscroll_pos == 0)
            thaw_logs(state);

        queue_redraw(shell);
        break;
    }
}

void
rig_curses_add_to_shell(rut_shell_t *shell)
{
    curses_state.shell = shell;

    rig_curses_init();

    rut_poll_shell_add_fd(shell, real_stdin,
                          RUT_POLL_FD_EVENT_IN,
                          NULL, /* prepare */
                          handle_input_cb,
                          shell);
}
