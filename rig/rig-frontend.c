/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation.
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
#include <sys/socket.h>

#ifdef __linux__
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#endif

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-frontend.h"
#include "rig-renderer.h"
#include "rig-load-save.h"
#include "rig-pb.h"
#include "rig-logs.h"

#include "rig.pb-c.h"

static void spawn_simulator(rut_shell_t *shell,
                            rig_frontend_t *frontend,
                            const char *ui_filename);

/* Common frontend options, either set via environment variables or
 * command line options...
 */

enum rig_simulator_run_mode rig_simulator_run_mode_option;
const char *rig_simulator_address_option;
int rig_simulator_port_option;


static void
frontend__test(Rig__Frontend_Service *service,
               const Rig__Query *query,
               Rig__TestResult_Closure closure,
               void *closure_data)
{
    Rig__TestResult result = RIG__TEST_RESULT__INIT;
    // rig_frontend_t *frontend = rig_pb_rpc_closure_get_connection_data
    // (closure_data);

    c_return_if_fail(query != NULL);

    // c_debug ("Frontend Service: Test Query\n");

    closure(&result, closure_data);
}

static void
frontend__forward_log(Rig__Frontend_Service *service,
                      const Rig__Log *log,
                      Rig__LogAck_Closure closure,
                      void *closure_data)
{
    Rig__LogAck ack = RIG__LOG_ACK__INIT;
    int i;

    c_return_if_fail(log != NULL);

    for (i = 0; i < log->n_entries; i++)
        rig_logs_pb_log(log->type, log->entries[i]);

    closure(&ack, closure_data);
}

static rut_magazine_t *_rig_frontend_object_id_magazine = NULL;

static void
free_object_id(void *id)
{
    rut_magazine_chunk_free(_rig_frontend_object_id_magazine, id);
}

static void *
frontend_lookup_object(rig_frontend_t *frontend, uint64_t id)
{
    void *id_ptr = &id;

    /* There's no need for temporary IDs with a 'master simulator'
     * that can directly generate canonical IDs */
    c_return_val_if_fail(!(id & 1), NULL);

    return c_hash_table_lookup(frontend->id_to_object_map, id_ptr);
}

static uint64_t
frontend_map_simulator_id_to_object_cb(uint64_t id, void *user_data)
{
    rig_frontend_t *frontend = user_data;
    void *object = frontend_lookup_object(frontend, id);
    return (uint64_t)(uintptr_t)object;
}

static void
frontend_register_object_cb(void *object, uint64_t id, void *user_data)
{
    rig_frontend_t *frontend = user_data;
    uint64_t *id_ptr;

    /* There's no need for temporary IDs with a 'master simulator'
     * that can directly generate canonical IDs */
    c_return_if_fail(!(id & 1));

    id_ptr = rut_magazine_chunk_alloc(_rig_frontend_object_id_magazine);
    *id_ptr = id;

    c_hash_table_insert(frontend->object_to_id_map, object, id_ptr);
    c_hash_table_insert(frontend->id_to_object_map, id_ptr, object);
}

static void
frontend_unregister_object_cb(void *object, void *user_data)
{
    rig_frontend_t *frontend = user_data;
    void *id_ptr = c_hash_table_remove_value(frontend->object_to_id_map, object);

    if (id_ptr)
        c_hash_table_remove(frontend->id_to_object_map, id_ptr);
}

static void
print_id_to_obj_mapping_cb(void *key, void *value, void *user_data)
{
    uint64_t *id_ptr = key;
    char *obj = rig_engine_get_object_debug_name(value);

    c_debug("  [%" PRIx64 "] -> [%50s]\n", (uint64_t)*id_ptr, obj);

    c_free(obj);
}

static void
print_obj_to_id_mapping_cb(void *key, void *value, void *user_data)
{
    char *obj = rig_engine_get_object_debug_name(key);
    uint64_t *id_ptr = value;

    c_debug("  [%50s] -> [%" PRIx64 "]\n", obj, (uint64_t)*id_ptr);

    c_free(obj);
}

void
rig_frontend_print_mappings(rig_frontend_t *frontend)
{
    c_debug("ID to object map:\n");
    c_hash_table_foreach(
        frontend->id_to_object_map, print_id_to_obj_mapping_cb, NULL);

    c_debug("\n\n");
    c_debug("Object to ID map:\n");
    c_hash_table_foreach(
        frontend->object_to_id_map, print_obj_to_id_mapping_cb, NULL);
}

void
rig_frontend_garbage_collect_cb(void *object, void *user_data)
{
    frontend_unregister_object_cb(object, user_data);
}

static void *
lookup_object_cb(uint64_t id, void *user_data)
{
    rig_frontend_t *frontend = user_data;
    return frontend_lookup_object(frontend, id);
}

