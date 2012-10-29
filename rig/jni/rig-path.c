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

RutType rig_path_type;

static void
_rig_path_free (void *object)
{
  RigPath *path = RIG_PATH (object);
  RigNode *node, *t;

  rut_closure_list_disconnect_all (&path->operation_cb_list);

  rut_list_for_each_safe (node, t, &path->nodes, list_node)
    rig_node_free (path->type, node);

  rut_refable_unref (path->ctx);
  g_slice_free (RigPath, path);
}

static RutRefCountableVTable
_rig_path_ref_countable_vtable =
  {
    rut_refable_simple_ref,
    rut_refable_simple_unref,
    _rig_path_free
  };

void
_rig_path_init_type (void)
{
  rut_type_init (&rig_path_type);
  rut_type_add_interface (&rig_path_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigPath, ref_count),
                          &_rig_path_ref_countable_vtable);
}

RigPath *
rig_path_new (RutContext *ctx,
              RutPropertyType type)
{
  RigPath *path = g_slice_new (RigPath);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_path_init_type ();

      initialized = TRUE;
    }

  rut_object_init (&path->_parent, &rig_path_type);

  path->ctx = rut_refable_ref (ctx);
  path->ref_count = 1;

  path->type = type;

  rut_list_init (&path->nodes);
  path->pos = NULL;
  path->length = 0;

  rut_list_init (&path->operation_cb_list);

  return path;
}

static size_t
get_type_size (RutPropertyType type)
{
  switch (type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      return sizeof (RigNodeFloat);
    case RUT_PROPERTY_TYPE_DOUBLE:
      return sizeof (RigNodeDouble);
    case RUT_PROPERTY_TYPE_INTEGER:
      return sizeof (RigNodeInteger);
    case RUT_PROPERTY_TYPE_UINT32:
      return sizeof (RigNodeUint32);
    case RUT_PROPERTY_TYPE_VEC3:
      return sizeof (RigNodeVec3);
    case RUT_PROPERTY_TYPE_VEC4:
      return sizeof (RigNodeVec4);
    case RUT_PROPERTY_TYPE_COLOR:
      return sizeof (RigNodeColor);
    case RUT_PROPERTY_TYPE_QUATERNION:
      return sizeof (RigNodeQuaternion);
      /* These types of properties can't be interoplated so they
       * probably shouldn't end up in a path */
    case RUT_PROPERTY_TYPE_ENUM:
    case RUT_PROPERTY_TYPE_BOOLEAN:
    case RUT_PROPERTY_TYPE_TEXT:
    case RUT_PROPERTY_TYPE_OBJECT:
    case RUT_PROPERTY_TYPE_POINTER:
      break;
    }

  g_warn_if_reached ();

  return sizeof (RigNode);
}

RigPath *
rig_path_copy (RigPath *old_path)
{
  RigPath *new_path = rig_path_new (old_path->ctx, old_path->type);
  size_t node_size = get_type_size (old_path->type);
  RigNode *node;

  rut_list_for_each (node, &old_path->nodes, list_node)
    {
      RigNode *new_node = g_slice_copy (node_size, node);
      rut_list_insert (new_path->nodes.prev, &new_node->list_node);
    }
  new_path->length = old_path->length;

  return new_path;
}

/* Finds 1 point either side of the given t using the direction to resolve
 * which points to choose if t corresponds to a specific node.
 */
