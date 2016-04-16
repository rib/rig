/*
 * Rig C
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2015  Intel Corporation.
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

#include <rut.h>

#include "rig-c.h"
#include "rig-engine.h"
#include "rig-code-module.h"
#include "rig-curses-debug.h"
#include "rig-entity.h"

#include "components/rig-shape.h"
#include "components/rig-nine-slice.h"
#include "components/rig-diamond.h"
#include "components/rig-pointalism-grid.h"
#include "components/rig-material.h"
#include "components/rig-button-input.h"
#include "components/rig-text.h"
#include "components/rig-native-module.h"

RInputEventType
r_input_event_get_type(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    switch (rut_input_event_get_type(rut_event)) {
    case RUT_INPUT_EVENT_TYPE_MOTION:
        return R_INPUT_EVENT_TYPE_MOTION;
    case RUT_INPUT_EVENT_TYPE_KEY:
        return R_INPUT_EVENT_TYPE_KEY;
    case RUT_INPUT_EVENT_TYPE_TEXT:
        return R_INPUT_EVENT_TYPE_TEXT;
    default:
        c_return_val_if_reached(0);
    }
}

RKeyEventAction
r_key_event_get_action(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    switch (rut_key_event_get_action(rut_event)) {
    case RUT_KEY_EVENT_ACTION_UP:
        return RUT_KEY_EVENT_ACTION_UP;
    case RUT_KEY_EVENT_ACTION_DOWN:
        return RUT_KEY_EVENT_ACTION_DOWN;
    default:
        c_return_val_if_reached(0);
    }
}

int32_t
r_key_event_get_keysym(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return rut_key_event_get_keysym(rut_event);
}

RModifierState
r_key_event_get_modifier_state(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return (RModifierState)rut_key_event_get_modifier_state(rut_event);
}

RMotionEventAction
r_motion_event_get_action(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    switch (rut_motion_event_get_action(rut_event)) {
    case R_MOTION_EVENT_ACTION_UP:
        return RUT_MOTION_EVENT_ACTION_UP;
    case R_MOTION_EVENT_ACTION_DOWN:
        return RUT_MOTION_EVENT_ACTION_DOWN;
    case R_MOTION_EVENT_ACTION_MOVE:
        return RUT_MOTION_EVENT_ACTION_MOVE;
    default:
        c_return_val_if_reached(0);
    }
}

RButtonState
r_motion_event_get_button(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    switch (rut_motion_event_get_button(rut_event)) {
    case RUT_BUTTON_STATE_1:
        return R_BUTTON_STATE_1;
    case RUT_BUTTON_STATE_2:
        return R_BUTTON_STATE_2;
    case RUT_BUTTON_STATE_3:
        return R_BUTTON_STATE_3;
    default:
        c_return_val_if_reached(0);
    }
}

RButtonState
r_motion_event_get_button_state(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return (RButtonState)rut_motion_event_get_button_state(rut_event);
}

RModifierState
r_motion_event_get_modifier_state(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return (RModifierState)rut_motion_event_get_modifier_state(rut_event);
}

float
r_motion_event_get_x(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return rut_motion_event_get_x(rut_event);
}

float
r_motion_event_get_y(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return rut_motion_event_get_y(rut_event);
}

static void
_r_debugv(RModule *module,
          const char *format,
          va_list args)
{
    c_logv(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_DEBUG, format, args);
}

void
r_debug(RModule *module,
        const char *format,
        ...)
{
    va_list args;

    va_start(args, format);
    _r_debugv(module, format, args);
    va_end(args);
}

RObject *
r_find(RModule *module, const char *name)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_ui_t *ui = code_module->engine->ui;

    return (RObject *)rig_ui_find_entity(ui, name);
}

RObject *
r_entity_new(RModule *module, RObject *parent)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_entity_t *entity;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;

    prop_ctx->logging_disabled++;
    entity = rig_entity_new(engine);
    prop_ctx->logging_disabled--;

    /* Entities and components have to be explicitly deleted
     * via r_entity_delete() or r_component_delete(). We
     * give the engine ownership of the only reference. We
     * don't expose a ref count in the C binding api.
     */
    rut_object_claim(entity, engine);
    rut_object_unref(entity);

    if (!parent)
        parent = engine->ui->scene;

    rig_engine_op_add_entity(engine, (rig_entity_t *)parent, entity);

    return (RObject *)entity;
}