static void
apply_property_change(rig_frontend_t *frontend,
                      rig_pb_un_serializer_t *unserializer,
                      Rig__PropertyChange *pb_change)
{
    void *object;
    rut_property_t *property;
    rut_boxed_t boxed;

    if (!pb_change->has_object_id || pb_change->object_id == 0 ||
        !pb_change->has_property_id || !pb_change->value) {
        c_warning("Frontend: Invalid property change received");
        return;
    }

    object = frontend_lookup_object(frontend, pb_change->object_id);

#if 0
    c_debug ("Frontend: PropertyChange: %p(%s) prop_id=%d\n",
             object,
             rut_object_get_type_name (object),
             pb_change->property_id);
#endif

    property = rut_introspectable_get_property(object, pb_change->property_id);
    if (!property) {
        c_warning("Frontend: Failed to find object property by id");
        return;
    }

    /* XXX: ideally we shouldn't need to init a rut_boxed_t and set
     * that on a property, and instead we can just directly
     * apply the value to the property we have. */
    rig_pb_init_boxed_value(
        unserializer, &boxed, property->spec->type, pb_change->value);

    //#warning "XXX: frontend updates are disabled"
    rut_property_set_boxed(
        &frontend->engine->shell->property_ctx, property, &boxed);
}

static void
frontend__update_ui(Rig__Frontend_Service *service,
                    const Rig__UIDiff *pb_ui_diff,
                    Rig__UpdateUIAck_Closure closure,
                    void *closure_data)
{
    Rig__UpdateUIAck ack = RIG__UPDATE_UIACK__INIT;
    rig_frontend_t *frontend =
        rig_pb_rpc_closure_get_connection_data(closure_data);
    rig_engine_t *engine = frontend->engine;
    int i, j;
    int n_property_changes;
    rig_pb_un_serializer_t *unserializer;
    rig_engine_op_map_context_t *map_to_frontend_objects_op_ctx;
    rig_engine_op_apply_context_t *apply_op_ctx;
    Rig__UIEdit *pb_ui_edit;

#if 0
    frontend->ui_update_pending = false;

    closure (&ack, closure_data);

    /* XXX: The current use case we have forui update callbacks
     * requires that the frontend be in-sync with the simulator so
     * we invoke them after we have applied all the operations from
     * the simulator */
    rut_closure_list_invoke (&frontend->ui_update_cb_list,
                             rig_frontend_ui_update_callback_t,
                             frontend);
    return;
#endif
    // c_debug ("Frontend: Update UI Request\n");

    frontend->ui_update_pending = false;

    c_return_if_fail(pb_ui_diff != NULL);

    n_property_changes = pb_ui_diff->n_property_changes;

    map_to_frontend_objects_op_ctx = &frontend->map_to_frontend_objects_op_ctx;
    apply_op_ctx = &frontend->apply_op_ctx;
    unserializer = frontend->prop_change_unserializer;

    pb_ui_edit = pb_ui_diff->edit;

    /* For compactness, property changes are serialized separately from
     * more general UI edit operations and so we need to take care that
     * we apply property changes and edit operations in the correct
     * order, using the operation sequences to relate to the sequence
     * of property changes.
     */
    j = 0;
    if (pb_ui_edit) {
        for (i = 0; i < pb_ui_edit->n_ops; i++) {
            Rig__Operation *pb_op = pb_ui_edit->ops[i];
            int until = pb_op->sequence;

            for (; j < until; j++) {
                Rig__PropertyChange *pb_change =
                    pb_ui_diff->property_changes[j];
                apply_property_change(frontend, unserializer, pb_change);
            }

            if (!rig_engine_pb_op_map(map_to_frontend_objects_op_ctx,
                                      apply_op_ctx, pb_op)) {
                c_warning("Frontend: Failed to ID map simulator operation");
                continue;
            }
        }
    }

    for (; j < n_property_changes; j++) {
        Rig__PropertyChange *pb_change = pb_ui_diff->property_changes[j];
        apply_property_change(frontend, unserializer, pb_change);
    }

#if 0
    n_actions = pb_ui_diff->n_actions;
    for (i = 0; i < n_actions; i++) {
        Rig__SimulatorAction *pb_action = pb_ui_diff->actions[i];
        switch (pb_action->type) {
        }
    }
#endif

    if (pb_ui_diff->has_queue_frame)
        rut_shell_queue_redraw(engine->shell);

    closure(&ack, closure_data);

    /* XXX: The current use case we have forui update callbacks
     * requires that the frontend be in-sync with the simulator so
     * we invoke them after we have applied all the operations from
     * the simulator */
    rut_closure_list_invoke(&frontend->ui_update_cb_list,
                            rig_frontend_ui_update_callback_t,
                            frontend);
}

