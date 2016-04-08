/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation
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

#ifndef _RIG_TEXT_H_
#define _RIG_TEXT_H_

#include "rig-entity.h"

typedef struct _rig_text_t rig_text_t;

#include "rig-text-engine.h"

extern rut_type_t rig_text_type;

enum {
    RIG_TEXT_PROP_TEXT,
    RIG_TEXT_PROP_FONT_FAMILY,
    RIG_TEXT_PROP_FONT_SIZE,
    RIG_TEXT_PROP_COLOR,
    RIG_TEXT_PROP_WIDTH,
    RIG_TEXT_PROP_HEIGHT,
    RIG_TEXT_N_PROPS
};

struct _rig_text_t {
    rut_object_base_t _base;
    rut_componentable_props_t component;

    float width;
    float height;

    c_list_t preferred_size_cb_list;

    char *text;
    char *font_family;
    float font_size;
    cg_color_t color;

    /* XXX: should rig_text_engine_t be folded into rig_text_t? */
    rig_text_engine_t *text_engine;
    rut_mesh_t *pick_mesh;

    rut_introspectable_props_t introspectable;
    rig_property_t properties[RIG_TEXT_N_PROPS];
};

rig_text_t *rig_text_new(rig_engine_t *engine);

void rig_text_set_text(rut_object_t *obj, const char *text);

const char *rig_text_get_text(rut_object_t *obj);

const char *rig_text_get_font_family(rut_object_t *obj);

void rig_text_set_font_family(rut_object_t *obj, const char *font_family);

float rig_text_get_font_size(rut_object_t *obj);

void rig_text_set_font_size(rut_object_t *obj, float font_size);

void rig_text_set_color(rut_object_t *obj, const cg_color_t *color);

const cg_color_t *rig_text_get_color(rut_object_t *obj);

void rig_text_set_width(rut_object_t *obj, float width);

void rig_text_set_height(rut_object_t *obj, float height);


#endif /* _RIG_TEXT_H_ */
