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

#ifndef __RIG_POINTALISM_GRID_H__
#define __RIG_POINTALISM_GRID_H__

#include "rig-entity.h"

typedef struct _rig_pointalism_grid_slice_t rig_pointalism_grid_slice_t;
extern rut_type_t _rig_pointalism_grid_slice_type;

enum {
    RIG_POINTALISM_GRID_PROP_SCALE,
    RIG_POINTALISM_GRID_PROP_Z,
    RIG_POINTALISM_GRID_PROP_LIGHTER,
    RIG_POINTALISM_GRID_PROP_CELL_SIZE,
    RIG_POINTALISM_GRID_N_PROPS
};

struct _rig_pointalism_grid_slice_t {
    rut_object_base_t _base;
    rut_mesh_t *mesh;
};

void _rig_pointalism_grid_slice_init_type(void);

typedef struct _rig_pointalism_grid_t rig_pointalism_grid_t;
extern rut_type_t rig_pointalism_grid_type;

struct _rig_pointalism_grid_t {
    rut_object_base_t _base;

    rut_componentable_props_t component;

    c_list_t updated_cb_list;

    rut_mesh_t *pick_mesh;
    rut_mesh_t *mesh;

    float pointalism_scale;
    float pointalism_z;
    bool pointalism_lighter;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RIG_POINTALISM_GRID_N_PROPS];
    float cell_size;
    int tex_width;
    int tex_height;
};

void _rig_pointalism_grid_init_type(void);

rig_pointalism_grid_t *rig_pointalism_grid_new(rig_engine_t *engine, float size);

cg_primitive_t *rig_pointalism_grid_get_primitive(rut_object_t *object);

rut_mesh_t *rig_pointalism_grid_get_pick_mesh(rut_object_t *self);

float rig_pointalism_grid_get_scale(rut_object_t *obj);

void rig_pointalism_grid_set_scale(rut_object_t *obj, float scale);

float rig_pointalism_grid_get_z(rut_object_t *obj);

void rig_pointalism_grid_set_z(rut_object_t *obj, float z);

bool rig_pointalism_grid_get_lighter(rut_object_t *obj);

void rig_pointalism_grid_set_lighter(rut_object_t *obj, bool lighter);

float rig_pointalism_grid_get_cell_size(rut_object_t *obj);

void rig_pointalism_grid_set_cell_size(rut_object_t *obj, float rows);

typedef void (*rig_pointalism_grid_update_callback_t)(
    rig_pointalism_grid_t *grid, void *user_data);

void
rig_pointalism_grid_add_update_callback(rig_pointalism_grid_t *grid,
                                        rut_closure_t *closure);

void
rig_pointalism_grid_set_image_size(rut_object_t *self, int width, int height);

#endif /* __RIG_POINTALISM_GRID_H__ */
