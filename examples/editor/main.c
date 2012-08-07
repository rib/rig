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
#include <math.h>

#include <rig.h>

#define N_CUBES 10

typedef struct
{
  CoglTexture *source;
  unsigned int scale_factor_x, scale_factor_y;
  CoglTexture *destination;
  unsigned int destination_width, destination_height;
  CoglFramebuffer *fb;
  RigCamera *camera;
  CoglPipeline *pipeline;
} RigDownsample;

typedef struct
{
  CoglTexture *source;
  unsigned int width, height;

  RigCamera *x_pass_camera;
  CoglFramebuffer *x_pass_fb;
  CoglTexture *x_pass;
  CoglPipeline *x_pass_pipeline;

  RigCamera *y_pass_camera;
  CoglFramebuffer *y_pass_fb;
  CoglTexture *y_pass;
  CoglTexture *destination;
  CoglPipeline *y_pass_pipeline;
} RigGaussianBlur;

typedef struct
{
  CoglTexture *source;
  CoglFramebuffer *source_fb;

  CoglTexture *blurred_texture;

  CoglPipeline *pipeline;
} RigDepthOfField;

typedef struct
{
  RigShell *shell;
  RigContext *ctx;

  CoglFramebuffer *fb;
  float fb_width, fb_height;
  GTimer *timer;

  uint32_t next_entity_id;

  /* postprocessing */
  CoglFramebuffer *postprocess;
  CoglTexture2D *postprocess_color;
  RigDownsample down;
  RigGaussianBlur blur;
  RigDepthOfField dof;

  /* scene */
  RigGraph *scene;
  RigEntity *main_camera;
  RigCamera *main_camera_component;
  float main_camera_z;
  RigEntity *light;
  RigEntity *ui_camera;
  RigCamera *ui_camera_component;
  RigEntity *plane;
  RigEntity *cubes[N_CUBES];
  GList *entities;
  GList *pickables;

  /* shadow mapping */
  CoglOffscreen *shadow_fb;
  CoglTexture2D *shadow_color;
  CoglTexture   *shadow_map;
  RigCamera     *shadow_map_camera;

  CoglPipeline *shadow_color_tex;
  CoglPipeline *shadow_map_tex;

  /* root materials */
  CoglPipeline *diffuse_specular;

  /* editor state */
  bool button_down;
  RigArcball arcball;
  CoglQuaternion saved_rotation;
  RigEntity *selected_entity;
  RigTool *tool;
  bool edit;      /* in edit mode, we can temper with the entities. When edit
                     is turned off, we'll do the full render (including post
                     processing) as post-processing does interact well with
                     drawing the tools */

  /* picking ray */
  CoglPipeline *picking_ray_color;
  CoglPrimitive *picking_ray;

  /* debug features */
  bool debug_pick_ray;
  bool debug_shadows;

} Data;

/*
 * Materials
 */

static CoglPipeline *
create_color_pipeline (float r, float g, float b)
{
  static CoglPipeline *template = NULL, *new_pipeline;

  if (G_UNLIKELY (template == NULL))
    template = cogl_pipeline_new (rig_cogl_context);

  new_pipeline = cogl_pipeline_copy (template);
  cogl_pipeline_set_color4f (new_pipeline, r, g, b, 1.0f);

  return new_pipeline;
}

/*
 * RigDownsample
 */

static void
rig_downsample_init (RigDownsample *down,
                     RigContext *ctx,
                     CoglTexture *source,
                     unsigned int scale_factor_x,
                     unsigned int scale_factor_y)
{
  CoglPixelFormat format;
  CoglTexture2D *texture_2d;
  CoglOffscreen *offscreen;
  unsigned int src_w, src_h;
  GError *error = NULL;

  /* validation */
  src_w = cogl_texture_get_width (source);
  src_h = cogl_texture_get_height (source);

  if (src_w % scale_factor_x != 0)
    {
      g_warning ("downsample: the width of the texture (%d) is not a "
                 "multiple of the scale factor (%d)", src_w, scale_factor_x);
    }
  if (src_h % scale_factor_y != 0)
    {
      g_warning ("downsample: the height of the texture (%d) is not a "
                 "multiple of the scale factor (%d)", src_h, scale_factor_y);
    }

  /* init */
  down->source = cogl_object_ref (source);
  down->scale_factor_x = scale_factor_x;
  down->scale_factor_y = scale_factor_y;

  /* create the destination texture up front */
  down->destination_width = src_w / scale_factor_x;
  down->destination_height = src_h / scale_factor_y;
  format = cogl_texture_get_format (source);

  texture_2d = cogl_texture_2d_new_with_size (rig_cogl_context,
                                              down->destination_width,
                                              down->destination_height,
                                              format,
                                              &error);
  if (error)
    {
      g_warning ("downsample: could not create destination texture: %s",
                 error->message);
    }
  down->destination = COGL_TEXTURE (texture_2d);

  /* create the FBO to render the downsampled texture */
  offscreen = cogl_offscreen_new_to_texture (down->destination);
  down->fb = COGL_FRAMEBUFFER (offscreen);

  /* create the camera that will setup the scene for the render */
  down->camera = rig_camera_new (ctx, down->fb);
  rig_camera_set_near_plane (down->camera, -1.f);
  rig_camera_set_far_plane (down->camera, 1.f);

  /* and finally the pipeline to draw the source into the destination
   * texture */
  down->pipeline = rig_util_create_texture_pipeline (source);
}

#if 0
static void
rig_downsample_fini (RigDownsample *down)
{
  cogl_object_unref (down->source);
  cogl_object_unref (down->destination);
  cogl_object_unref (down->fb);
  cogl_object_unref (down->pipeline);
  /* XXX: free the camera */
}
#endif

