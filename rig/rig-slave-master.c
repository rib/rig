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

#include <config.h>

#include <glib.h>

#include <rut.h>

#include "protobuf-c-rpc/rig-protobuf-c-rpc.h"

#include "rig-slave-master.h"
#include "rig-slave-address.h"
#include "rig-rpc-network.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

static void
handle_load_response (const Rig__LoadResult *result,
                      void *closure_data)
{
  g_print ("UI loaded by slave\n");
}

void
slave_master_connected (PB_RPC_Client *pb_client,
                        void *user_data)
{
  RigSlaveMaster *master = user_data;

  master->connected = true;

  rig_slave_master_reload_ui (master);

  g_print ("XXXXXXXXXXXX Slave Connected and serialized UI sent!");
}

static void
destroy_slave_master (RigSlaveMaster *master)
{
  RigEngine *engine = master->engine;

  if (!master->rpc_client)
    return;

  rig_rpc_client_disconnect (master->rpc_client);
  rut_object_unref (master->rpc_client);
  master->rpc_client = NULL;

  master->connected = FALSE;

  engine->slave_masters = g_list_remove (engine->slave_masters, master);

  rut_object_unref (master);
}

static void
client_error_handler (PB_RPC_Error_Code code,
                      const char *message,
                      void *user_data)
{
  RigSlaveMaster *master = user_data;

  g_return_if_fail (master->rpc_client);

  g_warning ("RPC Client error: %s", message);

  destroy_slave_master (master);
}

static void
_rig_slave_master_free (void *object)
{
  RigSlaveMaster *master = object;

  destroy_slave_master (master);

  rut_object_free (RigSlaveMaster, master);
}

static RutType rig_slave_master_type;

static void
_rig_slave_master_init_type (void)
{
  rut_type_init (&rig_slave_master_type, G_STRINGIFY (TYPE),
                 _rig_slave_master_free);
}

static RigSlaveMaster *
rig_slave_master_new (RigEngine *engine,
                      RigSlaveAddress *slave_address)
{
  RigSlaveMaster *master =
    rut_object_alloc0 (RigSlaveMaster,
                       &rig_slave_master_type,
                       _rig_slave_master_init_type);

  master->engine = engine;

  master->slave_address = rut_object_ref (slave_address);

  if (slave_address->type == RIG_SLAVE_ADDRESS_TYPE_ADB_SERIAL)
    {
      RutException *catch = NULL;
      struct timespec spec;

      if (!rut_adb_run_shell_cmd (slave_address->serial,
                                  &catch,
                                  "shell:am force-stop org.rig.app"))
        {
          g_warning ("Failed to force stop of Rig slave application on Android "
                     "device %s", slave_address->serial);
          rut_exception_free (catch);
          catch = NULL;
        }

      if (!rut_adb_run_shell_cmd (slave_address->serial,
                                  &catch,
                                  "shell:am start -n "
                                  "org.rig.app/org.rig.app.RigSlave"))
        {
          g_warning ("Failed to start Rig slave application on Android "
                     "device %s", slave_address->serial);
          rut_exception_free (catch);
          catch = NULL;
        }

      /* Give the app a bit of time to start before trying to connect... */
      spec.tv_sec = 0;
      spec.tv_nsec = 500000000;
      nanosleep (&spec, NULL);
    }

  master->rpc_client =
    rig_rpc_client_new (engine->shell,
                        slave_address->hostname,
                        slave_address->port,
                        (ProtobufCServiceDescriptor *)&rig__slave__descriptor,
                        client_error_handler,
                        slave_master_connected,
                        master);

  master->connected = false;

  return master;
}

void
rig_connect_to_slave (RigEngine *engine, RigSlaveAddress *slave_address)
{
  RigSlaveMaster *slave_master = rig_slave_master_new (engine, slave_address);

  engine->slave_masters = g_list_prepend (engine->slave_masters, slave_master);
}

void
rig_slave_master_reload_ui (RigSlaveMaster *master)
{
  RigPBSerializer *serializer;
  RigEngine *engine = master->engine;
  ProtobufCService *service =
    rig_pb_rpc_client_get_service (master->rpc_client->pb_rpc_client);
  Rig__UI *pb_ui;

  if (!master->connected)
    return;

  serializer = rig_pb_serializer_new (engine);

  rig_pb_serializer_set_use_pointer_ids_enabled (serializer, true);

  /* NB: We always use the edit-mode-ui as the basis for any ui sent
   * to a slave device so that the slave device can maintain a mapping
   * from edit-mode IDs to its play-mode IDs so that we can handle
   * edit operations in the slave.
   */
  pb_ui = rig_pb_serialize_ui (serializer, true, engine->edit_mode_ui);

  rig__slave__load (service, pb_ui, handle_load_response, NULL);

  rig_pb_serialized_ui_destroy (pb_ui);

  rig_pb_serializer_destroy (serializer);
}

static void
handle_edit_response (const Rig__UIEditResult *result,
                      void *closure_data)
{
  g_print ("UI edited by slave\n");
}

void
rig_slave_master_forward_pb_ui_edit (RigSlaveMaster *master,
                                     Rig__UIEdit *pb_ui_edit)
{
  ProtobufCService *service =
    rig_pb_rpc_client_get_service (master->rpc_client->pb_rpc_client);

  if (!master->connected)
    return;

  rig__slave__edit (service, pb_ui_edit, handle_edit_response, NULL);
}
