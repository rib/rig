/*
 * Rig
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

#include "rig-light.h"
#include "rig-color.h"

static float *
get_color_array (RigColor *color)
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
                        CoglPipeline *pipeline)
{
  RigComponentableProps *component =
    rig_object_get_properties (light, RIG_INTERFACE_ID_COMPONENTABLE);
  RigEntity *entity = component->entity;
  float norm_direction[3];
  int location;

  /* the lighting shader expects the direction vector to be pointing towards
   * the light, we encode that with the light position in the case of a
   * directional light */
  norm_direction[0] = rig_entity_get_x (entity);
  norm_direction[1] = rig_entity_get_y (entity);
  norm_direction[2] = rig_entity_get_z (entity);
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
rig_light_update (RigObject *object,
                  int64_t time)
{
}

RigType rig_light_type;

static RigComponentableVTable _rig_light_componentable_vtable = {
  .update = rig_light_update
};

void
_rig_light_init_type (void)
{
  rig_type_init (&rig_light_type);
  rig_type_add_interface (&rig_light_type,
                           RIG_INTERFACE_ID_COMPONENTABLE,
                           offsetof (RigLight, component),
                           &_rig_light_componentable_vtable);
}

RigLight *
rig_light_new (void)
{
  RigLight *light;

  light = g_slice_new0 (RigLight);
  rig_object_init (&light->_parent, &rig_light_type);
  light->component.type = RIG_COMPONENT_TYPE_LIGHT;
  rig_color_init_from_4f (&light->ambient, 1.0, 1.0, 1.0, 1.0);
  rig_color_init_from_4f (&light->diffuse, 1.0, 1.0, 1.0, 1.0);
  rig_color_init_from_4f (&light->specular, 1.0, 1.0, 1.0, 1.0);

  return light;
}

void
rig_light_free (RigLight *light)
{
  g_slice_free (RigLight, light);
}

void
rig_light_set_ambient (RigLight  *light,
                       RigColor *ambient)
{
  light->ambient = *ambient;
}

const RigColor *
rig_light_get_ambient (RigLight *light)
{
  return &light->ambient;
}

void
rig_light_set_diffuse (RigLight  *light,
                       RigColor *diffuse)
{
  light->diffuse = *diffuse;
}

const RigColor *
rig_light_get_diffuse (RigLight *light)
{
  return &light->diffuse;
}

void
rig_light_set_specular (RigLight  *light,
                        RigColor *specular)
{
  light->specular = *specular;
}

const RigColor *
rig_light_get_specular (RigLight *light)
{
  return &light->specular;
}
