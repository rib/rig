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

#ifdef RIG_EDITOR_ENABLED
#include "rig-selection-tool.h"
#include "rig-rotation-tool.h"
#endif

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

struct _entity_translate_grab_closure_t {
    rig_camera_view_t *view;

    /* pointer position at start of grab */
    float grab_x;
    float grab_y;

    /* entity position at start of grab */
    float entity_grab_pos[3];
    rig_entity_t *entity;

    /* set as soon as a move event is encountered so that we can detect
     * situations where a grab is started but nothing actually moves */
    bool moved;

    float x_vec[3];
    float y_vec[3];

    entity_translate_callback_t entity_translate_cb;
    entity_translate_done_callback_t entity_translate_done_cb;
    void *user_data;
};

#ifdef RIG_EDITOR_ENABLED
struct _entities_translate_grab_closure_t {
    rig_camera_view_t *view;
    c_llist_t *entity_closures;
};

static void
update_grab_closure_vectors(entity_translate_grab_closure_t *closure);
#endif

static void
_rig_camera_view_free(void *object)
{
    rig_camera_view_t *view = object;

    rig_camera_view_set_ui(view, NULL);

    rut_shell_remove_pre_paint_callback_by_graphable(view->shell,
                                                     view);

    rut_object_unref(view->shell);

    rut_graphable_destroy(view);

    rut_object_unref(view->view_camera);
    rut_object_unref(view->view_camera_component);

#ifdef RIG_EDITOR_ENABLED
    if (view->selection_tool)
        rig_selection_tool_destroy(view->selection_tool);
    if (view->rotation_tool)
        rig_rotation_tool_destroy(view->rotation_tool);
#endif

    rut_object_free(rig_camera_view_t, view);
}

static void
paint_overlays(rig_camera_view_t *view,
               rut_paint_context_t *paint_ctx)
{
    rig_engine_t *engine = view->engine;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(paint_ctx->camera);
    bool need_camera_flush = false;
    bool draw_pick_ray = false;
    bool draw_tools = false;
    rut_object_t *suspended_camera = paint_ctx->camera;

    if (view->debug_pick_ray && view->picking_ray) {
        draw_pick_ray = true;
        need_camera_flush = true;
    }

    if (!view->play_mode) {
        draw_tools = true;
        need_camera_flush = true;
    }

    if (need_camera_flush) {
        suspended_camera = paint_ctx->camera;
        rut_camera_suspend(suspended_camera);

        rut_camera_flush(view->view_camera_component);
    }

/* Use this to visualize the depth-of-field alpha buffer... */
#if 0
    cg_pipeline_t *pipeline = cg_pipeline_new (engine->shell->cg_device);
    cg_pipeline_set_layer_texture (pipeline, 0, view->dof.depth_pass);
    cg_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);
    cg_framebuffer_draw_rectangle (fb,
                                   pipeline,
                                   0, 0,
                                   200, 200);
#endif

/* Use this to visualize the shadow_map */
#if 0
    cg_pipeline_t *pipeline = cg_pipeline_new (engine->shell->cg_device);
    cg_pipeline_set_layer_texture (pipeline, 0, engine->shadow_map);
    //cg_pipeline_set_layer_texture (pipeline, 0, engine->shadow_color);
    cg_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);
    cg_framebuffer_draw_rectangle (fb,
                                   pipeline,
                                   0, 0,
                                   200, 200);
#endif

    if (draw_pick_ray) {
        cg_primitive_draw(view->picking_ray, fb, view->picking_ray_color);
    }

#ifdef RIG_EDITOR_ENABLED
    if (draw_tools) {
        rig_editor_t *editor = rig_engine_get_editor(engine);
        cg_primitive_t *grid_prim = rig_editor_get_grid_prim(editor);

        if (grid_prim)
            rut_util_draw_jittered_primitive3f(fb, grid_prim, 0.5, 0.5, 0.5);

        switch (view->tool_id) {
        case RIG_TOOL_ID_SELECTION:
            rig_selection_tool_update(view->selection_tool, suspended_camera);
            break;
        case RIG_TOOL_ID_ROTATION:
            rig_rotation_tool_draw(view->rotation_tool, fb);
            break;
        }
    }
#endif /* RIG_EDITOR_ENABLED */

    if (need_camera_flush) {
        rut_camera_end_frame(view->view_camera_component);
        rut_camera_resume(suspended_camera);
    }
}

static void
update_camera_viewport(rig_camera_view_t *view,
                       rut_object_t *window_camera,
                       rut_object_t *camera)
{
    float x, y, z;

    x = y = z = 0;
    rut_graphable_fully_transform_point(view, window_camera, &x, &y, &z);

    x = c_nearbyint(x);
    y = c_nearbyint(y);

    /* XXX: if the viewport width/height get changed during allocation
     * then we should probably use a dirty flag so we can defer
     * the viewport update to here. */

    if (camera != view->view_camera_component)
        rut_camera_set_viewport(camera, x, y, view->width, view->height);
    else if (view->last_viewport_x != x || view->last_viewport_y != y ||
             view->dirty_viewport_size) {
        rut_camera_set_viewport(camera, x, y, view->width, view->height);

        view->last_viewport_x = x;
        view->last_viewport_y = y;
        view->dirty_viewport_size = false;
    }
}

static void
initialize_view_camera(rig_camera_view_t *view)
{
    rig_entity_t *view_camera = view->view_camera;
    rut_object_t *view_camera_comp = view->view_camera_component;
    rig_entity_t *play_camera = view->play_camera;
    rut_object_t *play_camera_comp = view->play_camera_component;

    rut_projection_t mode = rut_camera_get_projection_mode(play_camera_comp);


    rut_camera_set_projection_mode(view_camera_comp, mode);
    if (mode == RUT_PROJECTION_PERSPECTIVE) {
        rut_camera_set_field_of_view(view_camera_comp,
                                     rut_camera_get_field_of_view(play_camera_comp));
        rut_camera_set_near_plane(view_camera_comp,
                                  rut_camera_get_near_plane(play_camera_comp));
        rut_camera_set_far_plane(view_camera_comp,
                                 rut_camera_get_far_plane(play_camera_comp));
    } else {
        float x1, y1, x2, y2;

        rut_camera_get_orthographic_coordinates(play_camera_comp,
                                                &x1, &y1, &x2, &y2);
        rut_camera_set_orthographic_coordinates(view_camera_comp,
                                                x1, y1, x2, y2);
    }

    rut_camera_set_zoom(view_camera_comp,
                        rut_camera_get_zoom(play_camera_comp));

    rig_entity_set_position(view_camera, rig_entity_get_position(play_camera));
    rig_entity_set_scale(view_camera, rig_entity_get_scale(play_camera));
    rig_entity_set_rotation(view_camera, rig_entity_get_rotation(play_camera));

    rut_shell_queue_redraw(view->shell);
}

