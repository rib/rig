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

#include <config.h>

#include <stdlib.h>
#include <clib.h>

#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif

#include <rut.h>

#include "rig-slave.h"
#include "rig-engine.h"
#include "rig-avahi.h"
#include "rig-rpc-network.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

static void
slave__test(Rig__Slave_Service *service,
            const Rig__Query *query,
            Rig__TestResult_Closure closure,
            void *closure_data)
{
    Rig__TestResult result = RIG__TEST_RESULT__INIT;
    // rig_slave_t *slave = rig_pb_rpc_closure_get_connection_data
    // (closure_data);

    c_return_if_fail(query != NULL);

    c_print("Test Query\n");

    closure(&result, closure_data);
}

static rut_magazine_t *_rig_slave_object_id_magazine = NULL;

static void
free_object_id(void *id)
{
    rut_magazine_chunk_free(_rig_slave_object_id_magazine, id);
}

static void *
lookup_object_cb(uint64_t id, void *user_data)
{
    rig_slave_t *slave = user_data;
    return c_hash_table_lookup(slave->edit_id_to_play_object_map, &id);
}

static void *
lookup_object(rig_slave_t *slave, uint64_t id)
{
    return lookup_object_cb(id, slave);
}

static void
register_object_cb(void *object, uint64_t edit_mode_id, void *user_data)
{
    rig_slave_t *slave = user_data;
    uint64_t *object_id;

    if (lookup_object(slave, edit_mode_id)) {
        c_critical("Tried to re-register object");
        return;
    }

    /* XXX: We need a mechanism for hooking into frontend edits that
     * happen as a result of UI logic so we can make sure to unregister
     * objects that might be deleted by UI logic. */

    object_id = rut_magazine_chunk_alloc(_rig_slave_object_id_magazine);
    *object_id = edit_mode_id;

    c_hash_table_insert(slave->edit_id_to_play_object_map, object_id, object);
    c_hash_table_insert(slave->play_object_to_edit_id_map, object, object_id);
}

static void
load_ui(rig_slave_t *slave)
{
    Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
    rig_engine_t *engine = slave->engine;
    float width, height;
    rig_pb_un_serializer_t *unserializer;
    const Rig__UI *pb_ui = slave->pending_ui_load;
    rig_ui_t *ui;

    c_return_if_fail(pb_ui != NULL);

    if (slave->edit_id_to_play_object_map) {
        rig_engine_set_play_mode_ui(engine, NULL);

        c_hash_table_destroy(slave->edit_id_to_play_object_map);
        slave->edit_id_to_play_object_map = NULL;
        c_hash_table_destroy(slave->play_object_to_edit_id_map);
        slave->play_object_to_edit_id_map = NULL;
    }

    slave->edit_id_to_play_object_map =
        c_hash_table_new_full(c_int64_hash,
                              c_int64_equal, /* key equal */
                              free_object_id, /* key destroy */
                              NULL); /* value destroy */

    /* Note: we don't have a free function for the value because we
     * share object_ids between both hash-tables and need to be
     * careful not to double free them. */
    slave->play_object_to_edit_id_map =
        c_hash_table_new(NULL, /* direct hash */
                         NULL); /* direct key equal */

    unserializer = rig_pb_unserializer_new(engine);

    rig_pb_unserializer_set_object_register_callback(
        unserializer, register_object_cb, slave);

    rig_pb_unserializer_set_id_to_object_callback(
        unserializer, lookup_object_cb, slave);

    ui = rig_pb_unserialize_ui(unserializer, pb_ui);

    rig_pb_unserializer_destroy(unserializer);

    rig_engine_set_play_mode_ui(engine, ui);

    rig_frontend_reload_simulator_ui(slave->frontend, ui, true /* play mode */);

    if (slave->request_width > 0 && slave->request_height > 0) {
        width = slave->request_width;
        height = slave->request_height;
    } else if (slave->request_scale) {
        width = engine->device_width * slave->request_scale;
        height = engine->device_height * slave->request_scale;
    } else {
        width = engine->device_width / 2;
        height = engine->device_height / 2;
    }

    rig_engine_set_onscreen_size(engine, width, height);

    rig_engine_op_apply_context_set_ui(&slave->apply_op_ctx, ui);

    slave->pending_ui_load_closure(&result,
                                   slave->pending_ui_load_closure_data);

    slave->pending_ui_load = NULL;
    slave->pending_ui_load_closure = NULL;
    slave->pending_ui_load_closure_data = NULL;
}

