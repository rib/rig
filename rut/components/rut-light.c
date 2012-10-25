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

#include "rut-light.h"
#include "rut-color.h"

static float *
get_color_array (RutColor *color)
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
    rut_object_get_properties (light, RUT_INTERFACE_ID_COMPONENTABLE);
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
rut_light_update (RutObject *object,
                  int64_t time)
{
}

RutType rut_light_type;

static RutComponentableVTable _rut_light_componentable_vtable = {
  .update = rut_light_update
};

static void
_rut_light_free (void *object)
{
  RutLight *light = object;
  g_slice_free (RutLight, light);
}

static RutRefCountableVTable _rut_light_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_light_free
};

void
_rut_light_init_type (void)
{
  rut_type_init (&rut_light_type);
  rut_type_add_interface (&rut_light_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutLight, ref_count),
                          &_rut_light_ref_countable_vtable);
  rut_type_add_interface (&rut_light_type,
                           RUT_INTERFACE_ID_COMPONENTABLE,
                           offsetof (RutLight, component),
                           &_rut_light_componentable_vtable);
}

RutLight *
rut_light_new (void)
{
  RutLight *light;

  light = g_slice_new0 (RutLight);
  rut_object_init (&light->_parent, &rut_light_type);
  light->ref_count = 1;
  light->component.type = RUT_COMPONENT_TYPE_LIGHT;
  rut_color_init_from_4f (&light->ambient, 1.0, 1.0, 1.0, 1.0);
  rut_color_init_from_4f (&light->diffuse, 1.0, 1.0, 1.0, 1.0);
  rut_color_init_from_4f (&light->specular, 1.0, 1.0, 1.0, 1.0);

  return light;
}

void
rut_light_free (RutLight *light)
{
  g_slice_free (RutLight, light);
}

void
rut_light_set_ambient (RutLight  *light,
                       RutColor *ambient)
{
  light->ambient = *ambient;
}

const RutColor *
rut_light_get_ambient (RutLight *light)
{
  return &light->ambient;
}

void
rut_light_set_diffuse (RutLight  *light,
                       RutColor *diffuse)
{
  light->diffuse = *diffuse;
}

const RutColor *
rut_light_get_diffuse (RutLight *light)
{
  return &light->diffuse;
}

void
rut_light_set_specular (RutLight  *light,
                        RutColor *specular)
{
  light->specular = *specular;
}

const RutColor *
rut_light_get_specular (RutLight *light)
{
  return &light->specular;
}
