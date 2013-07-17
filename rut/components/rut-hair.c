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

#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "rut-hair.h"
#include "rut-global.h"

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
    .name = "hair-density",
    .nick = "Density",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = rut_hair_get_density,
    .setter.integer_type = rut_hair_set_density,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .int_range = { 500, INT_MAX }},
  },
  {
    .name = "hair-thickness",
    .nick = "Thickness",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .getter.float_type = rut_hair_get_thickness,
    .setter.float_type = rut_hair_set_thickness,
    .flags = RUT_PROPERTY_FLAG_READWRITE |
      RUT_PROPERTY_FLAG_VALIDATE,
    .validation = { .float_range = { 0.02, 100 }}
  },
  { NULL }
};

typedef struct _HairParticle{
  float lifetime;
  float diameter;
  float color[4];
  float position[3];
  float velocity[3];
  float acceleration[3];
}HairParticle;

float
_get_interpolated_value (float fmin,
                         float fmax,
                         float min,
                         float max,
                         float x)
{
  return (x - min) / (max - min) * (fmax - fmin) + fmin;
}

static float
_get_fuzzy_float (GRand *rand,
                  float value,
                  float variance)
{
  float v = variance / 2.0;

  return (float)g_rand_double_range (rand, value - v, value + v);
}

HairParticle *
_rut_hair_create_particle (float diameter,
                           float follicle_x,
                           float follicle_y,
                           float follicle_z)
{
  HairParticle *particle = g_new (HairParticle, 1);
  GRand *rand = g_rand_new ();
  float magnitude, speed;

  particle->lifetime = _get_fuzzy_float (rand, 0.75, 0.5);
  particle->diameter = diameter;
  particle->acceleration[0] = particle->acceleration[2] = 0;
  particle->acceleration[1] = (-1.0 * particle->lifetime) * 0.5;
  particle->position[0] = follicle_x;
  particle->position[1] = follicle_y;
  particle->position[2] = follicle_z;
  particle->color[0] = particle->color[1] = particle->color[2] = 0.5;
  particle->color[3] = 1.0;
  particle->velocity[0] = _get_fuzzy_float (rand, 0, 0.2);
  particle->velocity[1] = _get_fuzzy_float (rand, 0.75, 0.5);
  particle->velocity[2] = _get_fuzzy_float (rand, 0, 0.2);

  magnitude = sqrt (pow (particle->velocity[0], 2) +
                    pow (particle->velocity[1], 2) +
                    pow (particle->velocity[2], 2));

  particle->velocity[0] /= magnitude;
  particle->velocity[1] /= magnitude;
  particle->velocity[2] /= magnitude;

  speed = particle->lifetime * 0.5;
  particle->velocity[0] *= speed;
  particle->velocity[1] *= speed;
  particle->velocity[2] *= speed;

  g_rand_free (rand);

  return particle;
}

static float *
_get_updated_particle_color (HairParticle *particle,
                             float time)
{
  float *color = g_new (float, 4);
  float blur = particle->lifetime / 10.f;
  float kernel[4] = { 0.15, 0.12, 0.09, 0.05 };
  int i;

  color[0] = color[1] = color[2] = color[3] = 0;

  for (i = 1; i < 5; i++)
    {
      color[0] += _get_interpolated_value (0.5, 1, 0,
                                           particle->lifetime, time -
                                           (blur * i)) * kernel[i - 1];
      color[3] += _get_interpolated_value (1, 0.5, 0,
                                           particle->lifetime, time -
                                           (blur * i)) * kernel[i - 1];
    }

  color[0] += _get_interpolated_value (0.5, 1, 0,
                                       particle->lifetime, time) * 0.16;
  color[3] += _get_interpolated_value (1, 0.5, 0,
                                       particle->lifetime, time) * 0.16;

  for (i = 1; i < 5; i++)
    {
      color[0] += _get_interpolated_value (0.5, 1, 0,
                                           particle->lifetime, time +
                                           (blur * i)) * kernel[i - 1];
      color[3] += _get_interpolated_value (1, 0.5, 0,
                                           particle->lifetime, time +
                                           (blur * i)) * kernel[i - 1];
    }

  color[1] = color[2] = color[0];
  return color;
}

static float
_get_updated_particle_diameter (HairParticle *particle,
                                float time)
{
  return _get_interpolated_value (particle->diameter, 0, 0,
                                  particle->lifetime, time);
}