static RObject *
entity_clone_under(RModule *module, rig_entity_t *entity, rig_entity_t *parent)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rig_entity_t *clone;
    c_ptr_array_t *entity_components = entity->components;
    rut_graphable_props_t *graph_props =
        rut_object_get_properties(entity, RUT_TRAIT_ID_GRAPHABLE);
    rut_queue_item_t *item;
    int i;

    prop_ctx->logging_disabled++;
    clone = rig_entity_copy_shallow(entity);
    prop_ctx->logging_disabled--;

    /* Entities and components have to be explicitly deleted
     * via r_entity_delete() or r_component_delete(). We
     * give the engine ownership of the only reference. We
     * don't expose a ref count in the C binding api.
     */
    rut_object_claim(clone, engine);
    rut_object_unref(clone);

    rig_engine_op_add_entity(engine, parent, clone);

    for (i = 0; i < entity_components->len; i++) {
        rut_object_t *component = c_ptr_array_index(entity_components, i);
        rut_componentable_vtable_t *componentable =
            rut_object_get_vtable(component, RUT_TRAIT_ID_COMPONENTABLE);
        rut_object_t *component_clone;

        prop_ctx->logging_disabled++;
        component_clone = componentable->copy(component);
        prop_ctx->logging_disabled--;

        rut_object_claim(component_clone, engine);
        rut_object_unref(component_clone);

        rig_engine_op_register_component(engine, component_clone);

        rig_engine_op_add_component(code_module->engine,
                                    clone,
                                    component_clone);
    }

    c_list_for_each(item, &graph_props->children.items, list_node) {
        rut_object_t *child = item->data;

#ifdef RIG_ENABLE_DEBUG
        if (rut_object_get_type(child) != &rig_entity_type) {
            c_warn_if_reached();
            continue;
        }
#endif

        prop_ctx->logging_disabled++;
        entity_clone_under(module, child, clone);
        prop_ctx->logging_disabled--;
    }

    return (RObject *)clone;
}

RObject *
r_entity_clone(RModule *module, RObject *entity)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;

    return entity_clone_under(module, (rig_entity_t *)entity, engine->ui->scene);
}

void
r_entity_delete(RModule *module, RObject *entity)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;

    rig_engine_op_delete_entity(engine, (rut_object_t *)entity);

    rut_object_release(entity, engine);
}

void
r_entity_translate(RModule *module, RObject *entity, float tx, float ty, float tz)
{
    rig_entity_t *e = (rig_entity_t *)entity;
    const float *cur = rig_entity_get_position(e);
    float pos[3] = { cur[0] + tx, cur[1] + ty, cur[2] + tz };

    r_set_vec3(module, entity, RUT_ENTITY_PROP_POSITION, pos);
}

void
r_entity_rotate_x_axis(RModule *module, RObject *entity, float x_angle)
{
    rig_entity_t *e = (rig_entity_t *)entity;
    c_quaternion_t x_rotation;
    c_quaternion_t q = *rig_entity_get_rotation(e);

    c_quaternion_init_from_x_rotation(&x_rotation, x_angle);
    c_quaternion_multiply(&q, &q, &x_rotation);

    r_set_quaternion(module, entity, RUT_ENTITY_PROP_ROTATION, (RQuaternion *)&q);
}

void
r_entity_rotate_y_axis(RModule *module, RObject *entity, float y_angle)
{
    rig_entity_t *e = (rig_entity_t *)entity;
    c_quaternion_t y_rotation;
    c_quaternion_t q = *rig_entity_get_rotation(e);

    c_quaternion_init_from_y_rotation(&y_rotation, y_angle);
    c_quaternion_multiply(&q, &q, &y_rotation);

    r_set_quaternion(module, entity, RUT_ENTITY_PROP_ROTATION, (RQuaternion *)&q);
}

void
r_entity_rotate_z_axis(RModule *module, RObject *entity, float z_angle)
{
    rig_entity_t *e = (rig_entity_t *)entity;
    c_quaternion_t z_rotation;
    c_quaternion_t q = *rig_entity_get_rotation(e);

    c_quaternion_init_from_z_rotation(&z_rotation, z_angle);
    c_quaternion_multiply(&q, &q, &z_rotation);

    r_set_quaternion(module, entity, RUT_ENTITY_PROP_ROTATION, (RQuaternion *)&q);
}

