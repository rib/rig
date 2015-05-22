/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation.
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

#include <config.h>

#include <math.h>

#include <rut.h>

#include "rig-ui.h"
#include "rig-code.h"
#include "rig-entity.h"
#include "rig-renderer.h"
#include "rig-asset.h"
#include "rig-code-module.h"
#include "rut-renderer.h"

#include "components/rig-material.h"

static void
_rig_ui_free(void *object)
{
    rig_ui_t *ui = object;
    c_llist_t *l;

    for (l = ui->suspended_controllers; l; l = l->next)
        rut_object_unref(l->data);
    c_llist_free(ui->suspended_controllers);

    for (l = ui->controllers; l; l = l->next)
        rut_object_unref(l->data);
    c_llist_free(ui->controllers);

    for (l = ui->assets; l; l = l->next)
        rut_object_unref(l->data);
    c_llist_free(ui->assets);

    if (ui->scene)
        rut_object_unref(ui->scene);

    if (ui->play_camera)
        rut_object_unref(ui->play_camera);

    if (ui->dso_data)
        c_free(ui->dso_data);

    rut_object_free(rig_ui_t, object);
}

static rut_traverse_visit_flags_t
reap_entity_cb(rut_object_t *object, int depth, void *user_data)
{
    rig_engine_t *engine = user_data;

    /* The root node is a rut_graph_t that shouldn't be reaped */
    if (rut_object_get_type(object) != &rig_entity_type)
        return RUT_TRAVERSE_VISIT_CONTINUE;

    rig_entity_reap(object, engine);

    rut_graphable_remove_child(object);

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
rig_ui_reap(rig_ui_t *ui)
{
    rig_engine_t *engine = ui->engine;
    c_llist_t *l;

    rut_graphable_traverse(ui->scene,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           reap_entity_cb,
                           NULL, /* post paint */
                           ui->engine);

    for (l = ui->controllers; l; l = l->next) {
        rig_controller_t *controller = l->data;
        rig_controller_reap(controller, engine);
        rut_object_release(controller, ui);
    }

    /* We could potentially leave these to be freed in _free() but it
     * seems a bit ugly to keep the list containing pointers to
     * controllers no longer owned by the ui. */
    c_llist_free(ui->controllers);
    ui->controllers = NULL;

    for (l = ui->assets; l; l = l->next) {
        rig_asset_t *asset = l->data;
        rig_asset_reap(asset, engine);
        rut_object_release(asset, ui);
    }

    /* We could potentially leave these to be freed in _free() but it
     * seems a bit ugly to keep the list containing pointers to
     * assets no longer owned by the ui. */
    c_llist_free(ui->assets);
    ui->assets = NULL;

    /* XXX: The ui itself is just a normal ref-counted object that
     * doesn't need to be unregistered so we don't call
     * rig_engine_queue_delete() for it */
}

rut_type_t rig_ui_type;

static void
_rig_ui_init_type(void)
{
    rut_type_init(&rig_ui_type, "rig_ui_t", _rig_ui_free);
}

rig_ui_t *
rig_ui_new(rig_engine_t *engine)
{
    rig_ui_t *ui = rut_object_alloc0(rig_ui_t, &rig_ui_type, _rig_ui_init_type);

    ui->engine = engine;

    if (engine->frontend)
        ui->renderer = engine->frontend->renderer;

    ui->pick_matrix_stack = rut_matrix_stack_new(engine->shell);

    c_list_init(&ui->code_modules);

    return ui;
}

void
rig_ui_set_dso_data(rig_ui_t *ui, uint8_t *data, int len)
{
    if (ui->dso_data)
        c_free(ui->dso_data);

    ui->dso_data = c_malloc(len);
    memcpy(ui->dso_data, data, len);
    ui->dso_len = len;
}

typedef struct {
    const char *label;
    rig_entity_t *entity;
} find_entity_data_t;

static rut_traverse_visit_flags_t
find_entity_cb(rut_object_t *object, int depth, void *user_data)
{
    find_entity_data_t *data = user_data;

    if (rut_object_get_type(object) == &rig_entity_type &&
        !strcmp(data->label, rig_entity_get_label(object))) {
        data->entity = object;
        return RUT_TRAVERSE_VISIT_BREAK;
    }

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

rig_entity_t *
rig_ui_find_entity(rig_ui_t *ui, const char *label)
{
    find_entity_data_t data = { .label = label, .entity = NULL };

    rut_graphable_traverse(ui->scene,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           find_entity_cb,
                           NULL, /* after_children_cb */
                           &data);

    return data.entity;
}

static void
initialise_play_camera_position(rig_engine_t *engine, rig_ui_t *ui)
{
#if 0
    float fov_y = 10; /* y-axis field of view */
    float aspect = (float)engine->device_width / (float)engine->device_height;
    float z_near = 10; /* distance to near clipping plane */
    float z_2d = 30;
    float position[3];
    float left, right, top;
    float left_2d_plane, right_2d_plane;
    float width_scale;
    float width_2d_start;

    /* Initialise the camera to the center of the device with a z
     * position that will give it pixel aligned coordinates at the
     * origin */
    top = z_near * tanf(fov_y * C_PI / 360.0);
    left = -top * aspect;
    right = top * aspect;

    left_2d_plane = left / z_near * z_2d;
    right_2d_plane = right / z_near * z_2d;

    width_2d_start = right_2d_plane - left_2d_plane;

    width_scale = width_2d_start / engine->device_width;

    position[0] = engine->device_width / 2.0f;
    position[1] = engine->device_height / 2.0f;
    position[2] = z_2d / width_scale;
#else
    float position[3];

    position[0] = 0;
    position[1] = 0;
    position[2] = 100;
#endif

    rig_entity_set_position(ui->play_camera, position);
}

static void
initialise_play_camera_component(rut_object_t *camera_component)
{
    rut_camera_set_projection_mode(camera_component, RUT_PROJECTION_PERSPECTIVE);
    rut_camera_set_field_of_view(camera_component, 10);
    rut_camera_set_near_plane(camera_component, 10);
    rut_camera_set_far_plane(camera_component, 10000);
}

void
rig_ui_prepare(rig_ui_t *ui)
{
    rig_engine_t *engine = ui->engine;
    rig_controller_t *controller;
    rut_object_t *light_camera;
    c_llist_t *l;

    if (!ui->scene)
        ui->scene = rut_graph_new(engine->shell);

    if (!ui->lights) {
        rig_entity_t *light;
        rig_light_t *light_component;
        float vector3[3];
        cg_color_t color;

        light = rig_entity_new(engine->shell);
        rig_entity_set_label(light, "light");

        vector3[0] = 0;
        vector3[1] = 0;
        vector3[2] = 500;
        rig_entity_set_position(light, vector3);

        rig_entity_rotate_x_axis(light, 20);
        rig_entity_rotate_y_axis(light, -20);

        light_component = rig_light_new(engine->shell);

        cg_color_init_from_4f(&color, .2f, .2f, .2f, 1.f);
        rig_light_set_ambient(light_component, &color);
        cg_color_init_from_4f(&color, .6f, .6f, .6f, 1.f);
        rig_light_set_diffuse(light_component, &color);
        cg_color_init_from_4f(&color, .4f, .4f, .4f, 1.f);
        rig_light_set_specular(light_component, &color);

        rig_entity_add_component(light, light_component);

        rut_graphable_add_child(ui->scene, light);
        ui->lights = c_llist_prepend(ui->lights, light);
    }

    /* TODO: remove limitation that we can only render a single light */
    ui->light = ui->lights->data;

    light_camera =
        rig_entity_get_component(ui->light, RUT_COMPONENT_TYPE_CAMERA);

    if (!light_camera) {
        light_camera = rig_camera_new(engine,
                                      1000, /* ortho/vp width */
                                      1000, /* ortho/vp height */
                                      NULL);

        rut_camera_set_background_color4f(light_camera, 0.f, .3f, 0.f, 1.f);
        rut_camera_set_projection_mode(light_camera,
                                       RUT_PROJECTION_ORTHOGRAPHIC);
        rut_camera_set_orthographic_coordinates(
            light_camera, -1000, -1000, 1000, 1000);
        rut_camera_set_near_plane(light_camera, 1.1f);
        rut_camera_set_far_plane(light_camera, 1500.f);

        rig_entity_add_component(ui->light, light_camera);
    }

    /* FIXME: this should be handled by the renderer and should
     * be triggered via a resize callback */
    if (engine->frontend) {
        cg_framebuffer_t *fb;
        int width, height;

        fb = rig_renderer_get_shadow_fb(ui->renderer);

        width = cg_framebuffer_get_width(fb);
        height = cg_framebuffer_get_height(fb);

        rut_camera_set_framebuffer(light_camera, fb);
        rut_camera_set_viewport(light_camera, 0, 0, width, height);
    }

    if (!ui->controllers) {
        controller = rig_controller_new(engine, "Controller 0");
        rig_controller_set_active(controller, true);
        ui->controllers = c_llist_prepend(ui->controllers, controller);
    }

    /* Explcitly transfer ownership of controllers to the UI for
     * improved ref-count debugging.
     *
     * XXX: don't RIG_ENABLE_DEBUG guard this without also
     * updating rig_ui_reap()
     */
    for (l = ui->controllers; l; l = l->next) {
        rut_object_claim(l->data, ui);
        rut_object_unref(l->data);
    }

    if (!ui->play_camera) {
        ui->play_camera = rig_entity_new(engine->shell);
        rig_entity_set_label(ui->play_camera, "play-camera");

        initialise_play_camera_position(engine, ui);

        rut_graphable_add_child(ui->scene, ui->play_camera);

        ui->cameras = c_llist_prepend(ui->cameras, ui->play_camera);
    }

    ui->play_camera_component =
        rig_entity_get_component(ui->play_camera, RUT_COMPONENT_TYPE_CAMERA);

    if (!ui->play_camera_component) {
        ui->play_camera_component = rig_camera_new(engine,
                                                   -1, /* ortho/vp width */
                                                   -1, /* ortho/vp height */
                                                   engine->frontend->onscreen);

        initialise_play_camera_component(ui->play_camera_component);

        rig_entity_add_component(ui->play_camera,
                                 ui->play_camera_component);
    }

#warning "HACK"
    initialise_play_camera_position(engine, ui);
    initialise_play_camera_component(ui->play_camera_component);

    rut_camera_set_clear(ui->play_camera_component, false);

    rig_ui_suspend(ui);
}

void
rig_ui_suspend(rig_ui_t *ui)
{
    c_llist_t *l;

    if (ui->suspended)
        return;

    for (l = ui->controllers; l; l = l->next) {
        rig_controller_t *controller = l->data;

        rig_controller_set_suspended(controller, true);

        ui->suspended_controllers =
            c_llist_prepend(ui->suspended_controllers, controller);

        /* We take a reference on all suspended controllers so we
         * don't need to worry if any of the controllers are deleted
         * while in edit mode. */
        rut_object_ref(controller);
    }

    ui->suspended = true;
}

void
rig_ui_resume(rig_ui_t *ui)
{
    c_llist_t *l;

    if (!ui->suspended)
        return;

    for (l = ui->suspended_controllers; l; l = l->next) {
        rig_controller_t *controller = l->data;

        rig_controller_set_suspended(controller, false);
        rut_object_unref(controller);
    }

    c_llist_free(ui->suspended_controllers);
    ui->suspended_controllers = NULL;

    ui->suspended = false;
}

static bool
print_component_cb(rut_object_t *component, void *user_data)
{
    int depth = *(int *)user_data;
    char *name = rig_engine_get_object_debug_name(component);
    c_debug("%*s%s\n", depth + 2, " ", name);
    c_free(name);

    return true; /* continue */
}

static rut_traverse_visit_flags_t
print_entity_cb(rut_object_t *object, int depth, void *user_data)
{
    char *name = rig_engine_get_object_debug_name(object);
    c_debug("%*s%s\n", depth, " ", name);

    if (rut_object_get_type(object) == &rig_entity_type) {
        rig_entity_foreach_component_safe(object, print_component_cb, &depth);
    }

    c_free(name);

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
rig_ui_print(rig_ui_t *ui)
{
    c_llist_t *l;

    c_debug("Scenegraph:\n");
    rut_graphable_traverse(ui->scene,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           print_entity_cb,
                           NULL, /* post paint */
                           NULL); /* user data */

    c_debug("Controllers:\n");
    for (l = ui->controllers; l; l = l->next) {
        char *name = rig_engine_get_object_debug_name(l->data);
        c_debug("  %s\n", name);
        c_free(name);
    }

    c_debug("Assets:\n");
    for (l = ui->assets; l; l = l->next) {
        char *name = rig_engine_get_object_debug_name(l->data);
        c_debug("  %s\n", name);
        c_free(name);
    }
}

void
rig_ui_add_controller(rig_ui_t *ui, rig_controller_t *controller)
{
    ui->controllers = c_llist_prepend(ui->controllers, controller);
    rut_object_ref(controller);

    if (!ui->suspended)
        rig_controller_set_suspended(controller, false);
}

void
rig_ui_remove_controller(rig_ui_t *ui, rig_controller_t *controller)
{
    rig_controller_set_suspended(controller, true);

    ui->controllers = c_llist_remove(ui->controllers, controller);
    rut_object_unref(controller);
}

/* These notifications lets us associate entities with particular
 * sub-systems, such as rendering or physics.
 *
 * For now we don't differentiate entities for rendering here and the
 * renderer code simply traverses the full scenegraph matching
 * appropriate entities at that point.
 *
 * So we don't need to walk through *all* entities each frame to
 * run any neccesary update() functions, we track components
 * associated with code in separate list...
 */
void
rig_ui_entity_component_added_notify(rig_ui_t *ui,
                                     rig_entity_t *entity,
                                     rut_component_t *component)
{
    if (ui->renderer)
        rut_renderer_notify_entity_changed(ui->renderer, entity);

    if (rut_object_is(component, RUT_TRAIT_ID_CAMERA)) {
        ui->cameras = c_llist_prepend(ui->cameras, entity);

        if(strcmp("play-camera", rig_entity_get_label(entity)))
            ui->play_camera = rut_object_ref(entity);
    } else if (rut_object_is(component, rig_code_module_trait_id)) {
        rig_code_module_props_t *module_props =
            rut_object_get_properties(component, rig_code_module_trait_id);

        c_list_insert(ui->code_modules.prev, &module_props->system_link);
    }
}

void
rig_ui_entity_component_pre_remove_notify(rig_ui_t *ui,
                                          rig_entity_t *entity,
                                          rut_component_t *component)
{
    if (ui->renderer)
        rut_renderer_notify_entity_changed(ui->renderer, entity);

    if (rut_object_is(component, rig_code_module_trait_id)) {
        rig_code_module_props_t *module_props =
            rut_object_get_properties(component, rig_code_module_trait_id);

        c_list_remove(&module_props->system_link);
    }
}

static bool
register_component_cb(rut_object_t *component, void *user_data)
{
    rig_ui_t *ui = user_data;
    rut_componentable_props_t *component_props =
        rut_object_get_properties(component, RUT_TRAIT_ID_COMPONENTABLE);

    rig_ui_entity_component_added_notify(ui, component_props->entity, component);

    return true; /* continue */
}

/* Used when first loading a UI where the entities are registered
 * at the end of loading with components already added... */
void
rig_ui_register_entity(rig_ui_t *ui, rig_entity_t *entity)
{
    rig_entity_foreach_component(entity, register_component_cb, ui);
}

void
rig_ui_code_modules_load(rig_ui_t *ui)
{
    rig_code_module_props_t *module;

    c_list_for_each(module, &ui->code_modules, system_link) {
        rig_code_module_vtable_t *vtable =
            rut_object_get_vtable(module->object, rig_code_module_trait_id);

        vtable->load(module->object);
    }
}

void
rig_ui_code_modules_update(rig_ui_t *ui)
{
    rig_code_module_props_t *module;

    c_list_for_each(module, &ui->code_modules, system_link) {
        rig_code_module_vtable_t *vtable =
            rut_object_get_vtable(module->object, rig_code_module_trait_id);

        if (vtable->update)
            vtable->update(module->object);
    }
}

void
rig_ui_code_modules_handle_input(rig_ui_t *ui, rut_input_event_t *event)
{
    rig_code_module_props_t *module;

    c_list_for_each(module, &ui->code_modules, system_link) {
        rig_code_module_vtable_t *vtable =
            rut_object_get_vtable(module->object, rig_code_module_trait_id);

        if (vtable->input)
            vtable->input(module->object, event);
    }
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
    rig_ui_t *ui;
    rig_engine_t *engine;
    rut_object_t *camera;
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
        if (!input)
            return RUT_TRAVERSE_VISIT_CONTINUE;

        if (rut_object_is(input, RUT_TRAIT_ID_PICKABLE)) {
            rut_matrix_stack_get(pick_ctx->matrix_stack, &transform);

            if (rut_pickable_pick(input,
                                  pick_ctx->camera,
                                  &transform,
                                  pick_ctx->x,
                                  pick_ctx->y)) {
                pick_ctx->selected_entity = entity;
                return RUT_TRAVERSE_VISIT_BREAK;
            } else
                return RUT_TRAVERSE_VISIT_CONTINUE;
        }

        geometry = rig_entity_get_component(entity, RUT_COMPONENT_TYPE_GEOMETRY);

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
                rut_camera_get_view_transform(pick_ctx->camera);
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

rig_entity_t *
pick(rig_ui_t *ui,
     rut_object_t *camera,
     float x,
     float y,
     float ray_origin[3],
     float ray_direction[3])
{
    rig_engine_t *engine = ui->engine;
    pick_context_t pick_ctx;

    pick_ctx.ui = ui;
    pick_ctx.engine = engine;
    pick_ctx.camera = camera;
    pick_ctx.matrix_stack = ui->pick_matrix_stack;
    pick_ctx.x = x;
    pick_ctx.y = y;
    pick_ctx.selected_distance = -FLT_MAX;
    pick_ctx.selected_entity = NULL;
    pick_ctx.ray_origin = ray_origin;
    pick_ctx.ray_direction = ray_direction;

    rut_graphable_traverse(ui->scene,
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

rig_entity_t *
pick_for_camera(rig_ui_t *ui,
                rig_entity_t *camera,
                rut_object_t *camera_component,
                float x, float y)
{
    float ray_position[3], ray_direction[3], screen_pos[2];
    const float *viewport;
    const cg_matrix_t *inverse_projection;
    const cg_matrix_t *camera_view;
    cg_matrix_t camera_transform;
    rut_object_t *picked_entity;

    rig_entity_set_camera_view_from_transform(camera);

    viewport = rut_camera_get_viewport(camera_component);
    inverse_projection = rut_camera_get_inverse_projection(camera_component);

    // c_debug("Camera inverse projection: %p\n", engine->simulator);
    // cg_debug_matrix_print(inverse_projection);

    camera_view = rut_camera_get_view_transform(camera_component);
    cg_matrix_get_inverse(camera_view, &camera_transform);

    // c_debug("Camera transform:\n");
    // cg_debug_matrix_print (&camera_transform);

    rut_camera_transform_window_coordinate(camera_component, &x, &y);

    screen_pos[0] = x;
    screen_pos[1] = y;

    // c_debug("screen pos x=%f, y=%f\n", x, y);

    rut_util_create_pick_ray(viewport,
                             inverse_projection,
                             &camera_transform,
                             screen_pos,
                             ray_position,
                             ray_direction);

#if 0
    c_debug("ray pos %f,%f,%f dir %f,%f,%f\n",
            ray_position[0],
            ray_position[1],
            ray_position[2],
            ray_direction[0],
            ray_direction[1],
            ray_direction[2]);
#endif

    picked_entity =
        pick(ui, camera_component, x, y, ray_position, ray_direction);

#if 0
    if (picked_entity)
    {
        rut_property_t *label =
            rut_introspectable_lookup_property (picked_entity, "label");

        c_debug ("Entity picked: %s\n", rut_property_get_text (label));
    }
#endif

    return picked_entity;
}

static rut_object_t *
pick_for_event(rig_ui_t *ui, rut_input_event_t *event)
{
    rut_input_event_type_t type = rut_input_event_get_type(event);

    /* TODO: setup up event->camera and don't assume we only have
     * one camera that can have associated input events */
    rig_entity_t *camera = ui->play_camera;
    rut_object_t *camera_component = ui->play_camera_component;

    switch (type) {
    case RUT_INPUT_EVENT_TYPE_KEY:
    case RUT_INPUT_EVENT_TYPE_TEXT:
        return pick_for_camera(ui,
                               camera,
                               camera_component,
                               ui->engine->shell->mouse_x,
                               ui->engine->shell->mouse_y);
    case RUT_INPUT_EVENT_TYPE_MOTION:
        return pick_for_camera(ui,
                               camera,
                               camera_component,
                               rut_motion_event_get_x(event),
                               rut_motion_event_get_y(event));
    }

    return NULL;
}

rut_input_event_status_t
rig_ui_handle_input_event(rig_ui_t *ui, rut_input_event_t *event)
{
    rut_input_event_status_t status = RUT_INPUT_EVENT_STATUS_UNHANDLED;
    rig_entity_t *entity = pick_for_event(ui, event);
    rut_object_t *inputable;

    if (!entity)
        return status;

    inputable = rig_entity_get_component(entity, RUT_COMPONENT_TYPE_INPUT);

    /* entity should only be NULL if we didn't find an inputable */
    c_return_val_if_fail(inputable, RUT_INPUT_EVENT_STATUS_UNHANDLED);

    while (inputable) {

        status = rut_inputable_handle_event(inputable, event);

        if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
            break;

#if 0
        inputable = rut_inputable_get_next(inputable);
#else
#warning "TODO: support rut_inputable_get_next()"
        break;
#endif
    }

    return status;
}
