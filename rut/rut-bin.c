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

#include <math.h>

#include "rut-bin.h"
#include "rut-interfaces.h"
#include "rut-transform.h"

struct _rut_bin_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    c_list_t preferred_size_cb_list;

    rut_object_t *child_transform;

    rut_object_t *child;
    rut_closure_t *child_preferred_size_closure;

    bool in_allocate;

    float left_padding, right_padding;
    float top_padding, bottom_padding;

    rut_bin_position_t x_position;
    rut_bin_position_t y_position;

    float width, height;

    rut_graphable_props_t graphable;
};

static void
_rut_bin_free(void *object)
{
    rut_bin_t *bin = object;

    rut_closure_list_disconnect_all_FIXME(&bin->preferred_size_cb_list);

    rut_bin_set_child(bin, NULL);

    rut_shell_remove_pre_paint_callback_by_graphable(bin->shell, bin);

    rut_graphable_destroy(bin);

    rut_object_free(rut_bin_t, bin);
}

static void
allocate_cb(rut_object_t *graphable, void *user_data)
{
    rut_bin_t *bin = graphable;
    float child_width, child_height;
    float child_x = bin->left_padding;
    float child_y = bin->top_padding;
    float available_width =
        bin->width - bin->left_padding - bin->right_padding;
    float available_height =
        bin->height - bin->top_padding - bin->bottom_padding;

    if (!bin->child)
        return;

    bin->in_allocate = true;

    rut_sizable_get_preferred_width(bin->child,
                                    -1, /* for_height */
                                    NULL, /* min_width_p */
                                    &child_width);

    if (child_width > available_width)
        child_width = available_width;

    switch (bin->x_position) {
        case RUT_BIN_POSITION_BEGIN:
            break;

        case RUT_BIN_POSITION_CENTER:
            if (child_width < available_width)
                child_x = nearbyintf(bin->width / 2.0f - child_width / 2.0f);
            break;

        case RUT_BIN_POSITION_END:
            if (child_width < available_width)
                child_x = bin->width - bin->right_padding - child_width;
            break;

        case RUT_BIN_POSITION_EXPAND:
            child_width = available_width;
            break;
    }

    rut_sizable_get_preferred_height(bin->child,
                                     child_width, /* for_width */
                                     NULL, /* min_height_p */
                                     &child_height);

    if (child_height > available_height)
        child_height = available_height;

    switch (bin->y_position) {
        case RUT_BIN_POSITION_BEGIN:
            break;

        case RUT_BIN_POSITION_CENTER:
            if (child_height < available_height)
                child_y = nearbyintf(bin->height / 2.0f - child_height / 2.0f);
            break;

        case RUT_BIN_POSITION_END:
            if (child_height < available_height)
                child_y = bin->height - bin->bottom_padding - child_height;
            break;

        case RUT_BIN_POSITION_EXPAND:
            child_height = available_height;
            break;
    }

    rut_transform_init_identity(bin->child_transform);
    rut_transform_translate(bin->child_transform, child_x, child_y, 0.0f);
    rut_sizable_set_size(bin->child, child_width, child_height);

    bin->in_allocate = false;
}

static void
queue_allocation(rut_bin_t *bin)
{
    rut_shell_add_pre_paint_callback(
        bin->shell, bin, allocate_cb, NULL /* user_data */);
}

static void
preferred_size_changed(rut_bin_t *bin)
{
    rut_closure_list_invoke(
        &bin->preferred_size_cb_list, rut_sizeable_preferred_size_callback_t, bin);
}

static void
rut_bin_set_size(void *object, float width, float height)
{
    rut_bin_t *bin = object;

    if (width == bin->width && height == bin->height)
        return;

    bin->width = width;
    bin->height = height;

    queue_allocation(bin);
}

static void
rut_bin_get_preferred_width(void *sizable,
                            float for_height,
                            float *min_width_p,
                            float *natural_width_p)
{
    rut_bin_t *bin = sizable;
    float min_width = bin->left_padding + bin->right_padding;
    float natural_width = min_width;

    if (bin->child) {
        float child_min_width = 0.0f, child_natural_width = 0.0f;

        if (for_height != -1.0f) {
            for_height -= bin->top_padding + bin->bottom_padding;
            if (for_height < 0.0f)
                for_height = 0.0f;
        }

        rut_sizable_get_preferred_width(bin->child,
                                        for_height,
                                        min_width_p ? &child_min_width : NULL,
                                        natural_width_p ? &child_natural_width
                                        : NULL);

        min_width += child_min_width;
        natural_width += child_natural_width;
    }

    if (min_width_p)
        *min_width_p = min_width;
    if (natural_width_p)
        *natural_width_p = natural_width;
}

