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

#include "rut-property.h"
#include "rut-object.h"
#include "rut-type.h"

#ifndef _RUT_INTROSPECTABLE_H_
#define _RUT_INTROSPECTABLE_H_

typedef struct _RutIntrospectableProps
{
  RutProperty *first_property;
  int n_properties;
} RutIntrospectableProps;

typedef void (*RutIntrospectablePropertyCallback) (RutProperty *property,
                                                   void *user_data);

void
rut_introspectable_init (RutObject *object,
                         RutPropertySpec *specs,
                         RutProperty *properties);

void
rut_introspectable_destroy (RutObject *object);

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

#if 0
static inline int
rut_introspectable_get_property_id (RutObject *object,
                                    RutProperty *property)
{
  RutIntrospectableProps *introspectable =
    rut_object_get_properties (object, RUT_TRAIT_ID_INTROSPECTABLE);

  return property - introspectable->first_property;
}
#endif

static inline RutProperty *
rut_introspectable_get_property (RutObject *object,
                                 int id)
{
  RutIntrospectableProps *introspectable =
    rut_object_get_properties (object, RUT_TRAIT_ID_INTROSPECTABLE);

  c_return_val_if_fail (id < introspectable->n_properties, NULL);

  return &introspectable->first_property[id];
}

#endif /* _RUT_INTROSPECTABLE_H_ */
