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
  float position[3];
  float velocity[3];
  RigParticleEngineColor initial_color;

  /* Time of creation in milliseconds relative to the start of the
   * engine when the particle was created */
  int32_t creation_time;
  /* The maximum age of this particle in msecs. The particle will
   * linearly fade out until this age */
  int32_t max_age;
  /* Index into engine->textures for the texture of this particle */
  uint8_t texture_number;
} RigParticleEngineParticle;

typedef struct
{
  float position[3];
  RigParticleEngineColor color;
  /* This part is optional and will only be set if there are more than
   * one texture */
  uint8_t texture_number;
} RigParticleEngineVertex;

struct _RigParticleEngine
{
  RigObjectProps _parent;

  RigContext *context;

  /* Configurable options that affect the resources */
  GPtrArray *textures;
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

  int vertex_size;

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
  int i;

  /* Add all of the textures to a separate layer */
  for (i = 0; i < engine->textures->len; i++)
    {
      cogl_pipeline_set_layer_texture (pipeline,
                                       i,
                                       g_ptr_array_index (engine->textures, i));
      cogl_pipeline_set_layer_wrap_mode (pipeline,
                                         i,
                                         COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    }

  /* If there are any textures then we'll enable point sprites for the
   * first layer. We're only going to use the texture coordinates from
   * the first layer so the other layers don't matter */
  if (engine->textures->len > 0)
    cogl_pipeline_set_layer_point_sprite_coords_enabled (pipeline,
                                                         0, /* layer */
                                                         TRUE, /* enable */
                                                         NULL /* error */);

  /* If there is more than one texture then we'll add a shader snippet
   * to pick a texture based on an attribute */
  if (engine->textures->len > 1)
    {
      CoglSnippet *snippet =
        cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
                          "attribute int rig_texture_number;\n"
                          "varying float rig_texture_number_varying;\n",
                          "rig_texture_number_varying = "
                          "float (rig_texture_number);\n");
      char *snippet_source;
      int *sampler_values;
      int uniform_location;

      cogl_pipeline_add_snippet (pipeline, snippet);

      cogl_object_unref (snippet);

      snippet_source =
        g_strdup_printf ("varying float rig_texture_number_varying;\n"
                         "uniform sampler2D rig_textures[%i];\n",
                         engine->textures->len);


      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  snippet_source,
                                  NULL);

      g_free (snippet_source);

      cogl_snippet_set_replace (snippet,
                                "int tex_num = "
                                "int (rig_texture_number_varying);\n"
                                "sampler2D tex = "
                                "rig_textures[tex_num];\n"
                                "cogl_color_out = "
                                "texture2D (tex, cogl_tex_coord_in[0].st);\n"
                                "cogl_color_out *= cogl_color_in;\n");

      cogl_pipeline_add_snippet (pipeline, snippet);

      cogl_object_unref (snippet);

      /* We need an array of sampler uniforms. XXX: this is probably a
       * bit dodgy because we're assuming the layer numbers will match
       * the unit numbers. Cogl could maybe do with some builtin
       * uniform or function to get a sampler number from a layer
       * number in the shader */
      sampler_values = g_alloca (engine->textures->len * sizeof (int));
      for (i = 0; i < engine->textures->len; i++)
        sampler_values[i] = i;

      uniform_location =
        cogl_pipeline_get_uniform_location (pipeline, "rig_textures");
      cogl_pipeline_set_uniform_int (pipeline,
                                     uniform_location,
                                     1, /* n_components */
                                     engine->textures->len,
                                     sampler_values);
    }

  cogl_pipeline_set_point_size (pipeline, engine->point_size);

  return pipeline;
}

static int
_rig_particle_engine_get_vertex_size (RigParticleEngine *engine)
{
  int size = 0;

  /* The size of the vertex that we'll use depends on the properties
   * enabled for the engine. */

  /* If there is more than one texture then we'll add an extra byte
   * for the texture number */
  if (engine->textures->len > 1)
    size = sizeof (RigParticleEngineVertex);
  else
    size = G_STRUCT_OFFSET (RigParticleEngineVertex, texture_number);

  /* Align the size to 4 bytes */
  size = (size + 3) & ~3;

  return size;
}

