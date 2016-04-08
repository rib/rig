/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012-2014 Intel Corporation.
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

#include "rut-box-layout.h"
#include "rut-transform.h"
#include "rut-util.h"
#include "rig-introspectable.h"

enum {
    RUT_BOX_LAYOUT_PROP_PACKING,
    RUT_BOX_LAYOUT_N_PROPS
};

typedef struct {
    c_list_t link;
    rut_object_t *transform;
    rut_object_t *widget;
    rut_closure_t *preferred_size_closure;

    float flex_grow;
    float flex_shrink;

    /* The allocation algorithm needs to repeatedly iterate over
     * 'flexible' children to resolve their size based on the
     * flex_grow/shrink weights of all other flexible children
     * but without violating the minimum size constraints of
     * any of the children.
     */
    c_list_t flexible_link;

    float main_size;
    float min_size;

} rut_box_layout_child_t;

struct _rut_box_layout_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    c_list_t preferred_size_cb_list;
    c_list_t children;
    int n_children;

    bool in_allocate;

    rut_box_layout_packing_t packing;

    float width, height;

    rut_graphable_props_t graphable;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RUT_BOX_LAYOUT_N_PROPS];
};

static rig_property_spec_t _rut_box_layout_prop_specs[] = {
    { .name = "packing",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = (void *)rut_box_layout_get_packing,
      .setter.integer_type = (void *)rut_box_layout_set_packing,
      .nick = "Packing",
      .blurb = "The packing direction",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .integer = RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT } },
    { 0 }
};

rut_type_t rut_box_layout_type;

static void
_rut_box_layout_free(void *object)
{
    rut_box_layout_t *box = object;

    rut_closure_list_disconnect_all_FIXME(&box->preferred_size_cb_list);

    while (!c_list_empty(&box->children)) {
        rut_box_layout_child_t *child =
            rut_container_of(box->children.next, child, link);

        rut_box_layout_remove(box, child->widget);
    }

    rut_shell_remove_pre_paint_callback_by_graphable(box->shell, box);

    rut_object_unref(box->shell);

    rut_graphable_destroy(box);

    rut_object_free(rut_box_layout_t, box);
}


struct allocate_state
{
    void (*get_child_main_size)(void *sizable,
                                float for_b,
                                float *min_size_p,
                                float *natural_size_p);

    float main_size;
    float cross_size;

    c_list_t flexible;
};

static void
allocate_cb(rut_object_t *graphable, void *user_data)
{
    rut_box_layout_t *box = graphable;
    struct allocate_state state;
    int n_children = box->n_children;
    rut_box_layout_child_t *child;
    float total_main_size = 0;
    float main_offset = 0;

    if (n_children == 0)
        return;

    box->in_allocate = true;

    switch (box->packing) {
    case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
    case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
        state.get_child_main_size = rut_sizable_get_preferred_width;
        state.main_size = box->width;
        state.cross_size = box->height;
        break;
    case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
    case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
        state.get_child_main_size = rut_sizable_get_preferred_height;
        state.main_size = box->height;
        state.cross_size = box->width;
        break;
    }

    c_list_for_each(child, &box->children, link) {
        state.get_child_main_size(child->widget,
                                  state.cross_size,
                                  &child->min_size,
                                  &child->main_size);

        total_main_size += child->main_size;
    }

    if (total_main_size > state.main_size) {
        float current_size = total_main_size;
        bool hit_constraint = true;

        /* shrink */

        c_list_init(&state.flexible);

        c_list_for_each(child, &box->children, link) {
            if (child->flex_shrink)
                c_list_insert(state.flexible.prev, &child->flexible_link);
        }

        /* We shrink iteratively because we might reach the minimum
         * size of some children and therefore one iteration might
         * not shrink as much as is required. */
        while (!c_list_empty(&state.flexible) && hit_constraint) {
            rut_box_layout_child_t *tmp;
            float total_shrink_size = current_size - state.main_size;
            float weights_total = 0;

            c_list_for_each(child, &state.flexible, flexible_link)
                weights_total += child->flex_shrink;

            hit_constraint = false;
            c_list_for_each_safe(child, tmp, &state.flexible, flexible_link) {
                float proportion = child->flex_shrink / weights_total;
                float shrink_size = total_shrink_size * proportion;

                child->main_size -= shrink_size;
                current_size -= shrink_size;

                /* Check if we've broken a minimum size constraint.. */
                if (child->main_size < child->min_size) {
                    current_size += (child->min_size - child->main_size);
                    child->main_size = child->min_size;

                    /* This child should no longer flex */
                    c_list_remove(&child->flexible_link);

                    hit_constraint = true;
                }
            }
        }

    } else if (total_main_size < state.main_size) {

        /* grow */

        float total_grow_size = state.main_size - total_main_size;
        float weights_total = 0;

        c_list_init(&state.flexible);

        c_list_for_each(child, &box->children, link) {
            if (child->flex_grow) {
                c_list_insert(state.flexible.prev, &child->flexible_link);
                weights_total += child->flex_grow;
            }
        }

        /* XXX: when growing we will never reach a maximum size
         * constraint for a child, so we don't need to worry about
         * flexing iteratively like we do when shrinking. */

        c_list_for_each(child, &state.flexible, flexible_link) {
            float proportion = child->flex_grow / weights_total;

            child->main_size += total_grow_size * proportion;
        }
    }

    c_list_for_each(child, &box->children, link) {
        float x = 0, y = 0, width, height;

        switch (box->packing) {
            case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
                width = child->main_size;
                height = state.cross_size;
                x = main_offset;
                break;
            case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
                width = child->main_size;
                height = state.cross_size;
                x = state.main_size - main_offset - child->main_size;
                break;
            case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
                width = state.cross_size;
                height = child->main_size;
                y = main_offset;
                break;
            case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
                width = state.cross_size;
                height = child->main_size;
                y = state.main_size - main_offset - child->main_size;
                break;
        }

        rut_sizable_set_size(child->widget, width, height);
        rut_transform_init_identity(child->transform);
        rut_transform_translate(child->transform, x, y, 0);

        main_offset += child->main_size;
    }

    box->in_allocate = false;
}

