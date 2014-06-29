/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <config.h>

#include <clib.h>

#include "rut-interfaces.h"
#include "rut-transform-private.h"
#include "rut-transform.h"

static void
_rut_transform_free (void *object)
{
  RutTransform *transform = object;

  rut_graphable_destroy (transform);

  rut_object_free (RutTransform, object);
}

RutType rut_transform_type;

static void
_rut_transform_init_type (void)
{

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

  rut_type_init (type, C_STRINGIFY (TYPE), _rut_transform_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_TRANSFORMABLE,
                      0,
                      &transformable_vtable);

#undef TYPE
}

RutTransform *
rut_transform_new (RutContext *ctx)
{
  RutTransform *transform =
    rut_object_alloc0 (RutTransform, &rut_transform_type, _rut_transform_init_type);



  rut_graphable_init (transform);

  cg_matrix_init_identity (&transform->matrix);

  return transform;
}

void
rut_transform_translate (RutTransform *transform,
                         float x,
                         float y,
                         float z)
{
  cg_matrix_translate (&transform->matrix, x, y, z);
}

void
rut_transform_quaternion_rotate (RutTransform *transform,
                                 const cg_quaternion_t *quaternion)
{
  cg_matrix_t rotation;
  cg_matrix_init_from_quaternion (&rotation, quaternion);
  cg_matrix_multiply (&transform->matrix, &transform->matrix, &rotation);
}

void
rut_transform_rotate (RutTransform *transform,
                      float angle,
                      float x,
                      float y,
                      float z)
{
  cg_matrix_rotate (&transform->matrix, angle, x, y, z);
}
void
rut_transform_scale (RutTransform *transform,
                     float x,
                     float y,
                     float z)
{
  cg_matrix_scale (&transform->matrix, x, y, z);
}

void
rut_transform_transform (RutTransform *transform,
                         const cg_matrix_t *matrix)
{
  cg_matrix_multiply (&transform->matrix, &transform->matrix, matrix);
}

void
rut_transform_init_identity (RutTransform *transform)
{
  cg_matrix_init_identity (&transform->matrix);
}

const cg_matrix_t *
rut_transform_get_matrix (RutObject *self)
{
  RutTransform *transform = self;
  return &transform->matrix;
}