/* When this is called we know that the frontend is in sync with the
 * simulator which has just sent the frontend a ui-update that has
 * been applied and so we can apply edits without fear of conflicting
 * with the simulator...
 */
static void
ui_load_cb(rig_frontend_t *frontend, void *user_data)
{
    rig_slave_t *slave = user_data;

    rut_closure_disconnect(slave->ui_load_closure);
    slave->ui_load_closure = NULL;
    load_ui(slave);
}

typedef struct _pending_edit_t {
    rig_slave_t *slave;
    rut_closure_t *ui_update_closure;

    Rig__UIEdit *edit;
    Rig__UIEditResult_Closure closure;
    void *closure_data;

    bool status;
} pending_edit_t;

static void
slave__load(Rig__Slave_Service *service,
            const Rig__UI *pb_ui,
            Rig__LoadResult_Closure closure,
            void *closure_data)
{
    rig_slave_t *slave = rig_pb_rpc_closure_get_connection_data(closure_data);
    rig_frontend_t *frontend = slave->frontend;
    rut_queue_item_t *item;

    c_print("Slave: UI Load Request\n");

    /* Discard any previous pending ui load, since it's now redundant */
    if (slave->pending_ui_load) {
        Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
        slave->pending_ui_load_closure(&result,
                                       slave->pending_ui_load_closure_data);
    }

    slave->pending_ui_load = pb_ui;
    slave->pending_ui_load_closure = closure;
    slave->pending_ui_load_closure_data = closure_data;

    /* Discard any pending edit, since it's now redundant... */
    rut_list_for_each(item, &slave->pending_edits->items, list_node)
    {
        Rig__UIEditResult result = RIG__UIEDIT_RESULT__INIT;
        pending_edit_t *pending_edit = item->data;

        pending_edit->closure(&result, pending_edit->closure_data);

        c_slice_free(pending_edit_t, pending_edit);
    }

    /* XXX: If the simulator is busy we need to synchonize with it
     * before applying any edits... */
    if (!frontend->ui_update_pending)
        load_ui(slave);
    else {
        slave->ui_load_closure = rig_frontend_add_ui_update_callback(
            frontend, ui_load_cb, slave, NULL); /* destroy */
    }
}

/* When this is called we know that the frontend is in sync with the
 * simulator which has just sent the frontend a ui-update that has
 * been applied and so we can apply edits without fear of conflicting
 * with the simulator...
 */
static void
ui_updated_cb(rig_frontend_t *frontend, void *user_data)
{
    rig_slave_t *slave = user_data;

    rut_closure_disconnect(slave->ui_update_closure);
    slave->ui_update_closure = NULL;

    /* We don't apply all pending edits now, and instead wait until
     * we are setting up the next simulator frame, since we can
     * only forward the simulator one set of edits at a time and
     * we want to apply the edits in the frontend at the same time
     * they are forwarded to the simulator...
     */
    rut_shell_queue_redraw(slave->engine->shell);
}

