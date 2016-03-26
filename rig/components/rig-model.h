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

#ifndef _RIG_MODEL_H_
#define _RIG_MODEL_H_

#include <stdint.h>

#include <cglib/cglib.h>

#include <rut.h>

#include "rig-entity.h"
#include "rig-asset.h"

typedef struct _rig_model_t rig_model_t;
typedef struct _rig_model_private_t rig_model_private_t;
extern rut_type_t rig_model_type;

struct _rig_model_t {
    rut_object_base_t _base;

    rut_componentable_props_t component;

    rig_asset_t *asset;

    rut_mesh_t *mesh;

    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float min_z;
    float max_z;

    cg_primitive_t *primitive;
};

void _rig_model_init_type(void);

rig_model_t *rig_model_new(rig_engine_t *engine, rut_mesh_t *mesh);

rut_mesh_t *rig_model_get_mesh(rut_object_t *self);

cg_primitive_t *rig_model_get_primitive(rut_object_t *object);

#endif /* _RIG_MODEL_H_ */
