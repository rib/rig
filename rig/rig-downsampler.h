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

#ifndef __RIG_DOWNSAMPLER_H__
#define __RIG_DOWNSAMPLER_H__

#include <cogl/cogl.h>

#include "rig-context.h"
#include "rig-camera-private.h"

typedef struct
{
  RigContext *ctx;
  CoglPipeline *pipeline;
  CoglTexture *dest;
  CoglFramebuffer *fb;
  RigCamera *camera;
} RigDownsampler;

RigDownsampler *
rig_downsampler_new (RigContext *ctx);

void
rig_downsampler_free (RigDownsampler *downsampler);

CoglTexture *
rig_downsampler_downsample (RigDownsampler *downsampler,
                            CoglTexture *source,
                            int scale_factor_x,
                            int scale_factor_y);

#endif /* __RIG_DOWNSAMPLER_H__ */