static void
frontend__load(Rig__Frontend_Service *service,
               const Rig__UI *pb_ui,
               Rig__LoadResult_Closure closure,
               void *closure_data)
{
    Rig__LoadResult result = RIG__LOAD_RESULT__INIT;
    rig_frontend_t *frontend =
        rig_pb_rpc_closure_get_connection_data(closure_data);
    rig_engine_t *engine = frontend->engine;
    rig_ui_t *ui;
    c_llist_t *l;

    c_return_if_fail(pb_ui != NULL);

    /* First make sure to cleanup the current ui  */
    rig_engine_set_ui(engine, NULL);

    /* Kick garbage collection now so that all the objects being
     * replaced are unregistered before before we load the new UI.
     */
    rig_engine_garbage_collect(engine);

    ui = rig_pb_unserialize_ui(frontend->ui_unserializer, pb_ui);

    rig_engine_set_ui(engine, ui);

    rig_ui_code_modules_load(ui);

    for (l = frontend->camera_views; l; l = l->next) {
        rig_camera_view_t *camera_view = l->data;

        rig_camera_view_set_ui(camera_view, ui);
    }

    rut_object_unref(ui);

    rig_engine_op_apply_context_set_ui(&frontend->apply_op_ctx, ui);

    rut_shell_queue_redraw(engine->shell);

    closure(&result, closure_data);

    /* XXX: The current use case we have forui update callbacks
     * requires that the frontend be in-sync with the simulator so
     * we invoke them after we have applied all the operations from
     * the simulator */
    rut_closure_list_invoke(&frontend->ui_update_cb_list,
                            rig_frontend_ui_update_callback_t,
                            frontend);
}


static Rig__Frontend_Service rig_frontend_service =
    RIG__FRONTEND__INIT(frontend__);

#if 0
static void
handle_simulator_test_response (const Rig__TestResult *result,
                                void *closure_data)
{
    c_debug ("Simulator test response received\n");
}
#endif

static void
frontend_peer_connected(rig_pb_rpc_client_t *pb_client,
                        void *user_data)
{
    rig_frontend_t *frontend = user_data;

    frontend->connected = true;

    if (frontend->simulator_connected_callback) {
        void *user_data = frontend->simulator_connected_data;
        frontend->simulator_connected_callback(user_data);
    }

#if 0
    Rig__Query query = RIG__QUERY__INIT;


    rig__simulator__test (simulator_service, &query,
                          handle_simulator_test_response, NULL);
#endif

    // c_debug ("Frontend peer connected\n");
}

static void
frontend_stop_service(rig_frontend_t *frontend)
{
    rig_rpc_peer_t *peer = frontend->frontend_peer;

    if (!peer)
        return;

    /* Avoid recursion, in case freeing the peer results in any
     * callbacks that also try to stop the frontend service.
     *
     * XXX: maybe we should update the rpc layer to avoid synchronous
     * callbacks and instead always defer work to the mainloop.
     */
    frontend->frontend_peer = NULL;

    rut_object_unref(peer);

    rut_object_unref(frontend->stream);
    frontend->stream = NULL;

    frontend->connected = false;
    frontend->ui_update_pending = false;
}

static void
frontend_peer_error_handler(rig_pb_rpc_error_code_t code,
                            const char *message,
                            void *user_data)
{
    rig_frontend_t *frontend = user_data;

    c_warning("Frontend peer error: %s", message);

    frontend_stop_service(frontend);
}

static void
frontend_start_service(rut_shell_t *shell,
                       rig_frontend_t *frontend,
                       rig_pb_stream_t *stream)
{
    frontend->stream = rut_object_ref(stream);
    frontend->frontend_peer = rig_rpc_peer_new(
        stream,
        &rig_frontend_service.base,
        (ProtobufCServiceDescriptor *)&rig__simulator__descriptor,
        frontend_peer_error_handler,
        frontend_peer_connected,
        frontend);
}

void
rig_frontend_set_simulator_connected_callback(rig_frontend_t *frontend,
                                              void (*callback)(void *user_data),
                                              void *user_data)
{
    frontend->simulator_connected_callback = callback;
    frontend->simulator_connected_data = user_data;
}

/* TODO: should support a destroy_notify callback */
void
rig_frontend_sync(rig_frontend_t *frontend,
                  void (*synchronized)(const Rig__SyncAck *result,
                                       void *user_data),
                  void *user_data)
{
    ProtobufCService *simulator_service;
    Rig__Sync sync = RIG__SYNC__INIT;

    if (!frontend->connected)
        return;

    simulator_service =
        rig_pb_rpc_client_get_service(frontend->frontend_peer->pb_rpc_client);

    rig__simulator__synchronize(
        simulator_service, &sync, synchronized, user_data);
}

static void
frame_running_ack(const Rig__RunFrameAck *ack, void *closure_data)
{
    //rig_frontend_t *frontend = closure_data;
    //rig_engine_t *engine = frontend->engine;

    // c_debug ("Frontend: Run Frame ACK received from simulator\n");
}

void
rig_frontend_run_simulator_frame(rig_frontend_t *frontend,
                                 rig_pb_serializer_t *serializer,
                                 Rig__FrameSetup *setup)
{
    rig_engine_t *engine = frontend->engine;
    ProtobufCService *simulator_service;

    if (!frontend->connected)
        return;

    simulator_service =
        rig_pb_rpc_client_get_service(frontend->frontend_peer->pb_rpc_client);

    if (frontend->pending_dso_data) {
        setup->has_dso = true;
        setup->dso.len = frontend->pending_dso_len;
        setup->dso.data = rut_memory_stack_memalign(engine->frame_stack,
                                                    frontend->pending_dso_len,
                                                    C_ALIGNOF(uint8_t));
        memcpy(setup->dso.data, frontend->pending_dso_data, setup->dso.len);
    }

    rig__simulator__run_frame(
        simulator_service, setup, frame_running_ack, frontend); /* user data */

    frontend->ui_update_pending = true;

    if (frontend->pending_dso_data) {
        c_free(frontend->pending_dso_data);
        frontend->pending_dso_data = NULL;
    }
}