void
r_component_delete(RModule *module, RObject *component)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;

    rig_engine_op_delete_component(engine, (rut_object_t *)component);
    rut_object_release(component, engine);
}

void
r_request_animation_frame(RModule *module)
{
    rig_code_module_props_t *code_module = (void *)module;

    rut_shell_queue_redraw(code_module->engine->shell);
}

RObject *
r_camera_new(RModule *module)
{
    rig_code_module_props_t *code_module = (void *)module;
    rut_object_t *component;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;

    prop_ctx->logging_disabled++;
    component = rig_camera_new(engine,
                               1, /* ortho width */
                               1, /* ortho height */
                               NULL); /* fb */
#warning "FIXME: sync aspect ratio of camera from frontend to simulator"
    prop_ctx->logging_disabled--;

    /* Entities and components have to be explicitly deleted
     * via r_entity_delete() or r_component_delete(). We
     * give the engine ownership of the only reference. We
     * don't expose a ref count in the C binding api.
     */
    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

RObject *
r_view_new(RModule *module)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rig_view_t *view;

    prop_ctx->logging_disabled++;
    view = rig_view_new(engine);
    prop_ctx->logging_disabled--;

    /* Views have to be explicitly deleted via r_view_delete(). We
     * give the engine ownership of the only reference. We don't
     * expose a ref count in the C binding api.
     */
    rut_object_claim(view, engine);
    rut_object_unref(view);

    rig_engine_op_add_view(engine, view);

    return (RObject *)view;
}

void
r_view_delete(RModule *module, RObject *view)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;

    rig_engine_op_delete_view(engine, (rig_view_t *)view);

    rut_object_release(view, engine);
}

RObject *
r_controller_new(RModule *module, const char *name)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rig_controller_t *controller;

    prop_ctx->logging_disabled++;
    controller = rig_controller_new(engine, name);
    prop_ctx->logging_disabled--;

    /* Controllers have to be explicitly deleted via
     * r_controller_delete(). We give the engine ownership of the only
     * reference. We don't expose a ref count in the C binding api.
     */
    rut_object_claim(controller, engine);
    rut_object_unref(controller);

    rig_engine_op_add_controller(engine, controller);

    return (RObject *)controller;
}

void
r_controller_delete(RModule *module, RObject *controller)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;

    rig_engine_op_delete_controller(engine, (rig_controller_t *)controller);

    rut_object_release(controller, engine);
}

void
r_controller_bind(RModule *module, RObject *controller,
                  RObject *dst_obj, const char *dst_prop_name,
                  RObject *src_obj, const char *src_prop_name)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_t *dst_prop;
    rig_property_t *src_prop;
    rig_binding_t *binding;

    c_return_if_fail(rut_object_is(dst_obj, RUT_TRAIT_ID_INTROSPECTABLE));
    c_return_if_fail(rut_object_is(src_obj, RUT_TRAIT_ID_INTROSPECTABLE));

    dst_prop = rig_introspectable_lookup_property(dst_obj, dst_prop_name);
    src_prop = rig_introspectable_lookup_property(src_obj, src_prop_name);

    binding = rig_binding_new_simple_copy(engine, dst_prop, src_prop);

    rig_controller_add_property((rig_controller_t *)controller,
                                dst_prop);
    rig_controller_set_property_method((rig_controller_t *)controller,
                                       dst_prop,
                                       RIG_CONTROLLER_METHOD_BINDING);
    rig_controller_set_property_binding((rig_controller_t *)controller,
                                        dst_prop,
                                        binding);
}

