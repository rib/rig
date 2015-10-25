/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#include "rig-controller.h"
#include "rig-engine.h"
#include "rig-timeline.h"

static rut_property_spec_t _rig_controller_prop_specs[] = {
    { .name = "label",
      .nick = "label_t",
      .blurb = "A label for the entity",
      .type = RUT_PROPERTY_TYPE_TEXT,
      .getter.text_type = rig_controller_get_label,
      .setter.text_type = rig_controller_set_label,
      .flags = RUT_PROPERTY_FLAG_READWRITE },
    { .name = "active",
      .nick = "Active",
      .blurb = "Whether the controller is actively asserting "
               "control over its properties",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_controller_get_active,
      .setter.boolean_type = rig_controller_set_active,
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .animatable = true },
    { /* This property supersedes the "active" property and is used by
         the editor to suspend controllers in edit-mode without the
         risk of inadvertantly triggering bindings if it were to
         directly change the "active" property */
        .name = "suspended",
        .nick = "Suspended",
        .blurb = "Whether the controller is suspended from actively asserting "
                 "control over its properties",
        .type = RUT_PROPERTY_TYPE_BOOLEAN,
        .getter.boolean_type = rig_controller_get_suspended,
        .setter.boolean_type = rig_controller_set_suspended,
        .flags = 0, /* PRIVATE */
        .animatable = false
    },
    { .name = "auto_deactivate",
      .nick = "Auto Deactivate",
      .blurb =
          "Whether the controller deactivates on reaching a progress of 1.0",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_controller_get_auto_deactivate,
      .setter.boolean_type = rig_controller_set_auto_deactivate,
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .animatable = true },
    { .name = "loop",
      .nick = "Loop",
      .blurb = "Whether the controller progress loops",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_controller_get_loop,
      .setter.boolean_type = rig_controller_set_loop,
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .animatable = true },
    { .name = "running",
      .nick = "Running",
      .blurb = "The sequencing position is progressing over time",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_controller_get_running,
      .setter.boolean_type = rig_controller_set_running,
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .animatable = true },
    { .name = "length",
      .nick = "Length",
      .blurb = "The length over which property changes can be sequenced",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_controller_get_length,
      .setter.float_type = rig_controller_set_length,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { .name = "elapsed",
      .nick = "Elapsed",
      .blurb = "The current sequencing position, between 0 and Length",
      .type = RUT_PROPERTY_TYPE_DOUBLE,
      .getter.double_type = rig_controller_get_elapsed,
      .setter.double_type = rig_controller_set_elapsed,
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .animatable = true },
    { .name = "progress",
      .nick = "Progress",
      .blurb = "The current sequencing position, between 0 and 1",
      .type = RUT_PROPERTY_TYPE_DOUBLE,
      .getter.double_type = rig_controller_get_progress,
      .setter.double_type = rig_controller_set_progress,
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .animatable = true },
    { 0 }
};

static void
_rig_controller_free(rut_object_t *object)
{
    rig_controller_t *controller = object;

    rut_closure_list_disconnect_all_FIXME(&controller->operation_cb_list);

    rut_introspectable_destroy(controller);

    c_hash_table_destroy(controller->properties);

    rut_object_unref(controller->shell);

    c_free(controller->label);

    rut_object_unref(controller->timeline);

    rut_object_free(rig_controller_t, controller);
}

void
rig_controller_reap(rig_controller_t *controller, rig_engine_t *engine)
{
    /* Currently controllers don't own any objects that need to be
     * explicitly reaped
     */

    rig_engine_queue_delete(engine, controller);
}

rut_type_t rig_controller_type;

static void
_rig_controller_type_init(void)
{

    rut_type_t *type = &rig_controller_type;
#define TYPE rig_controller_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_controller_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

static void
free_prop_data_cb(void *user_data)
{
    rig_controller_prop_data_t *prop_data = user_data;

    if (prop_data->path)
        rut_object_unref(prop_data->path);

    if (prop_data->binding)
        rut_object_unref(prop_data->binding);

    rut_boxed_destroy(&prop_data->constant_value);

    c_slice_free(rig_controller_prop_data_t, prop_data);
}

rig_controller_t *
rig_controller_new(rig_engine_t *engine, const char *label)
{
    rig_controller_t *controller = rut_object_alloc0(
        rig_controller_t, &rig_controller_type, _rig_controller_type_init);
    rig_timeline_t *timeline;

    controller->label = c_strdup(label);

    controller->engine = engine;
    controller->shell = engine->shell;
    timeline = rig_timeline_new(engine, 0);
    rig_timeline_stop(timeline);
    controller->timeline = timeline;

    c_list_init(&controller->operation_cb_list);

    rut_introspectable_init(
        controller, _rig_controller_prop_specs, controller->props);

    controller->properties = c_hash_table_new_full(c_direct_hash,
                                                   c_direct_equal,
                                                   NULL, /* key_destroy */
                                                   free_prop_data_cb);

    rut_property_set_copy_binding(
        &engine->shell->property_ctx,
        &controller->props[RIG_CONTROLLER_PROP_PROGRESS],
        rut_introspectable_lookup_property(timeline, "progress"));

    rut_property_set_copy_binding(
        &engine->shell->property_ctx,
        &controller->props[RIG_CONTROLLER_PROP_ELAPSED],
        rut_introspectable_lookup_property(timeline, "elapsed"));

    return controller;
}

void
rig_controller_set_label(rut_object_t *object, const char *label)
{
    rig_controller_t *controller = object;

    if (controller->label && strcmp(controller->label, label) == 0)
        return;

    c_free(controller->label);
    controller->label = c_strdup(label);
    rut_property_dirty(&controller->shell->property_ctx,
                       &controller->props[RIG_CONTROLLER_PROP_LABEL]);
}

const char *
rig_controller_get_label(rut_object_t *object)
{
    rig_controller_t *controller = object;

    return controller->label;
}

static void
dummy_binding_cb(rut_property_t *property, void *user_data)
{
}

static void
assert_path_value_cb(rut_property_t *property, void *user_data)
{
    rig_controller_prop_data_t *prop_data = user_data;
    rig_controller_t *controller = prop_data->controller;
    rut_property_t *progress_prop =
        &controller->props[RIG_CONTROLLER_PROP_PROGRESS];
    float progress = rut_property_get_double(progress_prop);

    c_return_if_fail(prop_data->method == RIG_CONTROLLER_METHOD_PATH);
    c_return_if_fail(prop_data->path);

    rig_path_lerp_property(prop_data->path, prop_data->property, progress);
}

static void
activate_property_binding(rig_controller_prop_data_t *prop_data,
                          void *user_data)
{
    rig_controller_t *controller = user_data;
    rut_property_t *property = prop_data->property;

    if (property->binding) {
        /* FIXME: we should find a way of reporting this to the
         * user when running in an editor!! */
        char *debug_name =
            rig_engine_get_object_debug_name(prop_data->property->object);
        c_warning("Controller collision for \"%s\" property on %s",
                  prop_data->property->spec->name,
                  debug_name);
        c_free(debug_name);

        return;
    }

    switch (prop_data->method) {
    case RIG_CONTROLLER_METHOD_CONSTANT: {
        rut_property_t *active_prop;

        /* Even though we are only asserting the property's constant
         * value once on activate, we add a binding for the property
         * so we can block conflicting bindings being set while this
         * controller is active...
         *
         * FIXME: We should probably not make the dummy binding depend
         * on the active property since it may lead to a lot of
         * redundant callbacks when activating/deactivating
         * controllers.
         */
        active_prop = &controller->props[RIG_CONTROLLER_PROP_ACTIVE];
        rut_property_set_binding(property,
                                 dummy_binding_cb,
                                 prop_data,
                                 active_prop,
                                 NULL); /* sentinal */

        rut_property_set_boxed(&controller->shell->property_ctx,
                               property,
                               &prop_data->constant_value);
        break;
    }
    case RIG_CONTROLLER_METHOD_PATH: {
        rut_property_t *progress_prop =
            &controller->props[RIG_CONTROLLER_PROP_PROGRESS];

        rut_property_set_binding(property,
                                 assert_path_value_cb,
                                 prop_data,
                                 progress_prop,
                                 NULL); /* sentinal */
        break;
    }
    case RIG_CONTROLLER_METHOD_BINDING:
        if (prop_data->binding)
            rig_binding_activate(prop_data->binding);
        break;
    }

    prop_data->active = true;
}

static void
deactivate_property_binding(rig_controller_prop_data_t *prop_data,
                            void *user_data)
{
    if (!prop_data->active)
        return;

    switch (prop_data->method) {
    case RIG_CONTROLLER_METHOD_CONSTANT:
    case RIG_CONTROLLER_METHOD_PATH:
        rut_property_remove_binding(prop_data->property);
        break;
    case RIG_CONTROLLER_METHOD_BINDING:
        if (prop_data->binding)
            rig_binding_deactivate(prop_data->binding);
        break;
    }

    prop_data->active = false;
}

static bool
effective_active(rig_controller_t *controller)
{
    return controller->active && !controller->suspended;
}

static void
update_effective_active_state(rig_controller_t *controller)
{
    if (effective_active(controller)) {
        rig_controller_foreach_property(
            controller, activate_property_binding, controller);
    } else {
        rig_controller_foreach_property(
            controller, deactivate_property_binding, controller);
    }
}

void
rig_controller_set_active(rut_object_t *object, bool active)
{
    rig_controller_t *controller = object;

    if (controller->active == active)
        return;

    controller->active = active;

    update_effective_active_state(controller);

    rut_property_dirty(&controller->shell->property_ctx,
                       &controller->props[RIG_CONTROLLER_PROP_ACTIVE]);
}

bool
rig_controller_get_active(rut_object_t *object)
{
    rig_controller_t *controller = object;

    return controller->active;
}

void
rig_controller_set_suspended(rut_object_t *object, bool suspended)
{
    rig_controller_t *controller = object;

    if (controller->suspended == suspended)
        return;

    controller->suspended = suspended;

    update_effective_active_state(controller);

    rut_property_dirty(&controller->shell->property_ctx,
                       &controller->props[RIG_CONTROLLER_PROP_SUSPENDED]);
}

bool
rig_controller_get_suspended(rut_object_t *object)
{
    rig_controller_t *controller = object;

    return controller->suspended;
}

void
rig_controller_set_auto_deactivate(rut_object_t *object,
                                   bool auto_deactivate)
{
    rig_controller_t *controller = object;

    if (controller->auto_deactivate == auto_deactivate)
        return;

    controller->auto_deactivate = auto_deactivate;

    rut_property_dirty(&controller->shell->property_ctx,
                       &controller->props[RIG_CONTROLLER_PROP_AUTO_DEACTIVATE]);
}

bool
rig_controller_get_auto_deactivate(rut_object_t *object)
{
    rig_controller_t *controller = object;

    return controller->auto_deactivate;
}

void
rig_controller_set_loop(rut_object_t *object, bool loop)
{
    rig_controller_t *controller = object;

    if (rig_timeline_get_loop_enabled(controller->timeline) == loop)
        return;

    rig_timeline_set_loop_enabled(controller->timeline, loop);

    rut_property_dirty(&controller->shell->property_ctx,
                       &controller->props[RIG_CONTROLLER_PROP_LOOP]);
}

bool
rig_controller_get_loop(rut_object_t *object)
{
    rig_controller_t *controller = object;

    return rig_timeline_get_loop_enabled(controller->timeline);
}

void
rig_controller_set_running(rut_object_t *object, bool running)
{
    rig_controller_t *controller = object;

    if (rig_timeline_is_running(controller->timeline) == running)
        return;

    rig_timeline_set_running(controller->timeline, running);

    rut_property_dirty(&controller->shell->property_ctx,
                       &controller->props[RIG_CONTROLLER_PROP_RUNNING]);
}

bool
rig_controller_get_running(rut_object_t *object)
{
    rig_controller_t *controller = object;

    return rig_timeline_is_running(controller->timeline);
}

void
rig_controller_set_length(rut_object_t *object, float length)
{
    rig_controller_t *controller = object;

    if (rig_timeline_get_length(controller->timeline) == length)
        return;

    rig_timeline_set_length(controller->timeline, length);

    rut_property_dirty(&controller->shell->property_ctx,
                       &controller->props[RIG_CONTROLLER_PROP_LENGTH]);
}

float
rig_controller_get_length(rut_object_t *object)
{
    rig_controller_t *controller = object;

    return rig_timeline_get_length(controller->timeline);
}

void
rig_controller_set_elapsed(rut_object_t *object, double elapsed)
{
    rig_controller_t *controller = object;
    double prev_elapsed;

    if (controller->elapsed == elapsed)
        return;

    prev_elapsed = controller->elapsed;

    rig_timeline_set_elapsed(controller->timeline, elapsed);

    /* NB: the timeline will validate the elapsed value to make sure it
     * isn't out of bounds, considering the length of the timeline.
     */
    controller->elapsed = rig_timeline_get_elapsed(controller->timeline);

    if (controller->elapsed == prev_elapsed)
        return;

    rut_property_dirty(&controller->shell->property_ctx,
                       &controller->props[RIG_CONTROLLER_PROP_ELAPSED]);
    rut_property_dirty(&controller->shell->property_ctx,
                       &controller->props[RIG_CONTROLLER_PROP_PROGRESS]);
}

double
rig_controller_get_elapsed(rut_object_t *object)
{
    rig_controller_t *controller = object;

    return rig_timeline_get_elapsed(controller->timeline);
}

void
rig_controller_set_progress(rut_object_t *object, double progress)
{
    rig_controller_t *controller = object;

    float length = rig_controller_get_length(controller);
    rig_controller_set_elapsed(object, length * progress);
}

double
rig_controller_get_progress(rut_object_t *object)
{
    rig_controller_t *controller = object;

    return rig_timeline_get_progress(controller->timeline);
}

rig_controller_prop_data_t *
rig_controller_find_prop_data_for_property(rig_controller_t *controller,
                                           rut_property_t *property)
{
    return c_hash_table_lookup(controller->properties, property);
}

rig_path_t *
rig_controller_find_path(rig_controller_t *controller,
                         rut_property_t *property)
{
    rig_controller_prop_data_t *prop_data;

    prop_data =
        rig_controller_find_prop_data_for_property(controller, property);

    return prop_data ? prop_data->path : NULL;
}

rig_path_t *
rig_controller_get_path_for_prop_data(rig_controller_t *controller,
                                      rig_controller_prop_data_t *prop_data)
{
    if (prop_data->path == NULL) {
        rig_path_t *path =
            rig_path_new(controller->shell, prop_data->property->spec->type);
        rig_controller_set_property_path(controller, prop_data->property, path);
    }

    return prop_data->path;
}

rig_path_t *
rig_controller_get_path_for_property(rig_controller_t *controller,
                                     rut_property_t *property)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);

    if (!prop_data)
        return NULL;

    return rig_controller_get_path_for_prop_data(controller, prop_data);
}

