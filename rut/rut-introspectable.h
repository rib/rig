/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013,2014  Intel Corporation
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

#include "rig-property.h"
#include "rut-object.h"
#include "rut-type.h"

#ifndef _RUT_INTROSPECTABLE_H_
#define _RUT_INTROSPECTABLE_H_

C_BEGIN_DECLS

typedef struct _rut_introspectable_props_t {
    rig_property_t *first_property;
    int n_properties;
} rut_introspectable_props_t;

typedef void (*rut_introspectable_property_callback_t)(rig_property_t *property,
                                                       void *user_data);

void rut_introspectable_init(rut_object_t *object,
                             rig_property_spec_t *specs,
                             rig_property_t *properties);

void rut_introspectable_destroy(rut_object_t *object);

rig_property_t *rut_introspectable_lookup_property(rut_object_t *object,
                                                   const char *name);

void rut_introspectable_foreach_property(
    rut_object_t *object,
    rut_introspectable_property_callback_t callback,
    void *user_data);

void rut_introspectable_copy_properties(rig_property_context_t *property_ctx,
                                        rut_object_t *src,
                                        rut_object_t *dst);

#if 0
static inline int
rut_introspectable_get_property_id (rut_object_t *object,
                                    rig_property_t *property)
{
    rut_introspectable_props_t *introspectable =
        rut_object_get_properties (object, RUT_TRAIT_ID_INTROSPECTABLE);

    return property - introspectable->first_property;
}
#endif

static inline rig_property_t *
rut_introspectable_get_property(rut_object_t *object, int id)
{
    rut_introspectable_props_t *introspectable = (rut_introspectable_props_t *)
        rut_object_get_properties(object, RUT_TRAIT_ID_INTROSPECTABLE);

    c_return_val_if_fail(id < introspectable->n_properties, NULL);

    return &introspectable->first_property[id];
}

C_END_DECLS

#endif /* _RUT_INTROSPECTABLE_H_ */