static void
rig_downsample_render (RigDownsample *down)
{
  rig_camera_draw (down->camera, down->fb);

  cogl_framebuffer_draw_rectangle (down->fb,
                                   down->pipeline,
                                   0,
                                   0,
                                   down->destination_width,
                                   down->destination_height);

  rig_camera_end_frame (down->camera);
}

/*
 * RigGaussianBlur
 *
 * What would make sense if you want to animate the bluriness is to give
 * sigma to _init() and compute the number of taps based on that (as
 * discussed with Neil)
 *
 * Being lazy and having already written the code with th number of taps
 * as input, I'll stick with the number of taps, let's say it's because you
 * get a good idea of the performance of the shader this way.
 */

static float
gaussian (float sigma, float x)
{
  return 1 / (sigma * sqrtf (2 * G_PI)) *
              powf (G_E, - (x * x)/(2 * sigma * sigma));
}

/* http://theinstructionlimit.com/gaussian-blur-revisited-part-two */
static float
n_taps_to_sigma (int n_taps)
{
  static const float sigma[7] = {1.35, 1.55, 1.8, 2.18, 2.49, 2.85, 3.66};

  return sigma[n_taps / 2 - 2];
}

static CoglPipeline *
create_1d_gaussian_blur_pipeline (RigGaussianBlur *blur,
                                  RigContext *ctx,
                                  int n_taps)
{
  static GHashTable *pipeline_cache = NULL;
  CoglPipeline *pipeline;
  CoglSnippet *snippet;
  GString *shader;
  int i;

  /* initialize the pipeline cache. The shaders are only dependent on the
   * number of taps, not the sigma, so we cache the corresponding pipelines
   * in a hash table 'n_taps' => 'pipeline' */
  if (G_UNLIKELY (pipeline_cache == NULL))
    {
      pipeline_cache =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL, /* key destroy notify */
                               (GDestroyNotify) cogl_object_unref);
    }

  pipeline = g_hash_table_lookup (pipeline_cache, GINT_TO_POINTER (n_taps));
  if (pipeline)
    return pipeline;

  shader = g_string_new (NULL);

  g_string_append_printf (shader,
                          "uniform vec2 pixel_step;\n"
                          "uniform float factors[%i];\n",
                          n_taps);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                              shader->str,
                              NULL /* post */);

  g_string_set_size (shader, 0);

  pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_layer_null_texture (pipeline,
                                        0, /* layer_num */
                                        COGL_TEXTURE_TYPE_2D);
  cogl_pipeline_set_layer_wrap_mode (pipeline,
                                     0, /* layer_num */
                                     COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
  cogl_pipeline_set_layer_filters (pipeline,
                                   0, /* layer_num */
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  for (i = 0; i < n_taps; i++)
    {
      g_string_append (shader, "cogl_texel ");

      if (i == 0)
        g_string_append (shader, "=");
      else
        g_string_append (shader, "+=");

      g_string_append_printf (shader,
                              " texture2D (cogl_sampler, "
                              "cogl_tex_coord.st");
      if (i != (n_taps - 1) / 2)
        g_string_append_printf (shader,
                                " + pixel_step * %f",
                                (float) (i - ((n_taps - 1) / 2)));
      g_string_append_printf (shader,
                              ") * factors[%i];\n",
                              i);
    }

  cogl_snippet_set_replace (snippet, shader->str);

  g_string_free (shader, TRUE);

  cogl_pipeline_add_layer_snippet (pipeline, 0, snippet);

  cogl_object_unref (snippet);

  g_hash_table_insert (pipeline_cache, GINT_TO_POINTER (n_taps), pipeline);

  return pipeline;
}

static void
rig_gaussian_blur_update_x_pass_pipeline_texture (RigGaussianBlur *blur)
{
  float pixel_step[2];
  int pixel_step_location;

  /* our input in the source texture */
  cogl_pipeline_set_layer_texture (blur->x_pass_pipeline,
                                   0, /* layer_num */
                                   blur->source);

  pixel_step[0] = 1.0f / blur->width;
  pixel_step[1] = 0.0f;
  pixel_step_location =
    cogl_pipeline_get_uniform_location (blur->x_pass_pipeline, "pixel_step");
  g_assert (pixel_step_location);
  cogl_pipeline_set_uniform_float (blur->x_pass_pipeline,
                                   pixel_step_location,
                                   2, /* n_components */
                                   1, /* count */
                                   pixel_step);
}

static void
rig_gaussian_blur_update_y_pass_pipeline_texture (RigGaussianBlur *blur)
{
  float pixel_step[2];
  int pixel_step_location;

  /* our input in the result of the x pass */
  cogl_pipeline_set_layer_texture (blur->y_pass_pipeline,
                                   0, /* layer_num */
                                   blur->x_pass);

  pixel_step[0] = 0.0f;
  pixel_step[1] = 1.0f / blur->height;
  pixel_step_location =
    cogl_pipeline_get_uniform_location (blur->y_pass_pipeline, "pixel_step");
  g_assert (pixel_step_location);
  cogl_pipeline_set_uniform_float (blur->y_pass_pipeline,
                                   pixel_step_location,
                                   2, /* n_components */
                                   1, /* count */
                                   pixel_step);
}

static void
rig_gaussian_blur_update_factors (RigGaussianBlur *blur,
                                  int n_taps)
{
  int i, radius;
  float *factors, sigma;
  int location;

  radius = n_taps / 2; /* which is (n_taps - 1) / 2 as well */

  factors = g_alloca (n_taps * sizeof (float));
  sigma = n_taps_to_sigma (n_taps);

  for (i = -radius; i <= radius; i++)
    {
      factors[i + radius] = gaussian (sigma, i);
    }

  /* FIXME: normalize */
  location = cogl_pipeline_get_uniform_location (blur->x_pass_pipeline,
                                                 "factors");
  cogl_pipeline_set_uniform_float (blur->x_pass_pipeline,
                                   location,
                                   1 /* n_components */,
                                   n_taps /* count */,
                                   factors);

  location = cogl_pipeline_get_uniform_location (blur->y_pass_pipeline,
                                                 "factors");
  cogl_pipeline_set_uniform_float (blur->y_pass_pipeline,
                                   location,
                                   1 /* n_components */,
                                   n_taps /* count */,
                                   factors);
}

static void
rig_gaussian_blur_init (RigGaussianBlur *blur,
                        RigContext *ctx,
                        CoglTexture *source,
                        int n_taps)
{
  unsigned int src_w, src_h;
  CoglTexture2D *texture_2d;
  CoglPixelFormat format;
  CoglOffscreen *offscreen;
  CoglPipeline *base_pipeline;
  GError *error = NULL;

  /* validation */
  if (n_taps < 5 || n_taps > 17 || n_taps % 2 == 0 )
    {
      g_critical ("blur: the numbers of taps must belong to the {5, 7, 9, "
                  "11, 13, 14, 17, 19} set");
      g_assert_not_reached ();
      return;
    }

  /* init */
  blur->source = cogl_object_ref (source);

  /* create the first FBO to render the x pass */
  src_w = cogl_texture_get_width (source);
  src_h = cogl_texture_get_height (source);
  format = cogl_texture_get_format (source);
  texture_2d = cogl_texture_2d_new_with_size (rig_cogl_context,
                                              src_w,
                                              src_h,
                                              format,
                                              &error);
  if (error)
    {
      g_warning ("blur: could not create x pass texture: %s",
                 error->message);
    }
  blur->x_pass = COGL_TEXTURE (texture_2d);
  blur->width = src_w;
  blur->height = src_h;

  offscreen = cogl_offscreen_new_to_texture (blur->x_pass);
  blur->x_pass_fb = COGL_FRAMEBUFFER (offscreen);

  /* create the second FBO (final destination) to render the y pass */
  texture_2d = cogl_texture_2d_new_with_size (rig_cogl_context,
                                              src_w,
                                              src_h,
                                              format,
                                              &error);
  if (error)
    {
      g_warning ("blur: could not create destination texture: %s",
                 error->message);
    }
  blur->destination = COGL_TEXTURE (texture_2d);
  blur->y_pass = blur->destination;

  offscreen = cogl_offscreen_new_to_texture (blur->destination);
  blur->y_pass_fb = COGL_FRAMEBUFFER (offscreen);

  /* create the camera that will setup the scene for the render */
  blur->x_pass_camera = rig_camera_new (ctx, blur->x_pass_fb);
  blur->y_pass_camera = rig_camera_new (ctx, blur->y_pass_fb);

  base_pipeline = create_1d_gaussian_blur_pipeline (blur, ctx, n_taps);

  blur->x_pass_pipeline = cogl_pipeline_copy (base_pipeline);
  rig_gaussian_blur_update_x_pass_pipeline_texture (blur);
  blur->y_pass_pipeline = cogl_pipeline_copy (base_pipeline);
  rig_gaussian_blur_update_y_pass_pipeline_texture (blur);

  rig_gaussian_blur_update_factors (blur, n_taps);
}

static void
rig_gaussian_blur_render (RigGaussianBlur *blur)
{
  /* x pass */
  rig_camera_draw (blur->x_pass_camera, blur->x_pass_fb);

  cogl_framebuffer_draw_rectangle (blur->x_pass_fb,
                                   blur->x_pass_pipeline,
                                   0,
                                   0,
                                   blur->width,
                                   blur->height);

  rig_camera_end_frame (blur->x_pass_camera);

  /* y pass */
  rig_camera_draw (blur->y_pass_camera, blur->y_pass_fb);

  cogl_framebuffer_draw_rectangle (blur->y_pass_fb,
                                   blur->y_pass_pipeline,
                                   0,
                                   0,
                                   blur->width,
                                   blur->height);

  rig_camera_end_frame (blur->y_pass_camera);
}

/*
 * RigDepthOfField
 *
 * FIXME: Put the whole DoF logic in there
 */

static void
rig_depth_of_field_init (RigDepthOfField *dof,
                         RigContext *ctx,
                         CoglTexture *source,
                         CoglTexture *blurred)
{
  CoglPipeline *pipeline;
  CoglSnippet *snippet;

  dof->source = cogl_object_ref (source);
  dof->blurred_texture = cogl_object_ref (blurred);

  pipeline = cogl_pipeline_new (ctx->cogl_context);
  dof->pipeline = pipeline;

  cogl_pipeline_set_layer_texture (pipeline, 0, source);
  cogl_pipeline_set_layer_texture (pipeline, 1, blurred);

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                              NULL, /* definitions */
                              NULL  /* post */);

  cogl_snippet_set_replace (snippet,
      "cogl_texel0 = texture2D (cogl_sampler0, cogl_tex_coord_in[0].st);\n"
      "cogl_texel1 = texture2D (cogl_sampler1, cogl_tex_coord_in[1].st);\n"
      "cogl_color_out = mix (cogl_texel0, cogl_texel1, cogl_texel0.a);\n"
      "cogl_color_out.a = 1.0;\n");

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);
}

