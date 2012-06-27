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

#include "rig-animation-clip.h"

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
rig_animation_clip_update (RigComponent *component,
			   int64_t       time)
{
  RigAnimationClip *clip = RIG_ANIMATION_CLIP (component);
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

/*
 * duration is given in ms in the API, but internally all computations are done
 * in micro seconds
 */
RigComponent *
rig_animation_clip_new (int32_t duration)
{
  RigAnimationClip *renderer;

  renderer = g_slice_new0 (RigAnimationClip);
  renderer->component.type = RIG_COMPONENT_TYPE_ANIMATION_CLIP;
  renderer->component.update = rig_animation_clip_update;
  renderer->duration = duration * 1000;

  return RIG_COMPONENT (renderer);
}

void
rig_animation_clip_free (RigAnimationClip *clip)
{
  if (clip->float_animation_data)
    g_array_unref (clip->float_animation_data);

  if (clip->quaternion_animation_data)
    g_array_unref (clip->quaternion_animation_data);

  g_slice_free (RigAnimationClip, clip);
}

void
rig_animation_clip_add_float (RigAnimationClip *clip,
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
rig_animation_clip_add_quaternion (RigAnimationClip *clip,
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
has_float_animation_data (RigAnimationClip *clip)
{
  return (clip->float_animation_data == NULL ||
          clip->float_animation_data->len == 0);
}

static int
has_quaternion_animation_data (RigAnimationClip *clip)
{
  return (clip->quaternion_animation_data == NULL ||
          clip->quaternion_animation_data->len == 0);
}

void
rig_animation_clip_start (RigAnimationClip *clip,
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
