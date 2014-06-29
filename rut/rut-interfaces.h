/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

typedef struct RutTransformableVTable
{
  const cg_matrix_t *(*get_matrix) (RutObject *object);
} RutTransformableVTable;

const cg_matrix_t *
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
  cg_primitive_t *(*get_primitive)(void *object);
} RutPrimableVTable;

cg_primitive_t *
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