/* in micro seconds  */
static int64_t
get_current_time (Data *data)
{
  return (int64_t) (g_timer_elapsed (data->timer, NULL) * 1e6);
}

static void
compute_light_shadow_matrix (Data       *data,
                             CoglMatrix *light_matrix,
                             CoglMatrix *light_projection,
                             RigEntity  *light)
{
  CoglMatrix *main_camera, *light_transform, light_view;
  /* Move the unit data from [-1,1] to [0,1], column major order */
  float bias[16] = {
    .5f, .0f, .0f, .0f,
    .0f, .5f, .0f, .0f,
    .0f, .0f, .5f, .0f,
    .5f, .5f, .5f, 1.f
  };

  main_camera = rig_entity_get_transform (data->main_camera);
  light_transform = rig_entity_get_transform (light);
  cogl_matrix_get_inverse (light_transform, &light_view);

  cogl_matrix_init_from_array (light_matrix, bias);
  cogl_matrix_multiply (light_matrix, light_matrix, light_projection);
  cogl_matrix_multiply (light_matrix, light_matrix, &light_view);
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

/* This adds the Depth of Field bits to the main pipeline used for rendering
 * the scene. The goal here is to store some idea of how blurry the final
 * pixel should be in the alpha component of the rendered texture, blurriness
 * base on the distance of the vertex to the focal plane in */
static void
add_dof_snippet (CoglPipeline *pipeline)
{
  CoglSnippet *snippet;

  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX,

      /* definitions */
      "uniform float dof_focal_length;\n"
      "uniform float dof_focal_distance;\n"
      "uniform mat4  dof_camera;\n"

      "varying float dof_blur;\n",

      /* compute the amount of bluriness we want */
      "vec4 world_pos = cogl_modelview_matrix * cogl_position_in;\n"
      "dof_blur = clamp (abs (world_pos.z - dof_focal_length) /\n"
      "                  dof_focal_distance, 0.0, 1.0);\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);

/* This was used to debug the focal distance and bluriness amout in the DoF
 * effect: */
#if 0
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
      "varying vec4 world_pos;\n"
      "varying float dof_blur;",

     "cogl_color_out = vec4(dof_blur,0,0,1);\n"
     "if (abs (world_pos.z + 20.f) < 0.1) cogl_color_out = vec4(0,1,0,1);\n"
  );
#endif

  /* store the bluriness in the alpha channel */
  snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
      "varying float dof_blur;",

      "cogl_color_out.a = dof_blur;\n"
  );

  cogl_pipeline_add_snippet (pipeline, snippet);
  cogl_object_unref (snippet);
}