static void
_rig_particle_engine_create_resources (RigParticleEngine *engine)
{
  if (engine->pipeline == NULL)
    {
      CoglAttribute *attributes[3];
      int n_attributes = 0;
      int offset = 0;
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

      engine->vertex_size = _rig_particle_engine_get_vertex_size (engine);

      engine->attribute_buffer =
        cogl_attribute_buffer_new (engine->context->cogl_context,
                                   engine->vertex_size *
                                   engine->max_particles,
                                   NULL /* data */);

      attributes[n_attributes++] =
        cogl_attribute_new (engine->attribute_buffer,
                            "cogl_position_in",
                            engine->vertex_size,
                            offset,
                            3, /* n_components */
                            COGL_ATTRIBUTE_TYPE_FLOAT);
      offset += sizeof (float) * 3;

      attributes[n_attributes++] =
        cogl_attribute_new (engine->attribute_buffer,
                            "cogl_color_in",
                            engine->vertex_size,
                            offset,
                            4, /* n_components */
                            COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
      offset += sizeof (RigParticleEngineColor);

      if (engine->textures->len > 1)
        {
          attributes[n_attributes++] =
            cogl_attribute_new (engine->attribute_buffer,
                                "rig_texture_number",
                                engine->vertex_size,
                                offset,
                                1, /* n_components */
                                COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
          offset += 1;
        }

      engine->primitive =
        cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_POINTS,
                                            engine->max_particles,
                                            attributes,
                                            n_attributes);

      for (i = 0; i < n_attributes; i++)
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
_rig_particle_engine_get_initial_velocity (RigParticleEngine *engine,
                                           float velocity[3])
{
  /* TODO: make the initial velocity configurable with some randomness */
  velocity[0] = g_rand_double_range (engine->rand, -20.0f, 20.0f);
  velocity[1] = g_rand_double_range (engine->rand, -10.0f, 30.0f);
  velocity[2] = g_rand_double_range (engine->rand, -10.0f, 10.0f);
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

static int
_rig_particle_engine_get_texture_number (RigParticleEngine *engine)
{
  if (engine->textures->len > 0)
    return g_rand_int_range (engine->rand, 0, engine->textures->len);
  else
    return 0;
}

static void
_rig_particle_engine_initialise_particle (RigParticleEngine *engine,
                                          RigParticleEngineParticle *particle)
{
  memset (particle->position, 0, sizeof (float) * 3);

  _rig_particle_engine_get_initial_velocity (engine, particle->velocity);
  _rig_particle_engine_get_initial_color (engine, &particle->initial_color);
  particle->max_age =
    _rig_particle_engine_get_max_age (engine);
  particle->texture_number =
    _rig_particle_engine_get_texture_number (engine);

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
_rig_particle_engine_get_force (RigParticleEngine *engine,
                                float force[3])
{
  /* TODO: add some customization. This could maybe have an array of
   * forces. The forces could be virtual objects with an interface to
   * get the current force based on the time. This function could then
   * accumulate all of these into a single force to apply to particles
   * for this time slice. That would allow for gravity and things like
   * a wind force that would vary direction over time */

  /* The force is a misnomer because the unit is actually just treated
   * as the acceltation to apply to the object. Maybe I can excuse
   * this if I pretend the particles all have a constant mass of 1. */

  /* The 'force' is measured in position units per second per second */

  force[0] = 0.0f;
  force[1] = -10.0f;
  force[2] = 0.0f;
}

static void
_rig_particle_engine_update_physics (float *position,
                                     float *velocity,
                                     float acceleration,
                                     int32_t elapsed_time)
{
  /* This updates the physics for one axis of the particle */

  float v = *velocity;
  float time_secs = elapsed_time / 1000.0f;
  float delta_v = acceleration * time_secs;
  float final_v = v + delta_v;

  *position += (v + final_v) / 2.0f * time_secs;
  *velocity = final_v;
}

static void
_rig_particle_engine_update (RigParticleEngine *engine)
{
  int32_t elapsed_time = engine->current_time - engine->last_update_time;
  int n_particles = 0;
  float force[3];
  int particle_num;
  uint8_t *data;

  _rig_particle_engine_create_resources (engine);
  _rig_particle_engine_create_new_particles (engine);
  _rig_particle_engine_get_force (engine, force);

  data = cogl_buffer_map (COGL_BUFFER (engine->attribute_buffer),
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
          RigParticleEngineVertex *vertex =
            (RigParticleEngineVertex *) (data +
                                         engine->vertex_size * n_particles);
          /* The opacity fades linearly over the lifetime of the particle */
          float opacity = 1.0f - particle_age / (float) particle->max_age;
          int i;

          vertex->color.r = particle->initial_color.r * opacity;
          vertex->color.g = particle->initial_color.g * opacity;
          vertex->color.b = particle->initial_color.b * opacity;
          vertex->color.a = particle->initial_color.a * opacity;

          for (i = 0; i < 3; i++)
            _rig_particle_engine_update_physics (particle->position + i,
                                                 particle->velocity + i,
                                                 force[i],
                                                 elapsed_time);

          memcpy (vertex->position, particle->position, sizeof (float) * 3);

          if (engine->textures->len > 0)
            vertex->texture_number = particle->texture_number;

          n_particles++;
        }
    }
  RIG_FLAGS_FOREACH_END;

  engine->last_update_time = engine->current_time;

  cogl_buffer_unmap (COGL_BUFFER (engine->attribute_buffer));

  cogl_primitive_set_n_vertices (engine->primitive, n_particles);
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

  g_ptr_array_free (engine->textures, TRUE);
  g_array_free (engine->colors, TRUE);

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
  engine->textures = g_ptr_array_new ();
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
rig_particle_engine_add_texture (RigParticleEngine *engine,
                                 CoglTexture *texture)
{
  _rig_particle_engine_clear_resources (engine);
  g_ptr_array_add (engine->textures, cogl_object_ref (texture));
}

void
rig_particle_engine_remove_texture (RigParticleEngine *engine,
                                    CoglTexture *texture)
{
  int i;

  for (i = 0; i < engine->colors->len; i++)
    if (g_ptr_array_index (engine->textures, i) == texture)
      {
        cogl_object_unref (texture);
        g_ptr_array_remove_index (engine->textures, i);
        break;
      }
}
