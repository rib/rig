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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include "rig-node.h"

void
rig_node_integer_lerp (RigNodeInteger *a,
                       RigNodeInteger *b,
                       float t,
                       int *value)
{
  float range = b->t - a->t;
  if (range)
    {
      float offset = t - a->t;
      float factor = offset / range;

      *value = nearbyint (a->value + (b->value - a->value) * factor);
    }
  else
    *value = a->value;
}

void
rig_node_uint32_lerp (RigNodeUint32 *a,
                      RigNodeUint32 *b,
                      float t,
                      uint32_t *value)
{
  float range = b->t - a->t;
  if (range)
    {
      float offset = t - a->t;
      float factor = offset / range;

      *value = nearbyint (a->value + (b->value - a->value) * factor);
    }
  else
    *value = a->value;
}

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
rig_node_double_lerp (RigNodeDouble *a,
                      RigNodeDouble *b,
                      float t,
                      double *value)
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
rig_node_vec4_lerp (RigNodeVec4 *a,
                    RigNodeVec4 *b,
                    float t,
                    float value[4])
{
  float range = b->t - a->t;

  if (range)
    {
      float offset = t - a->t;
      float factor = offset / range;

      value[0] = a->value[0] + (b->value[0] - a->value[0]) * factor;
      value[1] = a->value[1] + (b->value[1] - a->value[1]) * factor;
      value[2] = a->value[2] + (b->value[2] - a->value[2]) * factor;
      value[3] = a->value[3] + (b->value[3] - a->value[3]) * factor;
    }
  else
    memcpy (value, a->value, sizeof (float) * 4);
}

void
rig_node_color_lerp (RigNodeColor *a,
                     RigNodeColor *b,
                     float t,
                     RutColor *value)
{
  float range = b->t - a->t;

  if (range)
    {
      float offset = t - a->t;
      float factor = offset / range;

      value->red =
        a->value.red + (b->value.red - a->value.red) * factor;
      value->green =
        a->value.green + (b->value.green - a->value.green) * factor;
      value->blue =
        a->value.blue + (b->value.blue - a->value.blue) * factor;
      value->alpha =
        a->value.alpha + (b->value.alpha - a->value.alpha) * factor;
    }
  else
    memcpy (value, &a->value, sizeof (RutColor));
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
rig_node_free (RutPropertyType type,
               void *node)
{
  switch (type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      g_slice_free (RigNodeFloat, node);
      return;

    case RUT_PROPERTY_TYPE_DOUBLE:
      g_slice_free (RigNodeDouble, node);
      return;

    case RUT_PROPERTY_TYPE_INTEGER:
      g_slice_free (RigNodeInteger, node);
      return;

    case RUT_PROPERTY_TYPE_UINT32:
      g_slice_free (RigNodeUint32, node);
      return;

    case RUT_PROPERTY_TYPE_VEC3:
      g_slice_free (RigNodeVec3, node);
      return;

    case RUT_PROPERTY_TYPE_VEC4:
      g_slice_free (RigNodeVec4, node);
      return;

    case RUT_PROPERTY_TYPE_QUATERNION:
      g_slice_free (RigNodeQuaternion, node);
      return;

    case RUT_PROPERTY_TYPE_COLOR:
      g_slice_free (RigNodeColor, node);
      return;

      /* These types shouldn't become nodes */
    case RUT_PROPERTY_TYPE_ENUM:
    case RUT_PROPERTY_TYPE_BOOLEAN:
    case RUT_PROPERTY_TYPE_TEXT:
    case RUT_PROPERTY_TYPE_OBJECT:
    case RUT_PROPERTY_TYPE_POINTER:
      break;
    }

  g_warn_if_reached ();
}

RigNodeInteger *
rig_node_new_for_integer (float t, int value)
{
  RigNodeInteger *node = g_slice_new (RigNodeInteger);
  node->t = t;
  node->value = value;
  return node;
}

RigNodeUint32 *
rig_node_new_for_uint32 (float t, uint32_t value)
{
  RigNodeUint32 *node = g_slice_new (RigNodeUint32);
  node->t = t;
  node->value = value;
  return node;
}

RigNodeFloat *
rig_node_new_for_float (float t, float value)
{
  RigNodeFloat *node = g_slice_new (RigNodeFloat);
  node->t = t;
  node->value = value;
  return node;
}

RigNodeDouble *
rig_node_new_for_double (float t, double value)
{
  RigNodeDouble *node = g_slice_new (RigNodeDouble);
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

RigNodeVec4 *
rig_node_new_for_vec4 (float t, const float value[4])
{
  RigNodeVec4 *node = g_slice_new (RigNodeVec4);
  node->t = t;
  memcpy (node->value, value, sizeof (float) * 4);
  return node;
}

RigNodeQuaternion *
rig_node_new_for_quaternion (float t, const CoglQuaternion *value)
{
  RigNodeQuaternion *node = g_slice_new (RigNodeQuaternion);
  node->t = t;
  node->value = *value;

  return node;
}

RigNodeColor *
rig_node_new_for_color (float t, const RutColor *value)
{
  RigNodeColor *node = g_slice_new (RigNodeColor);
  node->t = t;
  node->value = *value;

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
