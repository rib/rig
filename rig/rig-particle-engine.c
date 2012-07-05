/*
 * Rig
 *
 * A tiny toolkit
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include <cogl/cogl.h>
#include <string.h>

#include "rig.h"
#include "rig-particle-engine.h"
#include "rig-flags.h"

typedef struct
{
  uint8_t r, g, b, a;
} RigParticleEngineColor;

typedef struct
{
  float initial_position[3];
  float initial_velocity[3];
  RigParticleEngineColor initial_color;

  /* Time of creation in milliseconds relative to the start of the
   * engine when the particle was created */
  int32_t creation_time;
  /* The maximum age of this particle in msecs. The particle will
   * linearly fade out until this age */
  int32_t max_age;
} RigParticleEngineParticle;

typedef struct
{
  float position[3];
  float point_size;
  RigParticleEngineColor color;
} RigParticleEngineVertex;

struct _RigParticleEngine
{
  RigObjectProps _parent;

  RigContext *context;

  /* Configurable options that affect the resources */
  CoglTexture *texture;
  GArray *colors;
  /* Maximum number of particles to generate */
  int max_particles;

  /* Resources that get freed and lazily recreated whenever the
   * above options change */
  CoglPipeline *pipeline;
  RigParticleEngineParticle *particles;
  /* Bit array of used particles */
  int used_particles_n_longs;
  unsigned long *used_particles;
  CoglAttributeBuffer *attribute_buffer;
  CoglPrimitive *primitive;

  /* The unused particles are linked together in a linked list so we
   * can quickly find the next free one. The particle structure is
   * reused as a list node to avoid allocating anything */
  RigParticleEngineParticle *next_unused_particle;

  GRand *rand;

  /* The time (as set by rig_particle_engine_set_time) that was
   * current when the particles were last updated */
  int32_t last_update_time;
  /* The next time that we should generate a new particle */
  int32_t next_particle_time;
  /* The 'current' time within the animation */
  int32_t current_time;

  RigPaintableProps paintable_props;

  float point_size;

  int ref_count;
};

RigType rig_particle_engine_type;

