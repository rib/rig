/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef __RUT_PICKABLE_H__
#define __RUT_PICKABLE_H__

#include <cglib/cglib.h>

#include "rut-object.h"

typedef bool (*rut_pickable_pick_t)(rut_object_t *pickable,
                                    rut_object_t *camera,
                                    const c_matrix_t *graphable_modelview,
                                    float x,
                                    float y);

typedef struct _rut_pickable_vtable_t {
    rut_pickable_pick_t pick;
} rut_pickable_vtable_t;

static inline bool
rut_pickable_pick(rut_object_t *pickable,
                  rut_object_t *camera,
                  const c_matrix_t *graphable_modelview,
                  float x,
                  float y)
{
    rut_pickable_vtable_t *vtable = (rut_pickable_vtable_t *)
        rut_object_get_vtable(pickable, RUT_TRAIT_ID_PICKABLE);

    return vtable->pick(pickable, camera, graphable_modelview, x, y);
}

#endif /* __RUT_PICKABLE_H__ */
