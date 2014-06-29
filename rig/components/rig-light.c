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

#include <config.h>

#include "rig-light.h"
#include "rut-color.h"

static RutPropertySpec
_rig_light_prop_specs[] = {
  {
    .name = "ambient",
    .nick = "Ambient",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .data_offset = offsetof (RigLight, ambient),
    .setter.color_type = rig_light_set_ambient,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = true
  },
  {
    .name = "diffuse",
    .nick = "Diffuse",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .data_offset = offsetof (RigLight, diffuse),
    .setter.color_type = rig_light_set_diffuse,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = true
  },
  {
    .name = "specular",
    .nick = "Specular",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .data_offset = offsetof (RigLight, specular),
    .setter.color_type = rig_light_set_specular,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = true
  },
  { 0 }
};

static float *
get_color_array (cg_color_t *color)
{
  static float array[4];

  array[0] = color->red;
  array[1] = color->green;
  array[2] = color->blue;
  array[3] = color->alpha;

  return array;
}

void
rig_light_set_uniforms (RigLight *light,
                        cg_pipeline_t *pipeline)
{
  RutComponentableProps *component =
    rut_object_get_properties (light, RUT_TRAIT_ID_COMPONENTABLE);
  RigEntity *entity = component->entity;
  float origin[3] = {0, 0, 0};
  float norm_direction[3] = {0, 0, 1};
  int location;

  rig_entity_get_transformed_position (entity, origin);
  rig_entity_get_transformed_position (entity, norm_direction);
  cg_vector3_subtract (norm_direction, norm_direction, origin);
  cg_vector3_normalize (norm_direction);

  location = cg_pipeline_get_uniform_location (pipeline,
                                                 "light0_direction_norm");
  cg_pipeline_set_uniform_float (pipeline,
                                   location,
                                   3, 1,
                                   norm_direction);

  location = cg_pipeline_get_uniform_location (pipeline,
                                                 "light0_ambient");
  cg_pipeline_set_uniform_float (pipeline,
                                   location,
                                   4, 1,
                                   get_color_array (&light->ambient));

  location = cg_pipeline_get_uniform_location (pipeline,
                                                 "light0_diffuse");
  cg_pipeline_set_uniform_float (pipeline,
                                   location,
                                   4, 1,
                                   get_color_array (&light->diffuse));

  location = cg_pipeline_get_uniform_location (pipeline,
                                                 "light0_specular");
  cg_pipeline_set_uniform_float (pipeline,
                                   location,
                                   4, 1,
                                   get_color_array (&light->specular));
}

static void
_rig_light_free (void *object)
{
  RigLight *light = object;

#ifdef RIG_ENABLE_DEBUG
  {
    RutComponentableProps *component =
      rut_object_get_properties (object, RUT_TRAIT_ID_COMPONENTABLE);
    c_return_if_fail (component->entity == NULL);
  }
#endif

  rut_object_free (RigLight, light);
}

static RutObject *
_rig_light_copy (RutObject *object)
{
  RigLight *light = object;
  RigLight *copy = rig_light_new (light->context);

  copy->ambient = light->ambient;
  copy->diffuse = light->diffuse;
  copy->specular = light->specular;

  return copy;
}

RutType rig_light_type;

static void
_rig_light_init_type (void)
{
  static RutComponentableVTable componentable_vtable = {
    .copy = _rig_light_copy
  };


  RutType *type = &rig_light_type;
#define TYPE RigLight

  rut_type_init (type, C_STRINGIFY (TYPE), _rig_light_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPONENTABLE,
                      offsetof (TYPE, component),
                      &componentable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INTROSPECTABLE,
                      offsetof (TYPE, introspectable),
                      NULL); /* no implied vtable */

#undef TYPE
}

RigLight *
rig_light_new (RutContext *context)
{
  RigLight *light;

  light =
    rut_object_alloc0 (RigLight, &rig_light_type, _rig_light_init_type);

  light->component.type = RUT_COMPONENT_TYPE_LIGHT;
  light->context = rut_object_ref (context);

  rut_introspectable_init (light,
                           _rig_light_prop_specs,
                           light->properties);

  cg_color_init_from_4f (&light->ambient, 1.0, 1.0, 1.0, 1.0);
  cg_color_init_from_4f (&light->diffuse, 1.0, 1.0, 1.0, 1.0);
  cg_color_init_from_4f (&light->specular, 1.0, 1.0, 1.0, 1.0);

  return light;
}

void
rig_light_free (RigLight *light)
{
  rut_object_unref (light->context);

  rut_introspectable_destroy (light);

  rut_object_free (RigLight, light);
}

void
rig_light_set_ambient (RutObject *obj,
                       const cg_color_t *ambient)
{
  RigLight *light = obj;

  light->ambient = *ambient;

  rut_property_dirty (&light->context->property_ctx,
                      &light->properties[RIG_LIGHT_PROP_AMBIENT]);
}

const cg_color_t *
rig_light_get_ambient (RutObject *obj)
{
  RigLight *light = obj;

  return &light->ambient;
}

void
rig_light_set_diffuse (RutObject *obj,
                       const cg_color_t *diffuse)
{
  RigLight *light = obj;

  light->diffuse = *diffuse;

  rut_property_dirty (&light->context->property_ctx,
                      &light->properties[RIG_LIGHT_PROP_DIFFUSE]);
}

const cg_color_t *
rig_light_get_diffuse (RigLight *light)
{
  return &light->diffuse;
}

void
rig_light_set_specular (RutObject *obj,
                        const cg_color_t *specular)
{
  RigLight *light = obj;

  light->specular = *specular;

  rut_property_dirty (&light->context->property_ctx,
                      &light->properties[RIG_LIGHT_PROP_SPECULAR]);
}

const cg_color_t *
rig_light_get_specular (RigLight *light)
{
  return &light->specular;
}
