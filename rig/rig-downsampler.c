/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012,2013  Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <config.h>

#include <rut.h>

#include "rig-downsampler.h"
#include "rig-engine.h"

#include "components/rig-camera.h"

RigDownsampler *
rig_downsampler_new (RigEngine *engine)
{
  RigDownsampler *downsampler = c_slice_new0 (RigDownsampler);
  CoglPipeline *pipeline;

  downsampler->engine = engine;

  pipeline = cogl_pipeline_new (engine->ctx->cogl_context);
  cogl_pipeline_set_layer_texture (pipeline, 0, NULL);
  cogl_pipeline_set_blend (pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);

  downsampler->pipeline = pipeline;

  return downsampler;
}

static void
_rig_downsampler_reset (RigDownsampler *downsampler)
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
      rut_object_unref (downsampler->camera);
      downsampler->camera = NULL;
    }
}

void
rig_downsampler_free (RigDownsampler *downsampler)
{
  _rig_downsampler_reset (downsampler);
  c_slice_free (RigDownsampler, downsampler);
}

CoglTexture *
rig_downsampler_downsample (RigDownsampler *downsampler,
                            CoglTexture *source,
                            int scale_factor_x,
                            int scale_factor_y)
{
  CoglTextureComponents components;
  int src_w, src_h;
  int dest_width, dest_height;
  CoglPipeline *pipeline;

  /* validation */
  src_w = cogl_texture_get_width (source);
  src_h = cogl_texture_get_height (source);

  if (src_w % scale_factor_x != 0)
    {
      c_warning ("downsample: the width of the texture (%d) is not a "
                 "multiple of the scale factor (%d)", src_w, scale_factor_x);
    }
  if (src_h % scale_factor_y != 0)
    {
      c_warning ("downsample: the height of the texture (%d) is not a "
                 "multiple of the scale factor (%d)", src_h, scale_factor_y);
    }

  /* create the destination texture up front */
  dest_width = src_w / scale_factor_x;
  dest_height = src_h / scale_factor_y;
  components = cogl_texture_get_components (source);

  if (downsampler->dest == NULL ||
      cogl_texture_get_width (downsampler->dest) != dest_width ||
      cogl_texture_get_height (downsampler->dest) != dest_height ||
      cogl_texture_get_components (downsampler->dest) != components)
    {
      CoglOffscreen *offscreen;
      CoglTexture2D *texture_2d =
        cogl_texture_2d_new_with_size (downsampler->engine->ctx->cogl_context,
                                       dest_width,
                                       dest_height);

      cogl_texture_set_components (texture_2d, components);

      _rig_downsampler_reset (downsampler);

      downsampler->dest = texture_2d;

      /* create the FBO to render the downsampled texture */
      offscreen = cogl_offscreen_new_with_texture (downsampler->dest);
      downsampler->fb = offscreen;

      /* create the camera that will setup the scene for the render */
      downsampler->camera = rig_camera_new (downsampler->engine,
                                            dest_width, /* ortho width */
                                            dest_height, /* ortho height */
                                            downsampler->fb);
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
