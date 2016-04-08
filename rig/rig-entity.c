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

#include "rut-renderer.h"

#include "rig-entity.h"
#include "rig-engine.h"
#include "rig-code-module.h"

static rig_property_spec_t _rig_entity_prop_specs[] = {
    { .name = "label",
      .type = RUT_PROPERTY_TYPE_TEXT,
      .getter.text_type = rig_entity_get_label,
      .setter.text_type = rig_entity_set_label,
      .nick = "label_t",
      .blurb = "A label for the entity",
      .flags = RUT_PROPERTY_FLAG_READWRITE },
    { .name = "parent",
      .type = RUT_PROPERTY_TYPE_OBJECT,
      .getter.object_type = rig_entity_get_parent,
      .setter.object_type = rig_entity_set_parent,
      .validation.object.type = &rig_entity_type,
      .nick = "Parent",
      .blurb = "The entity's parent",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "position",
      .type = RUT_PROPERTY_TYPE_VEC3,
      .getter.vec3_type = rig_entity_get_position,
      .setter.vec3_type = rig_entity_set_position,
      .nick = "Position",
      .blurb = "The entity's position",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "rotation",
      .type = RUT_PROPERTY_TYPE_QUATERNION,
      .getter.quaternion_type = rig_entity_get_rotation,
      .setter.quaternion_type = rig_entity_set_rotation,
      .nick = "Rotation",
      .blurb = "The entity's rotation",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "scale",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_entity_get_scale,
      .setter.float_type = rig_entity_set_scale,
      .nick = "Scale",
      .blurb = "The entity's uniform scale factor",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { 0 }
};

static void
_rig_entity_free(void *object)
{
    rig_entity_t *entity = object;

    c_free(entity->label);

    while (entity->components->len)
        rig_entity_remove_component(entity,
                                    c_ptr_array_index(entity->components, 0));

    c_ptr_array_free(entity->components, true);

    rut_graphable_destroy(entity);

    if (entity->renderer_priv) {
        rut_object_t *renderer = *(rut_object_t **)entity->renderer_priv;
        rut_renderer_free_priv(renderer, entity);
    }

    rut_object_free(rig_entity_t, entity);
}

void
rig_entity_reap(rig_entity_t *entity, rig_engine_t *engine)
{
    int i;

    for (i = 0; i < entity->components->len; i++) {
        rut_object_t *component = c_ptr_array_index(entity->components, i);
        rut_componentable_props_t *componentable =
            rut_object_get_properties(component, RUT_TRAIT_ID_COMPONENTABLE);

        /* XXX: any changes made here should be consistent with how
         * rig_entity_remove_component() works too. */

        /* Disassociate the component from the entity:
         * NB: if ->entity is NULL then ->engine must be set */
        componentable->entity = NULL;
        componentable->parented = false;
        componentable->engine = entity->engine;
        rut_object_release(component, entity);

        /* We want to defer garbage collection until the end of a frame
         * so we pass our reference to the engine */
        rut_object_claim(component, engine);

        rig_engine_queue_delete(engine, component);
    }
    c_ptr_array_set_size(entity->components, 0);

    rig_engine_queue_delete(engine, entity);
}

void
rig_component_reap(rut_object_t *component, rig_engine_t *engine)
{
    /* Currently no components reference any other objects that need
     * to be garbage collected. */

    rig_engine_queue_delete(engine, component);
}

void
rig_entity_add_component(rig_entity_t *entity, rut_object_t *object)
{
    rut_componentable_props_t *component =
        rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);

#ifdef RIG_ENABLE_DEBUG
    {
        int i;

        c_return_if_fail(rut_object_get_type(component->engine) == &rig_engine_type);
        c_return_if_fail(component->parented == false);

        if (!rut_object_is(object, rig_code_module_trait_id)) {
            for (i = 0; i < entity->components->len; i++) {
                rut_object_t *existing = c_ptr_array_index(entity->components, i);

                rut_componentable_props_t *existing_component =
                    rut_object_get_properties(existing, RUT_TRAIT_ID_COMPONENTABLE);

                c_return_if_fail(existing != object);
                c_return_if_fail(existing_component->type != component->type);
            }
        }
    }
#endif

    component->entity = entity;
    component->parented = true;

    rut_object_claim(object, entity);
    c_ptr_array_add(entity->components, object);

    if (entity->renderer_priv) {
        rut_object_t *renderer = *(rut_object_t **)entity->renderer_priv;
        rut_renderer_notify_entity_changed(renderer, entity);
    }
}

/* XXX: any changes made here should be consistent with how
 * rig_entity_reap() works too. */
