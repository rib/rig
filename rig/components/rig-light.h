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

#ifndef __RIG_LIGHT_H__
#define __RIG_LIGHT_H__

#include "rig-entity.h"

typedef struct _RigLight RigLight;
extern RutType rig_light_type;
#define RIG_LIGHT(p) ((RigLight *)(p))

enum {
  RIG_LIGHT_PROP_AMBIENT,
  RIG_LIGHT_PROP_DIFFUSE,
  RIG_LIGHT_PROP_SPECULAR,
  RIG_LIGHT_N_PROPS
};


struct _RigLight
{
  RutObjectBase _base;
  RutComponentableProps component;
  CoglColor ambient;
  CoglColor diffuse;
  CoglColor specular;

  RutContext *context;

  RutIntrospectableProps introspectable;
  RutProperty properties[RIG_LIGHT_N_PROPS];
};

void
_rig_light_init_type (void);

RigLight *
rig_light_new (RutContext *context);

void
rig_light_free (RigLight *light);

void
rig_light_set_ambient (RutObject *light,
                       const CoglColor *ambient);

const CoglColor *
rig_light_get_ambient (RutObject *light);

void
rig_light_set_diffuse (RutObject *light,
                       const CoglColor *diffuse);

const CoglColor *
rig_light_get_diffuse (RigLight *light);

void
rig_light_set_specular (RutObject *light,
                        const CoglColor *specular);

const CoglColor *
rig_light_get_specular (RigLight *light);

void
rig_light_add_pipeline (RigLight *light,
                        CoglPipeline *pipeline);

void
rig_light_set_uniforms (RigLight *light,
                        CoglPipeline *pipeline);

#endif /* __RIG_LIGHT_H__ */