static void
set_focal_parameters (CoglPipeline *pipeline,
                      float         focal_length,
                      float         focal_distance)
{
  int location;
  float length;

  /* I want to have the focal length as positive when it's in front of the
   * camera (it seems more natural, but as, in OpenGL, the camera is facing
   * the negative Ys, the actual value to give to the shader has to be
   * negated */
  length = -focal_length;

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "dof_focal_length");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   1 /* n_components */, 1 /* count */,
                                   &length);

  location = cogl_pipeline_get_uniform_location (pipeline,
                                                 "dof_focal_distance");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   1 /* n_components */, 1 /* count */,
                                   &focal_distance);
}

static void
draw_entities (Data            *data,
               CoglFramebuffer *fb,
               RigEntity       *camera,
               gboolean         shadow_pass)
{
  GList *l;

  for (l = data->entities; l; l = l->next)
    {
      RigEntity *entity = l->data;
      const CoglMatrix *transform;

      if (shadow_pass && !rig_entity_get_cast_shadow (entity))
        continue;

      cogl_framebuffer_push_matrix (fb);

      transform = rig_entity_get_transform (entity);
      cogl_framebuffer_transform (fb, transform);

      rig_entity_draw (entity, fb);

      cogl_framebuffer_pop_matrix (fb);
    }
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
  CoglColor color;
  float vector3[3];
  int i;

  data->ctx = rig_context_new (data->shell);

  rig_context_init (data->ctx);

  data->fb_width = 800;
  data->fb_height = 600;
  onscreen = cogl_onscreen_new (data->ctx->cogl_context,
                                data->fb_width,
                                data->fb_height);
  data->fb = COGL_FRAMEBUFFER (onscreen);

  cogl_onscreen_show (onscreen);

  /*
   * Offscreen render for post-processing
   */
  color_buffer = cogl_texture_2d_new_with_size (rig_cogl_context,
                                                data->fb_width,
                                                data->fb_height,
                                                COGL_PIXEL_FORMAT_RGBA_8888,
                                                &error);
  if (error)
    g_critical ("could not create texture: %s", error->message);

  data->postprocess = COGL_FRAMEBUFFER (
    cogl_offscreen_new_to_texture (COGL_TEXTURE (color_buffer)));

  data->postprocess_color = color_buffer;

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
   * Depth of Field
   */

  add_dof_snippet (root_pipeline);
  set_focal_parameters (root_pipeline, 20.f, 15.0f);
  rig_downsample_init (&data->down,
                       data->ctx,
                       COGL_TEXTURE (data->postprocess_color),
                       4, 4);
  rig_gaussian_blur_init (&data->blur, data->ctx, data->down.destination, 7);
  rig_depth_of_field_init (&data->dof,
                           data->ctx,
                           COGL_TEXTURE (data->postprocess_color),
                           data->blur.destination);

  /*
   * Setup CoglObjects to render our plane and cube
   */

  data->scene = rig_graph_new (data->ctx, NULL);

  /* camera */
  data->main_camera = rig_entity_new (data->ctx, data->next_entity_id++);
  //data->entities = g_list_prepend (data->entities, data->main_camera);

  data->main_camera_z = 20.f;
  vector3[0] = 0.f;
  vector3[1] = 0.f;
  vector3[2] = data->main_camera_z;
  rig_entity_set_position (data->main_camera, vector3);

  component = rig_camera_new (data->ctx, data->fb);
  data->main_camera_component = component;

  rig_camera_set_projection_mode (RIG_CAMERA (component),
                                  RIG_PROJECTION_PERSPECTIVE);
  rig_camera_set_field_of_view (RIG_CAMERA (component), 60.f);
  rig_camera_set_near_plane (RIG_CAMERA (component), 1.1f);
  rig_camera_set_far_plane (RIG_CAMERA (component), 100.f);

  rig_entity_add_component (data->main_camera, component);

  rig_graphable_add_child (data->scene, data->main_camera);

  /* light */
  data->light = rig_entity_new (data->ctx, data->next_entity_id++);
  data->entities = g_list_prepend (data->entities, data->light);

  vector3[0] = 12.0f;
  vector3[1] = 8.0f;
  vector3[2] = -2.0f;
  rig_entity_set_position (data->light, vector3);

  rig_entity_rotate_x_axis (data->light, -120);
  rig_entity_rotate_y_axis (data->light, 10);

  component = rig_light_new ();
  cogl_color_init_from_4f (&color, .2f, .2f, .2f, 1.f);
  rig_light_set_ambient (RIG_LIGHT (component), &color);
  cogl_color_init_from_4f (&color, .6f, .6f, .6f, 1.f);
  rig_light_set_diffuse (RIG_LIGHT (component), &color);
  cogl_color_init_from_4f (&color, .4f, .4f, .4f, 1.f);
  rig_light_set_specular (RIG_LIGHT (component), &color);
  rig_light_add_pipeline (RIG_LIGHT (component), root_pipeline);


  rig_entity_add_component (data->light, component);

  component = rig_camera_new (data->ctx, COGL_FRAMEBUFFER (data->shadow_fb));
  data->shadow_map_camera = component;

  rig_camera_set_background_color4f (RIG_CAMERA (component), 0.f, .3f, 0.f, 1.f);
  rig_camera_set_projection_mode (RIG_CAMERA (component),
                                  RIG_PROJECTION_ORTHOGRAPHIC);
  rig_camera_set_orthographic_coordinates (RIG_CAMERA (component),
                                           15, 5, -15, -5);
  rig_camera_set_near_plane (RIG_CAMERA (component), 1.1f);
  rig_camera_set_far_plane (RIG_CAMERA (component), 20.f);

  rig_entity_add_component (data->light, component);

  rig_graphable_add_child (data->scene, data->light);

  /* plane */
  data->plane = rig_entity_new (data->ctx, data->next_entity_id++);
  data->entities = g_list_prepend (data->entities, data->plane);
  data->pickables = g_list_prepend (data->pickables, data->plane);
  rig_entity_set_cast_shadow (data->plane, FALSE);
  rig_entity_set_y (data->plane, -1.f);

  component = rig_mesh_renderer_new_from_template (data->ctx, "plane");
  rig_entity_add_component (data->plane, component);
  component = rig_material_new_with_pipeline (data->ctx, root_pipeline);
  rig_entity_add_component (data->plane, component);

  rig_graphable_add_child (data->scene, data->plane);

  /* 5 cubes */
  pipeline = cogl_pipeline_copy (root_pipeline);
  cogl_pipeline_set_color4f (pipeline, 0.6f, 0.6f, 0.6f, 1.0f);
  for (i = 0; i < N_CUBES; i++)
    {

      data->cubes[i] = rig_entity_new (data->ctx, data->next_entity_id++);
      data->entities = g_list_prepend (data->entities, data->cubes[i]);
      data->pickables = g_list_prepend (data->pickables, data->cubes[i]);

      rig_entity_set_cast_shadow (data->cubes[i], TRUE);
      rig_entity_set_x (data->cubes[i], i * 2.5f);
#if 0
      rig_entity_set_y (data->cubes[i], .5);
      rig_entity_set_z (data->cubes[i], 1);
      rig_entity_rotate_y_axis (data->cubes[i], 10);
#endif

      component = rig_mesh_renderer_new_from_template (data->ctx, "cube");
      rig_entity_add_component (data->cubes[i], component);
      component = rig_material_new_with_pipeline (data->ctx, pipeline);
      //cogl_object_unref (pipeline);
      rig_entity_add_component (data->cubes[i], component);

      rig_graphable_add_child (data->scene, data->cubes[i]);
    }
  cogl_object_unref (pipeline);

  /* create the pipelines to display the shadow color and depth textures */
  data->shadow_color_tex =
      rig_util_create_texture_pipeline (COGL_TEXTURE (data->shadow_color));
  data->shadow_map_tex =
      rig_util_create_texture_pipeline (COGL_TEXTURE (data->shadow_map));

  cogl_object_unref (root_pipeline);

  /* editor data */
  {
    int w, h;

    w = cogl_framebuffer_get_width (data->fb);
    h = cogl_framebuffer_get_height (data->fb);

    rig_arcball_init (&data->arcball, w / 2, h / 2, sqrtf (w * w + h * h) / 2);

    /* picking ray */
    data->picking_ray_color = create_color_pipeline (1.f, 0.f, 0.f);

  }

  /* UI layer camera */
  data->ui_camera = rig_entity_new (data->ctx, data->next_entity_id++);

  component = rig_camera_new (data->ctx, data->fb);
  data->ui_camera_component = component;
  rig_camera_set_projection_mode (RIG_CAMERA (component),
                                  RIG_PROJECTION_ORTHOGRAPHIC);
  rig_camera_set_orthographic_coordinates (RIG_CAMERA (component),
                                           0,
                                           0,
                                           data->fb_width,
                                           data->fb_height);
  rig_camera_set_near_plane (RIG_CAMERA (component), -64.f);
  rig_camera_set_far_plane (RIG_CAMERA (component), 64.f);
  rig_camera_set_clear (RIG_CAMERA (component), FALSE);

  rig_entity_add_component (data->ui_camera, component);

  /* tool */
  data->tool = rig_tool_new (data->shell);
  rig_tool_set_camera (data->tool, data->main_camera);

  /* we default to edit mode */
  data->edit = TRUE;

  /* We draw/pick the entities in the order they are listed and so
   * that matches the order we created the entities we now reverse the
   * list... */
  data->entities = g_list_reverse (data->entities);
  data->pickables = g_list_reverse (data->pickables);

  /* timer for the world time */
  data->timer = g_timer_new ();
  g_timer_start (data->timer);
}

