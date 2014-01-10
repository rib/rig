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

#ifndef __RUT_MESHABLE_H__
#define __RUT_MESHABLE_H__

/*
 *
 * Meshable Interface
 * (E.g. implemented by all geometry components)
 *
 */
typedef struct _RutMeshableVTable
{
  RutMesh *(*get_mesh)(void *object);
} RutMeshableVTable;

static inline void *
rut_meshable_get_mesh (RutObject *object)
{
  RutMeshableVTable *meshable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_MESHABLE);

  return meshable->get_mesh (object);
}

#endif /* __RUT_MESHABLE_H__ */