RObject *
r_light_new(RModule *module)
{
    rig_code_module_props_t *code_module = (void *)module;
    rut_object_t *component;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;

    prop_ctx->logging_disabled++;
    component = rig_light_new(engine);
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

RObject *
r_shape_new(RModule *module, float width, float height)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rut_object_t *component;

    prop_ctx->logging_disabled++;
    component = rig_shape_new(code_module->engine, false, width, height);
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

RObject *
r_nine_slice_new(RModule *module,
                 float top, float right, float bottom, float left,
                 float width, float height)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rut_object_t *component;

    prop_ctx->logging_disabled++;
    component = rig_nine_slice_new(engine,
                                   top, right, bottom, left,
                                   width, right);
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

RObject *
r_diamond_new(RModule *module, float size)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rut_object_t *component;

    prop_ctx->logging_disabled++;
    component = rig_diamond_new(engine, size);
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

RObject *
r_pointalism_grid_new(RModule *module, float size)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rut_object_t *component;

    prop_ctx->logging_disabled++;
    component = rig_pointalism_grid_new(engine, size);
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

RObject *
r_material_new(RModule *module)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rut_object_t *component;

    prop_ctx->logging_disabled++;
    component = rig_material_new(engine);
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

RObject *
r_source_new(RModule *module, const char *url)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rut_object_t *component;

    prop_ctx->logging_disabled++;
    component = rig_source_new(engine,
                               NULL, /* mime */
                               url, /* url */
                               NULL, /* data */
                               0, /* data_len */
                               0, /* natural width */
                               0); /* natural height */
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

RObject *
r_button_input_new(RModule *module)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rut_object_t *component;

    prop_ctx->logging_disabled++;
    component = rig_button_input_new(engine);
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

RObject *
r_text_new(RModule *module)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rut_object_t *component;

    prop_ctx->logging_disabled++;
    component = rig_text_new(engine);
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(code_module->engine, component);

    return (RObject *)component;
}

void
r_add_component(RModule *module, RObject *entity, RObject *component)
{
    rig_code_module_props_t *code_module = (void *)module;

    rig_engine_op_add_component(code_module->engine,
                                (rig_entity_t *)entity,
                                (rig_entity_t *)component);
}

RColor *
r_color_init_from_string(RModule *module, RColor *color, const char *str)
{
    rig_code_module_props_t *code_module = (void *)module;
    rut_color_init_from_string(code_module->engine->shell, (void *)color, str);
    return color;
}

#if 0
void
r_set_float_by_name(RModule *module, RObject *object, const char *name, float value)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_t *prop;

    c_return_if_fail(rut_object_is(object, RUT_TRAIT_ID_INTROSPECTABLE));

    prop = rig_introspectable_lookup_property(object, name);

    c_return_if_fail(prop);

#if 1
    rut_boxed_t boxed;
    boxed.type = RUT_PROPERTY_TYPE_FLOAT;
    boxed.d.float_val = value;
    rig_engine_op_set_property(engine, prop, &boxed);
#else
    rig_property_set_float(&engine->_property_ctx, prop, value);
#endif
}
#endif

// E.g. boolean bool BOOLEAN
#define SCALAR_TYPE(SUFFIX, CTYPE, TYPE) \
void \
r_set_##SUFFIX(RModule *module, RObject *object, int id, CTYPE value) \
{ \
    rig_code_module_props_t *code_module = (void *)module; \
    rig_engine_t *engine = code_module->engine; \
    rig_introspectable_props_t *priv = \
        rut_object_get_properties(object, RUT_TRAIT_ID_INTROSPECTABLE); \
    rig_property_t *prop = priv->first_property + id; \
 \
    c_return_if_fail(id < priv->n_properties); \
 \
    rig_property_set_##SUFFIX(engine->property_ctx, prop, value); \
} \
 \
void \
r_set_##SUFFIX##_by_name(RModule *module, RObject *object, const char *name, CTYPE value) \
{ \
    rig_code_module_props_t *code_module = (void *)module; \
    rig_engine_t *engine = code_module->engine; \
    rig_property_t *prop; \
 \
    c_return_if_fail(rut_object_is(object, RUT_TRAIT_ID_INTROSPECTABLE)); \
 \
    prop = rig_introspectable_lookup_property(object, name); \
 \
    rig_property_set_##SUFFIX(engine->property_ctx, prop, value); \
}

#define POINTER_TYPE(SUFFIX, CTYPE, TYPE)
#define _POINTER_TYPE(SUFFIX, CTYPE, TYPE) SCALAR_TYPE(SUFFIX, CTYPE, TYPE)

#define ARRAY_TYPE(SUFFIX, CTYPE, TYPE, LEN) \
void \
r_set_##SUFFIX(RModule *module, RObject *object, int id, const CTYPE value[LEN]) \
{ \
    rig_code_module_props_t *code_module = (void *)module; \
    rig_engine_t *engine = code_module->engine; \
    rig_introspectable_props_t *priv = \
        rut_object_get_properties(object, RUT_TRAIT_ID_INTROSPECTABLE); \
    rig_property_t *prop = priv->first_property + id; \
 \
    c_return_if_fail(id < priv->n_properties); \
 \
    rig_property_set_##SUFFIX(engine->property_ctx, prop, value); \
} \
 \
