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

#if 0
RigCamera *
rig_graphable_find_camera (RigObject *object);
#endif

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
rig_graphable_remove_child (RigObject *child);

void
rig_graphable_remove_all_children (RigObject *parent);

RigObject *
rig_graphable_get_parent (RigObject *child);

void
rig_graphable_apply_transform (RigObject *graphable,
                               CoglMatrix *transform);

void
rig_graphable_get_transform (RigObject *graphable,
                             CoglMatrix *transform);

void
rig_graphable_get_modelview (RigObject *graphable,
                             RigCamera *camera,
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

typedef struct _RigSizableVTable
{
  void (* set_size) (void *object,
                     float width,
                     float height);
  void (* get_size) (void *object,
                     float *width,
                     float *height);
  void (* get_preferred_width) (void *object,
                                float for_height,
                                float *min_width_p,
                                float *natural_width_p);
  void (* get_preferred_height) (void *object,
                                 float for_width,
                                 float *min_height_p,
                                 float *natural_height_p);
} RigSizableVTable;

void
rig_sizable_set_size (RigObject *object,
                      float width,
                      float height);

void
rig_sizable_get_size (void *object,
                      float *width,
                      float *height);

void
rig_sizable_get_preferred_width (void *object,
                                 float for_height,
                                 float *min_width_p,
                                 float *natural_width_p);
void
rig_sizable_get_preferred_height (void *object,
                                  float for_width,
                                  float *min_height_p,
                                  float *natural_height_p);

/*
 *
 * Primable Interface
 * (E.g. implemented by all geometry components)
 *
 */
typedef struct _RigPrimableVTable
{
  CoglPrimitive *(*get_primitive)(void *object);
} RigPrimableVTable;

CoglPrimitive *
rig_primable_get_primitive (RigObject *object);


/*
 *
 * Pickable Interface
 * (E.g. implemented by all geometry components)
 *
 */
typedef struct _RigPickableVTable
{
  void *(*get_vertex_data)(void *object,
                           size_t *stride,
                           int *n_vertices);
} RigPickableVTable;

void *
rig_pickable_get_vertex_data (RigObject *object,
                              size_t *stride,
                              int *n_vertices);

#endif /* _RIG_INTERFACES_H_ */
