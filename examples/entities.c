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

#include <stdbool.h>

#include <rig/rig.h>


typedef struct
{
  RigShell *shell;
  RigContext *ctx;

  CoglFramebuffer *fb;
  GTimer *timer;

  uint32_t next_entity_id;

  RigEntity *main_camera;
  RigCamera *main_camera_component;
  RigEntity *light;
  RigEntity *plane;
  RigEntity *cube;
  GList *entities;

  /* shadow mapping */
  CoglOffscreen *shadow_fb;
  CoglTexture2D *shadow_color;
  CoglTexture   *shadow_map;
  RigCamera     *shadow_map_camera;

  CoglPipeline *shadow_color_tex;
  CoglPipeline *shadow_map_tex;

  /* root materials */
  CoglPipeline *diffuse_specular;

} Data;

/* in micro seconds  */
static int64_t
get_current_time (Data *data)
{
  return (int64_t) (g_timer_elapsed (data->timer, NULL) * 1e6);
}

static CoglPipeline *
create_texture_pipeline (CoglTexture *texture)
{
  static CoglPipeline *template = NULL, *new_pipeline;

  if (G_UNLIKELY (template == NULL))
    {
      template = cogl_pipeline_new (rig_cogl_context);
      cogl_pipeline_set_layer_null_texture (template, 0, COGL_TEXTURE_TYPE_2D);
    }

  new_pipeline = cogl_pipeline_copy (template);
  cogl_pipeline_set_layer_texture (new_pipeline, 0, texture);

  return new_pipeline;
}


static void
compute_light_shadow_matrix (CoglMatrix *light_matrix,
                             CoglMatrix *light_projection,
                             CoglMatrix *light_view,
                             CoglMatrix *main_camera)
{
  /* Move the unit data from [-1,1] to [0,1], column major order */
  float bias[16] = {
    .5f, .0f, .0f, .0f,
    .0f, .5f, .0f, .0f,
    .0f, .0f, .5f, .0f,
    .5f, .5f, .5f, 1.f
  };

  cogl_matrix_init_from_array (light_matrix, bias);
  cogl_matrix_multiply (light_matrix, light_matrix, light_projection);
  cogl_matrix_multiply (light_matrix, light_matrix, light_view);
  cogl_matrix_multiply (light_matrix, light_matrix, main_camera);
}

CoglPipeline *
create_diffuse_specular_material (void)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  CoglDepthState depth_state;

  pipeline = cogl_pipeline_new (rig_cogl_context);
  cogl_pipeline_set_color4f (pipeline, 0.8f, 0.8f, 0.8f, 1.f);

  /* enable depth testing */
  cogl_depth_state_init (&depth_state);
  cogl_depth_state_set_test_enabled (&depth_state, TRUE);
  cogl_pipeline_set_depth_state (pipeline, &depth_state, NULL);

  /* set up our vertex shader */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

      /* definitions */
      "uniform mat4 light_shadow_matrix;\n"
      "uniform mat3 normal_matrix;\n"
      "varying vec3 normal_direction, eye_direction;\n"
      "varying vec4 shadow_coords;\n",

      "normal_direction = normalize(normal_matrix * cogl_normal_in);\n"
      "eye_direction    = -vec3(cogl_modelview_matrix * cogl_position_in);\n"

      "shadow_coords = light_shadow_matrix * cogl_modelview_matrix *\n"
      "                cogl_position_in;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  /* and fragment shader */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
      /* definitions */
      "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
      "uniform vec3 light0_direction_norm;\n"
      "varying vec3 normal_direction, eye_direction;\n",

      /* post */
      NULL);

  cogl_snippet_set_replace (snippet,
      "vec4 final_color = light0_ambient * cogl_color_in;\n"

      " vec3 L = light0_direction_norm;\n"
      " vec3 N = normalize(normal_direction);\n"

      "float lambert = dot(N, L);\n"

      "if (lambert > 0.0)\n"
      "{\n"
      "  final_color += cogl_color_in * light0_diffuse * lambert;\n"

      "  vec3 E = normalize(eye_direction);\n"
      "  vec3 R = reflect (-L, N);\n"
      "  float specular = pow (max(dot(R, E), 0.0),\n"
      "                        2.);\n"
      "  final_color += light0_specular * vec4(.6, .6, .6, 1.0) * specular;\n"
      "}\n"

      "shadow_coords_d = shadow_coords / shadow_coords.w;\n"
      "cogl_texel7 =  cogl_texture_lookup7 (cogl_sampler7, cogl_tex_coord_in[0]);\n"
      "float distance_from_light = cogl_texel7.z + 0.0005;\n"
      "float shadow = 1.0;\n"
      "if (shadow_coords.w > 0.0 && distance_from_light < shadow_coords_d.z)\n"
      "    shadow = 0.5;\n"

      "cogl_color_out = shadow * final_color;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

  return pipeline;
}

