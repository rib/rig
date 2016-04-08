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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <config.h>

#include "rut-closure.h"
#include "rut-shell.h"
#include "rut-interfaces.h"
#include "rut-flow-layout.h"
#include "rut-transform.h"
#include "rut-introspectable.h"

#include <math.h>

enum {
    RUT_FLOW_LAYOUT_PROP_PACKING,
    RUT_FLOW_LAYOUT_PROP_X_PADDING,
    RUT_FLOW_LAYOUT_PROP_Y_PADDING,
    RUT_FLOW_LAYOUT_PROP_MIN_CHILD_WIDTH,
    RUT_FLOW_LAYOUT_PROP_MAX_CHILD_WIDTH,
    RUT_FLOW_LAYOUT_PROP_MIN_CHILD_HEIGHT,
    RUT_FLOW_LAYOUT_PROP_MAX_CHILD_HEIGHT,
    RUT_FLOW_LAYOUT_N_PROPS,
};

typedef struct {
    c_list_t link;
    rut_object_t *transform;
    rut_object_t *widget;
    rut_closure_t *preferred_size_closure;

    /* re-flowing is done on a line-by-line basis and so this is used
     * during re-flowing to link the child into the current line being
     * handled...
     */
    c_list_t line_link;

    /* During re-flowing we track the allocation in normalized
     * coordinates here. 'normalized' means that instead of using x, y,
     * width, height, we instead track coordinates that relate to the
     * packing direction of the layout. 'a' corresponds to the axis
     * in-line with the direction of the layout. */
    int a_pos;
    int b_pos;
    int a_size;

    /* We re-flow on a line-by-line basis, and once we get to the end of
     * a line we then iterate all the children of the line and map from
     * the normalized re-flow coordinates (see above) into final
     * coordinates that can later be used for allocation.
     */
    int flow_x;
    int flow_y;
    int flow_width;
    int flow_height;

} rut_flow_layout_child_t;

struct _rut_flow_layout_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    float width, height;

    rut_graphable_props_t graphable;

    c_list_t preferred_size_cb_list;
    c_list_t children;
    int n_children;

    bool in_allocate;

    rut_flow_layout_packing_t packing;

    int x_padding;
    int y_padding;

    int min_child_width;
    int max_child_width;
    int min_child_height;
    int max_child_height;

    int last_flow_line_length;

    rut_introspectable_props_t introspectable;
    rig_property_t properties[RUT_FLOW_LAYOUT_N_PROPS];

    unsigned int needs_reflow : 1;
};

