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

#ifndef __RIG_DIAMOND_H__
#define __RIG_DIAMOND_H__

#include "rig-entity.h"

typedef struct _rig_diamond_slice_t rig_diamond_slice_t;
extern rut_type_t _rig_diamond_slice_type;

struct _rig_diamond_slice_t {
    rut_object_base_t _base;

    c_matrix_t rotate_matrix;

    float size;

    rut_mesh_t *mesh;
    rut_mesh_t *pick_mesh;
};

void _rig_diamond_slice_init_type(void);

typedef struct _rig_diamond_t rig_diamond_t;
extern rut_type_t rig_diamond_type;

enum {
    RIG_DIAMOND_PROP_SIZE,
    RIG_DIAMOND_N_PROPS
};

struct _rig_diamond_t {
    rut_object_base_t _base;

    rut_componentable_props_t component;

    c_list_t updated_cb_list;

    rig_diamond_slice_t *slice;

    int tex_width;
    int tex_height;
    float size;

    rut_introspectable_props_t introspectable;
    rig_property_t properties[RIG_DIAMOND_N_PROPS];
};

void _rig_diamond_init_type(void);

rig_diamond_t *rig_diamond_new(rig_engine_t *engine, float size);

float rig_diamond_get_size(rig_diamond_t *diamond);

void rig_diamond_set_size(rut_object_t *object, float size);

cg_primitive_t *rig_diamond_get_primitive(rut_object_t *object);

void rig_diamond_apply_mask(rig_diamond_t *diamond, cg_pipeline_t *pipeline);

rut_mesh_t *rig_diamond_get_pick_mesh(rut_object_t *self);

typedef void (*rig_diamond_update_callback_t)(rig_diamond_t *diamond,
                                              void *user_data);

void
rig_diamond_add_update_callback(rig_diamond_t *diamond,
                                rut_closure_t *closure);

void rig_diamond_set_image_size(rut_object_t *self, int width, int height);

#endif /* __RIG_DIAMOND_H__ */
