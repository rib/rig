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

#include <stdbool.h>

#include "rut-type.h"
#include "rut-bitmask.h"

static bool
find_max_id (int bit_num, void *user_data)
{
  int *max_id = user_data;
  if (bit_num > *max_id)
    *max_id = bit_num;

  return true;
}

void
rut_type_add_trait (RutType *type,
                    RutTraitID id,
                    size_t instance_priv_offset,
                    void *interface_vtable)
{
  int max_id = 0;

  _rut_bitmask_foreach (&type->traits_mask,
                        find_max_id,
                        &max_id);

  if (id > max_id)
    {
      type->traits = g_realloc (type->traits,
                                sizeof (RutTraitImplementation) * (id + 1));
    }

  _rut_bitmask_set (&type->traits_mask, id, true);
  type->traits[id].props_offset = instance_priv_offset;
  type->traits[id].vtable = interface_vtable;
  type->traits[id].destructor = NULL;
}

void
rut_trait_set_destructor (RutType *type,
                          RutTraitID id,
                          RutTraitDestructor trait_destructor)
{
  type->traits[id].destructor = trait_destructor;
  rut_list_insert (type->destructors.prev,
                   &type->traits[id].destructor_link);
}

void
rut_type_init (RutType *type,
               const char *name,
               RutTypeDestructor destructor)
{
  /* Note: we can assume the type is zeroed at this point */

  type->name = name;
  type->free = destructor;

  _rut_bitmask_init (&type->traits_mask);
  rut_list_init (&type->destructors);
}

void
rut_type_set_magazine (RutType *type, RutMagazine *magazine)
{
  type->magazine = magazine;
}
