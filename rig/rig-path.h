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

#ifndef _RUT_PATH_H_
#define _RUT_PATH_H_

#include <rut.h>
#include "rig-node.h"
#include "rut-list.h"

typedef struct _RigPath RigPath;

struct _RigPath
{
  RutObjectBase _base;

  RutContext *ctx;
  RutPropertyType type;
  RutList nodes;
  int length;
  RigNode *pos;
  RutList operation_cb_list;

};

typedef enum
{
  RIG_PATH_OPERATION_ADDED,
  RIG_PATH_OPERATION_REMOVED,
  RIG_PATH_OPERATION_MODIFIED,
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

typedef enum _RigPathDirection
{
  RIG_PATH_DIRECTION_FORWARDS = 1,
  RIG_PATH_DIRECTION_BACKWARDS
} RigPathDirection;

bool
rig_path_find_control_points2 (RigPath *path,
                               float t,
                               RigPathDirection direction,
                               RigNode **n0,
                               RigNode **n1);

void
rig_path_insert_node (RigPath *path,
                      RigNode *node);

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
                            const cg_quaternion_t *value);

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
                       const cg_color_t *value);

bool
rig_path_lerp_property (RigPath *path,
                        RutProperty *property,
                        float t);

bool
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

RigNode *
rig_path_find_nearest (RigPath *path,
                       float t);

typedef void (*RigPathNodeCallback) (RigNode *node, void *user_data);

void
rut_path_foreach_node (RigPath *path,
                       RigPathNodeCallback callback,
                       void *user_data);

#endif /* _RUT_PATH_H_ */