void \
r_set_##SUFFIX##_by_name(RModule *module, RObject *object, const char *name, const CTYPE value[LEN]) \
{ \
    rig_code_module_props_t *code_module = (void *)module; \
    rig_engine_t *engine = code_module->engine; \
    rig_property_t *prop; \
 \
    c_return_if_fail(rut_object_is(object, RUT_TRAIT_ID_INTROSPECTABLE)); \
 \
    prop = rig_introspectable_lookup_property(object, name); \
 \
    rig_property_set_##SUFFIX(engine->property_ctx, prop, value); \
}

#define COMPOSITE_TYPE(SUFFIX, CTYPE, TYPE)
#define _COMPOSITE_TYPE(SUFFIX, CTYPE, PRIV_CTYPE, TYPE) \
void \
r_set_##SUFFIX(RModule *module, RObject *object, int id, const CTYPE *value) \
{ \
    rig_code_module_props_t *code_module = (void *)module; \
    rig_engine_t *engine = code_module->engine; \
    rig_introspectable_props_t *priv = \
        rut_object_get_properties(object, RUT_TRAIT_ID_INTROSPECTABLE); \
    rig_property_t *prop = priv->first_property + id; \
 \
    c_return_if_fail(id < priv->n_properties); \
 \
    rig_property_set_##SUFFIX(&engine->_property_ctx, prop, (PRIV_CTYPE *)value); \
} \
 \
void \
r_set_##SUFFIX##_by_name(RModule *module, RObject *object, const char *name, const CTYPE *value) \
{ \
    rig_code_module_props_t *code_module = (void *)module; \
    rig_engine_t *engine = code_module->engine; \
    rig_property_t *prop; \
 \
    c_return_if_fail(rut_object_is(object, RUT_TRAIT_ID_INTROSPECTABLE)); \
 \
    prop = rig_introspectable_lookup_property(object, name); \
 \
    c_return_if_fail(prop); \
 \
    rig_property_set_##SUFFIX(&engine->_property_ctx, prop, (PRIV_CTYPE *)value); \
}

#include "rig-property-types.h"
_COMPOSITE_TYPE(color, RColor, cg_color_t, COLOR)
_COMPOSITE_TYPE(quaternion, RQuaternion, c_quaternion_t, QUATERNION)

_POINTER_TYPE(object, RObject *, OBJECT)

void
r_set_text_by_name(RModule *module, RObject *object, const char *name, const char *value)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_property_t *prop;

    c_return_if_fail(rut_object_is(object, RUT_TRAIT_ID_INTROSPECTABLE));

    prop = rig_introspectable_lookup_property(object, name);

    c_return_if_fail(prop);

#if 1
    rut_boxed_t boxed;
    boxed.type = RUT_PROPERTY_TYPE_TEXT;
    boxed.d.text_val = (char *)value;
    rig_engine_op_set_property(engine, prop, &boxed);
#else
    rig_property_set_text(&engine->_property_ctx, prop, value);
#endif
}

void
r_set_text(RModule *module, RObject *object, int id, const char *value)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rig_introspectable_props_t *priv =
        rut_object_get_properties(object, RUT_TRAIT_ID_INTROSPECTABLE);
    rig_property_t *prop = priv->first_property + id;

    c_return_if_fail(id < priv->n_properties);

#if 1
    rut_boxed_t boxed;
    boxed.type = RUT_PROPERTY_TYPE_TEXT;
    boxed.d.text_val = (char *)value;
    rig_engine_op_set_property(engine, prop, &boxed);
#else
    rig_property_set_text(&engine->_property_ctx, prop, value);
#endif
}

RQuaternion r_quaternion_identity(void)
{
    RQuaternion q;
    c_quaternion_init_identity((c_quaternion_t *)&q);
    return q;
}

RQuaternion r_quaternion(float angle, float x, float y, float z)
{
    RQuaternion q;
    c_quaternion_init((c_quaternion_t *)&q, angle, x, y, z);
    return q;
}

RQuaternion r_quaternion_from_angle_vector(float angle,
                                           const float *axis3f)
{
    RQuaternion q;
    c_quaternion_init_from_angle_vector((c_quaternion_t *)&q, angle, axis3f);
    return q;
}

RQuaternion r_quaternion_from_array(const float *array)
{
    RQuaternion q;
    c_quaternion_init_from_array((c_quaternion_t *)&q, array);
    return q;
}

