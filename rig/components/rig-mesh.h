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

#pragma once

#include <stdint.h>

#include <cglib/cglib.h>

#include <rut.h>

#include "rig-entity.h"
#include "rig-asset.h"

typedef struct _rig_mesh_t rig_mesh_t;
extern rut_type_t rig_mesh_type;

enum {
    RIG_MESH_PROP_N_VERTICES,
    RIG_MESH_PROP_VERTICES_MODE,
    RIG_MESH_PROP_INDICES,
    RIG_MESH_PROP_INDICES_TYPE,
    RIG_MESH_PROP_N_INDICES,
    RIG_MESH_PROP_X_MIN,
    RIG_MESH_PROP_X_MAX,
    RIG_MESH_PROP_Y_MIN,
    RIG_MESH_PROP_Y_MAX,
    RIG_MESH_PROP_Z_MIN,
    RIG_MESH_PROP_Z_MAX,
    RIG_MESH_N_PROPS,
};

struct _rig_mesh_t {
    rut_object_base_t _base;

    rut_componentable_props_t component;

    rut_mesh_t *rut_mesh;

    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float min_z;
    float max_z;

    cg_primitive_t *primitive;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RIG_MESH_N_PROPS];
};

void _rig_mesh_init_type(void);

rig_mesh_t *rig_mesh_new(rig_engine_t *engine);
rig_mesh_t *rig_mesh_new_with_rut_mesh(rig_engine_t *engine, rut_mesh_t *rut_mesh);

void rig_mesh_set_attributes(rig_mesh_t *mesh,
                             rut_attribute_t **attributes,
                             int n_attributes);

rut_mesh_t *rig_mesh_get_rut_mesh(rut_object_t *self);

cg_primitive_t *rig_mesh_get_primitive(rut_object_t *object);

int rig_mesh_get_n_vertices(rut_object_t *obj);
void rig_mesh_set_n_vertices(rut_object_t *obj, int n_vertices);

int rig_mesh_get_vertices_mode(rut_object_t *obj);
void rig_mesh_set_vertices_mode(rut_object_t *obj, int mode);

rut_object_t *rig_mesh_get_indices(rut_object_t *obj);
void rig_mesh_set_indices(rut_object_t *obj, rut_object_t *buffer);

int rig_mesh_get_indices_type(rut_object_t *obj);
void rig_mesh_set_indices_type(rut_object_t *obj, int indices_type);

int rig_mesh_get_n_indices(rut_object_t *obj);
void rig_mesh_set_n_indices(rut_object_t *obj, int n_indices);