static CoglPipeline *
_rig_particle_engine_create_pipeline (RigParticleEngine *engine)
{
  CoglPipeline *pipeline = cogl_pipeline_new (engine->context->cogl_context);

  /* Add all of the textures to a separate layer */
  if (engine->texture)
    {
      cogl_pipeline_set_layer_texture (pipeline,
                                       0,
                                       engine->texture);
      cogl_pipeline_set_layer_wrap_mode (pipeline,
                                         0,
                                         COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
      cogl_pipeline_set_layer_point_sprite_coords_enabled (pipeline,
                                                           0, /* layer */
                                                           TRUE, /* enable */
                                                           NULL /* error */);
    }

  cogl_pipeline_set_per_vertex_point_size (pipeline, TRUE, NULL);

  return pipeline;
}

static void
_rig_particle_engine_create_resources (RigParticleEngine *engine)
{
  if (engine->pipeline == NULL)
    {
      CoglAttribute *attributes[3];
      int i;

      engine->pipeline = _rig_particle_engine_create_pipeline (engine);

      engine->particles =
        g_new (RigParticleEngineParticle, engine->max_particles);

      engine->used_particles_n_longs =
        RIG_FLAGS_N_LONGS_FOR_SIZE (engine->max_particles);
      engine->used_particles = g_new0 (unsigned long,
                                       engine->used_particles_n_longs);
      /* To begin with none of the particles are used */
      memset (engine->used_particles,
              0,
              engine->used_particles_n_longs * sizeof (unsigned long));

      /* Link together all of the particles in a linked list so we can
       * quickly find unused particles */
      *(RigParticleEngineParticle **) &engine->particles[0] = NULL;
      for (i = 1; i < engine->max_particles; i++)
          *(RigParticleEngineParticle **) &engine->particles[i] =
            engine->particles + i - 1;
      engine->next_unused_particle = engine->particles + i - 1;

      engine->attribute_buffer =
        cogl_attribute_buffer_new (engine->context->cogl_context,
                                   sizeof (RigParticleEngineVertex) *
                                   engine->max_particles,
                                   NULL /* data */);

      attributes[0] =
        cogl_attribute_new (engine->attribute_buffer,
                            "cogl_position_in",
                            sizeof (RigParticleEngineVertex),
                            G_STRUCT_OFFSET (RigParticleEngineVertex,
                                             position),
                            3, /* n_components */
                            COGL_ATTRIBUTE_TYPE_FLOAT);

      attributes[1] =
        cogl_attribute_new (engine->attribute_buffer,
                            "cogl_color_in",
                            sizeof (RigParticleEngineVertex),
                            G_STRUCT_OFFSET (RigParticleEngineVertex,
                                             color),
                            4, /* n_components */
                            COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

      attributes[2] =
        cogl_attribute_new (engine->attribute_buffer,
                            "cogl_point_size_in",
                            sizeof (RigParticleEngineVertex),
                            G_STRUCT_OFFSET (RigParticleEngineVertex,
                                             point_size),
                            1, /* n_components */
                            COGL_ATTRIBUTE_TYPE_FLOAT);

      engine->primitive =
        cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_POINTS,
                                            engine->max_particles,
                                            attributes,
                                            G_N_ELEMENTS (attributes));

      for (i = 0; i < G_N_ELEMENTS (attributes); i++)
        cogl_object_unref (attributes[i]);

      engine->last_update_time = engine->current_time;
      engine->next_particle_time = engine->current_time;
    }
}

static int32_t
_rig_particle_engine_get_next_particle_time (RigParticleEngine *engine,
                                             int32_t last_particle_time)
{
  /* TODO: this should have some parameters with some configurable
   * randomness */
  return last_particle_time + g_rand_int_range (engine->rand, 1, 16);
}

static void
_rig_particle_engine_get_initial_position (RigParticleEngine *engine,
                                           float position[3])
{
  /* TODO: make the initial position configurable with some randomness */
  position[0] = g_rand_double_range (engine->rand, -40.0f, 40.0f);
  position[1] = g_rand_double_range (engine->rand, -40.0f, 40.0f);
  position[2] = 0.0f;
}

static void
_rig_particle_engine_get_initial_velocity (RigParticleEngine *engine,
                                           float velocity[3])
{
  /* TODO: make the initial velocity configurable with some randomness */
  velocity[0] = g_rand_double_range (engine->rand, -200.0f, 200.0f);
  velocity[1] = g_rand_double_range (engine->rand, -300.0f, 100.0f);
  velocity[2] = 0.0f;
}

static void
_rig_particle_engine_get_initial_color (RigParticleEngine *engine,
                                        RigParticleEngineColor *color_out)
{
  if (engine->colors->len > 0)
    {
      int color_num = g_rand_int_range (engine->rand, 0, engine->colors->len);
      *color_out = g_array_index (engine->colors,
                                  RigParticleEngineColor,
                                  color_num);
    }
  else
    memset (color_out, 0xff, sizeof (RigParticleEngineColor));
}

static int32_t
_rig_particle_engine_get_max_age (RigParticleEngine *engine)
{
  /* TODO: make this a configurable option with some randomness */
  return 1000;
}

static void
_rig_particle_engine_initialise_particle (RigParticleEngine *engine,
                                          RigParticleEngineParticle *particle)
{
  _rig_particle_engine_get_initial_position (engine,
                                             particle->initial_position);
  _rig_particle_engine_get_initial_velocity (engine,
                                             particle->initial_velocity);
  _rig_particle_engine_get_initial_color (engine, &particle->initial_color);
  particle->max_age =
    _rig_particle_engine_get_max_age (engine);

  particle->creation_time = engine->current_time;
}

static void
_rig_particle_engine_create_new_particles (RigParticleEngine *engine)
{
  /* Fill in any unused particles until we either run out of unused
   * particles or we've used up enough time */
  while (engine->next_unused_particle &&
         engine->current_time > engine->next_particle_time)
    {
      RigParticleEngineParticle *particle = engine->next_unused_particle;
      int last_particle_time;

      engine->next_unused_particle =
        *(RigParticleEngineParticle **) particle;

      _rig_particle_engine_initialise_particle (engine, particle);

      RIG_FLAGS_SET (engine->used_particles,
                     particle - engine->particles,
                     TRUE);

      last_particle_time = engine->next_particle_time;
      engine->next_particle_time =
        _rig_particle_engine_get_next_particle_time (engine,
                                                     last_particle_time);

    }
}

static void
_rig_particle_engine_calculate_color (RigParticleEngine *engine,
                                      const RigParticleEngineParticle *particle,
                                      float t,
                                      RigParticleEngineColor *color)
{
  *color = particle->initial_color;
}

static void
_rig_particle_engine_calculate_position (RigParticleEngine *engine,
                                         const RigParticleEngineParticle *
                                           particle,
                                         float t,
                                         float position[3])
{
  static const float acceleration[3] = { 0.0f, 500.0f, 0.0f };
  float elapsed_time = particle->max_age * t / 1000.0f;
  float half_elapsed_time2 = elapsed_time * elapsed_time * 0.5f;
  int i;

  for (i = 0; i < 3; i++)
    {
      /* s = ut + 0.5×(at²)
       * (s = displacement, u = initial velocity, a = acceleration, t = time) */
      position[i] = (particle->initial_position[i] +
                     particle->initial_velocity[i] * elapsed_time +
                     acceleration[i] * half_elapsed_time2);
    }
}

static float
_rig_particle_engine_calculate_point_size (RigParticleEngine *engine,
                                           const RigParticleEngineParticle *
                                             particle,
                                           float t)
{
  return 16.0f * (1.0f - t);
}

static void
_rig_particle_engine_update (RigParticleEngine *engine)
{
  int particle_num;
  RigParticleEngineVertex *data;
  RigParticleEngineVertex *v;

  _rig_particle_engine_create_resources (engine);
  _rig_particle_engine_create_new_particles (engine);

  v = data = cogl_buffer_map (COGL_BUFFER (engine->attribute_buffer),
                              COGL_BUFFER_ACCESS_WRITE,
                              COGL_BUFFER_MAP_HINT_DISCARD);

  RIG_FLAGS_FOREACH_START (engine->used_particles,
                           engine->used_particles_n_longs,
                           particle_num)
    {
      RigParticleEngineParticle *particle = engine->particles + particle_num;
      int32_t particle_age = engine->current_time - particle->creation_time;

      if (particle_age >= particle->max_age)
        {
          /* Destroy the particle */
          RIG_FLAGS_SET (engine->used_particles,
                         particle - engine->particles,
                         FALSE);
          *(RigParticleEngineParticle **) particle =
            engine->next_unused_particle;
          engine->next_unused_particle = particle;
        }
      else
        {
          float t = particle_age / (float) particle->max_age;

          _rig_particle_engine_calculate_color (engine,
                                                particle,
                                                t,
                                                &v->color);

          _rig_particle_engine_calculate_position (engine,
                                                   particle,
                                                   t,
                                                   v->position);
          v->point_size =
            _rig_particle_engine_calculate_point_size (engine,
                                                       particle,
                                                       t);

          v++;
        }
    }
  RIG_FLAGS_FOREACH_END;

  engine->last_update_time = engine->current_time;

  cogl_buffer_unmap (COGL_BUFFER (engine->attribute_buffer));

  cogl_primitive_set_n_vertices (engine->primitive, v - data);
}

static void
_rig_particle_engine_clear_resources (RigParticleEngine *engine)
{
  if (engine->pipeline)
    {
      g_free (engine->particles);
      g_free (engine->used_particles);
      cogl_object_unref (engine->pipeline);
      engine->pipeline = NULL;
      cogl_object_unref (engine->attribute_buffer);
      cogl_object_unref (engine->primitive);
    }
}

static void
_rig_particle_engine_free (void *object)
{
  RigParticleEngine *engine = object;

  _rig_particle_engine_clear_resources (engine);

  rig_ref_countable_unref (engine->context);

  if (engine->texture)
    cogl_object_unref (engine->texture);

  g_rand_free (engine->rand);

  g_slice_free (RigParticleEngine, engine);
}

RigRefCountableVTable _rig_particle_engine_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_particle_engine_free
};

