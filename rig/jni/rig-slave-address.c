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
  g_slice_free (RigSlaveAddress, slave_address);
}

static RutType rig_slave_address_type;

static void
_rig_slave_address_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rig_slave_address_free
  };

  RutType *type = &rig_slave_address_type;
#define TYPE RigSlaveAddress

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);

#undef TYPE
}

RigSlaveAddress *
rig_slave_address_new (const char *name,
                       const char *hostname,
                       int port)
{
  RigSlaveAddress *slave_address = g_slice_new (RigSlaveAddress);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_slave_address_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&slave_address->_parent, &rig_slave_address_type);

  slave_address->ref_count = 1;

  slave_address->name = g_strdup (name);
  slave_address->hostname = g_strdup (hostname);
  slave_address->port = port;

  return slave_address;
}
