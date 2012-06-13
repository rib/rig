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

#ifndef __RIG_MESH_RENDERER_H__
#define __RIG_MESH_RENDERER_H__

#include <stdint.h>

#include <cogl/cogl.h>

#include "rig-entity.h"
#include "mash-data-loader.h"

#define RIG_MESH_RENDERER(p) ((RigMeshRenderer *)(p))

typedef struct _RigMeshRenderer RigMeshRenderer;

struct _RigMeshRenderer
{
  RigComponent component;
  CoglPrimitive *primitive;
  MashData *mesh_data;
  CoglPipeline *pipeline;
};

RigComponent *  rig_mesh_renderer_new_from_file     (const char   *file,
                                                     CoglPipeline *pipeline);
RigComponent *  rig_mesh_renderer_new_from_template (const char   *name,
                                                     CoglPipeline *pipeline);

void            rig_mesh_renderer_free              (RigMeshRenderer *renderer);

void            rig_mesh_renderer_set_pipeline      (RigMeshRenderer *renderer,
                                                     CoglPipeline    *pipeline);

#endif /* __RIG_MESH_RENDERER_H__ */
