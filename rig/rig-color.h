/*
 * Rig
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

#ifndef _RIG_COLOR_H_
#define _RIG_COLOR_H_

#include <glib.h>

#include <cogl/cogl.h>

#include "rig-context.h"

void
rig_color_init_from_4ub (RigColor *color,
                         uint8_t red,
                         uint8_t green,
                         uint8_t blue,
                         uint8_t alpha);

static void inline
rig_color_init_from_4f (RigColor *color,
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
rig_color_get_red_byte (const RigColor *color)
{
  return color->red * 255.0;
}

static inline uint8_t
rig_color_get_green_byte (const RigColor *color)
{
  return color->green * 255.0;
}

static inline uint8_t
rig_color_get_blue_byte (const RigColor *color)
{
  return color->blue * 255.0;
}

static inline uint8_t
rig_color_get_alpha_byte (const RigColor *color)
{
  return color->alpha * 255.0;
}

gboolean
rig_color_init_from_string (RigContext *ctx,
                            RigColor *color,
                            const char *str);

/**
 * rig_color_add:
 * @a: a #RigColor
 * @b: a #RigColor
 * @result: (out caller-allocates): return location for the result
 *
 * Adds @a to @b and saves the resulting color inside @result.
 *
 * The alpha channel of @result is set as as the maximum value
 * between the alpha channels of @a and @b.
 */
void
rig_color_add (const RigColor *a,
               const RigColor *b,
               RigColor *result);

/**
 * rig_color_subtract:
 * @a: a #RigColor
 * @b: a #RigColor
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
rig_color_subtract (const RigColor *a,
                    const RigColor *b,
                    RigColor *result);

/**
 * rig_color_lighten:
 * @color: a #RigColor
 * @result: (out caller-allocates): return location for the lighter color
 *
 * Lightens @color by a fixed amount, and saves the changed color
 * in @result.
 */
void
rig_color_lighten (const RigColor *color,
		   RigColor *result);

/**
 * rig_color_darken:
 * @color: a #RigColor
 * @result: (out caller-allocates): return location for the darker color
 *
 * Darkens @color by a fixed amount, and saves the changed color
 * in @result.
 */
void
rig_color_darken (const RigColor *color,
		  RigColor *result);

/**
 * rig_color_to_hls:
 * @color: a #RigColor
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
rig_color_to_hls (const RigColor *color,
                  float *hue,
                  float *luminance,
                  float *saturation);

/**
 * rig_color_shade:
 * @color: a #RigColor
 * @factor: the shade factor to apply
 * @result: (out caller-allocates): return location for the shaded color
 *
 * Shades @color by @factor and saves the modified color into @result.
 */
void
rig_color_shade (const RigColor *color,
                 float factor,
                 RigColor *result);

/**
 * rig_color_to_string:
 * @color: a #RigColor
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
rig_color_to_string (const RigColor *color);

/**
 * rig_color_interpolate:
 * @initial: the initial #RigColor
 * @final: the final #RigColor
 * @progress: the interpolation progress
 * @result: (out): return location for the interpolation
 *
 * Interpolates between @initial and @final #RigColor<!-- -->s
 * using @progress
 */
void
rig_color_interpolate (const RigColor *initial,
                       const RigColor *final,
                       float progress,
                       RigColor *result);

#endif /* _RIG_COLOR_H_ */
