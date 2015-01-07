/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *
 *
 */

#ifndef __CG_PATH_PRIVATE_H
#define __CG_PATH_PRIVATE_H

#include "cogl-object.h"
#include "cogl-attribute-private.h"

typedef struct _float_vec2_t {
    float x;
    float y;
} float_vec2_t;

typedef struct _cg_path_node_t {
    float x;
    float y;
    unsigned int path_size;
} cg_path_node_t;

typedef struct _cg_bez_quad_t {
    float_vec2_t p1;
    float_vec2_t p2;
    float_vec2_t p3;
} cg_bez_quad_t;

typedef struct _cg_bez_cubic_t {
    float_vec2_t p1;
    float_vec2_t p2;
    float_vec2_t p3;
    float_vec2_t p4;
} cg_bez_cubic_t;

typedef struct _cg_path_data_t cg_path_data_t;

struct _cg_path_t {
    cg_object_t _parent;

    cg_path_data_t *data;
};

#define CG_PATH_N_ATTRIBUTES 2

struct _cg_path_data_t {
    unsigned int ref_count;

    cg_device_t *dev;

    cg_path_fill_rule_t fill_rule;

    c_array_t *path_nodes;

    float_vec2_t path_start;
    float_vec2_t path_pen;
    unsigned int last_path;
    float_vec2_t path_nodes_min;
    float_vec2_t path_nodes_max;

    cg_attribute_buffer_t *fill_attribute_buffer;
    cg_indices_t *fill_vbo_indices;
    unsigned int fill_vbo_n_indices;
    cg_attribute_t *fill_attributes[CG_PATH_N_ATTRIBUTES + 1];
    cg_primitive_t *fill_primitive;

    cg_attribute_buffer_t *stroke_attribute_buffer;
    cg_attribute_t **stroke_attributes;
    unsigned int stroke_n_attributes;

    /* This is used as an optimisation for when the path contains a
       single contour specified using cogl2_path_rectangle. Cogl is more
       optimised to handle rectangles than paths so we can detect this
       case and divert to the journal or a rectangle clip. If it is true
       then the entire path can be described by calling
       _cg_path_get_bounds */
    bool is_rectangle;
};

#endif /* __CG_PATH_PRIVATE_H */
