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

#include "config.h"

#include <ncurses.h>
#include <locale.h>

#include <clib.h>

#include <rut.h>

#include "rig-logs.h"
#include "rig-curses-debug.h"

struct curses_state
{
    int hscroll_pos;
    int vscroll_pos;

    rut_closure_t *redraw_closure;

    int screen_width;
    int screen_height;

    int current_page;

    //WINDOW *main_window;
    WINDOW *titlebar_window;
    WINDOW *header_window;
    WINDOW *log0_window;
    WINDOW *log1_window;

} curses_state;

enum {
    RIG_DEFAULT_COLOR,
    RIG_HEADER_COLOR,
    RIG_WARNING_COLOR
};

#define PAGE_COUNT 1

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
print_log(WINDOW *log_window,
          int log_win_width,
          int log_win_height,
          const char *header,
          struct rig_log *log)
{
    struct curses_state *state = &curses_state;
    struct rig_log_entry *entry;
    int max_lines = log_win_height - 1;
    int pos = 0;
    int i = 0;

    wattrset(log_window, COLOR_PAIR (RIG_DEFAULT_COLOR));
    wbkgd(log_window, COLOR_PAIR (RIG_DEFAULT_COLOR));
    //scrollok(state->log0_window, true);

    werase(log_window);
    mvwprintw(log_window, 0, 0, header);

    rut_list_for_each(entry, &log->entries, link) {
        const char *line;
        const char *next;

        for (line = entry->message;
             i < max_lines && line && line[0] != '\0';
             line = next)
        {
            char buf[1024];
            int len = -1;

            next = strstr(line, "\n");
            if (next) {
                len = MIN(next - line, sizeof(buf) - 1);
                next++;
            }

            if (pos++ >= state->vscroll_pos) {
                const char *ptr;
                int cursor_y = log_win_height - 1 - i;
                int j;

                if (len == -1)
                    len = MIN(strlen(line), sizeof(buf) - 1);

                /* Copy string so we can NULL terminate
                 *
                 * N.B. we are only doing this because the docs for
                 * mvwaddnstr() imply we have to pass a len of -1 if
                 * we want it to stop when it reaches the edge of the
                 * window, otherwise we'd just pass len to
                 * mvwaddnstr()
                 */
                strncpy(buf, line, len);
                buf[len] = '\0';

                if (state->hscroll_pos) {
                    for (j = 0, ptr = buf;
                         j < state->hscroll_pos && ptr < buf + len;
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
                i++;
            }
        }
        if (i >= max_lines)
            break;
    }
}

static void
redraw_cb(void *user_data)
{
    rut_shell_t *shell = user_data;
    struct curses_state *state = &curses_state;
    struct rig_log *frontend_log;
    struct rig_log *simulator_log;
    int log0_win_width;

    rut_poll_shell_remove_idle(shell, state->redraw_closure);
    state->redraw_closure = NULL;

    destroy_windows();

    getmaxyx(stdscr, state->screen_height, state->screen_width);

    werase(stdscr);

    state->titlebar_window =
        subwin(stdscr, 1, state->screen_width, 0, 0);

    wattrset(state->titlebar_window, COLOR_PAIR (RIG_HEADER_COLOR));
    wbkgd(state->titlebar_window, COLOR_PAIR (RIG_HEADER_COLOR));
    werase(state->titlebar_window);
    mvwprintw(state->titlebar_window, 0, 0,
              "     Rig version %s       ← Page %d/%d →",
              PACKAGE_VERSION,
              state->current_page, PAGE_COUNT);

#if 0
    state->headerbar_window =
        subwin(stdscr, 1, state->screen_width, 1, 0);

    wattrset(state->titlebar_window, COLOR_PAIR (RIG_HEADER_COLOR));
    wbkgd(state->titlebar_window, COLOR_PAIR (RIG_HEADER_COLOR));
    werase(state->titlebar_window);
    mvwprintw(state->titlebar_window, 0, 0,
              "     Rig version %d       ← Page %d/%d →",
              PACKAGE_VERSION,
              state->current_page, PAGE_COUNT);
#endif

#if 0
    state->main_window =
        subwin(stdscr,
               state->screen_height - 1,
               state->screen_width, 1, 0);
#endif

    rig_logs_resolve(&frontend_log, &simulator_log);

    if (frontend_log && simulator_log)
        log0_win_width = state->screen_width / 2;
    else
        log0_win_width = state->screen_width;

    if (frontend_log) {
        int log_win_height = state->screen_height - 1;

        state->log0_window =
            subwin(stdscr, log_win_height, log0_win_width, 1, 0);

        print_log(state->log0_window,
                  log0_win_width,
                  log_win_height,
                  "[Frontend Log]",
                  frontend_log);
    }

    if (simulator_log) {
        int log_win_height = state->screen_height - 1;
        int log_win_width = state->screen_width - log0_win_width;

        state->log1_window =
            subwin(stdscr,
                   log_win_height,
                   log_win_width,
                   1, log0_win_width);

        print_log(state->log1_window,
                  log_win_width,
                  log_win_height,
                  "[Simulator Log]",
                  simulator_log);
    }

    redrawwin(stdscr);
}

/* NB: make sure to hold the log_lock when calling. */
static void
queue_redraw(rut_shell_t *shell)
{
    if (curses_state.redraw_closure)
        return;

    curses_state.redraw_closure =
        rut_poll_shell_add_idle(shell,
                                redraw_cb,
                                shell,
                                NULL /* destroy */);
}

static void
deinit_curses(void)
{
    destroy_windows();
    endwin();

    rig_logs_fini();
}

/* XXX: called with logs locked */
static void
log_cb(struct rig_log *log)
{
    struct rig_log *frontend_log;
    struct rig_log *simulator_log;

    rig_logs_resolve(&frontend_log, &simulator_log);

    if (frontend_log && frontend_log == log)
        queue_redraw(log->shell);
}

void
rig_curses_init(void)
{
    struct curses_state *state = &curses_state;

    rig_logs_init(log_cb);

    state->current_page = 0;

    /* XXX: we're assuming we'll get a utf8 locale */
    setlocale(LC_ALL, "");

    initscr();
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
    init_pair(RIG_WARNING_COLOR, COLOR_YELLOW, COLOR_BLACK);
#if 0
    init_pair(RIG_KEY_LABEL_COLOR, COLOR_WHITE, COLOR_GREEN);
#endif

    atexit(deinit_curses);
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
        state->vscroll_pos += 1;
        queue_redraw(shell);
        break;
    case KEY_DOWN:
        if (state->vscroll_pos)
            state->vscroll_pos -= 1;
        queue_redraw(shell);
        break;
    case KEY_PPAGE:
        state->vscroll_pos += (state->screen_height - 10);
        queue_redraw(shell);
        break;
    case KEY_NPAGE:
        state->vscroll_pos -= (state->screen_height - 10);
        if (state->vscroll_pos < 0)
            state->vscroll_pos = 0;
        queue_redraw(shell);
        break;
    }
}

void
rig_curses_add_to_shell(rut_shell_t *shell)
{
    rut_poll_shell_add_fd(shell, 0,
                          RUT_POLL_FD_EVENT_IN,
                          NULL, /* prepare */
                          handle_input_cb,
                          shell);
}
