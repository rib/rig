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

#include <rut.h>


#include "rig-entity.h"
#include "rig-entity-inlines.h"
#include "rig-material.h"
#include "rig-source.h"

static rig_property_spec_t _rig_material_prop_specs[] = {
    { .name = "visible",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_material_get_visible,
      .setter.boolean_type = rig_material_set_visible,
      .nick = "Visible",
      .blurb = "Whether the material is visible or not",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "cast_shadow",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_material_get_cast_shadow,
      .setter.boolean_type = rig_material_set_cast_shadow,
      .nick = "Cast Shadow",
      .blurb = "Whether the material casts shadows or not",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "receive_shadow",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_material_get_receive_shadow,
      .setter.boolean_type = rig_material_set_receive_shadow,
      .nick = "Receive Shadow",
      .blurb = "Whether the material receives shadows or not",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "color_source",
      .nick = "Color Source",
      .type = RUT_PROPERTY_TYPE_OBJECT,
      .validation = { .object.type = &rig_source_type },
      .getter.object_type = rig_material_get_color_source,
      .setter.object_type = rig_material_set_color_source,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = false },
    { .name = "ambient_occlusion_source",
      .nick = "Ambient Occlusion Source",
      .type = RUT_PROPERTY_TYPE_OBJECT,
      .validation = { .object.type = &rig_source_type },
      .getter.object_type = rig_material_get_ambient_occlusion_source,
      .setter.object_type = rig_material_set_ambient_occlusion_source,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = false },
    { .name = "normal_map",
      .nick = "Normal Map",
      .type = RUT_PROPERTY_TYPE_OBJECT,
      .validation = { .object.type = &rig_source_type },
      .getter.object_type = rig_material_get_normal_map_source,
      .setter.object_type = rig_material_set_normal_map_source,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = false },
    { .name = "alpha_mask",
      .nick = "Alpha Mask",
      .type = RUT_PROPERTY_TYPE_OBJECT,
      .validation = { .object.type = &rig_source_type },
      .getter.object_type = rig_material_get_alpha_mask_source,
      .setter.object_type = rig_material_set_alpha_mask_source,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = false },
    { .name = "ambient",
      .nick = "Ambient",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .getter.color_type = rig_material_get_ambient,
      .setter.color_type = rig_material_set_ambient,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "diffuse",
      .nick = "Diffuse",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .getter.color_type = rig_material_get_diffuse,
      .setter.color_type = rig_material_set_diffuse,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "specular",
      .nick = "Specular",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .getter.color_type = rig_material_get_specular,
      .setter.color_type = rig_material_set_specular,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "shininess",
      .nick = "Shininess",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_material_get_shininess,
      .setter.float_type = rig_material_set_shininess,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, 1000 } },
      .animatable = true },
    { .name = "alpha-mask-threshold",
      .nick = "Alpha Threshold",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_material_get_alpha_mask_threshold,
      .setter.float_type = rig_material_set_alpha_mask_threshold,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, 1 } },
      .animatable = true },
    { 0 }
};

static void
_rig_material_free(void *object)
{
    rig_material_t *material = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    if (material->color_source)
        rut_object_unref(material->color_source);

    if (material->ambient_occlusion_source)
        rut_object_unref(material->ambient_occlusion_source);

    if (material->normal_map_source)
        rut_object_unref(material->normal_map_source);

    if (material->alpha_mask_source)
        rut_object_unref(material->alpha_mask_source);

    rig_introspectable_destroy(material);

    rut_object_free(rig_material_t, material);
}

static rut_object_t *
_rig_material_copy(rut_object_t *object)
{
    rig_material_t *material = object;
    rig_engine_t *engine = rig_component_props_get_engine(&material->component);
    rig_material_t *copy = rig_material_new(engine);

    copy->visible = material->visible;
    copy->cast_shadow = material->cast_shadow;
    copy->receive_shadow = material->receive_shadow;

    if (material->color_source)
        copy->color_source = rut_object_ref(material->color_source);
    if (material->ambient_occlusion_source)
        copy->ambient_occlusion_source = rut_object_ref(material->color_source);
    if (material->normal_map_source)
        copy->normal_map_source = rut_object_ref(material->normal_map_source);
    if (material->alpha_mask_source)
        copy->alpha_mask_source = rut_object_ref(material->alpha_mask_source);

    copy->ambient = material->ambient;
    copy->diffuse = material->diffuse;
    copy->specular = material->specular;
    copy->shininess = material->shininess;
    copy->alpha_mask_threshold = material->alpha_mask_threshold;

    return copy;
}

rut_type_t rig_material_type;

static void
_rig_material_init_type(void)
{

    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_material_copy
    };

    rut_type_t *type = &rig_material_type;
#define TYPE rig_material_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_material_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPONENTABLE,
                       offsetof(TYPE, component),
                       &componentable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE
}