static void
camera_update_view (RigEntity *camera, CoglBool shadow_map)
{
  RigCamera *camera_component =
    rig_entity_get_component (camera, RIG_COMPONENT_TYPE_CAMERA);
  CoglMatrix transform;
  CoglMatrix view;

  rig_graphable_get_transform (camera, &transform);
  cogl_matrix_get_inverse (&transform, &view);

  if (shadow_map)
    {
      CoglMatrix flipped_view;
      cogl_matrix_init_identity (&flipped_view);
      cogl_matrix_scale (&flipped_view, 1, -1, 1);
      cogl_matrix_multiply (&flipped_view, &flipped_view, &view);
      rig_camera_set_view_transform (camera_component, &flipped_view);
    }
  else
    rig_camera_set_view_transform (camera_component, &view);
}

static CoglBool
test_paint (RigShell *shell, void *user_data)
{
  Data *data = user_data;
  int64_t time; /* micro seconds */
  CoglFramebuffer *shadow_fb, *draw_fb;
  GList *l;

  /*
   * update entities
   */

  time = get_current_time (data);

  camera_update_view (data->main_camera, FALSE);
  camera_update_view (data->light, TRUE);
  camera_update_view (data->ui_camera, FALSE);

  for (l = data->entities; l; l = l->next)
    {
      RigEntity *entity = l->data;

      rig_entity_update (entity, time);
    }
  rig_entity_update (data->ui_camera, time);

  /*
   * render the shadow map
   */

  shadow_fb = COGL_FRAMEBUFFER (data->shadow_fb);

  /* update uniforms in materials */
  {
    CoglMatrix light_shadow_matrix, light_projection;
    CoglPipeline *pipeline;
    RigMaterial *material;
    const float *light_matrix;
    int location;

    cogl_framebuffer_get_projection_matrix (shadow_fb, &light_projection);
    compute_light_shadow_matrix (data,
                                 &light_shadow_matrix,
                                 &light_projection,
                                 data->light);
    light_matrix = cogl_matrix_get_array (&light_shadow_matrix);

    /* plane material */
    material = rig_entity_get_component (data->plane,
                                         RIG_COMPONENT_TYPE_MATERIAL);
    pipeline = rig_material_get_pipeline (material);
    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      light_matrix);

    /* cubes material */
    material = rig_entity_get_component (data->cubes[0],
                                         RIG_COMPONENT_TYPE_MATERIAL);
    pipeline = rig_material_get_pipeline (material);
    location = cogl_pipeline_get_uniform_location (pipeline,
                                                   "light_shadow_matrix");
    cogl_pipeline_set_uniform_matrix (pipeline,
                                      location,
                                      4, 1,
                                      FALSE,
                                      light_matrix);
  }

  rig_camera_flush (data->shadow_map_camera);

  draw_entities (data, shadow_fb, data->light, TRUE /* shadow pass */);

  rig_camera_end_frame (data->shadow_map_camera);

  /*
   * render the scene
   */

  /* post processing or not? */
  if (data->edit)
    draw_fb = data->fb;
  else
    draw_fb = data->postprocess;

  rig_camera_set_framebuffer (data->main_camera_component, draw_fb);

  rig_camera_flush (data->main_camera_component);

  /* draw entities */
  draw_entities (data, draw_fb, data->main_camera, FALSE);

  if (data->debug_pick_ray && data->picking_ray)
    {
      cogl_framebuffer_draw_primitive (draw_fb,
                                       data->picking_ray_color,
                                       data->picking_ray);
    }

  if (data->edit && data->selected_entity)
    {
      rig_tool_update (data->tool, data->selected_entity);
      rig_tool_draw (data->tool, draw_fb);
    }

  rig_camera_end_frame (data->main_camera_component);

  /* The UI layer is drawn using an orthographic projection */
  rig_camera_flush (data->ui_camera_component);

  cogl_framebuffer_push_matrix (data->fb);
  cogl_framebuffer_identity_matrix (data->fb);

  /* draw the postprocess framebuffer to the real onscreen with the
   * postprocess pipeline */
  if (!data->edit)
    {
      rig_downsample_render (&data->down);
      rig_gaussian_blur_render (&data->blur);

      cogl_framebuffer_draw_rectangle (data->fb, data->dof.pipeline,
                                       0, 0, data->fb_width, data->fb_height);
    }

  /* draw the color and depth buffers of the shadow FBO to debug them */
  if (data->debug_shadows)
    {
      cogl_framebuffer_draw_rectangle (data->fb, data->shadow_color_tex,
                                       128, 128, 0, 0);
      cogl_framebuffer_draw_rectangle (data->fb, data->shadow_map_tex,
                                       128, 256, 0, 128);
    }

  cogl_framebuffer_pop_matrix (data->fb);

  rig_camera_end_frame (data->ui_camera_component);

  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));

  return TRUE;
}

