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

#ifndef __RIG_SHAPE_H__
#define __RIG_SHAPE_H__

#include "rig-entity.h"

typedef struct _RigShapeModel RigShapeModel;
extern RutType _rig_shape_model_type;

struct _RigShapeModel
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
_rig_shape_model_init_type (void);

typedef struct _RigShape RigShape;
extern RutType rig_shape_type;

enum {
  RIG_SHAPE_PROP_SHAPED,
  RIG_SHAPE_PROP_WIDTH,
  RIG_SHAPE_PROP_HEIGHT,
  RIG_SHAPE_N_PROPS
};

struct _RigShape
{
  RutObjectBase _base;


  RutComponentableProps component;

  RutContext *ctx;

  float width;
  float height;
  bool shaped;

  RigShapeModel *model;

  RutList reshaped_cb_list;

  RutIntrospectableProps introspectable;
  RutProperty properties[RIG_SHAPE_N_PROPS];
};

void
_rig_shape_init_type (void);

RigShape *
rig_shape_new (RutContext *ctx,
               bool shaped,
               int width,
               int height);

CoglPrimitive *
rig_shape_get_primitive (RutObject *object);

/* TODO: Perhaps add a RUT_TRAIT_ID_GEOMETRY_COMPONENTABLE
 * interface with a ->get_shape_texture() methos so we can
 * generalize rig_diamond_apply_mask() and
 * rig_shape_get_shape_texture()
 */
CoglTexture *
rig_shape_get_shape_texture (RigShape *shape);

RutMesh *
rig_shape_get_pick_mesh (RutObject *self);

void
rig_shape_set_shaped (RutObject *shape,
                      bool shaped);

bool
rig_shape_get_shaped (RutObject *shape);

typedef void (*RigShapeReShapedCallback) (RigShape *shape,
                                          void *user_data);

RutClosure *
rig_shape_add_reshaped_callback (RigShape *shape,
                                 RigShapeReShapedCallback callback,
                                 void *user_data,
                                 RutClosureDestroyCallback destroy_cb);

void
rig_shape_set_width (RutObject *obj, float width);

void
rig_shape_set_height (RutObject *obj, float height);

void
rig_shape_set_texture_size (RigShape *shape,
                            int width,
                            int height);

void
rig_shape_set_size (RutObject *self,
                    float width,
                    float height);

void
rig_shape_get_size (RutObject *self,
                    float *width,
                    float *height);

#endif /* __RIG_SHAPE_H__ */