rig_binding_t *
rig_controller_get_binding_for_prop_data(rig_controller_t *controller,
                                         rig_controller_prop_data_t *prop_data)
{
    if (prop_data->binding == NULL) {
        rig_engine_t *engine = controller->engine;
        rig_binding_t *binding;

        c_return_val_if_fail(engine->frontend, NULL);

        binding = rig_binding_new(engine, prop_data->property,
                                  engine->next_code_id++);
        rig_controller_set_property_binding(
            controller, prop_data->property, binding);
    }

    return prop_data->binding;
}

rig_controller_prop_data_t *
rig_controller_find_prop_data(rig_controller_t *controller,
                              rut_object_t *object,
                              const char *property_name)
{
    rut_property_t *property =
        rut_introspectable_lookup_property(object, property_name);

    if (!property)
        return NULL;

    return rig_controller_find_prop_data_for_property(controller, property);
}

rig_path_t *
rig_controller_get_path(rig_controller_t *controller,
                        rut_object_t *object,
                        const char *property_name)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data(controller, object, property_name);

    if (!prop_data)
        return NULL;

    return rig_controller_get_path_for_prop_data(controller, prop_data);
}

typedef struct {
    rig_controller_property_iter_func_t callback;
    void *user_data;
} foreach_path_data_t;

