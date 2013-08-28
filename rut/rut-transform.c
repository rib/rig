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

#include "rut-interfaces.h"
#include "rut-transform-private.h"
#include "rut-transform.h"

static void
_rut_transform_free (void *object)
{
  RutTransform *transform = object;

  rut_graphable_destroy (transform);

  g_slice_free (RutTransform, object);
}

RutType rut_transform_type;

static void
_rut_transform_init_type (void)
{
  static RutRefableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_transform_free
  };

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutTransformableVTable transformable_vtable = {
      rut_transform_get_matrix
  };

  RutType *type = &rut_transform_type;
#define TYPE RutTransform

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_TRANSFORMABLE,
                          0,
                          &transformable_vtable);

#undef TYPE
}

RutTransform *
rut_transform_new (RutContext *ctx)
{
  RutTransform *transform = g_slice_new (RutTransform);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_transform_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&transform->_parent, &rut_transform_type);

  transform->ref_count = 1;

  rut_graphable_init (transform);

  cogl_matrix_init_identity (&transform->matrix);

  return transform;
}

void
rut_transform_translate (RutTransform *transform,
                         float x,
                         float y,
                         float z)
{
  cogl_matrix_translate (&transform->matrix, x, y, z);
}

void
rut_transform_quaternion_rotate (RutTransform *transform,
                                 const CoglQuaternion *quaternion)
{
  CoglMatrix rotation;
  cogl_matrix_init_from_quaternion (&rotation, quaternion);
  cogl_matrix_multiply (&transform->matrix, &transform->matrix, &rotation);
}

void
rut_transform_rotate (RutTransform *transform,
                      float angle,
                      float x,
                      float y,
                      float z)
{
  cogl_matrix_rotate (&transform->matrix, angle, x, y, z);
}
void
rut_transform_scale (RutTransform *transform,
                     float x,
                     float y,
                     float z)
{
  cogl_matrix_scale (&transform->matrix, x, y, z);
}

void
rut_transform_transform (RutTransform *transform,
                         const CoglMatrix *matrix)
{
  cogl_matrix_multiply (&transform->matrix, &transform->matrix, matrix);
}

void
rut_transform_init_identity (RutTransform *transform)
{
  cogl_matrix_init_identity (&transform->matrix);
}

const CoglMatrix *
rut_transform_get_matrix (RutObject *self)
{
  RutTransform *transform = self;
  return &transform->matrix;
}
