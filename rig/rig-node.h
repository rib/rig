/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RUT_NODE_H_
#define _RUT_NODE_H_

#include <cogl/cogl.h>
#include <rut.h>

typedef struct
{
  RutList list_node;

  RutBoxed boxed;

  float t;
} RigNode;

void
rig_node_float_lerp (RigNode *a,
                     RigNode *b,
                     float t,
                     float *value);

void
rig_node_vec3_lerp (RigNode *a,
                    RigNode *b,
                    float t,
                    float value[3]);

void
rig_node_quaternion_lerp (RigNode *a,
                          RigNode *b,
                          float t,
                          CoglQuaternion *value);

void
rig_node_double_lerp (RigNode *a,
                      RigNode *b,
                      float t,
                      double *value);

void
rig_node_integer_lerp (RigNode *a,
                       RigNode *b,
                       float t,
                       int *value);

void
rig_node_uint32_lerp (RigNode *a,
                      RigNode *b,
                      float t,
                      uint32_t *value);

void
rig_node_vec4_lerp (RigNode *a,
                    RigNode *b,
                    float t,
                    float value[4]);

void
rig_node_color_lerp (RigNode *a,
                     RigNode *b,
                     float t,
                     CoglColor *value);

void
rig_node_enum_lerp (RigNode *a,
                    RigNode *b,
                    float t,
                    int *value);

void
rig_node_boolean_lerp (RigNode *a,
                       RigNode *b,
                       float t,
                       bool *value);

void
rig_node_text_lerp (RigNode *a,
                    RigNode *b,
                    float t,
                    const char **value);

void
rig_node_asset_lerp (RigNode *a,
                     RigNode *b,
                     float t,
                     RutAsset **value);

void
rig_node_object_lerp (RigNode *a,
                      RigNode *b,
                      float t,
                      RutObject **value);

CoglBool
rig_node_box (RutPropertyType type,
              RigNode *node,
              RutBoxed *value);

void
rig_node_free (RigNode *node);

RigNode *
rig_node_new_for_float (float t, float value);

RigNode *
rig_node_new_for_double (float t, double value);

RigNode *
rig_node_new_for_vec3 (float t, const float value[3]);

RigNode *
rig_node_new_for_vec4 (float t, const float value[4]);

RigNode *
rig_node_new_for_integer (float t, int value);

RigNode *
rig_node_new_for_uint32 (float t, uint32_t value);

RigNode *
rig_node_new_for_quaternion (float t, const CoglQuaternion *value);

RigNode *
rig_node_new_for_color (float t, const CoglColor *value);

RigNode *
rig_node_new_for_boolean (float t, bool value);

RigNode *
rig_node_new_for_enum (float t, int value);

RigNode *
rig_node_new_for_text (float t, const char *value);

RigNode *
rig_node_new_for_asset (float t, RutAsset *value);

RigNode *
rig_node_new_for_object (float t, RutObject *value);

RigNode *
rig_node_copy (RigNode *node);

RigNode *
rig_nodes_find_less_than (RigNode *start, RutList *end, float t);

RigNode *
rig_nodes_find_less_than_equal (RigNode *start, RutList *end, float t);

RigNode *
rig_nodes_find_greater_than (RigNode *start, RutList *end, float t);

RigNode *
rig_nodes_find_greater_than_equal (RigNode *start, RutList *end, float t);

#endif /* _RUT_NODE_H_ */