static void
_rut_camera_view_paint(rut_object_t *object,
                       rut_paint_context_t *paint_ctx)
{
    rig_camera_view_t *view = object;
    rig_engine_t *engine = view->engine;
    rut_object_t *suspended_camera = paint_ctx->camera;
    rig_paint_context_t *rig_paint_ctx = (rig_paint_context_t *)paint_ctx;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(paint_ctx->camera);
    rig_entity_t *camera;
    rut_object_t *camera_component;

    if (view->ui == NULL)
        return;

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

    if (engine->frontend && engine->frontend_id == RIG_FRONTEND_ID_EDITOR) {
        cg_framebuffer_draw_rectangle(
            fb, view->bg_pipeline, 0, 0, view->width, view->height);
    }

    rut_camera_suspend(suspended_camera);
    paint_ctx->camera = camera_component;

    rut_camera_set_framebuffer(camera_component, fb);
    update_camera_viewport(view, engine->camera_2d, camera_component);
    rig_entity_set_camera_view_from_transform(camera);

    rig_paint_ctx->enable_dof = view->enable_dof;
    rig_renderer_paint_camera(rig_paint_ctx, camera);

    rut_camera_resume(suspended_camera);
    paint_ctx->camera = suspended_camera;

    paint_overlays(view, paint_ctx);
}

static void
allocate_cb(rut_object_t *graphable, void *user_data)
{
    rig_camera_view_t *view = graphable;
    rig_engine_t *engine = view->engine;

    if (view->input_region) {
        rut_input_region_set_rectangle(
            view->input_region, 0, 0, view->width, view->height);
    }

#ifdef RIG_EDITOR_ENABLED
    if (engine->frontend && engine->frontend_id == RIG_FRONTEND_ID_EDITOR) {
        if (view->entities_translate_grab_closure) {
            c_llist_t *l;

            update_camera_viewport(view, engine->camera_2d,
                                   view->view_camera_component);

            rig_entity_set_camera_view_from_transform(view->view_camera);

            for (l = view->entities_translate_grab_closure->entity_closures; l;
                 l = l->next)
                update_grab_closure_vectors(l->data);
        }
    }
#endif /* RIG_EDITOR_ENABLED */
}

static void
queue_allocation(rig_camera_view_t *view)
{
    rut_shell_add_pre_paint_callback(
        view->shell, view, allocate_cb, NULL /* user_data */);
}

static void
rig_camera_view_set_size(void *object, float width, float height)
{
    rig_camera_view_t *view = object;
    rig_engine_t *engine = view->engine;

    if (width == view->width && height == view->height)
        return;

    view->width = width;
    view->height = height;

    view->dirty_viewport_size = true;

    if (engine->frontend) {
        engine->frontend->has_resized = true;
        engine->frontend->pending_width = width;
        engine->frontend->pending_height = height;
    }

    queue_allocation(view);
}

static void
rig_camera_view_get_preferred_width(void *sizable,
                                    float for_height,
                                    float *min_width_p,
                                    float *natural_width_p)
{
    rig_camera_view_t *view = sizable;
    rig_engine_t *engine = view->engine;

    if (min_width_p)
        *min_width_p = 0;
    if (natural_width_p)
        *natural_width_p = MAX(engine->device_width, engine->device_height);
}

static void
rig_camera_view_get_preferred_height(void *sizable,
                                     float for_width,
                                     float *min_height_p,
                                     float *natural_height_p)
{
    rig_camera_view_t *view = sizable;
    rig_engine_t *engine = view->engine;

    if (min_height_p)
        *min_height_p = 0;
    if (natural_height_p)
        *natural_height_p = MAX(engine->device_width, engine->device_height);
}

static void
rig_camera_view_get_size(void *object, float *width, float *height)
{
    rig_camera_view_t *view = object;

    *width = view->width;
    *height = view->height;
}

rut_type_t rig_camera_view_type;

static void
_rig_camera_view_init_type(void)
{

    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child removed */
                                                       NULL, /* child addded */
                                                       NULL /* parent changed */
    };

    static rut_paintable_vtable_t paintable_vtable = { _rut_camera_view_paint };

    static rut_sizable_vtable_t sizable_vtable = {
        rig_camera_view_set_size,
        rig_camera_view_get_size,
        rig_camera_view_get_preferred_width,
        rig_camera_view_get_preferred_height,
        NULL
    };

    rut_type_t *type = &rig_camera_view_type;
#define TYPE rig_camera_view_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_camera_view_free);
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

#ifdef RIG_EDITOR_ENABLED
static void
scene_translate_done_cb(rig_entity_t *entity,
                        bool moved,
                        float start[3],
                        float rel[3],
                        void *user_data)
{
    rig_camera_view_t *view = user_data;
    float pos[3] = {
        start[0] - rel[0],
        start[1] - rel[1],
        start[2] - rel[2]
    };

    view->origin[0] -= rel[0];
    view->origin[1] -= rel[1];
    view->origin[2] -= rel[2];

    /* If the entity hasn't actually moved then we'll ignore it. It
     * that case the user is presumably just trying to select the
     * entity and we don't want it to modify the controller
     */

    rig_entity_set_position(view->view_camera, pos);
}

static void
scene_translate_cb(rig_entity_t *entity,
                   float start[3],
                   float rel[3],
                   void *user_data)
{
    rig_camera_view_t *view = user_data;
    float pos[3] = {
        start[0] - rel[0],
        start[1] - rel[1],
        start[2] - rel[2]
    };

    rig_entity_set_position(view->view_camera, pos);
}

