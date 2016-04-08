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

#include <rut.h>

#include "rig-engine.h"
#include "rig-node.h"

typedef struct _rig_path_t rig_path_t;

struct _rig_path_t {
    rut_object_base_t _base;

    rig_engine_t *engine;
    rig_property_type_t type;
    c_list_t nodes;
    int length;
    rig_node_t *pos;
    c_list_t operation_cb_list;
};

typedef enum {
    RIG_PATH_OPERATION_ADDED,
    RIG_PATH_OPERATION_REMOVED,
    RIG_PATH_OPERATION_MODIFIED,
} rig_path_operation_t;

typedef void (*rig_path_operation_callback_t)(rig_path_t *path,
                                             rig_path_operation_t op,
                                             rig_node_t *node,
                                             void *user_data);

extern rut_type_t rig_path_type;

rig_property_t *rig_path_get_property(rig_path_t *path);

rig_path_t *rig_path_new(rig_engine_t *engine, rig_property_type_t type);

rig_path_t *rig_path_copy(rig_path_t *path);

typedef enum _rig_path_direction_t {
    RIG_PATH_DIRECTION_FORWARDS = 1,
    RIG_PATH_DIRECTION_BACKWARDS
} rig_path_direction_t;

bool rig_path_find_control_points2(rig_path_t *path,
                                   float t,
                                   rig_path_direction_t direction,
                                   rig_node_t **n0,
                                   rig_node_t **n1);

void rig_path_insert_node(rig_path_t *path, rig_node_t *node);

void rig_path_insert_vec3(rig_path_t *path, float t, const float value[3]);

void rig_path_insert_vec4(rig_path_t *path, float t, const float value[4]);

void rig_path_insert_float(rig_path_t *path, float t, float value);

void rig_path_insert_quaternion(rig_path_t *path,
                                float t,
                                const c_quaternion_t *value);

void rig_path_insert_double(rig_path_t *path, float t, double value);

void rig_path_insert_integer(rig_path_t *path, float t, int value);

void rig_path_insert_uint32(rig_path_t *path, float t, uint32_t value);

void rig_path_insert_color(rig_path_t *path, float t, const cg_color_t *value);

bool
rig_path_lerp_property(rig_path_t *path, rig_property_t *property, float t);

bool rig_path_get_boxed(rig_path_t *path, float t, rut_boxed_t *value);

void rig_path_insert_boxed(rig_path_t *path, float t, const rut_boxed_t *value);

void rig_path_remove(rig_path_t *path, float t);

void rig_path_remove_node(rig_path_t *path, rig_node_t *node);

rut_closure_t *
rig_path_add_operation_callback(rig_path_t *path,
                                rig_path_operation_callback_t callback,
                                void *user_data,
                                rut_closure_destroy_callback_t destroy_cb);

/**
 * rig_path_find_node:
 * @path: A #rig_path_t
 * @t: The time to search for
 *
 * Finds and returns a node which has exactly the time @t. The
 * returned node is owned by the path but is guaranteed to remain
 * alive until either the path is destroyed or the operation
 * %RIG_PATH_OPERATION_REMOVED is reported with the node's timestamp.
 */
rig_node_t *rig_path_find_node(rig_path_t *path, float t);

rig_node_t *rig_path_find_nearest(rig_path_t *path, float t);

typedef void (*rig_path_node_callback_t)(rig_node_t *node, void *user_data);

void rut_path_foreach_node(rig_path_t *path,
                           rig_path_node_callback_t callback,
                           void *user_data);
