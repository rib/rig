/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "rut-type.h"
#include "rut-bitmask.h"

static CoglBool
find_max_id (int bit_num, void *user_data)
{
  int *max_id = user_data;
  if (bit_num > *max_id)
    *max_id = bit_num;

  return TRUE;
}

void
rut_type_add_interface (RutType *type,
                        RutInterfaceID id,
                        size_t instance_priv_offset,
                        void *interface_vtable)
{
  int max_id = 0;

  _rut_bitmask_foreach (&type->interfaces_mask,
                        find_max_id,
                        &max_id);

  if (id > max_id)
    {
      type->interfaces = g_realloc (type->interfaces,
                                    sizeof (RutInterface) * (id + 1));
    }

  _rut_bitmask_set (&type->interfaces_mask, id, TRUE);
  type->interfaces[id].props_offset = instance_priv_offset;
  type->interfaces[id].vtable = interface_vtable;

  /* TODO
   * type->interfaces[interface_id].v8_prototype_append (type);
   */
}

void
rut_type_init (RutType *type, const char *name)
{
  type->name = name;

  _rut_bitmask_init (&type->interfaces_mask);

  type->interfaces = NULL;

  /* TODO
   * type->v8_template = FunctionTemplate::New();
   */
}
