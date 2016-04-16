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

#include <rig-config.h>

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

struct _rig_ui_grab {
    c_list_t list_node;
    rig_input_grab_callback_t callback;
    rut_object_t *camera_entity;
    void *user_data;
};

static void
_rig_ui_remove_grab_link(rig_ui_t *ui, rig_ui_grab_t *grab)
{
    if (grab->camera_entity)
        rut_object_unref(grab->camera_entity);

    /* If we are in the middle of iterating the grab callbacks then this
     * will make it cope with removing arbritrary nodes from the list
     * while iterating it */
    if (ui->next_grab == grab)
        ui->next_grab = rut_container_of(grab->list_node.next, grab, list_node);

    c_list_remove(&grab->list_node);

    c_slice_free(rig_ui_grab_t, grab);
}

static void
_rig_ui_free(void *object)
{
    rig_ui_t *ui = object;
    c_llist_t *l;

    while (!c_list_empty(&ui->grabs)) {
        rig_ui_grab_t *first_grab =
            c_container_of(ui->grabs.next, rig_ui_grab_t, list_node);

        _rig_ui_remove_grab_link(ui, first_grab);
    }

    for (l = ui->views; l; l = l->next)
        rut_object_release(l->data, ui);
    c_llist_free(ui->views);

    for (l = ui->suspended_controllers; l; l = l->next)
        rut_object_unref(l->data);
    c_llist_free(ui->suspended_controllers);

    for (l = ui->controllers; l; l = l->next)
        rut_object_release(l->data, ui);
    c_llist_free(ui->controllers);

    for (l = ui->assets; l; l = l->next)
        rut_object_unref(l->data);
    c_llist_free(ui->assets);

    if (ui->dso_data)
        c_free(ui->dso_data);

    rut_object_free(rig_ui_t, object);
}

