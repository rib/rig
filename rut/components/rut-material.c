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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "rut-material.h"
#include "rut-global.h"
#include "rut-asset.h"
#include "rut-color.h"

static RutPropertySpec _rut_material_prop_specs[] = {
  {
    .name = "ambient",
    .nick = "Ambient",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .getter = rut_material_get_ambient,
    .setter = rut_material_set_ambient,
    .flags = RUT_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "diffuse",
    .nick = "Diffuse",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .getter = rut_material_get_diffuse,
    .setter = rut_material_set_diffuse,
    .flags = RUT_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "specular",
    .nick = "Specular",
    .type = RUT_PROPERTY_TYPE_COLOR,
    .getter = rut_material_get_specular,
    .setter = rut_material_set_specular,
    .flags = RUT_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "shininess",
    .nick = "Shininess",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter = rut_material_get_shininess,
    .setter = rut_material_set_shininess,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .float_range = { 0, 1000 }}
  },
  { 0 }
};

RutType rut_material_type;

static RutComponentableVTable _rut_material_componentable_vtable = {
    0
};

static RutIntrospectableVTable _rut_material_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

void
_rut_material_init_type (void)
{
  rut_type_init (&rut_material_type);
  rut_type_add_interface (&rut_material_type,
                           RUT_INTERFACE_ID_COMPONENTABLE,
                           offsetof (RutMaterial, component),
                           &_rut_material_componentable_vtable);
  rut_type_add_interface (&rut_material_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_material_introspectable_vtable);
  rut_type_add_interface (&rut_material_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutMaterial, introspectable),
                          NULL); /* no implied vtable */

}

RutMaterial *
rut_material_new_full (RutContext *ctx,
                       RutAsset *texture_asset,
                       CoglPipeline *pipeline)
{
  RutMaterial *material = g_slice_new0 (RutMaterial);

  rut_object_init (&material->_parent, &rut_material_type);
  material->component.type = RUT_COMPONENT_TYPE_MATERIAL;

  rut_color_init_from_4f (&material->ambient, 0.23, 0.23, 0.23, 1);
  rut_color_init_from_4f (&material->diffuse, 0.75, 0.75, 0.75, 1);
  rut_color_init_from_4f (&material->specular, 0.64, 0.64, 0.64, 1);

  material->shininess = 100;

  rut_simple_introspectable_init (material,
                                  _rut_material_prop_specs,
                                  material->properties);

  material->uniforms_flush_age = -1;

  if (texture_asset)
    material->texture_asset = rut_refable_ref (texture_asset);

  return material;
}

RutMaterial *
rut_material_new (RutContext *ctx,
                  RutAsset *texture_asset)
{
  return rut_material_new_full (ctx, texture_asset, NULL);
}

void
rut_material_set_texture_asset (RutMaterial *material,
                                RutAsset *texture_asset)
{
  if (material->texture_asset)
    {
      rut_refable_unref (material->texture_asset);
      material->texture_asset = NULL;
    }

  if (texture_asset)
    material->texture_asset = rut_refable_ref (texture_asset);
}

RutAsset *
rut_material_get_texture_asset (RutMaterial *material)
{
  return material->texture_asset;
}

void
rut_material_set_normal_map_asset (RutMaterial *material,
                                   RutAsset *normal_map_asset)
{
  if (material->normal_map_asset)
    {
      rut_refable_unref (material->normal_map_asset);
      material->normal_map_asset = NULL;
    }

  if (normal_map_asset)
    material->normal_map_asset = rut_refable_ref (normal_map_asset);
}

RutAsset *
rut_material_get_normal_map_asset (RutMaterial *material)
{
  return material->normal_map_asset;
}

void
rut_material_set_ambient (RutMaterial *material,
                          const RutColor *color)
{
  material->ambient = *color;
  material->uniforms_age++;
}

const RutColor *
rut_material_get_ambient (RutMaterial *material)
{
  return &material->ambient;
}

void
rut_material_set_diffuse (RutMaterial *material,
                          const RutColor *color)
{
  material->diffuse = *color;
  material->uniforms_age++;

}

const RutColor *
rut_material_get_diffuse (RutMaterial *material)
{
  return &material->diffuse;
}

void
rut_material_set_specular (RutMaterial *material,
                           const RutColor *color)
{
  material->specular = *color;
  material->uniforms_age++;
}

const RutColor *
rut_material_get_specular (RutMaterial *material)
{
  return &material->specular;
}

void
rut_material_set_shininess (RutMaterial *material,
                            float shininess)
{
  material->shininess = shininess;
  material->uniforms_age++;
}

float
rut_material_get_shininess (RutMaterial *material)
{
  return material->shininess;
}

void
rut_material_flush_uniforms (RutMaterial *material,
                             CoglPipeline *pipeline)
{
  int location;

  //if (material->uniforms_age == material->uniforms_flush_age)
  //  return;

  location = cogl_pipeline_get_uniform_location (pipeline, "material_ambient");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   4, 1,
                                   (float *)&material->ambient);

  location = cogl_pipeline_get_uniform_location (pipeline, "material_diffuse");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   4, 1,
                                   (float *)&material->diffuse);

  location = cogl_pipeline_get_uniform_location (pipeline, "material_specular");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   4, 1,
                                   (float *)&material->specular);

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "material_shininess");
  cogl_pipeline_set_uniform_1f (pipeline, location, material->shininess);

  material->uniforms_flush_age = material->uniforms_age;
}

void
rut_material_dirty_uniforms (RutMaterial *material)
{
  material->uniforms_flush_age = material->uniforms_age -1;
}
