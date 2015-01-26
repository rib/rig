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

#ifndef _RIG_LOGS_H_
#define _RIG_LOGS_H_

#include <rut.h>

#include "rig-frontend.h"
#include "rig-simulator.h"

struct rig_log_entry
{
    c_list_t link;

    uint64_t timestamp;
    c_quark_t log_domain;
    c_log_level_flags_t log_level;
    char *message;
};

struct rig_log
{
    const char *title;

    rut_shell_t *shell;

    c_list_t entries;
    int len;
};

void rig_logs_init(void (*log_notify)(struct rig_log *log));

void rig_logs_set_frontend(rig_frontend_t *frontend);
void rig_logs_set_simulator(rig_simulator_t *simulator);

struct rig_log *rig_logs_get_frontend_log(void);
struct rig_log *rig_logs_get_simulator_log(void);

void rig_logs_pb_log(Rig__Log__LogType pb_type,
                     Rig__LogEntry *pb_entry);

void rig_logs_entry_free(struct rig_log_entry *entry);

void rig_logs_lock(void);
void rig_logs_unlock(void);

struct rig_log *rig_logs_copy_log(struct rig_log *log);
void rig_logs_free_copy(struct rig_log *copy);

void rig_logs_clear_log(struct rig_log *log);

void rig_logs_fini(void);

void rig_simulator_logs_init(void);

#endif /* _RIG_LOGS_H_ */

