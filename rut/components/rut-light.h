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

#ifndef __RUT_LIGHT_H__
#define __RUT_LIGHT_H__

#include "rut-entity.h"

typedef struct _RutLight RutLight;
extern RutType rut_light_type;
#define RUT_LIGHT(p) ((RutLight *)(p))

struct _RutLight
{
  RutObjectProps _parent;
  int ref_count;
  RutComponentableProps component;
  RutColor ambient;
  RutColor diffuse;
  RutColor specular;
};

void
_rut_light_init_type (void);

RutLight *
rut_light_new (void);

void
rut_light_free (RutLight *light);

void
rut_light_set_ambient (RutLight *light,
                       RutColor *ambient);

const RutColor *
rut_light_get_ambient (RutLight *light);

void
rut_light_set_diffuse (RutLight *light,
                       RutColor *diffuse);

const RutColor *
rut_light_get_diffuse (RutLight *light);

void
rut_light_set_specular (RutLight *light,
                        RutColor *specular);

const RutColor *
rut_light_get_specular (RutLight *light);

void
rut_light_add_pipeline (RutLight *light,
                        CoglPipeline *pipeline);

void
rut_light_set_uniforms (RutLight *light,
                        CoglPipeline *pipeline);

#endif /* __RUT_LIGHT_H__ */
