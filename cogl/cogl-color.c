/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2012 Intel Corporation.
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
                          const float *color_array)
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

bool
cogl_color_equal (const void *v1, const void *v2)
{
  _COGL_RETURN_VAL_IF_FAIL (v1 != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (v2 != NULL, FALSE);

  return memcmp (v1, v2, sizeof (CoglColor)) == 0;
}

CoglColor *
cogl_color_copy (const CoglColor *color)
{
  if (C_LIKELY (color))
    return c_slice_dup (CoglColor, color);

  return NULL;
}

void
cogl_color_free (CoglColor *color)
{
  if (C_LIKELY (color))
    c_slice_free (CoglColor, color);
}

void
cogl_color_to_hsl (const CoglColor *color,
                   float           *hue,
                   float           *saturation,
                   float           *luminance)
{
  float red, green, blue;
  float min, max, delta;
  float h, l, s;

  red   = color->red;
  green = color->green;
  blue  = color->blue;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;

      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;

      if (red < blue)
	min = red;
      else
	min = blue;
    }

  l = (max + min) / 2;
  s = 0;
  h = 0;

  if (max != min)
    {
      if (l <= 0.5)
	s = (max - min) / (max + min);
      else
	s = (max - min) / (2.0 - max - min);

      delta = max - min;

      if (red == max)
	h = (green - blue) / delta;
      else if (green == max)
	h = 2.0 + (blue - red) / delta;
      else if (blue == max)
	h = 4.0 + (red - green) / delta;

      h *= 60;

      if (h < 0)
	h += 360.0;
    }

  if (hue)
    *hue = h;

  if (luminance)
    *luminance = l;

  if (saturation)
    *saturation = s;
}

void
cogl_color_init_from_hsl (CoglColor *color,
                          float      hue,
                          float      saturation,
                          float      luminance)
{
  float tmp1, tmp2;
  float tmp3[3];
  float clr[3];
  int   i;

  hue /= 360.0;

  if (saturation == 0)
    {
      cogl_color_init_from_4f (color, luminance, luminance, luminance, 1.0f);
      return;
    }

  if (luminance <= 0.5)
    tmp2 = luminance * (1.0 + saturation);
  else
    tmp2 = luminance + saturation - (luminance * saturation);

  tmp1 = 2.0 * luminance - tmp2;

  tmp3[0] = hue + 1.0 / 3.0;
  tmp3[1] = hue;
  tmp3[2] = hue - 1.0 / 3.0;

  for (i = 0; i < 3; i++)
    {
      if (tmp3[i] < 0)
        tmp3[i] += 1.0;

      if (tmp3[i] > 1)
        tmp3[i] -= 1.0;

      if (6.0 * tmp3[i] < 1.0)
        clr[i] = tmp1 + (tmp2 - tmp1) * tmp3[i] * 6.0;
      else if (2.0 * tmp3[i] < 1.0)
        clr[i] = tmp2;
      else if (3.0 * tmp3[i] < 2.0)
        clr[i] = (tmp1 + (tmp2 - tmp1) * ((2.0 / 3.0) - tmp3[i]) * 6.0);
      else
        clr[i] = tmp1;
    }

  color->red   = clr[0];
  color->green = clr[1];
  color->blue  = clr[2];
  color->alpha = 1.0f;
}

void
_cogl_color_get_rgba_4fv (const CoglColor *color,
                          float *dest)
{
  memcpy (dest, color, sizeof (CoglColor));
}