static void
_rig_frontend_free(void *object)
{
    rig_frontend_t *frontend = object;
    c_llist_t *l;

#ifdef __APPLE__
    rig_osx_deinit(frontend->engine);
#endif

    if (frontend->pending_dso_data)
        c_free(frontend->pending_dso_data);

    rig_engine_op_apply_context_destroy(&frontend->apply_op_ctx);
    rig_engine_op_map_context_destroy(&frontend->map_to_frontend_objects_op_ctx);
    rig_pb_unserializer_destroy(frontend->prop_change_unserializer);

    rut_closure_list_disconnect_all(&frontend->ui_update_cb_list);

    frontend_stop_service(frontend);

    for (l = frontend->camera_views; l; l = l->next)
        rut_object_unref(l->data);

    c_llist_free(frontend->camera_views);
    frontend->camera_views = NULL;

    rig_renderer_fini(frontend->renderer);
    rut_object_unref(frontend->renderer);

    _rig_destroy_image_source_wrappers(frontend);

    cg_object_unref(frontend->circle_node_attribute);

    cg_object_unref(frontend->default_pipeline);

    cg_object_unref(frontend->default_tex2d);
    cg_object_unref(frontend->default_tex2d_pipeline);

    rut_object_unref(frontend->engine);

    rut_object_free(rig_frontend_t, object);
}

rut_type_t rig_frontend_type;

static void
_rig_frontend_init_type(void)
{
    rut_type_init(&rig_frontend_type, "rig_frontend_t", _rig_frontend_free);
}

#ifdef RIG_SUPPORT_SIMULATOR_PROCESS

static void
simulator_sigchild_cb(void *user_data)
{
    rig_frontend_t *frontend = user_data;
    rig_engine_t *engine = frontend->engine;
    rut_shell_t *shell = engine->shell;
    pid_t pid;
    int status;

    pid = waitpid(frontend->simulator_pid, &status, WNOHANG);
    if (pid < 0) {
        c_error("Error checking simulator child status: %s", strerror(errno));
        return;
    }
    if (pid != frontend->simulator_pid)
        return;

    frontend_stop_service(frontend);

    c_debug("SIGCHLD received: Simulator Gone!");

    rut_shell_quit(shell);
}

static void
fork_simulator(rut_shell_t *shell,
               rig_frontend_t *frontend,
               const char *ui_filename)
{
    pid_t pid;
    int sp[2];
    rig_pb_stream_t *stream;

    c_return_if_fail(frontend->connected == false);

    /*
     * Spawn a simulator process...
     */

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0)
        c_error("Failed to open simulator ipc");

    pid = fork();
    if (pid == 0) {
        char fd_str[10];
        char *path = RIG_BIN_DIR "rig-simulator";

        /* child - simulator process */
        close(sp[0]);

        if (snprintf(fd_str, sizeof(fd_str), "%d", sp[1]) >= sizeof(fd_str))
            c_error("Failed to setup environment for simulator process");

        setenv("_RIG_IPC_FD", fd_str, true);

        if (getenv("RIG_SIMULATOR"))
            path = getenv("RIG_SIMULATOR");

#ifdef RIG_ENABLE_DEBUG
        if (execlp("libtool", "libtool", "e", path, ui_filename, NULL) < 0)
            c_error("Failed to run simulator process via libtool");

#if 0
        if (execlp ("libtool", "libtool", "e", "valgrind", path, NULL))
            g_error ("Failed to run simulator process under valgrind");
#endif

#else
        if (execl(path, path, ui_filename, NULL) < 0)
            c_error("Failed to run simulator process");
#endif

        return;
    }

    frontend->simulator_pid = pid;

    rut_poll_shell_add_sigchild(shell, simulator_sigchild_cb,
                                frontend,
                                NULL); /* destroy notify */

    stream = rig_pb_stream_new(shell);
    rig_pb_stream_set_fd_transport(stream, sp[0]);

    frontend_start_service(shell, frontend, stream);

    /* frontend_start_service will take ownership of the stream */
    rut_object_unref (stream);
}
#endif /* RIG_SUPPORT_SIMULATOR_PROCESS */

#ifdef C_SUPPORTS_THREADS

typedef struct _thread_state_t {
#ifdef C_HAVE_PTHREADS
    pthread_t thread_id;
    const char *name;
    void (*start)(void *start_data);
    void *start_data;
#elif defined(USE_UV)
    uv_thread_t thread_id;
#endif

    rig_frontend_t *frontend;
    char *ui_filename;
    int fd;
} thread_state_t;

