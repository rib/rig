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

#include <rig-config.h>

#include <math.h>

#include "rig-node.h"

void
rig_node_integer_lerp(rig_node_t *a, rig_node_t *b, float t, int *value)
{
    float range = b->t - a->t;
    if (range) {
        float offset = t - a->t;
        float factor = offset / range;

        *value = nearbyint(a->boxed.d.integer_val +
                           (b->boxed.d.integer_val - a->boxed.d.integer_val) *
                           factor);
    } else
        *value = a->boxed.d.integer_val;
}

void
rig_node_uint32_lerp(rig_node_t *a, rig_node_t *b, float t, uint32_t *value)
{
    float range = b->t - a->t;
    if (range) {
        float offset = t - a->t;
        float factor = offset / range;

        *value =
            nearbyint(a->boxed.d.uint32_val +
                      (b->boxed.d.uint32_val - a->boxed.d.uint32_val) * factor);
    } else
        *value = a->boxed.d.uint32_val;
}

void
rig_node_float_lerp(rig_node_t *a, rig_node_t *b, float t, float *value)
{
    float range = b->t - a->t;
    if (range) {
        float offset = t - a->t;
        float factor = offset / range;

        *value = a->boxed.d.float_val +
                 (b->boxed.d.float_val - a->boxed.d.float_val) * factor;
    } else
        *value = a->boxed.d.float_val;
}

void
rig_node_double_lerp(rig_node_t *a, rig_node_t *b, float t, double *value)
{
    float range = b->t - a->t;
    if (range) {
        float offset = t - a->t;
        float factor = offset / range;

        *value = a->boxed.d.double_val +
                 (b->boxed.d.double_val - a->boxed.d.double_val) * factor;
    } else
        *value = a->boxed.d.double_val;
}

void
rig_node_vec3_lerp(rig_node_t *a, rig_node_t *b, float t, float value[3])
{
    float range = b->t - a->t;

    if (range) {
        float offset = t - a->t;
        float factor = offset / range;

        value[0] = a->boxed.d.vec3_val[0] +
                   (b->boxed.d.vec3_val[0] - a->boxed.d.vec3_val[0]) * factor;
        value[1] = a->boxed.d.vec3_val[1] +
                   (b->boxed.d.vec3_val[1] - a->boxed.d.vec3_val[1]) * factor;
        value[2] = a->boxed.d.vec3_val[2] +
                   (b->boxed.d.vec3_val[2] - a->boxed.d.vec3_val[2]) * factor;
    } else
        memcpy(value, a->boxed.d.vec3_val, sizeof(float) * 3);
}

void
rig_node_vec4_lerp(rig_node_t *a, rig_node_t *b, float t, float value[4])
{
    float range = b->t - a->t;

    if (range) {
        float offset = t - a->t;
        float factor = offset / range;

        value[0] = a->boxed.d.vec4_val[0] +
                   (b->boxed.d.vec4_val[0] - a->boxed.d.vec4_val[0]) * factor;
        value[1] = a->boxed.d.vec4_val[1] +
                   (b->boxed.d.vec4_val[1] - a->boxed.d.vec4_val[1]) * factor;
        value[2] = a->boxed.d.vec4_val[2] +
                   (b->boxed.d.vec4_val[2] - a->boxed.d.vec4_val[2]) * factor;
        value[3] = a->boxed.d.vec4_val[3] +
                   (b->boxed.d.vec4_val[3] - a->boxed.d.vec4_val[3]) * factor;
    } else
        memcpy(value, a->boxed.d.vec4_val, sizeof(float) * 4);
}

void
rig_node_color_lerp(rig_node_t *a, rig_node_t *b, float t, cg_color_t *value)
{
    float range = b->t - a->t;

    if (range) {
        float offset = t - a->t;
        float factor = offset / range;

        value->red =
            a->boxed.d.color_val.red +
            (b->boxed.d.color_val.red - a->boxed.d.color_val.red) * factor;
        value->green =
            a->boxed.d.color_val.green +
            (b->boxed.d.color_val.green - a->boxed.d.color_val.green) * factor;
        value->blue =
            a->boxed.d.color_val.blue +
            (b->boxed.d.color_val.blue - a->boxed.d.color_val.blue) * factor;
        value->alpha =
            a->boxed.d.color_val.alpha +
            (b->boxed.d.color_val.alpha - a->boxed.d.color_val.alpha) * factor;
    } else
        memcpy(value, &a->boxed.d.color_val, sizeof(cg_color_t));
}