static void
entity_translate_done_cb(rig_entity_t *entity,
                         bool moved,
                         float start[3],
                         float rel[3],
                         void *user_data)
{
    rig_camera_view_t *view = user_data;
    rig_engine_t *engine = view->engine;
    rig_editor_t *editor = rig_engine_get_editor(engine);
    rig_controller_view_t *controller_view =
        rig_editor_get_controller_view(editor);

    /* If the entity hasn't actually moved then we'll ignore it. It that
     * case the user is presumably just trying to select the entity and we
     * don't want it to modify the controller */
    if (moved) {
        rut_property_t *position_prop =
            &entity->properties[RUT_ENTITY_PROP_POSITION];
        rut_boxed_t boxed_position;

        /* Reset the entities position, before logging the move in the
         * journal... */
        rig_entity_set_translate(entity, start[0], start[1], start[2]);

        boxed_position.type = RUT_PROPERTY_TYPE_VEC3;
        boxed_position.d.vec3_val[0] = start[0] + rel[0];
        boxed_position.d.vec3_val[1] = start[1] + rel[1];
        boxed_position.d.vec3_val[2] = start[2] + rel[2];

        rig_controller_view_edit_property(controller_view,
                                          false, /* mergable */
                                          position_prop,
                                          &boxed_position);

        rig_reload_position_inspector(editor, entity);

        rut_shell_queue_redraw(engine->shell);
    }
}

static void
entity_translate_cb(rig_entity_t *entity,
                    float start[3],
                    float rel[3],
                    void *user_data)
{
    rig_camera_view_t *view = user_data;
    rig_engine_t *engine = view->engine;
    rig_editor_t *editor = rig_engine_get_editor(engine);

    rig_entity_set_translate(
        entity, start[0] + rel[0], start[1] + rel[1], start[2] + rel[2]);

    rig_reload_position_inspector(editor, entity);

    rut_shell_queue_redraw(engine->shell);
}

static void
handle_entity_translate_grab_motion(rut_input_event_t *event,
                                    entity_translate_grab_closure_t *closure)
{
    rig_entity_t *entity = closure->entity;
    float x = rut_motion_event_get_x(event);
    float y = rut_motion_event_get_y(event);
    float move_x, move_y;
    float rel[3];
    float *x_vec = closure->x_vec;
    float *y_vec = closure->y_vec;

    move_x = x - closure->grab_x;
    move_y = y - closure->grab_y;

    rel[0] = x_vec[0] * move_x;
    rel[1] = x_vec[1] * move_x;
    rel[2] = x_vec[2] * move_x;

    rel[0] += y_vec[0] * move_y;
    rel[1] += y_vec[1] * move_y;
    rel[2] += y_vec[2] * move_y;

    if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
        if (closure->entity_translate_done_cb)
            closure->entity_translate_done_cb(entity,
                                              closure->moved,
                                              closure->entity_grab_pos,
                                              rel,
                                              closure->user_data);

        c_slice_free(entity_translate_grab_closure_t, closure);
    } else if (rut_motion_event_get_action(event) ==
               RUT_MOTION_EVENT_ACTION_MOVE) {
        closure->moved = true;

        closure->entity_translate_cb(
            entity, closure->entity_grab_pos, rel, closure->user_data);
    }
}

static rut_input_event_status_t
entities_translate_grab_input_cb(rut_input_event_t *event, void *user_data)

{
    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        entities_translate_grab_closure_t *closure = user_data;
        c_llist_t *l;

        for (l = closure->entity_closures; l; l = l->next)
            handle_entity_translate_grab_motion(event, l->data);

        if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
            rig_engine_t *engine = closure->view->engine;

            rut_shell_ungrab_input(engine->shell,
                                   entities_translate_grab_input_cb,
                                   user_data);
            closure->view->entities_translate_grab_closure = NULL;

            /* XXX: handle_entity_translate_grab_motion() will free the
             * entity-closures themselves on ACTION_UP so we just need
             * to free the list here. */
            c_llist_free(closure->entity_closures);
            c_slice_free(entities_translate_grab_closure_t, closure);
        }

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
unproject_window_coord(rut_object_t *camera,
                       const cg_matrix_t *modelview,
                       const cg_matrix_t *inverse_modelview,
                       float object_coord_z,
                       float *x,
                       float *y)
{
    const cg_matrix_t *projection = rut_camera_get_projection(camera);
    const cg_matrix_t *inverse_projection =
        rut_camera_get_inverse_projection(camera);
    // float z;
    float ndc_x, ndc_y, ndc_z, ndc_w;
    float eye_x, eye_y, eye_z, eye_w;
    const float *viewport = rut_camera_get_viewport(camera);

    /* Convert object coord z into NDC z */
    {
        float tmp_x, tmp_y, tmp_z;
        const cg_matrix_t *m = modelview;
        float z, w;

        tmp_x = m->xz * object_coord_z + m->xw;
        tmp_y = m->yz * object_coord_z + m->yw;
        tmp_z = m->zz * object_coord_z + m->zw;

        m = projection;
        z = m->zx * tmp_x + m->zy * tmp_y + m->zz * tmp_z + m->zw;
        w = m->wx * tmp_x + m->wy * tmp_y + m->wz * tmp_z + m->ww;

        ndc_z = z / w;
    }

    /* Undo the Viewport transform, putting us in Normalized Device Coords */
    ndc_x = (*x - viewport[0]) * 2.0f / viewport[2] - 1.0f;
    ndc_y = ((viewport[3] - 1 + viewport[1] - *y) * 2.0f / viewport[3] - 1.0f);

    /* Undo the Projection, putting us in Eye Coords. */
    ndc_w = 1;
    cg_matrix_transform_point(
        inverse_projection, &ndc_x, &ndc_y, &ndc_z, &ndc_w);
    eye_x = ndc_x / ndc_w;
    eye_y = ndc_y / ndc_w;
    eye_z = ndc_z / ndc_w;
    eye_w = 1;

    /* Undo the Modelview transform, putting us in Object Coords */
    cg_matrix_transform_point(
        inverse_modelview, &eye_x, &eye_y, &eye_z, &eye_w);

    *x = eye_x;
    *y = eye_y;
    //*z = eye_z;
}

