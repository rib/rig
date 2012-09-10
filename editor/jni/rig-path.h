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

#ifndef _RIG_PATH_H_
#define _RIG_PATH_H_

#include "rig.h"
#include "rig-node.h"

struct _RigPath
{
  RigContext *ctx;
  RigPropertyType type;
  GQueue nodes;
  GList *pos;
};

extern RigType rig_path_type;

typedef struct _RigPath RigPath;

#define RIG_PATH(x) ((RigPath *) x)

void
rig_path_free (RigPath *path);

RigProperty *
rig_path_get_property (RigPath *path);

RigPath *
rig_path_new (RigContext *ctx,
              RigPropertyType type);

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
                       const RigColor *value);

void
rig_path_lerp_property (RigPath *path,
                        RigProperty *property,
                        float t);

#endif /* _RIG_PATH_H_ */
