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

#include "rut-animation-clip.h"
#include "rut-context.h"

typedef struct
{
  FloatSetter setter;
  void *object;
  float start, end;
  float (*easing) (float progress);
} FloatAnimationData;

typedef struct
{
  QuaternionSetter setter;
  void *object;
  CoglQuaternion start, end;
  float (*easing) (float progress);
} QuaternionAnimationData;

static float
easing_linear (float progress)
{
  return progress;
}

static void
rut_animation_clip_update (RutObject *object,
			   int64_t time)
{
  RutAnimationClip *clip = object;
  float progress;
  int i;

  if (!clip->started)
    return;

  if (time >= (clip->start_time + clip->duration))
    {
      clip->started = FALSE;
      return;
    }

  /* everything is in micro seconds */
  progress = (time - clip->start_time) / (float) clip->duration;

  /* update floats */
  if (clip->float_animation_data)
    {
      FloatAnimationData *data;
      float new_value;

      for (i = 0; i < clip->float_animation_data->len; i ++)
        {
          data = &g_array_index (clip->float_animation_data,
                                 FloatAnimationData,
                                 i);

          new_value = data->start +
            (data->end - data->start) * data->easing (progress);

          data->setter (data->object, new_value);
      }
    }

  /* update quaternions */
  if (clip->quaternion_animation_data)
    {
      QuaternionAnimationData *data;
      CoglQuaternion new_value;

      for (i = 0; i < clip->quaternion_animation_data->len; i ++)
        {
          data = &g_array_index (clip->quaternion_animation_data,
                                 QuaternionAnimationData,
                                 i);

          cogl_quaternion_slerp (&new_value,
                                 &data->start, &data->end,
                                 data->easing (progress));

          data->setter (data->object, &new_value);
        }
    }
}

RutType rut_animation_clip_type;

static RutComponentableVTable _rut_animation_clip_componentable_vtable = {
  .update = rut_animation_clip_update
};

void
_rut_animation_clip_init_type (void)
{
  rut_type_init (&rut_animation_clip_type);
  rut_type_add_interface (&rut_animation_clip_type,
                           RUT_INTERFACE_ID_COMPONENTABLE,
                           offsetof (RutAnimationClip, component),
                           &_rut_animation_clip_componentable_vtable);
}

/*
 * duration is given in ms in the API, but internally all computations are done
 * in micro seconds
 */
RutAnimationClip *
rut_animation_clip_new (int32_t duration)
{
  RutAnimationClip *clip;

  clip = g_slice_new0 (RutAnimationClip);

  rut_object_init (&clip->_parent, &rut_animation_clip_type);

  clip->component.type = RUT_COMPONENT_TYPE_ANIMATION_CLIP;
  clip->duration = duration * 1000;

  return clip;
}

void
rut_animation_clip_free (RutAnimationClip *clip)
{
  if (clip->float_animation_data)
    g_array_unref (clip->float_animation_data);

  if (clip->quaternion_animation_data)
    g_array_unref (clip->quaternion_animation_data);

  g_slice_free (RutAnimationClip, clip);
}

void
rut_animation_clip_add_float (RutAnimationClip *clip,
			      void             *object,
			      FloatGetter       getter,
			      FloatSetter       setter,
			      float             end_value)
{
  FloatAnimationData data;

  if (clip->float_animation_data == NULL)
    {
      clip->float_animation_data =
        g_array_sized_new (FALSE, FALSE, sizeof (FloatAnimationData), 1);
    }

  data.object = object;
  data.setter = setter;
  data.start = getter (object);
  data.end = end_value;
  data.easing = easing_linear;

  g_array_append_val (clip->float_animation_data, data);
}

void
rut_animation_clip_add_quaternion (RutAnimationClip *clip,
				   void             *object,
				   QuaternionGetter  getter,
				   QuaternionSetter  setter,
				   CoglQuaternion   *end_value)
{
  QuaternionAnimationData data;

  if (clip->quaternion_animation_data == NULL)
    {
      clip->quaternion_animation_data =
        g_array_sized_new (FALSE, FALSE, sizeof (QuaternionAnimationData), 1);
    }

  data.object = object;
  data.setter = setter;
  cogl_quaternion_init_from_quaternion (&data.start, getter (object));
  cogl_quaternion_init_from_quaternion (&data.end, end_value);
  data.easing = easing_linear;

  g_array_append_val (clip->quaternion_animation_data, data);
}

static int
has_float_animation_data (RutAnimationClip *clip)
{
  return (clip->float_animation_data == NULL ||
          clip->float_animation_data->len == 0);
}

static int
has_quaternion_animation_data (RutAnimationClip *clip)
{
  return (clip->quaternion_animation_data == NULL ||
          clip->quaternion_animation_data->len == 0);
}

void
rut_animation_clip_start (RutAnimationClip *clip,
			  int64_t           start_time)
{
  if (!has_float_animation_data (clip) &&
      !has_quaternion_animation_data (clip))
    {
      g_warning ("Tried to start an animation clip without anything to "
                 "animate");
      return;
    }

  if (clip->started)
    return;

  clip->start_time = start_time;

  clip->started = TRUE;
}
