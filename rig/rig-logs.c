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

#include "rig-logs.h"

struct log_state
{
    void (*log_notify)(struct rig_log *log);

    /* We maintain two parallel logs so we can maintain
     * the frontend ans simulator logs separately.
     */
    struct rig_log logs[2];
    c_mutex_t log_lock;

    /* So we can start logging from a shell before it's known
     * whether the shell corresponds to a frontend or simulator
     * when we're logging we just look for a log that matches
     * a given rut_shell_t pointer and only associate them
     * with a frontend/simulator later when
     * rig_log_set_frontend/simulator are called.
     */
    rig_frontend_t *frontend;
    rig_simulator_t *simulator;
};

static struct log_state log_state;

#define MAX_LOG_LEN 10000


static struct rig_log *
lookup_log(bool remote, rut_shell_t *shell)
{
    struct log_state *state = &log_state;

    if (remote)
        return &state->logs[1];

    if (state->simulator && state->simulator->shell == shell)
        return &state->logs[1];

    return &state->logs[0];
}

/* XXX: 'remote' here really just means out-of-thread (whereby we
 * won't have a shell pointer). The log might have come from a
 * simulator running in another process or it could actually be
 * remote. */
static void
log_full(rut_shell_t *shell,
         bool remote,
         const char *log_domain,
         c_log_level_flags_t log_level,
         const char *message)
{
    struct rig_log_entry *entry = c_slice_new(struct rig_log_entry);
    struct rig_log *log;
    struct log_state *state = &log_state;

    entry->log_domain = c_quark_from_string(log_domain ? log_domain : "");
    entry->log_level = log_level;
    entry->message = c_strdup(message);

    c_mutex_lock(&state->log_lock);

    log = lookup_log(remote, shell);

    rut_list_insert(&log->log, &entry->link);

    if (log->len < MAX_LOG_LEN)
        log->len++;
    else
        rut_list_remove(log->log.prev);

    if (state->log_notify)
        state->log_notify(log);

    c_mutex_unlock(&state->log_lock);
}

static void
log_hook(c_log_context_t *lctx,
         const char *log_domain,
         c_log_level_flags_t log_level,
         const char *message)
{
    log_full(rut_get_thread_current_shell(),
             false, /* remote */
             log_domain,
             log_level,
             message);
}

void
rig_logs_log_from_remote(c_log_level_flags_t log_level,
                         const char *message)
{
    log_full(NULL, /* shell */
             true, /* remote */
             NULL, /* domain */
             log_level,
             message);
}

void
rig_logs_init(void (*notify)(struct rig_log *log))
{
    c_mutex_init(&log_state.log_lock);

    log_state.log_notify = notify;

    rut_list_init(&log_state.logs[0].log);
    rut_list_init(&log_state.logs[1].log);

    c_log_hook = log_hook;
}

void
rig_logs_fini(void)
{
    struct rig_log_entry *entry;
    struct rig_log *frontend_log;
    struct rig_log *simulator_log;

    c_log_hook = NULL;

    rig_logs_resolve(&frontend_log, &simulator_log);

    fprintf(stderr, "Final logs...\n");
    rut_list_for_each(entry, &frontend_log->log, link)
        fprintf(stderr, "FE: %s\n", entry->message);
    rut_list_for_each(entry, &simulator_log->log, link)
        fprintf(stderr, "SIM: %s\n", entry->message);
}

void
rig_logs_resolve(struct rig_log **frontend_log,
                 struct rig_log **simulator_log)
{
    struct log_state *state = &log_state;

    *frontend_log = NULL;
    *simulator_log = NULL;

    if (state->frontend)
        *frontend_log = &state->logs[0];

    if (state->simulator)
        *simulator_log = &state->logs[1];
}

void
rig_logs_set_frontend(rig_frontend_t *frontend)
{
    log_state.frontend = frontend;
    log_state.logs[0].shell = frontend->engine->shell;
}

void
rig_logs_set_simulator(rig_simulator_t *simulator)
{
    log_state.simulator = simulator;
    log_state.logs[1].shell = simulator->shell;
}