static void
foreach_path_cb(void *key, void *value, void *user_data)
{
    rig_controller_prop_data_t *prop_data = value;
    foreach_path_data_t *foreach_data = user_data;

    foreach_data->callback(prop_data, foreach_data->user_data);
}

void
rig_controller_foreach_property(rig_controller_t *controller,
                                rig_controller_property_iter_func_t callback,
                                void *user_data)
{
    foreach_path_data_t foreach_data;

    foreach_data.callback = callback;
    foreach_data.user_data = user_data;

    c_hash_table_foreach(
        controller->properties, foreach_path_cb, &foreach_data);
}

rut_closure_t *
rig_controller_add_operation_callback(
    rig_controller_t *controller,
    rig_controller_operation_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb)
{
    return rut_closure_list_add_FIXME(
        &controller->operation_cb_list, callback, user_data, destroy_cb);
}

void
rig_controller_add_property(rig_controller_t *controller,
                            rut_property_t *property)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);

    if (prop_data)
        return;

    prop_data = c_slice_new0(rig_controller_prop_data_t);
    prop_data->controller = controller;
    prop_data->method = RIG_CONTROLLER_METHOD_CONSTANT;
    prop_data->property = property;

    rut_property_box(property, &prop_data->constant_value);

    c_hash_table_insert(controller->properties, property, prop_data);

    if (effective_active(controller))
        activate_property_binding(prop_data, controller);

    rut_closure_list_invoke(&controller->operation_cb_list,
                            rig_controller_operation_callback_t,
                            controller,
                            RIG_CONTROLLER_OPERATION_ADDED,
                            prop_data);
}

