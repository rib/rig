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

#ifndef _RUT_GRAPHABLE_H_
#define _RUT_GRAPHABLE_H_

#include "rut-type.h"
#include "rut-object.h"
#include "rut-types.h"
#include "rut-queue.h"

typedef struct _RutGraphableVTable
{
  void (*child_removed) (RutObject *parent, RutObject *child);
  void (*child_added) (RutObject *parent, RutObject *child);

  void (*parent_changed) (RutObject *child,
                          RutObject *old_parent,
                          RutObject *new_parent);
} RutGraphableVTable;

typedef struct _RutGraphableProps
{
  RutObject *parent;
  RutQueue children;
} RutGraphableProps;

#if 0
RutObject *
rut_graphable_find_camera (RutObject *object);
#endif

/* RutTraverseFlags:
 * RUT_TRAVERSE_DEPTH_FIRST: Traverse the graph in
 *   a depth first order.
 * RUT_TRAVERSE_BREADTH_FIRST: Traverse the graph in a
 *   breadth first order.
 *
 * Controls some options for how rut_graphable_traverse() iterates
 * through a graph.
 */
typedef enum {
  RUT_TRAVERSE_DEPTH_FIRST   = 1L<<0,
  RUT_TRAVERSE_BREADTH_FIRST = 1L<<1
} RutTraverseFlags;

/* RutTraverseVisitFlags:
 * RUT_TRAVERSE_VISIT_CONTINUE: Continue traversing as
 *   normal
 * RUT_TRAVERSE_VISIT_SKIP_CHILDREN: Don't traverse the
 *   children of the last visited object. (Not applicable when using
 *   %RUT_TRAVERSE_DEPTH_FIRST_POST_ORDER since the children
 *   are visited before having an opportunity to bail out)
 * RUT_TRAVERSE_VISIT_BREAK: Immediately bail out without
 *   visiting any more objects.
 *
 * Each time an object is visited during a graph traversal the
 * RutTraverseCallback can return a set of flags that may affect the
 * continuing traversal. It may stop traversal completely, just skip
 * over children for the current object or continue as normal.
 */
typedef enum {
  RUT_TRAVERSE_VISIT_CONTINUE       = 1L<<0,
  RUT_TRAVERSE_VISIT_SKIP_CHILDREN  = 1L<<1,
  RUT_TRAVERSE_VISIT_BREAK          = 1L<<2
} RutTraverseVisitFlags;

/* The callback prototype used with rut_graphable_traverse. The
 * returned flags can be used to affect the continuing traversal
 * either by continuing as normal, skipping over children of an
 * object or bailing out completely.
 */
typedef RutTraverseVisitFlags (*RutTraverseCallback) (RutObject *object,
                                                      int depth,
                                                      void *user_data);

RutTraverseVisitFlags
rut_graphable_traverse (RutObject *root,
                        RutTraverseFlags flags,
                        RutTraverseCallback before_children_callback,
                        RutTraverseCallback after_children_callback,
                        void *user_data);

void
rut_graphable_init (RutObject *object);

void
rut_graphable_destroy (RutObject *object);

void
rut_graphable_add_child (RutObject *parent, RutObject *child);

void
rut_graphable_remove_child (RutObject *child);

void
rut_graphable_remove_all_children (RutObject *parent);

RutObject *
rut_graphable_get_parent (RutObject *child);

RutObject *
rut_graphable_first (RutObject *parent);

RutObject *
rut_graphable_nth (RutObject *parent, int n);

RutObject *
rut_graphable_get_root (RutObject *child);

void
rut_graphable_apply_transform (RutObject *graphable,
                               CoglMatrix *transform);

void
rut_graphable_get_transform (RutObject *graphable,
                             CoglMatrix *transform);

void
rut_graphable_get_modelview (RutObject *graphable,
                             RutObject *camera,
                             CoglMatrix *transform);

void
rut_graphable_fully_transform_point (RutObject *graphable,
                                     RutObject *camera,
                                     float *x,
                                     float *y,
                                     float *z);

#endif /* _RUT_GRAPHABLE_H_ */