static void
test_fini (RigShell *shell, void *user_data)
{

}

static CoglPrimitive *
create_line_primitive (float a[3], float b[3])
{
  CoglVertexP3 data[2];
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[1];
  CoglPrimitive *primitive;

  data[0].x = a[0];
  data[0].y = a[1];
  data[0].z = a[2];
  data[1].x = b[0];
  data[1].y = b[1];
  data[1].z = b[2];

  attribute_buffer = cogl_attribute_buffer_new (rig_cogl_context,
                                                2 * sizeof (CoglVertexP3),
                                                data);

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (CoglVertexP3),
                                      offsetof (CoglVertexP3, x),
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  primitive = cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_LINES,
                                                  2, attributes, 1);

  cogl_object_unref (attribute_buffer);
  cogl_object_unref (attributes[0]);

  return primitive;
}

static void
transform_ray (CoglMatrix *transform,
               bool        inverse_transform,
               float       ray_origin[3],
               float       ray_direction[3])
{
  CoglMatrix inverse, normal_matrix, *m;

  m = transform;
  if (inverse_transform)
    {
      cogl_matrix_get_inverse (transform, &inverse);
      m = &inverse;
    }

  cogl_matrix_transform_points (m,
                                3, /* num components for input */
                                sizeof (float) * 3, /* input stride */
                                ray_origin,
                                sizeof (float) * 3, /* output stride */
                                ray_origin,
                                1 /* n_points */);

  cogl_matrix_get_inverse (m, &normal_matrix);
  cogl_matrix_transpose (&normal_matrix);

  rig_util_transform_normal (&normal_matrix,
                             &ray_direction[0],
                             &ray_direction[1],
                             &ray_direction[2]);
}