static void
queue_allocation(rut_box_layout_t *box)
{
    rut_shell_add_pre_paint_callback(
        box->shell, box, allocate_cb, NULL /* user_data */);
}

static void
preferred_size_changed(rut_box_layout_t *box)
{
    rut_closure_list_invoke(
        &box->preferred_size_cb_list, rut_sizeable_preferred_size_callback_t, box);
}

static void
rut_box_layout_set_size(void *object, float width, float height)
{
    rut_box_layout_t *box = object;

    if (width == box->width && height == box->height)
        return;

    box->width = width;
    box->height = height;

    queue_allocation(box);
}

static void
get_preferred_main_size(rut_box_layout_t *box,
                        float for_size,
                        float *min_size_p,
                        float *natural_size_p)
{
    float total_min_size = 0;
    float total_natural_size = 0;
    rut_box_layout_child_t *child;

    c_list_for_each(child, &box->children, link)
    {
        float min_size = 0, natural_size = 0;

        switch (box->packing) {
        case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
        case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
            rut_sizable_get_preferred_width(child->widget,
                                            for_size, /* for_height */
                                            min_size_p ? &min_size : NULL,
                                            natural_size_p ? &natural_size
                                            : NULL);
            break;

        case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
        case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
            rut_sizable_get_preferred_height(child->widget,
                                             for_size, /* for_width */
                                             min_size_p ? &min_size : NULL,
                                             natural_size_p ? &natural_size
                                             : NULL);
            break;
        }

        total_min_size += min_size;
        total_natural_size += natural_size;
    }

    if (min_size_p)
        *min_size_p = total_min_size;
    if (natural_size_p)
        *natural_size_p = total_natural_size;
}

static void
get_preferred_cross_size(rut_box_layout_t *box,
                         float for_size,
                         float *min_size_p,
                         float *natural_size_p)
{
    float max_min_size = 0.0f;
    float max_natural_size = 0.0f;
    rut_box_layout_child_t *child;

    c_list_for_each(child, &box->children, link)
    {
        float min_size = 0.0f, natural_size = 0.0f;

        switch (box->packing) {
        case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
        case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
            rut_sizable_get_preferred_height(child->widget,
                                             -1, /* for_width */
                                             min_size_p ? &min_size : NULL,
                                             natural_size_p ? &natural_size
                                             : NULL);
            break;

        case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
        case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
            rut_sizable_get_preferred_width(child->widget,
                                            -1, /* for_height */
                                            min_size_p ? &min_size : NULL,
                                            natural_size_p ? &natural_size
                                            : NULL);
            break;
        }

        if (min_size > max_min_size)
            max_min_size = min_size;
        if (natural_size > max_natural_size)
            max_natural_size = natural_size;
    }

    if (min_size_p)
        *min_size_p = max_min_size;
    if (natural_size_p)
        *natural_size_p = max_natural_size;
}

static void
rut_box_layout_get_preferred_width(void *sizable,
                                   float for_height,
                                   float *min_width_p,
                                   float *natural_width_p)
{
    rut_box_layout_t *box = sizable;

    switch (box->packing) {
    case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
    case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
        get_preferred_main_size(box, for_height, min_width_p, natural_width_p);
        break;

    case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
    case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
        get_preferred_cross_size(box, for_height, min_width_p, natural_width_p);
        break;
    }
}

