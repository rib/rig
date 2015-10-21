/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012 Intel Corporation.
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

#include <cglib/cglib.h>
#include <string.h>
#include <math.h>

#include "rut-shell.h"
#include "rut-closure.h"
#include "rut-interfaces.h"
#include "rut-introspectable.h"
#include "rut-stack.h"

enum {
    RUT_STACK_PROP_WIDTH,
    RUT_STACK_PROP_HEIGHT,
    RUT_STACK_N_PROPS
};

typedef struct {
    c_list_t list_node;

    rut_closure_t *preferred_size_closure;

    rut_object_t *child;
} rut_stack_child_t;

struct _rut_stack_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_graphable_props_t graphable;

    int width;
    int height;

    c_list_t children;

    bool in_allocate;

    c_list_t preferred_size_cb_list;

    rut_introspectable_props_t introspectable;
    rut_property_t properties[RUT_STACK_N_PROPS];
};

static rut_property_spec_t _rut_stack_prop_specs[] = {
    { .name = "width",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rut_stack_t, width),
      .setter.float_type = rut_stack_set_width },
    { .name = "height",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rut_stack_t, height),
      .setter.float_type = rut_stack_set_height },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_stack_free(void *object)
{
    rut_stack_t *stack = object;

    rut_introspectable_destroy(stack);
    rut_graphable_destroy(stack);

    rut_shell_remove_pre_paint_callback_by_graphable(stack->shell, stack);

    /* Destroying the graphable state should remove all the children */
    c_warn_if_fail(c_list_empty(&stack->children));

    rut_object_free(rut_stack_t, stack);
}

static void
allocate_cb(rut_object_t *graphable, void *user_data)
{
    rut_stack_t *stack = graphable;
    rut_stack_child_t *child_data;

    stack->in_allocate = true;

    c_list_for_each(child_data, &stack->children, list_node) {
        rut_object_t *child = child_data->child;
        if (rut_object_is(child, RUT_TRAIT_ID_SIZABLE))
            rut_sizable_set_size(child, stack->width, stack->height);
    }

    stack->in_allocate = false;
}

static void
queue_allocation(rut_stack_t *stack)
{
    rut_shell_add_pre_paint_callback(
        stack->shell, stack, allocate_cb, NULL /* user_data */);
}

static void
preferred_size_changed(rut_stack_t *stack)
{
    rut_closure_list_invoke(&stack->preferred_size_cb_list,
                            rut_sizeable_preferred_size_callback_t,
                            stack);
}

static void
_rut_stack_child_removed_cb(rut_object_t *parent,
                            rut_object_t *child)
{
    rut_stack_t *stack = parent;
    rut_stack_child_t *child_data;

    /* non-sizable children are allowed but we don't track any
     * child-data for them... */
    if (!rut_object_is(child, RUT_TRAIT_ID_SIZABLE))
        return;

    c_list_for_each(child_data, &stack->children, list_node)
    if (child_data->child == child) {
        rut_closure_disconnect_FIXME(child_data->preferred_size_closure);
        c_list_remove(&child_data->list_node);
        c_slice_free(rut_stack_child_t, child_data);
        rut_object_release(child, parent);

        preferred_size_changed(stack);
        if (!c_list_empty(&stack->children))
            queue_allocation(stack);
        return;
    }

    c_warn_if_reached();
}

static void
child_preferred_size_cb(rut_object_t *sizable, void *user_data)
{
    rut_stack_t *stack = user_data;

    /* The change in preference will be because we just changed the
     * child's size... */
    if (stack->in_allocate)
        return;

    preferred_size_changed(stack);
    queue_allocation(stack);
}

static void
_rut_stack_child_added_cb(rut_object_t *parent, rut_object_t *child)
{
    rut_stack_t *stack = parent;
    rut_stack_child_t *child_data;

    /* non-sizable children are allowed but we don't track any
     * child-data for them... */
    if (!rut_object_is(child, RUT_TRAIT_ID_SIZABLE))
        return;

    child_data = c_slice_new(rut_stack_child_t);
    child_data->child = rut_object_claim(child, parent);

    child_data->preferred_size_closure =
        rut_sizable_add_preferred_size_callback(
            child, child_preferred_size_cb, stack, NULL /* destroy */);

    c_list_insert(stack->children.prev, &child_data->list_node);

    preferred_size_changed(stack);
    queue_allocation(stack);
}

