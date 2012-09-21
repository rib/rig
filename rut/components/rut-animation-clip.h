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

#ifndef __RUT_ANIMATION_CLIP_H__
#define __RUT_ANIMATION_CLIP_H__

#include <cogl/cogl.h>

#include "rut-entity.h"

#define FLOAT_GETTER(func) ((FloatGetter) (func))
#define FLOAT_SETTER(func) ((FloatSetter) (func))

typedef float (*FloatGetter) (void *object);
typedef void (*FloatSetter) (void *object, float f);

#define QUATERNION_GETTER(func) ((QuaternionGetter) (func))
#define QUATERNION_SETTER(func) ((QuaternionSetter) (func))

typedef CoglQuaternion * (*QuaternionGetter) (void *object);
typedef void (*QuaternionSetter) (void *object, CoglQuaternion *quaternion);

#define RUT_ANIMATION_CLIP(p) ((RutAnimationClip *)(p))
typedef struct _AnimationClip RutAnimationClip;
extern RutType rut_animation_clip_type;

struct _AnimationClip
{
  RutObjectProps _parent;
  RutComponentableProps component;
  int64_t duration;   /* micro seconds */
  int64_t start_time; /* micro seconds */
  GArray *float_animation_data;
  GArray *quaternion_animation_data;
  unsigned int started:1;
};

void
_rut_animation_clip_init_type (void);

RutAnimationClip *
rut_animation_clip_new (int32_t duration);

void
rut_animation_clip_free (RutAnimationClip *clip);

void
rut_animation_clip_add_float (RutAnimationClip *clip,
                              void *object,
                              FloatGetter getter,
                              FloatSetter setter,
                              float end_value);

void
rut_animation_clip_add_quaternion (RutAnimationClip *clip,
                                   void *object,
                                   QuaternionGetter getter,
                                   QuaternionSetter setter,
                                   CoglQuaternion *end_value);

void
rut_animation_clip_start (RutAnimationClip *clip,
                          int64_t start_time);

void
rut_animation_clip_stop (RutAnimationClip *clip);

#endif /* __RUT_ANIMATION_CLIP_H__ */
