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

#ifdef USE_UV
#include <uv.h>
#endif

#include <clib.h>

#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif

#include <rut.h>

#include "rig-slave.h"
#include "rig-engine.h"
#ifdef USE_AVAHI
#include "rig-avahi.h"
#endif
#include "rig-rpc-network.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

enum rig_slave_connect_mode rig_slave_connect_mode_option;

#ifdef __linux__
const char *rig_slave_abstract_socket_option;
#endif

const char *rig_slave_address_option;
int rig_slave_port_option;

bool rig_slave_fullscreen_option;
bool rig_slave_oculus_option;

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

    c_debug("Test Query\n");

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
register_edit_object_cb(void *object, uint64_t edit_mode_id, void *user_data)
{
    rig_slave_t *slave = user_data;
    uint64_t *id_chunk;

    if (lookup_object(slave, edit_mode_id)) {
        c_critical("Tried to re-register object");
        return;
    }

    /* XXX: We need a mechanism for hooking into frontend edits that
     * happen as a result of UI logic so we can make sure to unregister
     * objects that might be deleted by UI logic. */

    id_chunk = rut_magazine_chunk_alloc(_rig_slave_object_id_magazine);
    *id_chunk = edit_mode_id;

    c_hash_table_insert(slave->edit_id_to_play_object_map, id_chunk, object);
    c_hash_table_insert(slave->play_object_to_edit_id_map, object, id_chunk);
}

static void
unregister_edit_id_cb(uint64_t edit_mode_id, void *user_data)
{
    void *object = c_hash_table_remove_value(slave->edit_id_to_play_object_map,
                                             &edit_mode_id);

    if (object)
        c_hash_table_remove_value(slave->play_object_to_edit_id_map, object);
}

static void
load_ui(rig_slave_t *slave)
{
    Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
    rig_engine_t *engine = slave->engine;
    float width, height;
    rig_pb_unserializer_t *unserializer;
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
        unserializer, register_edit_object_cb, slave);

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

    rut_closure_disconnect_FIXME(slave->ui_load_closure);
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

    c_debug("Slave: UI Load Request\n");

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
    c_list_for_each(item, &slave->pending_edits->items, list_node)
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

    rut_closure_disconnect_FIXME(slave->ui_update_closure);
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

    c_debug("Slave: UI Edit Request\n");

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

static void
slave__debug_control(Rig__Slave_Service *service,
                     const Rig__DebugConfig *pb_debug_config,
                     Rig__DebugConfigAck_Closure closure,
                     void *closure_data)
{
    //rig_slave_t *slave = rig_pb_rpc_closure_get_connection_data(closure_data);
    Rig__DebugConfigAck ack = RIG__DEBUG_CONFIG_ACK__INIT;

    closure(&ack, closure_data);
}

static Rig__Slave_Service rig_slave_service = RIG__SLAVE__INIT(slave__);

static void
slave_peer_connected(rig_pb_rpc_client_t *pb_client,
                     void *user_data)
{
    rig_slave_t *slave = user_data;

    slave->connected = true;

#if 0
    if (slave->editor_connected_callback) {
        void *user_data = slave->editor_connected_data;
        slave->editor_connected_callback(user_data);
    }
#endif

    c_debug("Slave peer connected\n");
}

static void
slave_stop_service(rig_slave_t *slave)
{
    rut_object_unref(slave->slave_peer);
    slave->slave_peer = NULL;
    rut_object_unref(slave->stream);
    slave->stream = NULL;

    slave->connected = false;
}

static void
slave_peer_error_handler(rig_pb_rpc_error_code_t code,
                         const char *message,
                         void *user_data)
{
    rig_slave_t *slave = user_data;

    c_warning("Slave peer error: %s", message);

    slave_stop_service(slave);
}

static void
slave_start_service(rut_shell_t *shell,
                    rig_slave_t *slave,
                    rig_pb_stream_t *stream)
{
    slave->stream = rut_object_ref(stream);
    slave->slave_peer = rig_rpc_peer_new(
        stream,
        &rig_slave_service.base,
        (ProtobufCServiceDescriptor *)&rig__slave_master__descriptor,
        slave_peer_error_handler,
        slave_peer_connected,
        slave);
}

static uint64_t
map_edit_id_to_play_object_cb(uint64_t edit_id, void *user_data)
{
    void *play_object = lookup_object(user_data, edit_id);
    return (uint64_t)(uintptr_t)play_object;
}

