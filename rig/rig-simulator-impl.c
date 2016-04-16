/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <clib.h>

#include <rut.h>
#include <rut-headless-shell.h>

#include "rig-engine.h"
#include "rig-engine-op.h"
#include "rig-pb.h"
#include "rig-ui.h"
#include "rig-logs.h"
#include "rig-load-save.h"
#include "rig-frontend.h"
#include "rig-code-module.h"
#include "rig-js.h"

#include "rig.pb-c.h"

typedef struct _rig_simulator_action_t {
    rig_simulator_action_type_t type;
    c_list_t list_node;
    union {
        struct {
            rut_object_t *object;
            rut_select_action_t action;
        } select_object;
    };
} rig_simulator_action_t;

static void
simulator__test(Rig__Simulator_Service *service,
                const Rig__Query *query,
                Rig__TestResult_Closure closure,
                void *closure_data)
{
    Rig__TestResult result = RIG__TEST_RESULT__INIT;
    // rig_simulator_t *simulator = rig_pb_rpc_closure_get_connection_data
    // (closure_data);

    c_return_if_fail(query != NULL);

    c_debug("Simulator Service: Test Query\n");

    closure(&result, closure_data);
}

static void
clear_actions(rig_simulator_t *simulator)
{
    rig_simulator_action_t *action, *tmp;

    c_list_for_each_safe(action, tmp, &simulator->actions, list_node)
    {
        switch (action->type) {
        case RIG_SIMULATOR_ACTION_TYPE_REPORT_EDIT_FAILURE:
            break;
        }

        c_list_remove(&action->list_node);
        c_slice_free(rig_simulator_action_t, action);
    }

    simulator->n_actions = 0;
}

static rut_object_t *
simulator_lookup_object(rig_simulator_t *simulator, uint64_t id)
{
    return (void *)(intptr_t)id;
}

static void *
simulator_lookup_object_cb(rig_ui_t *ui, uint64_t id, void *user_data)
{
    rig_simulator_t *simulator = user_data;

    return simulator_lookup_object(simulator, id);
}

static uint64_t
simulator_lookup_object_id(rig_simulator_t *simulator, void *object)
{
    if (c_hash_table_lookup(simulator->object_registry, object))
        return (intptr_t)object;
    else
        return 0;
}

static uint64_t
lookup_frontend_id_cb(void *object, void *user_data)
{
    rig_simulator_t *simulator = user_data;
    return simulator_lookup_object_id(simulator, object);
}

static uint64_t
simulator_map_id_to_sim_object_cb(uint64_t id, void *user_data)
{
    rig_simulator_t *simulator = user_data;
    void *object = simulator_lookup_object(simulator, id);
    return (uint64_t)(uintptr_t)object;
}

static uint64_t
simulator_map_object_to_frontend_id_cb(uint64_t id, void *user_data)
{
    rig_simulator_t *simulator = user_data;
    void *object = (void *)(uintptr_t)id;
    return simulator_lookup_object_id(simulator, object);
}

static void
simulator_register_object_with_id_cb(rig_ui_t *ui,
                                     void *object,
                                     uint64_t id,
                                     void *user_data)
{
    c_return_if_fail(id != 0);

    /* We don't expect temp IDs since they are registered via
     * temporarily_register_object_cb() instead */
    c_return_if_fail((id & 0x1) == 0);

    /* XXX: currently we don't have any use case where we'd expect a
     * simulator to be told to register an object with an arbitrary ID
     */
    c_return_if_reached();
}

static uint64_t
simulator_register_object_cb(void *object, void *user_data)
{
    rig_simulator_t *simulator = user_data;

#ifdef RIG_ENABLE_DEBUG
    if (c_hash_table_lookup(simulator->object_registry, object))
        c_warning("Simulator: duplicate object registration!");
#endif

    c_hash_table_insert(simulator->object_registry, object, object);

    return (intptr_t)object;
}

static void
simulator_garbage_collect_object_cb(void *object, void *user_data)
{
    rig_simulator_t *simulator = user_data;

    c_hash_table_remove(simulator->object_registry, object);
}

#if 0
static void *
simulator_unregister_id(rig_simulator_t *simulator, uint64_t id)
{
    /* We don't expect temp IDs since they are registered via
     * temporarily_register_object_cb() instead */
    c_return_val_if_fail((id & 0x1) == 0, NULL);

    return (void *)(intptr_t)id;
}
#endif

static uint64_t
direct_object_id_cb(void *object, void *user_data)
{
    return (uint64_t)(uintptr_t)object;
}

