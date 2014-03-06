/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef __RUT_GAUSSIAN_BLURRER_H__
#define __RUT_GAUSSIAN_BLURRER_H__

#include <cogl/cogl.h>

#include "rut-context.h"

typedef struct _RutGaussianBlurrer
{
  RutContext *ctx;

  int n_taps;

  int width, height;
  CoglTextureComponents components;

  CoglFramebuffer *x_pass_fb;
  CoglTexture *x_pass;
  CoglPipeline *x_pass_pipeline;

  CoglFramebuffer *y_pass_fb;
  CoglTexture *y_pass;
  CoglTexture *destination;
  CoglPipeline *y_pass_pipeline;
} RutGaussianBlurrer;

RutGaussianBlurrer *
rut_gaussian_blurrer_new (RutContext *ctx, int n_taps);

void
rut_gaussian_blurrer_free (RutGaussianBlurrer *blurrer);

CoglTexture *
rut_gaussian_blurrer_blur (RutGaussianBlurrer *blurrer,
                           CoglTexture *source);

#endif /* __RUT_GAUSSIAN_BLURRER_H__ */