static void
slave__edit(Rig__Slave_Service *service,
            const Rig__UIEdit *pb_ui_edit,
            Rig__UIEditResult_Closure closure,
            void *closure_data)
{
    rig_slave_t *slave = rig_pb_rpc_closure_get_connection_data(closure_data);
    pending_edit_t *pending_edit = c_slice_new0(pending_edit_t);
    rig_frontend_t *frontend = slave->frontend;

    c_print("Slave: UI Edit Request\n");

    pending_edit->slave = slave;
    pending_edit->edit = (Rig__UIEdit *)pb_ui_edit;
    pending_edit->status = true;
    pending_edit->closure = closure;
    pending_edit->closure_data = closure_data;

    rut_queue_push_tail(slave->pending_edits, pending_edit);

    /* XXX: If the simulator is busy we need to synchonize with it
     * before applying any edits. The edits will be applied the
     * next time we setup a frame for the simulator. */
    if (!frontend->ui_update_pending)
        rut_shell_queue_redraw(slave->engine->shell);
    else {
        if (!slave->ui_update_closure) {
            slave->ui_update_closure = rig_frontend_add_ui_update_callback(
                frontend, ui_updated_cb, slave, NULL); /* destroy */
        }
    }
}

static Rig__Slave_Service rig_slave_service = RIG__SLAVE__INIT(slave__);

static void
client_close_handler(pb_rpc__server_connection_t *conn,
                     void *user_data)
{
    c_warning("slave master disconnected %p", conn);
}

static void
new_client_handler(pb_rpc__server_t *server,
                   pb_rpc__server_connection_t *conn,
                   void *user_data)
{
    rig_slave_t *slave = user_data;
    // rig_engine_t *engine = slave->engine;

    rig_pb_rpc_server_connection_set_close_handler(
        conn, client_close_handler, slave);

    rig_pb_rpc_server_connection_set_data(conn, slave);

    c_message("slave master connected %p", conn);
}

static void
server_error_handler(PB_RPC_Error_Code code,
                     const char *message,
                     void *user_data)
{
    rig_slave_t *slave = user_data;
    rig_engine_t *engine = slave->engine;

    c_warning("Server error: %s", message);

#ifdef USE_AVAHI
    rig_avahi_unregister_service(engine);
#endif

    rig_rpc_server_shutdown(engine->slave_service);

    rut_object_unref(engine->slave_service);
    engine->slave_service = NULL;
}

static uint64_t
map_edit_id_to_play_object_cb(uint64_t edit_id, void *user_data)
{
    void *play_object = lookup_object(user_data, edit_id);
    return (uint64_t)(uintptr_t)play_object;
}

static void
rig_slave_init(rut_shell_t *shell, void *user_data)
{
    rig_slave_t *slave = user_data;
    rig_engine_t *engine;
    int listening_fd;

    slave->ui_update_closure = NULL;

    slave->frontend = rig_frontend_new(
        shell, RIG_FRONTEND_ID_SLAVE, true /* start in play mode */);

    engine = slave->frontend->engine;
    slave->engine = engine;

    /* Finish the slave specific engine setup...
     */
    engine->main_camera_view = rig_camera_view_new(engine);
    rut_stack_add(engine->top_stack, engine->main_camera_view);

    /* Initialize the current mode */
    rig_engine_set_play_mode_enabled(engine, true /* start in play mode */);

    rig_frontend_post_init_engine(slave->frontend, NULL /* no ui to load */);

    _rig_slave_object_id_magazine = engine->object_id_magazine;

    rig_engine_op_map_context_init(&slave->map_op_ctx,
                                   engine,
                                   map_edit_id_to_play_object_cb,
                                   slave); /* user data */

    rig_engine_op_apply_context_init(&slave->apply_op_ctx,
                                     engine,
                                     register_object_cb,
                                     NULL, /* unregister id */
                                     slave); /* user data */

    slave->pending_edits = rut_queue_new();

#ifdef __ANDROID__
    listening_fd = rut_os_listen_on_abstract_socket("rig-slave", NULL);
#else
    listening_fd = rut_os_listen_on_tcp_socket(0, NULL);
#endif

    engine->slave_service = rig_rpc_server_new(engine->shell,
                                               "Slave",
                                               listening_fd,
                                               &rig_slave_service.base,
                                               server_error_handler,
                                               new_client_handler,
                                               slave);

#ifdef USE_AVAHI
    rig_avahi_register_service(engine);
#endif

    rut_shell_add_input_callback(
        slave->shell, rig_engine_input_handler, engine, NULL);
}

