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
#include "rig-ease.h"

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

enum {
  RIG_PARTICLE_ENGINE_PROP_MAX_PARTICLES,
  RIG_PARTICLE_ENGINE_PROP_MIN_INITIAL_VELOCITY,
  RIG_PARTICLE_ENGINE_PROP_MAX_INITIAL_VELOCITY,
  RIG_PARTICLE_ENGINE_PROP_MIN_INITIAL_POSITION,
  RIG_PARTICLE_ENGINE_PROP_MAX_INITIAL_POSITION,
  RIG_PARTICLE_ENGINE_PROP_SIZE_EASE_MODE,
  RIG_PARTICLE_ENGINE_N_PROPS
};

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

  RigGraphableProps graphable;
  RigPaintableProps paintable;

  float min_initial_velocity[3];
  float max_initial_velocity[3];

  float min_initial_position[3];
  float max_initial_position[3];

  float point_size;

  RigEaseMode size_ease_mode;

  int ref_count;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_PARTICLE_ENGINE_N_PROPS];
};

RigType rig_particle_engine_type;

#define RIG_PARTICLE_ENGINE_VERTEX_PROP_SPEC(prop_name)         \
  {                                                             \
    .name = G_STRINGIFY (prop_name),                            \
      .type = RIG_PROPERTY_TYPE_VEC3,                           \
      .data_offset = offsetof (RigParticleEngine, prop_name),   \
      .setter = rig_particle_engine_set_ ## prop_name           \
      }

#define RIG_PARTICLE_ENGINE_VERTEX_PROP_SPEC_RANGE(name)        \
  RIG_PARTICLE_ENGINE_VERTEX_PROP_SPEC (min_ ## name),          \
    RIG_PARTICLE_ENGINE_VERTEX_PROP_SPEC (max_ ## name)

static RigPropertySpec
_rig_particle_engine_prop_specs[] =
  {
    {
      .name = "max_particles",
      .type = RIG_PROPERTY_TYPE_INTEGER,
      .data_offset = offsetof (RigParticleEngine, max_particles),
      .setter = rig_particle_engine_set_max_particles,
    },

    {
      .name = "size_ease_mode",
      .type = RIG_PROPERTY_TYPE_ENUM,
      .data_offset = offsetof (RigParticleEngine, size_ease_mode),
      .flags = RIG_PROPERTY_FLAG_VALIDATE,
      .validation.ui_enum = &rig_ease_mode_enum
    },

    RIG_PARTICLE_ENGINE_VERTEX_PROP_SPEC_RANGE (initial_velocity),
    RIG_PARTICLE_ENGINE_VERTEX_PROP_SPEC_RANGE (initial_position),
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
  };

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
  int i;

  for (i = 0; i < 3; i++)
    position[i] = g_rand_double_range (engine->rand,
                                       engine->min_initial_position[i],
                                       engine->max_initial_position[i]);
}

static void
_rig_particle_engine_get_initial_velocity (RigParticleEngine *engine,
                                           float velocity[3])
{
  int i;

  for (i = 0; i < 3; i++)
    velocity[i] = g_rand_double_range (engine->rand,
                                       engine->min_initial_velocity[i],
                                       engine->max_initial_velocity[i]);
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

  rig_simple_introspectable_destroy (engine);

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

static RigGraphableVTable _rig_particle_engine_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static RigPaintableVTable _rig_particle_engine_paintable_vtable = {
  _rig_particle_engine_paint
};

static RigIntrospectableVTable _rig_particle_engine_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
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
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigParticleEngine, graphable),
                          &_rig_particle_engine_graphable_vtable);
  rig_type_add_interface (&rig_particle_engine_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigParticleEngine, paintable),
                          &_rig_particle_engine_paintable_vtable);
  rig_type_add_interface (&rig_particle_engine_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_particle_engine_introspectable_vtable);
  rig_type_add_interface (&rig_particle_engine_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigParticleEngine, introspectable),
                          NULL); /* no implied vtable */
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

  rig_paintable_init (RIG_OBJECT (engine));
  rig_graphable_init (RIG_OBJECT (engine));

  rig_simple_introspectable_init (engine,
                                  _rig_particle_engine_prop_specs,
                                  engine->properties);

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


void
rig_particle_engine_set_max_particles (RigParticleEngine *engine,
                                       int max_particles)
{
  const int prop = RIG_PARTICLE_ENGINE_PROP_MAX_PARTICLES;

  engine->max_particles = max_particles;
  _rig_particle_engine_clear_resources (engine);
  rig_property_dirty (&engine->context->property_ctx,
                      &engine->properties[prop]);
}

void
rig_particle_engine_set_min_initial_velocity (RigParticleEngine *engine,
                                              const float *value)
{
  const int prop = RIG_PARTICLE_ENGINE_PROP_MIN_INITIAL_VELOCITY;

  memcpy (engine->min_initial_velocity, value, sizeof (float) * 3);

  rig_property_dirty (&engine->context->property_ctx,
                      &engine->properties[prop]);

}

void
rig_particle_engine_set_max_initial_velocity (RigParticleEngine *engine,
                                              const float *value)
{
  const int prop = RIG_PARTICLE_ENGINE_PROP_MAX_INITIAL_VELOCITY;

  memcpy (engine->max_initial_velocity, value, sizeof (float) * 3);

  rig_property_dirty (&engine->context->property_ctx,
                      &engine->properties[prop]);

}

void
rig_particle_engine_set_min_initial_position (RigParticleEngine *engine,
                                              const float *value)
{
  const int prop = RIG_PARTICLE_ENGINE_PROP_MIN_INITIAL_POSITION;

  memcpy (engine->min_initial_position, value, sizeof (float) * 3);

  rig_property_dirty (&engine->context->property_ctx,
                      &engine->properties[prop]);

}

void
rig_particle_engine_set_max_initial_position (RigParticleEngine *engine,
                                              const float *value)
{
  const int prop = RIG_PARTICLE_ENGINE_PROP_MAX_INITIAL_POSITION;

  memcpy (engine->max_initial_position, value, sizeof (float) * 3);

  rig_property_dirty (&engine->context->property_ctx,
                      &engine->properties[prop]);

}
