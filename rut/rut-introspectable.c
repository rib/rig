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

#include <config.h>

#include "rut-introspectable.h"

typedef struct _CopyPropertiesState
{
  RutPropertyContext *property_ctx;
  RutObject *dst;
} CopyPropertiesState;

static void
copy_property_cb (RutProperty *property,
                  void *user_data)
{
  CopyPropertiesState *state = user_data;
  RutObject *dst = state->dst;
  RutProperty *dst_property =
    rut_introspectable_lookup_property (dst, property->spec->name);

  g_return_if_fail (dst_property != NULL);

  rut_property_copy_value (state->property_ctx, dst_property, property);
}

void
rut_introspectable_copy_properties (RutPropertyContext *property_ctx,
                                    RutObject *src,
                                    RutObject *dst)
{
  CopyPropertiesState state = { property_ctx, dst };
  rut_introspectable_foreach_property (src,
                                       copy_property_cb,
                                       &state);
}

void
rut_introspectable_init (RutObject *object,
                         RutPropertySpec *specs,
                         RutProperty *properties)
{
  RutIntrospectableProps *props =
    rut_object_get_properties (object, RUT_TRAIT_ID_INTROSPECTABLE);
  int n;

  for (n = 0; specs[n].name; n++)
    {
      rut_property_init (&properties[n],
                         &specs[n],
                         object,
                         n);
    }

  props->first_property = properties;
  props->n_properties = n;
}

void
rut_introspectable_destroy (RutObject *object)
{
  RutIntrospectableProps *props =
    rut_object_get_properties (object, RUT_TRAIT_ID_INTROSPECTABLE);
  RutProperty *properties = props->first_property;
  int i;

  for (i = 0; i < props->n_properties; i++)
    rut_property_destroy (&properties[i]);
}

RutProperty *
rut_introspectable_lookup_property (RutObject *object,
                                    const char *name)
{
  RutIntrospectableProps *priv =
    rut_object_get_properties (object, RUT_TRAIT_ID_INTROSPECTABLE);
  int i;

  for (i = 0; i < priv->n_properties; i++)
    {
      RutProperty *property = priv->first_property + i;
      if (strcmp (property->spec->name, name) == 0)
        return property;
    }

  return NULL;
}

void
rut_introspectable_foreach_property (RutObject *object,
                                     RutIntrospectablePropertyCallback callback,
                                     void *user_data)
{
  RutIntrospectableProps *priv =
    rut_object_get_properties (object, RUT_TRAIT_ID_INTROSPECTABLE);
  int i;

  for (i = 0; i < priv->n_properties; i++)
    {
      RutProperty *property = priv->first_property + i;
      callback (property, user_data);
    }
}
