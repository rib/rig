/*
 * Rig
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

#ifndef _RIG_NODE_H_
#define _RIG_NODE_H_

#include <cogl/cogl.h>
#include "rig.h"

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
  float value[3];
} RigNodeVec3;

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
rig_node_free (void *node, void *user_data);

RigNodeFloat *
rig_node_new_for_float (float t, float value);

RigNodeVec3 *
rig_node_new_for_vec3 (float t, const float value[3]);

RigNodeQuaternion *
rig_node_new_for_quaternion (float t, float angle, float x, float y, float z);

GList *
rig_nodes_find_less_than (GList *start, float t);

GList *
rig_nodes_find_less_than_equal (GList *start, float t);

GList *
rig_nodes_find_greater_than (GList *start, float t);

GList *
rig_nodes_find_first (GList *pos);

GList *
rig_nodes_find_last (GList *pos);

GList *
rig_nodes_find_greater_than_equal (GList *start, float t);

#endif /* _RIG_NODE_H_ */
