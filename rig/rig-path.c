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
  RigPath *path = object;
  RigNode *node, *t;

  rut_closure_list_disconnect_all (&path->operation_cb_list);

  rut_list_for_each_safe (node, t, &path->nodes, list_node)
    rig_node_free (node);

  rut_refable_unref (path->ctx);
  g_slice_free (RigPath, path);
}

static RutRefableVTable
_rig_path_refable_vtable =
  {
    rut_refable_simple_ref,
    rut_refable_simple_unref,
    _rig_path_free
  };

void
_rig_path_init_type (void)
{
  rut_type_init (&rig_path_type, "RigPath");
  rut_type_add_interface (&rig_path_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigPath, ref_count),
                          &_rig_path_refable_vtable);
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

RigPath *
rig_path_copy (RigPath *old_path)
{
  RigPath *new_path = rig_path_new (old_path->ctx, old_path->type);
  RigNode *node;

  rut_list_for_each (node, &old_path->nodes, list_node)
    {
      RigNode *new_node = rig_node_copy (node);
      rut_list_insert (new_path->nodes.prev, &new_node->list_node);
    }
  new_path->length = old_path->length;

  return new_path;
}

/* Finds 1 point either side of the given t using the direction to resolve
 * which points to choose if t corresponds to a specific node.
 */