static CoglBool
path_find_control_points2 (RigPath *path,
                           float t,
                           int direction,
                           RigNode **n0,
                           RigNode **n1)
{
  RigNode *pos;

  if (G_UNLIKELY (rut_list_empty (&path->nodes)))
    return FALSE;

  if (G_UNLIKELY (path->pos == NULL))
    path->pos = rut_container_of (path->nodes.next, path->pos, list_node);

  pos = path->pos;

  /*
   * Note:
   *
   * A node with t exactly == t may only be considered as the first control
   * point moving in the current direction.
   */

  if (direction > 0)
    {
      if (pos->t > t)
        {
          /* > --- T -------- Pos ---- */
          RigNode *tmp = rig_nodes_find_less_than_equal (pos, &path->nodes, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos =
                rut_container_of (path->nodes.next, path->pos, list_node);
              return TRUE;
            }
          pos = tmp;
        }
      else
        {
          /* > --- Pos -------- T ---- */
          RigNode *tmp = rig_nodes_find_greater_than (pos, &path->nodes, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos =
                rut_container_of (path->nodes.prev, path->pos, list_node);
              return TRUE;
            }
          pos = rut_container_of (tmp->list_node.prev, pos, list_node);
        }

      *n0 = pos;
      if (pos->list_node.next == &path->nodes)
        *n1 = pos;
      else
        *n1 = rut_container_of (pos->list_node.next, *n1, list_node);
    }
  else
    {
      if (pos->t > t)
        {
          /* < --- T -------- Pos ---- */
          RigNode *tmp = rig_nodes_find_less_than (pos, &path->nodes, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos =
                rut_container_of (path->nodes.next, path->pos, list_node);
              return TRUE;
            }
          pos = rut_container_of (tmp->list_node.next, pos, list_node);
        }
      else
        {
          /* < --- Pos -------- T ---- */
          RigNode *tmp =
            rig_nodes_find_greater_than_equal (pos, &path->nodes, t);
          if (!tmp)
            {
              *n0 = *n1 = path->pos =
                rut_container_of (path->nodes.prev, path->pos, list_node);
              return TRUE;
            }
          pos = tmp;
        }

      *n0 = pos;
      if (pos->list_node.prev == &path->nodes)
        *n1 = pos;
      else
        *n1 = rut_container_of (pos->list_node.prev, *n1, list_node);
    }

  path->pos = pos;

  return TRUE;
}

/* Finds 2 points either side of the given t using the direction to resolve
 * which points to choose if t corresponds to a specific node. */
#if 0
static CoglBool
path_find_control_points4 (RigPath *path,
                           float t,
                           int direction,
                           RigNode **n0,
                           RigNode **n1,
                           RigNode **n2,
                           RigNode **n3)
{
  GList *l1, *l2;

  if (!path_find_control_links2 (path, t, direction, &l1, &l2))
    return FALSE;

  if (direction > 0)
    {
      if (l1->prev == NULL || l2->next == NULL)
        return FALSE;

      *n0 = l1->prev->data;
      *n3 = l2->next->data;
    }
  else
    {
      if (l1->next == NULL || l2->prev == NULL)
        return FALSE;

      *n0 = l1->next->data;
      *n3 = l2->prev->data;
    }

  *n1 = l1->data;
  *n2 = l2->data;

  return TRUE;
}
#endif

void
rig_path_print (RigPath *path)
{
  RutPropertyType type = path->type;
  RigNode *node;

  g_print ("path=%p\n", path);
  rut_list_for_each (node, &path->nodes, list_node)
    {
      switch (type)
        {
        case RUT_PROPERTY_TYPE_FLOAT:
          {
            RigNodeFloat *f_node = (RigNodeFloat *)node;

            g_print (" t = %f value = %f\n", f_node->base.t, f_node->value);
            break;
          }

        case RUT_PROPERTY_TYPE_VEC3:
          {
            RigNodeVec3 *vec3_node = (RigNodeVec3 *)node;

            g_print (" t = %f value.x = %f .y = %f .z = %f\n",
                     vec3_node->base.t,
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
                     q_node->base.t,
                     q->w, q->x, q->y, q->z);
            break;
          }

        default:
          g_warn_if_reached ();
        }
    }
}

static void
notify_node_added (RigPath *path,
                   RigNode *node)
{
  rut_closure_list_invoke (&path->operation_cb_list,
                           RigPathOperationCallback,
                           path,
                           RIG_PATH_OPERATION_ADDED,
                           node);
}

static void
notify_node_modified (RigPath *path,
                      RigNode *node)
{
  rut_closure_list_invoke (&path->operation_cb_list,
                           RigPathOperationCallback,
                           path,
                           RIG_PATH_OPERATION_MODIFIED,
                           node);
}