static void
update_grab_closure_vectors(entity_translate_grab_closure_t *closure)
{
    rig_entity_t *parent = rut_graphable_get_parent(closure->entity);
    rig_camera_view_t *view = closure->view;
    rut_object_t *camera = view->view_camera_component;
    rig_engine_t *engine = view->engine;
    cg_matrix_t parent_transform;
    cg_matrix_t inverse_transform;
    float origin[3] = { 0, 0, 0 };
    float unit_x[3] = { 1, 0, 0 };
    float unit_y[3] = { 0, 1, 0 };
    float x_vec[3];
    float y_vec[3];
    float entity_x, entity_y, entity_z;
    float w;

    rut_graphable_get_modelview(parent, camera, &parent_transform);

    if (!cg_matrix_get_inverse(&parent_transform, &inverse_transform)) {
        memset(closure->x_vec, 0, sizeof(float) * 3);
        memset(closure->y_vec, 0, sizeof(float) * 3);
        c_warning("Failed to get inverse transform of entity");
        return;
    }

    /* Find the z of our selected entity in eye coordinates */
    entity_x = 0;
    entity_y = 0;
    entity_z = 0;
    w = 1;
    cg_matrix_transform_point(
        &parent_transform, &entity_x, &entity_y, &entity_z, &w);

    // c_debug ("Entity origin in eye coords: %f %f %f\n", entity_x, entity_y,
    // entity_z);

    /* Convert unit x and y vectors in screen coordinate
     * into points in eye coordinates with the same z depth
     * as our selected entity */

    unproject_window_coord(camera,
                           &engine->identity,
                           &engine->identity,
                           entity_z,
                           &origin[0],
                           &origin[1]);
    origin[2] = entity_z;
    // c_debug ("eye origin: %f %f %f\n", origin[0], origin[1], origin[2]);

    unproject_window_coord(camera,
                           &engine->identity,
                           &engine->identity,
                           entity_z,
                           &unit_x[0],
                           &unit_x[1]);
    unit_x[2] = entity_z;
    // c_debug ("eye unit_x: %f %f %f\n", unit_x[0], unit_x[1], unit_x[2]);

    unproject_window_coord(camera,
                           &engine->identity,
                           &engine->identity,
                           entity_z,
                           &unit_y[0],
                           &unit_y[1]);
    unit_y[2] = entity_z;
    // c_debug ("eye unit_y: %f %f %f\n", unit_y[0], unit_y[1], unit_y[2]);

    /* Transform our points from eye coordinates into entity
     * coordinates and convert into input mapping vectors */

    w = 1;
    cg_matrix_transform_point(
        &inverse_transform, &origin[0], &origin[1], &origin[2], &w);
    w = 1;
    cg_matrix_transform_point(
        &inverse_transform, &unit_x[0], &unit_x[1], &unit_x[2], &w);
    w = 1;
    cg_matrix_transform_point(
        &inverse_transform, &unit_y[0], &unit_y[1], &unit_y[2], &w);

    x_vec[0] = unit_x[0] - origin[0];
    x_vec[1] = unit_x[1] - origin[1];
    x_vec[2] = unit_x[2] - origin[2];

    // c_debug (" =========================== Entity coords: x_vec = %f, %f,
    // %f\n",
    //         x_vec[0], x_vec[1], x_vec[2]);

    y_vec[0] = unit_y[0] - origin[0];
    y_vec[1] = unit_y[1] - origin[1];
    y_vec[2] = unit_y[2] - origin[2];

    // c_debug (" =========================== Entity coords: y_vec = %f, %f,
    // %f\n",
    //         y_vec[0], y_vec[1], y_vec[2]);

    memcpy(closure->x_vec, x_vec, sizeof(float) * 3);
    memcpy(closure->y_vec, y_vec, sizeof(float) * 3);
}

static entity_translate_grab_closure_t *
translate_grab_entity(rig_camera_view_t *view,
                      rig_entity_t *entity,
                      float grab_x,
                      float grab_y,
                      entity_translate_callback_t translate_cb,
                      entity_translate_done_callback_t done_cb,
                      void *user_data)
{
    entity_translate_grab_closure_t *closure;
    rig_entity_t *parent = rut_graphable_get_parent(entity);

    if (!parent)
        return NULL;

    closure = c_slice_new(entity_translate_grab_closure_t);
    closure->view = view;
    closure->grab_x = grab_x;
    closure->grab_y = grab_y;

    memcpy(closure->entity_grab_pos,
           rig_entity_get_position(entity),
           sizeof(float) * 3);

    closure->entity = entity;
    closure->entity_translate_cb = translate_cb;
    closure->entity_translate_done_cb = done_cb;
    closure->moved = false;
    closure->user_data = user_data;

    update_grab_closure_vectors(closure);

    return closure;
}

static bool
translate_grab_entities(rig_camera_view_t *view,
                        c_llist_t *entities,
                        float grab_x,
                        float grab_y,
                        entity_translate_callback_t translate_cb,
                        entity_translate_done_callback_t done_cb,
                        void *user_data)
{
    rut_object_t *camera = view->view_camera_component;
    entities_translate_grab_closure_t *closure;
    c_llist_t *l;

    if (view->entities_translate_grab_closure)
        return false;

    closure = c_slice_new(entities_translate_grab_closure_t);
    closure->view = view;
    closure->entity_closures = NULL;

    for (l = entities; l; l = l->next) {
        entity_translate_grab_closure_t *entity_closure = translate_grab_entity(
            view, l->data, grab_x, grab_y, translate_cb, done_cb, user_data);
        if (entity_closure)
            closure->entity_closures =
                c_llist_prepend(closure->entity_closures, entity_closure);
    }

    if (!closure->entity_closures) {
        c_slice_free(entities_translate_grab_closure_t, closure);
        return false;
    }

    rut_shell_grab_input(view->engine->shell,
                         camera,
                         entities_translate_grab_input_cb,
                         closure);

    view->entities_translate_grab_closure = closure;

    return true;
}