static CoglPrimitive *
create_picking_ray (Data            *data,
                    CoglFramebuffer *fb,
                    float            ray_position[3],
                    float            ray_direction[3],
                    float            length)
{
  CoglPrimitive *line;
  float points[6];

  points[0] = ray_position[0];
  points[1] = ray_position[1];
  points[2] = ray_position[2];
  points[3] = ray_position[0] + length * ray_direction[0];
  points[4] = ray_position[1] + length * ray_direction[1];
  points[5] = ray_position[2] + length * ray_direction[2];

  line = create_line_primitive (points, points + 3);

  return line;
}

static RigEntity *
pick (Data  *data,
      float  ray_origin[3],
      float  ray_direction[3])
{
  RigEntity *entity, *selected_entity = NULL;
  RigComponent *mesh;
  uint8_t *vertex_data;
  int n_vertices;
  size_t stride;
  int index;
  int i;
  float distance, min_distance = G_MAXFLOAT;
  bool hit;
  float transformed_ray_origin[3];
  float transformed_ray_direction[3];
  static const char *names[11] = { "plane", "cube0", "cube1", "cube2",
                                  "cube3", "cube4", "cube5", "cube6",
                                  "cube7", "cube8", "cube9"}, *name;
  GList *l;

  for (l = data->pickables, i = 0; l; l = l->next, i++)
    {
      entity = l->data;

      mesh = rig_entity_get_component (entity, RIG_COMPONENT_TYPE_GEOMETRY);
      if (!mesh || rig_object_get_type (mesh) != &rig_mesh_renderer_type)
        continue;

      /* transform the ray into the model space */
      memcpy (transformed_ray_origin, ray_origin, 3 * sizeof (float));
      memcpy (transformed_ray_direction, ray_direction, 3 * sizeof (float));
      transform_ray (rig_entity_get_transform (entity),
                     TRUE, /* inverse of the transform */
                     transformed_ray_origin,
                     transformed_ray_direction);

      /* intersect the transformed ray with the mesh data */
      vertex_data =
        rig_mesh_renderer_get_vertex_data (RIG_MESH_RENDERER (mesh),
                                           &stride, &n_vertices);

      hit = rig_util_intersect_mesh (vertex_data,
                                     n_vertices,
                                     stride,
                                     transformed_ray_origin,
                                     transformed_ray_direction,
                                     &index,
                                     &distance);

      /* to compare intersection distances we need to re-transform it back
       * to the world space, */
      cogl_vector3_normalize (transformed_ray_direction);
      transformed_ray_direction[0] *= distance;
      transformed_ray_direction[1] *= distance;
      transformed_ray_direction[2] *= distance;

      rig_util_transform_normal (rig_entity_get_transform (entity),
                                 &transformed_ray_direction[0],
                                 &transformed_ray_direction[1],
                                 &transformed_ray_direction[2]);
      distance = cogl_vector3_magnitude (transformed_ray_direction);

      if (hit && distance < min_distance)
        {
          min_distance = distance;
          selected_entity = entity;
          name = names[i];
        }
    }

  if (selected_entity)
    {
      g_message ("Hit the %s, triangle #%d, distance %.2f",
                 name, index, distance);
    }

  return selected_entity;
}

static void
update_camera_position (Data *data)
{
  /* Calculate where the origin currently is from the camera's
   * point of view. Then we can fixup the camera's position
   * so this matches the real position of the origin. */
  float relative_origin[3] = { 0, 0, -data->main_camera_z };
  rig_entity_get_transformed_position (data->main_camera,
                                       relative_origin);

  rig_entity_translate (data->main_camera,
                        -relative_origin[0],
                        -relative_origin[1],
                        -relative_origin[2]);
}