static rig_property_spec_t _rut_flow_layout_prop_specs[] = {

    /**
     * rut_flow_layout_t:packing:
     *
     * The packing direction of the #rut_flow_layout_t. The children
     * of the layout will be layed out following this direction.
     *
     * This property also controls the overflowing directions
     */
    { .name = "packing",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = (void *)rut_flow_layout_get_packing,
      .setter.integer_type = (void *)rut_flow_layout_set_packing,
      .nick = "Packing",
      .blurb = "The flow packing direction",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .default_value = { .integer = RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT } },

    /**
     * rut_flow_layout_t:x-padding:
     *
     * The x-axis padding between children, in pixels.
     */
    { .name = "x-padding",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = (void *)rut_flow_layout_get_x_padding,
      .setter.integer_type = (void *)rut_flow_layout_set_x_padding,
      .nick = "X Axis Padding",
      .blurb = "The x-axis padding between children",
      .flags = RUT_PROPERTY_FLAG_READWRITE, },

    /**
     * rut_flow_layout_t:y-padding:
     *
     * The y-axis padding between children, in pixels.
     */
    { .name = "y-padding",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = (void *)rut_flow_layout_get_y_padding,
      .setter.integer_type = (void *)rut_flow_layout_set_y_padding,
      .nick = "Y Axis Padding",
      .blurb = "The y-axis padding between children",
      .flags = RUT_PROPERTY_FLAG_READWRITE, },

    /**
     * rut_flow_layout_t:min-box-width:
     *
     * Minimum width for each box in the layout, in pixels
     */
    { .name = "min_child_width",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = (void *)rut_flow_layout_get_min_child_width,
      .setter.integer_type = (void *)rut_flow_layout_set_min_child_width,
      .nick = "Minimum Child Width",
      .blurb = "The minimum width for children",
      .flags = RUT_PROPERTY_FLAG_READWRITE, },

    /**
     * rut_flow_layout_t:max-child-width:
     *
     * Maximum width for each child in the layout, in pixels. If
     * set to -1 that means there is no specific limit
     */
    { .name = "max_child_width",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = (void *)rut_flow_layout_get_max_child_width,
      .setter.integer_type = (void *)rut_flow_layout_set_max_child_width,
      .nick = "Maximum Child Width",
      .blurb = "The maximum width for children",
      .flags = RUT_PROPERTY_FLAG_READWRITE, },

    /**
     * rut_flow_layout_t:min-child-height:
     *
     * Minimum height for each child in the layout, in pixels
     */
    { .name = "min_child_height",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = (void *)rut_flow_layout_get_min_child_height,
      .setter.integer_type = (void *)rut_flow_layout_set_min_child_height,
      .nick = "Minimum Child Height",
      .blurb = "The minimum height for children",
      .flags = RUT_PROPERTY_FLAG_READWRITE, },

    /**
     * rut_flow_layout_t:max-child-height:
     *
     * Maximum height for each child in the layout, in pixels. If
     * set to -1 that means there is no specific limit
     */
    { .name = "max_child_height",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = (void *)rut_flow_layout_get_max_child_height,
      .setter.integer_type = (void *)rut_flow_layout_set_max_child_height,
      .nick = "Maximum Child Height",
      .blurb = "The maximum height for children",
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { 0 }
};

static void
rut_flow_layout_remove_child(rut_flow_layout_t *flow,
                             rut_flow_layout_child_t *child)
{
    rut_closure_disconnect_FIXME(child->preferred_size_closure);

    rut_graphable_remove_child(child->widget);
    rut_graphable_remove_child(child->transform);

    c_list_remove(&child->link);
    c_slice_free(rut_flow_layout_child_t, child);

    flow->n_children--;
}

static void
_rut_flow_layout_free(void *object)
{
    rut_flow_layout_t *flow = object;

    rut_closure_list_disconnect_all_FIXME(&flow->preferred_size_cb_list);

    while (!c_list_empty(&flow->children)) {
        rut_flow_layout_child_t *child =
            rut_container_of(flow->children.next, child, link);

        rut_flow_layout_remove_child(flow, child);
    }

    rut_shell_remove_pre_paint_callback_by_graphable(flow->shell, flow);

    rut_introspectable_destroy(flow);

    rut_graphable_destroy(flow);

    rut_object_free(rut_flow_layout_t, flow);
}

typedef void (*preferred_size_callback_t)(void *sizable,
                                          float for_b,
                                          float *min_size_p,
                                          float *natural_size_p);

typedef void (*flow_line_callback_t)(c_list_t *line_list,
                                     float line_length,
                                     float height);

static void
flow_horizontal_line_ltr(c_list_t *line_list, float line_length, float height)
{
    rut_flow_layout_child_t *tmp, *child;

    c_list_for_each_safe(child, tmp, line_list, line_link)
    {
        child->flow_x = child->a_pos;
        child->flow_y = child->b_pos;
        child->flow_width = child->a_size;
        child->flow_height = height;

        c_list_remove(&child->line_link);
    }
}

static void
flow_horizontal_line_rtl(c_list_t *line_list, float line_length, float height)
{
    rut_flow_layout_child_t *tmp, *child;

    c_list_for_each_safe(child, tmp, line_list, line_link)
    {
        child->flow_x = line_length - child->a_size - child->a_pos;
        child->flow_y = child->b_pos;
        child->flow_width = child->a_size;
        child->flow_height = height;

        c_list_remove(&child->line_link);
    }
}

static void
flow_vertical_line_ttb(c_list_t *line_list, float line_length, float width)
{
    rut_flow_layout_child_t *tmp, *child;

    c_list_for_each_safe(child, tmp, line_list, line_link)
    {
        child->flow_x = child->b_pos;
        child->flow_y = child->a_pos;
        child->flow_width = width;
        child->flow_height = child->a_size;

        c_list_remove(&child->line_link);
    }
}

static void
flow_vertical_line_btt(c_list_t *line_list, float line_length, float width)
{
    rut_flow_layout_child_t *tmp, *child;

    c_list_for_each_safe(child, tmp, line_list, line_link)
    {
        child->flow_x = child->b_pos;
        child->flow_y = line_length - child->a_size - child->a_pos;
        child->flow_width = width;
        child->flow_height = child->a_size;

        c_list_remove(&child->line_link);
    }
}

typedef struct _re_flow_state_t {
    float min_child_a_size;
    float max_child_a_size;
    preferred_size_callback_t get_a_size;
    float a_pad;
    float min_child_b_size;
    float max_child_b_size;
    preferred_size_callback_t get_b_size;
    float b_pad;
    float line_length;
    flow_line_callback_t flow_line;
} re_flow_state_t;

static void
init_reflow_state(re_flow_state_t *state,
                  rut_flow_layout_t *flow,
                  float for_width,
                  float for_height)
{
    switch (flow->packing) {
    case RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT:
    case RUT_FLOW_LAYOUT_PACKING_RIGHT_TO_LEFT:

        state->max_child_a_size = flow->max_child_width;
        state->max_child_b_size = flow->max_child_height;

        /* NB: max_child_width/height and for_width/height may == -1 */

        if (for_width >= 0) {
            if (state->max_child_a_size < 0)
                state->max_child_a_size = for_width;
            else
                state->max_child_a_size =
                    MIN(state->max_child_a_size, for_width);
        }

        if (for_height >= 0) {
            if (state->max_child_b_size < 0)
                state->max_child_b_size = for_height;
            else
                state->max_child_b_size =
                    MIN(state->max_child_b_size, for_height);
        }

        state->min_child_a_size = flow->min_child_width;
        state->min_child_b_size = flow->min_child_height;

        state->get_a_size = rut_sizable_get_preferred_width;
        state->get_b_size = rut_sizable_get_preferred_height;

        state->a_pad = flow->x_padding;
        state->b_pad = flow->y_padding;

        state->line_length = for_width;

        break;

    case RUT_FLOW_LAYOUT_PACKING_TOP_TO_BOTTOM:
    case RUT_FLOW_LAYOUT_PACKING_BOTTOM_TO_TOP:

        state->max_child_a_size = flow->max_child_height;
        state->max_child_b_size = flow->max_child_width;

        /* NB: max_child_width/height and for_width/height may == -1 */

        if (for_width >= 0) {
            if (state->max_child_b_size < 0)
                state->max_child_b_size = for_width;
            else
                state->max_child_b_size =
                    MIN(state->max_child_b_size, for_width);
        }

        if (for_height >= 0) {
            if (state->max_child_a_size < 0)
                state->max_child_a_size = for_height;
            else
                state->max_child_a_size =
                    MIN(state->max_child_b_size, for_height);
        }

        state->min_child_a_size = flow->min_child_height;
        state->min_child_b_size = flow->min_child_width;

        state->get_a_size = rut_sizable_get_preferred_height;
        state->get_b_size = rut_sizable_get_preferred_width;

        state->a_pad = flow->y_padding;
        state->b_pad = flow->x_padding;

        state->line_length = for_height;

        break;
    }

    switch (flow->packing) {
    case RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT:
        state->flow_line = flow_horizontal_line_ltr;
        break;
    case RUT_FLOW_LAYOUT_PACKING_RIGHT_TO_LEFT:
        state->flow_line = flow_horizontal_line_rtl;
        break;
    case RUT_FLOW_LAYOUT_PACKING_TOP_TO_BOTTOM:
        state->flow_line = flow_vertical_line_ttb;
        break;
    case RUT_FLOW_LAYOUT_PACKING_BOTTOM_TO_TOP:
        state->flow_line = flow_vertical_line_btt;
        break;
    }
}

static void
reflow(rut_flow_layout_t *flow, re_flow_state_t *reflow_state, float *length_p)
{
    re_flow_state_t state = *reflow_state;
    rut_flow_layout_child_t *child;
    float a_pos = 0;
    float b_pos = 0;
    c_list_t line_list;
    float line_max_b_size = 0;

    c_list_init(&line_list);

    c_list_for_each(child, &flow->children, link)
    {
        float a_size, b_size;

        /* First we want to know how long the child would prefer to be
         * along the a axis...
         */
        state.get_a_size(child->widget, state.max_child_b_size, NULL, &a_size);

        /* Apply the min/max_child_a_size constraints... */
        a_size = MAX(a_size, state.min_child_a_size);
        if (state.max_child_a_size >= 0)
            a_size = MIN(a_size, state.max_child_a_size);

        /* Check if we need to wrap because we've run out of space for
         * the current line... */

        if (state.line_length >= 0 && !c_list_empty(&line_list) &&
            a_size > (state.line_length - a_pos)) {
            state.flow_line(&line_list, state.line_length, line_max_b_size);

            a_pos = 0;
            b_pos += line_max_b_size + state.b_pad;

            line_max_b_size = 0;
        }

        /* Now find out what size the child would like to be along the b
         * axis, given the constrained a_size we have calculated...
         */
        state.get_b_size(child->widget, a_size, NULL, &b_size);

        /* Apply the min/max_child_b_size constraints... */
        b_size = MAX(b_size, state.min_child_b_size);
        if (state.max_child_b_size >= 0)
            b_size = MIN(b_size, state.max_child_b_size);

        child->a_pos = a_pos;
        child->b_pos = b_pos;
        child->a_size = a_size;

        c_list_insert(&line_list, &child->line_link);

        a_pos += a_size + state.a_pad;
        line_max_b_size = MAX(line_max_b_size, b_size);
    }

    if (!c_list_empty(&line_list)) {
        float line_length =
            state.line_length >= 0 ? state.line_length : a_pos - state.a_pad;

        state.flow_line(&line_list, line_length, line_max_b_size);
        *length_p = b_pos + line_max_b_size;
    } else
        *length_p = 0;

    flow->needs_reflow = false;
    flow->last_flow_line_length = state.line_length;
}

static void
flush_allocations(rut_flow_layout_t *flow)
{
    rut_flow_layout_child_t *child;

    c_list_for_each(child, &flow->children, link)
    {
        rut_transform_init_identity(child->transform);
        rut_transform_translate(
            child->transform, child->flow_x, child->flow_y, 0);
        rut_sizable_set_size(
            child->widget, child->flow_width, child->flow_height);
    }
}

static void
allocate_cb(rut_object_t *graphable, void *user_data)
{
    rut_flow_layout_t *flow = graphable;
    re_flow_state_t state;
    float length_ignore;

    if (flow->n_children == 0)
        return;

    flow->in_allocate = true;

    init_reflow_state(&state, flow, flow->width, flow->height);

    /* Since it's quite likely we will be allocated according to a
     * previous get_preferred_width/height request which will have had
     * to reflow the children we can sometimes avoid needing another
     * reflow here... */
    if (flow->needs_reflow || state.line_length != flow->last_flow_line_length)
        reflow(flow, &state, &length_ignore);

    flush_allocations(flow);

    flow->in_allocate = false;
}

static void
queue_allocation(rut_flow_layout_t *flow)
{
    rut_shell_add_pre_paint_callback(
        flow->shell, flow, allocate_cb, NULL /* user_data */);
}

static void
rut_flow_layout_set_size(void *object, float width, float height)
{
    rut_flow_layout_t *flow = object;

    if (width == flow->width && height == flow->height)
        return;

    flow->width = width;
    flow->height = height;

    queue_allocation(flow);
}

static void
rut_flow_layout_get_size(void *object, float *width, float *height)
{
    rut_flow_layout_t *flow = object;

    *width = flow->width;
    *height = flow->height;
}

static void
rut_flow_layout_get_preferred_height(void *sizable,
                                     float for_width,
                                     float *min_height_p,
                                     float *natural_height_p)
{
    rut_flow_layout_t *flow = sizable;
    re_flow_state_t state;
    float length;

    init_reflow_state(&state, flow, for_width, -1);
    reflow(flow, &state, &length);

    length = floorf(length + 0.5f);

    if (min_height_p)
        *min_height_p = length;
    if (natural_height_p)
        *natural_height_p = length;
}

static void
rut_flow_layout_get_preferred_width(void *sizable,
                                    float for_height,
                                    float *min_width_p,
                                    float *natural_width_p)
{
    rut_flow_layout_t *flow = sizable;
    re_flow_state_t state;
    float length;

    init_reflow_state(&state, flow, -1, for_height);
    reflow(flow, &state, &length);

    if (min_width_p)
        *min_width_p = length;
    if (natural_width_p)
        *natural_width_p = length;
}

static rut_closure_t *
rut_flow_layout_add_preferred_size_callback(
    void *object,
    rut_sizeable_preferred_size_callback_t cb,
    void *user_data,
    rut_closure_destroy_callback_t destroy)
{
    rut_flow_layout_t *flow = object;

    return rut_closure_list_add_FIXME(
        &flow->preferred_size_cb_list, cb, user_data, destroy);
}

rut_type_t rut_flow_layout_type;

static void
_rut_flow_layout_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };
    static rut_sizable_vtable_t sizable_vtable = {
        rut_flow_layout_set_size,
        rut_flow_layout_get_size,
        rut_flow_layout_get_preferred_width,
        rut_flow_layout_get_preferred_height,
        rut_flow_layout_add_preferred_size_callback
    };

    rut_type_t *type = &rut_flow_layout_type;
