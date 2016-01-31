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

typedef struct _rig_shape_model_t rig_shape_model_t;
extern rut_type_t _rig_shape_model_type;

struct _rig_shape_model_t {
    rut_object_base_t _base;

    /* TODO: Allow this to be an asset */
    cg_texture_t *shape_texture;

    rut_mesh_t *mesh;

    /* TODO: optionally copy the shape texture into a cpu cached buffer
     * and pick by sampling into that instead of using geometry. */
    rut_mesh_t *pick_mesh;
    rut_mesh_t *shape_mesh;
};

void _rig_shape_model_init_type(void);

typedef struct _rig_shape_t rig_shape_t;
extern rut_type_t rig_shape_type;

enum {
    RIG_SHAPE_PROP_SHAPED,
    RIG_SHAPE_PROP_WIDTH,
    RIG_SHAPE_PROP_HEIGHT,
    RIG_SHAPE_PROP_SHAPE_ASSET,
    RIG_SHAPE_N_PROPS
};

struct _rig_shape_t {
    rut_object_base_t _base;

    rut_componentable_props_t component;

    float width;
    float height;
    bool shaped;

    rig_shape_model_t *model;

    rig_asset_t *shape_asset;

    c_list_t reshaped_cb_list;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RIG_SHAPE_N_PROPS];
};

void _rig_shape_init_type(void);

rig_shape_t *
rig_shape_new(rig_engine_t *engine, bool shaped, int width, int height);

cg_primitive_t *rig_shape_get_primitive(rut_object_t *object);

/* TODO: Perhaps add a RUT_TRAIT_ID_GEOMETRY_COMPONENTABLE
 * interface with a ->get_shape_texture() methos so we can
 * generalize rig_diamond_apply_mask() and
 * rig_shape_get_shape_texture()
 */
cg_texture_t *rig_shape_get_shape_texture(rig_shape_t *shape);

rut_mesh_t *rig_shape_get_pick_mesh(rut_object_t *self);

void rig_shape_set_shaped(rut_object_t *shape, bool shaped);

bool rig_shape_get_shaped(rut_object_t *shape);

typedef void (*rig_shape_re_shaped_callback_t)(rig_shape_t *shape,
                                               void *user_data);

void
rig_shape_add_reshaped_callback(rig_shape_t *shape,
                                rut_closure_t *closure);

void rig_shape_set_width(rut_object_t *obj, float width);

void rig_shape_set_height(rut_object_t *obj, float height);

void rig_shape_set_size(rut_object_t *self, float width, float height);

void rig_shape_get_size(rut_object_t *self, float *width, float *height);

void rig_shape_set_shape_mask(rut_object_t *object, rig_asset_t *asset);

rig_asset_t *rig_shape_get_shape_mask(rut_object_t *object);

#endif /* __RIG_SHAPE_H__ */
