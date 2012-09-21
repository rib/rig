/*
 * Rut
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */

#ifndef _RUT_COLOR_H_
#define _RUT_COLOR_H_

#include <glib.h>

#include <cogl/cogl.h>

#include "rut-context.h"

void
rut_color_init_from_4ub (RutColor *color,
                         uint8_t red,
                         uint8_t green,
                         uint8_t blue,
                         uint8_t alpha);

static void inline
rut_color_init_from_4f (RutColor *color,
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

static inline uint8_t
rut_color_get_red_byte (const RutColor *color)
{
  return color->red * 255.0;
}

static inline uint8_t
rut_color_get_green_byte (const RutColor *color)
{
  return color->green * 255.0;
}

static inline uint8_t
rut_color_get_blue_byte (const RutColor *color)
{
  return color->blue * 255.0;
}

static inline uint8_t
rut_color_get_alpha_byte (const RutColor *color)
{
  return color->alpha * 255.0;
}

gboolean
rut_color_init_from_string (RutContext *ctx,
                            RutColor *color,
                            const char *str);

/**
 * rut_color_add:
 * @a: a #RutColor
 * @b: a #RutColor
 * @result: (out caller-allocates): return location for the result
 *
 * Adds @a to @b and saves the resulting color inside @result.
 *
 * The alpha channel of @result is set as as the maximum value
 * between the alpha channels of @a and @b.
 */
void
rut_color_add (const RutColor *a,
               const RutColor *b,
               RutColor *result);

/**
 * rut_color_subtract:
 * @a: a #RutColor
 * @b: a #RutColor
 * @result: (out caller-allocates): return location for the result
 *
 * Subtracts @b from @a and saves the resulting color inside @result.
 *
 * This function assumes that the components of @a are greater than the
 * components of @b; the result is, otherwise, undefined.
 *
 * The alpha channel of @result is set as the minimum value
 * between the alpha channels of @a and @b.
 */
void
rut_color_subtract (const RutColor *a,
                    const RutColor *b,
                    RutColor *result);

/**
 * rut_color_lighten:
 * @color: a #RutColor
 * @result: (out caller-allocates): return location for the lighter color
 *
 * Lightens @color by a fixed amount, and saves the changed color
 * in @result.
 */
void
rut_color_lighten (const RutColor *color,
		   RutColor *result);

/**
 * rut_color_darken:
 * @color: a #RutColor
 * @result: (out caller-allocates): return location for the darker color
 *
 * Darkens @color by a fixed amount, and saves the changed color
 * in @result.
 */
void
rut_color_darken (const RutColor *color,
		  RutColor *result);

/**
 * rut_color_to_hls:
 * @color: a #RutColor
 * @hue: (out): return location for the hue value or %NULL
 * @luminance: (out): return location for the luminance value or %NULL
 * @saturation: (out): return location for the saturation value or %NULL
 *
 * Converts @color to the HLS format.
 *
 * The @hue value is in the 0 .. 360 range. The @luminance and
 * @saturation values are in the 0 .. 1 range.
 */
void
rut_color_to_hls (const RutColor *color,
                  float *hue,
                  float *luminance,
                  float *saturation);

/**
 * rut_color_shade:
 * @color: a #RutColor
 * @factor: the shade factor to apply
 * @result: (out caller-allocates): return location for the shaded color
 *
 * Shades @color by @factor and saves the modified color into @result.
 */
void
rut_color_shade (const RutColor *color,
                 float factor,
                 RutColor *result);

/**
 * rut_color_to_string:
 * @color: a #RutColor
 *
 * Returns a textual specification of @color in the hexadecimal form
 * <literal>&num;rrggbbaa</literal>, where <literal>r</literal>,
 * <literal>g</literal>, <literal>b</literal> and <literal>a</literal> are
 * hexadecimal digits representing the red, green, blue and alpha components
 * respectively.
 *
 * Return value: (transfer full): a newly-allocated text string
 */
char *
rut_color_to_string (const RutColor *color);

/**
 * rut_color_interpolate:
 * @initial: the initial #RutColor
 * @final: the final #RutColor
 * @progress: the interpolation progress
 * @result: (out): return location for the interpolation
 *
 * Interpolates between @initial and @final #RutColor<!-- -->s
 * using @progress
 */
void
rut_color_interpolate (const RutColor *initial,
                       const RutColor *final,
                       float progress,
                       RutColor *result);

#endif /* _RUT_COLOR_H_ */
