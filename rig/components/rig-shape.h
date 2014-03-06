/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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
