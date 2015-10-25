/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#include <cglib/cglib.h>

#include "rut-input-region.h"
#include "rut-util.h"
#include "rut-inputable.h"
#include "rut-pickable.h"
#include "rut-camera.h"

typedef enum _rut_input_shape_type_t {
    RUT_INPUT_SHAPE_TYPE_RECTANGLE,
    RUT_INPUT_SHAPE_TYPE_CIRCLE
} rut_input_shape_type_t;

typedef struct _rut_input_shape_rectangle_t {
    rut_input_shape_type_t type;
    float x0, y0, x1, y1;
} rut_input_shape_rectange_t;

typedef struct _rut_input_shape_circle_t {
    rut_input_shape_type_t type;
    float x, y;
    float r;
    float r_squared;
} rut_input_shape_circle_t;

typedef struct _rut_input_shape_any_t {
    rut_input_shape_type_t type;
} rut_input_shape_any_t;

typedef union _rut_input_shape_t {
    rut_input_shape_any_t any;
    rut_input_shape_rectange_t rectangle;
    rut_input_shape_circle_t circle;
} rut_input_shape_t;

struct _rut_input_region_t {
    rut_object_base_t _base;

    rut_input_shape_t shape;

    rut_graphable_props_t graphable;

    rut_input_region_callback_t callback;
    void *user_data;
};

static void
_rut_input_region_free(void *object)
{
    rut_input_region_t *region = object;

    rut_graphable_destroy(region);

    rut_object_free(rut_input_region_t, region);
}

static void
poly_init_from_rectangle(float *poly,
                         rut_input_shape_rectange_t *rectangle)
{
    poly[0] = rectangle->x0;
    poly[1] = rectangle->y0;
    poly[2] = 0;
    poly[3] = 1;

    poly[4] = rectangle->x0;
    poly[5] = rectangle->y1;
    poly[6] = 0;
    poly[7] = 1;

    poly[8] = rectangle->x1;
    poly[9] = rectangle->y1;
    poly[10] = 0;
    poly[11] = 1;

    poly[12] = rectangle->x1;
    poly[13] = rectangle->y0;
    poly[14] = 0;
    poly[15] = 1;
}
/* Given an (x0,y0) (x1,y1) rectangle this transforms it into
 * a polygon in window coordinates that can be intersected
 * with input coordinates for picking.
 */
static void
rect_to_screen_polygon(rut_input_shape_rectange_t *rectangle,
                       const c_matrix_t *modelview,
                       const c_matrix_t *projection,
                       const float *viewport,
                       float *poly)
{
    poly_init_from_rectangle(poly, rectangle);

    rut_util_fully_transform_points(modelview, projection, viewport, poly, 4);
}

static bool
_rut_input_region_pick(rut_object_t *inputable,
                       rut_object_t *camera,
                       const c_matrix_t *graphable_modelview,
                       float x,
                       float y)
{
    rut_input_region_t *region = inputable;
    c_matrix_t matrix;
    const c_matrix_t *modelview = NULL;
    float poly[16];
    const c_matrix_t *view = rut_camera_get_view_transform(camera);

    if (graphable_modelview)
        modelview = graphable_modelview;
    else {
        matrix = *view;
        rut_graphable_apply_transform(inputable, &matrix);
        modelview = &matrix;
    }

    switch (region->shape.any.type) {
    case RUT_INPUT_SHAPE_TYPE_RECTANGLE: {
        const c_matrix_t *projection = rut_camera_get_projection(camera);
        const float *viewport = rut_camera_get_viewport(camera);
        rect_to_screen_polygon(&region->shape.rectangle,
                               modelview,
                               projection,
                               viewport,
                               poly);

#if 0
        c_debug ("transformed input region\n");
        for (i = 0; i < 4; i++)
        {
            float *p = poly + 4 * i;
            c_debug ("poly[].x=%f\n", p[0]);
            c_debug ("poly[].y=%f\n", p[1]);
        }
        c_debug ("x=%f y=%f\n", x, y);
#endif
        if (rut_util_point_in_screen_poly(x, y, poly, sizeof(float) * 4, 4))
            return true;
        else
            return false;
    }
    case RUT_INPUT_SHAPE_TYPE_CIRCLE: {
        rut_input_shape_circle_t *circle = &region->shape.circle;
        float center_x = circle->x;
        float center_y = circle->y;
        float z = 0;
        float w = 1;
        float a;
        float b;
        float c2;

        /* Note the circle hit regions are billboarded, such that only the
         * center point is transformed but the raius of the circle stays
         * constant. */

        c_matrix_transform_point(modelview, &center_x, &center_y, &z, &w);

        a = x - center_x;
        b = y - center_y;
        c2 = a * a + b * b;

        if (c2 < circle->r_squared)
            return true;
        else
            return false;
    }
    }

    c_warn_if_reached();
    return false;
}