#if 0
static void
print_quaternion (const cg_quaternion_t *q,
                  const char *label)
{
    float angle = cg_quaternion_get_rotation_angle (q);
    float axis[3];
    cg_quaternion_get_rotation_axis (q, axis);
    c_debug ("%s: [%f (%f, %f, %f)]\n", label, angle, axis[0], axis[1], axis[2]);
}
#endif

static cg_primitive_t *
create_line_primitive(rig_engine_t *engine, float a[3], float b[3])
{
    cg_vertex_p3_t data[2];
    cg_attribute_buffer_t *attribute_buffer;
    cg_attribute_t *attributes[1];
    cg_primitive_t *primitive;

    data[0].x = a[0];
    data[0].y = a[1];
    data[0].z = a[2];
    data[1].x = b[0];
    data[1].y = b[1];
    data[1].z = b[2];

    attribute_buffer = cg_attribute_buffer_new(engine->shell->cg_device,
                                               2 * sizeof(cg_vertex_p3_t),
                                               data);

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(cg_vertex_p3_t),
                                     offsetof(cg_vertex_p3_t, x),
                                     3,
                                     CG_ATTRIBUTE_TYPE_FLOAT);

    primitive = cg_primitive_new_with_attributes(
        CG_VERTICES_MODE_LINES, 2, attributes, 1);

    cg_object_unref(attribute_buffer);
    cg_object_unref(attributes[0]);

    return primitive;
}

static cg_primitive_t *
create_picking_ray(rig_engine_t *engine,
                   float ray_position[3],
                   float ray_direction[3],
                   float length)
{
    cg_primitive_t *line;
    float points[6];

    points[0] = ray_position[0];
    points[1] = ray_position[1];
    points[2] = ray_position[2];
    points[3] = ray_position[0] + length * ray_direction[0];
    points[4] = ray_position[1] + length * ray_direction[1];
    points[5] = ray_position[2] + length * ray_direction[2];

    line = create_line_primitive(engine, points, points + 3);

    return line;
}
#endif /* RIG_EDITOR_ENABLED */

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

#ifdef RIG_EDITOR_ENABLED
static void
move_entity_to_camera(rig_camera_view_t *view, rig_entity_t *entity)
{
    rig_engine_t *engine = view->engine;
    rig_undo_journal_t *sub_journal;
    const float *camera_position;
    rut_property_t *position_prop =
        &entity->properties[RUT_ENTITY_PROP_POSITION];
    rut_boxed_t boxed_position;
    const cg_quaternion_t *camera_rotation;
    rut_property_t *rotation_property =
        &entity->properties[RUT_ENTITY_PROP_ROTATION];
    rut_boxed_t boxed_rotation;
    rig_editor_t *editor = rig_engine_get_editor(engine);
    rig_controller_view_t *controller_view =
        rig_editor_get_controller_view(editor);

    camera_position = rig_entity_get_position(view->view_camera);

    boxed_position.type = RUT_PROPERTY_TYPE_VEC3;
    boxed_position.d.vec3_val[0] = camera_position[0];
    boxed_position.d.vec3_val[1] = camera_position[1];
    boxed_position.d.vec3_val[2] = camera_position[2];

    rig_controller_view_edit_property(controller_view,
                                      false, /* mergable */
                                      position_prop,
                                      &boxed_position);

    camera_rotation = rig_entity_get_rotation(view->view_camera);

    boxed_rotation.type = RUT_PROPERTY_TYPE_QUATERNION;
    boxed_rotation.d.quaternion_val = *camera_rotation;

    rig_controller_view_edit_property(controller_view,
                                      false, /* mergable */
                                      rotation_property,
                                      &boxed_rotation);

    sub_journal = rig_editor_pop_undo_subjournal(editor);
    rig_undo_journal_log_subjournal(engine->undo_journal, sub_journal);
}
#endif /* RIG_EDITOR_ENABLED */

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

void
rig_camera_view_set_play_mode_enabled(rig_camera_view_t *view,
                                      bool enabled)
{
    view->play_mode = enabled;

    if (enabled) {
        /* depth of field effect */
        view->enable_dof = true;
    } else {
        view->enable_dof = false;
    }
}

struct scene_rot_grab_state {
    rut_object_t *camera;
    rig_camera_view_t *view;
    cg_matrix_t transform;
    cg_matrix_t inverse_transform;

    rut_arcball_t arcball;
    cg_quaternion_t saved_camera_rotation;
    float saved_camera_pos[3];
};

