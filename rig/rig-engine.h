/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef _RIG_ENGINE_H_
#define _RIG_ENGINE_H_

#ifdef USE_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-client/lookup.h>
#endif

#include <cmodule.h>

#include <rut.h>

/* Forward declare since there is a circular dependency between this
 * header and rig-camera-view.h which depends on this typedef... */
typedef enum _rig_tool_id_t {
    RIG_TOOL_ID_SELECTION = 1,
    RIG_TOOL_ID_ROTATION,
} rig_tool_id_t;

typedef enum _rut_select_action_t {
    /* replaces the current selection */
    RUT_SELECT_ACTION_REPLACE,
    /* toggles whether the given item is selected or not */
    RUT_SELECT_ACTION_TOGGLE,
} rut_select_action_t;

#include "rig-types.h"

#include "rig-protobuf-c-rpc.h"
#include "rig-rpc-network.h"
#include "rig-controller.h"
#include "rig-frontend.h"
#include "rig-simulator.h"
#include "rig-code.h"
#include "rig-text-engine.h"

#include "components/rig-source.h"

enum {
    RIG_ENGINE_PROP_DEVICE_WIDTH,
    RIG_ENGINE_PROP_DEVICE_HEIGHT,
    RIG_ENGINE_N_PROPS
};

extern rut_type_t rig_engine_type;

struct _rig_engine_t {
    rut_object_base_t _base;

    bool headless;

    c_matrix_t identity;

    rut_shell_t *shell;
    rig_property_context_t *property_ctx;

    rig_pb_serializer_t *ops_serializer;
    rut_memory_stack_t *frame_stack;
    rut_magazine_t *object_id_magazine;

    void (*garbage_collect_callback)(rut_object_t *object,
                                     void *user_data);
    void *garbage_collect_data;

    /* TODO: Move to rig_frontend_t */
    rut_memory_stack_t *sim_frame_stack;

    rut_input_queue_t *simulator_input_queue;

    /* TODO: Move to rig_simulator_t */
    rig_code_node_t *code_graph;
    c_module_t *code_dso_module;

    rig_text_engine_state_t *text_state;

    float grab_x;
    float grab_y;
    float entity_grab_pos[3];
    rut_input_callback_t key_focus_callback;
    float grab_progress;

    /* XXX: The object/type composition we use here isn't very
     * clean...
     *
     * TODO: The frontend and simulator state should be accessed as
     * traits of the engine.
     */
    rig_frontend_t *frontend; /* NULL if engine not acting as a frontend process */
    rig_simulator_t *simulator; /* NULL if engine not acting as a simulator */

    rig_ui_t *ui;

    rut_queue_t *queued_deletes;

    /* Note: this is only referenced by the rig_engine_op_XYZ operation
     * functions, and when calling apis like rig_engine_pb_op_apply()
     * that take an explicit apply_op_ctx argument then this isn't
     * referenced. */
    rig_engine_op_apply_context_t *apply_op_ctx;
    void (*log_op_callback)(Rig__Operation *pb_op, void *user_data);
    void *log_op_data;

    /* TODO: move into editor */
    c_string_t *codegen_string0;
    c_string_t *codegen_string1;
    int next_code_id;

    c_string_t *code_string;
    char *code_dso_filename;
    bool need_recompile;

    c_sllist_t *timelines;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RIG_ENGINE_N_PROPS];
};

rig_engine_t *rig_engine_new_for_frontend(rut_shell_t *shell,
                                          rig_frontend_t *frontend);

rig_engine_t *rig_engine_new_for_simulator(rut_shell_t *shell,
                                           rig_simulator_t *simulator);

void rig_engine_set_ui(rig_engine_t *engine, rig_ui_t *ui);

/* FIXME: find a better place to put these prototypes */
void rig_register_asset(rig_engine_t *engine, rig_asset_t *asset);
rig_asset_t *rig_lookup_asset(rig_engine_t *engine, const char *path);

void rig_engine_set_log_op_callback(rig_engine_t *engine,
                                    void (*callback)(Rig__Operation *pb_op,
                                                     void *user_data),
                                    void *user_data);

void rig_engine_queue_delete(rig_engine_t *engine, rut_object_t *object);

void rig_engine_garbage_collect(rig_engine_t *engine);

char *rig_engine_get_object_debug_name(rut_object_t *object);

void rig_engine_set_apply_op_context(rig_engine_t *engine,
                                     rig_engine_op_apply_context_t *ctx);

void rig_engine_progress_timelines(rig_engine_t *engine, double delta);
/* Determines whether any timelines are running */
bool rig_engine_check_timelines(rig_engine_t *engine);

#endif /* _RIG_ENGINE_H_ */