static void
draw_entities (Data            *data,
               CoglFramebuffer *fb,
               RigEntity       *camera,
               gboolean         shadow_pass)
{
  CoglMatrix *transform, inverse;
  GList *l;

  transform = rig_entity_get_transform (camera);
  cogl_matrix_get_inverse (transform, &inverse);

  rig_entity_draw (camera, fb);

  cogl_framebuffer_push_matrix (fb);

  if (shadow_pass)
    {
      cogl_framebuffer_identity_matrix (fb);
      cogl_framebuffer_scale (fb, 1, -1, 1);
      cogl_framebuffer_transform (fb, &inverse);
    }
  else
    {
      cogl_framebuffer_set_modelview_matrix (fb, &inverse);
    }

  for (l = data->entities; l; l = l->next)
    {
      RigEntity *entity = l->data;

      if (shadow_pass && !rig_entity_get_cast_shadow (entity))
        continue;

      cogl_framebuffer_push_matrix (fb);

      transform = rig_entity_get_transform (entity);
      cogl_framebuffer_transform (fb, transform);

      rig_entity_draw (entity, fb);

      cogl_framebuffer_pop_matrix (fb);
    }

  /* XXX: Note we don't pop the matrix stack here since we want to
   * allow additional things to be drawn using the same transform.
   * This means it's the callers responsibility for now to pop the
   * modelview matrix after using this function.
   *
   * At some point we should probably factor out the camera flushing
   * from this function so we can avoid this asymmetry.
   */
}

