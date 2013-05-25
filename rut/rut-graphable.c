/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <cogl/cogl.h>

#include "rut-graphable.h"
#include "rut-interfaces.h"
#include "rut-util.h"
#include "components/rut-camera.h"
#include "rut-queue.h"

void
rut_graphable_init (RutObject *object)
{
  RutGraphableProps *props =
    rut_object_get_properties (object, RUT_TRAIT_ID_GRAPHABLE);

  props->parent = NULL;
  rut_queue_init (&props->children);
}

void
rut_graphable_destroy (RutObject *object)
{
  RutGraphableProps *props =
    rut_object_get_properties (object, RUT_TRAIT_ID_GRAPHABLE);

  /* The node shouldn't have a parent, because if it did then it would
   * still have a reference and it shouldn't be being destroyed */
  g_warn_if_fail (props->parent == NULL);

  rut_graphable_remove_all_children (object);
}

void
rut_graphable_add_child (RutObject *parent, RutObject *child)
{
  RutGraphableProps *parent_props =
    rut_object_get_properties (parent, RUT_TRAIT_ID_GRAPHABLE);
  RutGraphableVTable *parent_vtable =
    rut_object_get_vtable (parent, RUT_TRAIT_ID_GRAPHABLE);
  RutGraphableProps *child_props =
    rut_object_get_properties (child, RUT_TRAIT_ID_GRAPHABLE);
  RutGraphableVTable *child_vtable =
    rut_object_get_vtable (child, RUT_TRAIT_ID_GRAPHABLE);
  RutObject *old_parent = child_props->parent;

  rut_object_claim (child, parent);

  if (old_parent)
    rut_graphable_remove_child (child);

  child_props->parent = parent;
  if (child_vtable && child_vtable->parent_changed)
    child_vtable->parent_changed (child, old_parent, parent);

  if (parent_vtable && parent_vtable->child_added)
    parent_vtable->child_added (parent, child);

  /* XXX: maybe this should be deferred to parent_vtable->child_added ? */
  rut_queue_push_tail (&parent_props->children, child);
}

void
rut_graphable_remove_child (RutObject *child)
{
  RutGraphableProps *child_props =
    rut_object_get_properties (child, RUT_TRAIT_ID_GRAPHABLE);
  RutObject *parent = child_props->parent;
  RutGraphableVTable *parent_vtable;
  RutGraphableProps *parent_props;

  if (!parent)
    return;

  parent_vtable = rut_object_get_vtable (parent, RUT_TRAIT_ID_GRAPHABLE);
  parent_props = rut_object_get_properties (parent, RUT_TRAIT_ID_GRAPHABLE);

  /* Note: we set ->parent to NULL here to avoid re-entrancy so
   * ->child_removed can be a general function for removing a child
   *  that might itself call rut_graphable_remove_child() */
  child_props->parent = NULL;

  if (parent_vtable->child_removed)
    parent_vtable->child_removed (parent, child);

  rut_queue_remove (&parent_props->children, child);
  rut_object_release (child, parent);
}

void
rut_graphable_remove_all_children (RutObject *parent)
{
  RutGraphableProps *parent_props =
    rut_object_get_properties (parent, RUT_TRAIT_ID_GRAPHABLE);
  RutObject *child;

  while ((child = rut_queue_pop_tail (&parent_props->children)))
    rut_graphable_remove_child (child);
}

static RutObject *
_rut_graphable_get_parent (RutObject *child)
{
  RutGraphableProps *child_props =
    rut_object_get_properties (child, RUT_TRAIT_ID_GRAPHABLE);

  return child_props->parent;
}

RutObject *
rut_graphable_get_parent (RutObject *child)
{
  return _rut_graphable_get_parent (child);
}

RutObject *
rut_graphable_first (RutObject *parent)
{
  RutGraphableProps *graphable =
    rut_object_get_properties (parent, RUT_TRAIT_ID_GRAPHABLE);

  return rut_queue_peek_head (&graphable->children);
}

