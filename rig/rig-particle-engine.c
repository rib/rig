#include <cogl/cogl.h>

#include "rig.h"
#include "rig-particle-engine.h"

typedef struct
{
  float position[3];
  float velocity[3];
  uint8_t initial_color[4];

  /* Time of creation in milliseconds relative to the start of the
   * engine when the particle was created */
  int32_t creation_time;
  /* The maximum age of this particle in msecs. The particle will
   * linearly fade out until this age */
  int32_t max_age;
  /* Index into engine->textures for the texture of this particle */
  uint8_t texture_number;
} RigParticleEngineParticle;

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
  /* Bit array of unused particles */
  int unused_particles_n_longs;
  unsigned long *unused_particles;
  CoglAttributeBuffer *attribute_buffer;
  CoglPrimitive *primitive;

  GRand *rand;

  int vertex_size;

  /* The time (as set by rig_particle_engine_set_time) that was
   * current when the particles were last updated */
  int32_t last_update_time;
  /* The next time that we should generate a new particle */
  int32_t next_particle_time;
  /* The 'current' time within the animation */
  int32_t current_time;

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
    cogl_pipeline_set_layer_texture (pipeline,
                                     i,
                                     g_ptr_array_index (engine->textures, i));

  /* If there are any textures then we'll enable point sprites for the
   * first layer. We're only going to use the texture coordinates from
   * the first layer so the other layers don't matter */
  if (engine->textures->len > 0)
    cogl_pipeline_set_layer_point_sprite_coords_enabled (pipeline, 0, TRUE);

  /* If there is more than one texture then we'll add a shader snippet
   * to pick a texture based on an attribute */
  if (engine->textures->len > 1)
    {
      CoglSnippet *snippet =
        cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,
                          "attribute int rig_texture_number;\n"
                          "varying int rig_texture_number_varying;\n",
                          "rig_texture_number_varying = rig_texture_number;\n");
      char *snippet_source;
      int *sampler_values;
      int uniform_location;

      cogl_pipeline_add_snippet (pipeline, snippet);

      cogl_object_unref (snippet);

      snippet_source =
        g_strdup_printf ("varying int rig_texture_number_varying;\n"
                         "uniform rig_textures[%i];\n",
                         engine->textures->len);


      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  snippet_sources,
                                  NULL);

      g_free (snippet_source);

      cogl_snippet_set_replace (snippet,
                                "sampler2D tex = "
                                "rig_textures[rig_texture_number_varying];\n"
                                "cogl_color_out = "
                                "texture2D (tex, cogl_tex_coord_in[0]);\n"
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

      uniform_location = cogl_pipeline_get_uniform_location ("rig_textures");
      cogl_pipeline_set_uniform_int (pipeline,
                                     uniform_location,
                                     1, /* n_components */
                                     engine->textures->len,
                                     sampler_values);
    }

  cogl_pipeline_set_point_size (pipeline, engine->point_size);

  engine->last_update_time = engine->current_time;
  engine->next_particle_time = engine->current_time;
}

static int
_rig_particle_engine_get_vertex_size (RigParticleEngine *engine)
{
  int size = 0;

  /* The size of the vertex that we'll use depends on the properties
   * enabled for the engine. */

  /* There are always 3 floats for the position */
  size += sizeof (float) * 3;

  /* and 4 bytes for the color */
  size += 4;

  /* If there is more than one texture then we'll add an extra byte
   * for the texture number */
  if (engine->textures->len > 1)
    size++;

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

      particles = g_new (RigParticleEngineParticle, engine->max_particles);
      engine->unused_particles_n_longs =
        RIG_FLAGS_N_LONGS_FOR_SIZE (engine->max_particles);
      engine->unused_particles = g_new0 (unsigned long,
                                         engine->unused_particles_n_longs);
      /* To begin with all of the particles are unused */
      RIG_FLAGS_SET_ALL (engine->unused_particles,
                         engine->max_particles,
                         TRUE);

      engine->vertex_size = _rig_particle_engine_get_vertex_size (engine);

      engine->attribute_buffer =
        cogl_attribute_buffer_new (engine->context->cogl_context,
                                   engine->vertex_size *
                                   engine->max_particles);

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
      offset += 4;

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
    }
}

