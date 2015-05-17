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

    rut_object_unref(view->view_camera);
    rut_object_unref(view->view_camera_component);

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

    view->width = cg_framebuffer_get_width(fb);
    view->height = cg_framebuffer_get_height(fb);

#ifdef RIG_EDITOR_ENABLED
    if (!view->play_mode) {
        camera = view->view_camera;
        camera_component = view->view_camera_component;
    } else
#endif /* RIG_EDITOR_ENABLED */
    {
        if (view->play_camera == NULL)
            return;

        camera = view->play_camera;
        camera_component = view->play_camera_component;
    }

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

static void
transform_ray(cg_matrix_t *transform,
              bool inverse_transform,
              float ray_origin[3],
              float ray_direction[3])
{
    cg_matrix_t inverse, normal_matrix, *m;

    m = transform;
    if (inverse_transform) {
        cg_matrix_get_inverse(transform, &inverse);
        m = &inverse;
    }

    cg_matrix_transform_points(m,
                               3, /* num components for input */
                               sizeof(float) * 3, /* input stride */
                               ray_origin,
                               sizeof(float) * 3, /* output stride */
                               ray_origin,
                               1 /* n_points */);

    cg_matrix_get_inverse(m, &normal_matrix);
    cg_matrix_transpose(&normal_matrix);

    rut_util_transform_normal(&normal_matrix,
                              &ray_direction[0],
                              &ray_direction[1],
                              &ray_direction[2]);
}

typedef struct _pick_context_t {
    rig_camera_view_t *view;
    rig_engine_t *engine;
    rut_object_t *view_camera;
    rut_matrix_stack_t *matrix_stack;
    float x;
    float y;
    float *ray_origin;
    float *ray_direction;
    rig_entity_t *selected_entity;
    float selected_distance;
    int selected_index;
} pick_context_t;