static void
rig_slave_fini(rut_shell_t *shell, void *user_data)
{
    rig_slave_t *slave = user_data;
    rig_engine_t *engine = slave->engine;
    rut_queue_item_t *item;

    if (slave->ui_update_closure) {
        rut_closure_disconnect(slave->ui_update_closure);
        slave->ui_update_closure = NULL;
    }

    rut_list_for_each(item, &slave->pending_edits->items, list_node)
    c_slice_free(pending_edit_t, item->data);
    rut_queue_free(slave->pending_edits);

    rig_engine_op_map_context_destroy(&slave->map_op_ctx);
    rig_engine_op_apply_context_destroy(&slave->apply_op_ctx);

#ifdef USE_AVAHI
    /* TODO: move to frontend */
    rig_avahi_unregister_service(engine);
#endif

    rig_rpc_server_shutdown(engine->slave_service);

    slave->engine = NULL;

    rut_object_unref(slave->frontend);
    slave->frontend = NULL;
}

/* Note: here we have to consider objects that are deleted via edit
 * operations (where we can expect corresponding entries in
 * play_object_to_edit_id_map and edit_id_to_play_object_map) and
 * objects deleted via a ui_update from the simulator, due to some ui
 * logic (where the deleted play-mode object may not have a
 * corresponding edit-mode id).
 */
static void
object_delete_cb(rut_object_t *object, void *user_data)
{
    rig_slave_t *slave = user_data;
    uint64_t *object_id =
        c_hash_table_lookup(slave->play_object_to_edit_id_map, object);

    if (object_id) {
        c_hash_table_remove(slave->edit_id_to_play_object_map, object_id);
        c_hash_table_remove(slave->play_object_to_edit_id_map, object);
    }
}

static void
handle_pending_edit_operations(rig_slave_t *slave,
                               rig_pb_serializer_t *serializer,
                               pending_edit_t *pending_edit,
                               Rig__FrameSetup *setup)
{
    /* Note: Since a slave device is effectively always running in
     * play-mode the state of the UI is unpredictable and it's
     * always possible that edits made in an editor can no longer be
     * applied to the current state of a slave device (for example
     * an object being edited may have been deleted by some UI
     * logic)
     *
     * We apply edits on a best-effort basis, and if they fail we
     * report that status back to the editor so that it can inform
     * the user who can choose to reset the slave.
     */
    if (!rig_engine_map_pb_ui_edit(
            &slave->map_op_ctx, &slave->apply_op_ctx, pending_edit->edit)) {
        pending_edit->status = false;
    }

    /* Note: we disregard whether we failed to apply the edits
     * in the frontend, since some of the edit operations may
     * succeed, and as long as we can report the error to the
     * user they can decided if they want to reset the slave
     * device.
     */
    /* FIXME: protoc-c should declare this member as const */
    setup->play_edit = (Rig__UIEdit *)pending_edit->edit;
}

