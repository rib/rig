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

#include <math.h>

#include "rut-gaussian-blurrer.h"
#include "rut/components/rut-camera.h"

/*
 * RutGaussianBlur
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
create_1d_gaussian_blur_pipeline (RutContext *ctx, int n_taps)
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
    return cogl_object_ref (pipeline);

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

  cogl_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);

  g_hash_table_insert (pipeline_cache, GINT_TO_POINTER (n_taps), pipeline);

  return pipeline;
}

static void
set_blurrer_pipeline_factors (CoglPipeline *pipeline, int n_taps)
{
  int i, radius;
  float *factors, sigma;
  int location;
  float sum;
  float scale;

  radius = n_taps / 2; /* which is (n_taps - 1) / 2 as well */

  factors = g_alloca (n_taps * sizeof (float));
  sigma = n_taps_to_sigma (n_taps);

  sum = 0;
  for (i = -radius; i <= radius; i++)
    {
      factors[i + radius] = gaussian (sigma, i);
      sum += factors[i + radius];
    }

  /* So that we don't loose any brightness when blurring, we
   * normalized the factors... */
  scale = 1.0 / sum;
  for (i = -radius; i <= radius; i++)
    factors[i + radius] *= scale;

  location = cogl_pipeline_get_uniform_location (pipeline, "factors");
  cogl_pipeline_set_uniform_float (pipeline,
                                   location,
                                   1 /* n_components */,
                                   n_taps /* count */,
                                   factors);
}

static void
set_blurrer_pipeline_texture (CoglPipeline *pipeline,
                              CoglTexture *source,
                              float x_pixel_step,
                              float y_pixel_step)
{
  float pixel_step[2];
  int pixel_step_location;

  /* our input in the source texture */
  cogl_pipeline_set_layer_texture (pipeline,
                                   0, /* layer_num */
                                   source);

  pixel_step[0] = x_pixel_step;
  pixel_step[1] = y_pixel_step;
  pixel_step_location =
    cogl_pipeline_get_uniform_location (pipeline, "pixel_step");
  g_assert (pixel_step_location);
  cogl_pipeline_set_uniform_float (pipeline,
                                   pixel_step_location,
                                   2, /* n_components */
                                   1, /* count */
                                   pixel_step);
}

RutGaussianBlurrer *
rut_gaussian_blurrer_new (RutContext *ctx, int n_taps)
{
  RutGaussianBlurrer *blurrer = g_slice_new0 (RutGaussianBlurrer);
  CoglPipeline *base_pipeline;

  /* validation */
  if (n_taps < 5 || n_taps > 17 || n_taps % 2 == 0 )
    {
      g_critical ("blur: the numbers of taps must belong to the {5, 7, 9, "
                  "11, 13, 14, 17, 19} set");
      g_assert_not_reached ();
      return NULL;
    }

  blurrer->ctx = ctx;
  blurrer->n_taps = n_taps;

  base_pipeline = create_1d_gaussian_blur_pipeline (ctx, n_taps);

  blurrer->x_pass_pipeline = cogl_pipeline_copy (base_pipeline);
  set_blurrer_pipeline_factors (blurrer->x_pass_pipeline, n_taps);
  blurrer->y_pass_pipeline = cogl_pipeline_copy (base_pipeline);
  set_blurrer_pipeline_factors (blurrer->x_pass_pipeline, n_taps);

  cogl_object_unref (base_pipeline);

  return blurrer;
}

static void
_rut_gaussian_blurrer_free_buffers (RutGaussianBlurrer *blurrer)
{
  if (blurrer->x_pass)
    {
      cogl_object_unref (blurrer->x_pass);
      blurrer->x_pass = NULL;
    }
  if (blurrer->x_pass_fb)
    {
      cogl_object_unref (blurrer->x_pass_fb);
      blurrer->x_pass_fb = NULL;
    }

  if (blurrer->y_pass)
    {
      cogl_object_unref (blurrer->y_pass);
      blurrer->y_pass = NULL;
    }
  if (blurrer->y_pass_fb)
    {
      cogl_object_unref (blurrer->y_pass_fb);
      blurrer->y_pass_fb = NULL;
    }
}

