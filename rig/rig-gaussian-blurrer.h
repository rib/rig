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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __RIG_GAUSSIAN_BLURRER_H__
#define __RIG_GAUSSIAN_BLURRER_H__

#include <cogl/cogl.h>

#include "rig-context.h"
#include "rig-camera-private.h"

typedef struct _RigGaussianBlurrer
{
  RigContext *ctx;

  int n_taps;

  int width, height;
  CoglPixelFormat format;

  RigCamera *x_pass_camera;
  CoglFramebuffer *x_pass_fb;
  CoglTexture *x_pass;
  CoglPipeline *x_pass_pipeline;

  RigCamera *y_pass_camera;
  CoglFramebuffer *y_pass_fb;
  CoglTexture *y_pass;
  CoglTexture *destination;
  CoglPipeline *y_pass_pipeline;
} RigGaussianBlurrer;

RigGaussianBlurrer *
rig_gaussian_blurrer_new (RigContext *ctx, int n_taps);

void
rig_gaussian_blurrer_free (RigGaussianBlurrer *blurrer);

CoglTexture *
rig_gaussian_blurrer_blur (RigGaussianBlurrer *blurrer,
                           CoglTexture *source);

#endif /* __RIG_GAUSSIAN_BLURRER_H__ */