RQuaternion r_quaternion_from_x_rotation(float angle)
{
    RQuaternion q;
    c_quaternion_init_from_x_rotation((c_quaternion_t *)&q, angle);
    return q;
}

RQuaternion r_quaternion_from_y_rotation(float angle)
{
    RQuaternion q;
    c_quaternion_init_from_y_rotation((c_quaternion_t *)&q, angle);
    return q;
}

RQuaternion r_quaternion_from_z_rotation(float angle)
{
    RQuaternion q;
    c_quaternion_init_from_z_rotation((c_quaternion_t *)&q, angle);
    return q;
}

RQuaternion r_quaternion_from_euler(const REuler *euler)
{
    RQuaternion q;
    c_quaternion_init_from_euler((c_quaternion_t *)&q, (const c_euler_t *)euler);
    return q;
}

#if 0
RQuaternion r_quaternion_from_matrix(const RMatrix *matrix)
{
    RQuaternion q;
    c_quaternion_init_from_angle_vector(&q, angle, axis3f);
    return (RQuaternion)q;
}
#endif

bool r_quaternion_equal(const RQuaternion *a, const RQuaternion *b)
{
    if (a == b)
        return true;

    return (a->w == b->w && a->x == b->x && a->y == b->y && a->z == b->z);
}

float r_quaternion_get_rotation_angle(const RQuaternion *quaternion)
{
    return c_quaternion_get_rotation_angle((c_quaternion_t *)quaternion);
}

void r_quaternion_get_rotation_axis(const RQuaternion *quaternion,
                                    float *vector3)
{
    c_quaternion_get_rotation_axis((c_quaternion_t *)quaternion,
                                    vector3);
}

void r_quaternion_normalize(RQuaternion *quaternion)
{
    c_quaternion_normalize((c_quaternion_t *)quaternion);
}

void r_quaternion_invert(RQuaternion *quaternion)
{
    c_quaternion_invert((c_quaternion_t *)quaternion);
}

RQuaternion r_quaternion_multiply(const RQuaternion *left,
                                  const RQuaternion *right)
{
    RQuaternion q;
    c_quaternion_multiply((c_quaternion_t *)&q,
                          (c_quaternion_t *)left,
                          (c_quaternion_t *)right);
    return q;
}

void
r_quaternion_rotate_x_axis(RQuaternion *quaternion, float x_angle)
{
    c_quaternion_t x_rotation;

    c_quaternion_init_from_x_rotation(&x_rotation, x_angle);
    c_quaternion_multiply((c_quaternion_t *)quaternion,
                           (c_quaternion_t *)quaternion,
                           &x_rotation);
}

void
r_quaternion_rotate_y_axis(RQuaternion *quaternion, float y_angle)
{
    c_quaternion_t y_rotation;

    c_quaternion_init_from_y_rotation(&y_rotation, y_angle);
    c_quaternion_multiply((c_quaternion_t *)quaternion,
                           (c_quaternion_t *)quaternion,
                           &y_rotation);
}

void
r_quaternion_rotate_z_axis(RQuaternion *quaternion, float z_angle)
{
    c_quaternion_t z_rotation;

    c_quaternion_init_from_z_rotation(&z_rotation, z_angle);
    c_quaternion_multiply((c_quaternion_t *)quaternion,
                           (c_quaternion_t *)quaternion,
                           &z_rotation);
}

void r_quaternion_pow(RQuaternion *quaternion, float exponent)
{
    c_quaternion_pow((c_quaternion_t *)quaternion, exponent);
}

float r_quaternion_dot_product(const RQuaternion *a,
                               const RQuaternion *b)
{
    return c_quaternion_dot_product((c_quaternion_t *)a,
                                     (c_quaternion_t *)b);
}

RQuaternion r_quaternion_slerp(const RQuaternion *a,
                               const RQuaternion *b,
                               float t)
{
    RQuaternion q;
    c_quaternion_slerp((c_quaternion_t *)&q,
                       (c_quaternion_t *)a,
                       (c_quaternion_t *)b,
                       t);
    return q;
}

RQuaternion r_quaternion_nlerp(const RQuaternion *a,
                               const RQuaternion *b,
                               float t)
{
    RQuaternion q;
    c_quaternion_nlerp((c_quaternion_t *)&q,
                       (c_quaternion_t *)a,
                       (c_quaternion_t *)b,
                       t);
    return q;
}