rig_material_t *
rig_material_new(rig_engine_t *engine)
{
    rig_material_t *material = rut_object_alloc0(
        rig_material_t, &rig_material_type, _rig_material_init_type);

    material->component.type = RUT_COMPONENT_TYPE_MATERIAL;
    material->component.parented = false;
    material->component.engine = engine;

    material->visible = true;
    material->receive_shadow = true;

    cg_color_init_from_4f(&material->ambient, 0.23, 0.23, 0.23, 1);
    cg_color_init_from_4f(&material->diffuse, 0.75, 0.75, 0.75, 1);
    cg_color_init_from_4f(&material->specular, 0.64, 0.64, 0.64, 1);

    material->shininess = 100;

    rig_introspectable_init(
        material, _rig_material_prop_specs, material->properties);

    material->uniforms_flush_age = -1;

    material->color_source = NULL;
    material->ambient_occlusion_source = NULL;
    material->normal_map_source = NULL;
    material->alpha_mask_source = NULL;

    return material;
}

void
rig_material_set_color_source(rut_object_t *object,
                              rut_object_t *source)
{
    rig_material_t *material = object;

    if (material->color_source == source)
        return;

    if (material->color_source) {
        rut_object_unref(material->color_source);
        material->color_source = NULL;
    }

    material->color_source = source;
    if (source)
        rut_object_ref(source);

    if (material->component.parented)
        rig_entity_notify_changed(material->component.entity);
}

rut_object_t *
rig_material_get_color_source(rut_object_t *object)
{
    rig_material_t *material = object;
    return material->color_source;
}

void
rig_material_set_ambient_occlusion_source(rut_object_t *object,
                                          rut_object_t *source)
{
    rig_material_t *material = object;

    if (material->ambient_occlusion_source == source)
        return;

    if (material->ambient_occlusion_source) {
        rut_object_unref(material->ambient_occlusion_source);
        material->ambient_occlusion_source = NULL;
    }

    material->ambient_occlusion_source = source;
    if (source)
        rut_object_ref(source);

    if (material->component.parented)
        rig_entity_notify_changed(material->component.entity);
}

rut_object_t *
rig_material_get_ambient_occlusion_source(rut_object_t *object)
{
    rig_material_t *material = object;
    return material->ambient_occlusion_source;
}

void
rig_material_set_normal_map_source(rut_object_t *object,
                                   rut_object_t *source)
{
    rig_material_t *material = object;

    if (material->normal_map_source == source)
        return;

    if (material->normal_map_source) {
        rut_object_unref(material->normal_map_source);
        material->normal_map_source = NULL;
    }

    material->normal_map_source = source;
    if (source)
        rut_object_ref(source);

    if (material->component.parented)
        rig_entity_notify_changed(material->component.entity);
}

rut_object_t *
rig_material_get_normal_map_source(rut_object_t *object)
{
    rig_material_t *material = object;
    return material->normal_map_source;
}

void
rig_material_set_alpha_mask_source(rut_object_t *object,
                                   rut_object_t *source)
{
    rig_material_t *material = object;

    if (material->alpha_mask_source == source)
        return;

    if (material->alpha_mask_source) {
        rut_object_unref(material->alpha_mask_source);
        material->alpha_mask_source = NULL;
    }

    material->alpha_mask_source = source;
    if (source)
        rut_object_ref(source);

    if (material->component.parented)
        rig_entity_notify_changed(material->component.entity);
}

rut_object_t *
rig_material_get_alpha_mask_source(rut_object_t *object)
{
    rig_material_t *material = object;
    return material->alpha_mask_source;
}

void
rig_material_set_ambient(rut_object_t *obj, const cg_color_t *color)
{
    rig_material_t *material = obj;
    rig_property_context_t *prop_ctx;

    material->ambient = *color;
    material->uniforms_age++;

    prop_ctx = rig_component_props_get_property_context(&material->component);
    rig_property_dirty(prop_ctx, &material->properties[RIG_MATERIAL_PROP_AMBIENT]);
}

const cg_color_t *
rig_material_get_ambient(rut_object_t *obj)
{
    rig_material_t *material = obj;

    return &material->ambient;
}

void
rig_material_set_diffuse(rut_object_t *obj, const cg_color_t *color)
{
    rig_material_t *material = obj;
    rig_property_context_t *prop_ctx;

    material->diffuse = *color;
    material->uniforms_age++;

    prop_ctx = rig_component_props_get_property_context(&material->component);
    rig_property_dirty(prop_ctx, &material->properties[RIG_MATERIAL_PROP_DIFFUSE]);
}

const cg_color_t *
rig_material_get_diffuse(rut_object_t *obj)
{
    rig_material_t *material = obj;

    return &material->diffuse;
}

void
rig_material_set_specular(rut_object_t *obj, const cg_color_t *color)
{
    rig_material_t *material = obj;
    rig_property_context_t *prop_ctx;

    material->specular = *color;
    material->uniforms_age++;

    prop_ctx = rig_component_props_get_property_context(&material->component);
    rig_property_dirty(prop_ctx, &material->properties[RIG_MATERIAL_PROP_SPECULAR]);
}