static rut_input_event_status_t
scene_rotate_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    struct scene_rot_grab_state *state = user_data;
    rig_camera_view_t *view = state->view;
    rut_shell_onscreen_t *onscreen = event->onscreen;
    rut_shell_t *shell = onscreen->shell;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        if (rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
            rut_shell_ungrab_input(shell, scene_rotate_grab_input_cb, user_data);
            c_slice_free(struct scene_rot_grab_state, state);
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else if (rut_motion_event_get_action(event) ==
                   RUT_MOTION_EVENT_ACTION_MOVE) {
            float x = rut_motion_event_get_x(event);
            float y = rut_motion_event_get_y(event);
            rut_object_t *camera = state->camera;
            cg_quaternion_t new_rotation;
            cg_matrix_t rot_matrix;
            float pos[3];
            float w = 1;

            rut_camera_unproject_coord(camera,
                                       &state->transform,
                                       &state->inverse_transform,
                                       0,
                                       &x,
                                       &y);

            rut_arcball_mouse_motion(&state->arcball, view->width - x, y);

            pos[0] = state->saved_camera_pos[0] - view->origin[0];
            pos[1] = state->saved_camera_pos[1] - view->origin[1];
            pos[2] = state->saved_camera_pos[2] - view->origin[2];

            cg_matrix_init_from_quaternion(&rot_matrix, &state->arcball.q_drag);

            cg_matrix_transform_point(&rot_matrix, &pos[0], &pos[1], &pos[2], &w);

            pos[0] += view->origin[0];
            pos[1] += view->origin[1];
            pos[2] += view->origin[2];

            rig_entity_set_position(view->view_camera, pos);

            cg_quaternion_multiply(&new_rotation,
                                   &state->arcball.q_drag,
                                   &state->saved_camera_rotation
                                   );
            rig_entity_set_rotation(view->view_camera, &new_rotation);

            rut_shell_queue_redraw(shell);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

#ifdef RIG_EDITOR_ENABLED
static rut_input_event_status_t
input_cb(rut_input_event_t *event,
         void *user_data)
{
    rig_camera_view_t *view = user_data;
    rig_engine_t *engine = view->engine;

    /* It's possible that no UI has been loaded yet and to avoid various
     * NULL pointer dereferencing risks we can bail immediately... */
    if (!view->ui)
        return RUT_INPUT_EVENT_STATUS_UNHANDLED;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        rut_motion_event_action_t action = rut_motion_event_get_action(event);
        rut_modifier_state_t modifiers =
            rut_motion_event_get_modifier_state(event);
        float x = rut_motion_event_get_x(event);
        float y = rut_motion_event_get_y(event);
        rut_button_state_t state;
        float ray_position[3], ray_direction[3], screen_pos[2];
        const float *viewport;
        const cg_matrix_t *inverse_projection;
        // cg_matrix_t *camera_transform;
        const cg_matrix_t *camera_view;
        cg_matrix_t camera_transform;
        rut_object_t *picked_entity;
        rig_entity_t *camera;
        rut_object_t *camera_component;

        /* XXX: Simplify since this api is now never used in play mode */
        if (!view->play_mode) {
            camera = view->view_camera;
            camera_component = view->view_camera_component;
        } else {
            camera = view->play_camera;
            camera_component = view->play_camera_component;
        }

        update_camera_viewport(view, engine->camera_2d, camera_component);
        rig_entity_set_camera_view_from_transform(camera);

        state = rut_motion_event_get_button_state(event);

        viewport = rut_camera_get_viewport(camera_component);
        inverse_projection =
            rut_camera_get_inverse_projection(camera_component);

// c_debug ("Camera inverse projection: %p\n", engine->simulator);
// cg_debug_matrix_print (inverse_projection);

#if 0
        camera_transform = rig_entity_get_transform (camera);
#else
        camera_view = rut_camera_get_view_transform(camera_component);
        cg_matrix_get_inverse(camera_view, &camera_transform);
#endif
        // c_debug ("Camera transform:\n");
        // cg_debug_matrix_print (&camera_transform);

        /* With the editor, the camera view may be offset within a
         * window... */
        rut_camera_transform_window_coordinate(camera_component, &x, &y);

        screen_pos[0] = x;
        screen_pos[1] = y;
        // c_debug ("screen pos x=%f, y=%f\n", x, y);

        rut_util_create_pick_ray(viewport,
                                 inverse_projection,
                                 &camera_transform,
                                 screen_pos,
                                 ray_position,
                                 ray_direction);

#if 0
        c_debug ("ray pos %f,%f,%f dir %f,%f,%f\n",
                 ray_position[0],
                 ray_position[1],
                 ray_position[2],
                 ray_direction[0],
                 ray_direction[1],
                 ray_direction[2]);
#endif

        if (view->debug_pick_ray) {
            float z_near = rut_camera_get_near_plane(camera);
            float z_far = rut_camera_get_far_plane(camera);
            float x1 = 0, y1 = 0, z1 = z_near, w1 = 1;
            float x2 = 0, y2 = 0, z2 = z_far, w2 = 1;
            float len;

            if (view->picking_ray)
                cg_object_unref(view->picking_ray);

            /* FIXME: This is a hack, we should intersect the ray with
             * the far plane to decide how long the debug primitive
             * should be */
            cg_matrix_transform_point(&camera_transform, &x1, &y1, &z1, &w1);
            cg_matrix_transform_point(&camera_transform, &x2, &y2, &z2, &w2);
            len = z2 - z1;

            view->picking_ray =
                create_picking_ray(engine, ray_position, ray_direction, len);
        }

        picked_entity =
            pick(view, camera_component, x, y, ray_position, ray_direction);

#if 0
        if (picked_entity)
        {
            rut_property_t *label =
                rut_introspectable_lookup_property (picked_entity, "label");

            c_debug ("Entity picked: %s\n", rut_property_get_text (label));
        }
#endif

        if (view->play_mode) {
            if (picked_entity) {
                rut_object_t *inputable = rig_entity_get_component(
                    picked_entity, RUT_COMPONENT_TYPE_INPUT);
                // rut_property_t *label =
                //  rut_introspectable_lookup_property (picked_entity, "label");
                // c_debug ("Entity picked: %s\n", rut_property_get_text
                // (label));

                if (inputable)
                    return rut_inputable_handle_event(inputable, event);
                else
                    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
            } else {
                // c_debug ("No entity picked\n");
                return RUT_INPUT_EVENT_STATUS_UNHANDLED;
            }
        } else if (action == RUT_MOTION_EVENT_ACTION_DOWN &&
                   state == RUT_BUTTON_STATE_1) {
            rig_editor_t *editor = rig_engine_get_editor(engine);
            rig_objects_selection_t *selection =
                rig_editor_get_objects_selection(editor);

            if ((rut_motion_event_get_modifier_state(event) &
                 RUT_MODIFIER_SHIFT_ON)) {
                rig_select_object(
                    editor, picked_entity, RUT_SELECT_ACTION_TOGGLE);
            } else
                rig_select_object(
                    editor, picked_entity, RUT_SELECT_ACTION_REPLACE);

            /* If we have selected an entity then initiate a grab so the
             * entity can be moved with the mouse...
             */
            if (selection->objects) {
                if (!translate_grab_entities(view,
                                             selection->objects,
                                             rut_motion_event_get_x(event),
                                             rut_motion_event_get_y(event),
                                             entity_translate_cb,
                                             entity_translate_done_cb,
                                             view))
                    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
            }

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else if (action == RUT_MOTION_EVENT_ACTION_DOWN &&
                   state == RUT_BUTTON_STATE_2 &&
                   (!(modifiers & RUT_MODIFIER_SHIFT_ON))) {
            struct scene_rot_grab_state *state = c_slice_new(struct scene_rot_grab_state);
            float view_x = rut_motion_event_get_x(event);
            float view_y = rut_motion_event_get_y(event);

            state->view = view;
            state->camera = rut_input_event_get_camera(event);
            state->transform = *rut_camera_get_view_transform(state->camera);
            rut_graphable_apply_transform(view, &state->transform);
            if (!cg_matrix_get_inverse(&state->transform,
                                       &state->inverse_transform)) {
                c_warning("Failed to calculate inverse of camera view transform\n");
                c_slice_free(struct rot_grab_grab_state, state);
                return RUT_INPUT_EVENT_STATUS_UNHANDLED;
            }

            rut_camera_unproject_coord(state->camera,
                                       &state->transform,
                                       &state->inverse_transform,
                                       0,
                                       &view_x,
                                       &view_y);

            rut_arcball_init(&state->arcball,
                             view->width / 2, view->height / 2,
                             sqrtf(view->width * view->width +
                                   view->height * view->height) / 2);
            rut_arcball_mouse_down(&state->arcball, view->width - view_x, view_y);

            state->saved_camera_rotation =
                *rig_entity_get_rotation(view->view_camera);

            memcpy(state->saved_camera_pos,
                   rig_entity_get_position(view->view_camera),
                   sizeof(state->saved_camera_pos));

            //cg_quaternion_init_identity(&state->arcball.q_drag);


            // rut_arcball_mouse_down (&view->arcball, engine->width - x, y);
            // c_debug ("Arcball init, mouse = (%d, %d)\n", (int)(engine->width
            // - x), (int)(engine->height - y));

            // print_quaternion (&view->arcball.q_drag, "Arcball Initial
            // Quaternion");
            // engine->button_down = true;

            // memcpy (view->saved_origin, view->origin, sizeof (view->origin));

            rut_shell_grab_input(engine->shell, state->camera,
                                 scene_rotate_grab_input_cb, state);

            return RUT_INPUT_EVENT_STATUS_HANDLED;
        } else if (action == RUT_MOTION_EVENT_ACTION_MOVE &&
                   state == RUT_BUTTON_STATE_2 &&
                   modifiers & RUT_MODIFIER_SHIFT_ON) {
            c_llist_t link;

            link.data = view->view_camera;
            link.next = NULL;

            if (!translate_grab_entities(view,
                                         &link,
                                         rut_motion_event_get_x(event),
                                         rut_motion_event_get_y(event),
                                         scene_translate_cb,
                                         scene_translate_done_cb,
                                         view))
                return RUT_INPUT_EVENT_STATUS_UNHANDLED;
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }

    } else if (engine->frontend_id == RIG_FRONTEND_ID_EDITOR) {
        if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_KEY &&
            rut_key_event_get_action(event) == RUT_KEY_EVENT_ACTION_DOWN) {
            switch (rut_key_event_get_keysym(event)) {
            case RUT_KEY_minus: {
                float zoom = rut_camera_get_zoom(view->view_camera_component);

                zoom *= 0.8;

                rut_camera_set_zoom(view->view_camera_component, zoom);

                rut_shell_queue_redraw(engine->shell);

                break;
            }
            case RUT_KEY_equal: {
                float zoom = rut_camera_get_zoom(view->view_camera_component);

                if (zoom)
                    zoom *= 1.2;
                else
                    zoom = 0.1;

                rut_camera_set_zoom(view->view_camera_component, zoom);

                rut_shell_queue_redraw(engine->shell);

                break;
            }
            case RUT_KEY_j:
                if ((rut_key_event_get_modifier_state(event) &
                     RUT_MODIFIER_CTRL_ON)) {
                    rig_editor_t *editor = rig_engine_get_editor(engine);
                    rig_objects_selection_t *selection =
                        rig_editor_get_objects_selection(editor);

                    if (selection->objects) {
                        c_llist_t *l;
                        for (l = selection->objects; l; l = l->next)
                            move_entity_to_camera(view, l->data);
                    }
                }
                break;
            case RUT_KEY_0:
                initialize_view_camera(view);
                break;
            }
        } else if (rut_input_event_get_type(event) ==
                   RUT_INPUT_EVENT_TYPE_DROP) {
            rut_object_t *data = rut_drop_event_get_data(event);

            if (data &&
                rut_object_get_type(data) == &rig_objects_selection_type) {
                rig_objects_selection_t *selection = data;
                int n_entities = c_llist_length(selection->objects);

                if (n_entities) {
                    rig_entity_t *parent = (rig_entity_t *)view->ui->scene;
                    c_llist_t *l;

                    for (l = selection->objects; l; l = l->next) {
                        rig_undo_journal_add_entity(
                            engine->undo_journal, parent, l->data);
                    }
                }
            }
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}
#endif /* RIG_EDITOR_ENABLED */

static rut_input_event_status_t
device_mode_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    rig_camera_view_t *view = user_data;
    rig_engine_t *engine = view->engine;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        rut_motion_event_action_t action = rut_motion_event_get_action(event);

        switch (action) {
        case RUT_MOTION_EVENT_ACTION_UP:
            rut_shell_ungrab_input(
                engine->shell, device_mode_grab_input_cb, user_data);
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        case RUT_MOTION_EVENT_ACTION_MOVE: {
            float x = rut_motion_event_get_x(event);
            float dx = x - engine->grab_x;
            float progression = dx / engine->device_width;

            rig_controller_set_progress(view->ui->controllers->data,
                                        engine->grab_progress + progression);

            rut_shell_queue_redraw(engine->shell);
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
        default:
            return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
device_mode_input_cb(rut_input_event_t *event,
                     void *user_data)
{
    rig_camera_view_t *view = user_data;
    rig_engine_t *engine = view->engine;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION) {
        rut_motion_event_action_t action = rut_motion_event_get_action(event);
        rut_button_state_t state = rut_motion_event_get_button_state(event);

        if (action == RUT_MOTION_EVENT_ACTION_DOWN &&
            state == RUT_BUTTON_STATE_1) {
            engine->grab_x = rut_motion_event_get_x(event);
            engine->grab_y = rut_motion_event_get_y(event);
            engine->grab_progress =
                rig_controller_get_progress(view->ui->controllers->data);

            /* TODO: Add rut_shell_implicit_grab_input() that handles releasing
             * the grab for you */
            rut_shell_grab_input(engine->shell,
                                 rut_input_event_get_camera(event),
                                 device_mode_grab_input_cb,
                                 view);
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

    return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static rut_input_event_status_t
simulator_implicit_grab_input_cb(rut_input_event_t *event, void *user_data)
{
    rig_camera_view_t *view = user_data;
    rig_engine_t *engine = view->engine;

    if (rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_MOTION &&
        rut_motion_event_get_action(event) == RUT_MOTION_EVENT_ACTION_UP) {
        rut_shell_ungrab_input(
            view->shell, simulator_implicit_grab_input_cb, view);
    }

    rut_input_queue_append(engine->simulator_input_queue, event);

    return RUT_INPUT_EVENT_STATUS_HANDLED;
}

static rut_input_event_status_t
input_region_cb(rut_input_region_t *region,
                rut_input_event_t *event,
                void *user_data)
{
    rig_camera_view_t *view = user_data;
    rig_engine_t *engine = view->engine;

    /* XXX: it could be nice if the way we forwarded events to the
     * simulator was the same for the editor as for device mode,
     * though it would also seem unnecessary to have any indirection
     * for events in device mode where we currently just assume
     * all events need to be forwarded to the simulator.
     */
    if (engine->frontend) {
        rig_camera_view_t *view = user_data;
        rig_engine_t *engine = view->engine;

        if (engine->frontend_id == RIG_FRONTEND_ID_EDITOR &&
            rut_input_event_get_type(event) == RUT_INPUT_EVENT_TYPE_KEY &&
            rut_key_event_get_action(event) == RUT_KEY_EVENT_ACTION_UP &&
            rut_key_event_get_keysym(event) == RUT_KEY_p) {
            rig_frontend_queue_set_play_mode_enabled(engine->frontend,
                                                     !engine->play_mode);
            return RUT_INPUT_EVENT_STATUS_HANDLED;
        }

        if (view->play_mode) {
            if (rut_input_event_get_type(event) ==
                RUT_INPUT_EVENT_TYPE_MOTION &&
                rut_motion_event_get_action(event) ==
                RUT_MOTION_EVENT_ACTION_DOWN) {
                rut_shell_grab_input(view->shell,
                                     rut_input_event_get_camera(event),
                                     simulator_implicit_grab_input_cb,
                                     view);
            }

            rut_input_queue_append(engine->simulator_input_queue, event);
        }
#ifdef RIG_EDITOR_ENABLED
        else {
            /* While editing we do picking in the editor itself since its
             * the graph in the frontend process that gets edited and
             * we then send operations to the simulator to update its
             * UI description. */
            return input_cb(event, user_data);
        }
#endif /* RIG_EDITOR_ENABLED */

        return RUT_INPUT_EVENT_STATUS_HANDLED;
    } else if (view->play_mode) {
        /* While in play mode then we do picking in the simulator */
        return input_cb(event, user_data);
        // return device_mode_input_cb (event, user_data);
    }
    // else
    //  return device_mode_input_cb (event, user_data);
    return RUT_INPUT_EVENT_STATUS_HANDLED;
}

#ifdef RIG_EDITOR_ENABLED
static void
tool_changed_cb(rig_editor_t *editor, rig_tool_id_t tool_id, void *user_data)
{
    rig_camera_view_t *view = user_data;

    switch (tool_id) {
    case RIG_TOOL_ID_SELECTION:
        rig_selection_tool_set_active(view->selection_tool, true);
        rig_rotation_tool_set_active(view->rotation_tool, false);
        break;
    case RIG_TOOL_ID_ROTATION:
        rig_rotation_tool_set_active(view->rotation_tool, true);
        rig_selection_tool_set_active(view->selection_tool, false);
        break;
    }
    view->tool_id = tool_id;
}
#endif /* RIG_EDITOR_ENABLED */

rig_camera_view_t *
rig_camera_view_new(rig_engine_t *engine)
{
    rig_camera_view_t *view = rut_object_alloc0(
        rig_camera_view_t, &rig_camera_view_type, _rig_camera_view_init_type);
    rut_shell_t *shell = engine->shell;

    view->shell = rut_object_ref(shell);
    view->engine = engine;

    rut_graphable_init(view);
    rut_paintable_init(view);

    view->input_region =
        rut_input_region_new_rectangle(0, 0, 0, 0, input_region_cb, view);
    rut_graphable_add_child(view, view->input_region);

    if (engine->frontend) {
        /* picking ray */
        view->picking_ray_color = cg_pipeline_new(engine->shell->cg_device);
        cg_pipeline_set_color4f(view->picking_ray_color, 1.0, 0.0, 0.0, 1.0);

        view->bg_pipeline = cg_pipeline_new(shell->cg_device);
    }

    view->matrix_stack = rut_matrix_stack_new(shell);

    view->view_camera = rig_entity_new(engine->shell);
    rig_entity_set_label(view->view_camera, "rig:camera");

    view->view_camera_component = rig_camera_new(engine,
                                                 -1, /* ortho/vp width */
                                                 -1, /* ortho/vp height */
                                                 NULL);
    rut_camera_set_clear(view->view_camera_component, false);
    rig_entity_add_component(view->view_camera, view->view_camera_component);

#ifdef RIG_EDITOR_ENABLED
    if (engine->frontend && engine->frontend_id == RIG_FRONTEND_ID_EDITOR) {
        rig_editor_t *editor = rig_engine_get_editor(engine);

        view->tool_overlay = rut_graph_new(engine->shell);
        rut_graphable_add_child(view, view->tool_overlay);
        rut_object_unref(view->tool_overlay);

        view->selection_tool = rig_selection_tool_new(view, view->tool_overlay);
        view->rotation_tool = rig_rotation_tool_new(view);

        rig_add_tool_changed_callback(editor,
                                      tool_changed_cb, view,
                                      NULL); /* destroy notify */
        tool_changed_cb(editor, RIG_TOOL_ID_SELECTION, view);
    }
#endif /* RIG_EDITOR_ENABLED */

    return view;
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
        initialize_view_camera(view);

        view->origin[0] = engine->device_width / 2;
        view->origin[1] = engine->device_height / 2;
        view->origin[2] = 0;
    }
}