static void
simulator__run_frame(Rig__Simulator_Service *service,
                     const Rig__FrameSetup *setup,
                     Rig__RunFrameAck_Closure closure,
                     void *closure_data)
{
    Rig__RunFrameAck ack = RIG__RUN_FRAME_ACK__INIT;
    rig_simulator_t *simulator =
        rig_pb_rpc_closure_get_connection_data(closure_data);
    rig_engine_t *engine = simulator->engine;
    int i;

    c_return_if_fail(setup != NULL);

    c_return_if_fail(setup->has_progress);

    simulator->frame_info.progress = setup->progress;

    for (i = 0; i < setup->n_view_updates; i++) {
        Rig__ViewUpdate *pb_update = setup->view_updates[i];
        rig_view_t *view = simulator_lookup_object(simulator, pb_update->id);

        if (!view) {
            c_warning("Spurious update for unknown view");
            continue;
        }

        rig_view_set_width(view, pb_update->width);
        rig_view_set_height(view, pb_update->height);
    }

    // c_debug ("Simulator: Run Frame Request: n_events = %d\n",
    //         setup->n_events);

    for (i = 0; i < setup->n_events; i++) {
        Rig__Event *pb_event = setup->events[i];
        rut_stream_event_t *event;

        if (!pb_event->has_type) {
            c_warning("Event missing type");
            continue;
        }

        event = c_slice_new0(rut_stream_event_t);

        if (pb_event->has_camera_id) {
            event->camera_entity = simulator_lookup_object(simulator,
                                                           pb_event->camera_id);
        }

        switch (pb_event->type) {
        case RIG__EVENT__TYPE__POINTER_MOVE:
            event->pointer_move.state = simulator->button_state;
            break;

        case RIG__EVENT__TYPE__POINTER_DOWN:
        case RIG__EVENT__TYPE__POINTER_UP:

            event->pointer_button.state = simulator->button_state;

            event->pointer_button.x = simulator->last_pointer_x;
            event->pointer_button.y = simulator->last_pointer_y;

            if (pb_event->pointer_button->has_button)
                event->pointer_button.button = pb_event->pointer_button->button;
            else {
                c_warn_if_reached();
                event->pointer_button.button = RUT_BUTTON_STATE_1;
            }
            break;

        case RIG__EVENT__TYPE__KEY_DOWN:
        case RIG__EVENT__TYPE__KEY_UP:

            if (pb_event->key->has_keysym)
                event->key.keysym = pb_event->key->keysym;
            else {
                c_warn_if_reached();
                event->key.keysym = RUT_KEY_a;
            }

            if (pb_event->key->has_mod_state)
                event->key.mod_state = pb_event->key->mod_state;
            else {
                c_warn_if_reached();
                event->key.mod_state = 0;
            }
            break;
        }

        switch (pb_event->type) {
        case RIG__EVENT__TYPE__POINTER_MOVE:
            event->type = RUT_STREAM_EVENT_POINTER_MOVE;

            if (pb_event->pointer_move->has_x) {
                /* Note: we can translate all simulator events to
                 * account for the position of a rig_camera_view_t in
                 * an editor. */
                event->pointer_move.x =
                    pb_event->pointer_move->x - simulator->view_x;
            } else {
                c_warn_if_reached();
                event->pointer_move.x = 0;
            }

            if (pb_event->pointer_move->has_y) {
                event->pointer_move.y =
                    pb_event->pointer_move->y - simulator->view_y;
            } else {
                c_warn_if_reached();
                event->pointer_move.y = 0;
            }

            simulator->last_pointer_x = event->pointer_move.x;
            simulator->last_pointer_y = event->pointer_move.y;

            // c_debug ("Simulator: Read Event: Pointer move (%f, %f)\n",
            //         event->pointer_move.x, event->pointer_move.y);
            break;
        case RIG__EVENT__TYPE__POINTER_DOWN:
            event->type = RUT_STREAM_EVENT_POINTER_DOWN;
            simulator->button_state |= event->pointer_button.button;
            event->pointer_button.state |= event->pointer_button.button;
            // c_debug ("Simulator: Read Event: Pointer down\n");
            break;
        case RIG__EVENT__TYPE__POINTER_UP:
            event->type = RUT_STREAM_EVENT_POINTER_UP;
            simulator->button_state &= ~event->pointer_button.button;
            event->pointer_button.state &= ~event->pointer_button.button;
            // c_debug ("Simulator: Read Event: Pointer up\n");
            break;
        case RIG__EVENT__TYPE__KEY_DOWN:
            event->type = RUT_STREAM_EVENT_KEY_DOWN;
            // c_debug ("Simulator: Read Event: Key down\n");
            break;
        case RIG__EVENT__TYPE__KEY_UP:
            event->type = RUT_STREAM_EVENT_KEY_UP;
            // c_debug ("Simulator: Read Event: Key up\n");
            break;
        }

        rut_headless_shell_handle_stream_event(engine->shell, event);
    }

    rut_shell_queue_redraw_real(engine->shell);

    closure(&ack, closure_data);
}

