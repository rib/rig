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

#ifndef __RIG_ANIMATION_CLIP_H__
#define __RIG_ANIMATION_CLIP_H__

#include <cogl/cogl.h>

#include "rig-entity.h"

#define FLOAT_GETTER(func) ((FloatGetter) (func))
#define FLOAT_SETTER(func) ((FloatSetter) (func))

typedef float (*FloatGetter) (void *object);
typedef void (*FloatSetter) (void *object, float f);

#define QUATERNION_GETTER(func) ((QuaternionGetter) (func))
#define QUATERNION_SETTER(func) ((QuaternionSetter) (func))

typedef CoglQuaternion * (*QuaternionGetter) (void *object);
typedef void (*QuaternionSetter) (void *object, CoglQuaternion *quaternion);

#define RIG_ANIMATION_CLIP(p) ((RigAnimationClip *)(p))

typedef struct _AnimationClip RigAnimationClip;

struct _AnimationClip
{
  RigComponent component;
  int64_t duration;   /* micro seconds */
  int64_t start_time; /* micro seconds */
  GArray *float_animation_data;
  GArray *quaternion_animation_data;
  unsigned int started:1;
};

RigComponent *	rig_animation_clip_new            (int32_t duration);

void		rig_animation_clip_free           (RigAnimationClip *clip);

void		rig_animation_clip_add_float      (RigAnimationClip *clip,
						   void          *object,
						   FloatGetter    getter,
						   FloatSetter    setter,
						   float          end_value);
void		rig_animation_clip_add_quaternion (RigAnimationClip    *clip,
						   void             *object,
						   QuaternionGetter  getter,
						   QuaternionSetter  setter,
						   CoglQuaternion   *end_value);
void		rig_animation_clip_start          (RigAnimationClip *clip,
						   int64_t           start_time);
void		rig_animation_clip_stop           (RigAnimationClip *clip);

#endif /* __RIG_ANIMATION_CLIP_H__ */
