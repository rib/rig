/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
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

#include <rut.h>

#include "rig-engine.h"
#include "rig-split-view.h"

/* The width of the area which can be clicked on to change the size of
 * the split view */
#define RIG_SPLIT_VIEW_GRABBER_SIZE 2

enum {
    RIG_SPLIT_VIEW_PROP_WIDTH,
    RIG_SPLIT_VIEW_PROP_HEIGHT,
    RIG_SPLIT_VIEW_N_PROPS
};

struct _rig_split_view_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    rut_graphable_props_t graphable;

    int width;
    int height;

    rig_split_view_split_t split;

    float split_fraction;

    rut_transform_t *child1_transform;

    rut_object_t *child0;
    rut_object_t *child1;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RIG_SPLIT_VIEW_N_PROPS];
};

static rig_property_spec_t _rig_split_view_prop_specs[] = {
    { .name = "width",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_split_view_t, width),
      .setter.float_type = rig_split_view_set_width },
    { .name = "height",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_split_view_t, height),
      .setter.float_type = rig_split_view_set_height },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rig_split_view_free(void *object)
{
    rig_split_view_t *split_view = object;

    rig_split_view_set_child0(split_view, NULL);
    rig_split_view_set_child1(split_view, NULL);

    rut_graphable_remove_child(split_view->child1_transform);
    rut_object_unref(split_view->child1_transform);

    rig_introspectable_destroy(split_view);
    rut_graphable_destroy(split_view);

    rut_object_free(rig_split_view_t, split_view);
}

static void
rig_split_view_get_preferred_width(void *object,
                                   float for_height,
                                   float *min_width_p,
                                   float *natural_width_p)
{
    rig_split_view_t *split_view = object;
    float child0_min = 0;
    float child0_natural = 0;
    float child1_min = 0;
    float child1_natural = 0;

    if (split_view->split == RIG_SPLIT_VIEW_SPLIT_HORIZONTAL) {
        int child0_for_height = for_height * split_view->split_fraction;
        int child1_for_height = for_height - child0_for_height;

        if (split_view->child0) {
            rut_sizable_get_preferred_width(split_view->child0,
                                            child0_for_height,
                                            &child0_min,
                                            &child0_natural);
        }

        if (split_view->child1) {
            rut_sizable_get_preferred_width(split_view->child0,
                                            child1_for_height,
                                            &child1_min,
                                            &child1_natural);
        }

        if (min_width_p)
            *min_width_p = MAX(child0_min, child1_min);
        if (natural_width_p)
            *natural_width_p = MAX(child0_natural, child1_natural);
    } else {
        float ratio0 =
            (1.0 - split_view->split_fraction) / split_view->split_fraction;
        float ratio1 = 1.0 / ratio0;

        if (split_view->child0) {
            rut_sizable_get_preferred_width(
                split_view->child0, for_height, &child0_min, &child0_natural);
        }

        if (split_view->child1) {
            rut_sizable_get_preferred_width(
                split_view->child0, for_height, &child1_min, &child1_natural);
        }

        if (min_width_p) {
            if (child0_min * ratio0 >= child1_min)
                *min_width_p = child0_min + child0_min * ratio0;
            else
                *min_width_p = child1_min + child1_min * ratio1;
        }

        if (natural_width_p) {
            if (child0_natural * ratio0 >= child1_natural)
                *natural_width_p = child0_natural + child0_natural * ratio0;
            else
                *natural_width_p = child1_natural + child1_natural * ratio1;
        }
    }
}

static void
rig_split_view_get_preferred_height(void *object,
                                    float for_width,
                                    float *min_height_p,
                                    float *natural_height_p)
{
    rig_split_view_t *split_view = object;
    float child0_min = 0;
    float child0_natural = 0;
    float child1_min = 0;
    float child1_natural = 0;

    if (split_view->split == RIG_SPLIT_VIEW_SPLIT_VERTICAL) {
        int child0_for_width = for_width * split_view->split_fraction;
        int child1_for_width = for_width - child0_for_width;

        if (split_view->child0) {
            rut_sizable_get_preferred_height(split_view->child0,
                                             child0_for_width,
                                             &child0_min,
                                             &child0_natural);
        }

        if (split_view->child1) {
            rut_sizable_get_preferred_height(split_view->child0,
                                             child1_for_width,
                                             &child1_min,
                                             &child1_natural);
        }

        if (min_height_p)
            *min_height_p = MAX(child0_min, child1_min);
        if (natural_height_p)
            *natural_height_p = MAX(child0_natural, child1_natural);
    } else {
        float ratio0 =
            (1.0 - split_view->split_fraction) / split_view->split_fraction;
        float ratio1 = 1.0 / ratio0;

        if (split_view->child0) {
            rut_sizable_get_preferred_height(
                split_view->child0, for_width, &child0_min, &child0_natural);
        }

        if (split_view->child1) {
            rut_sizable_get_preferred_height(
                split_view->child0, for_width, &child1_min, &child1_natural);
        }

        if (min_height_p) {
            if (child0_min * ratio0 >= child1_min)
                *min_height_p = child0_min + child0_min * ratio0;
            else
                *min_height_p = child1_min + child1_min * ratio1;
        }

        if (natural_height_p) {
            if (child0_natural * ratio0 >= child1_natural)
                *natural_height_p = child0_natural + child0_natural * ratio0;
            else
                *natural_height_p = child1_natural + child1_natural * ratio1;
        }
    }
}

