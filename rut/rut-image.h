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

#ifndef _RUT_IMAGE_H_
#define _RUT_IMAGE_H_

#include "rut-property.h"

extern RutType rut_image_type;

typedef struct _RutImage RutImage;

#define RUT_IMAGE(x) ((RutImage *) x)

typedef enum
{
  /* Fills the widget with repeats of the image */
  RUT_IMAGE_DRAW_MODE_REPEAT,
  /* Scales the image to fill the size of the widget */
  RUT_IMAGE_DRAW_MODE_SCALE,
  /* Scales the image to fill the size of the widget as much as
   * possible without breaking the aspect ratio */
  RUT_IMAGE_DRAW_MODE_SCALE_WITH_ASPECT_RATIO
} RutImageDrawMode;

RutImage *
rut_image_new (RutContext *ctx,
               CoglTexture *texture);

void
rut_image_set_draw_mode (RutImage *image,
                         RutImageDrawMode draw_mode);

#endif /* _RUT_IMAGE_H_ */
