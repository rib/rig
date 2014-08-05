/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013 Intel Corporation.
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
 *
 */

#include <config.h>

#include "rut-object.h"
#include "rut-closure.h"
#include "rut-interfaces.h"

void
rut_composite_sizable_get_preferred_width(void *sizable,
                                          float for_height,
                                          float *min_width_p,
                                          float *natural_width_p)
{
    rut_object_t **container =
        rut_object_get_properties(sizable, RUT_TRAIT_ID_COMPOSITE_SIZABLE);

    rut_sizable_get_preferred_width(
        *container, for_height, min_width_p, natural_width_p);
}

void
rut_composite_sizable_get_preferred_height(void *sizable,
                                           float for_width,
                                           float *min_height_p,
                                           float *natural_height_p)
{
    rut_object_t **container =
        rut_object_get_properties(sizable, RUT_TRAIT_ID_COMPOSITE_SIZABLE);

    rut_sizable_get_preferred_height(
        *container, for_width, min_height_p, natural_height_p);
}

typedef struct _forwarding_closure_t {
    rut_object_t *composite_sizable;
    rut_sizeable_preferred_size_callback_t cb;
    void *user_data;
} forwarding_closure_t;

static void
forwarding_closure_destroy_cb(void *user_data)
{
    c_slice_free(forwarding_closure_t, user_data);
}

static void
forward_preferred_size_change_cb(rut_object_t *container,
                                 void *user_data)
{
    forwarding_closure_t *closure = user_data;
    closure->cb(closure->composite_sizable, closure->user_data);
}

rut_closure_t *
rut_composite_sizable_add_preferred_size_callback(
    void *object,
    rut_sizeable_preferred_size_callback_t cb,
    void *user_data,
    rut_closure_destroy_callback_t destroy)
{
    rut_object_t **container =
        rut_object_get_properties(object, RUT_TRAIT_ID_COMPOSITE_SIZABLE);
    forwarding_closure_t *closure = c_slice_new(forwarding_closure_t);

    closure->composite_sizable = object;
    closure->cb = cb;
    closure->user_data = user_data;

    return rut_sizable_add_preferred_size_callback(
        *container,
        forward_preferred_size_change_cb,
        closure,
        forwarding_closure_destroy_cb);
}

void
rut_composite_sizable_set_size(void *object, float width, float height)
{
    rut_object_t **container =
        rut_object_get_properties(object, RUT_TRAIT_ID_COMPOSITE_SIZABLE);

    rut_sizable_set_size(*container, width, height);
}

void
rut_composite_sizable_get_size(void *object, float *width, float *height)
{
    rut_object_t **container =
        rut_object_get_properties(object, RUT_TRAIT_ID_COMPOSITE_SIZABLE);

    rut_sizable_get_size(*container, width, height);
}
