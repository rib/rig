/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-util.h"
#include "cogl-color.h"
#include "cogl-color-private.h"

void
cogl_color_init_from_4ub (CoglColor *color,
                          uint8_t red,
                          uint8_t green,
                          uint8_t blue,
                          uint8_t alpha)
{
  color->red = red / 255.0f;
  color->green = green / 255.0f;
  color->blue = blue / 255.0f;
  color->alpha = alpha / 255.0f;
}

void
cogl_color_init_from_4f (CoglColor *color,
                         float red,
                         float green,
                         float blue,
                         float alpha)
{
  color->red = red;
  color->green = green;
  color->blue = blue;
  color->alpha = alpha;
}

void
cogl_color_init_from_4fv (CoglColor *color,
                          float *color_array)
{
  memcpy (color, color_array, sizeof (CoglColor));
}

uint8_t
cogl_color_get_red_byte (const CoglColor *color)
{
  return color->red * 255;
}

float
cogl_color_get_red_float (const CoglColor *color)
{
  return color->red;
}

float
cogl_color_get_red (const CoglColor *color)
{
  return color->red;
}

uint8_t
cogl_color_get_green_byte (const CoglColor *color)
{
  return color->green * 255;
}

float
cogl_color_get_green_float (const CoglColor *color)
{
  return color->green;
}

float
cogl_color_get_green (const CoglColor *color)
{
  return color->green;
}

uint8_t
cogl_color_get_blue_byte (const CoglColor *color)
{
  return color->blue * 255;
}

float
cogl_color_get_blue_float (const CoglColor *color)
{
  return color->blue;
}

float
cogl_color_get_blue (const CoglColor *color)
{
  return color->blue;
}

uint8_t
cogl_color_get_alpha_byte (const CoglColor *color)
{
  return color->alpha * 255;
}

float
cogl_color_get_alpha_float (const CoglColor *color)
{
  return color->alpha;
}

float
cogl_color_get_alpha (const CoglColor *color)
{
  return color->alpha;
}

void
cogl_color_set_red_byte (CoglColor *color,
                         uint8_t red)
{
  color->red = red / 255.0f;
}

void
cogl_color_set_red_float (CoglColor *color,
                          float red)
{
  color->red = red;
}

void
cogl_color_set_red (CoglColor *color,
                    float red)
{
  color->red = red;
}

void
cogl_color_set_green_byte (CoglColor *color,
                           uint8_t green)
{
  color->green = green / 255.0f;
}

void
cogl_color_set_green_float (CoglColor *color,
                            float green)
{
  color->green = green;
}

void
cogl_color_set_green (CoglColor *color,
                      float green)
{
  color->green = green;
}

void
cogl_color_set_blue_byte (CoglColor *color,
                          uint8_t blue)
{
  color->blue = blue / 255.0f;
}

void
cogl_color_set_blue_float (CoglColor *color,
                           float blue)
{
  color->blue = blue;
}

void
cogl_color_set_blue (CoglColor *color,
                     float blue)
{
  color->blue = blue;
}

void
cogl_color_set_alpha_byte (CoglColor *color,
                           uint8_t alpha)
{
  color->alpha = alpha / 255.0f;
}

void
cogl_color_set_alpha_float (CoglColor *color,
                            float alpha)
{
  color->alpha = alpha;
}

void
cogl_color_set_alpha (CoglColor *color,
                      float alpha)
{
  color->alpha = alpha;
}

void
cogl_color_premultiply (CoglColor *color)
{
  color->red *= color->alpha;
  color->green *= color->alpha;
  color->blue *= color->alpha;
}

void
cogl_color_unpremultiply (CoglColor *color)
{
  if (color->alpha != 0)
    {
      color->red /= color->alpha;
      color->green /= color->alpha;
      color->blue /= color->alpha;
    }
}

CoglBool
cogl_color_equal (const void *v1, const void *v2)
{
  _COGL_RETURN_VAL_IF_FAIL (v1 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (v2 != NULL, FALSE);

  return memcmp (v1, v2, sizeof (CoglColor)) == 0;
}

CoglColor *
cogl_color_copy (const CoglColor *color)
{
  if (G_LIKELY (color))
    return g_slice_dup (CoglColor, color);

  return NULL;
}

void
cogl_color_free (CoglColor *color)
{
  if (G_LIKELY (color))
    g_slice_free (CoglColor, color);
}

void
_cogl_color_get_rgba_4fv (const CoglColor *color,
                          float *dest)
{
  memcpy (dest, color, sizeof (CoglColor));
}