static float
_get_current_particle_time (HairParticle *particle,
                            float current_y)
{
  float v[3], t;

  v[1] = pow (particle->velocity[1], 2) + 2 *
         particle->acceleration[1] * current_y;

  if (v[1] < 0.0)
    return -1;
  else
    v[1] = sqrt (v[1]);

  t = (v[1] - particle->velocity[1]) / particle->acceleration[1];

  if (t > particle->lifetime)
    return -1;

  return t;
}

static float *
_get_updated_particle_velocity (HairParticle *particle,
                                float time)
{
  float *v = g_new (float, 3);

  v[0] = particle->velocity[0] + particle->acceleration[0] * time;
  v[1] = particle->velocity[1] + particle->acceleration[1] * time;
  v[2] = particle->velocity[2] + particle->acceleration[2] * time;

  return v;
}
static float *
_get_updated_particle_position (HairParticle *particle,
                                float *velocity,
                                float current_y,
                                float time)
{
  float *s = g_new (float, 3);

  s[0] = 0.5 * (particle->velocity[0] + velocity[0]) * time;
  s[1] = 0.5 * (particle->velocity[1] + velocity[1]) * time;
  s[2] = 0.5 * (particle->velocity[2] + velocity[2]) * time;

  if (s[1] > current_y + (current_y / 10.0) ||
      s[1] < current_y - (current_y / 10.0))
    {
      g_free (s);
      return NULL;
    }

  return s;
}

static HairParticle *
_rut_hair_get_updated_particle (HairParticle *particle,
                                float current_y)
{
  HairParticle *new_particle;
  float time, *v, *s, *color;
  int i;

  time = _get_current_particle_time (particle, current_y);

  if (time < 0.0)
    return NULL;

  v = _get_updated_particle_velocity (particle, time);
  s = _get_updated_particle_position (particle, v, current_y, time);

  if (!s)
    {
      g_free (v);
      return NULL;
    }

  color = _get_updated_particle_color (particle, time);

  new_particle = g_new (HairParticle, 1);
  new_particle->diameter = _get_updated_particle_diameter (particle, time);
  new_particle->lifetime = particle->lifetime - time;
  for (i = 0; i < 3; i++)
    {
      new_particle->velocity[i] = v[i];
      new_particle->position[i] = particle->position[i] + s[i] +
                                  particle->diameter;
      new_particle->color[i] = color[i];
    }

  new_particle->color[3] = color[3];
  g_free (v);
  g_free (s);
  g_free (color);

  return new_particle;
}

static CoglTexture *
_rut_hair_get_fin_texture (RutHair *hair)
{
  CoglOffscreen *offscreen;
  CoglPipeline *pipeline;
  CoglTexture *fin_texture;
  float current_y = -1;
  float geometric_y = -0.995;
  float geo_y_iter = 0.01;
  int fin_density = hair->density * 0.01;
  float y_iter = 0.01;
  int i;

  fin_texture = (CoglTexture*)
    cogl_texture_2d_new_with_size (hair->ctx->cogl_context, 1000, 1000,
                                   COGL_PIXEL_FORMAT_RGBA_8888);

  pipeline = cogl_pipeline_new (hair->ctx->cogl_context);

  offscreen = cogl_offscreen_new_with_texture (fin_texture);

  cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen),
                            COGL_BUFFER_BIT_COLOR, 0, 0, 0, 0);

  while (current_y <= 1.f)
    {
      HairParticle *particle, *updated_particle;
      float pos = _get_interpolated_value (0, 1, -1, 1, current_y);
      for (i = 0; i < fin_density; i++)
        {
          particle = g_queue_peek_nth (hair->particles, i);
          particle->diameter = hair->thickness;
          updated_particle = _rut_hair_get_updated_particle (particle, pos);

          if (updated_particle)
            {
              float x = _get_interpolated_value (-1, 1, 0, 1,
                updated_particle->position[0]);

              cogl_pipeline_set_color4f (pipeline, updated_particle->color[0],
                                         updated_particle->color[1],
                                         updated_particle->color[2],
                                         updated_particle->color[3]);

              cogl_framebuffer_draw_rectangle (
                COGL_FRAMEBUFFER (offscreen), pipeline,
                x - updated_particle->diameter / 2,
                geometric_y - geo_y_iter,
                x + updated_particle->diameter / 2,
                geometric_y + geo_y_iter);

              g_free (updated_particle);
            }
        }

      current_y += y_iter;
      geometric_y += geo_y_iter;
    }

  cogl_object_unref (offscreen);
  cogl_object_unref (pipeline);

  return fin_texture;
}

