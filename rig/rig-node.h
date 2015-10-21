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

#ifndef _RUT_NODE_H_
#define _RUT_NODE_H_

#include <cglib/cglib.h>
#include <rut.h>

typedef struct {
    c_list_t list_node;

    rut_boxed_t boxed;

    float t;
} rig_node_t;

void rig_node_float_lerp(rig_node_t *a, rig_node_t *b, float t, float *value);

void rig_node_vec3_lerp(rig_node_t *a, rig_node_t *b, float t, float value[3]);

void rig_node_quaternion_lerp(rig_node_t *a,
                              rig_node_t *b,
                              float t,
                              c_quaternion_t *value);

void rig_node_double_lerp(rig_node_t *a, rig_node_t *b, float t, double *value);

void rig_node_integer_lerp(rig_node_t *a, rig_node_t *b, float t, int *value);

void
rig_node_uint32_lerp(rig_node_t *a, rig_node_t *b, float t, uint32_t *value);

void rig_node_vec4_lerp(rig_node_t *a, rig_node_t *b, float t, float value[4]);

void
rig_node_color_lerp(rig_node_t *a, rig_node_t *b, float t, cg_color_t *value);

void rig_node_enum_lerp(rig_node_t *a, rig_node_t *b, float t, int *value);

void rig_node_boolean_lerp(rig_node_t *a, rig_node_t *b, float t, bool *value);

void
rig_node_text_lerp(rig_node_t *a, rig_node_t *b, float t, const char **value);

void
rig_node_asset_lerp(rig_node_t *a, rig_node_t *b, float t, rig_asset_t **value);

void rig_node_object_lerp(rig_node_t *a,
                          rig_node_t *b,
                          float t,
                          rut_object_t **value);

bool
rig_node_box(rut_property_type_t type, rig_node_t *node, rut_boxed_t *value);

void rig_node_free(rig_node_t *node);

rig_node_t *rig_node_new_for_float(float t, float value);

rig_node_t *rig_node_new_for_double(float t, double value);

rig_node_t *rig_node_new_for_vec3(float t, const float value[3]);

rig_node_t *rig_node_new_for_vec4(float t, const float value[4]);

rig_node_t *rig_node_new_for_integer(float t, int value);

rig_node_t *rig_node_new_for_uint32(float t, uint32_t value);

rig_node_t *rig_node_new_for_quaternion(float t, const c_quaternion_t *value);

rig_node_t *rig_node_new_for_color(float t, const cg_color_t *value);

rig_node_t *rig_node_new_for_boolean(float t, bool value);

rig_node_t *rig_node_new_for_enum(float t, int value);

rig_node_t *rig_node_new_for_text(float t, const char *value);

rig_node_t *rig_node_new_for_asset(float t, rig_asset_t *value);

rig_node_t *rig_node_new_for_object(float t, rut_object_t *value);

rig_node_t *rig_node_copy(rig_node_t *node);

rig_node_t *
rig_nodes_find_less_than(rig_node_t *start, c_list_t *end, float t);

rig_node_t *
rig_nodes_find_less_than_equal(rig_node_t *start, c_list_t *end, float t);

rig_node_t *
rig_nodes_find_greater_than(rig_node_t *start, c_list_t *end, float t);

rig_node_t *
rig_nodes_find_greater_than_equal(rig_node_t *start, c_list_t *end, float t);

#endif /* _RUT_NODE_H_ */