void
rig_node_quaternion_lerp(rig_node_t *a,
                         rig_node_t *b,
                         float t,
                         c_quaternion_t *value)
{
    float range = b->t - a->t;
    if (range) {
        float offset = t - a->t;
        float factor = offset / range;

        c_quaternion_nlerp(value,
                            &a->boxed.d.quaternion_val,
                            &b->boxed.d.quaternion_val,
                            factor);
    } else
        *value = a->boxed.d.quaternion_val;
}

void
rig_node_enum_lerp(rig_node_t *a, rig_node_t *b, float t, int *value)
{
    if (a->t >= b->t)
        *value = a->boxed.d.enum_val;
    else
        *value = b->boxed.d.enum_val;
}

void
rig_node_boolean_lerp(rig_node_t *a, rig_node_t *b, float t, bool *value)
{
    if (a->t >= b->t)
        *value = a->boxed.d.boolean_val;
    else
        *value = b->boxed.d.boolean_val;
}

void
rig_node_text_lerp(rig_node_t *a, rig_node_t *b, float t, const char **value)
{
    if (a->t >= b->t)
        *value = a->boxed.d.text_val;
    else
        *value = b->boxed.d.text_val;
}

void
rig_node_asset_lerp(rig_node_t *a, rig_node_t *b, float t, rig_asset_t **value)
{
    if (a->t >= b->t)
        *value = a->boxed.d.asset_val;
    else
        *value = b->boxed.d.asset_val;
}

void
rig_node_object_lerp(rig_node_t *a,
                     rig_node_t *b,
                     float t,
                     rut_object_t **value)
{
    if (a->t >= b->t)
        *value = a->boxed.d.object_val;
    else
        *value = b->boxed.d.object_val;
}

bool
rig_node_box(rut_property_type_t type, rig_node_t *node, rut_boxed_t *value)
{
    switch (type) {
    case RUT_PROPERTY_TYPE_FLOAT:
        value->type = RUT_PROPERTY_TYPE_FLOAT;
        value->d.float_val = node->boxed.d.float_val;
        return true;

    case RUT_PROPERTY_TYPE_DOUBLE:
        value->type = RUT_PROPERTY_TYPE_DOUBLE;
        value->d.double_val = node->boxed.d.double_val;
        return true;

    case RUT_PROPERTY_TYPE_INTEGER:
        value->type = RUT_PROPERTY_TYPE_INTEGER;
        value->d.integer_val = node->boxed.d.integer_val;
        return true;

    case RUT_PROPERTY_TYPE_UINT32:
        value->type = RUT_PROPERTY_TYPE_UINT32;
        value->d.uint32_val = node->boxed.d.uint32_val;
        return true;

    case RUT_PROPERTY_TYPE_VEC3:
        value->type = RUT_PROPERTY_TYPE_VEC3;
        memcpy(value->d.vec3_val, node->boxed.d.vec3_val, sizeof(float) * 3);
        return true;

    case RUT_PROPERTY_TYPE_VEC4:
        value->type = RUT_PROPERTY_TYPE_VEC4;
        memcpy(value->d.vec4_val, node->boxed.d.vec4_val, sizeof(float) * 4);
        return true;

    case RUT_PROPERTY_TYPE_COLOR:
        value->type = RUT_PROPERTY_TYPE_COLOR;
        value->d.color_val = node->boxed.d.color_val;
        return true;

    case RUT_PROPERTY_TYPE_QUATERNION:
        value->type = RUT_PROPERTY_TYPE_QUATERNION;
        value->d.quaternion_val = node->boxed.d.quaternion_val;
        return true;

    case RUT_PROPERTY_TYPE_ENUM:
        value->type = RUT_PROPERTY_TYPE_ENUM;
        value->d.enum_val = node->boxed.d.enum_val;
        return true;

    case RUT_PROPERTY_TYPE_BOOLEAN:
        value->type = RUT_PROPERTY_TYPE_BOOLEAN;
        value->d.boolean_val = node->boxed.d.boolean_val;
        return true;

    case RUT_PROPERTY_TYPE_TEXT:
        value->type = RUT_PROPERTY_TYPE_TEXT;
        value->d.text_val = c_strdup(node->boxed.d.text_val);
        return true;

    case RUT_PROPERTY_TYPE_ASSET:
        value->type = RUT_PROPERTY_TYPE_ASSET;
        value->d.asset_val = rut_object_ref(node->boxed.d.asset_val);
        return true;

    case RUT_PROPERTY_TYPE_OBJECT:
        value->type = RUT_PROPERTY_TYPE_OBJECT;
        value->d.object_val = rut_object_ref(node->boxed.d.object_val);
        return true;

    case RUT_PROPERTY_TYPE_POINTER:
        value->type = RUT_PROPERTY_TYPE_OBJECT;
        value->d.pointer_val = node->boxed.d.pointer_val;
        return true;
    }

    c_warn_if_reached();

    return false;
}