#define TYPE rut_flow_layout_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_flow_layout_free);
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

rut_flow_layout_t *
rut_flow_layout_new(rut_shell_t *shell,
                    rut_flow_layout_packing_t packing)
{
    rut_flow_layout_t *flow = rut_object_alloc0(
        rut_flow_layout_t, &rut_flow_layout_type, _rut_flow_layout_init_type);

    c_list_init(&flow->preferred_size_cb_list);
    c_list_init(&flow->children);

    rut_graphable_init(flow);

    rut_introspectable_init(
        flow, _rut_flow_layout_prop_specs, flow->properties);

    flow->shell = shell;
    flow->packing = packing;

    flow->x_padding = 0;
    flow->y_padding = 0;

    flow->min_child_width = flow->min_child_height = 0;
    flow->max_child_width = flow->max_child_height = -1;

    flow->needs_reflow = true;
    queue_allocation(flow);

    return flow;
}

static void
preferred_size_changed(rut_flow_layout_t *flow)
{
    flow->needs_reflow = true;

    rut_closure_list_invoke(&flow->preferred_size_cb_list,
                            rut_sizeable_preferred_size_callback_t,
                            flow);
}

static void
child_preferred_size_cb(rut_object_t *sizable, void *user_data)
{
    rut_flow_layout_t *flow = user_data;

    /* The change in preference will be because we just changed the
     * child's size... */
    if (flow->in_allocate)
        return;

    preferred_size_changed(flow);
    queue_allocation(flow);
}