static void
rut_bin_get_preferred_height(void *sizable,
                             float for_width,
                             float *min_height_p,
                             float *natural_height_p)
{
    rut_bin_t *bin = sizable;
    float min_height = bin->top_padding + bin->bottom_padding;
    float natural_height = min_height;

    if (bin->child) {
        float child_min_height = 0.0f, child_natural_height = 0.0f;

        if (for_width != -1.0f) {
            for_width -= bin->left_padding + bin->right_padding;
            if (for_width < 0.0f)
                for_width = 0.0f;
        }

        rut_sizable_get_preferred_height(
            bin->child,
            for_width,
            min_height_p ? &child_min_height : NULL,
            natural_height_p ? &child_natural_height : NULL);

        min_height += child_min_height;
        natural_height += child_natural_height;
    }

    if (min_height_p)
        *min_height_p = min_height;
    if (natural_height_p)
        *natural_height_p = natural_height;
}

static rut_closure_t *
rut_bin_add_preferred_size_callback(void *object,
                                    rut_sizeable_preferred_size_callback_t cb,
                                    void *user_data,
                                    rut_closure_destroy_callback_t destroy)
{
    rut_bin_t *bin = object;

    return rut_closure_list_add_FIXME(
        &bin->preferred_size_cb_list, cb, user_data, destroy);
}

static void
rut_bin_get_size(void *object, float *width, float *height)
{
    rut_bin_t *bin = object;

    *width = bin->width;
    *height = bin->height;
}

rut_type_t rut_bin_type;

static void
_rut_bin_init_type(void)
{

    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_bin_set_size,                   rut_bin_get_size,
        rut_bin_get_preferred_width,        rut_bin_get_preferred_height,
        rut_bin_add_preferred_size_callback
    };

    rut_type_t *type = &rut_bin_type;
#define TYPE rut_bin_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_bin_free);
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

rut_bin_t *
rut_bin_new(rut_shell_t *shell)
{
    rut_bin_t *bin =
        rut_object_alloc0(rut_bin_t, &rut_bin_type, _rut_bin_init_type);

    bin->shell = shell;

    bin->x_position = RUT_BIN_POSITION_EXPAND;
    bin->y_position = RUT_BIN_POSITION_EXPAND;

    c_list_init(&bin->preferred_size_cb_list);

    rut_graphable_init(bin);

    bin->child_transform = rut_transform_new(shell);
    rut_graphable_add_child(bin, bin->child_transform);
    rut_object_unref(bin->child_transform);

    return bin;
}

static void
child_preferred_size_cb(rut_object_t *sizable, void *user_data)
{
    rut_bin_t *bin = user_data;

    /* The change in preference will be because we just changed the
     * child's size... */
    if (bin->in_allocate)
        return;

    preferred_size_changed(bin);
    queue_allocation(bin);
}

void
rut_bin_set_child(rut_bin_t *bin, rut_object_t *child_widget)
{
    c_return_if_fail(rut_object_get_type(bin) == &rut_bin_type);

    if (bin->child == child_widget)
        return;

    if (child_widget)
        rut_object_claim(child_widget, bin);

    if (bin->child) {
        rut_graphable_remove_child(bin->child);
        rut_closure_disconnect_FIXME(bin->child_preferred_size_closure);
        bin->child_preferred_size_closure = NULL;
        rut_object_release(bin->child, bin);
    }

    bin->child = child_widget;

    if (child_widget) {
        rut_graphable_add_child(bin->child_transform, child_widget);
        bin->child_preferred_size_closure =
            rut_sizable_add_preferred_size_callback(
                child_widget, child_preferred_size_cb, bin, NULL /* destroy */);
        queue_allocation(bin);
    }

    preferred_size_changed(bin);
    rut_shell_queue_redraw(bin->shell);
}

rut_object_t *
rut_bin_get_child(rut_bin_t *bin)
{
    return bin->child;
}

void
rut_bin_set_x_position(rut_bin_t *bin, rut_bin_position_t position)
{
    bin->x_position = position;
    queue_allocation(bin);
}

void
rut_bin_set_y_position(rut_bin_t *bin, rut_bin_position_t position)
{
    bin->y_position = position;
    queue_allocation(bin);
}

void
rut_bin_set_top_padding(rut_object_t *obj, float top_padding)
{
    rut_bin_t *bin = obj;

    bin->top_padding = top_padding;
    preferred_size_changed(bin);
    queue_allocation(bin);
}

void
rut_bin_set_bottom_padding(rut_object_t *obj, float bottom_padding)
{
    rut_bin_t *bin = obj;

    bin->bottom_padding = bottom_padding;
    preferred_size_changed(bin);
    queue_allocation(bin);
}

void
rut_bin_set_left_padding(rut_object_t *obj, float left_padding)
{
    rut_bin_t *bin = obj;

    bin->left_padding = left_padding;
    preferred_size_changed(bin);
    queue_allocation(bin);
}

void
rut_bin_set_right_padding(rut_object_t *obj, float right_padding)
{
    rut_bin_t *bin = obj;

    bin->right_padding = right_padding;
    preferred_size_changed(bin);
    queue_allocation(bin);
}