static void
test_init (RigShell *shell, void *user_data)
{
  Data *data = user_data;
  CoglOnscreen *onscreen;
  CoglTexture2D *color_buffer;
  GError *error = NULL;
  RigObject *component;
  CoglPipeline *root_pipeline, *pipeline;
  CoglSnippet *snippet;
  RigColor color;
  float vector3[3];

  onscreen = cogl_onscreen_new (data->ctx->cogl_context, 800, 600);
  data->fb = COGL_FRAMEBUFFER (onscreen);

  cogl_onscreen_show (onscreen);

  /*
   * Shadow mapping
   */

  /* Setup the shadow map */
  color_buffer = cogl_texture_2d_new_with_size (rig_cogl_context,
                                                512, 512,
                                                COGL_PIXEL_FORMAT_ANY,
                                                &error);
  if (error)
    g_critical ("could not create texture: %s", error->message);

  data->shadow_color = color_buffer;

  /* XXX: Right now there's no way to disable rendering to the the color
   * buffer. */
  data->shadow_fb =
    cogl_offscreen_new_to_texture (COGL_TEXTURE (color_buffer));

  /* retrieve the depth texture */
  cogl_framebuffer_enable_depth_texture (COGL_FRAMEBUFFER (data->shadow_fb),
                                         TRUE);
  data->shadow_map =
    cogl_framebuffer_get_depth_texture (COGL_FRAMEBUFFER (data->shadow_fb));

  if (data->shadow_fb == NULL)
    g_critical ("could not create offscreen buffer");

  /* Hook the shadow sampling */
  root_pipeline = create_diffuse_specular_material ();
  cogl_pipeline_set_layer_texture (root_pipeline, 7, data->shadow_map);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              /* declarations */
                              "varying vec4 shadow_coords;\n"
                              "vec4 shadow_coords_d;\n",
                              /* post */
                              "");

  cogl_snippet_set_replace (snippet,
                            "cogl_texel = texture2D(cogl_sampler7, shadow_coords_d.st);\n");

  cogl_pipeline_add_layer_snippet (root_pipeline, 7, snippet);
  cogl_object_unref (snippet);

  /*
   * Setup CoglObjects to render our plane and cube
   */

  /* camera */
  data->main_camera = rig_entity_new (data->ctx, data->next_entity_id++);
  data->entities = g_list_prepend (data->entities, data->main_camera);

  vector3[0] = 0.f;
  vector3[1] = 2.f;
  vector3[2] = 10.f;
  rig_entity_set_position (data->main_camera, vector3);

  component = rig_camera_new (data->ctx, data->fb);
  data->main_camera_component = component;

  rig_camera_set_projection_mode (RIG_CAMERA (component),
                                  RIG_PROJECTION_PERSPECTIVE);
  rig_camera_set_field_of_view (RIG_CAMERA (component), 60.f);
  rig_camera_set_near_plane (RIG_CAMERA (component), 1.1f);
  rig_camera_set_far_plane (RIG_CAMERA (component), 100.f);

  rig_entity_add_component (data->main_camera, component);

  /* light */
  data->light = rig_entity_new (data->ctx, data->next_entity_id++);
  data->entities = g_list_prepend (data->entities, data->light);

  vector3[0] = 1.0f;
  vector3[1] = 8.0f;
  vector3[2] = -2.0f;
  rig_entity_set_position (data->light, vector3);

  rig_entity_rotate_x_axis (data->light, -120);
  rig_entity_rotate_y_axis (data->light, 10);

  component = rig_light_new ();
  rig_color_init_from_4f (&color, .2f, .2f, .2f, 1.f);
  rig_light_set_ambient (RIG_LIGHT (component), &color);
  rig_color_init_from_4f (&color, .6f, .6f, .6f, 1.f);
  rig_light_set_diffuse (RIG_LIGHT (component), &color);
  rig_color_init_from_4f (&color, .4f, .4f, .4f, 1.f);
  rig_light_set_specular (RIG_LIGHT (component), &color);
  rig_light_add_pipeline (RIG_LIGHT (component), root_pipeline);


  rig_entity_add_component (data->light, component);

  component = rig_camera_new (data->ctx, COGL_FRAMEBUFFER (data->shadow_fb));
  data->shadow_map_camera = component;

  rig_color_init_from_4f (&color, 0.f, .3f, 0.f, 1.f);
  rig_camera_set_background_color4f (RIG_CAMERA (component), 0.f, .3f, 0.f, 1.f);
  rig_camera_set_projection_mode (RIG_CAMERA (component),
                                  RIG_PROJECTION_ORTHOGRAPHIC);
  rig_camera_set_orthographic_coordinates (RIG_CAMERA (component), -6, 6, 6, -6);
  rig_camera_set_near_plane (RIG_CAMERA (component), 1.1f);
  rig_camera_set_far_plane (RIG_CAMERA (component), 20.f);

  rig_entity_add_component (data->light, component);


  /* plane */
  data->plane = rig_entity_new (data->ctx, data->next_entity_id++);
  data->entities = g_list_prepend (data->entities, data->plane);
  rig_entity_set_cast_shadow (data->plane, FALSE);
  rig_entity_set_y (data->plane, -1.5f);

  component = rig_mesh_renderer_new_from_template (data->ctx, "plane");
  rig_entity_add_component (data->plane, component);
  component = rig_material_new_with_pipeline (data->ctx, root_pipeline);
  rig_entity_add_component (data->plane, component);

  /* a second, more interesting, entity */
  data->cube = rig_entity_new (data->ctx, data->next_entity_id++);
  data->entities = g_list_prepend (data->entities, data->cube);
  rig_entity_set_cast_shadow (data->cube, TRUE);
  rig_entity_set_y (data->cube, .5);
  rig_entity_set_z (data->cube, 1);
  rig_entity_rotate_y_axis (data->cube, 10);

  component = rig_mesh_renderer_new_from_template (data->ctx, "cube");
  rig_entity_add_component (data->cube, component);

  pipeline = cogl_pipeline_copy (root_pipeline);
  cogl_pipeline_set_color4f (pipeline, 0.6f, 0.6f, 0.6f, 1.0f);
  component = rig_material_new_with_pipeline (data->ctx, pipeline);
  cogl_object_unref (pipeline);
  rig_entity_add_component (data->cube, component);

  /* We draw the entities in the order they are listed and so that
   * matches the order we created the entities we now reverse the
   * list... */
  data->entities = g_list_reverse (data->entities);

  /* create the pipelines to display the shadow color and depth textures */
  data->shadow_color_tex =
      create_texture_pipeline (COGL_TEXTURE (data->shadow_color));
  data->shadow_map_tex =
      create_texture_pipeline (COGL_TEXTURE (data->shadow_map));

  cogl_object_unref (root_pipeline);
  /* timer for the world time */
  data->timer = g_timer_new ();
  g_timer_start (data->timer);
}