void
rut_flow_layout_add(rut_flow_layout_t *flow, rut_object_t *child_widget)
{
    rut_flow_layout_child_t *child = c_slice_new(rut_flow_layout_child_t);

    child->transform = rut_transform_new(flow->shell);
    rut_graphable_add_child(flow, child->transform);
    rut_object_unref(child->transform);

    child->widget = child_widget;
    rut_graphable_add_child(child->transform, child_widget);
    flow->n_children++;

    child->preferred_size_closure = rut_sizable_add_preferred_size_callback(
        child_widget, child_preferred_size_cb, flow, NULL /* destroy */);

    c_list_insert(flow->children.prev, &child->link);

    preferred_size_changed(flow);
    queue_allocation(flow);
}

void
rut_flow_layout_remove(rut_flow_layout_t *flow, rut_object_t *child_widget)
{
    rut_flow_layout_child_t *child;

    c_return_if_fail(flow->n_children > 0);

    c_list_for_each(child, &flow->children, link)
    {
        if (child->widget == child_widget) {
            rut_flow_layout_remove_child(flow, child);

            preferred_size_changed(flow);
            queue_allocation(flow);
            break;
        }
    }
}

void
rut_flow_layout_set_packing(rut_flow_layout_t *flow,
                            rut_flow_layout_packing_t packing)
{
    if (flow->packing == packing)
        return;

    flow->packing = packing;

    queue_allocation(flow);
    preferred_size_changed(flow);

    rig_property_dirty(&flow->shell->property_ctx,
                       &flow->properties[RUT_FLOW_LAYOUT_PROP_PACKING]);
}