RutObject *
rut_graphable_nth (RutObject *parent, int n)
{
  RutGraphableProps *graphable =
    rut_object_get_properties (parent, RUT_TRAIT_ID_GRAPHABLE);

  return rut_queue_peek_nth (&graphable->children, n);
}

RutObject *
rut_graphable_get_root (RutObject *child)
{
  RutObject *root, *parent;

  root = child;
  while ((parent = _rut_graphable_get_parent (root)))
    root = parent;

  return root;
}

static RutTraverseVisitFlags
_rut_graphable_traverse_breadth (RutObject *graphable,
                                 RutTraverseCallback callback,
                                 void *user_data)
{
  RutQueue queue;
  int dummy;
  int current_depth = 0;
  RutTraverseVisitFlags flags = 0;

  rut_queue_init (&queue);

  rut_queue_push_tail (&queue, graphable);
  rut_queue_push_tail (&queue, &dummy); /* use to delimit depth changes */

  while ((graphable = rut_queue_pop_head (&queue)))
    {
      if (graphable == &dummy)
        {
          current_depth++;
          rut_queue_push_tail (&queue, &dummy);
          continue;
        }

      flags = callback (graphable, current_depth, user_data);
      if (flags & RUT_TRAVERSE_VISIT_BREAK)
        break;
      else if (!(flags & RUT_TRAVERSE_VISIT_SKIP_CHILDREN))
        {
          RutGraphableProps *props =
            rut_object_get_properties (graphable, RUT_TRAIT_ID_GRAPHABLE);
          RutQueueItem *item;

          rut_list_for_each (item, &props->children.items, list_node)
            {
              rut_queue_push_tail (&queue, item->data);
            }
        }
    }

  return flags;
}

static RutTraverseVisitFlags
_rut_graphable_traverse_depth (RutObject *graphable,
                               RutTraverseCallback before_children_callback,
                               RutTraverseCallback after_children_callback,
                               int current_depth,
                               void *user_data)
{
  RutTraverseVisitFlags flags;

  if (before_children_callback)
    {
      flags = before_children_callback (graphable, current_depth, user_data);
      if (flags & RUT_TRAVERSE_VISIT_BREAK)
        return RUT_TRAVERSE_VISIT_BREAK;
    }

  if (!(flags & RUT_TRAVERSE_VISIT_SKIP_CHILDREN))
    {
      RutGraphableProps *props =
        rut_object_get_properties (graphable, RUT_TRAIT_ID_GRAPHABLE);
      RutQueueItem *item, *tmp;

      rut_list_for_each_safe (item, tmp, &props->children.items, list_node)
        {
          flags = _rut_graphable_traverse_depth (item->data,
                                                 before_children_callback,
                                                 after_children_callback,
                                                 current_depth + 1,
                                                 user_data);

          if (flags & RUT_TRAVERSE_VISIT_BREAK)
            return RUT_TRAVERSE_VISIT_BREAK;
        }
    }

  if (after_children_callback)
    return after_children_callback (graphable, current_depth, user_data);
  else
    return RUT_TRAVERSE_VISIT_CONTINUE;
}

/* rut_graphable_traverse:
 * @graphable: The graphable object to start traversing the graph from
 * @flags: These flags may affect how the traversal is done
 * @before_children_callback: A function to call before visiting the
 *   children of the current object.
 * @after_children_callback: A function to call after visiting the
 *   children of the current object. (Ignored if
 *   %RUT_TRAVERSE_BREADTH_FIRST is passed to @flags.)
 * @user_data: The private data to pass to the callbacks
 *
 * Traverses the graph starting at the specified @root and descending
 * through all its children and its children's children.  For each
 * object traversed @before_children_callback and
 * @after_children_callback are called with the specified @user_data,
 * before and after visiting that object's children.
 *
 * The callbacks can return flags that affect the ongoing traversal
 * such as by skipping over an object's children or bailing out of
 * any further traversing.
 */
