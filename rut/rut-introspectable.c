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

#include <rut-config.h>

#include "rut-introspectable.h"

typedef struct _copy_properties_state_t {
    rig_property_context_t *property_ctx;
    rut_object_t *dst;
} copy_properties_state_t;

static void
copy_property_cb(rig_property_t *property, void *user_data)
{
    copy_properties_state_t *state = user_data;
    rut_object_t *dst = state->dst;
    rig_property_t *dst_property =
        rut_introspectable_lookup_property(dst, property->spec->name);

    c_return_if_fail(dst_property != NULL);

    rig_property_copy_value(state->property_ctx, dst_property, property);
}

void
rut_introspectable_copy_properties(rig_property_context_t *property_ctx,
                                   rut_object_t *src,
                                   rut_object_t *dst)
{
    copy_properties_state_t state = { property_ctx, dst };
    rut_introspectable_foreach_property(src, copy_property_cb, &state);
}

void
rut_introspectable_init(rut_object_t *object,
                        rig_property_spec_t *specs,
                        rig_property_t *properties)
{
    rut_introspectable_props_t *props =
        rut_object_get_properties(object, RUT_TRAIT_ID_INTROSPECTABLE);
    int n;

    for (n = 0; specs[n].name; n++) {
        rig_property_init(&properties[n], &specs[n], object, n);
    }

    props->first_property = properties;
    props->n_properties = n;
}

void
rut_introspectable_destroy(rut_object_t *object)
{
    rut_introspectable_props_t *props =
        rut_object_get_properties(object, RUT_TRAIT_ID_INTROSPECTABLE);
    rig_property_t *properties = props->first_property;
    int i;

    for (i = 0; i < props->n_properties; i++)
        rig_property_destroy(&properties[i]);
}

rig_property_t *
rut_introspectable_lookup_property(rut_object_t *object,
                                   const char *name)
{
    rut_introspectable_props_t *priv =
        rut_object_get_properties(object, RUT_TRAIT_ID_INTROSPECTABLE);
    int i;

    for (i = 0; i < priv->n_properties; i++) {
        rig_property_t *property = priv->first_property + i;
        if (strcmp(property->spec->name, name) == 0)
            return property;
    }

    return NULL;
}

void
rut_introspectable_foreach_property(
    rut_object_t *object,
    rut_introspectable_property_callback_t callback,
    void *user_data)
{
    rut_introspectable_props_t *priv =
        rut_object_get_properties(object, RUT_TRAIT_ID_INTROSPECTABLE);
    int i;

    for (i = 0; i < priv->n_properties; i++) {
        rig_property_t *property = priv->first_property + i;
        callback(property, user_data);
    }
}
