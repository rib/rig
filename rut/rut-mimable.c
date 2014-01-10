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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "rut-type.h"
#include "rut-mimable.h"

bool
rut_mimable_has_text (RutObject *object)
{
  RutMimableVTable *mimable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_MIMABLE);

  return mimable->has (object, RUT_MIMABLE_TYPE_TEXT);
}

char *
rut_mimable_get_text (RutObject *object)
{
  RutMimableVTable *mimable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_MIMABLE);

  if (!mimable->has (object, RUT_MIMABLE_TYPE_TEXT))
    return NULL;

  return mimable->get (object, RUT_MIMABLE_TYPE_TEXT);
}
