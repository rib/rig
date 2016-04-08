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
#include "rig-property.h"
#include "rut-mesh.h"
#include "rut-graphable.h"

C_BEGIN_DECLS

/* A Collection of really simple, common interfaces that don't seem to
 * warrent being split out into separate files.
 */

typedef struct rut_transformable_vtable_t {
    const c_matrix_t *(*get_matrix)(rut_object_t *object);
} rut_transformable_vtable_t;

const c_matrix_t *rut_transformable_get_matrix(rut_object_t *object);

typedef void (*rut_sizeable_preferred_size_callback_t)(rut_object_t *sizable,
                                                    void *user_data);

typedef struct _rut_sizable_vtable_t {
    void (*set_size)(void *object, float width, float height);
    void (*get_size)(void *object, float *width, float *height);
    void (*get_preferred_width)(rut_object_t *object,
                                float for_height,
                                float *min_width_p,
                                float *natural_width_p);
    void (*get_preferred_height)(rut_object_t *object,
                                 float for_width,
                                 float *min_height_p,
                                 float *natural_height_p);
    /* Registers a callback that gets invoked whenever the preferred
     * size of the sizable object changes. The implementation is
     * optional. */
    rut_closure_t *(*add_preferred_size_callback)(rut_object_t *object,
                                                  rut_closure_t *closure);
} rut_sizable_vtable_t;

void rut_sizable_set_size(rut_object_t *object, float width, float height);

void rut_sizable_get_size(void *object, float *width, float *height);

void rut_sizable_get_preferred_width(void *object,
                                     float for_height,
                                     float *min_width_p,
                                     float *natural_width_p);
void rut_sizable_get_preferred_height(void *object,
                                      float for_width,
                                      float *min_height_p,
                                      float *natural_height_p);

void rut_simple_sizable_get_preferred_width(void *object,
                                            float for_height,
                                            float *min_width_p,
                                            float *natural_width_p);

void rut_simple_sizable_get_preferred_height(void *object,
                                             float for_width,
                                             float *min_height_p,
                                             float *natural_height_p);

/**
 * rut_sizable_add_preferred_size_callback:
 * @object: An object implementing the sizable interface
 * @closure: A closure with associated callback
 *
 * Adds a callback to be invoked whenever the preferred size of the
 * given sizable object changes.
 */
void rut_sizable_add_preferred_size_callback(rut_object_t *object,
                                             rut_closure_t *closure);

/*
 *
 * Primable Interface
 * (E.g. implemented by all geometry components)
 *
 */
typedef struct _rut_primable_vtable_t {
    cg_primitive_t *(*get_primitive)(void *object);
} rut_primable_vtable_t;

cg_primitive_t *rut_primable_get_primitive(rut_object_t *object);

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
typedef struct _rut_image_size_dependant_vtable_t {
    void (*set_image_size)(rut_object_t *object, int width, int height);
} rut_image_size_dependant_vtable_t;

/*
 * Selectable Interface
 *
 * Anything that can be selected by the user and optionally cut and
 * copied to a clipboard should be tracked using a selectable object.
 *
 * Whenever a new user selection is made then an object implementing
 * the selectable interface should be created to track the selected
 * objects and that object should be registered with a rut_shell_t.
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
typedef struct _rut_selectable_vtable_t {
    void (*cancel)(rut_object_t *selectable);
    rut_object_t *(*copy)(rut_object_t *selectable);
    void (*del)(rut_object_t *selectable);
} rut_selectable_vtable_t;

void rut_selectable_cancel(rut_object_t *object);

C_END_DECLS

#endif /* _RUT_INTERFACES_H_ */