void
rig_split_view_get_size(rut_object_t *object, float *width, float *height)
{
    rig_split_view_t *split_view = object;

    *width = split_view->width;
    *height = split_view->height;
}

static void
allocate_cb(rut_object_t *graphable, void *user_data)
{
    rig_split_view_t *split_view = graphable;
    int width = split_view->width;
    int height = split_view->height;
    int child0_width, child0_height;
    rut_rectangle_int_t geom1;

    geom1.x = geom1.y = 0;
    geom1.width = width;
    geom1.height = height;

    if (split_view->split == RIG_SPLIT_VIEW_SPLIT_VERTICAL) {
        int offset = split_view->split_fraction * split_view->width;
        child0_width = offset;
        child0_height = height;
        geom1.x = child0_width;
        geom1.width = width - geom1.x;
    } else if (split_view->split == RIG_SPLIT_VIEW_SPLIT_HORIZONTAL) {
        int offset = split_view->split_fraction * split_view->height;
        child0_width = width;
        child0_height = offset;
        geom1.y = child0_height;
        geom1.height = height - geom1.y;
    }

    if (split_view->child0)
        rut_sizable_set_size(split_view->child0, child0_width, child0_height);

    if (split_view->child1) {
        rut_transform_init_identity(split_view->child1_transform);

        rut_transform_translate(
            split_view->child1_transform, geom1.x, geom1.y, 0);

        rut_sizable_set_size(split_view->child1, geom1.width, geom1.height);
    }
}

static void
queue_allocation(rig_split_view_t *split_view)
{
    rut_shell_add_pre_paint_callback(
        split_view->shell, split_view, allocate_cb, NULL /* user_data */);
    rut_shell_queue_redraw(split_view->shell);
}

void
rig_split_view_set_size(rut_object_t *object, float width, float height)
{
    rig_split_view_t *split_view = object;

    if (split_view->width == width && split_view->height == height)
        return;

    split_view->width = width;
    split_view->height = height;

    queue_allocation(split_view);

    rig_property_dirty(&split_view->shell->property_ctx,
                       &split_view->properties[RIG_SPLIT_VIEW_PROP_WIDTH]);
    rig_property_dirty(&split_view->shell->property_ctx,
                       &split_view->properties[RIG_SPLIT_VIEW_PROP_HEIGHT]);
}

void
rig_split_view_set_width(rut_object_t *object, float width)
{
    rig_split_view_t *split_view = object;

    rig_split_view_set_size(split_view, width, split_view->height);
}

void
rig_split_view_set_height(rut_object_t *object, float height)
{
    rig_split_view_t *split_view = object;

    rig_split_view_set_size(split_view, split_view->width, height);
}

rut_type_t rig_split_view_type;

static void
_rig_split_view_init_type(void)
{

    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rig_split_view_set_size,            rig_split_view_get_size,
        rig_split_view_get_preferred_width, rig_split_view_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    rut_type_t *type = &rig_split_view_type;
#define TYPE rig_split_view_t

    rut_type_init(
        &rig_split_view_type, C_STRINGIFY(TYPE), _rig_split_view_free);
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

rig_split_view_t *
rig_split_view_new(rig_engine_t *engine,
                   rig_split_view_split_t split,
                   float width,
                   float height)
{
    rut_shell_t *shell = engine->shell;
    rig_split_view_t *split_view = rut_object_alloc0(
        rig_split_view_t, &rig_split_view_type, _rig_split_view_init_type);

    rig_introspectable_init(
        split_view, _rig_split_view_prop_specs, split_view->properties);

    rut_graphable_init(split_view);

    split_view->shell = shell;

    split_view->width = width;
    split_view->height = height;

    split_view->split = split;

    split_view->child1_transform = rut_transform_new(shell);
    rut_graphable_add_child(split_view, split_view->child1_transform);

    queue_allocation(split_view);

    return split_view;
}

void
rig_split_view_set_split_fraction(rig_split_view_t *split_view,
                                  float fraction)
{
    c_return_if_fail(fraction != 0);

    split_view->split_fraction = fraction;

    queue_allocation(split_view);
}

void
rig_split_view_set_child0(rig_split_view_t *split_view,
                          rut_object_t *child0)
{
    if (split_view->child0 == child0)
        return;

    if (split_view->child0) {
        rut_graphable_remove_child(split_view->child0);
        rut_object_unref(split_view->child0);
    }

    if (child0) {
        rut_graphable_add_child(split_view, child0);
        rut_object_ref(child0);
    }

    split_view->child0 = child0;

    queue_allocation(split_view);
}

void
rig_split_view_set_child1(rig_split_view_t *split_view,
                          rut_object_t *child1)
{
    if (split_view->child1 == child1)
        return;

    if (split_view->child1) {
        rut_graphable_remove_child(split_view->child1);
        rut_object_unref(split_view->child1);
    }

    if (child1) {
        rut_graphable_add_child(split_view->child1_transform, child1);
        rut_object_ref(child1);
    }

    split_view->child1 = child1;

    queue_allocation(split_view);
}

#warning "need to handle add_preferred_size_callback"
