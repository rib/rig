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

#include "rig-entity.h"
#include "rig-entity-inlines.h"
#include "rig-light.h"
#include "rut-color.h"

static rig_property_spec_t _rig_light_prop_specs[] = {
    { .name = "ambient",
      .nick = "Ambient",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .data_offset = offsetof(rig_light_t, ambient),
      .setter.color_type = rig_light_set_ambient,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "diffuse",
      .nick = "Diffuse",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .data_offset = offsetof(rig_light_t, diffuse),
      .setter.color_type = rig_light_set_diffuse,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "specular",
      .nick = "Specular",
      .type = RUT_PROPERTY_TYPE_COLOR,
      .data_offset = offsetof(rig_light_t, specular),
      .setter.color_type = rig_light_set_specular,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { 0 }
};

static float *
get_color_array(cg_color_t *color)
{
    static float array[4];

    array[0] = color->red;
    array[1] = color->green;
    array[2] = color->blue;
    array[3] = color->alpha;

    return array;
}

void
rig_light_set_uniforms(rig_light_t *light, cg_pipeline_t *pipeline)
{
    rut_componentable_props_t *component =
        rut_object_get_properties(light, RUT_TRAIT_ID_COMPONENTABLE);
    rig_entity_t *entity = component->entity;
    float origin[3] = { 0, 0, 0 };
    float norm_direction[3] = { 0, 0, 1 };
    int location;

    rig_entity_get_transformed_position(entity, origin);
    rig_entity_get_transformed_position(entity, norm_direction);
    c_vector3_subtract(norm_direction, norm_direction, origin);
    c_vector3_normalize(norm_direction);

    location =
        cg_pipeline_get_uniform_location(pipeline, "light0_direction_norm");
    cg_pipeline_set_uniform_float(pipeline, location, 3, 1, norm_direction);

    location = cg_pipeline_get_uniform_location(pipeline, "light0_ambient");
    cg_pipeline_set_uniform_float(
        pipeline, location, 4, 1, get_color_array(&light->ambient));

    location = cg_pipeline_get_uniform_location(pipeline, "light0_diffuse");
    cg_pipeline_set_uniform_float(
        pipeline, location, 4, 1, get_color_array(&light->diffuse));

    location = cg_pipeline_get_uniform_location(pipeline, "light0_specular");
    cg_pipeline_set_uniform_float(
        pipeline, location, 4, 1, get_color_array(&light->specular));
}

static void
_rig_light_free(void *object)
{
    rig_light_t *light = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    rut_object_free(rig_light_t, light);
}

static rut_object_t *
_rig_light_copy(rut_object_t *object)
{
    rig_light_t *light = object;
    rig_engine_t *engine = rig_component_props_get_engine(&light->component);
    rig_light_t *copy = rig_light_new(engine);

    copy->ambient = light->ambient;
    copy->diffuse = light->diffuse;
    copy->specular = light->specular;

    return copy;
}

rut_type_t rig_light_type;

static void
_rig_light_init_type(void)
{
    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_light_copy
    };

    rut_type_t *type = &rig_light_type;
#define TYPE rig_light_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_light_free);
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

rig_light_t *
rig_light_new(rig_engine_t *engine)
{
    rig_light_t *light =
        rut_object_alloc0(rig_light_t, &rig_light_type, _rig_light_init_type);

    light->component.type = RIG_COMPONENT_TYPE_LIGHT;
    light->component.parented = false;
    light->component.engine = engine;

    rig_introspectable_init(light, _rig_light_prop_specs, light->properties);

    cg_color_init_from_4f(&light->ambient, 1.0, 1.0, 1.0, 1.0);
    cg_color_init_from_4f(&light->diffuse, 1.0, 1.0, 1.0, 1.0);
    cg_color_init_from_4f(&light->specular, 1.0, 1.0, 1.0, 1.0);

    return light;
}

void
rig_light_free(rig_light_t *light)
{
    rig_introspectable_destroy(light);

    rut_object_free(rig_light_t, light);
}

void
rig_light_set_ambient(rut_object_t *obj, const cg_color_t *ambient)
{
    rig_light_t *light = obj;
    rig_property_context_t *prop_ctx;

    light->ambient = *ambient;

    prop_ctx = rig_component_props_get_property_context(&light->component);
    rig_property_dirty(prop_ctx, &light->properties[RIG_LIGHT_PROP_AMBIENT]);
}

const cg_color_t *
rig_light_get_ambient(rut_object_t *obj)
{
    rig_light_t *light = obj;

    return &light->ambient;
}

void
rig_light_set_diffuse(rut_object_t *obj, const cg_color_t *diffuse)
{
    rig_light_t *light = obj;
    rig_property_context_t *prop_ctx;

    light->diffuse = *diffuse;

    prop_ctx = rig_component_props_get_property_context(&light->component);
    rig_property_dirty(prop_ctx, &light->properties[RIG_LIGHT_PROP_DIFFUSE]);
}

const cg_color_t *
rig_light_get_diffuse(rig_light_t *light)
{
    return &light->diffuse;
}

void
rig_light_set_specular(rut_object_t *obj, const cg_color_t *specular)
{
    rig_light_t *light = obj;
    rig_property_context_t *prop_ctx;

    light->specular = *specular;

    prop_ctx = rig_component_props_get_property_context(&light->component);
    rig_property_dirty(prop_ctx, &light->properties[RIG_LIGHT_PROP_SPECULAR]);
}

const cg_color_t *
rig_light_get_specular(rig_light_t *light)
{
    return &light->specular;
}
