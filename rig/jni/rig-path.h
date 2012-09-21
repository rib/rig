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

struct _RigPath
{
  RutContext *ctx;
  RutPropertyType type;
  GQueue nodes;
  GList *pos;
};

extern RutType rig_path_type;

typedef struct _RigPath RigPath;

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

void
rig_path_lerp_property (RigPath *path,
                        RutProperty *property,
                        float t);

#endif /* _RUT_PATH_H_ */