static void
rut_box_layout_get_preferred_height(void *sizable,
                                    float for_width,
                                    float *min_height_p,
                                    float *natural_height_p)
{
    rut_box_layout_t *box = sizable;

    switch (box->packing) {
    case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
    case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
        get_preferred_cross_size(
            box, for_width, min_height_p, natural_height_p);
        break;

    case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
    case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
        get_preferred_main_size(box, for_width, min_height_p, natural_height_p);
        break;
    }
}

static rut_closure_t *
rut_box_layout_add_preferred_size_callback(
    void *object,
    rut_sizeable_preferred_size_callback_t cb,
    void *user_data,
    rut_closure_destroy_callback_t destroy)
{
    rut_box_layout_t *box = object;

    return rut_closure_list_add_FIXME(
        &box->preferred_size_cb_list, cb, user_data, destroy);
}

static void
rut_box_layout_get_size(void *object, float *width, float *height)
{
    rut_box_layout_t *box = object;

    *width = box->width;
    *height = box->height;
}

static void
_rut_box_layout_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_box_layout_set_size,
        rut_box_layout_get_size,
        rut_box_layout_get_preferred_width,
        rut_box_layout_get_preferred_height,
        rut_box_layout_add_preferred_size_callback
    };

    rut_type_t *type = &rut_box_layout_type;
#define TYPE rut_box_layout_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_box_layout_free);
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

rut_box_layout_t *
rut_box_layout_new(rut_shell_t *shell,
                   rut_box_layout_packing_t packing)
{
    rut_box_layout_t *box = rut_object_alloc0(
        rut_box_layout_t, &rut_box_layout_type, _rut_box_layout_init_type);

    box->shell = rut_object_ref(shell);
    box->packing = packing;

    c_list_init(&box->preferred_size_cb_list);
    c_list_init(&box->children);

    rut_graphable_init(box);

    rig_introspectable_init(box, _rut_box_layout_prop_specs, box->properties);

    queue_allocation(box);

    return box;
}

static void
child_preferred_size_cb(rut_object_t *sizable, void *user_data)
{
    rut_box_layout_t *box = user_data;

    /* The change in preference will be because we just changed the
     * child's size... */
    if (box->in_allocate)
        return;

    preferred_size_changed(box);
    queue_allocation(box);
}

void
rut_box_layout_add(rut_box_layout_t *box,
                   bool expand,
                   rut_object_t *child_widget)
{
    rut_box_layout_child_t *child = c_slice_new(rut_box_layout_child_t);

    c_return_if_fail(rut_object_get_type(box) == &rut_box_layout_type);

    child->transform = rut_transform_new(box->shell);
    rut_graphable_add_child(box, child->transform);
    rut_object_unref(child->transform);

    child->widget = child_widget;
    rut_graphable_add_child(child->transform, child_widget);

    child->flex_grow = 1;
    child->flex_shrink = 1;

    if (!expand)
        child->flex_grow = 0;

    box->n_children++;

    child->preferred_size_closure = rut_sizable_add_preferred_size_callback(
        child_widget, child_preferred_size_cb, box, NULL /* destroy */);

    c_list_insert(box->children.prev, &child->link);

    preferred_size_changed(box);
    queue_allocation(box);
}

void
rut_box_layout_remove(rut_box_layout_t *box, rut_object_t *child_widget)
{
    rut_box_layout_child_t *child;

    c_return_if_fail(box->n_children > 0);

    c_list_for_each(child, &box->children, link)
    {
        if (child->widget == child_widget) {
            rut_closure_disconnect_FIXME(child->preferred_size_closure);

            rut_graphable_remove_child(child->widget);
            rut_graphable_remove_child(child->transform);

            c_list_remove(&child->link);
            c_slice_free(rut_box_layout_child_t, child);

            preferred_size_changed(box);
            queue_allocation(box);

            box->n_children--;

            break;
        }
    }
}

int
rut_box_layout_get_packing(rut_object_t *obj)
{
    rut_box_layout_t *box = obj;
    return box->packing;
}

void
rut_box_layout_set_packing(rut_object_t *obj,
                           rut_box_layout_packing_t packing)
{
    rut_box_layout_t *box = obj;

    if (box->packing == packing)
        return;

    box->packing = packing;

    rig_property_dirty(&box->shell->property_ctx,
                       &box->properties[RUT_BOX_LAYOUT_PROP_PACKING]);

    queue_allocation(box);
}
