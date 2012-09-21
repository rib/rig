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

#include "rig-path.h"
#include "rig-node.h"

void
rig_path_free (RigPath *path)
{
  g_queue_foreach (&path->nodes,
                   rig_node_free,
                   GUINT_TO_POINTER (path->type));
  g_queue_clear (&path->nodes);
  rut_ref_countable_unref (path->ctx);
  g_slice_free (RigPath, path);
}

RigPath *
rig_path_new (RutContext *ctx,
              RutPropertyType type)
{
  RigPath *path = g_slice_new (RigPath);

  path->ctx = rut_ref_countable_ref (ctx);

  path->type = type;

  g_queue_init (&path->nodes);
  path->pos = NULL;

  return path;
}

/* Finds 1 point either side of the given t using the direction to resolve
 * which points to choose if t corresponds to a specific node.
 */
static void
path_find_control_links2 (RigPath *path,
                          float t,
                          int direction,
                          GList **n0,
                          GList **n1)
{
  GList *pos;
  RigNode *pos_node;

  if (G_UNLIKELY (path->nodes.head == NULL))
    {
      *n0 = NULL;
      *n1= NULL;
      return;
    }

  if (G_UNLIKELY (path->pos == NULL))
    path->pos = path->nodes.head;

  pos = path->pos;
  pos_node = pos->data;

  /*
   * Note:
   *
   * A node with t exactly == t may only be considered as the first control
   * point moving in the current direction.
   */

  if (direction > 0)
    {
      if (pos_node->t > t)
        {
          /* > --- T -------- Pos ---- */
          GList *tmp = rig_nodes_find_less_than_equal (pos, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos = rig_nodes_find_first (pos);
              return;
            }
          pos = tmp;
        }
      else
        {
          /* > --- Pos -------- T ---- */
          GList *tmp = rig_nodes_find_greater_than (pos, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos = rig_nodes_find_last (pos);
              return;
            }
          pos = tmp->prev;
        }

      *n0 = pos;
      *n1 = pos->next;
    }
  else
    {
      if (pos_node->t > t)
        {
          /* < --- T -------- Pos ---- */
          GList *tmp = rig_nodes_find_less_than (pos, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos = rig_nodes_find_first (pos);
              return;
            }
          pos = tmp->next;
        }
      else
        {
          /* < --- Pos -------- T ---- */
          GList *tmp = rig_nodes_find_greater_than_equal (pos, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos = rig_nodes_find_last (pos);
              return;
            }
          pos = tmp;
        }

      *n0 = pos;
      *n1 = pos->prev;
    }

  path->pos = pos;
}

void
path_find_control_points2 (RigPath *path,
                           float t,
                           int direction,
                           RigNode **n0,
                           RigNode **n1)
{
  GList *l0, *l1;
  path_find_control_links2 (path, t, direction, &l0, &l1);
  *n0 = l0->data;
  *n1 = l1->data;
}

/* Finds 2 points either side of the given t using the direction to resolve
 * which points to choose if t corresponds to a specific node. */
void
path_find_control_points4 (RigPath *path,
                           float t,
                           int direction,
                           RigNode **n0,
                           RigNode **n1,
                           RigNode **n2,
                           RigNode **n3)
{
  GList *l1, *l2;

  path_find_control_links2 (path, t, direction, &l1, &l2);

  if (direction > 0)
    {
      *n0 = l1->prev->data;
      *n3 = l2->next->data;
    }
  else
    {
      *n0 = l1->next->data;
      *n3 = l2->prev->data;
    }

  *n1 = l1->data;
  *n2 = l2->data;
}

static void
node_print (void *node, void *user_data)
{
  RutPropertyType type = GPOINTER_TO_UINT (user_data);
  switch (type)
    {
      case RUT_PROPERTY_TYPE_FLOAT:
	{
	  RigNodeFloat *f_node = (RigNodeFloat *)node;

	  g_print (" t = %f value = %f\n", f_node->t, f_node->value);
          break;
	}

      case RUT_PROPERTY_TYPE_VEC3:
	{
	  RigNodeVec3 *vec3_node = (RigNodeVec3 *)node;

	  g_print (" t = %f value.x = %f .y = %f .z = %f\n",
                   vec3_node->t,
                   vec3_node->value[0],
                   vec3_node->value[1],
                   vec3_node->value[2]);
          break;
	}

      case RUT_PROPERTY_TYPE_QUATERNION:
	{
	  RigNodeQuaternion *q_node = (RigNodeQuaternion *)node;
	  const CoglQuaternion *q = &q_node->value;
	  g_print (" t = %f [%f (%f, %f, %f)]\n",
                   q_node->t,
                   q->w, q->x, q->y, q->z);
	  break;
	}

      default:
        g_warn_if_reached ();
    }
}

void
rig_path_print (RigPath *path)
{
  g_print ("path=%p\n", path);
  g_queue_foreach (&path->nodes,
                   node_print,
                   GUINT_TO_POINTER (path->type));
}

static int
path_find_t_cb (gconstpointer a, gconstpointer b)
{
  const RigNode *node = a;
  const float *t = b;

  if (node->t == *t)
    return 0;

  return 1;
}

static int
path_node_sort_t_func (const RigNode *a,
                       const RigNode *b,
                       void *user_data)
{
  if (a->t == b->t)
    return 0;
  else if (a->t < b->t)
    return -1;
  else
    return 1;
}

