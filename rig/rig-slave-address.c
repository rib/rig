/*
 * Rut.
 *
 * Copyright (C) 2013  Intel Corporation.
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

#include "rig-slave-address.h"

static void
_rig_slave_address_free (void *object)
{
  RigSlaveAddress *slave_address = object;

  g_free (slave_address->name);
  g_free (slave_address->hostname);
  rut_object_free (RigSlaveAddress, slave_address);
}

static RutType rig_slave_address_type;

static void
_rig_slave_address_init_type (void)
{

  RutType *type = &rig_slave_address_type;
#define TYPE RigSlaveAddress

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_slave_address_free);

#undef TYPE
}

RigSlaveAddress *
rig_slave_address_new (const char *name,
                       const char *hostname,
                       int port)
{
  RigSlaveAddress *slave_address =
    rut_object_alloc0 (RigSlaveAddress, &rig_slave_address_type, _rig_slave_address_init_type);



  slave_address->name = g_strdup (name);
  slave_address->hostname = g_strdup (hostname);
  slave_address->port = port;

  return slave_address;
}
