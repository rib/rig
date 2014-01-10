/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
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
#include "rut-object.h"
#include "rut-refcount-debug.h"

void
rut_object_init (RutObjectBase *object, RutType *type)
{
  object->type = type;
  object->ref_count = 1;

  _rut_refcount_debug_object_created (object);
}

RutObject *
_rut_object_alloc (size_t bytes, RutType *type, RutTypeInit type_init)
{
  RutObject *object;

  if (G_UNLIKELY (type->name == NULL))
    type_init ();

  if (type->magazine)
    object = rut_magazine_chunk_alloc (type->magazine);
  else
    object = g_slice_alloc (bytes);

  rut_object_init (object, type);

  return object;
}

RutObject *
_rut_object_alloc0 (size_t bytes, RutType *type, RutTypeInit type_init)
{
  RutObject *object = g_slice_alloc0 (bytes);

  if (G_UNLIKELY (type->name == NULL))
    type_init ();

  rut_object_init (object, type);

  return object;
}

void
_rut_object_free (size_t bytes, void *object)
{
  RutObjectBase *base = object;
  RutType *type = base->type;
  RutTraitImplementation *trait, *tmp;

  rut_list_for_each_safe (trait, tmp, &type->destructors, destructor_link)
    {
      trait->destructor (object);
    }

  if (type->magazine)
    rut_magazine_chunk_free (type->magazine, object);
  else
    g_slice_free1 (bytes, object);
}
