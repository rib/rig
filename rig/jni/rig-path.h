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

#ifndef _RUT_PATH_H_
#define _RUT_PATH_H_

#include <rut.h>
#include "rig-node.h"
#include "rut-list.h"

typedef struct _RigPath RigPath;

struct _RigPath
{
  RutObjectProps _parent;

  RutContext *ctx;
  RutPropertyType type;
  RutList nodes;
  int length;
  RigNode *pos;
  RutList operation_cb_list;

  int ref_count;
};

typedef enum
{
  RIG_PATH_OPERATION_ADDED,
  RIG_PATH_OPERATION_REMOVED,
  RIG_PATH_OPERATION_MODIFIED,
  RIG_PATH_OPERATION_MOVED
} RigPathOperation;

typedef void
(* RigPathOperationCallback) (RigPath *path,
                              RigPathOperation op,
                              RigNode *node,
                              void *user_data);

extern RutType rig_path_type;

#define RIG_PATH(x) ((RigPath *) x)

RutProperty *
rig_path_get_property (RigPath *path);

RigPath *
rig_path_new (RutContext *ctx,
              RutPropertyType type);

RigPath *
rig_path_copy (RigPath *path);

void
rig_path_insert_vec3 (RigPath *path,
                      float t,
                      const float value[3]);

void
rig_path_insert_vec4 (RigPath *path,
                      float t,
                      const float value[4]);

void
rig_path_insert_float (RigPath *path,
                       float t,
                       float value);

void
rig_path_insert_quaternion (RigPath *path,
                            float t,
                            const CoglQuaternion *value);

void
rig_path_insert_double (RigPath *path,
                        float t,
                        double value);

void
rig_path_insert_integer (RigPath *path,
                         float t,
                         int value);

void
rig_path_insert_uint32 (RigPath *path,
                        float t,
                        uint32_t value);

void
rig_path_insert_color (RigPath *path,
                       float t,
                       const RutColor *value);

CoglBool
rig_path_lerp_property (RigPath *path,
                        RutProperty *property,
                        float t);

CoglBool
rig_path_get_boxed (RigPath *path,
                    float t,
                    RutBoxed *value);

void
rig_path_insert_boxed (RigPath *path,
                       float t,
                       const RutBoxed *value);

void
rig_path_remove (RigPath *path,
                 float t);

void
rig_path_remove_node (RigPath *path,
                      RigNode *node);

/**
 * rig_path_move_node:
 * @path: A #RigPath
 * @node: A node to move
 * @t: The new value
 *
 * Moves the given node to a new time position. Note that this
 * shouldn't be used to move a node to a location that would require
 * changing the order of the nodes
 */
void
rig_path_move_node (RigPath *path,
                    RigNode *node,
                    float new_value);

RutClosure *
rig_path_add_operation_callback (RigPath *path,
                                 RigPathOperationCallback callback,
                                 void *user_data,
                                 RutClosureDestroyCallback destroy_cb);

/**
 * rig_path_find_node:
 * @path: A #RigPath
 * @t: The time to search for
 *
 * Finds and returns a node which has exactly the time @t. The
 * returned node is owned by the path but is guaranteed to remain
 * alive until either the path is destroyed or the operation
 * %RIG_PATH_OPERATION_REMOVED is reported with the node's timestamp.
 */
RigNode *
rig_path_find_node (RigPath *path,
                    float t);

#endif /* _RUT_PATH_H_ */