static void
run_simulator_thread(void *user_data)
{
    thread_state_t *state = user_data;
    rig_simulator_t *simulator = rig_simulator_new(NULL, state->ui_filename);

    rig_simulator_set_frontend_fd(simulator, state->fd);

#ifdef USE_GLIB
    g_main_context_push_thread_default(g_main_context_new());
#endif

    rig_simulator_run(simulator);

    rut_object_unref(simulator);
}

#ifdef C_HAVE_PTHREADS
static void *
start_thread_cb(void *start_data)
{
    thread_state_t *state = start_data;

    pthread_setname_np(state->thread_id, state->name);

    state->start(state->start_data);

    return NULL;
}

static bool
create_posix_thread(thread_state_t *state,
                    const char *name,
                    void (*start)(void *),
                    void *start_data)
{
    int ret;
    pthread_attr_t attr;

    state->name = name;
    state->start = start;
    state->start_data = start_data;

    pthread_attr_init(&attr);
    ret = pthread_create(&state->thread_id, &attr,
                         start_thread_cb, state);
    if (ret != 0)
        c_error("%s", "Failed to spawn simulator thread: %s",
                strerror(errno));

    pthread_attr_destroy(&attr);

    return ret == 0;
}
#endif /* C_HAVE_PTHREADS */

static thread_state_t *
create_simulator_thread(rut_shell_t *shell,
                        rig_frontend_t *frontend,
                        const char *ui_filename)
{
    thread_state_t *state = c_new0(thread_state_t, 1);
    rig_pb_stream_t *stream;
    int sp[2];

    c_return_val_if_fail(frontend->connected == false, NULL);

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0)
        c_error("Failed to open simulator ipc socketpair");

    state->frontend = frontend;
    state->fd = sp[1];
    state->ui_filename = c_strdup(ui_filename);

#ifdef C_HAVE_PTHREADS
    if (!create_posix_thread(state,
                             "Simulator",
                             run_simulator_thread,
                             state))
        return NULL;
#elif defined(USE_UV)
    r = uv_thread_create(&state->thread_id, run_simulator_thread, state);
    if (r) {
        c_error("Failed to spawn simulator thread");
        return NULL;
    }
#else
#error "Missing platform api to create a thread"
#endif

    stream = rig_pb_stream_new(shell);
    rig_pb_stream_set_fd_transport(stream, sp[0]);

    frontend_start_service(shell, frontend, stream);

    /* frontend_start_service will take ownership of the stream */
    rut_object_unref(stream);

    return state;
}

#endif /* C_SUPPORTS_THREADS */

#ifdef __linux__
static void
handle_simulator_connect_cb(void *user_data, int listen_fd, int revents)
{
    rig_frontend_t *frontend = user_data;
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);
    int fd;

    c_return_if_fail(revents & RUT_POLL_FD_EVENT_IN);

    c_message("Simulator connect request received!");

    if (frontend->connected) {
        c_warning("Ignoring simulator connection while there's already one connected");
        return;
    }

    fd = accept(frontend->listen_fd, &addr, &addr_len);
    if (fd != -1) {
        rig_pb_stream_t *stream = rig_pb_stream_new(frontend->engine->shell);

        rig_pb_stream_set_fd_transport(stream, fd);

        c_message("Simulator connected!");

        frontend_start_service(frontend->engine->shell, frontend, stream);

        /* frontend_start_service will take ownership of the stream */
        rut_object_unref(stream);
    } else
        c_message("Failed to accept simulator connection: %s!",
                  strerror(errno));
}

static bool
bind_to_abstract_socket(rut_shell_t *shell, rig_frontend_t *frontend)
{
    rut_exception_t *catch = NULL;
    int fd = rut_os_listen_on_abstract_socket(rig_simulator_address_option,
                                              &catch);

    if (fd < 0) {
        c_critical("Failed to listen on abstract \"rig-simulator\" socket: %s",
                   catch->message);
        return false;
    }

    frontend->listen_fd = fd;

    rut_poll_shell_add_fd(shell,
                          frontend->listen_fd,
                          RUT_POLL_FD_EVENT_IN,
                          NULL, /* prepare */
                          handle_simulator_connect_cb, /* dispatch */
                          frontend);

    c_message("Waiting for simulator to connect to abstract socket \"%s\"...",
              rig_simulator_address_option);

    return true;
}

static void
connect_to_abstract_socket(rut_shell_t *shell, rig_frontend_t *frontend)
{
    rig_pb_stream_t *stream = rig_pb_stream_new(shell);
    int fd;

    while ((fd = rut_os_connect_to_abstract_socket(rig_simulator_address_option)) == -1) {
        static bool seen = false;
        if (seen)
            c_message("Waiting for simulator...");
        else
            seen = true;

        sleep(2);
    }

    rig_pb_stream_set_fd_transport(stream, fd);

    frontend_start_service(shell, frontend, stream);

    /* frontend_start_service will take ownership of the stream */
    rut_object_unref(stream);
}
#endif /* linux */