RigNode *
rig_path_find_node (RigPath *path,
                    float t)
{
  RigNode *node;

  rut_list_for_each (node, &path->nodes, list_node)
    if (node->t == t)
      return node;

  return NULL;
}

static void
insert_sorted_node (RigPath *path,
                    RigNode *node)
{
  RigNode *insertion_point;

  rut_list_for_each (insertion_point, &path->nodes, list_node)
    if (insertion_point->t >= node->t)
      break;

  /* If no node was found then insertion_point will be pointing to an
   * imaginary node containing the list head which is what we want
   * anyway */

  rut_list_insert (insertion_point->list_node.prev, &node->list_node);

  path->length++;
}

void
rig_path_insert_float (RigPath *path,
                       float t,
                       float value)
{
  RigNodeFloat *node;

#if 0
  g_print ("BEFORE:\n");
  path_print (path);
#endif

  node = (RigNodeFloat *) rig_path_find_node (path, t);

  if (node)
    {
      node->value = value;
      notify_node_modified (path, (RigNode *) node);
    }
  else
    {
      node = rig_node_new_for_float (t, value);
      insert_sorted_node (path, &node->base);
      notify_node_added (path, (RigNode *) node);
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
  RigNodeVec3 *node;

  node = (RigNodeVec3 *) rig_path_find_node (path, t);

  if (node)
    {
      memcpy (node->value, value, sizeof (node->value));
      notify_node_modified (path, (RigNode *) node);
    }
  else
    {
      node = rig_node_new_for_vec3 (t, value);
      insert_sorted_node (path, &node->base);
      notify_node_added (path, (RigNode *) node);
    }
}

void
rig_path_insert_vec4 (RigPath *path,
                      float t,
                      const float value[4])
{
  RigNodeVec4 *node;

  node = (RigNodeVec4 *) rig_path_find_node (path, t);

  if (node)
    {
      memcpy (node->value, value, sizeof (node->value));
      notify_node_modified (path, (RigNode *) node);
    }
  else
    {
      node = rig_node_new_for_vec4 (t, value);
      insert_sorted_node (path, &node->base);
      notify_node_added (path, (RigNode *) node);
    }
}

void
rig_path_insert_quaternion (RigPath *path,
                            float t,
                            const CoglQuaternion *value)
{
  RigNodeQuaternion *node;

#if 0
  g_print ("BEFORE:\n");
  path_print (path);
#endif

  node = (RigNodeQuaternion *) rig_path_find_node (path, t);

  if (node)
    {
      node->value = *value;
      notify_node_modified (path, (RigNode *) node);
    }
  else
    {
      node = rig_node_new_for_quaternion (t, value);
      insert_sorted_node (path, &node->base);
      notify_node_added (path, (RigNode *) node);
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
  RigNodeDouble *node;

  node = (RigNodeDouble *) rig_path_find_node (path, t);

  if (node)
    {
      node->value = value;
      notify_node_modified (path, (RigNode *) node);
    }
  else
    {
      node = rig_node_new_for_double (t, value);
      insert_sorted_node (path, &node->base);
      notify_node_added (path, (RigNode *) node);
    }
}

void
rig_path_insert_integer (RigPath *path,
                         float t,
                         int value)
{
  RigNodeInteger *node;

  node = (RigNodeInteger *) rig_path_find_node (path, t);

  if (node)
    {
      node->value = value;
      notify_node_modified (path, (RigNode *) node);
    }
  else
    {
      node = rig_node_new_for_integer (t, value);
      insert_sorted_node (path, &node->base);
      notify_node_added (path, (RigNode *) node);
    }
}

void
rig_path_insert_uint32 (RigPath *path,
                        float t,
                        uint32_t value)
{
  RigNodeUint32 *node;

  node = (RigNodeUint32 *) rig_path_find_node (path, t);

  if (node)
    {
      node->value = value;
      notify_node_modified (path, (RigNode *) node);
    }
  else
    {
      node = rig_node_new_for_uint32 (t, value);
      insert_sorted_node (path, &node->base);
      notify_node_added (path, (RigNode *) node);
    }
}

void
rig_path_insert_color (RigPath *path,
                       float t,
                       const RutColor *value)
{
  RigNodeColor *node;

  node = (RigNodeColor *) rig_path_find_node (path, t);

  if (node)
    {
      node->value = *value;
      notify_node_modified (path, (RigNode *) node);
    }
  else
    {
      node = rig_node_new_for_color (t, value);
      insert_sorted_node (path, &node->base);
      notify_node_added (path, (RigNode *) node);
    }
}

CoglBool
rig_path_lerp_property (RigPath *path,
                        RutProperty *property,
                        float t)
{
  RigNode *n0, *n1;

  g_return_val_if_fail (property->spec->type == path->type, FALSE);

  if (!path_find_control_points2 (path, t, 1,
                                  &n0,
                                  &n1))
    return FALSE;

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

  return TRUE;
}

CoglBool
rig_path_get_boxed (RigPath *path,
                    float t,
                    RutBoxed *value)
{
  RigNode *node;

  node = rig_path_find_node (path, t);

  if (node == NULL)
    return FALSE;

  return rig_node_box (path->type, node, value);
}

void
rig_path_insert_boxed (RigPath *path,
                       float t,
                       const RutBoxed *value)
{
  g_return_if_fail (value->type == path->type);

  switch (path->type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      rig_path_insert_float (path, t, value->d.float_val);
      return;

    case RUT_PROPERTY_TYPE_DOUBLE:
      rig_path_insert_double (path, t, value->d.double_val);
      return;

    case RUT_PROPERTY_TYPE_INTEGER:
      rig_path_insert_integer (path, t, value->d.integer_val);
      return;

    case RUT_PROPERTY_TYPE_UINT32:
      rig_path_insert_uint32 (path, t, value->d.uint32_val);
      return;

    case RUT_PROPERTY_TYPE_VEC3:
      rig_path_insert_vec3 (path, t, value->d.vec3_val);
      return;

    case RUT_PROPERTY_TYPE_VEC4:
      rig_path_insert_vec4 (path, t, value->d.vec4_val);
      return;

    case RUT_PROPERTY_TYPE_COLOR:
      rig_path_insert_color (path, t, &value->d.color_val);
      return;

    case RUT_PROPERTY_TYPE_QUATERNION:
      rig_path_insert_quaternion (path, t, &value->d.quaternion_val);
      return;

      /* These types of properties can't be interoplated so they
       * probably shouldn't end up in a path */
    case RUT_PROPERTY_TYPE_ENUM:
    case RUT_PROPERTY_TYPE_BOOLEAN:
    case RUT_PROPERTY_TYPE_TEXT:
    case RUT_PROPERTY_TYPE_OBJECT:
    case RUT_PROPERTY_TYPE_POINTER:
      break;
    }

  g_warn_if_reached ();
}

void
rig_path_remove (RigPath *path,
                 float t)
{
  RigNode *node;

  node = rig_path_find_node (path, t);

  if (node)
    rig_path_remove_node (path, node);
}

void
rig_path_remove_node (RigPath *path,
                      RigNode *node)
{
  rut_closure_list_invoke (&path->operation_cb_list,
                           RigPathOperationCallback,
                           path,
                           RIG_PATH_OPERATION_REMOVED,
                           node);
  rut_list_remove (&node->list_node);
  rig_node_free (path->type, node);
  path->length--;

  if (path->pos == node)
    path->pos = NULL;
}

void
rig_path_move_node (RigPath *path,
                    RigNode *node,
                    float new_value)
{
  node->t = new_value;

  rut_closure_list_invoke (&path->operation_cb_list,
                           RigPathOperationCallback,
                           path,
                           RIG_PATH_OPERATION_MOVED,
                           node);
}

RutClosure *
rig_path_add_operation_callback (RigPath *path,
                                 RigPathOperationCallback callback,
                                 void *user_data,
                                 RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&path->operation_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}