static rut_traverse_visit_flags_t
entitygraph_pre_pick_cb(rut_object_t *object, int depth, void *user_data)
{
    pick_context_t *pick_ctx = user_data;

    if (rut_object_is(object, RUT_TRAIT_ID_TRANSFORMABLE)) {
        const cg_matrix_t *matrix = rut_transformable_get_matrix(object);
        rut_matrix_stack_push(pick_ctx->matrix_stack);
        rut_matrix_stack_multiply(pick_ctx->matrix_stack, matrix);
    }

    if (rut_object_get_type(object) == &rig_entity_type) {
        rig_entity_t *entity = object;
        rig_material_t *material;
        rut_component_t *geometry;
        rut_mesh_t *mesh;
        int index;
        float distance;
        bool hit;
        float transformed_ray_origin[3];
        float transformed_ray_direction[3];
        cg_matrix_t transform;
        rut_object_t *input;

        input = rig_entity_get_component(entity, RUT_COMPONENT_TYPE_INPUT);

        if (input) {
            if (rut_object_is(input, RUT_TRAIT_ID_PICKABLE)) {
                rut_matrix_stack_get(pick_ctx->matrix_stack, &transform);

                if (rut_pickable_pick(input,
                                      pick_ctx->view_camera,
                                      &transform,
                                      pick_ctx->x,
                                      pick_ctx->y)) {
                    pick_ctx->selected_entity = entity;
                    return RUT_TRAVERSE_VISIT_BREAK;
                } else
                    return RUT_TRAVERSE_VISIT_CONTINUE;
            } else {
                geometry = rig_entity_get_component(
                    entity, RUT_COMPONENT_TYPE_GEOMETRY);
            }
        } else {
            if (!pick_ctx->view->play_mode) {
                material = rig_entity_get_component(
                    entity, RUT_COMPONENT_TYPE_MATERIAL);
                if (!material || !rig_material_get_visible(material))
                    return RUT_TRAVERSE_VISIT_CONTINUE;
            }

            geometry =
                rig_entity_get_component(entity, RUT_COMPONENT_TYPE_GEOMETRY);
        }

        /* Get a model we can pick against */
        if (!(geometry && rut_object_is(geometry, RUT_TRAIT_ID_MESHABLE) &&
              (mesh = rut_meshable_get_mesh(geometry))))
            return RUT_TRAVERSE_VISIT_CONTINUE;

        /* transform the ray into the model space */
        memcpy(transformed_ray_origin, pick_ctx->ray_origin, 3 * sizeof(float));
        memcpy(transformed_ray_direction,
               pick_ctx->ray_direction,
               3 * sizeof(float));

        rut_matrix_stack_get(pick_ctx->matrix_stack, &transform);

        transform_ray(&transform,
                      true, /* inverse of the transform */
                      transformed_ray_origin,
                      transformed_ray_direction);

#if 0
        c_debug ("transformed ray %f,%f,%f %f,%f,%f\n",
                 transformed_ray_origin[0],
                 transformed_ray_origin[1],
                 transformed_ray_origin[2],
                 transformed_ray_direction[0],
                 transformed_ray_direction[1],
                 transformed_ray_direction[2]);
#endif

        /* intersect the transformed ray with the model engine */
        hit = rut_util_intersect_mesh(mesh,
                                      transformed_ray_origin,
                                      transformed_ray_direction,
                                      &index,
                                      &distance);

        if (hit) {
            const cg_matrix_t *view =
                rut_camera_get_view_transform(pick_ctx->view_camera);
            float w = 1;

            /* to compare intersection distances we find the actual point of ray
             * intersection in model coordinates and transform that into eye
             * coordinates */

            transformed_ray_direction[0] *= distance;
            transformed_ray_direction[1] *= distance;
            transformed_ray_direction[2] *= distance;

            transformed_ray_direction[0] += transformed_ray_origin[0];
            transformed_ray_direction[1] += transformed_ray_origin[1];
            transformed_ray_direction[2] += transformed_ray_origin[2];

            cg_matrix_transform_point(&transform,
                                      &transformed_ray_direction[0],
                                      &transformed_ray_direction[1],
                                      &transformed_ray_direction[2],
                                      &w);
            cg_matrix_transform_point(view,
                                      &transformed_ray_direction[0],
                                      &transformed_ray_direction[1],
                                      &transformed_ray_direction[2],
                                      &w);
            distance = transformed_ray_direction[2];

            if (distance > pick_ctx->selected_distance) {
                pick_ctx->selected_entity = entity;
                pick_ctx->selected_distance = distance;
                pick_ctx->selected_index = index;
            }
        }
    }

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

static rut_traverse_visit_flags_t
entitygraph_post_pick_cb(rut_object_t *object, int depth, void *user_data)
{
    if (rut_object_is(object, RUT_TRAIT_ID_TRANSFORMABLE)) {
        pick_context_t *pick_ctx = user_data;
        rut_matrix_stack_pop(pick_ctx->matrix_stack);
    }

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

static rig_entity_t *
pick(rig_camera_view_t *view,
     rut_object_t *view_camera,
     float x,
     float y,
     float ray_origin[3],
     float ray_direction[3])
{
    rig_engine_t *engine = view->engine;
    pick_context_t pick_ctx;

    pick_ctx.view = view;
    pick_ctx.engine = engine;
    pick_ctx.view_camera = view_camera;
    pick_ctx.matrix_stack = view->matrix_stack;
    pick_ctx.x = x;
    pick_ctx.y = y;
    pick_ctx.selected_distance = -FLT_MAX;
    pick_ctx.selected_entity = NULL;
    pick_ctx.ray_origin = ray_origin;
    pick_ctx.ray_direction = ray_direction;

    rut_graphable_traverse(view->ui->scene,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           entitygraph_pre_pick_cb,
                           entitygraph_post_pick_cb,
                           &pick_ctx);

#if 0
    if (pick_ctx.selected_entity)
    {
        c_message("Hit entity, triangle #%d, distance %.2f",
                  pick_ctx.selected_index, pick_ctx.selected_distance);
    }
#endif

    return pick_ctx.selected_entity;
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

    view->matrix_stack = rut_matrix_stack_new(view->shell);

    view->view_camera = rig_entity_new(view->shell);
    rig_entity_set_label(view->view_camera, "rig:camera");

    view->view_camera_component = rig_camera_new(view->engine,
                                                 -1, /* ortho/vp width */
                                                 -1, /* ortho/vp height */
                                                 NULL);
    rut_camera_set_clear(view->view_camera_component, false);
    rig_entity_add_component(view->view_camera, view->view_camera_component);

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

    if (view->ui) {
        set_play_camera(view, NULL);
        rut_graphable_remove_child(view->view_camera);
        rut_shell_remove_input_camera(
            view->shell, view->view_camera_component, view->ui->scene);
    }

    /* XXX: to avoid having a circular reference we don't take a
     * reference on the ui... */
    view->ui = ui;

    if (ui) {
        rut_shell_add_input_camera(
            view->shell, view->view_camera_component, ui->scene);
        set_play_camera(view, ui->play_camera);
        rut_graphable_add_child(ui->scene, view->view_camera);
        init_camera_from_camera(view->view_camera, view->play_camera);

        view->origin[0] = 0;//engine->device_width / 2;
        view->origin[1] = 0;//engine->device_height / 2;
        view->origin[2] = 0;
    }

    rut_shell_queue_redraw(view->shell);
}