#ifdef USE_UV
static void
handle_tcp_connect_cb(uv_stream_t *server, int status)
{
    rig_frontend_t *frontend = server->data;
    rig_pb_stream_t *stream;

    if (status != 0) {
        c_warning("Connection failure: %s", uv_strerror(status));
        return;
    }

    c_message("Simulator tcp connect request received!");

    if (frontend->connected) {
        c_warning("Ignoring simulator connection while there's already one connected");
        return;
    }

    stream = rig_pb_stream_new(frontend->engine->shell);
    rig_pb_stream_accept_tcp_connection(stream, &frontend->listening_socket);

    c_message("Simulator connected!");
    frontend_start_service(frontend->engine->shell, frontend, stream);

    /* frontend_start_service will take ownership of the stream */
    rut_object_unref(stream);
}

static void
bind_to_tcp_socket(rut_shell_t *shell, rig_frontend_t *frontend)
{
    uv_loop_t *loop = rut_uv_shell_get_loop(shell);
    struct sockaddr_in bind_addr;
    struct sockaddr name;
    int namelen;
    int err;

    uv_tcp_init(loop, &frontend->listening_socket);
    frontend->listening_socket.data = frontend;

    uv_ip4_addr(rig_simulator_address_option,
                rig_simulator_port_option, &bind_addr);
    uv_tcp_bind(&frontend->listening_socket, (struct sockaddr *)&bind_addr, 0);
    err = uv_listen((uv_stream_t*)&frontend->listening_socket,
                    128, handle_tcp_connect_cb);
    if (err < 0) {
        c_critical("Failed to starting listening for simulator connection: %s",
                   uv_strerror(err));
        return;
    }

    err = uv_tcp_getsockname(&frontend->listening_socket, &name, &namelen);
    if (err != 0) {
        c_critical("Failed to query peer address of listening tcp socket");
        return;
    } else {
        struct sockaddr_in *addr = (struct sockaddr_in *)&name;
        char ip_address[17] = {'\0'};

        c_return_if_fail(name.sa_family == AF_INET);

        uv_ip4_name(addr, ip_address, 16);
        frontend->listening_address = c_strdup(ip_address);
        frontend->listening_port = ntohs(addr->sin_port);
    }
}
#endif /* USE_UV */

static void
run_simulator_in_process(rut_shell_t *shell,
                         rig_frontend_t *frontend,
                         const char *ui_filename)
{
    rig_simulator_t *simulator = rig_simulator_new(shell, ui_filename);
    rig_pb_stream_t *stream;

    /* N.B. This won't block running the mainloop since rut-loop
     * will see that simulator->shell isn't the main shell. */
    rig_simulator_run(simulator);

    stream = rig_pb_stream_new(shell);

    frontend_start_service(shell, frontend, stream);

    /* frontend_start_service will take ownership of the stream */
    rut_object_unref(stream);

    rig_pb_stream_set_in_thread_direct_transport(stream,
                                                 simulator->stream);
    rig_pb_stream_set_in_thread_direct_transport(simulator->stream,
                                                 stream);
}

#ifdef __EMSCRIPTEN__
static void
spawn_web_worker(rut_shell_t *shell, rig_frontend_t *frontend)
{
    rig_pb_stream_t *stream = rig_pb_stream_new(shell);

    frontend->sim_worker = rig_emscripten_worker_create("rig-simulator-worker.js");

    frontend_start_service(shell, frontend, stream);

    /* frontend_start_service will take ownership of the stream */
    rut_object_unref(stream);

    rig_pb_stream_set_worker(stream, frontend->sim_worker);
}

static void
connect_to_websocket(rut_shell_t *shell, rig_frontend_t *frontend)
{
    rig_pb_stream_t *stream = rig_pb_stream_new(shell);
    struct sockaddr_in addr;
    int ret;

    frontend->sim_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (frontend->sim_fd == -1) {
        c_critical("Failed to create socket");
        return;
    }
    fcntl(frontend->sim_fd, F_SETFL, O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7890);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        c_critical("inet_pton failed");
        close(frontend->sim_fd);
        return;
    }

    ret = connect(frontend->sim_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1 && errno != EINPROGRESS) {
        c_critical("connect failed");
        close(frontend->sim_fd);
        return;
    }

    frontend_start_service(shell, frontend, stream);

    /* frontend_start_service will take ownership of the stream */
    rut_object_unref(stream);

    rig_pb_stream_set_websocket_client_fd(stream, frontend->sim_fd);
}
#endif