RQuaternion r_quaternion_squad(const RQuaternion *prev,
                               const RQuaternion *a,
                               const RQuaternion *b,
                               const RQuaternion *next,
                               float t)
{
    RQuaternion q;
    c_quaternion_squad((c_quaternion_t *)&q,
                       (c_quaternion_t *)prev,
                       (c_quaternion_t *)a,
                       (c_quaternion_t *)b,
                       (c_quaternion_t *)next,
                       t);
    return q;
}

typedef struct
{
    rig_simulator_t *simulator;
    rut_closure_t init_idle;

    bool add_self_as_native_module;
    const char *native_symbol_prefix;
    int native_abi_version;

    uv_lib_t self;
} r_sim_t;

static void *
resolve_cb(const char *symbol, void *user_data)
{
#ifdef USE_UV
    r_sim_t *stub_sim = user_data;
    const char *prefix = stub_sim->native_symbol_prefix;
    char full_name[512];
    void *ptr = NULL;

    if (snprintf(full_name, sizeof(full_name), "%s%s", prefix, symbol) >= sizeof(full_name))
        return NULL;

    if (uv_dlsym(&stub_sim->self, full_name, &ptr) < 0) {
        c_warning("Error resolving symbol %s: %s",
                  full_name, uv_dlerror(&stub_sim->self));
        ptr = NULL;
    }

    return ptr;
#else
    return NULL;
#endif
}

static rig_native_module_t *
native_module_new(rig_engine_t *engine)
{
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rig_native_module_t *component;

    prop_ctx->logging_disabled++;
    component = rig_native_module_new(engine);
    prop_ctx->logging_disabled--;

    return component;
}

static void
simulator_init_cb(r_sim_t *stub_sim)
{
    rig_simulator_t *simulator = stub_sim->simulator;
    rig_engine_t *engine = simulator->engine;
    rig_property_context_t *prop_ctx = engine->property_ctx;
    rig_ui_t *ui = rig_ui_new(engine);
    rig_entity_t *root;

    /* XXX: we need to take care not to log properties
     * during these initial steps, until we call the
     * 'load' callback.
     *
     * We're assuming the property context is in its
     * initial state with logging disabled.
     *
     * It would be better if this were integrated with
     * rig-simulator-impl.c which is also responsible
     * for enabling logging before calling user's
     * 'update' code.
     */
    c_return_if_fail(prop_ctx->logging_disabled == 1);

    rig_engine_set_ui(engine, ui);
    rut_object_unref(ui);

    rig_engine_op_apply_context_set_ui(&simulator->apply_op_ctx, ui);

    root = rig_entity_new(engine);

    rig_engine_op_add_entity(engine, NULL, root);

    if (stub_sim->add_self_as_native_module) {
        if (uv_dlopen(NULL, &stub_sim->self) == 0) {
            rig_native_module_t *native_module = native_module_new(engine);

            if (!stub_sim->native_symbol_prefix)
                stub_sim->native_symbol_prefix = "";

            rig_native_module_set_resolver(native_module, resolve_cb, stub_sim);

            rig_engine_op_register_component(engine, native_module);
            rig_engine_op_add_component(engine, root, native_module);

            prop_ctx->logging_disabled--;
            rig_ui_code_modules_load(ui);
            prop_ctx->logging_disabled++;

        } else {
            c_error("Failed to add self as native module: %s",
                    uv_dlerror(&stub_sim->self));
        }
    }

    rut_closure_remove(&stub_sim->init_idle);

    c_debug("Stub Simulator Initialized");
}

struct _r_engine {
    rut_object_base_t _base;

    rut_shell_t *shell;
    rig_frontend_t *frontend;
    rig_engine_t *engine;

    char *native_symbol_prefix;
    int native_abi_version;
    bool add_self_as_native_module;

    enum rig_simulator_run_mode simulator_mode;
    char *simulator_address;
    int simulator_port;
};

