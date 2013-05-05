/*
* Rut
*
* Copyright (C) 2012 Intel Corporation
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library. If not, see
* <http://www.gnu.org/licenses/>.
*/

#include "rut-hair.h"
#include "rut-global.h"
#include "math.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

static RutPropertySpec _rut_hair_prop_specs[] = {
  {
    .name = "hair-length",
    .nick = "Length",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rut_hair_get_length,
    .setter.float_type = rut_hair_set_length,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .float_range = { 0, 1000 }},
    .animatable = TRUE
  },
  {
    .name = "hair-detail",
    .nick = "Detail",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = rut_hair_get_n_shells,
    .setter.integer_type = rut_hair_set_n_shells,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .int_range = { -1, INT_MAX }},
  },
  {
    .name = "hair-gravity",
    .nick = "Gravity",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rut_hair_get_gravity,
    .setter.float_type = rut_hair_set_gravity,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .float_range = { 0, 100 }},
    .animatable = TRUE
  },
  {
    .name = "hair-density",
    .nick = "Density",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = rut_hair_get_density,
    .setter.integer_type = rut_hair_set_density,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .int_range = { -1, INT_MAX }},
  },
  {
    .name = "hair-thickness",
    .nick = "Thickness",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rut_hair_get_thickness,
    .setter.float_type = rut_hair_set_thickness,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .float_range = { 0, 100 }}
  },
  {
    .name = "hair-resolution",
    .nick = "Resolution",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = rut_hair_get_resolution,
    .setter.integer_type = rut_hair_set_resolution,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .int_range = { -1, INT_MAX }},
  },
  { NULL }
};

static void
_rut_hair_generate_textures (RutHair *hair)
{
  CoglPipeline *pipeline;
  float diameter, diameter_iter;
  int i;

  if (hair->textures)
    {
      for (i = 0; i < sizeof (hair->textures) / sizeof (CoglTexture*); i++)
        cogl_object_unref (hair->textures[i]);

      g_free (hair->textures);
    }

  hair->textures = g_new (CoglTexture*, hair->resolution);

  diameter = hair->thickness;
  diameter_iter = hair->thickness / hair->resolution;
  pipeline = cogl_pipeline_new (hair->ctx->cogl_context);
  cogl_pipeline_set_layer_texture (pipeline, 0, hair->circle);

  for (i = 0; i < hair->resolution; i++)
    {
      CoglOffscreen *offscreen;
      int j;
      hair->textures[i] = COGL_TEXTURE (
        cogl_texture_2d_new_with_size (hair->ctx->cogl_context,
                                       256, 256,
                                       COGL_PIXEL_FORMAT_RGBA_8888_PRE));

      offscreen = cogl_offscreen_new_with_texture (hair->textures[i]);

      cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen),
                                COGL_BUFFER_BIT_COLOR, 0, 0, 0, 0);

      cogl_pipeline_set_color4f (pipeline, 0.3, 0.3, 0.3, 1.0);

      srand (7025);
      for (j = 0; j < hair->density; j++)
        {
          float x = ((rand() / (float)RAND_MAX) * 2) - 1;
          float y = ((rand() / (float)RAND_MAX) * 2) - 1;

          cogl_framebuffer_draw_rectangle (COGL_FRAMEBUFFER (offscreen),
                                           pipeline,
                                           x - (diameter / 2.0),
                                           y - (diameter / 2.0),
                                           x + (diameter / 2.0),
                                           y + (diameter / 2.0));
        }

      cogl_pipeline_set_color4f (pipeline, 1, 1, 1, 1);

      srand (7025);
      for (j = 0; j < hair->density; j++)
        {
          float x = ((rand() / (float)RAND_MAX) * 2) - 1;
          float y = ((rand() / (float)RAND_MAX) * 2) - 1;
          float shadow_offset = diameter / 4.0f;
          x += shadow_offset;
          y += shadow_offset;

          cogl_framebuffer_draw_rectangle (COGL_FRAMEBUFFER (offscreen),
                                           pipeline,
                                           x - (diameter / 2.0),
                                           y - (diameter / 2.0),
                                           x + (diameter / 2.0),
                                           y + (diameter / 2.0));
        }

      cogl_object_unref (offscreen);

      diameter -= diameter_iter;
    }

  hair->dirty_textures = FALSE;
}

static void
_rut_hair_free (void *object)
{
  RutHair *hair = object;

  rut_simple_introspectable_destroy (hair);

  g_slice_free (RutHair, hair);
}

static RutObject *
_rut_hair_copy (RutObject *obj)
{
  RutHair *hair = obj;
  RutHair *copy = rut_hair_new (hair->ctx);

  copy->length = hair->length;
  copy->n_shells = hair->n_shells;
  copy->gravity = hair->gravity;
  copy->density = hair->density;
  copy->thickness = hair->thickness;
  copy->resolution = hair->resolution;

  copy->dirty_textures = TRUE;

  return copy;
}

RutType rut_hair_type;

void
_rut_hair_init_type (void)
{
  static RutRefableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_hair_free
  };

  static RutIntrospectableVTable introspectable_vtable = {
      rut_simple_introspectable_lookup_property,
      rut_simple_introspectable_foreach_property
  };

  static RutComponentableVTable componentable_vtable = {
      .copy = _rut_hair_copy
  };

  RutType *type = &rut_hair_type;