static int32_t
_rig_particle_engine_get_next_particle_time (RigParticleEngine *engine,
                                             int32_t last_particle_time)
{
  /* TODO: this should have some parameters with some configurable
   * randomness */
  return last_particle_time + 16;
}

static void
_rig_particle_engine_get_initial_velocity (RigParticleEngine *engine,
                                           float velocity[3])
{
  /* TODO: make the initial velocity configurable with some randomness */
  velocity[0] = g_rand_double_range (engine->rand, 0.0f, 10.0f);
  velocity[1] = g_rand_double_range (engine->rand, 0.0f, 10.0f);
  velocity[2] = g_rand_double_range (engine->rand, 0.0f, 10.0f);
}

static void
_rig_particle_get_initial_color (RigParticleEngine *engine,
                                 uint8_t color[4])
{
  if (engine->colors->len > 0)
    {
      int color_num = g_rand_int_range (0, engine->colors->len);
      const CoglColor *color = &g_array_index (engine->colors,
                                               CoglColor,
                                               color_num);
      color[0] = cogl_color_get_red_byte (color);
      color[1] = cogl_color_get_green_byte (color);
      color[2] = cogl_color_get_blue_byte (color);
      color[3] = cogl_color_get_alpha_byte (color);
    }
  else
    memset (color, 0xff, sizeof (uint8_t) * 4);
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
    return g_rand_int_range (0, engine->textures->len);
  else
    return 0;
}

static void
_rig_particle_engine_initialise_particle (RigParticleEngine *engine,
                                          RigParticleEngineParticle *particle)
{
  memset (particle->position, 0, sizeof (float) * 3);

  _rig_particle_engine_get_initial_velocity (engine, particle->velocity);
  _rig_particle_engine_get_initial_color (engine, particle->initial_color);
  particle->max_age =
    _rig_particle_engine_get_max_age (engine);
  particle->texture_number =
    _rig_particle_engine_get_texture_number (engine);

  particle->creation_time = engine->current_time;
}

static void
_rig_particle_create_new_particles (RigParticleEngine *engine)
{
  int particle_num;

  /* Fill in any unused particles until we either run out of unused
   * particles or we've used up enough time */
  RIG_FLAGS_FOREACH_START (engine->unused_particles,
                           engine->unused_particles_n_longs,
                           particle_num)
    {
      int last_particle_time;

      /* If we've used too much time then stop creating particles */
      if (engine->current_time < engine->next_particle_time)
        return;

      _rig_particle_engine_initialise_particle (engine,
                                                engine->particles +
                                                particle_num);

      RIG_FLAGS_SET (engine->unused_particles, particle_num, TRUE);

      last_particle_time = engine->next_particle_time;
      engine->next_particle_time =
        _rig_particle_engine_get_next_particle_time (engine,
                                                     last_particle_time);

    }
  RIG_FLAGS_FOREACH_END;
}

static void
_rig_particle_engine_update (RigParticleEngine *engine)
{
  int n_particles = 0;

  _rig_particle_engine_create_resources (engine);
  _rig_particle_engine_create_new_particles (engine);
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
      cogl_object_unref (engine->primitive_buffer);
    }
}

static void
_rig_particle_engine_free (void *object)
{
  RigParticleEngine *engine = object;

  _rig_particle_engine_clear_resources (engine);

  rig_ref_countable_unref (engine->context);

  g_ptr_array_free (engine->textures, TRUE);

  g_rand_free (engine->rand);

  g_slice_free (RigParticleEngine, engine);
}

RigRefCountableVTable _rig_particle_engine_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_particle_engine_free
};

static void
_rig_particle_engine_init_type (void)
{
  rig_type_init (&rig_particle_engine_type);
  rig_type_add_interface (&rig_input_region_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigParticleEngine, ref_count),
                          &_rig_particle_engine_ref_countable_vtable);
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

  engine->max_particles = 128;
  engine->point_size = 16.0f;

  engine->rand = g_rand_new ();

  rig_object_init (&engine->_parent, &rig_particle_engine_type);

  return engine;
}
