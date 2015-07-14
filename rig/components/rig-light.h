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

#ifndef __RIG_LIGHT_H__
#define __RIG_LIGHT_H__

#include "rig-entity.h"

typedef struct _rig_light_t rig_light_t;
extern rut_type_t rig_light_type;

enum {
    RIG_LIGHT_PROP_AMBIENT,
    RIG_LIGHT_PROP_DIFFUSE,
    RIG_LIGHT_PROP_SPECULAR,
    RIG_LIGHT_N_PROPS
};

struct _rig_light_t {
    rut_object_base_t _base;
    rut_componentable_props_t component;
    cg_color_t ambient;
    cg_color_t diffuse;
    cg_color_t specular;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RIG_LIGHT_N_PROPS];
};

rig_light_t *rig_light_new(rig_engine_t *engine);

void rig_light_free(rig_light_t *light);

void rig_light_set_ambient(rut_object_t *light, const cg_color_t *ambient);

const cg_color_t *rig_light_get_ambient(rut_object_t *light);

void rig_light_set_diffuse(rut_object_t *light, const cg_color_t *diffuse);

const cg_color_t *rig_light_get_diffuse(rig_light_t *light);

void rig_light_set_specular(rut_object_t *light, const cg_color_t *specular);

const cg_color_t *rig_light_get_specular(rig_light_t *light);

void rig_light_add_pipeline(rig_light_t *light, cg_pipeline_t *pipeline);

void rig_light_set_uniforms(rig_light_t *light, cg_pipeline_t *pipeline);

#endif /* __RIG_LIGHT_H__ */
