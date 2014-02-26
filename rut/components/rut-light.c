/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "rut-light.h"
#include "rut-color.h"

static RutPropertySpec
_rut_light_prop_specs[] = {
  {
    .name = "ambient",
    .nick = "Ambient",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .data_offset = offsetof (RutLight, ambient),
    .setter.color_type = rut_light_set_ambient,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "diffuse",
    .nick = "Diffuse",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .data_offset = offsetof (RutLight, diffuse),
    .setter.color_type = rut_light_set_diffuse,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  {
    .name = "specular",
    .nick = "Specular",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .data_offset = offsetof (RutLight, specular),
    .setter.color_type = rut_light_set_specular,
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .animatable = TRUE
  },
  { 0 }
};

static float *
get_color_array (CoglColor *color)
{
  static float array[4];

  array[0] = color->red;
  array[1] = color->green;
  array[2] = color->blue;
  array[3] = color->alpha;

  return array;
}

void
rut_light_set_uniforms (RutLight *light,
                        CoglPipeline *pipeline)
{
  RutComponentableProps *component =
    rut_object_get_properties (light, RUT_TRAIT_ID_COMPONENTABLE);
  RutEntity *entity = component->entity;
  float origin[3] = {0, 0, 0};
  float norm_direction[3] = {0, 0, 1};
  int location;

  rut_entity_get_transformed_position (entity, origin);
  rut_entity_get_transformed_position (entity, norm_direction);
  cogl_vector3_subtract (norm_direction, norm_direction, origin);
  cogl_vector3_normalize (norm_direction);

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "light0_direction_norm");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   3, 1,
                                   norm_direction);

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "light0_ambient");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   4, 1,
                                   get_color_array (&light->ambient));

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "light0_diffuse");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   4, 1,
                                   get_color_array (&light->diffuse));

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "light0_specular");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   4, 1,
                                   get_color_array (&light->specular));
}

static void
_rut_light_free (void *object)
{
  RutLight *light = object;

#ifdef RIG_ENABLE_DEBUG
  {
    RutComponentableProps *component =
      rut_object_get_properties (object, RUT_TRAIT_ID_COMPONENTABLE);
    g_return_if_fail (component->entity == NULL);
  }
#endif

  rut_object_free (RutLight, light);
}

static RutObject *
_rut_light_copy (RutObject *object)
{
  RutLight *light = object;
  RutLight *copy = rut_light_new (light->context);

  copy->ambient = light->ambient;
  copy->diffuse = light->diffuse;
  copy->specular = light->specular;

  return copy;
}

RutType rut_light_type;

void
_rut_light_init_type (void)
{

  static RutComponentableVTable componentable_vtable = {
    .copy = _rut_light_copy
  };


  RutType *type = &rut_light_type;
#define TYPE RutLight

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_light_free);
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

RutLight *
rut_light_new (RutContext *context)
{
  RutLight *light;

  light =
    rut_object_alloc0 (RutLight, &rut_light_type, _rut_light_init_type);

  light->component.type = RUT_COMPONENT_TYPE_LIGHT;
  light->context = rut_object_ref (context);

  rut_introspectable_init (light,
                           _rut_light_prop_specs,
                           light->properties);

  cogl_color_init_from_4f (&light->ambient, 1.0, 1.0, 1.0, 1.0);
  cogl_color_init_from_4f (&light->diffuse, 1.0, 1.0, 1.0, 1.0);
  cogl_color_init_from_4f (&light->specular, 1.0, 1.0, 1.0, 1.0);

  return light;
}

void
rut_light_free (RutLight *light)
{
  rut_object_unref (light->context);

  rut_introspectable_destroy (light);

  rut_object_free (RutLight, light);
}

void
rut_light_set_ambient (RutObject *obj,
                       const CoglColor *ambient)
{
  RutLight *light = RUT_LIGHT (obj);

  light->ambient = *ambient;

  rut_property_dirty (&light->context->property_ctx,
                      &light->properties[RUT_LIGHT_PROP_AMBIENT]);
}

const CoglColor *
rut_light_get_ambient (RutObject *obj)
{
  RutLight *light = RUT_LIGHT (obj);

  return &light->ambient;
}

void
rut_light_set_diffuse (RutObject *obj,
                       const CoglColor *diffuse)
{
  RutLight *light = RUT_LIGHT (obj);

  light->diffuse = *diffuse;

  rut_property_dirty (&light->context->property_ctx,
                      &light->properties[RUT_LIGHT_PROP_DIFFUSE]);
}

const CoglColor *
rut_light_get_diffuse (RutLight *light)
{
  return &light->diffuse;
}

void
rut_light_set_specular (RutObject *obj,
                        const CoglColor *specular)
{
  RutLight *light = RUT_LIGHT (obj);

  light->specular = *specular;

  rut_property_dirty (&light->context->property_ctx,
                      &light->properties[RUT_LIGHT_PROP_SPECULAR]);
}

const CoglColor *
rut_light_get_specular (RutLight *light)
{
  return &light->specular;
}
