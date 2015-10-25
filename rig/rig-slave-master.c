/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013 Intel Corporation.
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
 *
 */

#include <rig-config.h>

#include <clib.h>

#include <rut.h>

#include "protobuf-c-rpc/rig-protobuf-c-rpc.h"
#include "protobuf-c-rpc/rig-protobuf-c-stream.h"

#include "rig-slave-master.h"
#include "rig-slave-address.h"
#include "rig-rpc-network.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

static void
handle_load_response(const Rig__LoadResult *result,
                     void *closure_data)
{
    c_debug("UI loaded by slave\n");
}

static void
master_peer_connected(rig_pb_rpc_client_t *pb_client, void *user_data)
{
    rig_slave_master_t *master = user_data;

    master->connected = true;

    rut_closure_list_invoke(&master->on_connect_closures,
                            rig_slave_master_connected_func_t,
                            master);

    rig_slave_master_reload_ui(master);

    c_debug("XXXXXXXXXXXX Slave Connected and serialized UI sent!");
}

static void
master_stop_service(rig_slave_master_t *master)
{
    rut_object_unref(master->peer);
    master->peer = NULL;
    rut_object_unref(master->stream);
    master->stream = NULL;

    master->connected = false;

    rut_closure_list_invoke(&master->on_error_closures,
                            rig_slave_master_error_func_t,
                            master);
}

static void
master_peer_error_handler(rig_pb_rpc_error_code_t code,
                          const char *message,
                          void *user_data)
{
    rig_slave_master_t *master = user_data;

    c_warning("Slave connection error: %s", message);

    master_stop_service(master);
}

static void
master__forward_log(Rig__SlaveMaster_Service *service,
                    const Rig__Log *pb_log,
                    Rig__LogAck_Closure closure,
                    void *closure_data)
{
    Rig__LogAck ack = RIG__LOG_ACK__INIT;
    //rig_slave_master_t *master =
    //    rig_pb_rpc_closure_get_connection_data(closure_data);

#warning "TODO: handle logs forwarded from a slave device"

    closure(&ack, closure_data);
}

static Rig__SlaveMaster_Service rig_slave_master_service =
    RIG__SLAVE_MASTER__INIT(master__);

static void
_rig_slave_master_free(void *object)
{
    rig_slave_master_t *master = object;

    master_stop_service(master);

    rut_closure_list_disconnect_all_FIXME(&master->on_connect_closures);
    rut_closure_list_disconnect_all_FIXME(&master->on_error_closures);

    rut_object_free(rig_slave_master_t, master);
}

static rut_type_t rig_slave_master_type;

static void
_rig_slave_master_init_type(void)
{
    rut_type_init(
        &rig_slave_master_type, C_STRINGIFY(TYPE), _rig_slave_master_free);
}

static void
start_connect_on_idle_cb(void *user_data)
{
    rig_slave_master_t *master = user_data;
    int fd;

    rut_poll_shell_remove_idle_FIXME(master->engine->shell, master->connect_idle);
    master->connect_idle = NULL;

    c_return_if_fail(master->stream == NULL);
    c_return_if_fail(master->peer == NULL);

    switch(master->slave_address->type)
    {
    case RIG_SLAVE_ADDRESS_TYPE_TCP:
        master->stream = rig_pb_stream_new(master->engine->shell);
        rig_pb_stream_set_tcp_transport(master->stream,
                                        master->slave_address->tcp.hostname,
                                        master->slave_address->tcp.port);
        break;
    case RIG_SLAVE_ADDRESS_TYPE_ADB_SERIAL:
        master->stream = rig_pb_stream_new(master->engine->shell);
        rig_pb_stream_set_tcp_transport(master->stream,
                                        "127.0.0.1",
                                        master->slave_address->adb.port);
        break;
    case RIG_SLAVE_ADDRESS_TYPE_ABSTRACT:
        fd = rut_os_connect_to_abstract_socket(master->slave_address->abstract.socket_name);
        if (fd != -1) {
            master->stream = rig_pb_stream_new(master->engine->shell);
            rig_pb_stream_set_fd_transport(master->stream, fd);
        }
        break;
    }

    if (master->stream) {
        master->peer = rig_rpc_peer_new(
            master->stream,
            &rig_slave_master_service.base,
            (ProtobufCServiceDescriptor *)&rig__slave__descriptor,
            master_peer_error_handler,
            master_peer_connected,
            master);
    }
}

