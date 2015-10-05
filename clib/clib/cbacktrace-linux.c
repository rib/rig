/*
 * Copyright (C) 2015 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <clib-config.h>

#include <clib.h>

#include <execinfo.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_BACKTRACE_SIZE 100
struct thread_state
{
    void *addresses[MAX_BACKTRACE_SIZE];
};

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static c_tls_t tls;
static char *self;
static c_mutex_t symbol_table_lock;
static c_hash_table_t *symbol_table;

struct _c_backtrace
{
    int n_frames;
    void *addresses[];
};

static void
destroy_tls_state_cb(void *tls_data)
{
    c_free(tls_data);
}

static void
initialize(void)
{
    c_tls_init(&tls, destroy_tls_state_cb);

    c_mutex_init(&symbol_table_lock);

    symbol_table = c_hash_table_new_full(c_direct_hash,
                                         c_direct_equal,
                                         NULL, /* key destroy */
                                         c_free /* value destroy */);
}

void **
c_backtrace(int *n_frames)
{
    struct thread_state *state;

    pthread_once(&init_once, initialize);

    state = c_tls_get(&tls);
    if (C_UNLIKELY(!state)) {
        state = c_malloc(sizeof(*state));
        c_tls_set(&tls, state);
    }

    *n_frames = backtrace(state->addresses, MAX_BACKTRACE_SIZE);

    return state->addresses;
}

static char *
readlink_alloc(const char *linkname)
{
    int buf_size = 32;

    while (true) {
        char *buf = c_malloc(buf_size);
        int got = readlink(linkname, buf, buf_size - 1);

        if (got < 0) {
            c_free(buf);
            return NULL;
        } else if (got < buf_size - 1) {
            buf[got] = '\0';
            return buf;
        } else {
            c_free(buf);
            buf_size *= 2;
        }
    }
}

static bool
resolve_symbols_via_addr2line(void **addresses,
                              int n_addresses)
{
    const char *base_args[] = { "addr2line", "-f", "-e" };
    char *addr_out;
    char **argv;
    int exit_status;
    int extra_args = C_N_ELEMENTS(base_args);
    int address_args = extra_args + 1;
    bool status;
    int i;

    argv = c_alloca(sizeof(char *) * (n_addresses + address_args + 1));
    memcpy(argv, base_args, sizeof(base_args));

    if (!self)
        self = readlink_alloc("/proc/self/exe");

    argv[extra_args] = self;
    if (argv[extra_args] == NULL)
        return false;

    for (i = 0; i < n_addresses; i++)
        argv[i + address_args] = c_strdup_printf("%p", addresses[i]);
    argv[address_args + n_addresses] = NULL;

    status = c_spawn_sync(NULL, /* working_directory */
                          argv,
                          NULL, /* envp */
                          C_SPAWN_STDERR_TO_DEV_NULL | C_SPAWN_SEARCH_PATH,
                          NULL, /* child_setup */
                          NULL, /* user_data for child_setup */
                          &addr_out,
                          NULL, /* standard_error */
                          &exit_status,
                          NULL /* error */);

    if (status && exit_status == 0) {
        int addr_num;
        char **lines = c_strsplit(addr_out, "\n", 0);
        char **line;

        for (line = lines, addr_num = 0; line[0] && line[1];
             line += 2, addr_num++) {
            char *result = c_strdup_printf("%s (%s)", line[1], line[0]);
            c_hash_table_insert(symbol_table, addresses[addr_num], result);
        }

        c_strfreev(lines);
    }

    for (i = 0; i < n_addresses; i++)
        c_free(argv[i + address_args]);

    return status;
}

static bool
resolve_symbols_via_gnu_backtrace_symbols(void **addresses,
                                          int n_addresses)
{
    char **symbols;
    int i;

    symbols = backtrace_symbols(addresses, n_addresses);

    for (i = 0; i < n_addresses; i++)
        c_hash_table_insert(symbol_table,
                            addresses[i], c_strdup(symbols[i]));

    free(symbols);

    return true;
}

static void
backtrace_get_frame_symbols(void **addresses,
                            char **frames,
                            int n_frames)
{
    void *missing[n_frames];
    int n_missing = 0;
    int i;

    c_mutex_lock(&symbol_table_lock);

    for (i = 0; i < n_frames; i++) {
        frames[i] = c_hash_table_lookup(symbol_table, addresses[i]);
        if (!frames[i])
            missing[n_missing++] = addresses[i];
    }

    if (n_missing) {

        if (!resolve_symbols_via_addr2line(missing, n_missing))
            resolve_symbols_via_gnu_backtrace_symbols(missing, n_missing);

        for (i = 0; i < n_frames; i++) {
            if (!frames[i])
                frames[i] = c_hash_table_lookup(symbol_table, addresses[i]);
            if (!frames[i])
                frames[i] = "unknown";
        }
    }

    c_mutex_unlock(&symbol_table_lock);
}

void
c_backtrace_symbols(void **addresses,
                    char **frames,
                    int n_frames)
{
    backtrace_get_frame_symbols(addresses, frames, n_frames);
}

c_backtrace_t *
c_backtrace_new(void)
{
    int n_frames = 0;
    void **addresses = c_backtrace(&n_frames);
    c_backtrace_t *bt = c_malloc(sizeof(*bt) + sizeof(void *) * n_frames);

    memcpy(bt->addresses, addresses, sizeof(void *) * n_frames);
    bt->n_frames = n_frames;

    return bt;
}

void
c_backtrace_free(c_backtrace_t *backtrace)
{
    c_free(backtrace);
}

int
c_backtrace_get_n_frames(c_backtrace_t *bt)
{
    return bt->n_frames;
}

void
c_backtrace_get_frame_symbols(c_backtrace_t *bt,
                              char **frames,
                              int n_frames)
{
    backtrace_get_frame_symbols(bt->addresses, frames, n_frames);
}

void
c_backtrace_log(c_backtrace_t *bt,
                c_log_context_t *lctx,
                const char *log_domain,
                c_log_level_flags_t log_level)
{
    char *symbols[bt->n_frames];
    int i;

    c_backtrace_get_frame_symbols(bt, symbols, bt->n_frames);

    for (i = 0; i < bt->n_frames; i++)
        c_print("%s\n", symbols[i]);
}

void
c_backtrace_log_error(c_backtrace_t *bt)
{
    c_backtrace_log(bt, NULL, C_LOG_DOMAIN, C_LOG_LEVEL_ERROR);
}

c_backtrace_t *
c_backtrace_copy(c_backtrace_t *bt)
{
    c_backtrace_t *copy = c_malloc(sizeof (*bt) + sizeof(void *) * bt->n_frames);

    copy->n_frames = bt->n_frames;
    memcpy(copy->addresses, bt->addresses, sizeof(void *) * bt->n_frames);

    return copy;
}