#define TYPE RutHair

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_COMPONENTABLE,
                          offsetof (TYPE, component),
                          &componentable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &introspectable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (TYPE, introspectable),
                          NULL); /* no implied vtable */

#undef TYPE
}

RutHair *
rut_hair_new (RutContext *ctx)
{
  RutHair *hair = g_slice_new0 (RutHair);

  rut_object_init (&hair->_parent, &rut_hair_type);

  hair->ref_count = 1;

  hair->component.type = RUT_COMPONENT_TYPE_HAIR;

  hair->ctx = rut_refable_ref (ctx);

  hair->length = 100;
  hair->n_shells = 100;
  hair->gravity = 0.5;
  hair->density = 3000;
  hair->thickness = 0.02;
  hair->resolution = 100;
  hair->textures = NULL;
  hair->circle = COGL_TEXTURE (
    cogl_texture_2d_new_from_file (hair->ctx->cogl_context,
                                   rut_find_data_file ("circle1.png"),
                                   COGL_PIXEL_FORMAT_ANY,
                                   NULL));

  rut_simple_introspectable_init (hair, _rut_hair_prop_specs,
                                  hair->properties);

  hair->dirty_textures = TRUE;

  return hair;
}


CoglTexture *
rut_hair_get_texture (RutObject *obj, int layer)
{
  RutHair *hair = obj;

  if (hair->dirty_textures)
    _rut_hair_generate_textures (hair);

  return hair->textures[layer];
}

float
rut_hair_get_length (RutObject *obj)
{
  RutHair *hair = RUT_HAIR (obj);

  return hair->length;
}

void
rut_hair_set_length (RutObject *obj,
                     float length)
{
  RutHair *hair = RUT_HAIR (obj);
  RutEntity *entity;
  RutContext *ctx;

  if (length == hair->length)
    return;

  hair->length = length;

  entity = hair->component.entity;
  ctx = rut_entity_get_context (entity);
  rut_property_dirty (&ctx->property_ctx,
                     &hair->properties[RUT_HAIR_PROP_LENGTH]);
}

int
rut_hair_get_n_shells (RutObject *obj)
{
  RutHair *hair = RUT_HAIR (obj);

  return hair->n_shells;
}

void
rut_hair_set_n_shells (RutObject *obj,
                       int n_shells)
{
  RutHair *hair = RUT_HAIR (obj);
  RutEntity *entity;
  RutContext *ctx;

  if (n_shells == hair->n_shells)
    return;

  hair->n_shells = n_shells;

  entity = hair->component.entity;
  ctx = rut_entity_get_context (entity);

  rut_property_dirty (&ctx->property_ctx,
                      &hair->properties[RUT_HAIR_PROP_DETAIL]);
}

float
rut_hair_get_gravity (RutObject *obj)
{
  RutHair *hair = RUT_HAIR (obj);
  return hair->gravity;
}

void
rut_hair_set_gravity (RutObject *obj,
                      float gravity)
{
  RutHair *hair = RUT_HAIR (obj);
  RutEntity *entity;
  RutContext *ctx;

  if (gravity == hair->gravity)
    return;

  hair->gravity = gravity;

  entity = hair->component.entity;
  ctx = rut_entity_get_context (entity);
  rut_property_dirty (&ctx->property_ctx,
                     &hair->properties[RUT_HAIR_PROP_GRAVITY]);
}

int
rut_hair_get_density (RutObject *obj)
{
  RutHair *hair = obj;
  return hair->density;
}

void
rut_hair_set_density (RutObject *obj,
                      int density)
{
  RutHair *hair = obj;
  RutEntity *entity;
  RutContext *ctx;

  if (density == hair->density)
    return;

  hair->density = density;

  hair->dirty_textures = TRUE;

  entity = hair->component.entity;
  ctx = rut_entity_get_context (entity);
  rut_property_dirty (&ctx->property_ctx,
                      &hair->properties[RUT_HAIR_PROP_DENSITY]);
}

float
rut_hair_get_thickness (RutObject *obj)

{
  RutHair *hair = obj;
  return hair->thickness;
}

void
rut_hair_set_thickness (RutObject *obj,
                        float thickness)
{
  RutHair *hair = obj;
  RutEntity *entity;
  RutContext *ctx;

  if (thickness == hair->thickness)
    return;

  hair->thickness = thickness;

  hair->dirty_textures = TRUE;

  entity = hair->component.entity;
  ctx = rut_entity_get_context (entity);
  rut_property_dirty (&ctx->property_ctx,
                      &hair->properties[RUT_HAIR_PROP_THICKNESS]);
}

int
rut_hair_get_resolution (RutObject *obj)
{
  RutHair *hair = obj;
  return hair->resolution;
}

void
rut_hair_set_resolution (RutObject *obj,
                         int resolution)
{
  RutHair *hair = obj;
  RutEntity *entity;
  RutContext *ctx;

  if (resolution == hair->resolution)
    return;

  hair->resolution = resolution;

  hair->dirty_textures = TRUE;

  entity = hair->component.entity;
  ctx = rut_entity_get_context (entity);
  rut_property_dirty (&ctx->property_ctx,
                      &hair->properties[RUT_HAIR_PROP_RESOLUTION]);
}