static CoglBool
test_paint (RigShell *shell, void *user_data)
{
  Data *data = user_data;
  int64_t time; /* micro seconds */
  CoglFramebuffer *shadow_fb;
  GList *l;

  /*
   * update entities
   */

  time = get_current_time (data);

  for (l = data->entities; l; l = l->next)
    {
      RigEntity *entity = l->data;

      rig_entity_update (entity, time);
    }

  /*
   * render the shadow map
   */

  shadow_fb = COGL_FRAMEBUFFER (data->shadow_fb);

  /* update the light matrix uniform */
  {
    CoglMatrix light_shadow_matrix, light_projection, *light_transform,
               light_view;
    CoglPipeline *pipeline;
    RigMaterial *material;

    int location;

    cogl_framebuffer_get_projection_matrix (shadow_fb, &light_projection);
    light_transform = rig_entity_get_transform (data->light);
    cogl_matrix_get_inverse (light_transform, &light_view);
    compute_light_shadow_matrix (&light_shadow_matrix,
                                 &light_projection,
                                 &light_view,
                                 rig_entity_get_transform (data->main_camera));

    material = rig_entity_get_component (data->plane, RIG_COMPONENT_TYPE_MATERIAL);
    pipeline = rig_material_get_pipeline (material);
    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      cogl_matrix_get_array (&light_shadow_matrix));

    material = rig_entity_get_component (data->cube, RIG_COMPONENT_TYPE_MATERIAL);
    pipeline = rig_material_get_pipeline (material);
    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      cogl_matrix_get_array (&light_shadow_matrix));
  }

  draw_entities (data, shadow_fb, data->light, TRUE /* shadow pass */);

  /* XXX: Big hack!
   *
   * draw_entities() leaves it's view transform pushed so we can draw
   * more things using the same transform before popping it. (See
   * below where we render the shadow maps for debugging after drawing
   * the entities)...
   */
  cogl_framebuffer_pop_matrix (shadow_fb);

  /* XXX: Somewhat asymmetric and hacky for now... */
  rig_camera_end_frame (data->shadow_map_camera);

  /*
   * render the scene
   */

  /* draw entities */
  draw_entities (data, data->fb, data->main_camera, FALSE /* shadow pass */);

  /* draw the color and depth buffers of the shadow FBO to debug them */
  cogl_framebuffer_draw_rectangle (data->fb, data->shadow_color_tex,
                                   -2, 1, -4, 3);
  cogl_framebuffer_draw_rectangle (data->fb, data->shadow_map_tex,
                                   -2, -1, -4, 1);

  /* XXX: Big hack!
   *
   * draw_entities() leaves it's view transform pushed so we can draw
   * the shadow map textures using the same transform, but we need
   * to make sure we pop the transform when we are done...
   */
  cogl_framebuffer_pop_matrix (data->fb);

  /* XXX: Somewhat asymmetric and hacky for now... */
  rig_camera_end_frame (data->main_camera_component);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));

  return TRUE;
}

static void
test_fini (RigShell *shell, void *user_data)
{
  Data *data = user_data;
  GList *l;

  for (l = data->entities; l; l = l->next)
    rig_ref_countable_simple_unref (l->data);
  g_list_free (data->entities);

  cogl_object_unref (data->shadow_color);
  cogl_object_unref (data->shadow_map);
  cogl_object_unref (data->shadow_fb);

  cogl_object_unref (data->shadow_map_tex);
  cogl_object_unref (data->shadow_color_tex);
  cogl_object_unref (data->diffuse_specular);

  cogl_object_unref (data->fb);

  g_timer_destroy (data->timer);
}

int
main (int argc, char **argv)
{
  Data data;

  memset (&data, 0, sizeof (Data));

  data.shell = rig_shell_new (test_init, test_fini, test_paint, &data);

  data.ctx = rig_context_new (data.shell);

  rig_context_init (data.ctx);

  rig_shell_main (data.shell);

  return 0;
}
