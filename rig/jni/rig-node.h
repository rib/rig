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
  float t;
} RigNode;

typedef struct
{
  float t;
  float value;
} RigNodeFloat;

typedef struct
{
  float t;
  double value;
} RigNodeDouble;

typedef struct
{
  float t;
  int value;
} RigNodeInteger;

typedef struct
{
  float t;
  uint32_t value;
} RigNodeUint32;

typedef struct
{
  float t;
  float value[3];
} RigNodeVec3;

typedef struct
{
  float t;
  float value[4];
} RigNodeVec4;

typedef struct
{
  float t;
  RutColor value;
} RigNodeColor;

typedef struct
{
  float t;
  CoglQuaternion value;
} RigNodeQuaternion;

void
rig_node_float_lerp (RigNodeFloat *a,
                     RigNodeFloat *b,
                     float t,
                     float *value);

void
rig_node_vec3_lerp (RigNodeVec3 *a,
                    RigNodeVec3 *b,
                    float t,
                    float value[3]);

void
rig_node_quaternion_lerp (RigNodeQuaternion *a,
                          RigNodeQuaternion *b,
                          float t,
                          CoglQuaternion *value);

void
rig_node_double_lerp (RigNodeDouble *a,
                      RigNodeDouble *b,
                      float t,
                      double *value);

void
rig_node_integer_lerp (RigNodeInteger *a,
                       RigNodeInteger *b,
                       float t,
                       int *value);

void
rig_node_uint32_lerp (RigNodeUint32 *a,
                      RigNodeUint32 *b,
                      float t,
                      uint32_t *value);

void
rig_node_vec4_lerp (RigNodeVec4 *a,
                    RigNodeVec4 *b,
                    float t,
                    float value[4]);

void
rig_node_color_lerp (RigNodeColor *a,
                     RigNodeColor *b,
                     float t,
                     RutColor *value);

void
rig_node_free (RutPropertyType type,
               void *node);

RigNodeFloat *
rig_node_new_for_float (float t, float value);

RigNodeDouble *
rig_node_new_for_double (float t, double value);

RigNodeVec3 *
rig_node_new_for_vec3 (float t, const float value[3]);

RigNodeVec4 *
rig_node_new_for_vec4 (float t, const float value[4]);

RigNodeInteger *
rig_node_new_for_integer (float t, int value);

RigNodeUint32 *
rig_node_new_for_uint32 (float t, uint32_t value);

RigNodeQuaternion *
rig_node_new_for_quaternion (float t, const CoglQuaternion *value);

RigNodeColor *
rig_node_new_for_color (float t, const RutColor *value);

GList *
rig_nodes_find_less_than (GList *start, float t);

GList *
rig_nodes_find_less_than_equal (GList *start, float t);

GList *
rig_nodes_find_greater_than (GList *start, float t);

GList *
rig_nodes_find_greater_than_equal (GList *start, float t);

#endif /* _RUT_NODE_H_ */
