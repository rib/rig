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

#ifndef __RIG_SLAVE_MASTER__
#define __RIG_SLAVE_MASTER__

#include <glib.h>

#include "protobuf-c-rpc/rig-protobuf-c-rpc.h"

#include "rig-slave-address.h"
#include "rig-rpc-network.h"
#include "rig-engine.h"

typedef struct _RigSlaveMaster
{
  RutObjectBase _base;

  RigEngine *engine;

  RigSlaveAddress *slave_address;
  RigRPCClient *rpc_client;
  gboolean connected;
  GHashTable *registry;

  GList *required_assets;

} RigSlaveMaster;

void
rig_connect_to_slave (RigEngine *engine, RigSlaveAddress *slave_address);

void
rig_slave_master_sync_ui (RigSlaveMaster *master);

#endif /* __RIG_SLAVE_MASTER__ */
