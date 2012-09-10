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
  RigProperty *progress_prop;
  RigProperty *prop;
  GQueue nodes;
  GList *pos;
};

extern RigType rig_path_type;

typedef struct _RigPath RigPath;

#define RIG_PATH(x) ((RigPath *) x)

void
rig_path_lerp_property (RigPath *path, float t);

void
rig_path_free (RigPath *path);

RigProperty *
rig_path_get_property (RigPath *path);

RigPath *
rig_path_new_for_property (RigContext *ctx,
                           RigProperty *progress_prop,
                           RigProperty *path_prop);

void
rig_path_insert_vec3 (RigPath *path,
                      float t,
                      const float value[3]);

void
rig_path_insert_float (RigPath *path,
                       float t,
                       float value);

void
rig_path_insert_quaternion (RigPath *path,
                            float t,
                            float angle,
                            float x,
                            float y,
                            float z);

void
rig_path_lerp_property (RigPath *path, float t);

#endif /* _RIG_PATH_H_ */