rut_flow_layout_packing_t
rut_flow_layout_get_packing(rut_flow_layout_t *flow)
{
    return flow->packing;
}

void
rut_flow_layout_set_x_padding(rut_flow_layout_t *flow, int padding)
{
    if (flow->x_padding == padding)
        return;

    flow->x_padding = padding;

    queue_allocation(flow);
    preferred_size_changed(flow);

    rig_property_dirty(&flow->shell->property_ctx,
                       &flow->properties[RUT_FLOW_LAYOUT_PROP_X_PADDING]);
}

int
rut_flow_layout_get_x_padding(rut_flow_layout_t *flow)
{
    return flow->x_padding;
}

void
rut_flow_layout_set_y_padding(rut_flow_layout_t *flow, int padding)
{
    if (flow->y_padding == padding)
        return;

    flow->y_padding = padding;

    queue_allocation(flow);
    preferred_size_changed(flow);

    rig_property_dirty(&flow->shell->property_ctx,
                       &flow->properties[RUT_FLOW_LAYOUT_PROP_Y_PADDING]);
}

int
rut_flow_layout_get_y_padding(rut_flow_layout_t *flow)
{
    return flow->y_padding;
}

void
rut_flow_layout_set_min_child_width(rut_flow_layout_t *flow, int min_width)
{
    if (flow->min_child_width == min_width)
        return;

    flow->min_child_width = min_width;

    queue_allocation(flow);
    preferred_size_changed(flow);

    rig_property_dirty(&flow->shell->property_ctx,
                       &flow->properties[RUT_FLOW_LAYOUT_PROP_MIN_CHILD_WIDTH]);
}

