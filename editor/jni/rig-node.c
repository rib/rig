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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rig-node.h"

void
rig_node_float_lerp (RigNodeFloat *a,
                     RigNodeFloat *b,
                     float t,
                     float *value)
{
  float range = b->t - a->t;
  if (range)
    {
      float offset = t - a->t;
      float factor = offset / range;

      *value = a->value + (b->value - a->value) * factor;
    }
  else
    *value = a->value;
}

void
rig_node_vec3_lerp (RigNodeVec3 *a,
                    RigNodeVec3 *b,
                    float t,
                    float value[3])
{
  float range = b->t - a->t;

  if (range)
    {
      float offset = t - a->t;
      float factor = offset / range;

      value[0] = a->value[0] + (b->value[0] - a->value[0]) * factor;
      value[1] = a->value[1] + (b->value[1] - a->value[1]) * factor;
      value[2] = a->value[2] + (b->value[2] - a->value[2]) * factor;
    }
  else
    memcpy (value, a->value, sizeof (float) * 3);
}

void
rig_node_quaternion_lerp (RigNodeQuaternion *a,
                          RigNodeQuaternion *b,
                          float t,
                          CoglQuaternion *value)
{
  float range = b->t - a->t;
  if (range)
    {
      float offset = t - a->t;
      float factor = offset / range;

      cogl_quaternion_nlerp (value, &a->value, &b->value, factor);
    }
  else
    *value = a->value;
}

void
rig_node_free (void *node, void *user_data)
{
  RigPropertyType type = GPOINTER_TO_UINT (user_data);

  switch (type)
    {
    case RIG_PROPERTY_TYPE_FLOAT:
      g_slice_free (RigNodeFloat, node);
      break;

    case RIG_PROPERTY_TYPE_VEC3:
      g_slice_free (RigNodeVec3, node);
      break;

    case RIG_PROPERTY_TYPE_QUATERNION:
      g_slice_free (RigNodeQuaternion, node);
      break;

    default:
      g_warn_if_reached ();
    }
}

RigNodeFloat *
rig_node_new_for_float (float t, float value)
{
  RigNodeFloat *node = g_slice_new (RigNodeFloat);
  node->t = t;
  node->value = value;
  return node;
}

RigNodeVec3 *
rig_node_new_for_vec3 (float t, const float value[3])
{
  RigNodeVec3 *node = g_slice_new (RigNodeVec3);
  node->t = t;
  memcpy (node->value, value, sizeof (float) * 3);
  return node;
}


RigNodeQuaternion *
rig_node_new_for_quaternion (float t, float angle, float x, float y, float z)
{
  RigNodeQuaternion *node = g_slice_new (RigNodeQuaternion);
  node->t = t;
  cogl_quaternion_init (&node->value, angle, x, y, z);

  return node;
}

GList *
rig_nodes_find_less_than (GList *start, float t)
{
  GList *l;

  for (l = start; l; l = l->prev)
    {
      RigNode *node = l->data;
      if (node->t < t)
        return l;
    }

  return NULL;
}

GList *
rig_nodes_find_less_than_equal (GList *start, float t)
{
  GList *l;

  for (l = start; l; l = l->prev)
    {
      RigNode *node = l->data;
      if (node->t <= t)
        return l;
    }

  return NULL;
}

GList *
rig_nodes_find_greater_than (GList *start, float t)
{
  GList *l;

  for (l = start; l; l = l->next)
    {
      RigNode *node = l->data;
      if (node->t > t)
        return l;
    }

  return NULL;
}

GList *
rig_nodes_find_first (GList *pos)
{
  GList *l;

  for (l = pos; l->prev; l = l->prev)
    ;

  return l;
}

GList *
rig_nodes_find_last (GList *pos)
{
  GList *l;

  for (l = pos; l->next; l = l->next)
    ;

  return l;
}

GList *
rig_nodes_find_greater_than_equal (GList *start, float t)
{
  GList *l;

  for (l = start; l; l = l->next)
    {
      RigNode *node = l->data;
      if (node->t >= t)
        return l;
    }

  return NULL;
}