void
rig_controller_remove_property(rig_controller_t *controller,
                               rut_property_t *property)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);

    if (prop_data) {
        if (effective_active(controller))
            deactivate_property_binding(prop_data, controller);

        rut_closure_list_invoke(&controller->operation_cb_list,
                                rig_controller_operation_callback_t,
                                controller,
                                RIG_CONTROLLER_OPERATION_REMOVED,
                                prop_data);

        c_hash_table_remove(controller->properties, property);
    }
}

void
rig_controller_set_property_method(rig_controller_t *controller,
                                   rut_property_t *property,
                                   rig_controller_method_t method)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);

    c_return_if_fail(prop_data != NULL);

    if (prop_data->method == method)
        return;

    if (effective_active(controller)) {
        deactivate_property_binding(prop_data, controller);

        /* XXX: only update the method after deactivating the current
         * binding */
        prop_data->method = method;
        activate_property_binding(prop_data, controller);
    } else
        prop_data->method = method;

    rut_closure_list_invoke(&controller->operation_cb_list,
                            rig_controller_operation_callback_t,
                            controller,
                            RIG_CONTROLLER_OPERATION_METHOD_CHANGED,
                            prop_data);
}

void
rig_controller_set_property_constant(rig_controller_t *controller,
                                     rut_property_t *property,
                                     rut_boxed_t *boxed_value)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);

    c_return_if_fail(prop_data != NULL);

    rut_boxed_destroy(&prop_data->constant_value);
    rut_boxed_copy(&prop_data->constant_value, boxed_value);

    if (effective_active(controller) &&
        prop_data->method == RIG_CONTROLLER_METHOD_CONSTANT) {
        rut_property_set_boxed(&controller->shell->property_ctx,
                               prop_data->property,
                               boxed_value);
    }
}