static rut_input_event_status_t
_rut_input_region_handle_event(rut_object_t *inputable,
                               rut_input_event_t *event)
{
    rut_input_region_t *region = inputable;

    return region->callback(region, event, region->user_data);
}

static void
_rut_input_region_set_size(rut_object_t *self, float width, float height)
{
    rut_input_region_t *region = self;

    switch (region->shape.any.type) {
    case RUT_INPUT_SHAPE_TYPE_RECTANGLE:
        region->shape.rectangle.x1 = region->shape.rectangle.x0 + width;
        region->shape.rectangle.y1 = region->shape.rectangle.y0 + height;
        break;
    case RUT_INPUT_SHAPE_TYPE_CIRCLE:
        region->shape.circle.r = MAX(width, height) / 2.0f;
        region->shape.circle.r_squared =
            region->shape.circle.r * region->shape.circle.r;
        break;
    }
}

static void
_rut_input_region_get_size(rut_object_t *self, float *width, float *height)
{
    rut_input_region_t *region = self;

    switch (region->shape.any.type) {
    case RUT_INPUT_SHAPE_TYPE_RECTANGLE:
        *width = region->shape.rectangle.x1 - region->shape.rectangle.x0;
        *height = region->shape.rectangle.y1 - region->shape.rectangle.y0;
        break;
    case RUT_INPUT_SHAPE_TYPE_CIRCLE:
        *width = *height = region->shape.circle.r * 2;
        break;
    }
}

rut_type_t rut_input_region_type;

static void
_rut_input_region_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    static rut_sizable_vtable_t sizable_vtable = {
        _rut_input_region_set_size,
        _rut_input_region_get_size,
        rut_simple_sizable_get_preferred_width,
        rut_simple_sizable_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    static rut_pickable_vtable_t pickable_vtable = {
        .pick = _rut_input_region_pick,
    };

    static rut_inputable_vtable_t inputable_vtable = {
        .handle_event = _rut_input_region_handle_event
    };

    rut_type_t *type = &rut_input_region_type;
#define TYPE rut_input_region_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_input_region_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PICKABLE,
                       0, /* no implied properties */
                       &pickable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INPUTABLE,
                       0, /* no implied properties */
                       &inputable_vtable);

#undef TYPE
}

static rut_input_region_t *
_rut_input_region_new_common(rut_input_region_callback_t callback,
                             void *user_data)
{
    rut_input_region_t *region = rut_object_alloc0(rut_input_region_t,
                                                   &rut_input_region_type,
                                                   _rut_input_region_init_type);

    rut_graphable_init(region);

    region->callback = callback;
    region->user_data = user_data;

    return region;
}

rut_input_region_t *
rut_input_region_new_rectangle(float x0,
                               float y0,
                               float x1,
                               float y1,
                               rut_input_region_callback_t callback,
                               void *user_data)
{
    rut_input_region_t *region =
        _rut_input_region_new_common(callback, user_data);

    rut_input_region_set_rectangle(region, x0, y0, x1, y1);

    return region;
}

rut_input_region_t *
rut_input_region_new_circle(float x0,
                            float y0,
                            float radius,
                            rut_input_region_callback_t callback,
                            void *user_data)
{
    rut_input_region_t *region =
        _rut_input_region_new_common(callback, user_data);

    rut_input_region_set_circle(region, x0, y0, radius);

    return region;
}

void
rut_input_region_set_rectangle(
    rut_input_region_t *region, float x0, float y0, float x1, float y1)
{
    region->shape.any.type = RUT_INPUT_SHAPE_TYPE_RECTANGLE;
    region->shape.rectangle.x0 = x0;
    region->shape.rectangle.y0 = y0;
    region->shape.rectangle.x1 = x1;
    region->shape.rectangle.y1 = y1;
}

void
rut_input_region_set_circle(rut_input_region_t *region,
                            float x,
                            float y,
                            float radius)
{
    region->shape.any.type = RUT_INPUT_SHAPE_TYPE_CIRCLE;
    region->shape.circle.x = x;
    region->shape.circle.y = y;
    region->shape.circle.r = radius;
    region->shape.circle.r_squared = radius * radius;
}