static void
simulator_run(rig_simulator_t *simulator, void *user_data)
{
    r_sim_t *stub_sim = c_new0(r_sim_t, 1);

    stub_sim->simulator = simulator;

    /* user_data only passed through if sim run in same thread
     * or mainloop as frontend...*/
    if (user_data) {
        r_engine_t *stub_engine = user_data;

        stub_sim->add_self_as_native_module = stub_engine->add_self_as_native_module;
        stub_sim->native_symbol_prefix = stub_engine->native_symbol_prefix;
        stub_sim->native_abi_version = stub_engine->native_abi_version;
    }

    rut_closure_init(&stub_sim->init_idle, simulator_init_cb, stub_sim);
    rut_poll_shell_add_idle(simulator->shell, &stub_sim->init_idle);

    c_debug("Stub Simulator Started\n");
}

static void
stub_engine_shell_redraw_cb(rut_shell_t *shell, void *user_data)
{
    r_engine_t *stub_engine = user_data;
    rig_engine_t *engine = stub_engine->engine;
    rig_frontend_t *frontend = engine->frontend;

    if (frontend->connected)
        rig_frontend_start_frame(frontend);
}

static void
_r_engine_free(void *object)
{
    r_engine_t *stub_engine = object;

    rut_object_unref(stub_engine->engine);

    rut_object_unref(stub_engine->shell);

    if (stub_engine->simulator_address)
        free(stub_engine->simulator_address);

    if (stub_engine->native_symbol_prefix)
        free(stub_engine->native_symbol_prefix);

    rut_object_free(r_engine_t, stub_engine);
}

static rut_type_t r_engine_type;

static void
_r_engine_init_type(void)
{
    rut_type_init(&r_engine_type, "r_engine_t", _r_engine_free);
}

static void
stub_engine_init_cb(rut_shell_t *shell, void *user_data)
{
    r_engine_t *stub_engine = user_data;
    rig_engine_t *engine;

    stub_engine->frontend = rig_frontend_new(stub_engine->shell);

    engine = stub_engine->frontend->engine;
    stub_engine->engine = engine;

    rig_frontend_spawn_simulator(stub_engine->frontend,
                                 stub_engine->simulator_mode,
                                 stub_engine->simulator_address,
                                 stub_engine->simulator_port,
                                 simulator_run,
                                 stub_engine, /* local sim init data */
                                 NULL); /* no ui to load */

#if 0
    if (stub_engine->fullscreen) {
        rig_onscreen_view_t *onscreen_view = engine->frontend->onscreen_views->data;
        rut_shell_onscreen_t *onscreen = onscreen_view->onscreen;

        rut_shell_onscreen_set_fullscreen(onscreen, true);
    }
#endif

    //rig_frontend_set_simulator_connected_callback(
    //    engine->frontend, simulator_connected_cb, stub_engine);
}

r_engine_t *
r_engine_new(REngineConfig *config)
{
    r_engine_t *stub_engine = rut_object_alloc0(
        r_engine_t, &r_engine_type, _r_engine_init_type);

#ifdef __EMSCRIPTEN__
    enum rig_simulator_run_mode simulator_mode =
        RIG_SIMULATOR_RUN_MODE_WEB_SOCKET;
#else
    enum rig_simulator_run_mode simulator_mode =
        RIG_SIMULATOR_RUN_MODE_MAINLOOP;
#endif

    if (config->require_vr_hmd) {
        /* TODO */
    }

    stub_engine->simulator_mode = simulator_mode;
    //stub_engine->simulator_address = simulator_address ? strdup(simulator_address) : NULL;
    //stub_engine->simulator_port = simulator_port;

    stub_engine->shell = rut_shell_new(NULL, stub_engine_shell_redraw_cb, stub_engine);

#ifdef USE_NCURSES
    //rig_curses_add_to_shell(stub_engine->shell);
#endif

    rut_shell_set_on_run_callback(stub_engine->shell,
                                  stub_engine_init_cb,
                                  stub_engine);

    return stub_engine;
}

bool
r_engine_add_self_as_native_component(r_engine_t *stub_engine,
                                      int abi_version,
                                      const char *symbol_prefix)
{
    if (abi_version != 1)
        return false;

    stub_engine->add_self_as_native_module = true;
    stub_engine->native_symbol_prefix = c_strdup(symbol_prefix);
    stub_engine->native_abi_version = abi_version;

    return true;
}

void
r_engine_run(r_engine_t *stub_engine)
{
    rut_shell_main(stub_engine->shell);

    rut_object_unref(stub_engine);
}

void
r_engine_ref(r_engine_t *stub_engine)
{
    rut_object_ref(stub_engine);
}

void
r_engine_unref(r_engine_t *stub_engine)
{
    rut_object_unref(stub_engine);
}
