/*
 * Rut
 *
 * Copyright (C) 2012,2013,2014  Intel Corporation
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

static inline RutProperty *
rut_introspectable_get_property (RutObject *object,
                                 int id)
{
  RutIntrospectableProps *introspectable =
    rut_object_get_properties (object, RUT_TRAIT_ID_INTROSPECTABLE);

  g_return_val_if_fail (id < introspectable->n_properties, NULL);

  return &introspectable->first_property[id];
}

#endif /* _RUT_INTROSPECTABLE_H_ */