static void
_rut_hair_draw_shell_texture (RutHair *hair,
                              CoglTexture *shell_texture,
                              int position)
{
  CoglOffscreen *offscreen;
  CoglPipeline *pipeline;
  int i;

  pipeline = cogl_pipeline_new (hair->ctx->cogl_context);

  offscreen = cogl_offscreen_new_with_texture (shell_texture);

  cogl_framebuffer_clear4f (COGL_FRAMEBUFFER (offscreen),
                            COGL_BUFFER_BIT_COLOR, 0, 0, 0, 0);

  if (position == 0)
    {
      cogl_pipeline_set_color4f (pipeline, 0.75, 0.75, 0.75, 1.0);
      cogl_framebuffer_draw_rectangle (COGL_FRAMEBUFFER (offscreen), pipeline,
                                       -1, -1, 1, 1);
      return;
    }

  cogl_pipeline_set_layer_texture (pipeline, 0, hair->circle);

  for (i = 0; i < hair->density; i++)
    {
      HairParticle *particle, *updated_particle;
      float current_y = (float) position / (float) hair->n_shells;

      particle = g_queue_pop_head (hair->particles);
      particle->diameter = hair->thickness;
      updated_particle = _rut_hair_get_updated_particle (particle,
                                                         current_y);

      if (updated_particle)
        {

          cogl_pipeline_set_color4f (pipeline, updated_particle->color[0],
                                     updated_particle->color[1],
                                     updated_particle->color[2],
                                     updated_particle->color[3]);

          cogl_framebuffer_draw_rectangle (
            COGL_FRAMEBUFFER (offscreen), pipeline,
            updated_particle->position[0] - (updated_particle->diameter / 2.0),
            updated_particle->position[2] - (updated_particle->diameter / 2.0),
            updated_particle->position[0] + (updated_particle->diameter / 2.0),
            updated_particle->position[2] + (updated_particle->diameter / 2.0));

          g_free (updated_particle);
        }

      g_queue_push_tail (hair->particles, particle);
    }

  cogl_object_unref (offscreen);
  cogl_object_unref (pipeline);
}

static void
_rut_hair_generate_shell_textures (RutHair *hair)
{
  GRand *rand = g_rand_new ();
  int num_particles = g_queue_get_length (hair->particles);
  int num_textures = g_queue_get_length (hair->shell_textures);
  int i;

  if (hair->density > num_particles)
    {
      for (i = 0; i < hair->density - num_particles; i++)
        {
          float x = (float) g_rand_double_range (rand, -1, 1);
          float z = (float) g_rand_double_range (rand, -1, 1);
          HairParticle *particle = _rut_hair_create_particle (hair->thickness,
                                                              x, 0, z);
          g_queue_push_tail (hair->particles, particle);
        }
     }
  else if (hair->density < num_particles)
    {
      for (i = 0; i < num_particles - hair->density; i++)
        {
          HairParticle *particle = g_queue_pop_tail (hair->particles);
          g_free (particle);
        }
    }

  if (hair->n_shells + 1 > num_textures)
    {
      for (i = 0; i < (hair->n_shells + 1) - num_textures; i++)
        {
          CoglTexture *texture = (CoglTexture *)
            cogl_texture_2d_new_with_size (hair->ctx->cogl_context, 256, 256,
                                           COGL_PIXEL_FORMAT_RGBA_8888);
          g_queue_push_tail (hair->shell_textures, texture);
        }
    }
  else if (hair->n_shells + 1 < num_textures)
    {
      for (i = 0; i < num_textures - (hair->n_shells + 1); i++)
        {
          CoglTexture *texture = g_queue_pop_tail (hair->shell_textures);
          cogl_object_unref (texture);
        }
    }

  for (i = 0; i < hair->n_shells + 1; i++)
    {
      CoglTexture *texture = g_queue_pop_head (hair->shell_textures);
      _rut_hair_draw_shell_texture (hair, texture, i);
      g_queue_push_tail (hair->shell_textures, texture);
    }

  hair->n_textures = hair->n_shells + 1;
}

static void
_rut_hair_generate_hair_positions (RutHair *hair)
{
  float *new_positions;
  int i;

  new_positions = g_new (float, hair->n_shells + 1);

  if (hair->shell_positions)
    g_free (hair->shell_positions);

  for (i = 2; i < hair->n_shells + 1; i++)
    new_positions[i] = ((float) (i + 1) / (float) hair->n_shells) *
                       hair->length;

  new_positions[0] = new_positions[1] = 0;

  hair->shell_positions = new_positions;
}