void
rig_path_insert_float (RigPath *path,
                       float t,
                       float value)
{
  GList *link;
  RigNodeFloat *node;

#if 0
  g_print ("BEFORE:\n");
  path_print (path);
#endif

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      node->value = value;
    }
  else
    {
      node = rig_node_new_for_float (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func,
                             NULL);
    }

#if 0
  g_print ("AFTER:\n");
  path_print (path);
#endif
}

void
rig_path_insert_vec3 (RigPath *path,
                      float t,
                      const float value[3])
{
  GList *link;
  RigNodeVec3 *node;

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      memcpy (node->value, value, sizeof (value));
    }
  else
    {
      node = rig_node_new_for_vec3 (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func,
                             NULL);
    }
}

void
rig_path_insert_vec4 (RigPath *path,
                      float t,
                      const float value[4])
{
  GList *link;
  RigNodeVec4 *node;

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      memcpy (node->value, value, sizeof (value));
    }
  else
    {
      node = rig_node_new_for_vec4 (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func,
                             NULL);
    }
}

void
rig_path_insert_quaternion (RigPath *path,
                            float t,
                            const CoglQuaternion *value)
{
  GList *link;
  RigNodeQuaternion *node;

#if 0
  g_print ("BEFORE:\n");
  path_print (path);
#endif

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      node->value = *value;
    }
  else
    {
      node = rig_node_new_for_quaternion (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func, NULL);
    }

#if 0
  g_print ("AFTER:\n");
  path_print (path);
#endif
}

void
rig_path_insert_double (RigPath *path,
                        float t,
                        double value)
{
  GList *link;
  RigNodeDouble *node;

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      node->value = value;
    }
  else
    {
      node = rig_node_new_for_double (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func,
                             NULL);
    }
}

void
rig_path_insert_integer (RigPath *path,
                         float t,
                         int value)
{
  GList *link;
  RigNodeInteger *node;

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      node->value = value;
    }
  else
    {
      node = rig_node_new_for_integer (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func,
                             NULL);
    }
}

void
rig_path_insert_uint32 (RigPath *path,
                        float t,
                        uint32_t value)
{
  GList *link;
  RigNodeUint32 *node;

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      node->value = value;
    }
  else
    {
      node = rig_node_new_for_uint32 (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func,
                             NULL);
    }
}

void
rig_path_insert_color (RigPath *path,
                       float t,
                       const RutColor *value)
{
  GList *link;
  RigNodeColor *node;

  link = g_queue_find_custom (&path->nodes, &t, path_find_t_cb);

  if (link)
    {
      node = link->data;
      node->value = *value;
    }
  else
    {
      node = rig_node_new_for_color (t, value);
      g_queue_insert_sorted (&path->nodes, node,
                             (GCompareDataFunc)path_node_sort_t_func, NULL);
    }
}

void
rig_path_lerp_property (RigPath *path,
                        RutProperty *property,
                        float t)
{
  RigNode *n0, *n1;

  g_return_if_fail (property->spec->type == path->type);

  path_find_control_points2 (path, t, 1,
                             &n0,
                             &n1);

  switch (path->type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      {
        float value;

        rig_node_float_lerp ((RigNodeFloat *)n0, (RigNodeFloat *)n1, t, &value);
        rut_property_set_float (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_DOUBLE:
      {
        double value;

        rig_node_double_lerp ((RigNodeDouble *) n0,
                              (RigNodeDouble *) n1,
                              t,
                              &value);
        rut_property_set_double (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_INTEGER:
      {
        int value;

        rig_node_integer_lerp ((RigNodeInteger *) n0,
                               (RigNodeInteger *) n1,
                               t,
                               &value);
        rut_property_set_integer (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_UINT32:
      {
        uint32_t value;

        rig_node_uint32_lerp ((RigNodeUint32 *) n0,
                              (RigNodeUint32 *) n1,
                              t,
                              &value);
        rut_property_set_uint32 (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_VEC3:
      {
        float value[3];
        rig_node_vec3_lerp ((RigNodeVec3 *)n0, (RigNodeVec3 *)n1, t, value);
        rut_property_set_vec3 (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_VEC4:
      {
        float value[4];
        rig_node_vec4_lerp ((RigNodeVec4 *)n0, (RigNodeVec4 *)n1, t, value);
        rut_property_set_vec4 (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_COLOR:
      {
        RutColor value;
        rig_node_color_lerp ((RigNodeColor *)n0, (RigNodeColor *)n1, t, &value);
        rut_property_set_color (&path->ctx->property_ctx, property, &value);
        break;
      }
    case RUT_PROPERTY_TYPE_QUATERNION:
      {
        CoglQuaternion value;
        rig_node_quaternion_lerp ((RigNodeQuaternion *)n0,
                                  (RigNodeQuaternion *)n1, t, &value);
        rut_property_set_quaternion (&path->ctx->property_ctx,
                                     property, &value);
        break;
      }

      /* These types of properties can't be interoplated so they
       * probably shouldn't end up in a path */
    case RUT_PROPERTY_TYPE_ENUM:
    case RUT_PROPERTY_TYPE_BOOLEAN:
    case RUT_PROPERTY_TYPE_TEXT:
    case RUT_PROPERTY_TYPE_OBJECT:
    case RUT_PROPERTY_TYPE_POINTER:
      g_warn_if_reached ();
      break;
    }
}
