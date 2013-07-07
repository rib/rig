#ifndef _RUT_INTERFACES_H_
#define _RUT_INTERFACES_H_

#include "rut-types.h"
#include "rut-property.h"
#include "rut-mesh.h"

/* A Collection of really simple, common interfaces that don't seem to
 * warrent being split out into separate files.
 */


/*
 *
 * Refcountable Interface
 *
 */

typedef struct _RutRefCountableVTable
{
  void *(*ref)(void *object);
  void (*unref)(void *object);
  void (*free)(void *object);
} RutRefCountableVTable;

void *
rut_refable_simple_ref (void *object);

void
rut_refable_simple_unref (void *object);

void *
rut_refable_ref (void *object);

void
rut_refable_unref (void *object);

/*
 *
 * Graphable Interface
 *
 */

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
  GQueue children;
} RutGraphableProps;

#if 0
RutCamera *
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

void
rut_graphable_apply_transform (RutObject *graphable,
                               CoglMatrix *transform);

void
rut_graphable_get_transform (RutObject *graphable,
                             CoglMatrix *transform);

void
rut_graphable_get_modelview (RutObject *graphable,
                             RutCamera *camera,
                             CoglMatrix *transform);

void
rut_graphable_fully_transform_point (RutObject *graphable,
                                     RutCamera *camera,
                                     float *x,
                                     float *y,
                                     float *z);

/*
 *
 * Introspectable Interface
 *
 */

typedef void (*RutIntrospectablePropertyCallback) (RutProperty *property,
                                                   void *user_data);

typedef struct _RutIntrospectableVTable
{
  RutProperty *(*lookup_property) (RutObject *object, const char *name);
  void (*foreach_property) (RutObject *object,
                            RutIntrospectablePropertyCallback callback,
                            void *user_data);
} RutIntrospectableVTable;

RutProperty *
rut_introspectable_lookup_property (RutObject *object,
                                    const char *name);

void
rut_introspectable_foreach_property (RutObject *object,
                                     RutIntrospectablePropertyCallback callback,
                                     void *user_data);

void
rut_introspectable_copy_properties (RutPropertyContext *property_ctx,
                                    RutObject *src,
                                    RutObject *dst);

typedef struct _RutSimpleIntrospectableProps
{
  RutProperty *first_property;
  int n_properties;
} RutSimpleIntrospectableProps;

void
rut_simple_introspectable_init (RutObject *object,
                                RutPropertySpec *specs,
                                RutProperty *properties);

void
rut_simple_introspectable_destroy (RutObject *object);

RutProperty *
rut_simple_introspectable_lookup_property (RutObject *object,
                                           const char *name);

void
rut_simple_introspectable_foreach_property (RutObject *object,
                                            RutIntrospectablePropertyCallback callback,
                                            void *user_data);

typedef struct RutTransformableVTable
{
  const CoglMatrix *(*get_matrix) (RutObject *object);
} RutTransformableVTable;

const CoglMatrix *
rut_transformable_get_matrix (RutObject *object);

typedef void
(* RutSizablePreferredSizeCallback) (RutObject *sizable,
                                     void *user_data);

typedef struct _RutSizableVTable
{
  void (* set_size) (void *object,
                     float width,
                     float height);
  void (* get_size) (void *object,
                     float *width,
                     float *height);
  void (* get_preferred_width) (RutObject *object,
                                float for_height,
                                float *min_width_p,
                                float *natural_width_p);
  void (* get_preferred_height) (RutObject *object,
                                 float for_width,
                                 float *min_height_p,
                                 float *natural_height_p);
  /* Registers a callback that gets invoked whenever the preferred
   * size of the sizable object changes. The implementation is
   * optional. If it is not implemented then a dummy closure object
   * will be returned and it is assumed that the object's preferred
   * size never changes */
  RutClosure *
  (* add_preferred_size_callback) (RutObject *object,
                                   RutSizablePreferredSizeCallback callback,
                                   void *user_data,
                                   RutClosureDestroyCallback destroy_cb);
} RutSizableVTable;

void
rut_sizable_set_size (RutObject *object,
                      float width,
                      float height);

void
rut_sizable_get_size (void *object,
                      float *width,
                      float *height);

void
rut_sizable_get_preferred_width (void *object,
                                 float for_height,
                                 float *min_width_p,
                                 float *natural_width_p);
void
rut_sizable_get_preferred_height (void *object,
                                  float for_width,
                                  float *min_height_p,
                                  float *natural_height_p);

void
rut_simple_sizable_get_preferred_width (void *object,
                                        float for_height,
                                        float *min_width_p,
                                        float *natural_width_p);

void
rut_simple_sizable_get_preferred_height (void *object,
                                         float for_width,
                                         float *min_height_p,
                                         float *natural_height_p);

/**
 * rut_sizable_add_preferred_size_callback:
 * @object: An object implementing the sizable interface
 * @cb: The callback to invoke
 * @user_data: A data pointer that will be passed to the callback
 * @destroy_cb: A callback that will be invoked when the callback is removed.
 *
 * Adds a callback to be invoked whenever the preferred size of the
 * given sizable object changes.
 *
 * Return value: A #RutClosure representing the callback. This can be
 *   removed with rut_closure_disconnect().
 */
RutClosure *
rut_sizable_add_preferred_size_callback (RutObject *object,
                                         RutSizablePreferredSizeCallback cb,
                                         void *user_data,
                                         RutClosureDestroyCallback destroy_cb);

/*
 *
 * Primable Interface
 * (E.g. implemented by all geometry components)
 *
 */
typedef struct _RutPrimableVTable
{
  CoglPrimitive *(*get_primitive)(void *object);
} RutPrimableVTable;

CoglPrimitive *
rut_primable_get_primitive (RutObject *object);


/*
 *
 * Pickable Interface
 * (E.g. implemented by all geometry components)
 *
 */
typedef struct _RutPickableVTable
{
  RutMesh *(*get_mesh)(void *object);
} RutPickableVTable;

void *
rut_pickable_get_mesh (RutObject *object);

/*
 *
 * Inputable Interface
 *
 * The inputable represents something that wants to receive input
 * events. This is acheived a property which points to the input
 * region for this object. There aren't actually any methods on this
 * interface, just the property.
 */

typedef struct _RutInputableProps
{
  RutInputRegion *input_region;
} RutInputableProps;

RutInputRegion *
rut_inputable_get_input_region (RutObject *object);

/*
 * Image Size Dependant Interface
 *
 * This implies the object is related in some way to an image whose
 * size affects the internal state of the object.
 *
 * For example a NineSlice's geometry depends on the size of the
 * texture being drawn, and the geometry of a pointalism component
 * depends on the size of the image.
 */
typedef struct _RutImageSizeDependantVTable
{
  void (*set_image_size)(RutObject *object,
                         int width,
                         int height);
} RutImageSizeDependantVTable;

#endif /* _RUT_INTERFACES_H_ */