static void
_rig_particle_engine_paint (RigObject *object,
                            RigPaintContext *paint_ctx)
{
  RigParticleEngine *engine = (RigParticleEngine *) object;
  CoglFramebuffer *fb;

  _rig_particle_engine_update (engine);

  fb = rig_camera_get_framebuffer (paint_ctx->camera);
  cogl_framebuffer_draw_primitive (fb,
                                   engine->pipeline,
                                   engine->primitive);
}

RigPaintableVTable _rig_particle_engine_paintable_vtable = {
  _rig_particle_engine_paint
};

static void
_rig_particle_engine_init_type (void)
{
  rig_type_init (&rig_particle_engine_type);
  rig_type_add_interface (&rig_particle_engine_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigParticleEngine, ref_count),
                          &_rig_particle_engine_ref_countable_vtable);
  rig_type_add_interface (&rig_particle_engine_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigParticleEngine, paintable_props),
                          &_rig_particle_engine_paintable_vtable);
}

void
rig_particle_engine_set_time (RigParticleEngine *engine,
                              int32_t msecs)
{
  engine->current_time = msecs;
}

RigParticleEngine *
rig_particle_engine_new (RigContext *context)
{
  RigParticleEngine *engine = g_slice_new0 (RigParticleEngine);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_particle_engine_init_type ();

      initialized = TRUE;
    }

  engine->ref_count = 1;
  engine->context = rig_ref_countable_ref (context);
  engine->texture = NULL;
  engine->colors = g_array_new (FALSE, FALSE, sizeof (RigParticleEngineColor));

  engine->max_particles = 256;
  engine->point_size = 16.0f;

  engine->rand = g_rand_new ();

  rig_object_init (&engine->_parent, &rig_particle_engine_type);

  return engine;
}

void
rig_particle_engine_add_color (RigParticleEngine *engine,
                               const uint8_t color[4])
{
  g_array_set_size (engine->colors, engine->colors->len + 1);
  memcpy (&g_array_index (engine->colors,
                          RigParticleEngineColor,
                          engine->colors->len - 1),
          color,
          sizeof (RigParticleEngineColor));
}

void
rig_particle_engine_remove_color (RigParticleEngine *engine,
                                  const uint8_t color[4])
{
  int i;

  for (i = 0; i < engine->colors->len; i++)
    if (!memcmp (&g_array_index (engine->colors, RigParticleEngineColor, i),
                 color,
                 sizeof (RigParticleEngineColor)))
      {
        g_array_remove_index (engine->colors, i);
        break;
      }
}

void
rig_particle_engine_set_texture (RigParticleEngine *engine,
                                 CoglTexture *texture)
{
  _rig_particle_engine_clear_resources (engine);
  if (texture)
    cogl_object_ref (texture);
  if (engine->texture)
    cogl_object_unref (engine->texture);
  engine->texture = texture;
}