static rut_traverse_visit_flags_t
reap_entity_cb(rut_object_t *object, int depth, void *user_data)
{
    rig_engine_t *engine = user_data;

    rig_entity_reap(object, engine);

    rut_graphable_remove_child(object);

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
rig_ui_reap(rig_ui_t *ui)
{
    rig_engine_t *engine = ui->engine;
    c_llist_t *l;

    if (ui->scene) {
        rut_graphable_traverse(ui->scene,
                               RUT_TRAVERSE_DEPTH_FIRST,
                               reap_entity_cb,
                               NULL, /* post paint */
                               ui->engine);
        ui->scene = NULL;
    }

    for (l = ui->controllers; l; l = l->next) {
        rig_controller_t *controller = l->data;
        rig_controller_reap(controller, engine);
        rut_object_release(controller, ui);
    }
    c_llist_free(ui->controllers);
    ui->controllers = NULL;

    for (l = ui->assets; l; l = l->next) {
        rig_asset_t *asset = l->data;
        rig_asset_reap(asset, engine);
        rut_object_release(asset, ui);
    }
    c_llist_free(ui->assets);
    ui->assets = NULL;

    for (l = ui->views; l; l = l->next) {
        rig_view_reap(l->data);
        rut_object_release(l->data, ui);
    }
    c_llist_free(ui->views);
    ui->views = NULL;

    for (l = ui->buffers; l; l = l->next) {
        rig_ui_reap_buffer(ui, l->data);
        rut_object_release(l->data, ui);
    }
    c_llist_free(ui->buffers);
    ui->buffers = NULL;

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
    c_list_init(&ui->grabs);

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

    /* XXX: what about code_modules?
     * Should code_modules be associated with controllers instead of
     * components?
     */
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
    if (ui->scene) {
        rut_graphable_traverse(ui->scene,
                               RUT_TRAVERSE_DEPTH_FIRST,
                               print_entity_cb,
                               NULL, /* post paint */
                               NULL); /* user data */
    } else
        c_debug("  <empty/>\n");

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
    rut_object_claim(controller, ui);

    rig_controller_set_suspended(controller, ui->suspended);
}

void
rig_ui_remove_controller(rig_ui_t *ui, rig_controller_t *controller)
{
    rig_controller_set_suspended(controller, true);

    ui->controllers = c_llist_remove(ui->controllers, controller);
    rut_object_release(controller, ui);
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
                                     rut_object_t *component)
{
    c_return_if_fail(rut_object_is(component, RUT_TRAIT_ID_COMPONENTABLE));

    /* XXX: don't forget to update
     * rig_ui_entity_component_pre_remove_notify() too */

    if (ui->renderer)
        rut_renderer_notify_entity_changed(ui->renderer, entity);

    if (rut_object_is(component, RUT_TRAIT_ID_CAMERA)) {
        ui->cameras = c_llist_prepend(ui->cameras, entity);

#if 0
        if (ui->renderer &&
            strcmp("play-camera", rig_entity_get_label(entity)) == 0)
        {
            rig_frontend_t *frontend = ui->engine->frontend;
            rig_onscreen_view_t *view = rig_onscreen_view_new(frontend);

            rig_camera_view_set_ui(view->camera_view, ui);
            rig_camera_view_set_camera_entity(view->camera_view, entity);
        }
#endif
    } else if (rut_object_is(component, rig_code_module_trait_id)) {
        rig_code_module_props_t *module_props =
            rut_object_get_properties(component, rig_code_module_trait_id);

        c_list_insert(ui->code_modules.prev, &module_props->system_link);
    } else if (rut_object_get_type(component) == &rig_light_type) {
        ui->lights = c_llist_prepend(ui->lights, entity);

        /* TODO: remove limitation that we can only render a single light */
        ui->light = ui->lights->data;
    } else if (rut_object_get_type(component) == &rig_source_type) {
        ui->sources = c_llist_prepend(ui->sources, entity);
    }
}

void
rig_ui_entity_component_pre_remove_notify(rig_ui_t *ui,
                                          rig_entity_t *entity,
                                          rut_object_t *component)
{
    c_return_if_fail(rut_object_is(component, RUT_TRAIT_ID_COMPONENTABLE));

    /* XXX: don't forget to update
     * rig_ui_entity_component_added_notify() too */

    if (ui->renderer)
        rut_renderer_notify_entity_changed(ui->renderer, entity);

    if (rut_object_is(component, RUT_TRAIT_ID_CAMERA)) {
        ui->cameras = c_llist_remove(ui->cameras, entity);
    } else if (rut_object_is(component, rig_code_module_trait_id)) {
        rig_code_module_props_t *module_props =
            rut_object_get_properties(component, rig_code_module_trait_id);

        c_list_remove(&module_props->system_link);
    } else if (rut_object_get_type(component) == &rig_light_type) {
        ui->lights = c_llist_remove(ui->lights, entity);

        /* TODO: remove limitation that we can only render a single light */
        ui->light = ui->lights ? ui->lights->data : NULL;
    } else if (rut_object_get_type(component) == &rig_camera_type) {
        ui->sources = c_llist_remove(ui->sources, entity);
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
rig_ui_register_all_entity_components(rig_ui_t *ui, rig_entity_t *entity)
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
rig_ui_code_modules_update(rig_ui_t *ui, rig_code_module_update_t *state)
{
    rig_code_module_props_t *module;

    c_list_for_each(module, &ui->code_modules, system_link) {
        rig_code_module_vtable_t *vtable =
            rut_object_get_vtable(module->object, rig_code_module_trait_id);

        if (vtable->update)
            vtable->update(module->object, state);
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
transform_ray(c_matrix_t *transform,
              bool inverse_transform,
              float ray_origin[3],
              float ray_direction[3])
{
    c_matrix_t inverse, normal_matrix, *m;

    m = transform;
    if (inverse_transform) {
        c_matrix_get_inverse(transform, &inverse);
        m = &inverse;
    }

    c_matrix_transform_points(m,
                               3, /* num components for input */
                               sizeof(float) * 3, /* input stride */
                               ray_origin,
                               sizeof(float) * 3, /* output stride */
                               ray_origin,
                               1 /* n_points */);

    c_matrix_get_inverse(m, &normal_matrix);
    c_matrix_transpose(&normal_matrix);

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
        const c_matrix_t *matrix = rut_transformable_get_matrix(object);
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
        c_matrix_t transform;
        rut_object_t *input;

        input = rig_entity_get_component(entity, RUT_COMPONENT_TYPE_INPUT);
        if (!input)
            return RUT_TRAVERSE_VISIT_CONTINUE;

#if 0
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
#endif

        geometry = rig_entity_get_component(entity, RUT_COMPONENT_TYPE_GEOMETRY);

        if (!(geometry && rut_object_is(geometry, RUT_TRAIT_ID_MESHABLE) &&
              (mesh = rut_meshable_get_mesh(geometry))))
        {
            return RUT_TRAVERSE_VISIT_CONTINUE;
        }

        /* transform the ray into the model space */
        memcpy(transformed_ray_origin, pick_ctx->ray_origin, 3 * sizeof(float));
        memcpy(transformed_ray_direction,
               pick_ctx->ray_direction,
               3 * sizeof(float));

        rut_matrix_stack_get(pick_ctx->matrix_stack, &transform);
        //c_debug("Inputtable transform:");
        //c_matrix_print(&transform);

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
            const c_matrix_t *view =
                rut_camera_get_view_transform(pick_ctx->camera);
            float w = 1;

            //c_debug("Hit something");

            /* to compare intersection distances we find the actual point of ray
             * intersection in model coordinates and transform that into eye
             * coordinates */

            transformed_ray_direction[0] *= distance;
            transformed_ray_direction[1] *= distance;
            transformed_ray_direction[2] *= distance;

            transformed_ray_direction[0] += transformed_ray_origin[0];
            transformed_ray_direction[1] += transformed_ray_origin[1];
            transformed_ray_direction[2] += transformed_ray_origin[2];

            c_matrix_transform_point(&transform,
                                      &transformed_ray_direction[0],
                                      &transformed_ray_direction[1],
                                      &transformed_ray_direction[2],
                                      &w);
            c_matrix_transform_point(view,
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
    if (pick_ctx.selected_entity) {
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
    const c_matrix_t *inverse_projection;
    const c_matrix_t *camera_view;
    c_matrix_t camera_transform;
    rut_object_t *picked_entity;

    rig_entity_set_camera_view_from_transform(camera);

    /* XXX: we could forward normalized input coordinates from the
     * frontend so that the simulator doesn't need to be notified of
     * windows resizes. */
    viewport = rut_camera_get_viewport(camera_component);
#if 0
    c_warning("camera viewport not intialized for picking: x=%f, y=%f, width=%f, height=%f",
              viewport[0],
              viewport[1],
              viewport[2],
              viewport[3]);
#endif
    inverse_projection = rut_camera_get_inverse_projection(camera_component);

    //c_debug("UI: Camera inverse projection:");
    //c_matrix_print(inverse_projection);

    camera_view = rut_camera_get_view_transform(camera_component);
    c_matrix_get_inverse(camera_view, &camera_transform);

    //c_debug("UI: Camera transform:");
    //c_matrix_print(&camera_transform);

    rut_camera_transform_window_coordinate(camera_component, &x, &y);

    screen_pos[0] = x;
    screen_pos[1] = y;

    //c_debug("screen pos x=%f, y=%f\n", x, y);

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
        rig_property_t *label =
            rig_introspectable_lookup_property (picked_entity, "label");

        c_debug ("Entity picked: %s\n", rig_property_get_text (label));
    }
#endif

    return picked_entity;
}

static rut_object_t *
pick_for_event(rig_ui_t *ui, rut_input_event_t *event)
{
    rut_input_event_type_t type = rut_input_event_get_type(event);
    rig_entity_t *camera = event->camera_entity;
    rut_object_t *camera_component;

    c_return_val_if_fail(camera, NULL);

    camera_component = rig_entity_get_component(camera, RUT_COMPONENT_TYPE_CAMERA);

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
    rig_ui_grab_t *grab;

    c_list_for_each_safe(grab, ui->next_grab, &ui->grabs, list_node) {
        rut_object_t *old_camera = event->camera_entity;
        rut_input_event_status_t grab_status;

        if (grab->camera_entity)
            event->camera_entity = grab->camera_entity;

        grab_status = grab->callback(event, entity, grab->user_data);

        event->camera_entity = old_camera;

        if (grab_status == RUT_INPUT_EVENT_STATUS_HANDLED)
            return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

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

void
rig_ui_grab_input(rig_ui_t *ui,
                  rut_object_t *camera_entity,
                  rig_input_grab_callback_t callback,
                  void *user_data)
{
    rig_ui_grab_t *grab = c_slice_new(rig_ui_grab_t);

    grab->callback = callback;
    grab->user_data = user_data;

    if (camera_entity)
        grab->camera_entity = rut_object_ref(camera_entity);
    else
        grab->camera_entity = NULL;

    c_list_insert(&ui->grabs, &grab->list_node);
}

void
rig_ui_ungrab_input(rig_ui_t *ui,
                    rig_input_grab_callback_t callback,
                    void *user_data)
{
    rig_ui_grab_t *grab;

    c_list_for_each(grab, &ui->grabs, list_node) {
        if (grab->callback == callback && grab->user_data == user_data) {
            _rig_ui_remove_grab_link(ui, grab);
            break;
        }
    }
}

static void
_rig_view_free(void *object)
{
    rig_view_t *view = object;

    if (view->camera_entity) {
        rut_object_unref(view->camera_entity);
        view->camera_entity = NULL;
    }

    if (view->onscreen)
        rut_object_unref(view->onscreen);
    if (view->camera_view)
        rut_object_unref(view->camera_view);

    rut_object_free(rig_view_t, object);
}

static rut_type_t rig_view_type;

static void
_rig_view_init_type(void)
{
    rut_type_t *type = &rig_view_type;
#define TYPE rig_view_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_view_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */
}

static void
on_onscreen_resize(cg_onscreen_t *onscreen,
                   int width,
                   int height,
                   void *user_data)
{
    rig_view_t *view = user_data;

    rig_view_set_width(view, width);
    rig_view_set_height(view, height);

    view->engine->frontend->dirty_view_geometry = true;

    rut_shell_queue_redraw(view->engine->shell);
}

static rig_property_spec_t _rig_view_prop_specs[] = {
    { .name = "width",
      .nick = "Width",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .data_offset = C_STRUCT_OFFSET(rig_view_t, width),
      .setter.integer_type = rig_view_set_width,
      .flags = RUT_PROPERTY_FLAG_READABLE, //XXX: Should only be writable in frontend
    },
    { .name = "height",
      .nick = "Height",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .data_offset = C_STRUCT_OFFSET(rig_view_t, height),
      .setter.integer_type = rig_view_set_height,
      .flags = RUT_PROPERTY_FLAG_READABLE, //XXX: Should only be writable in frontend
    },
    { .name = "camera_entity",
      .nick = "Camera Entity",
      .type = RUT_PROPERTY_TYPE_OBJECT,
      .data_offset = C_STRUCT_OFFSET(rig_view_t, camera_entity),
      .validation = { .object.type = &rig_entity_type },
      .setter.object_type = rig_view_set_camera,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = false },
    { NULL }
};

rig_view_t *
rig_view_new(rig_engine_t *engine)
{
    rig_view_t *view =
        rut_object_alloc0(rig_view_t, &rig_view_type,
                          _rig_view_init_type);

    view->engine = engine;

    view->width = 800;
    view->height = 600;

    rig_introspectable_init(view, _rig_view_prop_specs, view->properties);

    return view;
}

void
rig_ui_add_view(rig_ui_t *ui, rig_view_t *view)
{
    rig_frontend_t *frontend = ui->engine->frontend;

    ui->views = c_llist_prepend(ui->views, view);
    rut_object_claim(view, ui);

    if (frontend) {
        view->onscreen = rut_shell_onscreen_new(view->engine->shell,
                                                view->width,
                                                view->height);

        rut_shell_onscreen_allocate(view->onscreen);
        rut_shell_onscreen_set_resizable(view->onscreen, true);

        rut_shell_onscreen_set_title(view->onscreen, "Rig " C_STRINGIFY(RIG_VERSION));

        cg_onscreen_add_resize_callback(view->onscreen->cg_onscreen,
                                        on_onscreen_resize, view,
                                        NULL); /* destroy notify */

        rut_shell_onscreen_show(view->onscreen);

        view->camera_view = rig_camera_view_new(frontend);
        rig_camera_view_set_framebuffer(view->camera_view,
                                        view->onscreen->cg_onscreen);
        rig_camera_view_set_ui(view->camera_view, ui);

        rut_shell_queue_redraw(ui->engine->shell);
    }
}

void
rig_ui_remove_view(rig_ui_t *ui, rig_view_t *view)
{
    ui->views = c_llist_remove(ui->views, view);
    rut_object_release(view, ui);
}

void
rig_view_reap(rig_view_t *view)
{
    /* Currently views don't own any objects that need to be
     * explicitly reaped
     */

    rig_engine_queue_delete(view->engine, view);
}

void
rig_view_set_width(rut_object_t *obj, int width)
{
    rig_view_t *view = obj;
    rig_property_context_t *prop_ctx;

    if (view->width == width)
        return;

    view->width = width;

    prop_ctx = &view->engine->_property_ctx;
    rig_property_dirty(prop_ctx, &view->properties[RIG_VIEW_PROP_WIDTH]);
}

void
rig_view_set_height(rut_object_t *obj, int height)
{
    rig_view_t *view = obj;
    rig_property_context_t *prop_ctx;

    if (view->height == height)
        return;

    view->height = height;

    prop_ctx = &view->engine->_property_ctx;
    rig_property_dirty(prop_ctx, &view->properties[RIG_VIEW_PROP_HEIGHT]);
}

rig_view_t *
rig_ui_find_view_for_onscreen(rig_ui_t *ui, rut_shell_onscreen_t *onscreen)
{
    c_llist_t *l;

    for (l = ui->views; l; l = l->next) {
        rig_view_t *view = l->data;

        if (view->onscreen == onscreen)
            return view;
    }

    return NULL;
}

void
rig_view_set_camera(rut_object_t *obj, rut_object_t *camera_entity)
{
    rig_view_t *view = obj;
    rig_property_context_t *prop_ctx;

    if (view->camera_entity == camera_entity)
        return;

    if (view->camera_entity) {
        rut_object_unref(view->camera_entity);
        view->camera_entity = NULL;
    }

    if (camera_entity)
        view->camera_entity = rut_object_ref(camera_entity);

    if (view->camera_view) {
        rig_camera_view_set_camera_entity(view->camera_view, camera_entity);
        rut_shell_onscreen_set_input_camera(view->onscreen, camera_entity);
    }

    prop_ctx = &view->engine->_property_ctx;
    rig_property_dirty(prop_ctx, &view->properties[RIG_VIEW_PROP_CAMERA_ENTITY]);
}

void
rig_ui_add_buffer(rig_ui_t *ui, rut_buffer_t *buffer)
{
    ui->buffers = c_llist_prepend(ui->buffers, buffer);
    rut_object_claim(buffer, ui);
}

void
rig_ui_remove_buffer(rig_ui_t *ui, rut_buffer_t *buffer)
{
    ui->buffers = c_llist_remove(ui->buffers, buffer);
    rut_object_release(buffer, ui);
}

void
rig_ui_reap_buffer(rig_ui_t *ui, rut_buffer_t *buffer)
{
    /* Buffers don't own any objects that need to be
     * explicitly reaped
     */

    rig_engine_queue_delete(ui->engine, buffer);
}

void
rig_ui_start_frame(rig_ui_t *ui)
{
    c_llist_t *l;

    for (l = ui->sources; l; l = l->next) {
        rig_source_prepare_for_frame(l->data);
    }
}

void
rig_ui_register_object(rig_ui_t *ui, rut_object_t *object)
{
    /* TODO: find a more scalable approach... */
    if (rut_object_get_type(object) == &rig_source_type) {
        ui->sources = c_llist_prepend(ui->sources, object);
    }
}

void
rig_ui_unregister_object(rig_ui_t *ui, rut_object_t *object)
{
    if (rut_object_get_type(object) == &rig_source_type) {
        ui->sources= c_llist_remove(ui->sources, object);
    }
}