static Rig__Simulator_Service rig_simulator_service =
    RIG__SIMULATOR__INIT(simulator__);

static void
handle_frontend_test_response(const Rig__TestResult *result,
                              void *closure_data)
{
    // c_debug ("Renderer test response received\n");
}

static void
simulator_peer_connected(rig_pb_rpc_client_t *pb_client,
                         void *user_data)
{
    rig_simulator_t *simulator = user_data;
    ProtobufCService *frontend_service =
        rig_pb_rpc_client_get_service(pb_client);
    Rig__Query query = RIG__QUERY__INIT;

    rig__frontend__test(
        frontend_service, &query, handle_frontend_test_response, NULL);

    simulator->connected = true;

    rut_closure_list_invoke(&simulator->connected_closures,
                            rig_simulator_connected_func_t,
                            simulator);

    c_debug("Simulator peer connected\n");
}

static void
simulator_stop_service(rig_simulator_t *simulator)
{
    rut_object_unref(simulator->simulator_peer);
    simulator->simulator_peer = NULL;

    rut_shell_quit(simulator->shell);
}

static void
simulator_peer_error_handler(rig_pb_rpc_error_code_t code,
                             const char *message,
                             void *user_data)
{
    rig_simulator_t *simulator = user_data;

    c_warning("Simulator peer error: %s", message);

    simulator_stop_service(simulator);
}

static void
simulator_start_service(rut_shell_t *shell,
                        rig_simulator_t *simulator)
{
    simulator->simulator_peer = rig_rpc_peer_new(
        simulator->stream,
        &rig_simulator_service.base,
        (ProtobufCServiceDescriptor *)&rig__frontend__descriptor,
        simulator_peer_error_handler,
        simulator_peer_connected,
        simulator);
}

static void
log_op_cb(Rig__Operation *pb_op, void *user_data)
{
    rig_simulator_t *simulator = user_data;
    rig_property_context_t *prop_ctx = &simulator->engine->_property_ctx;

    /* We sequence all operations relative to the property updates that
     * are being logged, so that the frontend will be able to replay
     * operation and property updates in the same order.
     */
    pb_op->has_sequence = true;
    pb_op->sequence = prop_ctx->log_len;

#ifdef RIG_ENABLE_DEBUG
    {
        int n_frames = 0;
        void **addresses = c_backtrace(&n_frames);
        char *symbols[n_frames];
        rig_engine_t *engine = simulator->engine;
        int i;

        c_backtrace_symbols(addresses, symbols, n_frames);

        pb_op->n_backtrace_frames = n_frames;
        pb_op->backtrace_frames =
            rut_memory_stack_memalign(engine->ops_serializer->stack,
                                      sizeof(void *) * n_frames,
                                      C_ALIGNOF(void *));
        for (i = 0; i < n_frames; i++)
            pb_op->backtrace_frames[i] = symbols[i];
    }
#endif

    rut_queue_push_tail(simulator->ops, pb_op);
}

static void
_rig_simulator_free(void *object)
{
    rig_simulator_t *simulator = object;

#ifdef USE_MOZJS
    rut_object_unref(simulator->js);
#endif

    clear_actions(simulator);

    c_hash_table_destroy(simulator->object_registry);

    rig_engine_op_apply_context_destroy(&simulator->apply_op_ctx);

    rut_object_unref(simulator->engine);

    if (simulator->simulator_peer)
        rut_object_unref(simulator->simulator_peer);
    rut_object_unref(simulator->stream);

    rut_object_unref(simulator->shell);

    if (simulator->log_serializer) {
        rig_pb_serializer_destroy(simulator->log_serializer);
        rut_memory_stack_free(simulator->log_serializer_stack);
    }

    rut_closure_list_remove_all(&simulator->connected_closures);

    rut_object_free(rig_simulator_t, simulator);
}

rut_type_t rig_simulator_type;

static void
_rig_simulator_init_type(void)
{
    rut_type_init(&rig_simulator_type, "rig_simulator_t", _rig_simulator_free);
}