int
rut_flow_layout_get_min_child_width(rut_flow_layout_t *flow)
{
    return flow->min_child_width;
}

void
rut_flow_layout_set_max_child_width(rut_flow_layout_t *flow, int max_width)
{
    if (flow->max_child_width == max_width)
        return;

    flow->max_child_width = max_width;

    queue_allocation(flow);
    preferred_size_changed(flow);

    rig_property_dirty(&flow->shell->property_ctx,
                       &flow->properties[RUT_FLOW_LAYOUT_PROP_MAX_CHILD_WIDTH]);
}

int
rut_flow_layout_get_max_child_width(rut_flow_layout_t *flow)
{
    return flow->max_child_width;
}

void
rut_flow_layout_set_min_child_height(rut_flow_layout_t *flow,
                                     int min_height)
{
    if (flow->min_child_height == min_height)
        return;

    flow->min_child_height = min_height;

    queue_allocation(flow);
    preferred_size_changed(flow);

    rig_property_dirty(
        &flow->shell->property_ctx,
        &flow->properties[RUT_FLOW_LAYOUT_PROP_MIN_CHILD_HEIGHT]);
}

int
rut_flow_layout_get_min_child_height(rut_flow_layout_t *flow)
{
    return flow->min_child_height;
}

void
rut_flow_layout_set_max_child_height(rut_flow_layout_t *flow,
                                     int max_height)
{
    if (flow->max_child_height == max_height)
        return;

    flow->max_child_height = max_height;

    queue_allocation(flow);
    preferred_size_changed(flow);

    rig_property_dirty(
        &flow->shell->property_ctx,
        &flow->properties[RUT_FLOW_LAYOUT_PROP_MAX_CHILD_HEIGHT]);
}

int
rut_flow_layout_get_max_child_height(rut_flow_layout_t *flow)
{
    return flow->max_child_height;
}