static void
spawn_simulator(rut_shell_t *shell,
                rig_frontend_t *frontend,
                const char *ui_filename)
{
    switch(rig_simulator_run_mode_option)
    {
    case RIG_SIMULATOR_RUN_MODE_MAINLOOP:
        run_simulator_in_process(shell, frontend, ui_filename);
        break;
#ifdef C_SUPPORTS_THREADS
    case RIG_SIMULATOR_RUN_MODE_THREADED:
        create_simulator_thread(shell, frontend, ui_filename);
        break;
#endif
#ifdef RIG_SUPPORT_SIMULATOR_PROCESS
    case RIG_SIMULATOR_RUN_MODE_PROCESS:
        fork_simulator(shell, frontend, ui_filename);
        break;
#endif
#ifdef __linux__
    case RIG_SIMULATOR_RUN_MODE_LISTEN_ABSTRACT_SOCKET:
        bind_to_abstract_socket(shell, frontend);
        break;
    case RIG_SIMULATOR_RUN_MODE_CONNECT_ABSTRACT_SOCKET:
        connect_to_abstract_socket(shell, frontend);
        break;
#endif
#ifdef USE_UV
    case RIG_SIMULATOR_RUN_MODE_LISTEN_TCP:
        bind_to_tcp_socket(shell, frontend);
        break;
    case RIG_SIMULATOR_RUN_MODE_CONNECT_TCP:
        c_error("TODO: support connecting to a simulator via tcp");
        break;
#endif
#ifdef __EMSCRIPTEN__
    case RIG_SIMULATOR_RUN_MODE_WEB_WORKER:
        spawn_web_worker(shell, frontend);
        break;
    case RIG_SIMULATOR_RUN_MODE_WEB_SOCKET:
        connect_to_websocket(shell, frontend);
        break;
#endif
    }
}

static void
on_onscreen_resize(cg_onscreen_t *onscreen,
                   int width,
                   int height,
                   void *user_data)
{
    rig_frontend_t *frontend = user_data;

    rut_shell_queue_redraw(frontend->engine->shell);
}

void
finish_ui_load(rig_engine_t *engine, rig_ui_t *ui)
{
    rig_engine_set_ui(engine, ui);

    rut_object_unref(ui);

    if (engine->ui_load_callback)
        engine->ui_load_callback(engine->ui_load_data);
}

static void
finish_ui_load_cb(rig_frontend_t *frontend, void *user_data)
{
    rig_ui_t *ui = user_data;
    rig_engine_t *engine = frontend->engine;

    rut_closure_disconnect(frontend->finish_ui_load_closure);
    frontend->finish_ui_load_closure = NULL;

    finish_ui_load(engine, ui);
}

void
rig_frontend_load_file(rig_frontend_t *frontend, const char *filename)
{
    rig_ui_t *ui = NULL;
    rig_engine_t *engine = frontend->engine;

    if (filename)
        ui = rig_load(engine, filename);

    if (!ui) {
        ui = rig_ui_new(engine);
        rig_ui_prepare(ui);
    }

    /*
     * Wait until the simulator is idle before swapping in a new UI...
     */
    if (!frontend->ui_update_pending)
        finish_ui_load(engine, ui);
    else {
        /* Throw away any outstanding closure since it is now redundant... */
        if (frontend->finish_ui_load_closure)
            rut_closure_disconnect(frontend->finish_ui_load_closure);

        frontend->finish_ui_load_closure =
            rig_frontend_add_ui_update_callback(engine->frontend,
                                                finish_ui_load_cb,
                                                ui,
                                                rut_object_unref); /* destroy */
    }
}

void
rig_frontend_post_init_engine(rig_frontend_t *frontend,
                              const char *ui_filename)
{
    rig_engine_t *engine = frontend->engine;
    rut_shell_t *shell = engine->shell;
    rut_shell_onscreen_t *onscreen;
    rig_camera_view_t *camera_view;
    uint8_t rgba_pre_white[] = { 0xff, 0xff, 0xff, 0xff };

    frontend->default_pipeline = cg_pipeline_new(shell->cg_device);

    frontend->default_tex2d = cg_texture_2d_new_from_data(shell->cg_device,
                                                          1, 1,
                                                          CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                                          sizeof(rgba_pre_white),
                                                          rgba_pre_white,
                                                          NULL);

    frontend->default_tex2d_pipeline = cg_pipeline_new(shell->cg_device);
    cg_pipeline_set_layer_texture(frontend->default_tex2d_pipeline, 0,
                                  frontend->default_tex2d);

    frontend->circle_node_attribute =
        rut_create_circle_fan_p2(engine->shell, 20,
                                 &frontend->circle_node_n_verts);

    _rig_init_image_source_wrappers_cache(frontend);

    frontend->renderer = rig_renderer_new(frontend);
    rig_renderer_init(frontend->renderer);

    onscreen = rut_shell_onscreen_new(engine->shell,
                                      engine->device_width / 2,
                                      engine->device_height / 2);

    rut_shell_onscreen_allocate(onscreen);

    rut_shell_onscreen_set_resizable(onscreen, true);
    cg_onscreen_add_resize_callback(onscreen->cg_onscreen,
                                    on_onscreen_resize, frontend,
                                    NULL); /* destroy notify */

    camera_view = rig_camera_view_new(frontend);
    rig_camera_view_set_framebuffer(camera_view, onscreen->cg_onscreen);

    frontend->camera_views = c_llist_prepend(frontend->camera_views,
                                             camera_view);

#ifdef USE_GTK
    {
        rig_application_t *application = rig_application_new(engine);

        gtk_init(NULL, NULL);

        /* We need to register the application before showing the onscreen
         * because we need to set the dbus paths before the window is
         * mapped. FIXME: Eventually it might be nice to delay creating
         * the windows until the ‘activate’ or ‘open’ signal is emitted so
         * that we can support the single process properly. In that case
         * we could let g_application_run handle the registration
         * itself */
        if (!g_application_register(G_APPLICATION(application),
                                    NULL, /* cancellable */
                                    NULL /* error */))
            /* Another instance of the application is already running */
            rut_shell_quit(shell);

        rig_application_add_onscreen(application, onscreen);
    }
#endif

#ifdef __APPLE__
    rig_osx_init(engine);
#endif

    rut_shell_onscreen_set_title(onscreen, "Rig " C_STRINGIFY(RIG_VERSION));

    rut_shell_onscreen_show(onscreen);

    spawn_simulator(shell, frontend, ui_filename);
}