static void
rig_slave_paint(rut_shell_t *shell, void *user_data)
{
    rig_slave_t *slave = user_data;
    rig_engine_t *engine = slave->engine;
    rig_frontend_t *frontend = engine->frontend;

    rut_shell_start_redraw(shell);

    /* XXX: we only kick off a new frame in the simulator if it's not
     * still busy... */
    if (!frontend->ui_update_pending) {
        rut_input_queue_t *input_queue = rut_shell_get_input_queue(shell);
        Rig__FrameSetup setup = RIG__FRAME_SETUP__INIT;
        rig_pb_serializer_t *serializer;
        pending_edit_t *pending_edit = NULL;

        serializer = rig_pb_serializer_new(engine);

        setup.has_play_mode = true;
        setup.play_mode = engine->play_mode;

        setup.n_events = input_queue->n_events;
        setup.events = rig_pb_serialize_input_events(serializer, input_queue);

        if (frontend->has_resized) {
            setup.has_view_width = true;
            setup.view_width = engine->window_width;
            setup.has_view_height = true;
            setup.view_height = engine->window_height;
            frontend->has_resized = false;
        }

        /* Forward any received edits to the simulator too.
         *
         * Note: Although we may have a backlog of edits from the
         * editor, we can currently only send one Rig__UIEdit per
         * frame...
         */
        if (slave->pending_edits->len) {
            pending_edit = rut_queue_pop_head(slave->pending_edits);

            handle_pending_edit_operations(
                slave, serializer, pending_edit, &setup);
        }

        rig_frontend_run_simulator_frame(frontend, serializer, &setup);

        if (pending_edit) {
            Rig__UIEditResult result = RIG__UIEDIT_RESULT__INIT;

            if (pending_edit->status == false) {
                result.has_status = true;
                result.status = false;
            }

            pending_edit->closure(&result, pending_edit->closure_data);

            c_slice_free(pending_edit_t, pending_edit);
        }

        rig_pb_serializer_destroy(serializer);

        rut_input_queue_clear(input_queue);

        rut_memory_stack_rewind(engine->sim_frame_stack);
    }

    rut_shell_update_timelines(shell);

    rut_shell_run_pre_paint_callbacks(shell);

    rut_shell_run_start_paint_callbacks(shell);

    rig_engine_paint(engine);

    rig_engine_garbage_collect(engine, object_delete_cb, slave);

    rut_shell_run_post_paint_callbacks(shell);

    rut_memory_stack_rewind(engine->frame_stack);

    rut_shell_end_redraw(shell);

    /* XXX: It would probably be better if we could send multiple
     * Rig__UIEdits in one go when setting up a simulator frame so we
     * wouldn't need to use this trick of continuously queuing redraws
     * to flush the edits through to the simulator.
     */
    if (rut_shell_check_timelines(shell) || slave->pending_edits->len) {
        rut_shell_queue_redraw(shell);
    }
}

static void
_rig_slave_free(void *object)
{
    rig_slave_t *slave = object;

    if (slave->frontend)
        rut_object_unref(slave->frontend);

    rut_object_unref(slave->ctx);
    rut_object_unref(slave->shell);

    rut_object_free(rig_slave_t, slave);
}

static rut_type_t rig_slave_type;

static void
_rig_slave_init_type(void)
{
    rut_type_init(&rig_slave_type, "rig_slave_t", _rig_slave_free);
}

rig_slave_t *
rig_slave_new(int width, int height, int scale)
{
    rig_slave_t *slave =
        rut_object_alloc0(rig_slave_t, &rig_slave_type, _rig_slave_init_type);

    slave->request_width = width;
    slave->request_height = height;
    slave->request_scale = scale;

    slave->shell = rut_shell_new(false, /* not headless */
                                 rig_slave_init,
                                 rig_slave_fini,
                                 rig_slave_paint,
                                 slave);

    slave->ctx = rut_context_new(slave->shell);

    rut_context_init(slave->ctx);

    return slave;
}

void
rig_slave_run(rig_slave_t *slave)
{
    rut_shell_main(slave->shell);
}

static void
print_id_to_obj_mapping_cb(void *key, void *value, void *user_data)
{
    char *obj = rig_engine_get_object_debug_name(value);

    c_print("  [%llx] -> [%50s]\n", *(uint64_t *)key, obj);

    c_free(obj);
}

static void
print_obj_to_id_mapping_cb(void *key, void *value, void *user_data)
{
    char *obj = rig_engine_get_object_debug_name(key);

    c_print("  [%50s] -> [%llx]\n", obj, *(uint64_t *)key);

    c_free(obj);
}

void
rig_slave_print_mappings(rig_slave_t *slave)
{
    c_print("Edit ID to play object mappings:\n");
    c_hash_table_foreach(
        slave->edit_id_to_play_object_map, print_id_to_obj_mapping_cb, NULL);

    c_print("\n\n");
    c_print("Play object to edit ID mappings:\n");
    c_hash_table_foreach(
        slave->play_object_to_edit_id_map, print_obj_to_id_mapping_cb, NULL);
}
