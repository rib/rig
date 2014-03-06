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

#ifndef __RIG_HAIR_H__
#define __RIG_HAIR_H__

#include "rig-entity.h"
#include "rig-model.h"

enum {
  RIG_HAIR_SHELL_POSITION_BLENDED,
  RIG_HAIR_SHELL_POSITION_UNBLENDED,
  RIG_HAIR_SHELL_POSITION_SHADOW,
  RIG_HAIR_LENGTH,
  RIG_HAIR_N_UNIFORMS
};

enum {
  RIG_HAIR_PROP_LENGTH,
  RIG_HAIR_PROP_DETAIL,
  RIG_HAIR_PROP_DENSITY,
  RIG_HAIR_PROP_THICKNESS,
  RIG_HAIR_N_PROPS
};

typedef struct _RigHair RigHair;
#define RIG_HAIR(p) ((RigHair *)(p))
extern RutType rig_hair_type;

struct _RigHair
{
  RutObjectBase _base;


  RutComponentableProps component;
  RutContext *ctx;
  CoglTexture *circle;
  CoglTexture *fin_texture;
  float *shell_positions;
  GArray *shell_textures;
  GArray *particles;

  float length;
  int n_shells;
  int n_textures;
  int density;
  float thickness;
  int uniform_locations[4];

  RutIntrospectableProps introspectable;
  RutProperty properties[RIG_HAIR_N_PROPS];

  unsigned int dirty_shell_textures: 1;
  unsigned int dirty_fin_texture: 1;
  unsigned int dirty_hair_positions: 1;
};

void
_rig_hair_init_type (void);

RigHair *
rig_hair_new (RutContext *ctx);

float
rig_hair_get_length (RutObject *obj);

void
rig_hair_set_length (RutObject *obj,
                     float length);

int
rig_hair_get_n_shells (RutObject *obj);

void
rig_hair_set_n_shells (RutObject *obj,
                       int n_shells);

int
rig_hair_get_density (RutObject *obj);

void
rig_hair_set_density (RutObject *obj,
                      int density);

float
rig_hair_get_thickness (RutObject *obj);

void
rig_hair_set_thickness (RutObject *obj,
                        float thickness);

void
rig_hair_update_state (RigHair *hair);

float
rig_hair_get_shell_position (RutObject *obj,
                             int shell);

void
rig_hair_set_uniform_location (RutObject *obj,
                               CoglPipeline *pln,
                               int uniform);

void
rig_hair_set_uniform_float_value (RutObject *obj,
                                  CoglPipeline *pln,
                                  int uniform,
                                  float value);

#endif
