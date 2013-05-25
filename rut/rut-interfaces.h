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

#ifndef _RUT_INTERFACES_H_
#define _RUT_INTERFACES_H_

#include "rut-types.h"
#include "rut-property.h"
#include "rut-mesh.h"
#include "rut-graphable.h"

G_BEGIN_DECLS

/* A Collection of really simple, common interfaces that don't seem to
 * warrent being split out into separate files.
 */


/*
 *
 * Refcountable Interface
 *
 */

typedef struct _RutRefableVTable
{
  void *(*ref)(void *object);
  void (*unref)(void *object);
  void (*free)(void *object);
} RutRefableVTable;

void *
rut_refable_simple_ref (void *object);

void
rut_refable_simple_unref (void *object);

/* Most new types can use this convenience macro for adding
 * the _REFABLE interface when we don't need to hook into
 * the _ref and _unref */
#define rut_type_add_refable(TYPE_PTR, MEMBER, FREE_FUNC) \
  do { \
      static RutRefableVTable refable_vtable = { \
          rut_refable_simple_ref, \
          rut_refable_simple_unref, \
          FREE_FUNC \
      }; \
      rut_type_add_interface (TYPE_PTR, \
                              RUT_INTERFACE_ID_REF_COUNTABLE, \
                              offsetof (TYPE, MEMBER), \
                              &refable_vtable); \
  } while (0)

void *
rut_refable_ref (void *object);

void *
rut_refable_claim (void *object, void *owner);

void
rut_refable_unref (void *object);

void
rut_refable_release (void *object, void *owner);

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


/*
 * Selectable Interface
 *
 * Anything that can be selected by the user and optionally cut and
 * copied to a clipboard should be tracked using a selectable object.
 *
 * Whenever a new user selection is made then an object implementing
 * the selectable interface should be created to track the selected
 * objects and that object should be registered with a RutShell.
 *
 * Whenever a selection is registered then the ::cancel method of any
 * previous selection will be called.
 *
 * - If Ctrl-C is pressed the ::copy method will be called which should
 *   return a RutMimable object that will be set on the clipboard.
 * - If Ctrl-X is pressed the ::copy method will be called, followed
 *   by the ::del method. The copy method should return a RutMimable
 *   object that will be set on the clipboard.
 * - If Delete is pressed the ::del method will be called
 */
typedef struct _RutSelectableVTable
{
  void (*cancel) (RutObject *selectable);
  RutObject *(*copy) (RutObject *selectable);
  void (*del) (RutObject *selectable);
} RutSelectableVTable;

void
rut_selectable_cancel (RutObject *object);

G_END_DECLS

#endif /* _RUT_INTERFACES_H_ */
