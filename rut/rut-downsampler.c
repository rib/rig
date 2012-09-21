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

#include "rut-downsampler.h"
#include "rut/components/rut-camera.h"

RutDownsampler *
rut_downsampler_new (RutContext *ctx)
{
  RutDownsampler *downsampler = g_slice_new0 (RutDownsampler);
  CoglPipeline *pipeline;

  downsampler->ctx = ctx;

  pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_layer_texture (pipeline, 0, NULL);
  cogl_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);

  downsampler->pipeline = pipeline;

  return downsampler;
}

static void
_rut_downsampler_reset (RutDownsampler *downsampler)
{
  if (downsampler->dest)
    {
      cogl_object_unref (downsampler->dest);
      downsampler->dest = NULL;
    }

  if (downsampler->fb)
    {
      cogl_object_unref (downsampler->fb);
      downsampler->fb = NULL;
    }

  if (downsampler->camera)
    {
      rut_refable_unref (downsampler->camera);
      downsampler->camera = NULL;
    }
}

void
rut_downsampler_free (RutDownsampler *downsampler)
{
  _rut_downsampler_reset (downsampler);
  g_slice_free (RutDownsampler, downsampler);
}

CoglTexture *
rut_downsampler_downsample (RutDownsampler *downsampler,
                            CoglTexture *source,
                            int scale_factor_x,
                            int scale_factor_y)
{
  CoglPixelFormat format;
  int src_w, src_h;
  int dest_width, dest_height;
  CoglPipeline *pipeline;

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

  /* create the destination texture up front */
  dest_width = src_w / scale_factor_x;
  dest_height = src_h / scale_factor_y;
  format = cogl_texture_get_format (source);

  if (downsampler->dest == NULL ||
      cogl_texture_get_width (downsampler->dest) != dest_width ||
      cogl_texture_get_height (downsampler->dest) != dest_height ||
      cogl_texture_get_format (downsampler->dest) != format)
    {
      CoglError *error = NULL;
      CoglOffscreen *offscreen;
      CoglTexture2D *texture_2d =
        cogl_texture_2d_new_with_size (downsampler->ctx->cogl_context,
                                       dest_width,
                                       dest_height,
                                       format,
                                       &error);
      if (error)
        {
          g_warning ("downsample: could not create destination texture: %s",
                     error->message);
        }

      _rut_downsampler_reset (downsampler);

      downsampler->dest = COGL_TEXTURE (texture_2d);

      /* create the FBO to render the downsampled texture */
      offscreen = cogl_offscreen_new_to_texture (downsampler->dest);
      downsampler->fb = COGL_FRAMEBUFFER (offscreen);

      /* create the camera that will setup the scene for the render */
      downsampler->camera = rut_camera_new (downsampler->ctx, downsampler->fb);
      rut_camera_set_near_plane (downsampler->camera, -1.f);
      rut_camera_set_far_plane (downsampler->camera, 1.f);
    }

  pipeline = cogl_pipeline_copy (downsampler->pipeline);
  cogl_pipeline_set_layer_texture (pipeline, 0, source);

  rut_camera_flush (downsampler->camera);

  cogl_framebuffer_draw_rectangle (downsampler->fb,
                                   pipeline,
                                   0,
                                   0,
                                   dest_width,
                                   dest_height);

  rut_camera_end_frame (downsampler->camera);

  cogl_object_unref (pipeline);

  return cogl_object_ref (downsampler->dest);
}
