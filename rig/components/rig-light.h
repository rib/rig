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

#ifndef __RIG_LIGHT_H__
#define __RIG_LIGHT_H__

#include "rig-entity.h"

typedef struct _RigLight RigLight;
extern RutType rig_light_type;

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
  cg_color_t ambient;
  cg_color_t diffuse;
  cg_color_t specular;

  RutContext *context;

  RutIntrospectableProps introspectable;
  RutProperty properties[RIG_LIGHT_N_PROPS];
};

RigLight *
rig_light_new (RutContext *context);

void
rig_light_free (RigLight *light);

void
rig_light_set_ambient (RutObject *light,
                       const cg_color_t *ambient);

const cg_color_t *
rig_light_get_ambient (RutObject *light);

void
rig_light_set_diffuse (RutObject *light,
                       const cg_color_t *diffuse);

const cg_color_t *
rig_light_get_diffuse (RigLight *light);

void
rig_light_set_specular (RutObject *light,
                        const cg_color_t *specular);

const cg_color_t *
rig_light_get_specular (RigLight *light);

void
rig_light_add_pipeline (RigLight *light,
                        cg_pipeline_t *pipeline);

void
rig_light_set_uniforms (RigLight *light,
                        cg_pipeline_t *pipeline);

#endif /* __RIG_LIGHT_H__ */
