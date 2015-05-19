/*
 * Rig
 *
 * UI Engine & Editor
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

#include <rut.h>

#include "rig-engine.h"
#include "rig-view.h"
#include "rig-renderer.h"
#include "rig-dof-effect.h"

#include "components/rig-camera.h"
#include "components/rig-material.h"

typedef void (*entity_translate_callback_t)(rig_entity_t *entity,
                                            float start[3],
                                            float rel[3],
                                            void *user_data);

typedef void (*entity_translate_done_callback_t)(rig_entity_t *entity,
                                                 bool moved,
                                                 float start[3],
                                                 float rel[3],
                                                 void *user_data);

static void
_rig_camera_view_free(void *object)
{
    rig_camera_view_t *view = object;

    rig_camera_view_set_ui(view, NULL);

    rut_shell_remove_pre_paint_callback_by_graphable(view->shell,
                                                     view);

    rut_graphable_destroy(view);

    rut_object_free(rig_camera_view_t, view);
}

static void
init_camera_from_camera(rig_entity_t *dst_camera, rig_entity_t *src_camera)
{
    rut_object_t *dst_camera_comp =
        rig_entity_get_component(dst_camera, RUT_COMPONENT_TYPE_CAMERA);
    rut_object_t *src_camera_comp =
        rig_entity_get_component(src_camera, RUT_COMPONENT_TYPE_CAMERA);

    rut_projection_t mode = rut_camera_get_projection_mode(src_camera_comp);


    rut_camera_set_projection_mode(dst_camera_comp, mode);
    if (mode == RUT_PROJECTION_PERSPECTIVE) {
        rut_camera_set_field_of_view(dst_camera_comp,
                                     rut_camera_get_field_of_view(src_camera_comp));
        rut_camera_set_near_plane(dst_camera_comp,
                                  rut_camera_get_near_plane(src_camera_comp));
        rut_camera_set_far_plane(dst_camera_comp,
                                 rut_camera_get_far_plane(src_camera_comp));
    } else {
        float x1, y1, x2, y2;

        rut_camera_get_orthographic_coordinates(src_camera_comp,
                                                &x1, &y1, &x2, &y2);
        rut_camera_set_orthographic_coordinates(dst_camera_comp,
                                                x1, y1, x2, y2);
    }

    rut_camera_set_zoom(dst_camera_comp,
                        rut_camera_get_zoom(src_camera_comp));

    rig_entity_set_position(dst_camera, rig_entity_get_position(src_camera));
    rig_entity_set_scale(dst_camera, rig_entity_get_scale(src_camera));
    rig_entity_set_rotation(dst_camera, rig_entity_get_rotation(src_camera));
}

void
rig_camera_view_paint(rig_camera_view_t *view,
                      rut_object_t *renderer)
{
    cg_framebuffer_t *fb = view->fb;
    rig_entity_t *camera;
    rut_object_t *camera_component;
    rig_paint_context_t rig_paint_ctx;
    rut_paint_context_t *rut_paint_ctx = &rig_paint_ctx._parent;
    int i;

    if (view->ui == NULL)
        return;

    if (view->play_camera == NULL)
        return;

    view->width = cg_framebuffer_get_width(fb);
    view->height = cg_framebuffer_get_height(fb);

    camera = view->play_camera;
    camera_component = view->play_camera_component;

    rut_paint_ctx->camera = camera_component;

    rig_paint_ctx.engine = view->engine;
    rig_paint_ctx.renderer = renderer;

    rig_paint_ctx.pass = RIG_PASS_COLOR_BLENDED;
    rig_paint_ctx.enable_dof = view->enable_dof;
    rig_paint_ctx.enable_dof = false;

    rut_camera_set_framebuffer(camera_component, fb);
    rut_camera_set_viewport(camera_component,
                            view->fb_x, view->fb_y,
                            view->width, view->height);
    rig_entity_set_camera_view_from_transform(camera);

    cg_framebuffer_clear4f(fb,
                           CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH |
                           CG_BUFFER_BIT_STENCIL,
                           0, 0, 0, 1);

    rig_renderer_paint_camera(&rig_paint_ctx, camera);
}

rut_type_t rig_camera_view_type;

static void
_rig_camera_view_init_type(void)
{

    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    rut_type_t *type = &rig_camera_view_type;
#define TYPE rig_camera_view_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_camera_view_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
#undef TYPE
}

rig_camera_view_t *
rig_camera_view_new(rig_frontend_t *frontend)
{
    rig_camera_view_t *view = rut_object_alloc0(
        rig_camera_view_t, &rig_camera_view_type, _rig_camera_view_init_type);

    view->frontend = frontend;
    view->engine = frontend->engine;
    view->shell = view->engine->shell;

    rut_graphable_init(view);

    //rut_graphable_add_child(view, view->input_region);

    return view;
}

void
rig_camera_view_set_framebuffer(rig_camera_view_t *view,
                                cg_framebuffer_t *fb)
{
    if (view->fb == fb)
        return;

    if (view->fb) {
        cg_object_unref(view->fb);
        view->fb = NULL;
    }

    if (fb)
        view->fb = cg_object_ref(fb);
}

void
set_play_camera(rig_camera_view_t *view, rig_entity_t *play_camera)
{
    if (view->play_camera == play_camera)
        return;

    if (view->play_camera) {
        rut_object_unref(view->play_camera);
        view->play_camera = NULL;
        rut_object_unref(view->play_camera_component);
        view->play_camera_component = NULL;
    }

    if (play_camera) {
        view->play_camera = rut_object_ref(play_camera);
        view->play_camera_component =
            rig_entity_get_component(play_camera, RUT_COMPONENT_TYPE_CAMERA);
        rut_object_ref(view->play_camera_component);
    }
}

void
rig_camera_view_set_ui(rig_camera_view_t *view, rig_ui_t *ui)
{
    if (view->ui == ui)
        return;

    if (view->ui)
        set_play_camera(view, NULL);

    /* XXX: to avoid having a circular reference we don't take a
     * reference on the ui... */
    view->ui = ui;

    if (ui) {
        set_play_camera(view, ui->play_camera);

        view->origin[0] = 0;
        view->origin[1] = 0;
        view->origin[2] = 0;
    }

    rut_shell_queue_redraw(view->shell);
}
