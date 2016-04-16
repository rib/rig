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

#include <rut-config.h>

#include <clib.h>

#include <cglib/cglib.h>

#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-camera.h"
#include "rut-rectangle.h"

struct _rut_rectangle_t {
    rut_object_base_t _base;

    float width;
    float height;

    rut_graphable_props_t graphable;
    rut_paintable_props_t paintable;

    cg_pipeline_t *pipeline;
};

static void
_rut_rectangle_free(void *object)
{
    rut_rectangle_t *rectangle = object;

    cg_object_unref(rectangle->pipeline);

    rut_graphable_destroy(rectangle);

    rut_object_free(rut_rectangle_t, object);
}

static void
_rut_rectangle_paint(rut_object_t *object,
                     rut_paint_context_t *paint_ctx)
{
    rut_rectangle_t *rectangle = object;
    rut_object_t *camera = paint_ctx->camera;

    cg_framebuffer_draw_rectangle(rut_camera_get_framebuffer(camera),
                                  rectangle->pipeline,
                                  0,
                                  0,
                                  rectangle->width,
                                  rectangle->height);
}

rut_type_t rut_rectangle_type;

static void
_rut_rectangle_init_type(void)
{

    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    static rut_paintable_vtable_t paintable_vtable = { _rut_rectangle_paint };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_rectangle_set_size,
        rut_rectangle_get_size,
        rut_simple_sizable_get_preferred_width,
        rut_simple_sizable_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    rut_type_t *type = &rut_rectangle_type;
#define TYPE rut_rectangle_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_rectangle_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PAINTABLE,
                       offsetof(TYPE, paintable),
                       &paintable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);

#undef TYPE
}

rut_rectangle_t *
rut_rectangle_new4f(rut_shell_t *shell,
                    float width,
                    float height,
                    float red,
                    float green,
                    float blue,
                    float alpha)
{
    rut_rectangle_t *rectangle = rut_object_alloc0(
        rut_rectangle_t, &rut_rectangle_type, _rut_rectangle_init_type);

    rut_graphable_init(rectangle);
    rut_paintable_init(rectangle);

    rectangle->width = width;
    rectangle->height = height;

    rectangle->pipeline = cg_pipeline_new(shell->cg_device);
    cg_pipeline_set_color4f(rectangle->pipeline, red, green, blue, alpha);

    return rectangle;
}

void
rut_rectangle_set_width(rut_rectangle_t *rectangle, float width)
{
    rectangle->width = width;
}

void
rut_rectangle_set_height(rut_rectangle_t *rectangle, float height)
{
    rectangle->height = height;
}

void
rut_rectangle_set_size(rut_object_t *self, float width, float height)
{
    rut_rectangle_t *rectangle = self;
    rectangle->width = width;
    rectangle->height = height;
}

void
rut_rectangle_get_size(rut_object_t *self, float *width, float *height)
{
    rut_rectangle_t *rectangle = self;
    *width = rectangle->width;
    *height = rectangle->height;
}