rig_slave_master_t *
rig_slave_master_new(rig_engine_t *engine, rig_slave_address_t *slave_address)
{
    rig_slave_master_t *master = rut_object_alloc0(rig_slave_master_t,
                                                   &rig_slave_master_type,
                                                   _rig_slave_master_init_type);

    master->engine = engine;

    c_list_init(&master->on_connect_closures);
    c_list_init(&master->on_error_closures);

    master->slave_address = rut_object_ref(slave_address);

    if (slave_address->type == RIG_SLAVE_ADDRESS_TYPE_ADB_SERIAL) {
        rut_exception_t *catch = NULL;
        struct timespec spec;

        if (!rut_adb_run_shell_cmd(slave_address->adb.serial,
                                   &catch,
                                   "shell:am force-stop org.rig.app")) {
            c_warning(
                "Failed to force stop of Rig slave application on Android "
                "device %s",
                slave_address->adb.serial);
            rut_exception_free(catch);
            catch = NULL;
        }

        if (!rut_adb_run_shell_cmd(slave_address->adb.serial,
                                   &catch,
                                   "shell:am start -n "
                                   "org.rig.app/org.rig.app.rig_slave_t")) {
            c_warning("Failed to start Rig slave application on Android "
                      "device %s",
                      slave_address->adb.serial);
            rut_exception_free(catch);
            catch = NULL;
        }

        /* Give the app a bit of time to start before trying to connect... */
        /* FIXME: don't sleep synchronously here!
         * TODO: implement a rut_poll_shell_add_timeout api
         */
        spec.tv_sec = 0;
        spec.tv_nsec = 500000000;
        nanosleep(&spec, NULL);
    }

    /* Defer starting to connect to an idle handler to give an
     * opportunity to register on_connect and on_error callbacks
     */
    master->connect_idle =
        rut_poll_shell_add_idle_FIXME(engine->shell,
                                start_connect_on_idle_cb,
                                master,
                                NULL); /* destroy */

    return master;
}

rut_closure_t *
rig_slave_master_add_on_connect_callback(rig_slave_master_t *master,
                                         rig_slave_master_connected_func_t callback,
                                         void *user_data,
                                         rut_closure_destroy_callback_t destroy)
{
    return rut_closure_list_add_FIXME(&master->on_connect_closures,
                                callback,
                                user_data,
                                destroy);
}

rut_closure_t *
rig_slave_master_add_on_error_callback(rig_slave_master_t *master,
                                       rig_slave_master_error_func_t callback,
                                       void *user_data,
                                       rut_closure_destroy_callback_t destroy)
{
    return rut_closure_list_add_FIXME(&master->on_error_closures,
                                callback,
                                user_data,
                                destroy);
}

void
rig_slave_master_reload_ui(rig_slave_master_t *master)
{
    rig_pb_serializer_t *serializer;
    rig_engine_t *engine = master->engine;
    ProtobufCService *service =
        rig_pb_rpc_client_get_service(master->peer->pb_rpc_client);
    Rig__UI *pb_ui;

    if (!master->connected)
        return;

    serializer = rig_pb_serializer_new(engine);

    rig_pb_serializer_set_use_pointer_ids_enabled(serializer, true);

    /* NB: We always use the edit-mode-ui as the basis for any ui sent
     * to a slave device so that the slave device can maintain a mapping
     * from edit-mode IDs to its play-mode IDs so that we can handle
     * edit operations in the slave.
     */
    pb_ui = rig_pb_serialize_ui(serializer, true, engine->edit_mode_ui);

    rig__slave__load(service, pb_ui, handle_load_response, NULL);

    rig_pb_serialized_ui_destroy(pb_ui);

    rig_pb_serializer_destroy(serializer);
}

static void
handle_edit_response(const Rig__UIEditResult *result,
                     void *closure_data)
{
    c_debug("UI edited by slave\n");
}

void
rig_slave_master_forward_pb_ui_edit(rig_slave_master_t *master,
                                    Rig__UIEdit *pb_ui_edit)
{
    ProtobufCService *service =
        rig_pb_rpc_client_get_service(master->peer->pb_rpc_client);

    if (!master->connected)
        return;

    rig__slave__edit(service, pb_ui_edit, handle_edit_response, NULL);
}