static void
_rut_hair_free (void *object)
{
  RutHair *hair = object;

  while (!g_queue_is_empty (hair->shell_textures))
    {
      CoglTexture *texture = g_queue_pop_tail (hair->shell_textures);
      cogl_object_unref (texture);
    }

  while (!g_queue_is_empty (hair->particles))
    {
      HairParticle *particle = g_queue_pop_tail (hair->particles);
      g_free (particle);
    }

  rut_simple_introspectable_destroy (hair);

  if (hair->fin_texture)
    cogl_object_unref (hair->fin_texture);
  cogl_object_unref (hair->circle);
  g_free (hair->shell_positions);
  g_queue_free (hair->shell_textures);
  g_queue_free (hair->particles);
  g_slice_free (RutHair, hair);
}

static RutObject *
_rut_hair_copy (RutObject *obj)
{
  RutHair *hair = obj;
  RutHair *copy = rut_hair_new (hair->ctx);

  copy->length = hair->length;
  copy->n_shells = hair->n_shells;
  copy->n_textures = hair->n_textures;
  copy->density = hair->density;
  copy->thickness = hair->thickness;

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
  hair->n_shells = 50;
  hair->n_textures = 0;
  hair->density = 20000;
  hair->thickness = 0.05;
  hair->shell_textures = g_queue_new ();
  hair->fin_texture = NULL;
  hair->particles = g_queue_new ();
  hair->shell_positions = NULL;

  hair->circle = (CoglTexture*)
    cogl_texture_2d_new_from_file (hair->ctx->cogl_context,
                           				 rut_find_data_file ("circle1.png"),
                            			 COGL_PIXEL_FORMAT_ANY, NULL);

  rut_simple_introspectable_init (hair, _rut_hair_prop_specs,
                                  hair->properties);

  hair->dirty_hair_positions = TRUE;
  hair->dirty_shell_textures = TRUE;
  hair->dirty_fin_texture = TRUE;

  return hair;
}

void
rut_hair_update_state (RutHair *hair)
{
  if (hair->dirty_shell_textures)
    {
      _rut_hair_generate_shell_textures (hair);
      hair->dirty_shell_textures = FALSE;
    }

  if (hair->dirty_fin_texture)
    {
      if (hair->fin_texture)
        cogl_object_unref (hair->fin_texture);
      hair->fin_texture = _rut_hair_get_fin_texture (hair);
      hair->dirty_fin_texture = FALSE;
    }

  if (hair->dirty_hair_positions)
    {
      _rut_hair_generate_hair_positions (hair);
      hair->dirty_hair_positions = FALSE;
    }
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

  hair->dirty_hair_positions = TRUE;

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

  hair->dirty_hair_positions = TRUE;
  hair->dirty_shell_textures = TRUE;

  rut_property_dirty (&ctx->property_ctx,
                      &hair->properties[RUT_HAIR_PROP_DETAIL]);
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

  hair->dirty_shell_textures = TRUE;
  hair->dirty_fin_texture = TRUE;

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

  hair->dirty_shell_textures = TRUE;
  hair->dirty_fin_texture = TRUE;

  entity = hair->component.entity;
  ctx = rut_entity_get_context (entity);
  rut_property_dirty (&ctx->property_ctx,
                      &hair->properties[RUT_HAIR_PROP_THICKNESS]);
}

float
rut_hair_get_shell_position (RutObject *obj,
                             int shell)
{
  RutHair *hair = obj;

  return hair->shell_positions[shell];
}

void
rut_hair_set_uniform_location (RutObject *obj,
                               CoglPipeline *pln,
                               int uniform)
{
  RutHair *hair = obj;
  char *uniform_name = NULL;
  int location;

  if (uniform == RUT_HAIR_SHELL_POSITION_BLENDED ||
      uniform == RUT_HAIR_SHELL_POSITION_UNBLENDED ||
      uniform == RUT_HAIR_SHELL_POSITION_SHADOW)
    uniform_name = "hair_pos";
  else if (uniform == RUT_HAIR_LENGTH)
    uniform_name = "length";

  location = cogl_pipeline_get_uniform_location (pln, uniform_name);

  hair->uniform_locations[uniform] = location;
}

void
rut_hair_set_uniform_float_value (RutObject *obj,
                                  CoglPipeline *pln,
                                  int uniform,
                                  float value)
{
  RutHair *hair = obj;
  int location;

  location = hair->uniform_locations[uniform];

  cogl_pipeline_set_uniform_1f (pln, location, value);
}