RutTraverseVisitFlags
rut_graphable_traverse (RutObject *root,
                        RutTraverseFlags flags,
                        RutTraverseCallback before_children_callback,
                        RutTraverseCallback after_children_callback,
                        void *user_data)
{
  if (flags & RUT_TRAVERSE_BREADTH_FIRST)
    return _rut_graphable_traverse_breadth (root,
                                            before_children_callback,
                                            user_data);
  else /* DEPTH_FIRST */
    return _rut_graphable_traverse_depth (root,
                                          before_children_callback,
                                          after_children_callback,
                                          0, /* start depth */
                                          user_data);
}

#if 0
static RutTraverseVisitFlags
_rut_graphable_paint_cb (RutObject *object,
                         int depth,
                         void *user_data)
{
  RutPaintContext *paint_ctx = user_data;
  RutPaintableVTable *vtable =
    rut_object_get_vtable (object, RUT_TRAIT_ID_PAINTABLE);

  vtable->paint (object, paint_ctx);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
rut_graphable_paint (RutObject *root,
                     RutCamera *camera)
{
  RutPaintContext paint_ctx;

  paint_ctx.camera = camera;

  rut_graphable_traverse (root,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           _rut_graphable_paint_cb,
                           NULL, /* after callback */
                           &paint_ctx);
}
#endif

#if 0
RutCamera *
rut_graphable_find_camera (RutObject *object)
{
  do {
    RutGraphableProps *graphable_priv;

    if (rut_object_get_type (object) == &rut_camera_type)
      return RUT_CAMERA (object);

    graphable_priv =
      rut_object_get_properties (object, RUT_TRAIT_ID_GRAPHABLE);

    object = graphable_priv->parent;

  } while (object);

  return NULL;
}
#endif

void
rut_graphable_apply_transform (RutObject *graphable,
                               CoglMatrix *transform_matrix)
{
  int depth = 0;
  RutObject **transform_nodes;
  RutObject *node = graphable;
  int i;

  do {
    RutGraphableProps *graphable_priv =
      rut_object_get_properties (node, RUT_TRAIT_ID_GRAPHABLE);

    depth++;

    node = graphable_priv->parent;
  } while (node);

  transform_nodes = g_alloca (sizeof (RutObject *) * depth);

  node = graphable;
  i = 0;
  do {
    RutGraphableProps *graphable_priv;

    if (rut_object_is (node, RUT_TRAIT_ID_TRANSFORMABLE))
      transform_nodes[i++] = node;

    graphable_priv =
      rut_object_get_properties (node, RUT_TRAIT_ID_GRAPHABLE);
    node = graphable_priv->parent;
  } while (node);

  for (i--; i >= 0; i--)
    {
      const CoglMatrix *matrix = rut_transformable_get_matrix (transform_nodes[i]);
      cogl_matrix_multiply (transform_matrix, transform_matrix, matrix);
    }
}

void
rut_graphable_get_transform (RutObject *graphable,
                             CoglMatrix *transform)
{
  cogl_matrix_init_identity (transform);
  rut_graphable_apply_transform (graphable, transform);
}

void
rut_graphable_get_modelview (RutObject *graphable,
                             RutCamera *camera,
                             CoglMatrix *transform)
{
  const CoglMatrix *view = rut_camera_get_view_transform (camera);
  *transform = *view;
  rut_graphable_apply_transform (graphable, transform);
}

void
rut_graphable_fully_transform_point (RutObject *graphable,
                                     RutCamera *camera,
                                     float *x,
                                     float *y,
                                     float *z)
{
  CoglMatrix modelview;
  const CoglMatrix *projection;
  const float *viewport;
  float point[3] = { *x, *y, *z };

  rut_graphable_get_modelview (graphable, camera, &modelview);
  projection = rut_camera_get_projection (camera);
  viewport = rut_camera_get_viewport (camera);

  rut_util_fully_transform_vertices (&modelview,
                                     projection,
                                     viewport,
                                     point,
                                     point,
                                     1);

  *x = point[0];
  *y = point[1];
  *z = point[2];
}


