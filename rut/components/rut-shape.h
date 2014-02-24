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

#ifndef __RUT_SHAPE_H__
#define __RUT_SHAPE_H__

#include "rut-entity.h"

typedef struct _RutShapeModel RutShapeModel;
extern RutType _rut_shape_model_type;

struct _RutShapeModel
{
  RutObjectBase _base;

  /* TODO: Allow this to be an asset */
  CoglTexture *shape_texture;

  RutMesh *mesh;

  /* TODO: optionally copy the shape texture into a cpu cached buffer
   * and pick by sampling into that instead of using geometry. */
  RutMesh *pick_mesh;
  RutMesh *shape_mesh;
};

void
_rut_shape_model_init_type (void);

typedef struct _RutShape RutShape;
extern RutType rut_shape_type;

enum {
  RUT_SHAPE_PROP_SHAPED,
  RUT_SHAPE_PROP_WIDTH,
  RUT_SHAPE_PROP_HEIGHT,
  RUT_SHAPE_N_PROPS
};

struct _RutShape
{
  RutObjectBase _base;


  RutComponentableProps component;

  RutContext *ctx;

  float width;
  float height;
  bool shaped;

  RutShapeModel *model;

  RutList reshaped_cb_list;

  RutIntrospectableProps introspectable;
  RutProperty properties[RUT_SHAPE_N_PROPS];
};

void
_rut_shape_init_type (void);

RutShape *
rut_shape_new (RutContext *ctx,
               bool shaped,
               int width,
               int height);

CoglPrimitive *
rut_shape_get_primitive (RutObject *object);

/* TODO: Perhaps add a RUT_TRAIT_ID_GEOMETRY_COMPONENTABLE
 * interface with a ->get_shape_texture() methos so we can
 * generalize rut_diamond_apply_mask() and
 * rut_shape_get_shape_texture()
 */
CoglTexture *
rut_shape_get_shape_texture (RutShape *shape);

RutMesh *
rut_shape_get_pick_mesh (RutObject *self);

void
rut_shape_set_shaped (RutObject *shape,
                      bool shaped);

bool
rut_shape_get_shaped (RutObject *shape);

typedef void (*RutShapeReShapedCallback) (RutShape *shape,
                                          void *user_data);

RutClosure *
rut_shape_add_reshaped_callback (RutShape *shape,
                                 RutShapeReShapedCallback callback,
                                 void *user_data,
                                 RutClosureDestroyCallback destroy_cb);

void
rut_shape_set_width (RutObject *obj, float width);

void
rut_shape_set_height (RutObject *obj, float height);

void
rut_shape_set_texture_size (RutShape *shape,
                            int width,
                            int height);

void
rut_shape_set_size (RutObject *self,
                    float width,
                    float height);

void
rut_shape_get_size (RutObject *self,
                    float *width,
                    float *height);

#endif /* __RUT_SHAPE_H__ */