static void
rut_stack_get_preferred_width(void *object,
                              float for_height,
                              float *min_width_p,
                              float *natural_width_p)
{
    rut_stack_t *stack = object;
    float max_min_width = 0.0f;
    float max_natural_width = 0.0f;
    rut_stack_child_t *child_data;

    c_list_for_each(child_data, &stack->children, list_node)
    {
        rut_object_t *child = child_data->child;
        float child_min_width;
        float child_natural_width;
        rut_sizable_get_preferred_width(
            child, for_height, &child_min_width, &child_natural_width);
        if (child_min_width > max_min_width)
            max_min_width = child_min_width;
        if (child_natural_width > max_natural_width)
            max_natural_width = child_natural_width;
    }

    if (min_width_p)
        *min_width_p = max_min_width;
    if (natural_width_p)
        *natural_width_p = max_natural_width;
}

static void
rut_stack_get_preferred_height(void *object,
                               float for_width,
                               float *min_height_p,
                               float *natural_height_p)
{
    rut_stack_t *stack = object;
    float max_min_height = 0.0f;
    float max_natural_height = 0.0f;
    rut_stack_child_t *child_data;

    c_list_for_each(child_data, &stack->children, list_node)
    {
        rut_object_t *child = child_data->child;
        float child_min_height;
        float child_natural_height;
        rut_sizable_get_preferred_height(
            child, for_width, &child_min_height, &child_natural_height);
        if (child_min_height > max_min_height)
            max_min_height = child_min_height;
        if (child_natural_height > max_natural_height)
            max_natural_height = child_natural_height;
    }

    if (min_height_p)
        *min_height_p = max_min_height;
    if (natural_height_p)
        *natural_height_p = max_natural_height;
}

static rut_closure_t *
rut_stack_add_preferred_size_callback(void *object,
                                      rut_sizeable_preferred_size_callback_t cb,
                                      void *user_data,
                                      rut_closure_destroy_callback_t destroy)
{
    rut_stack_t *stack = object;

    return rut_closure_list_add_FIXME(
        &stack->preferred_size_cb_list, cb, user_data, destroy);
}

rut_type_t rut_stack_type;

static void
_rut_stack_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = {
        _rut_stack_child_removed_cb, _rut_stack_child_added_cb,
        NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_stack_set_size,                   rut_stack_get_size,
        rut_stack_get_preferred_width,        rut_stack_get_preferred_height,
        rut_stack_add_preferred_size_callback
    };

    rut_type_t *type = &rut_stack_type;
#define TYPE rut_stack_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_stack_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

void
rut_stack_set_size(rut_object_t *self, float width, float height)
{
    rut_stack_t *stack = self;

    if (stack->width == width && stack->height == height)
        return;

    stack->width = width;
    stack->height = height;

    rut_property_dirty(&stack->shell->property_ctx,
                       &stack->properties[RUT_STACK_PROP_WIDTH]);
    rut_property_dirty(&stack->shell->property_ctx,
                       &stack->properties[RUT_STACK_PROP_HEIGHT]);

    queue_allocation(stack);
}

void
rut_stack_set_width(rut_object_t *self, float width)
{
    rut_stack_t *stack = self;

    rut_stack_set_size(stack, width, stack->height);
}

void
rut_stack_set_height(rut_object_t *self, float height)
{
    rut_stack_t *stack = self;

    rut_stack_set_size(stack, stack->width, height);
}

void
rut_stack_get_size(rut_object_t *self, float *width, float *height)
{
    rut_stack_t *stack = self;

    *width = stack->width;
    *height = stack->height;
}

rut_stack_t *
rut_stack_new(rut_shell_t *shell, float width, float height)
{
    rut_stack_t *stack =
        rut_object_alloc0(rut_stack_t, &rut_stack_type, _rut_stack_init_type);

    stack->shell = shell;

    c_list_init(&stack->children);
    c_list_init(&stack->preferred_size_cb_list);

    rut_introspectable_init(stack, _rut_stack_prop_specs, stack->properties);

    rut_graphable_init(stack);

    rut_stack_set_size(stack, width, height);

    queue_allocation(stack);

    return stack;
}

void
rut_stack_add(rut_stack_t *stack, rut_object_t *child)
{
    rut_graphable_add_child(stack, child);
}