#ifdef __linux__
static void
handle_abstract_connect_cb(void *user_data, int listen_fd, int revents)
{
    rig_slave_t *slave = user_data;
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);
    int fd;

    c_return_if_fail(revents & RUT_POLL_FD_EVENT_IN);

    c_message("Editor abstract socket connect request received!");

    if (slave->connected) {
        c_warning("Ignoring editor connection while there's already one connected");
        return;
    }

    fd = accept(slave->listen_fd, &addr, &addr_len);
    if (fd != -1) {
        rig_pb_stream_t *stream = rig_pb_stream_new(slave->engine->shell);

        rig_pb_stream_set_fd_transport(stream, fd);

        c_message("Editor connected!");

        slave_start_service(slave->engine->shell, slave, stream);

        /* slave_start_service will take ownership of the stream */
        rut_object_unref(stream);
    } else
        c_message("Failed to accept editor connection: %s!",
                  strerror(errno));
}

static bool
bind_to_abstract_socket(rut_shell_t *shell,
                        const char *name,
                        rig_slave_t *slave)
{
    rut_exception_t *catch = NULL;
    int fd = rut_os_listen_on_abstract_socket(name, &catch);

    if (fd < 0) {
        c_critical("Failed to listen on abstract \"%s\" socket: %s",
                   name,
                   catch->message);
        return false;
    }

    slave->listen_fd = fd;

    rut_poll_shell_add_fd(shell,
                          slave->listen_fd,
                          RUT_POLL_FD_EVENT_IN,
                          NULL, /* prepare */
                          handle_abstract_connect_cb, /* dispatch */
                          slave);

    c_message("Waiting for simulator to connect to abstract socket \"%s\"...",
              name);

    return true;
}
#endif /* __linux__ */

#ifdef USE_UV
static void
handle_tcp_connect_cb(uv_stream_t *server, int status)
{
    rig_slave_t *slave = server->data;
    rig_pb_stream_t *stream;

    if (status != 0) {
        c_warning("Connection failure: %s", uv_strerror(status));
        return;
    }

    c_message("Editor tcp connect request received!");

    if (slave->connected) {
        c_warning("Ignoring editor connection while there's already one connected");
        return;
    }

    stream = rig_pb_stream_new(slave->engine->shell);
    rig_pb_stream_accept_tcp_connection(stream, &slave->listening_socket);

    c_message("Editor connected!");
    slave_start_service(slave->engine->shell, slave, stream);

    /* slave_start_service will take ownership of the stream */
    rut_object_unref(stream);
}

static void
bind_to_tcp_socket(rig_slave_t *slave)
{
    uv_loop_t *loop = rut_uv_shell_get_loop(slave->shell);
    struct sockaddr_in bind_addr;
    struct sockaddr name;
    int namelen;
    int err;

    uv_tcp_init(loop, &slave->listening_socket);
    slave->listening_socket.data = slave;

    uv_ip4_addr(rig_slave_address_option, rig_slave_port_option, &bind_addr);
    uv_tcp_bind(&slave->listening_socket, (struct sockaddr *)&bind_addr, 0);
    err = uv_listen((uv_stream_t*)&slave->listening_socket,
                    128, handle_tcp_connect_cb);
    if (err < 0) {
        c_critical("Failed to starting listening for slave connections: %s",
                   uv_strerror(err));
        return;
    }

    namelen = sizeof(name);
    err = uv_tcp_getsockname(&slave->listening_socket, &name, &namelen);
    if (err != 0) {
        c_critical("Failed to query peer address of listening tcp socket");
        return;
    } else {
        struct sockaddr_in *addr = (struct sockaddr_in *)&name;
        char ip_address[17] = {'\0'};

        c_return_if_fail(name.sa_family == AF_INET);

        uv_ip4_name(addr, ip_address, 16);
        slave->listening_address = c_strdup(ip_address);
        slave->listening_port = ntohs(addr->sin_port);
    }
}
#endif /* USE_UV */