const cg_color_t *
rig_material_get_specular(rut_object_t *obj)
{
    rig_material_t *material = obj;

    return &material->specular;
}

void
rig_material_set_shininess(rut_object_t *obj, float shininess)
{
    rig_material_t *material = obj;
    rig_property_context_t *prop_ctx;

    material->shininess = shininess;
    material->uniforms_age++;

    prop_ctx = rig_component_props_get_property_context(&material->component);
    rig_property_dirty(prop_ctx, &material->properties[RIG_MATERIAL_PROP_SPECULAR]);
}

float
rig_material_get_shininess(rut_object_t *obj)
{
    rig_material_t *material = obj;

    return material->shininess;
}

float
rig_material_get_alpha_mask_threshold(rut_object_t *obj)
{
    rig_material_t *material = obj;

    return material->alpha_mask_threshold;
}

void
rig_material_set_alpha_mask_threshold(rut_object_t *obj, float threshold)
{
    rig_material_t *material = obj;
    rig_property_context_t *prop_ctx;

    if (material->alpha_mask_threshold == threshold)
        return;

    material->alpha_mask_threshold = threshold;

    prop_ctx = rig_component_props_get_property_context(&material->component);
    rig_property_dirty(prop_ctx, &material->properties[RIG_MATERIAL_PROP_ALPHA_MASK_THRESHOLD]);
}

void
rig_material_flush_uniforms(rig_material_t *material,
                            cg_pipeline_t *pipeline)
{
    int location;
    rut_object_t *geo;
    rig_entity_t *entity = material->component.entity;

    c_return_if_fail(material->component.parented);

    // if (material->uniforms_age == material->uniforms_flush_age)
    //  return;

    location = cg_pipeline_get_uniform_location(pipeline, "material_ambient");
    cg_pipeline_set_uniform_float(
        pipeline, location, 4, 1, (float *)&material->ambient);

    location = cg_pipeline_get_uniform_location(pipeline, "material_diffuse");
    cg_pipeline_set_uniform_float(
        pipeline, location, 4, 1, (float *)&material->diffuse);

    location = cg_pipeline_get_uniform_location(pipeline, "material_specular");
    cg_pipeline_set_uniform_float(
        pipeline, location, 4, 1, (float *)&material->specular);

    location = cg_pipeline_get_uniform_location(pipeline, "material_shininess");
    cg_pipeline_set_uniform_1f(pipeline, location, material->shininess);

    location =
        cg_pipeline_get_uniform_location(pipeline, "material_alpha_threshold");
    cg_pipeline_set_uniform_1f(
        pipeline, location, material->alpha_mask_threshold);

    geo = rig_entity_get_component(entity, RUT_COMPONENT_TYPE_GEOMETRY);

    material->uniforms_flush_age = material->uniforms_age;
}

void
rig_material_dirty_uniforms(rig_material_t *material)
{
    material->uniforms_flush_age = material->uniforms_age - 1;
}

bool
rig_material_get_cast_shadow(rut_object_t *obj)
{
    rig_material_t *material = obj;

    return material->cast_shadow;
}

void
rig_material_set_cast_shadow(rut_object_t *obj, bool cast_shadow)
{
    rig_material_t *material = obj;
    rig_property_context_t *prop_ctx;

    if (material->cast_shadow == cast_shadow)
        return;

    material->cast_shadow = cast_shadow;

    prop_ctx = rig_component_props_get_property_context(&material->component);
    rig_property_dirty(prop_ctx,
                       &material->properties[RIG_MATERIAL_PROP_CAST_SHADOW]);
}

bool
rig_material_get_receive_shadow(rut_object_t *obj)
{
    rig_material_t *material = obj;

    return material->receive_shadow;
}

void
rig_material_set_receive_shadow(rut_object_t *obj, bool receive_shadow)
{
    rig_material_t *material = obj;
    rig_property_context_t *prop_ctx;

    if (material->receive_shadow == receive_shadow)
        return;

    material->receive_shadow = receive_shadow;

    prop_ctx = rig_component_props_get_property_context(&material->component);
    rig_property_dirty(prop_ctx,
                       &material->properties[RIG_MATERIAL_PROP_RECEIVE_SHADOW]);

    if (material->component.parented)
        rig_entity_notify_changed(material->component.entity);
}

bool
rig_material_get_visible(rut_object_t *obj)
{
    rig_material_t *material = obj;

    return material->visible;
}

void
rig_material_set_visible(rut_object_t *obj, bool visible)
{
    rig_material_t *material = obj;
    rig_property_context_t *prop_ctx;

    if (material->visible == visible)
        return;

    material->visible = visible;

    prop_ctx = rig_component_props_get_property_context(&material->component);
    rig_property_dirty(prop_ctx,
                       &material->properties[RIG_MATERIAL_PROP_VISIBLE]);
}