void
rig_controller_set_property_path(rig_controller_t *controller,
                                 rut_property_t *property,
                                 rig_path_t *path)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);

    c_return_if_fail(prop_data != NULL);

    if (prop_data->path) {
        // rut_closure_disconnect_FIXME (prop_data->path_change_closure);
        rut_object_unref(prop_data->path);
    }

    prop_data->path = rut_object_ref(path);
#warning "FIXME: what if this changes the length of the controller?"

    if (effective_active(controller) &&
        prop_data->method == RIG_CONTROLLER_METHOD_PATH) {
        assert_path_value_cb(prop_data->property, prop_data);
    }
}

void
rig_controller_set_property_binding(rig_controller_t *controller,
                                    rut_property_t *property,
                                    rig_binding_t *binding)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);
    bool need_activate = false;

    c_return_if_fail(prop_data != NULL);

    if (effective_active(controller) &&
        prop_data->method == RIG_CONTROLLER_METHOD_BINDING) {
        need_activate = true;
        deactivate_property_binding(prop_data, controller);
    }

    if (prop_data->binding)
        rut_object_unref(prop_data->binding);

    prop_data->binding = rut_object_ref(binding);

    if (need_activate)
        activate_property_binding(prop_data, controller);
}

typedef struct _foreach_node_state_t {
    rig_controller_node_callback_t callback;
    void *user_data;
} foreach_node_state_t;

