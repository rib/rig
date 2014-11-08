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
#include "rig-simulator.h"

/* TODO: To avoid needing to assume we just have two logs
 * for the 'frontend' and 'simulator' it might be nicer to
 * slightly generalise the log helpers and move them into rut/
 *
 * Currently it's a bit clunky deciding where we direct log
 * entries. We default to log[0] if unsure and always direct
 * to log[1] for the simulator. In the simulator though it
 * would probably be better to just log everything into log[0].
 *
 * It would probably make sense for us to maintain separate
 * frontend + simulator logs for all slave devices which wouldn't
 * be possible with the current approach.
 *
 * Each rut_shell_t could have an embedded log
 *
 * We could have a rut_log_push()/pop() api that pushes a log
 * to a thread-local stack which would be used in rut-poll.c
 * where we currently call rut_set_thread_current_shell().
 *
 * Pushing and poping a log would ensure a log is linked into
 * a global list of logs.
 *
 * It would be quite straight forward for the ncurses debug
 * api to have a mechanism for being told which logs correspond
 * to the frontend/simulator.
 *
 */
struct log_state
{
    void (*log_notify)(struct rig_log *log);

    /* We maintain two parallel logs so we can maintain
     * the frontend and simulator logs separately.
     */
    struct rig_log logs[2];
    c_mutex_t log_lock;

    rig_frontend_t *frontend;
    rig_simulator_t *simulator;
    rut_closure_t *simulator_log_idle;
};

static struct log_state log_state;

#define MAX_LOG_LEN 10000


void
rig_logs_lock(void)
{
    c_mutex_lock(&log_state.log_lock);
}

void
rig_logs_unlock(void)
{
    c_mutex_unlock(&log_state.log_lock);
}

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

void
rig_logs_entry_free(struct rig_log_entry *entry)
{
    c_free(entry->message);
    c_slice_free(struct rig_log_entry, entry);
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

    rig_logs_lock();

    log = lookup_log(remote, shell);

    rut_list_insert(&log->entries, &entry->link);

    if (log->len < MAX_LOG_LEN)
        log->len++;
    else {
        entry = rut_container_of(log->entries.prev, entry, link);
        rut_list_remove(&entry->link);
        rig_logs_entry_free(entry);
    }

    if (state->log_notify)
        state->log_notify(log);

    rig_logs_unlock();
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

    rut_list_init(&log_state.logs[0].entries);
    rut_list_init(&log_state.logs[1].entries);

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
    if (frontend_log)
        rut_list_for_each(entry, &frontend_log->entries, link)
            fprintf(stderr, "FE: %s\n", entry->message);

    if (simulator_log)
        rut_list_for_each(entry, &simulator_log->entries, link)
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

    if (state->logs[0].len)
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

static void
forward_simulator_logs_idle_cb(void *user_data)
{
    struct log_state *state = &log_state;

    rut_poll_shell_remove_idle(state->simulator->shell,
                               state->simulator_log_idle);
    state->simulator_log_idle = NULL;

    rig_simulator_forward_log(user_data);
}

static void
simulator_log_notify_cb(struct rig_log *log)
{
    struct log_state *state = &log_state;

    if (state->simulator_log_idle == NULL &&
        state->simulator)
    {
        state->simulator_log_idle =
            rut_poll_shell_add_idle(state->simulator->shell,
                                    forward_simulator_logs_idle_cb,
                                    NULL, /* user_data */
                                    NULL /* destroy */);
    }
}

void
rig_simulator_logs_init(void)
{
    rig_logs_init(simulator_log_notify_cb);
}