static rut_input_event_status_t
rig_simulator_input_handler(rut_input_event_t *event,
                            void *user_data)
{
    rig_simulator_t *simulator = user_data;
    rig_engine_t *engine = simulator->engine;

    rig_ui_code_modules_handle_input(engine->ui, event);

    rig_ui_handle_input_event(engine->ui, event);

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
simulator_on_run_cb(rut_shell_t *shell, void *user_data)
{
    rig_simulator_t *simulator = user_data;
    rig_engine_t *engine;

    simulator_start_service(simulator->shell, simulator);

    simulator->engine =
        rig_engine_new_for_simulator(simulator->shell, simulator);
    engine = simulator->engine;

    simulator->object_registry = c_hash_table_new(NULL, /* direct hash */
                                                  NULL); /* direct key equal */

    rig_engine_op_map_context_init(&simulator->map_to_sim_objects_op_ctx,
                                   engine,
                                   simulator_map_id_to_sim_object_cb,
                                   simulator); /* user data */

    rig_engine_op_map_context_init(&simulator->map_to_frontend_ids_op_ctx,
                                   engine,
                                   simulator_map_object_to_frontend_id_cb,
                                   simulator); /* user data */

    /* Finish the simulator specific engine setup...
     */
    engine->garbage_collect_callback = simulator_garbage_collect_object_cb;
    engine->garbage_collect_data = simulator;

    rig_engine_op_apply_context_init(&simulator->apply_op_ctx,
                                     engine,
                                     simulator_register_object_with_id_cb,
                                     simulator_lookup_object_cb,
                                     simulator); /* user data */
    rig_engine_set_apply_op_context(engine, &simulator->apply_op_ctx);

    rig_engine_set_log_op_callback(engine, log_op_cb, simulator);

    /* The ops_serializer is used to serialize operations generated by
     * UI logic in the simulator that will be forwarded to the frontend.
     */
    rig_pb_serializer_set_object_register_callback(
        engine->ops_serializer, simulator_register_object_cb, simulator);
    rig_pb_serializer_set_object_to_id_callback(
        engine->ops_serializer, lookup_frontend_id_cb, simulator);

    rut_shell_add_input_callback(
        simulator->shell, rig_simulator_input_handler, simulator, NULL);
}

rig_simulator_t *
rig_simulator_new(rut_shell_t *main_shell)
{
    rig_simulator_t *simulator = rut_object_alloc0(
        rig_simulator_t, &rig_simulator_type, _rig_simulator_init_type);

    simulator->redraw_queued = false;

    c_list_init(&simulator->connected_closures);

    simulator->ops = rut_queue_new();

    c_list_init(&simulator->actions);

    /* On platforms where we must run everything in a single thread
     * 'main_shell' associates the simulator's shell with the frontend
     * shell whose mainloop we will share... */
    simulator->shell = rut_shell_new(main_shell,
                                     rig_simulator_run_frame,
                                     simulator);

    rut_shell_set_is_headless(simulator->shell, true);

    rut_shell_set_queue_redraw_callback(
        simulator->shell, rig_simulator_queue_redraw_hook, simulator);

    rut_shell_set_on_run_callback(simulator->shell,
                                  simulator_on_run_cb,
                                  simulator);

    simulator->stream = rig_pb_stream_new(simulator->shell);

#ifdef __EMSCRIPTEN__
    rig_pb_stream_set_in_worker(simulator->stream, true);
#endif

    rig_logs_set_simulator(simulator);

#ifdef USE_MOZJS
    simulator->js = rig_js_runtime_new(simulator);
#endif

    return simulator;
}

void
rig_simulator_set_frontend_fd(rig_simulator_t *simulator, int fd)
{
#ifdef __EMSCRIPTEN__
    c_warn_if_reached();
#else
    rig_pb_stream_set_fd_transport(simulator->stream, fd);
#endif
}

void
rig_simulator_load_file(rig_simulator_t *simulator, const char *filename)
{
    rig_engine_t *engine = simulator->engine;
    rig_ui_t *ui = NULL;

    if (filename)
        ui = rig_load(engine, filename);

    if (!ui)
        ui = rig_ui_new(engine);

    rig_engine_set_ui(engine, ui);
    rut_object_unref(ui);

    rig_engine_op_apply_context_set_ui(&simulator->apply_op_ctx, ui);
}

struct sim_connected_state {
    rut_closure_t closure;
    rig_simulator_t *simulator;
    char *ui_filename;
};

static void
free_sim_connect_state(void *user_data)
{
    struct sim_connected_state *state = user_data;
    free(state->ui_filename);
    free(state);
}

static void
frontend_connected_cb(rig_simulator_t *simulator,
                      void *user_data)
{
    struct sim_connected_state *state = user_data;

    if (state->ui_filename) {
        rig_simulator_load_file(simulator, state->ui_filename);
        rig_simulator_reload_frontend_ui(simulator, simulator->engine->ui);
    }
}

void
rig_simulator_queue_ui_load_on_connect(rig_simulator_t *simulator,
                                       const char *ui_filename)
{
    struct sim_connected_state *state =
        c_malloc0(sizeof(struct sim_connected_state));

    state->simulator = simulator;
    state->ui_filename = ui_filename ? strdup(ui_filename) : NULL;

    rut_closure_init(&state->closure,
                     frontend_connected_cb,
                     state);
    rut_closure_set_finalize(&state->closure, free_sim_connect_state);

    rig_simulator_add_connected_callback(simulator, &state->closure);
}

void
rig_simulator_run(rig_simulator_t *simulator)
{
    /* XXX: normally this is handled by rut-poll.c, but until we enter
     * the mainloop we want to set the 'current shell' so any log
     * messages will be properly associated with the simulator */
    rut_set_thread_current_shell(simulator->shell);

    rut_shell_main(simulator->shell);
}

static void
handle_update_ui_ack(const Rig__UpdateUIAck *result,
                     void *closure_data)
{
    // c_debug ("Simulator: UI Update ACK received\n");
}

typedef struct _serialize_changes_state_t {
    rig_simulator_t *simulator;
    rig_pb_serializer_t *serializer;
    Rig__PropertyChange *pb_changes;
    Rig__PropertyValue *pb_values;
    int n_changes;
    int i;
} serialize_changes_state_t;

static void
stack_region_cb(uint8_t *data, size_t bytes, void *user_data)
{
    serialize_changes_state_t *state = user_data;
    rig_simulator_t *simulator = state->simulator;
    size_t step = sizeof(rig_property_change_t);
    size_t offset;
    int i;

    // c_debug ("Properties changed log %d bytes:\n", bytes);

    for (i = state->i, offset = 0;
         i < state->n_changes && (offset + step) <= bytes;
         i++, offset += step) {
        rig_property_change_t *change =
            (rig_property_change_t *)(data + offset);
        Rig__PropertyChange *pb_change = &state->pb_changes[i];
        Rig__PropertyValue *pb_value = &state->pb_values[i];

        rig__property_change__init(pb_change);
        rig__property_value__init(pb_value);

        pb_change->has_object_id = true;
        pb_change->object_id = simulator_lookup_object_id(simulator, change->object);
        pb_change->has_property_id = true;
        pb_change->property_id = change->prop_id;
        rig_pb_property_value_init(state->serializer, pb_value, &change->boxed);

        if (change->boxed.type == RUT_PROPERTY_TYPE_OBJECT) {
            pb_value->object_value =
                simulator_lookup_object_id(simulator, change->boxed.d.object_val);
        }

        rut_boxed_destroy(&change->boxed);

#if 0
        c_debug(
            "> %d: base = %p, offset = %d, obj id=%llu:%p:%s, prop id = %d\n",
            i,
            data,
            offset,
            (uint64_t)pb_change->object_id,
            change->object,
            rut_object_get_type_name(change->object),
            change->prop_id);
#endif
    }

    state->i = i;
}

void
rig_simulator_run_frame(rut_shell_t *shell, void *user_data)
{
    rig_simulator_t *simulator = user_data;
    rig_engine_t *engine = simulator->engine;
    ProtobufCService *frontend_service =
        rig_pb_rpc_client_get_service(simulator->simulator_peer->pb_rpc_client);
    Rig__UIDiff ui_diff;
    rig_property_context_t *prop_ctx = &engine->_property_ctx;
    int n_changes;
    rig_pb_serializer_t *serializer;
    rut_queue_t *ops;

    simulator->redraw_queued = false;
    rut_shell_remove_paint_idle(shell);

    if (!engine->ui)
        return;

    simulator->in_frame = true;

    /* Setup the property context to log all property changes so they
     * can be sent back to the frontend process each frame. */
    engine->_property_ctx.logging_disabled--;

    rig_engine_progress_timelines(engine, simulator->frame_info.progress);

    rut_shell_dispatch_input_events(shell);

    rig_ui_code_modules_update(engine->ui, &(rig_code_module_update_t) {
                                    .progress = simulator->frame_info.progress
                               });

#if 0
    static int counter = 0;

    if (counter++ > 100)
    {
        rut_object_t *entity = rig_ui_find_entity (engine->ui,
                                                   "pinwheel");
        rig_property_t *scale_prop;
        rut_boxed_t boxed;

        scale_prop = rig_introspectable_get_property (entity,
                                                      RUT_ENTITY_PROP_SCALE);
#if 1
        boxed.type = RUT_PROPERTY_TYPE_FLOAT;
        boxed.d.float_val = counter;
        rig_engine_op_set_property (engine,
                                    scale_prop,
                                    &boxed);
#else
        rig_property_set_float (&engine->_property_ctx,
                                scale_prop,
                                counter);
#endif
    }

    c_debug ("Frame = %d\n", counter);
#endif

    if (rig_engine_check_timelines(engine))
        rut_shell_queue_redraw(shell);

    // c_debug ("Simulator: Sending UI Update\n");

    n_changes = prop_ctx->log_len;
    serializer = rig_pb_serializer_new(engine);

    rig_pb_serializer_set_object_to_id_callback(serializer,
                                                direct_object_id_cb,
                                                simulator);

    rig__uidiff__init(&ui_diff);

    ui_diff.n_property_changes = n_changes;
    if (n_changes) {
        serialize_changes_state_t state;
        int i;

        state.simulator = simulator;
        state.serializer = serializer;

        state.pb_changes =
            rut_memory_stack_memalign(engine->frame_stack,
                                      sizeof(Rig__PropertyChange) * n_changes,
                                      C_ALIGNOF(Rig__PropertyChange));
        state.pb_values =
            rut_memory_stack_memalign(engine->frame_stack,
                                      sizeof(Rig__PropertyValue) * n_changes,
                                      C_ALIGNOF(Rig__PropertyValue));

        state.i = 0;
        state.n_changes = n_changes;

        rut_memory_stack_foreach_region(
            prop_ctx->change_log_stack, stack_region_cb, &state);

        ui_diff.property_changes =
            rut_memory_stack_memalign(engine->frame_stack,
                                      sizeof(void *) * n_changes,
                                      C_ALIGNOF(void *));

        for (i = 0; i < n_changes; i++) {
            ui_diff.property_changes[i] = &state.pb_changes[i];
            ui_diff.property_changes[i]->value = &state.pb_values[i];
        }
    }

    ops = simulator->ops;
    if (ops->len) {
        ui_diff.edit =
            rig_pb_new(engine->ops_serializer, Rig__UIEdit, rig__uiedit__init);
        ui_diff.edit->n_ops = ops->len;
        ui_diff.edit->ops =
            rig_pb_serialize_ops_queue(engine->ops_serializer, ops);
        rut_queue_clear(ops);

        rig_engine_map_pb_ui_edit(
            &simulator->map_to_frontend_ids_op_ctx,
            NULL, /* no apply ctx, since ops already applied */
            ui_diff.edit);
    }

    ui_diff.n_actions = simulator->n_actions;
    if (ui_diff.n_actions) {
        Rig__SimulatorAction *pb_actions;
        rig_simulator_action_t *action, *tmp;
        int i;

        ui_diff.actions =
            rut_memory_stack_memalign(engine->frame_stack,
                                      sizeof(void *) * ui_diff.n_actions,
                                      C_ALIGNOF(void *));
        pb_actions = rut_memory_stack_memalign(
            engine->frame_stack,
            sizeof(Rig__SimulatorAction) * ui_diff.n_actions,
            C_ALIGNOF(Rig__SimulatorAction));

        i = 0;
        c_list_for_each_safe(action, tmp, &simulator->actions, list_node)
        {
            Rig__SimulatorAction *pb_action = &pb_actions[i];

            rig__simulator_action__init(pb_action);

            pb_action->type = action->type;

            switch (action->type) {
            case RIG_SIMULATOR_ACTION_TYPE_REPORT_EDIT_FAILURE:
                pb_action->report_edit_failure = rig_pb_new(
                    serializer,
                    Rig__SimulatorAction__ReportEditFailure,
                    rig__simulator_action__report_edit_failure__init);
                break;
            }

            ui_diff.actions[i] = pb_action;

            i++;
        }
    }

    clear_actions(simulator);

    if (simulator->redraw_queued) {
        ui_diff.has_queue_frame = true;
        ui_diff.queue_frame = true;
    }

    rig__frontend__update_ui(
        frontend_service, &ui_diff, handle_update_ui_ack, NULL);

    simulator->in_frame = false;

    rig_pb_serializer_destroy(serializer);

    rig_property_context_clear_log(prop_ctx);

    /* Stop logging property changes until the next frame. */
    engine->_property_ctx.logging_disabled++;

    /* Garbage collect deleted objects
     *
     * XXX: We defer the freeing of objects until we have finished a
     * frame so that we can send our ui update back to the frontend
     * faster and handle freeing while we wait for new work from the
     * frontend.
     */
    rig_engine_garbage_collect(engine);

    rut_memory_stack_rewind(engine->frame_stack);
}

static void
handle_frame_req_ack(const Rig__FrameRequestAck *ack,
                     void *closure_data)
{
    // c_debug ("Simulator: Frame Request ACK received\n");
}


/* Redrawing in the simulator is driven by the frontend issuing
 * RunFrame requests, so we hook into rut_shell_queue_redraw()
 * and request a new frame from the frontend.
 */
void
rig_simulator_queue_redraw_hook(rut_shell_t *shell, void *user_data)
{
    rig_simulator_t *simulator = user_data;
    ProtobufCService *frontend_service =
        rig_pb_rpc_client_get_service(simulator->simulator_peer->pb_rpc_client);
    Rig__FrameRequest req = RIG__FRAME_REQUEST__INIT;

    if (!simulator->connected)
        return;

    if (simulator->redraw_queued)
        return;

    /* If we are in the middle of processing a frame then
     * we avoid sending mid-frame messages back to the
     * frontend and instead at the end of the frame the
     * update we send to the frontend will include a
     * flag requesting a new frame */
    if (!simulator->in_frame)
        rig__frontend__request_frame(frontend_service, &req,
                                     handle_frame_req_ack, NULL);

    simulator->redraw_queued = true;
}

static void
handle_forward_log_ack(const Rig__LogAck *ack,
                       void *closure_data)
{
}

void
rig_simulator_forward_log(rig_simulator_t *simulator)
{
    ProtobufCService *frontend_service =
        rig_pb_rpc_client_get_service(simulator->simulator_peer->pb_rpc_client);
    rig_pb_serializer_t *serializer;
    struct rig_log *simulator_log;
    struct rig_log_entry *entry;
    Rig__Log *pb_log;
    int i;

    if (!simulator->engine)
        return;

    if (!simulator->log_serializer) {
        simulator->log_serializer_stack = rut_memory_stack_new(8192);

        simulator->log_serializer = rig_pb_serializer_new (simulator->engine);
        rig_pb_serializer_set_stack(simulator->log_serializer,
                                    simulator->log_serializer_stack);
    }

    simulator_log = rig_logs_get_simulator_log();

    serializer = simulator->log_serializer;

    rig_logs_lock();
    pb_log = rig_pb_new(serializer, Rig__Log, rig__log__init);
    pb_log->has_type = true;
    pb_log->type = RIG__LOG__LOG_TYPE__SIMULATOR;
    pb_log->entries = rut_memory_stack_memalign(serializer->stack,
                                                sizeof(void *) * simulator_log->len,
                                                C_ALIGNOF(void *));
    pb_log->n_entries = simulator_log->len;

    i = 0;
    c_list_for_each(entry, &simulator_log->entries, link) {
        Rig__LogEntry *pb_entry = rig_pb_new(serializer,
                                             Rig__LogEntry,
                                             rig__log_entry__init);
        pb_entry->log_message = (char *)rig_pb_strdup(serializer, entry->message);
        pb_entry->has_log_level = true;
        pb_entry->log_level = entry->log_level;
        pb_entry->has_timestamp = true;
        pb_entry->timestamp = entry->timestamp;
        pb_log->entries[i++] = pb_entry;
    }
    rig_logs_clear_log(simulator_log);
    rig_logs_unlock();

    rig__frontend__forward_log(frontend_service,
                               pb_log,
                               handle_forward_log_ack, NULL);

    rut_memory_stack_rewind(simulator->log_serializer_stack);
}

bool
rig_simulator_parse_run_mode(const char *option,
                             void (*usage)(void),
                             enum rig_simulator_run_flags flags,
                             enum rig_simulator_run_mode *mode,
                             char **address_ret,
                             int *port_ret)
{
    char **strv = c_strsplit(option, ":", -1);
    bool listen = flags & RIG_SIMULATOR_LISTEN;
    bool standalone = (flags & RIG_SIMULATOR_STANDALONE) || listen;

    *address_ret = NULL;
    *port_ret = 0;

    if (!strv[0]) {
        usage();
        return false;
    }

    if (strcmp(strv[0], "tcp") == 0) {
#ifdef USE_UV
        *mode = listen ? RIG_SIMULATOR_RUN_MODE_LISTEN_TCP :
            RIG_SIMULATOR_RUN_MODE_CONNECT_TCP;

        if (!strv[1]) {
            fprintf(stderr, "Missing tcp address in form \"tcp:address\" or \"tcp:address:port\"\n");
            goto error;
        }

        *address_ret = c_strdup(strv[1]);
        *port_ret = strv[2] ? strtoul(strv[2], NULL, 10) : 0;
#else
        c_critical("TCP/IP sockets not supported");
        goto error;
#endif
    } else if (strcmp(strv[0], "abstract") == 0) {
#ifdef __linux__
        *mode = listen ? RIG_SIMULATOR_RUN_MODE_LISTEN_ABSTRACT_SOCKET :
            RIG_SIMULATOR_RUN_MODE_CONNECT_ABSTRACT_SOCKET;

        if (strv[1]) {
            *address_ret = c_strdup(strv[1]);
        } else {
            fprintf(stderr, "Missing abstract socket name in form \"abstract:my_socket_name\"\n");
            goto error;
        }
#else
        c_critical("Abstract sockets are only supported on Linux");
        goto error;
#endif
    } else if (!standalone) {

        if (strcmp(strv[0], "mainloop") == 0) {
            *mode = RIG_SIMULATOR_RUN_MODE_MAINLOOP;
        } else if (strcmp(strv[0], "thread") == 0) {
#ifdef C_SUPPORTS_THREADS
            *mode = RIG_SIMULATOR_RUN_MODE_THREADED;
#else
            c_critical("Platform doesn't support threads");
            goto error;
#endif
        } else if (strcmp(strv[0], "process") == 0) {
#ifdef SUPPORT_SIMULATOR_PROCESS
            *mode = RIG_SIMULATOR_RUN_MODE_PROCESS;
#else
            c_critical("Platform doesn't support sub-processes");
            goto error;
#endif
        }

    } else {
        fprintf(stderr, "Unsupported simulator mode \"%s\"\n", option);
        goto error;
    }

    c_strfreev(strv);
    return true;

error:
    c_strfreev(strv);
    usage();
    return false;
}

void
rig_simulator_add_connected_callback(rig_simulator_t *simulator,
                                     rut_closure_t *closure)
{
    rut_closure_list_add(&simulator->connected_closures, closure);
}

static void
handle_load_response(const Rig__LoadResult *result,
                     void *closure_data)
{
    c_debug ("UI loaded response received from frontend\n");
}

void
rig_simulator_forward_frontend_ui(rig_simulator_t *simulator,
                                  const Rig__UI *pb_ui)
{
    ProtobufCService *frontend_service;

    if (!simulator->connected)
        return;

    frontend_service =
        rig_pb_rpc_client_get_service(simulator->simulator_peer->pb_rpc_client);

    rig__frontend__load(frontend_service, pb_ui, handle_load_response, NULL);
}

#if 0
bool
asset_filter_cb(rig_asset_t *asset, void *user_data)
{
    rig_simulator_t *simulator = user_data;

    switch (rig_asset_get_type(asset)) {
    case RIG_ASSET_TYPE_BUILTIN:
    case RIG_ASSET_TYPE_TEXTURE:
    case RIG_ASSET_TYPE_NORMAL_MAP:
    case RIG_ASSET_TYPE_ALPHA_MASK:
        if (simulator->frontend_features.image_loader)
            return false;
        else
            return true;
    case RIG_ASSET_TYPE_FONT:
    case RIG_ASSET_TYPE_MESH:
        return true;
    }

    c_warn_if_reached();
    return false;
}
#endif

void
rig_simulator_reload_frontend_ui(rig_simulator_t *simulator,
                                 rig_ui_t *ui)
{
    rig_engine_t *engine;
    rig_pb_serializer_t *serializer;
    Rig__UI *pb_ui;

    c_return_if_fail(simulator->connected);

    engine = simulator->engine;
    serializer = rig_pb_serializer_new(engine);

    rig_pb_serializer_set_use_pointer_ids_enabled(serializer, true);
    if (simulator->frontend_features.image_loader)
        rig_pb_serializer_set_skip_image_data(serializer, true);
    //rig_pb_serializer_set_asset_filter(serializer, asset_filter_cb, simulator);

    pb_ui = rig_pb_serialize_ui(serializer, ui);

    rig_simulator_forward_frontend_ui(simulator, pb_ui);

    rig_pb_serialized_ui_destroy(pb_ui);

    rig_pb_serializer_destroy(serializer);

    rig_engine_op_apply_context_set_ui(&simulator->apply_op_ctx, ui);
}
