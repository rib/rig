/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation.
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

#include <clib.h>

#include "rut-interfaces.h"
#include "rut-shim.h"

struct _rut_shim_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    c_list_t preferred_size_cb_list;

    rut_shim_axis_t axis;

    float width;
    float height;

    rut_object_t *child;
    rut_closure_t *child_preferred_size_closure;

    bool in_allocate;

    rut_graphable_props_t graphable;
};

static void
_rut_shim_free(void *object)
{
    rut_shim_t *shim = object;

    rut_closure_list_disconnect_all_FIXME(&shim->preferred_size_cb_list);

    rut_graphable_destroy(shim);

    rut_object_free(rut_shim_t, object);
}

static void
allocate_cb(rut_object_t *graphable, void *user_data)
{
    rut_shim_t *shim = graphable;

    if (!shim->child)
        return;

    shim->in_allocate = true;

    rut_sizable_set_size(shim->child, shim->width, shim->height);

    shim->in_allocate = false;
}

static void
queue_allocation(rut_shim_t *shim)
{
    rut_shell_add_pre_paint_callback(
        shim->shell, shim, allocate_cb, NULL /* user_data */);
}

static void
rut_shim_get_preferred_width(void *sizable,
                             float for_height,
                             float *min_width_p,
                             float *natural_width_p)
{
    rut_shim_t *shim = sizable;

    if (shim->axis == RUT_SHIM_AXIS_Y && shim->child) {
        rut_sizable_get_preferred_width(
            shim->child, shim->height, min_width_p, natural_width_p);
    } else {
        if (min_width_p)
            *min_width_p = shim->width;
        if (natural_width_p)
            *natural_width_p = shim->width;
    }
}

static void
rut_shim_get_preferred_height(void *sizable,
                              float for_width,
                              float *min_height_p,
                              float *natural_height_p)
{
    rut_shim_t *shim = sizable;

    if (shim->axis == RUT_SHIM_AXIS_X && shim->child) {
        rut_sizable_get_preferred_height(
            shim->child, shim->width, min_height_p, natural_height_p);
    } else {
        if (min_height_p)
            *min_height_p = shim->height;
        if (natural_height_p)
            *natural_height_p = shim->height;
    }
}

static rut_closure_t *
rut_shim_add_preferred_size_callback(void *object,
                                     rut_sizeable_preferred_size_callback_t cb,
                                     void *user_data,
                                     rut_closure_destroy_callback_t destroy)
{
    rut_shim_t *shim = object;

    return rut_closure_list_add_FIXME(
        &shim->preferred_size_cb_list, cb, user_data, destroy);
}

rut_type_t rut_shim_type;

static void
_rut_shim_init_type(void)
{

    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_shim_set_size,                   rut_shim_get_size,
        rut_shim_get_preferred_width,        rut_shim_get_preferred_height,
        rut_shim_add_preferred_size_callback
    };

    rut_type_t *type = &rut_shim_type;
#define TYPE rut_shim_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_shim_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);

#undef TYPE
}

rut_shim_t *
rut_shim_new(rut_shell_t *shell, float width, float height)
{
    rut_shim_t *shim =
        rut_object_alloc0(rut_shim_t, &rut_shim_type, _rut_shim_init_type);

    shim->shell = shell;

    c_list_init(&shim->preferred_size_cb_list);

    rut_graphable_init(shim);

    shim->width = width;
    shim->height = height;

    return shim;
}

void
rut_shim_set_width(rut_shim_t *shim, float width)
{
    rut_shim_set_size(shim, width, shim->height);
}

void
rut_shim_set_height(rut_shim_t *shim, float height)
{
    rut_shim_set_size(shim, shim->width, height);
}

static void
preferred_size_changed(rut_shim_t *shim)
{
    rut_closure_list_invoke(&shim->preferred_size_cb_list,
                            rut_sizeable_preferred_size_callback_t,
                            shim);
}

void
rut_shim_set_size(rut_object_t *self, float width, float height)
{
    rut_shim_t *shim = self;

    if (shim->width == width && shim->height == height)
        return;

    shim->width = width;
    shim->height = height;

    preferred_size_changed(shim);
}

void
rut_shim_get_size(rut_object_t *self, float *width, float *height)
{
    rut_shim_t *shim = self;
    *width = shim->width;
    *height = shim->height;
}

static void
child_preferred_size_cb(rut_object_t *sizable, void *user_data)
{
    rut_shim_t *shim = user_data;

    /* The change in preference will be because we just changed the
     * child's size... */
    if (shim->in_allocate)
        return;

    if (shim->axis == RUT_SHIM_AXIS_XY)
        return;

    preferred_size_changed(shim);
    queue_allocation(shim);
}

void
rut_shim_set_child(rut_shim_t *shim, rut_object_t *child)
{
    c_return_if_fail(rut_object_get_type(shim) == &rut_shim_type);

    if (shim->child == child)
        return;

    if (shim->child) {
        rut_graphable_remove_child(shim->child);
        rut_closure_disconnect_FIXME(shim->child_preferred_size_closure);
        shim->child_preferred_size_closure = NULL;
        rut_object_unref(shim->child);
    }

    if (child) {
        shim->child = rut_object_ref(child);
        rut_graphable_add_child(shim, child);

        shim->child_preferred_size_closure =
            rut_sizable_add_preferred_size_callback(
                child, child_preferred_size_cb, shim, NULL /* destroy */);
        queue_allocation(shim);
    } else
        shim->child = NULL;

    rut_shell_queue_redraw(shim->shell);
}

void
rut_shim_remove_child(rut_shim_t *shim, rut_object_t *child)
{
    c_return_if_fail(rut_object_get_type(shim) == &rut_shim_type);
    rut_graphable_remove_child(child);
}

void
rut_shim_set_shim_axis(rut_shim_t *shim, rut_shim_axis_t axis)
{
    if (shim->axis == axis)
        return;

    shim->axis = axis;

    preferred_size_changed(shim);
}