rig_frontend_t *
rig_frontend_new(rut_shell_t *shell)
{
    rig_frontend_t *frontend = rut_object_alloc0(
        rig_frontend_t, &rig_frontend_type, _rig_frontend_init_type);
    rig_pb_un_serializer_t *unserializer;

    frontend->object_to_id_map = c_hash_table_new(NULL, /* direct hash */
                                                  NULL); /* direct key equal */
    frontend->id_to_object_map =
        c_hash_table_new_full(c_int64_hash,
                              c_int64_equal, /* key equal */
                              free_object_id, /* key destroy */
                              NULL); /* value destroy */

    c_list_init(&frontend->ui_update_cb_list);

    frontend->engine = rig_engine_new_for_frontend(shell, frontend);

    _rig_frontend_object_id_magazine = frontend->engine->object_id_magazine;

    frontend->engine->garbage_collect_callback = rig_frontend_garbage_collect_cb;
    frontend->engine->garbage_collect_data = frontend;

    rig_logs_set_frontend(frontend);

    /*
     * This unserializer is used to unserialize UIs in frontend__load
     * for example...
     */
    unserializer = rig_pb_unserializer_new(frontend->engine);
    rig_pb_unserializer_set_object_register_callback(
        unserializer, frontend_register_object_cb, frontend);
    rig_pb_unserializer_set_id_to_object_callback(
        unserializer, lookup_object_cb, frontend);
    frontend->ui_unserializer = unserializer;

    rig_engine_op_apply_context_init(&frontend->apply_op_ctx,
                                     frontend->engine,
                                     frontend_register_object_cb,
                                     NULL, /* unregister ID */
                                     NULL, /* id to object (TODO: remove unused) */
                                     frontend);

    rig_engine_op_map_context_init(&frontend->map_to_frontend_objects_op_ctx,
                                   frontend->engine,
                                   frontend_map_simulator_id_to_object_cb,
                                   frontend);

    unserializer = rig_pb_unserializer_new(frontend->engine);
    /* Just to make sure we don't mistakenly use this unserializer to
     * register any objects... */
    rig_pb_unserializer_set_object_register_callback(unserializer, NULL, NULL);
    rig_pb_unserializer_set_id_to_object_callback(
        unserializer, lookup_object_cb, frontend);
    frontend->prop_change_unserializer = unserializer;

    return frontend;
}

rut_closure_t *
rig_frontend_add_ui_update_callback(rig_frontend_t *frontend,
                                    rig_frontend_ui_update_callback_t callback,
                                    void *user_data,
                                    rut_closure_destroy_callback_t destroy)
{
    return rut_closure_list_add(
        &frontend->ui_update_cb_list, callback, user_data, destroy);
}

static void
queue_simulator_frame_cb(rig_frontend_t *frontend, void *user_data)
{
    rut_shell_queue_redraw(frontend->engine->shell);
}

/* Similar to rut_shell_queue_redraw() but for queuing a new simulator
 * frame. If the simulator is currently busy this waits until we
 * recieve an update from the simulator and then queues a redraw. */
void
rig_frontend_queue_simulator_frame(rig_frontend_t *frontend)
{
    if (!frontend->ui_update_pending)
        rut_shell_queue_redraw(frontend->engine->shell);
    else if (!frontend->simulator_queue_frame_closure) {
        frontend->simulator_queue_frame_closure =
            rig_frontend_add_ui_update_callback(frontend,
                                                queue_simulator_frame_cb,
                                                frontend,
                                                NULL); /* destroy */
    }
}

void
rig_frontend_update_simulator_dso(rig_frontend_t *frontend,
                                  uint8_t *dso,
                                  int len)
{
    if (frontend->pending_dso_data)
        c_free(frontend->pending_dso_data);

    frontend->pending_dso_data = dso;
    frontend->pending_dso_len = len;
}

void
rig_frontend_paint(rig_frontend_t *frontend)
{
    c_llist_t *l;

    for (l = frontend->camera_views; l; l = l->next) {
        rig_camera_view_t *camera_view = l->data;
        cg_framebuffer_t *fb = camera_view->fb;

        rig_camera_view_paint(camera_view, frontend->renderer);

        if (cg_is_onscreen(fb)) {
            if (frontend->swap_buffers_hook)
                frontend->swap_buffers_hook(fb, frontend->swap_buffers_hook_data);
            else
                cg_onscreen_swap_buffers(fb);
        }
    }
}


