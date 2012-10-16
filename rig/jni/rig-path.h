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

typedef struct _RigPath RigPath;

struct _RigPath
{
  RutContext *ctx;
  RutPropertyType type;
  GQueue nodes;
  GList *pos;
  RutList operation_cb_list;
};

typedef enum
{
  RIG_PATH_OPERATION_ADDED,
  RIG_PATH_OPERATION_REMOVED,
  RIG_PATH_OPERATION_MODIFIED
} RigPathOperation;

typedef void
(* RigPathOperationCallback) (RigPath *path,
                              RigPathOperation op,
                              RigNode *node,
                              void *user_data);

extern RutType rig_path_type;

#define RUT_PATH(x) ((RigPath *) x)

void
rig_path_free (RigPath *path);

RutProperty *
rig_path_get_property (RigPath *path);

RigPath *
rig_path_new (RutContext *ctx,
              RutPropertyType type);

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

RutClosure *
rig_path_add_operation_callback (RigPath *path,
                                 RigPathOperationCallback callback,
                                 void *user_data,
                                 RutClosureDestroyCallback destroy_cb);

#endif /* _RUT_PATH_H_ */