bool
rig_path_find_control_points2 (RigPath *path,
                               float t,
                               RigPathDirection direction,
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

  if (direction == RIG_PATH_DIRECTION_FORWARDS)
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

      *n0 = l1->prev->engine;
      *n3 = l2->next->engine;
    }
  else
    {
      if (l1->next == NULL || l2->prev == NULL)
        return FALSE;

      *n0 = l1->next->engine;
      *n3 = l2->prev->engine;
    }

  *n1 = l1->engine;
  *n2 = l2->engine;

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
            g_print (" t = %f value = %f\n",
                     node->t,
                     node->boxed.d.float_val);
            break;
          }

        case RUT_PROPERTY_TYPE_VEC3:
          {
            g_print (" t = %f value.x = %f .y = %f .z = %f\n",
                     node->t,
                     node->boxed.d.vec3_val[0],
                     node->boxed.d.vec3_val[1],
                     node->boxed.d.vec3_val[2]);
            break;
          }

        case RUT_PROPERTY_TYPE_QUATERNION:
          {
            const CoglQuaternion *q = &node->boxed.d.quaternion_val;
            g_print (" t = %f [%f (%f, %f, %f)]\n",
                     node->t,
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

RigNode *
rig_path_find_nearest (RigPath *path,
                       float t)
{
  float min_dt = G_MAXFLOAT;
  RigNode *min_dt_node = NULL;
  RigNode *node;

  rut_list_for_each (node, &path->nodes, list_node)
    {
      float dt = fabs (node->t - t);
      if (dt < min_dt)
        {
          min_dt = dt;
          min_dt_node = node;
        }
      else
        return min_dt_node;
    }

  return min_dt_node;;
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
rig_path_insert_node (RigPath *path,
                      RigNode *node)
{
  g_return_if_fail (rig_path_find_node (path, node->t) == NULL);

  insert_sorted_node (path, node);
  notify_node_added (path, node);
}

void
rig_path_insert_float (RigPath *path,
                       float t,
                       float value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      node->boxed.d.float_val = value;
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_float (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_vec3 (RigPath *path,
                      float t,
                      const float value[3])
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      memcpy (node->boxed.d.vec3_val,
              value,
              sizeof (float) * 3);
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_vec3 (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_vec4 (RigPath *path,
                      float t,
                      const float value[4])
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      memcpy (node->boxed.d.vec4_val,
              value,
              sizeof (float) * 4);
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_vec4 (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_quaternion (RigPath *path,
                            float t,
                            const CoglQuaternion *value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      node->boxed.d.quaternion_val = *value;
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_quaternion (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_double (RigPath *path,
                        float t,
                        double value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      node->boxed.d.double_val = value;
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_double (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_integer (RigPath *path,
                         float t,
                         int value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      node->boxed.d.integer_val = value;
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_integer (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_uint32 (RigPath *path,
                        float t,
                        uint32_t value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      node->boxed.d.uint32_val = value;
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_uint32 (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_color (RigPath *path,
                       float t,
                       const CoglColor *value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      node->boxed.d.color_val = *value;
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_color (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_enum (RigPath *path,
                      float t,
                      int value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      node->boxed.d.enum_val = value;
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_enum (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_boolean (RigPath *path,
                         float t,
                         bool value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      node->boxed.d.boolean_val = value;
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_boolean (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_text (RigPath *path,
                      float t,
                      const char *value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      if (node->boxed.d.text_val)
        g_free (node->boxed.d.text_val);
      node->boxed.d.text_val = g_strdup (value);
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_text (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_asset (RigPath *path,
                      float t,
                      RutAsset *value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      if (node->boxed.d.asset_val)
        rut_refable_unref (node->boxed.d.asset_val);
      node->boxed.d.asset_val = rut_refable_ref (value);
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_asset (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

void
rig_path_insert_object (RigPath *path,
                        float t,
                        RutObject *value)
{
  RigNode *node = rig_path_find_node (path, t);

  if (node)
    {
      if (node->boxed.d.object_val)
        rut_refable_unref (node->boxed.d.object_val);
      node->boxed.d.object_val = rut_refable_ref (value);
      notify_node_modified (path, node);
    }
  else
    {
      node = rig_node_new_for_object (t, value);
      insert_sorted_node (path, node);
      notify_node_added (path, node);
    }
}

CoglBool
rig_path_lerp_property (RigPath *path,
                        RutProperty *property,
                        float t)
{
  RigNode *n0, *n1;

  g_return_val_if_fail (property->spec->type == path->type, FALSE);

  if (!rig_path_find_control_points2 (path, t,
                                      RIG_PATH_DIRECTION_FORWARDS,
                                      &n0, &n1))
    return FALSE;

  switch (path->type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      {
        float value;

        rig_node_float_lerp (n0, n1, t, &value);
        rut_property_set_float (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_DOUBLE:
      {
        double value;

        rig_node_double_lerp (n0, n1, t, &value);
        rut_property_set_double (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_INTEGER:
      {
        int value;

        rig_node_integer_lerp (n0, n1, t, &value);
        rut_property_set_integer (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_UINT32:
      {
        uint32_t value;

        rig_node_uint32_lerp (n0, n1, t, &value);
        rut_property_set_uint32 (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_VEC3:
      {
        float value[3];
        rig_node_vec3_lerp (n0, n1, t, value);
        rut_property_set_vec3 (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_VEC4:
      {
        float value[4];
        rig_node_vec4_lerp (n0, n1, t, value);
        rut_property_set_vec4 (&path->ctx->property_ctx, property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_COLOR:
      {
        CoglColor value;
        rig_node_color_lerp (n0, n1, t, &value);
        rut_property_set_color (&path->ctx->property_ctx, property, &value);
        break;
      }
    case RUT_PROPERTY_TYPE_QUATERNION:
      {
        CoglQuaternion value;
        rig_node_quaternion_lerp (n0, n1, t, &value);
        rut_property_set_quaternion (&path->ctx->property_ctx,
                                     property, &value);
        break;
      }

      /* These types of properties can't be interoplated so they
       * probably shouldn't end up in a path */
    case RUT_PROPERTY_TYPE_ENUM:
      {
        int value;
        rig_node_enum_lerp (n0, n1, t, &value);
        rut_property_set_enum (&path->ctx->property_ctx,
                               property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_BOOLEAN:
      {
        bool value;
        rig_node_boolean_lerp (n0, n1, t, &value);
        rut_property_set_boolean (&path->ctx->property_ctx,
                                  property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_TEXT:
      {
        const char *value;
        rig_node_text_lerp (n0, n1, t, &value);
        rut_property_set_text (&path->ctx->property_ctx,
                               property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_ASSET:
      {
        RutAsset *value;
        rig_node_asset_lerp (n0, n1, t, &value);
        rut_property_set_asset (&path->ctx->property_ctx,
                                property, value);
        break;
      }
    case RUT_PROPERTY_TYPE_OBJECT:
      {
        RutObject *value;
        rig_node_object_lerp (n0, n1, t, &value);
        rut_property_set_object (&path->ctx->property_ctx,
                                 property, value);
        break;
      }
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

    case RUT_PROPERTY_TYPE_ENUM:
      rig_path_insert_enum (path, t, value->d.enum_val);
      return;

    case RUT_PROPERTY_TYPE_BOOLEAN:
      rig_path_insert_boolean (path, t, value->d.boolean_val);
      return;

    case RUT_PROPERTY_TYPE_TEXT:
      rig_path_insert_text (path, t, value->d.text_val);
      return;

    case RUT_PROPERTY_TYPE_ASSET:
      rig_path_insert_asset (path, t, value->d.asset_val);
      return;

    case RUT_PROPERTY_TYPE_OBJECT:
      rig_path_insert_object (path, t, value->d.object_val);
      return;

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
  rig_node_free (node);
  path->length--;

  if (path->pos == node)
    path->pos = NULL;
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

void
rut_path_foreach_node (RigPath *path,
                       RigPathNodeCallback callback,
                       void *user_data)
{
  RigNode *node;

  rut_list_for_each (node, &path->nodes, list_node)
    callback (node, user_data);
}