static rut_input_event_status_t
slave_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    //rig_slave_t *slave = user_data;
    rut_shell_onscreen_t *onscreen = rut_input_event_get_onscreen(event);

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_KEY &&
        rut_key_event_get_action(event) == RUT_KEY_EVENT_ACTION_DOWN) {

        switch(rut_key_event_get_keysym(event)) {
        case RUT_KEY_F11:
            rut_shell_onscreen_set_fullscreen(onscreen,
                                              !onscreen->fullscreen);
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rig_slave_init(rut_shell_t *shell, void *user_data)
{
    rig_slave_t *slave = user_data;
    rig_engine_t *engine;

    slave->ui_update_closure = NULL;

    slave->frontend = rig_frontend_new(shell);

    engine = slave->frontend->engine;
    slave->engine = engine;

    /* Finish the slave specific engine setup...
     */

#error "FIXME: rig_frontend_new() initializes garbage_collect_callback..."
    engine->garbage_collect_callback = object_delete_cb;
    engine->garbage_collect_data = slave;

    rig_frontend_post_init_engine(slave->frontend, NULL /* no ui to load */);

#error "FIXME: support starting slave fullscreen"
#if 0
    if (rig_slave_fullscreen_option) {
        rig_view_t *view = ui->views->data;
        rut_shell_onscreen_t *onscreen = view->onscreen;

        rut_shell_onscreen_set_fullscreen(onscreen, true);
    }
#endif

    _rig_slave_object_id_magazine = engine->object_id_magazine;

    rut_shell_grab_input(shell,
                         NULL, /* camera */
                         slave_grab_input_cb,
                         slave);

    rig_engine_op_map_context_init(&slave->map_op_ctx,
                                   engine,
                                   map_edit_id_to_play_object_cb,
                                   slave); /* user data */

    /* Note: We rely on the slave's garbage_collect_callback to
     * unregister objects instead of passing an unregister id callback
     * here */
    rig_engine_op_apply_context_init(&slave->apply_op_ctx,
                                     engine,
                                     register_edit_object_cb,
                                     NULL, /* unregister id cb */
                                     slave); /* user data */

    slave->pending_edits = rut_queue_new();

    switch (rig_slave_connect_mode_option) {
#ifdef __linux__
    case RIG_SLAVE_CONNECT_MODE_ABSTRACT_SOCKET:
        bind_to_abstract_socket(engine->shell,
                                rig_slave_abstract_socket_option, slave);
        break;
#endif
#ifdef USE_UV
    case RIG_SLAVE_CONNECT_MODE_TCP:
        bind_to_tcp_socket(slave);

#ifdef USE_AVAHI
        rig_avahi_register_service(engine);
#endif
        break;
#endif
    }
}

static void
rig_slave_fini(rut_shell_t *shell, void *user_data)
{
    rig_slave_t *slave = user_data;
    rig_engine_t *engine = slave->engine;
    rut_queue_item_t *item;

    rut_shell_ungrab_input(shell, slave_grab_input_cb, slave);

    slave_stop_service(slave);

#ifdef USE_AVAHI
    /* TODO: move to frontend */
    rig_avahi_unregister_service(engine);
#endif

    if (slave->ui_update_closure) {
        rut_closure_disconnect_FIXME(slave->ui_update_closure);
        slave->ui_update_closure = NULL;
    }

    c_list_for_each(item, &slave->pending_edits->items, list_node)
    c_slice_free(pending_edit_t, item->data);
    rut_queue_free(slave->pending_edits);

    rig_engine_op_map_context_destroy(&slave->map_op_ctx);
    rig_engine_op_apply_context_destroy(&slave->apply_op_ctx);

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

    rig_engine_update_timelines(engine);

    rut_shell_run_pre_paint_callbacks(shell);

    rut_shell_run_start_paint_callbacks(shell);

    rig_engine_paint(engine);

    rut_shell_run_post_paint_callbacks(shell);

    rig_engine_garbage_collect(engine);

    rut_memory_stack_rewind(engine->frame_stack);

    rut_shell_end_redraw(shell);

    /* XXX: It would probably be better if we could send multiple
     * Rig__UIEdits in one go when setting up a simulator frame so we
     * wouldn't need to use this trick of continuously queuing redraws
     * to flush the edits through to the simulator.
     */
    if (rig_engine_check_timelines(engine) || slave->pending_edits->len) {
        rut_shell_queue_redraw(shell);
    }
}

static void
_rig_slave_free(void *object)
{
    rig_slave_t *slave = object;

    if (slave->frontend)
        rut_object_unref(slave->frontend);

    rut_object_unref(slave->shell);
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

    slave->shell = rut_shell_new(rig_slave_paint,
                                 slave);

    rut_shell_set_on_run_callback(slave->shell,
                                  rig_slave_init,
                                  slave);
    rut_shell_set_on_quit_callback(slave->shell,
                                   rig_slave_fini,
                                   slave);

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

    c_debug("  [%llx] -> [%50s]\n", *(uint64_t *)key, obj);

    c_free(obj);
}

static void
print_obj_to_id_mapping_cb(void *key, void *value, void *user_data)
{
    char *obj = rig_engine_get_object_debug_name(key);

    c_debug("  [%50s] -> [%llx]\n", obj, *(uint64_t *)key);

    c_free(obj);
}

void
rig_slave_print_mappings(rig_slave_t *slave)
{
    c_debug("Edit ID to play object mappings:\n");
    c_hash_table_foreach(
        slave->edit_id_to_play_object_map, print_id_to_obj_mapping_cb, NULL);

    c_debug("\n\n");
    c_debug("Play object to edit ID mappings:\n");
    c_hash_table_foreach(
        slave->play_object_to_edit_id_map, print_obj_to_id_mapping_cb, NULL);
}
