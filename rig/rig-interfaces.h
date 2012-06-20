#ifndef _RIG_INTERFACES_H_
#define _RIG_INTERFACES_H_

#include "rig-types.h"
#include "rig-property.h"

/* A Collection of really simple, common interfaces that don't seem to
 * warrent being split out into separate files.
 */


/*
 *
 * Refcountable Interface
 *
 */

typedef struct _RigRefCountableVTable
{
  void *(*ref)(void *object);
  void (*unref)(void *object);
  void (*free)(void *object);
} RigRefCountableVTable;

void *
rig_ref_countable_simple_ref (void *object);

void
rig_ref_countable_simple_unref (void *object);

void *
rig_ref_countable_ref (void *object);

void
rig_ref_countable_unref (void *object);


/*
 *
 * Graphable Interface
 *
 */

typedef struct _RigGraphableVTable
{
  void (*child_removed) (RigObject *parent, RigObject *child);
  void (*child_added) (RigObject *parent, RigObject *child);

  void (*parent_changed) (RigObject *child,
                          RigObject *old_parent,
                          RigObject *new_parent);
} RigGraphableVTable;

typedef struct _RigGraphableProps
{
  RigObject *parent;
  GQueue children;
} RigGraphableProps;

RigCamera *
rig_graphable_find_camera (RigObject *object);

/* RigTraverseFlags:
 * RIG_TRAVERSE_DEPTH_FIRST: Traverse the graph in
 *   a depth first order.
 * RIG_TRAVERSE_BREADTH_FIRST: Traverse the graph in a
 *   breadth first order.
 *
 * Controls some options for how rig_graphable_traverse() iterates
 * through a graph.
 */
typedef enum {
  RIG_TRAVERSE_DEPTH_FIRST   = 1L<<0,
  RIG_TRAVERSE_BREADTH_FIRST = 1L<<1
} RigTraverseFlags;

/* RigTraverseVisitFlags:
 * RIG_TRAVERSE_VISIT_CONTINUE: Continue traversing as
 *   normal
 * RIG_TRAVERSE_VISIT_SKIP_CHILDREN: Don't traverse the
 *   children of the last visited object. (Not applicable when using
 *   %RIG_TRAVERSE_DEPTH_FIRST_POST_ORDER since the children
 *   are visited before having an opportunity to bail out)
 * RIG_TRAVERSE_VISIT_BREAK: Immediately bail out without
 *   visiting any more objects.
 *
 * Each time an object is visited during a graph traversal the
 * RigTraverseCallback can return a set of flags that may affect the
 * continuing traversal. It may stop traversal completely, just skip
 * over children for the current object or continue as normal.
 */
typedef enum {
  RIG_TRAVERSE_VISIT_CONTINUE       = 1L<<0,
  RIG_TRAVERSE_VISIT_SKIP_CHILDREN  = 1L<<1,
  RIG_TRAVERSE_VISIT_BREAK          = 1L<<2
} RigTraverseVisitFlags;

/* The callback prototype used with rig_graphable_traverse. The
 * returned flags can be used to affect the continuing traversal
 * either by continuing as normal, skipping over children of an
 * object or bailing out completely.
 */
typedef RigTraverseVisitFlags (*RigTraverseCallback) (RigObject *object,
                                                      int depth,
                                                      void *user_data);

void
rig_graphable_traverse (RigObject *root,
                        RigTraverseFlags flags,
                        RigTraverseCallback before_children_callback,
                        RigTraverseCallback after_children_callback,
                        void *user_data);

void
rig_graphable_init (RigObject *object);

void
rig_graphable_add_child (RigObject *parent, RigObject *child);

void
rig_graphable_remove_child (RigObject *parent, RigObject *child);

void
rig_graphable_remove_all_children (RigObject *parent);

void
rig_graphable_get_transform (RigObject *graphable,
                             CoglMatrix *transform);


/*
 *
 * Introspectable Interface
 *
 */

typedef void (*RigIntrospectablePropertyCallback) (RigProperty *property,
                                                   void *user_data);

typedef struct _RigIntrospectableVTable
{
  RigProperty *(*lookup_property) (RigObject *object, const char *name);
  void (*foreach_property) (RigObject *object,
                            RigIntrospectablePropertyCallback callback,
                            void *user_data);
} RigIntrospectableVTable;

RigProperty *
rig_introspectable_lookup_property (RigObject *object,
                                    const char *name);

void
rig_introspectable_foreach_property (RigObject *object,
                                     RigIntrospectablePropertyCallback callback,
                                     void *user_data);

typedef struct _RigSimpleIntrospectableProps
{
  RigProperty *first_property;
  int n_properties;
} RigSimpleIntrospectableProps;

void
rig_simple_introspectable_init (RigObject *object,
                                RigPropertySpec *specs,
                                RigProperty *properties);

void
rig_simple_introspectable_destroy (RigObject *object);

RigProperty *
rig_simple_introspectable_lookup_property (RigObject *object,
                                           const char *name);

void
rig_simple_introspectable_foreach_property (RigObject *object,
                                            RigIntrospectablePropertyCallback callback,
                                            void *user_data);

typedef struct RigTransformableVTable
{
  const CoglMatrix *(*get_matrix) (RigObject *object);
} RigTransformableVTable;

const CoglMatrix *
rig_transformable_get_matrix (RigObject *object);

#endif /* _RIG_INTERFACES_H_ */
