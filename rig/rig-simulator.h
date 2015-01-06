/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef _RIG_SIMULATOR_H_
#define _RIG_SIMULATOR_H_

#include <rut.h>
typedef struct _rig_simulator_t rig_simulator_t;

#include "rig-engine.h"
#include "rig-engine-op.h"
#include "rig-pb.h"
#include "rig-js.h"

/*
 * Simulator actions are sent back as requests to the frontend at the
 * end of a frame.
 */
typedef enum _rig_simulator_action_type_t {
    RIG_SIMULATOR_ACTION_TYPE_REPORT_EDIT_FAILURE = 1,
} rig_simulator_action_type_t;

/* The "simulator" is the process responsible for updating object
 * properties either in response to user input, the progression of
 * animations or running other forms of simulation such as physics.
 */
struct _rig_simulator_t {
    rut_object_base_t _base;

    rig_frontend_id_t frontend_id;

    bool redraw_queued;

    /* when running as an editor or slave device then the UI
     * can be edited at runtime and we handle some things a
     * bit differently. For example we only need to be able
     * to map ids to objects to support editing operations.
     */
    bool editable;

    rut_shell_t *shell;
    rig_engine_t *engine;

    rig_pb_stream_t *stream;
    rig_rpc_peer_t *simulator_peer;

    rig_pb_serializer_t *log_serializer;
    rut_memory_stack_t *log_serializer_stack;

    float view_x;
    float view_y;

    float last_pointer_x;
    float last_pointer_y;

    rut_button_state_t button_state;

    rig_pb_un_serializer_t *ui_unserializer;
    rig_pb_un_serializer_t *ops_unserializer;
    rig_engine_op_apply_context_t apply_op_ctx;
    rig_engine_op_map_context_t map_to_sim_objects_op_ctx;
    rig_engine_op_map_context_t map_to_frontend_ids_op_ctx;

    c_hash_table_t *object_to_id_map;
    c_hash_table_t *id_to_object_map;
    // c_hash_table_t *object_to_tmp_id_map;
    uint64_t next_tmp_id;

    c_list_t actions;
    int n_actions;

    rut_queue_t *ops;

    rig_js_runtime_t *js;
};

extern rut_type_t rig_simulator_type;

rig_simulator_t *rig_simulator_new(rig_frontend_id_t frontend_id,
                                   rut_shell_t *main_shell);

void rig_simulator_set_frontend_fd(rig_simulator_t *simulator, int fd);

void rig_simulator_run(rig_simulator_t *simulator);

void rig_simulator_run_frame(rut_shell_t *shell, void *user_data);

void rig_simulator_queue_redraw_hook(rut_shell_t *shell, void *user_data);

void rig_simulator_print_mappings(rig_simulator_t *simulator);

void rig_simulator_forward_log(rig_simulator_t *simulator);

void rig_simulator_parse_option(const char *option, void (*usage)(void));

#endif /* _RIG_SIMULATOR_H_ */
