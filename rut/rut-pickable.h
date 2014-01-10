/*
 * Rut
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef __RUT_PICKABLE_H__
#define __RUT_PICKABLE_H__

#include <cogl/cogl.h>

#include "rut-object.h"

typedef bool (*RutPickablePick) (RutObject *pickable,
                                 RutCamera *camera,
                                 const CoglMatrix *graphable_modelview,
                                 float x,
                                 float y);

typedef struct _RutPickableVTable
{
  RutPickablePick pick;
} RutPickableVTable;

static inline bool
rut_pickable_pick (RutObject *pickable,
                   RutCamera *camera,
                   const CoglMatrix *graphable_modelview,
                   float x,
                   float y)
{
  RutPickableVTable *vtable =
    rut_object_get_vtable (pickable, RUT_TRAIT_ID_PICKABLE);

  return vtable->pick (pickable, camera, graphable_modelview, x, y);
}

#endif /* __RUT_PICKABLE_H__ */