static void
prop_data_foreach_node(rig_controller_prop_data_t *prop_data,
                       void *user_data)
{
    if (prop_data->method == RIG_CONTROLLER_METHOD_PATH) {
        foreach_node_state_t *state = user_data;
        rig_node_t *node;

        c_list_for_each(node, &prop_data->path->nodes, list_node)
        state->callback(node, state->user_data);
    }
}

void
rig_controller_foreach_node(rig_controller_t *controller,
                            rig_controller_node_callback_t callback,
                            void *user_data)
{
    foreach_node_state_t state;

    state.callback = callback;
    state.user_data = user_data;
    rig_controller_foreach_property(controller, prop_data_foreach_node, &state);
}

static void
find_max_t_cb(rig_node_t *node, void *user_data)
{
    float *max_t = user_data;
    if (node->t > *max_t)
        *max_t = node->t;
}

typedef struct _update_state_t {
    float prev_length;
    float new_length;
} update_state_t;

static void
re_normalize_t_cb(rig_node_t *node, void *user_data)
{
    update_state_t *state = user_data;
    node->t *= state->prev_length;
    node->t /= state->new_length;
}

static void
zero_t_cb(rig_node_t *node, void *user_data)
{
    node->t = 0;
}

static void
update_length(rig_controller_t *controller, float new_length)
{
    update_state_t state = { .prev_length =
                                 rig_controller_get_length(controller),
                             .new_length = new_length };

#warning                                                                       \
    "fixme: setting a controller's length to 0 destroys any relative positioning of nodes!"
    /* Make sure to avoid divide by zero errors... */
    if (new_length == 0) {
        rig_controller_foreach_node(controller, zero_t_cb, &state);
        return;
    }

    rig_controller_foreach_node(controller, re_normalize_t_cb, &state);
    rig_controller_set_length(controller, state.new_length);
}

void
rig_controller_insert_path_value(rig_controller_t *controller,
                                 rut_property_t *property,
                                 float t,
                                 const rut_boxed_t *value)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);
    rig_path_t *path;
    float length;
    float normalized_t;

    c_return_if_fail(prop_data != NULL);

    path = rig_controller_get_path_for_prop_data(controller, prop_data);

    /* Or should we create a path on demand here? */
    c_return_if_fail(path != NULL);

    length = rig_controller_get_length(controller);

    if (t > length) {
        update_length(controller, t);
        length = t;
    }

    normalized_t = length ? t / length : 0;

    rig_path_insert_boxed(path, normalized_t, value);

    if (effective_active(controller) &&
        prop_data->method == RIG_CONTROLLER_METHOD_PATH) {
        assert_path_value_cb(property, prop_data);
    }
}

void
rig_controller_box_path_value(rig_controller_t *controller,
                              rut_property_t *property,
                              float t,
                              rut_boxed_t *out)
{
    rig_path_t *path =
        rig_controller_get_path_for_property(controller, property);

    float length = rig_controller_get_length(controller);
    float normalized_t;
    rig_node_t *node;
    bool status;

    c_return_if_fail(path != NULL);

    normalized_t = length ? t / length : 0;

    node = rig_path_find_nearest(path, normalized_t);

    c_return_if_fail(node != NULL);

    status = rig_node_box(path->type, node, out);

    c_return_if_fail(status);
}

void
rig_controller_remove_path_value(rig_controller_t *controller,
                                 rut_property_t *property,
                                 float t)
{
    rig_controller_prop_data_t *prop_data =
        rig_controller_find_prop_data_for_property(controller, property);
    rig_path_t *path;
    float length;
    float normalized_t;
    rig_node_t *node;

    c_return_if_fail(prop_data != NULL);

    path = rig_controller_get_path_for_prop_data(controller, prop_data);

    c_return_if_fail(path != NULL);

    length = rig_controller_get_length(controller);

    normalized_t = length ? t / length : 0;

    node = rig_path_find_nearest(path, normalized_t);

    c_return_if_fail(node != NULL);

    normalized_t = node->t;

    rig_path_remove_node(path, node);

    if (t > (length - 1e-3) || t < (length + 1e-3)) {
        float max_t;

        rig_controller_foreach_node(controller, find_max_t_cb, &max_t);

        if (max_t < normalized_t)
            update_length(controller, max_t * length);
    }

    if (effective_active(controller) &&
        prop_data->method == RIG_CONTROLLER_METHOD_PATH) {
        assert_path_value_cb(property, prop_data);
    }
}