void
rig_entity_remove_component(rig_entity_t *entity, rut_object_t *object)
{
    rut_componentable_props_t *component =
        rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
    bool status;

    if (component->parented) {
        /* Disassociate the component from the entity:
         * NB: if ->entity is NULL then ->engine must be set */
        component->entity = NULL;
        rut_object_release(object, entity);
        component->parented = false;
        component->engine = entity->engine;
    }

    status = c_ptr_array_remove_fast(entity->components, object);

    c_warn_if_fail(status);

    if (entity->renderer_priv) {
        rut_object_t *renderer = *(rut_object_t **)entity->renderer_priv;
        rut_renderer_notify_entity_changed(renderer, entity);
    }
}

void
rig_entity_translate(rig_entity_t *entity, float tx, float ty, float tz)
{
    float pos[3] = { entity->position[1] + tx, entity->position[1] + ty,
                     entity->position[2] + tz, };

    rig_entity_set_position(entity, pos);
}

rut_type_t rig_entity_type;

void
_rig_entity_init_type(void)
{
    static rut_graphable_vtable_t graphable_vtable = {
        NULL, /* child_removed */
        NULL, /* child_added */
        NULL, /* parent_changed */
    };
    static rut_transformable_vtable_t transformable_vtable = {
        rig_entity_get_transform
    };

    rut_type_t *type = &rig_entity_type;
#define TYPE rig_entity_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_entity_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(
        type, RUT_TRAIT_ID_TRANSFORMABLE, 0, &transformable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

rig_entity_t *
rig_entity_new(rig_engine_t *engine)
{
    rig_entity_t *entity = rut_object_alloc0(
        rig_entity_t, &rig_entity_type, _rig_entity_init_type);

    entity->engine = engine;

    rig_introspectable_init(entity, _rig_entity_prop_specs, entity->properties);

    entity->position[0] = 0.0f;
    entity->position[1] = 0.0f;
    entity->position[2] = 0.0f;

    entity->scale = 1.0f;

    c_quaternion_init_identity(&entity->rotation);
    c_matrix_init_identity(&entity->transform);
    entity->components = c_ptr_array_new();

    rut_graphable_init(entity);

    return entity;
}

void
rig_entity_set_label(rut_object_t *obj, const char *label)
{
    rig_entity_t *entity = obj;

    c_free(entity->label);
    entity->label = c_strdup(label);

    rig_property_dirty(entity->engine->property_ctx,
                       &entity->properties[RUT_ENTITY_PROP_LABEL]);
}

const char *
rig_entity_get_label(rut_object_t *obj)
{
    rig_entity_t *entity = obj;

    return entity->label ? entity->label : "";
}

const float *
rig_entity_get_position(rut_object_t *obj)
{
    rig_entity_t *entity = obj;

    return entity->position;
}

rut_object_t *
rig_entity_get_parent(rut_object_t *self)
{
    rig_entity_t *entity = self;

    return entity->graphable.parent;
}

void
rig_entity_set_parent(rut_object_t *self, rut_object_t *parent)
{
    rig_entity_t *entity = self;

    if (entity->graphable.parent == parent)
        return;

    rut_graphable_set_parent(entity, parent);

    rig_property_dirty(entity->engine->property_ctx,
                       &entity->properties[RUT_ENTITY_PROP_PARENT]);
}

void
rig_entity_set_position(rut_object_t *obj, const float position[3])
{
    rig_entity_t *entity = obj;

    if (memcpy(entity->position, position, sizeof(float) * 3) == 0)
        return;

    entity->position[0] = position[0];
    entity->position[1] = position[1];
    entity->position[2] = position[2];
    entity->dirty = true;

    rig_property_dirty(entity->engine->property_ctx,
                       &entity->properties[RUT_ENTITY_PROP_POSITION]);
}

float
rig_entity_get_x(rut_object_t *obj)
{
    rig_entity_t *entity = obj;

    return entity->position[0];
}

void
rig_entity_set_x(rut_object_t *obj, float x)
{
    rig_entity_t *entity = obj;
    float pos[3] = { x, entity->position[1], entity->position[2], };

    rig_entity_set_position(entity, pos);
}

float
rig_entity_get_y(rut_object_t *obj)
{
    rig_entity_t *entity = obj;

    return entity->position[1];
}

void
rig_entity_set_y(rut_object_t *obj, float y)
{
    rig_entity_t *entity = obj;
    float pos[3] = { entity->position[0], y, entity->position[2], };

    rig_entity_set_position(entity, pos);
}

float
rig_entity_get_z(rut_object_t *obj)
{
    rig_entity_t *entity = obj;

    return entity->position[2];
}

void
rig_entity_set_z(rut_object_t *obj, float z)
{
    rig_entity_t *entity = obj;
    float pos[3] = { entity->position[0], entity->position[1], z, };

    rig_entity_set_position(entity, pos);
}

void
rig_entity_get_transformed_position(rig_entity_t *entity,
                                    float position[3])
{
    c_matrix_t transform;
    float w = 1;

    rut_graphable_get_transform(entity, &transform);

    c_matrix_transform_point(
        &transform, &position[0], &position[1], &position[2], &w);
}

const c_quaternion_t *
rig_entity_get_rotation(rut_object_t *obj)
{
    rig_entity_t *entity = obj;

    return &entity->rotation;
}

void
rig_entity_set_rotation(rut_object_t *obj, const c_quaternion_t *rotation)
{
    rig_entity_t *entity = obj;

    if (memcmp(&entity->rotation, rotation, sizeof(*rotation)) == 0)
        return;

    entity->rotation = *rotation;
    entity->dirty = true;

    rig_property_dirty(entity->engine->property_ctx,
                       &entity->properties[RUT_ENTITY_PROP_ROTATION]);
}

void
rig_entity_apply_rotations(rut_object_t *entity,
                           c_quaternion_t *rotations)
{
    int depth = 0;
    rut_object_t **entity_nodes;
    rut_object_t *node = entity;
    int i;

    do {
        rut_graphable_props_t *graphable_priv =
            rut_object_get_properties(node, RUT_TRAIT_ID_GRAPHABLE);

        depth++;

        node = graphable_priv->parent;
    } while (node);

    entity_nodes = c_alloca(sizeof(rut_object_t *) * depth);

    node = entity;
    i = 0;
    do {
        rut_graphable_props_t *graphable_priv;
        rut_object_base_t *obj = node;

        if (obj->type == &rig_entity_type)
            entity_nodes[i++] = node;

        graphable_priv =
            rut_object_get_properties(node, RUT_TRAIT_ID_GRAPHABLE);
        node = graphable_priv->parent;
    } while (node);

    for (i--; i >= 0; i--) {
        const c_quaternion_t *rotation =
            rig_entity_get_rotation(entity_nodes[i]);
        c_quaternion_multiply(rotations, rotations, rotation);
    }
}

void
rig_entity_get_rotations(rut_object_t *entity, c_quaternion_t *rotation)
{
    c_quaternion_init_identity(rotation);
    rig_entity_apply_rotations(entity, rotation);
}

void
rig_entity_get_view_rotations(rut_object_t *entity,
                              rut_object_t *camera_entity,
                              c_quaternion_t *rotation)
{
    rig_entity_get_rotations(camera_entity, rotation);
    c_quaternion_invert(rotation);

    rig_entity_apply_rotations(entity, rotation);
}

float
rig_entity_get_scale(rut_object_t *obj)
{
    rig_entity_t *entity = obj;

    return entity->scale;
}

void
rig_entity_set_scale(rut_object_t *obj, float scale)
{
    rig_entity_t *entity = obj;

    if (entity->scale == scale)
        return;

    entity->scale = scale;
    entity->dirty = true;

    rig_property_dirty(entity->engine->property_ctx,
                       &entity->properties[RUT_ENTITY_PROP_SCALE]);
}

float
rig_entity_get_scales(rut_object_t *entity)
{
    rut_object_t *node = entity;
    float scales = 1;

    do {
        rut_graphable_props_t *graphable_priv =
            rut_object_get_properties(node, RUT_TRAIT_ID_GRAPHABLE);
        rut_object_base_t *obj = node;

        if (obj->type == &rig_entity_type)
            scales *= rig_entity_get_scale(node);

        node = graphable_priv->parent;
    } while (node);

    return scales;
}

const c_matrix_t *
rig_entity_get_transform(rut_object_t *self)
{
    rig_entity_t *entity = self;
    c_matrix_t rotation;

    if (!entity->dirty)
        return &entity->transform;

    c_matrix_init_translation(&entity->transform,
                               entity->position[0],
                               entity->position[1],
                               entity->position[2]);
    c_matrix_init_from_quaternion(&rotation, &entity->rotation);
    c_matrix_multiply(&entity->transform, &entity->transform, &rotation);
    c_matrix_scale(&entity->transform, entity->scale, entity->scale, entity->scale);

    entity->dirty = false;

    return &entity->transform;
}

void
rig_entity_set_translate(rig_entity_t *entity, float tx, float ty, float tz)
{
    float pos[3] = { tx, ty, tz, };

    rig_entity_set_position(entity, pos);
}

void
rig_entity_rotate_x_axis(rig_entity_t *entity, float x_angle)
{
    c_quaternion_t x_rotation;

    c_quaternion_init_from_x_rotation(&x_rotation, x_angle);
    c_quaternion_multiply(&entity->rotation, &entity->rotation, &x_rotation);

    entity->dirty = true;

    rig_property_dirty(entity->engine->property_ctx,
                       &entity->properties[RUT_ENTITY_PROP_ROTATION]);
}

void
rig_entity_rotate_y_axis(rig_entity_t *entity, float y_angle)
{
    c_quaternion_t y_rotation;

    c_quaternion_init_from_y_rotation(&y_rotation, y_angle);
    c_quaternion_multiply(&entity->rotation, &entity->rotation, &y_rotation);

    entity->dirty = true;

    rig_property_dirty(entity->engine->property_ctx,
                       &entity->properties[RUT_ENTITY_PROP_ROTATION]);
}

void
rig_entity_rotate_z_axis(rig_entity_t *entity, float z_angle)
{
    c_quaternion_t z_rotation;

    c_quaternion_init_from_z_rotation(&z_rotation, z_angle);
    c_quaternion_multiply(&entity->rotation, &entity->rotation, &z_rotation);

    entity->dirty = true;

    rig_property_dirty(entity->engine->property_ctx,
                       &entity->properties[RUT_ENTITY_PROP_ROTATION]);
}

rut_object_t *
rig_entity_get_component(rig_entity_t *entity,
                         rut_component_type_t type)
{
    int i;

    for (i = 0; i < entity->components->len; i++) {
        rut_object_t *component = c_ptr_array_index(entity->components, i);
        rut_componentable_props_t *component_props =
            rut_object_get_properties(component, RUT_TRAIT_ID_COMPONENTABLE);

        if (component_props->type == type)
            return component;
    }

    return NULL;
}

void
rig_entity_foreach_component_safe(rig_entity_t *entity,
                                  bool (*callback)(rut_object_t *component,
                                                   void *user_data),
                                  void *user_data)
{
    int i;
    int n_components = entity->components->len;
    size_t size = sizeof(void *) * n_components;
    void **components = c_alloca(size);
    bool cont = true;

    memcpy(components, entity->components->pdata, size);

    for (i = 0; cont && i < n_components; i++)
        cont = callback(components[i], user_data);
}

void
rig_entity_foreach_component(rig_entity_t *entity,
                             bool (*callback)(rut_object_t *component,
                                              void *user_data),
                             void *user_data)
{
    int i;
    bool cont = true;
    for (i = 0; cont && i < entity->components->len; i++)
        cont = callback(c_ptr_array_index(entity->components, i), user_data);
}

rig_entity_t *
rig_entity_copy_shallow(rig_entity_t *entity)
{
    rig_entity_t *copy = rig_entity_new(entity->engine);

    copy->label = NULL;

    memcpy(copy->position, entity->position, sizeof(entity->position));
    copy->rotation = entity->rotation;
    copy->scale = entity->scale;
    copy->transform = entity->transform;
    copy->dirty = entity->dirty;

    return copy;
}

rig_entity_t *
rig_entity_copy(rig_entity_t *entity)
{
    rig_entity_t *copy = rig_entity_copy_shallow(entity);
    c_ptr_array_t *entity_components = entity->components;
    c_ptr_array_t *copy_components;
    rut_graphable_props_t *graph_props =
        rut_object_get_properties(entity, RUT_TRAIT_ID_GRAPHABLE);
    rut_queue_item_t *item;
    int i;

    copy_components = c_ptr_array_sized_new(entity_components->len);
    copy->components = copy_components;

    for (i = 0; i < entity_components->len; i++) {
        rut_object_t *component = c_ptr_array_index(entity_components, i);
        rut_componentable_vtable_t *componentable =
            rut_object_get_vtable(component, RUT_TRAIT_ID_COMPONENTABLE);
        rut_object_t *component_copy = componentable->copy(component);

        rig_entity_add_component(copy, component_copy);
        rut_object_unref(component_copy);
    }

    c_list_for_each(item, &graph_props->children.items, list_node)
    {
        rut_object_t *child = item->data;
        rut_object_t *child_copy;

        if (rut_object_get_type(child) != &rig_entity_type)
            continue;

        child_copy = rig_entity_copy(child);
        rut_graphable_add_child(copy, child_copy);
    }

    return copy;
}

void
rig_entity_notify_changed(rig_entity_t *entity)
{
    if (entity->renderer_priv) {
        rut_object_t *renderer = *(rut_object_t **)entity->renderer_priv;
        rut_renderer_notify_entity_changed(renderer, entity);
    }
}

void
rig_entity_set_camera_view_from_transform(rig_entity_t *camera)
{
    rut_object_t *camera_component =
        rig_entity_get_component(camera, RUT_COMPONENT_TYPE_CAMERA);
    c_matrix_t transform;
    c_matrix_t view;

    rut_graphable_get_transform(camera, &transform);
    c_matrix_get_inverse(&transform, &view);

    rut_camera_set_view_transform(camera_component, &view);
}

