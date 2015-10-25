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
#define MAX_LOGS 2 /* frontend + simulator */
struct log_state
{
    void (*log_notify)(struct rig_log *log);

    c_mutex_t log_lock;

    struct rig_log logs[MAX_LOGS];
    int n_logs;

    rig_frontend_t *frontend;
    struct rig_log *frontend_log;

    rig_simulator_t *simulator;
    struct rig_log *simulator_log;
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

void
rig_logs_entry_free(struct rig_log_entry *entry)
{
    c_free(entry->message);
    c_slice_free(struct rig_log_entry, entry);
}

enum rig_log_type {
    RIG_LOG_TYPE_UNKNOWN,
    RIG_LOG_TYPE_FRONTEND,
    RIG_LOG_TYPE_SIMULATOR
};

static void
log_full(enum rig_log_type type,
         uint64_t timestamp,
         const char *log_domain,
         c_log_level_flags_t log_level,
         const char *message)
{
    struct rig_log_entry *entry = c_slice_new(struct rig_log_entry);
    struct rig_log *log;
    struct log_state *state = &log_state;

    entry->timestamp = timestamp;
    entry->log_domain = c_quark_from_string(log_domain ? log_domain : "");
    entry->log_level = log_level;
    entry->message = c_strdup(message);

    rig_logs_lock();

    if (type == RIG_LOG_TYPE_FRONTEND)
        log = state->frontend_log;
    else if (type == RIG_LOG_TYPE_SIMULATOR)
        log = state->simulator_log;
    else
        log = &state->logs[0];

    c_list_insert(&log->entries, &entry->link);

    if (log->len < MAX_LOG_LEN)
        log->len++;
    else {
        entry = rut_container_of(log->entries.prev, entry, link);
        c_list_remove(&entry->link);
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
    struct log_state *state = &log_state;
    rut_shell_t *shell = rut_get_thread_current_shell();
    enum rig_log_type type = RIG_LOG_TYPE_UNKNOWN;
    int64_t timestamp;

    if (state->frontend && state->frontend->engine->shell == shell)
        type = RIG_LOG_TYPE_FRONTEND;
    else if (state->simulator && state->simulator->shell == shell)
        type = RIG_LOG_TYPE_SIMULATOR;

    timestamp = c_get_monotonic_time();

    log_full(type,
             timestamp,
             log_domain,
             log_level,
             message);
}

void
rig_logs_pb_log(Rig__Log__LogType pb_type,
                Rig__LogEntry *pb_entry)
{
    enum rig_log_type type = RIG_LOG_TYPE_UNKNOWN;

    if (pb_type == RIG__LOG__LOG_TYPE__FRONTEND) {
        type = RIG_LOG_TYPE_FRONTEND;
        if (!log_state.frontend_log)
            log_state.frontend_log = &log_state.logs[log_state.n_logs++];
    } else if (pb_type == RIG__LOG__LOG_TYPE__SIMULATOR) {
        type = RIG_LOG_TYPE_SIMULATOR;
        if (!log_state.simulator_log)
            log_state.simulator_log = &log_state.logs[log_state.n_logs++];
    }

    log_full(type,
             pb_entry->timestamp,
             NULL, /* domain */
             pb_entry->log_level,
             pb_entry->log_message);
}

void
rig_logs_init(void (*notify)(struct rig_log *log))
{
    int i;

    c_mutex_init(&log_state.log_lock);

    log_state.log_notify = notify;

    for (i = 0; i < MAX_LOGS; i++)
        c_list_init(&log_state.logs[i].entries);

    c_log_hook = log_hook;
}

void
rig_logs_clear_log(struct rig_log *log)
{
    struct rig_log_entry *entry;
    struct rig_log_entry *tmp;

    c_list_for_each_safe(entry, tmp, &log->entries, link) {
        c_list_remove(&entry->link);
        rig_logs_entry_free(entry);
    }
    log->len = 0;
}

void
rig_logs_free_copy(struct rig_log *copy)
{
    rig_logs_clear_log(copy);
    c_slice_free(struct rig_log, copy);
}

struct rig_log *
rig_logs_copy_log(struct rig_log *log)
{
    struct rig_log *copy = c_slice_dup(struct rig_log, log);
    struct rig_log_entry *entry;

    c_list_init(&copy->entries);

    c_list_for_each(entry, &log->entries, link) {
        struct rig_log_entry *entry_copy =
            c_slice_dup(struct rig_log_entry, entry);

        entry_copy->message = c_strdup(entry->message);
        c_list_insert(copy->entries.prev, &entry_copy->link);
    }

    return copy;
}

static void
dump_and_clear_log(const char *prefix, struct rig_log *log)
{
    struct rig_log_entry *entry;
    struct rig_log_entry *tmp;

    c_list_for_each_safe(entry, tmp, &log->entries, link)
        fprintf(stderr, "%s%s\n", prefix, entry->message);

    rig_logs_clear_log(log);
}

void
rig_logs_fini(void)
{
    struct rig_log *frontend_log = rig_logs_get_frontend_log();
    struct rig_log *simulator_log = rig_logs_get_simulator_log();

    c_log_hook = NULL;

    if (frontend_log == NULL && simulator_log == NULL) {
        if (log_state.logs[0].len) {
            fprintf(stderr, "Final logs...\n");
            dump_and_clear_log("", &log_state.logs[0]);
        }
        return;
    }

    fprintf(stderr, "Final logs...\n");

    if (frontend_log)
        dump_and_clear_log("FE: ", frontend_log);

    if (simulator_log)
        dump_and_clear_log("FE: ", simulator_log);
}

struct rig_log *
rig_logs_get_frontend_log(void)
{
    struct log_state *state = &log_state;

    return state->frontend_log;
}

struct rig_log *
rig_logs_get_simulator_log(void)
{
    struct log_state *state = &log_state;

    return state->simulator_log;
}

void
rig_logs_set_frontend(rig_frontend_t *frontend)
{
    struct log_state *state = &log_state;

    state->frontend = frontend;

    if (!state->frontend_log) {
        state->frontend_log = &state->logs[state->n_logs++];
        state->frontend_log->shell = frontend->engine->shell;
        state->frontend_log->title = "[Frontend Log]";
    }
}

void
rig_logs_set_simulator(rig_simulator_t *simulator)
{
    struct log_state *state = &log_state;

    state->simulator = simulator;

    if (!state->simulator_log) {
        state->simulator_log = &state->logs[state->n_logs++];
        state->simulator_log->shell = simulator->shell;
        state->simulator_log->title = "[Simulator Log]";
    }
}

static void
forward_simulator_logs_idle_cb(void *user_data)
{
    struct log_state *state = &log_state;

    rut_poll_shell_remove_idle_FIXME(state->simulator->shell,
                               state->simulator_log_idle);
    state->simulator_log_idle = NULL;

    rig_simulator_forward_log(state->simulator);
}

static void
simulator_log_notify_cb(struct rig_log *log)
{
    struct log_state *state = &log_state;

    if (state->simulator &&
        state->simulator_log_idle == NULL)
    {
        state->simulator_log_idle =
            rut_poll_shell_add_idle_FIXME(state->simulator->shell,
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
