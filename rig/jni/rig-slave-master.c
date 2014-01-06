/*
 * Rut
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <glib.h>

#include <rut.h>

#include "protobuf-c/rig-protobuf-c-rpc.h"

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

  rig_slave_master_sync_ui (master);

  g_print ("XXXXXXXXXXXX Slave Connected and serialized UI sent!");
}

static void
destroy_slave_master (RigSlaveMaster *master)
{
  RigEngine *engine = master->engine;

  if (!master->rpc_client)
    return;

  rig_rpc_client_disconnect (master->rpc_client);
  rut_refable_unref (master->rpc_client);
  master->rpc_client = NULL;

  master->connected = FALSE;

  engine->slave_masters = g_list_remove (engine->slave_masters, master);

  rut_refable_unref (master);
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

  g_slice_free (RigSlaveMaster, master);
}

static RutType rig_slave_master_type;

static void
_rig_slave_master_init_type (void)
{
  static RutRefableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rig_slave_master_free
  };

  RutType *type = &rig_slave_master_type;
#define TYPE RigSlaveMaster

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);

#undef TYPE
}

static RigSlaveMaster *
rig_slave_master_new (RigEngine *engine,
                      RigSlaveAddress *slave_address)
{
  RigSlaveMaster *master = g_slice_new0 (RigSlaveMaster);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_slave_master_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&master->_parent, &rig_slave_master_type);

  master->ref_count = 1;

  master->engine = engine;

  master->slave_address = rut_refable_ref (slave_address);

  master->rpc_client =
    rig_rpc_client_new (engine,
                        slave_address->hostname,
                        slave_address->port,
                        (ProtobufCServiceDescriptor *)&rig__slave__descriptor,
                        client_error_handler,
                        slave_master_connected,
                        master);

  master->connected = FALSE;

  return master;
}

void
rig_connect_to_slave (RigEngine *engine, RigSlaveAddress *slave_address)
{
  RigSlaveMaster *slave_master = rig_slave_master_new (engine, slave_address);

  engine->slave_masters = g_list_prepend (engine->slave_masters, slave_master);
}

void
rig_slave_master_sync_ui (RigSlaveMaster *master)
{
  RigPBSerializer *serializer;
  RigEngine *engine = master->engine;
  ProtobufCService *service =
    rig_pb_rpc_client_get_service (master->rpc_client->pb_rpc_client);
  Rig__UI *ui;

  g_warn_if_fail (master->required_assets == NULL);

  serializer = rig_pb_serializer_new (engine);

  ui = rig_pb_serialize_ui (serializer);

  rig__slave__load (service, ui, handle_load_response, NULL);

  rig_pb_serialized_ui_destroy (ui);

  rig_pb_serializer_destroy (serializer);
}