static RigInputEventStatus
test_input_handler (RigInputEvent *event, void *user_data)
{
  Data *data = user_data;
  RigMotionEventAction action;
  RigButtonState state;
  RigInputEventStatus status = RIG_INPUT_EVENT_STATUS_UNHANDLED;
  float x, y;

  switch (rig_input_event_get_type (event))
    {
      case RIG_INPUT_EVENT_TYPE_MOTION:
      {
        action = rig_motion_event_get_action (event);
        state = rig_motion_event_get_button_state (event);
        x = rig_motion_event_get_x (event);
        y = rig_motion_event_get_y (event);

        if (action == RIG_MOTION_EVENT_ACTION_DOWN &&
            state == RIG_BUTTON_STATE_2)
          {
            data->saved_rotation = *rig_entity_get_rotation (data->main_camera);

            cogl_quaternion_init_identity (&data->arcball.q_drag);

            rig_arcball_mouse_down (&data->arcball, data->fb_width - x, y);

            data->button_down = TRUE;

            status = RIG_INPUT_EVENT_STATUS_HANDLED;
          }
        else if (action == RIG_MOTION_EVENT_ACTION_DOWN &&
                 state == RIG_BUTTON_STATE_1)
          {
            /* pick */
            RigComponent *camera;
            float ray_position[3], ray_direction[3], screen_pos[2],
                  z_far, z_near;
            const float *viewport;
            CoglMatrix *camera_transform;
            const CoglMatrix *inverse_projection;

            camera = rig_entity_get_component (data->main_camera,
                                               RIG_COMPONENT_TYPE_CAMERA);
            viewport = rig_camera_get_viewport (RIG_CAMERA (camera));
            z_near = rig_camera_get_near_plane (RIG_CAMERA (camera));
            z_far = rig_camera_get_far_plane (RIG_CAMERA (camera));
            inverse_projection =
              rig_camera_get_inverse_projection (RIG_CAMERA (camera));

            camera_transform = rig_entity_get_transform (data->main_camera);

            screen_pos[0] = x;
            screen_pos[1] = y;

            rig_util_create_pick_ray (viewport,
                                      inverse_projection,
                                      camera_transform,
                                      screen_pos,
                                      ray_position,
                                      ray_direction);

            if (data->picking_ray)
              cogl_object_unref (data->picking_ray);
            data->picking_ray = create_picking_ray (data,
                                                    data->fb,
                                                    ray_position,
                                                    ray_direction,
                                                    z_far - z_near);

            data->selected_entity = pick (data, ray_position, ray_direction);
            if (data->selected_entity == NULL)
              rig_tool_update (data->tool, NULL);

            //status = RIG_INPUT_EVENT_STATUS_HANDLED;
          }
        else if (action == RIG_MOTION_EVENT_ACTION_MOVE &&
                 state == RIG_BUTTON_STATE_2)
          {
            CoglQuaternion new_rotation;

            if (!data->button_down)
              break;

            rig_arcball_mouse_motion (&data->arcball, data->fb_width - x, y);

            cogl_quaternion_multiply (&new_rotation,
                                      &data->saved_rotation,
                                      &data->arcball.q_drag);

            rig_entity_set_rotation (data->main_camera, &new_rotation);

            /* XXX: The remaining problem is calculating the new
             * position for the camera!
             *
             * If we transform the point (0, 0, camera_z) by the
             * camera's transform we can find where the origin is
             * relative to the camera, and then find out how far that
             * point is from the true origin so we know how to
             * translate the camera.
             */
            update_camera_position (data);

            status = RIG_INPUT_EVENT_STATUS_HANDLED;
          }
        else if (action == RIG_MOTION_EVENT_ACTION_DOWN &&
                 state == RIG_BUTTON_STATE_WHEELUP)
          {
            data->main_camera_z += 5;
            update_camera_position (data);
          }
        else if (action == RIG_MOTION_EVENT_ACTION_DOWN &&
                 state == RIG_BUTTON_STATE_WHEELDOWN)
          {
            if (data->main_camera_z >= 5)
              data->main_camera_z -= 5;
            update_camera_position (data);
          }
        else if (action == RIG_MOTION_EVENT_ACTION_UP)
          {
            data->button_down = FALSE;
          }

      break;
      }

      case RIG_INPUT_EVENT_TYPE_KEY:
      {
        RigKeyEventAction action;
        int32_t key;

        key = rig_key_event_get_keysym (event);
        action = rig_key_event_get_action (event);

        switch (key)
          {
          case RIG_KEY_p:
            if (action == RIG_KEY_EVENT_ACTION_UP)
              {
                data->edit = !data->edit;
                status = RIG_INPUT_EVENT_STATUS_UNHANDLED;
              }
            break;
          case RIG_KEY_minus:
            data->main_camera_z += 5;
            update_camera_position (data);
            break;
          case RIG_KEY_equal:
            if (data->main_camera_z >= 5)
              data->main_camera_z -= 5;
            update_camera_position (data);
            break;

          default:
            break;
          }
      }
    break;

      default:
        break;
    }

  return status;
}

#ifdef __ANDROID__

void
android_main (struct android_app *application)
{
  Data data;

  /* Make sure glue isn't stripped */
  app_dummy ();

  g_android_init ();

  memset (&data, 0, sizeof (Data));

  data.shell = rig_android_shell_new (application,
                                      test_init,
                                      test_fini,
                                      test_paint,
                                      &data);

  rig_shell_set_input_callback (data.shell, test_input_handler, &data);

  rig_shell_main (data.shell);
}

#else

int
main (int argc, char **argv)
{
  Data data;

  memset (&data, 0, sizeof (Data));

  data.shell = rig_shell_new (test_init, test_fini, test_paint, &data);

  rig_shell_set_input_callback (data.shell, test_input_handler, &data);

  rig_shell_main (data.shell);

  return 0;
}

#endif