void
rut_gaussian_blurrer_free (RutGaussianBlurrer *blurrer)
{
  _rut_gaussian_blurrer_free_buffers (blurrer);
  rut_refable_unref (blurrer->x_pass_camera);
  rut_refable_unref (blurrer->y_pass_camera);
  g_slice_free (RutGaussianBlurrer, blurrer);
}

CoglTexture *
rut_gaussian_blurrer_blur (RutGaussianBlurrer *blurrer,
                           CoglTexture *source)
{
  unsigned int src_w, src_h;
  CoglPixelFormat format;
  CoglOffscreen *offscreen;

  /* create the first FBO to render the x pass */
  src_w = cogl_texture_get_width (source);
  src_h = cogl_texture_get_height (source);
  format = cogl_texture_get_format (source);

  if (blurrer->width != src_w ||
      blurrer->height != src_h ||
      blurrer->format != format)
    {
      _rut_gaussian_blurrer_free_buffers (blurrer);
    }

  if (!blurrer->x_pass)
    {
      CoglError *error = NULL;
      CoglTexture2D *texture_2d =
        cogl_texture_2d_new_with_size (blurrer->ctx->cogl_context,
                                       src_w,
                                       src_h,
                                       format,
                                       &error);
      if (error)
        {
          g_warning ("blurrer: could not create x pass texture: %s",
                     error->message);
        }
      blurrer->x_pass = COGL_TEXTURE (texture_2d);
      blurrer->width = src_w;
      blurrer->height = src_h;
      blurrer->format = format;

      offscreen = cogl_offscreen_new_to_texture (blurrer->x_pass);
      blurrer->x_pass_fb = COGL_FRAMEBUFFER (offscreen);
    }

  if (!blurrer->y_pass)
    {
      CoglError *error = NULL;
      /* create the second FBO (final destination) to render the y pass */
      CoglTexture2D *texture_2d =
        cogl_texture_2d_new_with_size (blurrer->ctx->cogl_context,
                                       src_w,
                                       src_h,
                                       format,
                                       &error);
      if (error)
        {
          g_warning ("blurrer: could not create destination texture: %s",
                     error->message);
        }
      blurrer->destination = COGL_TEXTURE (texture_2d);
      blurrer->y_pass = blurrer->destination;

      offscreen = cogl_offscreen_new_to_texture (blurrer->destination);
      blurrer->y_pass_fb = COGL_FRAMEBUFFER (offscreen);
    }

  if (blurrer->x_pass_camera)
    rut_camera_set_framebuffer (blurrer->x_pass_camera, blurrer->x_pass_fb);
  else
    blurrer->x_pass_camera = rut_camera_new (blurrer->ctx, blurrer->x_pass_fb);

  if (blurrer->y_pass_camera)
    rut_camera_set_framebuffer (blurrer->y_pass_camera, blurrer->y_pass_fb);
  else
    blurrer->y_pass_camera = rut_camera_new (blurrer->ctx, blurrer->y_pass_fb);

  set_blurrer_pipeline_texture (blurrer->x_pass_pipeline, source, 1.0f / src_w, 0);
  set_blurrer_pipeline_texture (blurrer->y_pass_pipeline, blurrer->x_pass, 0, 1.0f / src_h);

  /* x pass */
  rut_camera_flush (blurrer->x_pass_camera);

  cogl_framebuffer_draw_rectangle (blurrer->x_pass_fb,
                                   blurrer->x_pass_pipeline,
                                   0,
                                   0,
                                   blurrer->width,
                                   blurrer->height);

  rut_camera_end_frame (blurrer->x_pass_camera);

  /* y pass */
  rut_camera_flush (blurrer->y_pass_camera);

  cogl_framebuffer_draw_rectangle (blurrer->y_pass_fb,
                                   blurrer->y_pass_pipeline,
                                   0,
                                   0,
                                   blurrer->width,
                                   blurrer->height);

  rut_camera_end_frame (blurrer->y_pass_camera);

  return cogl_object_ref (blurrer->destination);
}