void
rig_node_free(rig_node_t *node)
{
    c_slice_free(rig_node_t, node);
}

rig_node_t *
rig_node_new_for_integer(float t, int value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_INTEGER;
    node->boxed.d.integer_val = value;
    return node;
}

rig_node_t *
rig_node_new_for_uint32(float t, uint32_t value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_UINT32;
    node->boxed.d.uint32_val = value;
    return node;
}

rig_node_t *
rig_node_new_for_float(float t, float value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_FLOAT;
    node->boxed.d.float_val = value;
    return node;
}

rig_node_t *
rig_node_new_for_double(float t, double value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_DOUBLE;
    node->boxed.d.double_val = value;
    return node;
}

rig_node_t *
rig_node_new_for_vec3(float t, const float value[3])
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_VEC3;
    memcpy(node->boxed.d.vec3_val, value, sizeof(float) * 3);
    return node;
}

rig_node_t *
rig_node_new_for_vec4(float t, const float value[4])
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_VEC4;
    memcpy(node->boxed.d.vec4_val, value, sizeof(float) * 4);
    return node;
}

rig_node_t *
rig_node_new_for_quaternion(float t, const c_quaternion_t *value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_QUATERNION;
    node->boxed.d.quaternion_val = *value;

    return node;
}

rig_node_t *
rig_node_new_for_color(float t, const cg_color_t *value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_COLOR;
    node->boxed.d.color_val = *value;

    return node;
}

rig_node_t *
rig_node_new_for_boolean(float t, bool value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_BOOLEAN;
    node->boxed.d.boolean_val = value;

    return node;
}

rig_node_t *
rig_node_new_for_enum(float t, int value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_ENUM;
    node->boxed.d.enum_val = value;

    return node;
}

rig_node_t *
rig_node_new_for_text(float t, const char *value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_TEXT;
    node->boxed.d.text_val = c_strdup(value);

    return node;
}

rig_node_t *
rig_node_new_for_asset(float t, rig_asset_t *value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_ASSET;
    node->boxed.d.asset_val = rut_object_ref(value);

    return node;
}

rig_node_t *
rig_node_new_for_object(float t, rut_object_t *value)
{
    rig_node_t *node = c_slice_new(rig_node_t);
    node->t = t;
    node->boxed.type = RUT_PROPERTY_TYPE_OBJECT;
    node->boxed.d.object_val = rut_object_ref(value);

    return node;
}

rig_node_t *
rig_node_copy(rig_node_t *node)
{
    return c_slice_dup(rig_node_t, node);
}

rig_node_t *
rig_nodes_find_less_than(rig_node_t *start, c_list_t *end, float t)
{
    rig_node_t *node;

    for (node = start; node != rut_container_of(end, node, list_node);
         node = rut_container_of(node->list_node.prev, node, list_node))
        if (node->t < t)
            return node;

    return NULL;
}

rig_node_t *
rig_nodes_find_less_than_equal(rig_node_t *start, c_list_t *end, float t)
{
    rig_node_t *node;

    for (node = start; node != rut_container_of(end, node, list_node);
         node = rut_container_of(node->list_node.prev, node, list_node))
        if (node->t <= t)
            return node;

    return NULL;
}

rig_node_t *
rig_nodes_find_greater_than(rig_node_t *start, c_list_t *end, float t)
{
    rig_node_t *node;

    for (node = start; node != rut_container_of(end, node, list_node);
         node = rut_container_of(node->list_node.next, node, list_node))
        if (node->t > t)
            return node;

    return NULL;
}

rig_node_t *
rig_nodes_find_greater_than_equal(rig_node_t *start, c_list_t *end, float t)
{
    rig_node_t *node;

    for (node = start; node != rut_container_of(end, node, list_node);
         node = rut_container_of(node->list_node.prev, node, list_node))
        if (node->t >= t)
            return node;

    return NULL;
}
