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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

#include <glib.h>

#include <cogl/cogl.h>

#include "color-table.h"
#include "rut-context.h"
#include "rut-color.h"

static inline void
skip_whitespace (char **str)
{
  while (g_ascii_isspace (**str))
    *str += 1;
}

static inline void
parse_rgb_value (char *str,
                 float *color,
                 char **endp)
{
  float number;
  char *p;

  skip_whitespace (&str);

  number = g_ascii_strtod (str, endp);

  p = *endp;

  skip_whitespace (&p);

  if (*p == '%')
    {
      *endp = p + 1;

      *color = CLAMP (number / 100.0, 0.0, 1.0);
    }
  else
    *color = CLAMP (number, 0.0, 1.0);
}

static gboolean
parse_rgba (CoglColor *color,
            char *str,
            gboolean has_alpha)
{
  float red, green, blue, alpha;

  skip_whitespace (&str);

  if (*str != '(')
    return FALSE;

  str += 1;

  /* red */
  parse_rgb_value (str, &red, &str);
  skip_whitespace (&str);
  if (*str != ',')
    return FALSE;

  str += 1;

  /* green */
  parse_rgb_value (str, &green, &str);
  skip_whitespace (&str);
  if (*str != ',')
    return FALSE;

  str += 1;

  /* blue */
  parse_rgb_value (str, &blue, &str);
  skip_whitespace (&str);

  /* alpha (optional); since the alpha channel value can only
   * be between 0 and 1 we don't use the parse_rgb_value()
   * function
   */
  if (has_alpha)
    {
      if (*str != ',')
        return FALSE;

      str += 1;

      skip_whitespace (&str);
      alpha = g_ascii_strtod (str, &str);
      alpha = CLAMP (alpha, 0.0, 1.0);
    }
  else
    alpha = 1.0;

  skip_whitespace (&str);
  if (*str != ')')
    return FALSE;

  cogl_color_init_from_4f (color, red, green, blue, alpha);

  return TRUE;
}

void
rut_color_init_from_hls (CoglColor *color,
                         float hue,
                         float luminance,
                         float saturation)
{
  float tmp1, tmp2;
  float tmp3[3];
  float clr[3];
  int   i;

  hue /= 360.0;

  if (saturation == 0)
    {
      cogl_color_init_from_4f (color, luminance, luminance, luminance, 1.0);
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

  color->red = clr[0];
  color->green = clr[1];
  color->blue = clr[2];
  color->alpha = 1.0;
}

static gboolean
parse_hsla (CoglColor *color,
            char *str,
            gboolean has_alpha)
{
  float number;
  float h, l, s, a;

  skip_whitespace (&str);

  if (*str != '(')
    return FALSE;

  str += 1;

  /* hue */
  skip_whitespace (&str);
  /* we don't do any angle normalization here because
   * rut_color_from_hls() will do it for us
   */
  number = g_ascii_strtod (str, &str);
  skip_whitespace (&str);
  if (*str != ',')
    return FALSE;

  h = number;

  str += 1;

  /* saturation */
  skip_whitespace (&str);
  number = g_ascii_strtod (str, &str);
  skip_whitespace (&str);
  if (*str != '%')
    return FALSE;

  str += 1;

  s = CLAMP (number / 100.0, 0.0, 1.0);
  skip_whitespace (&str);
  if (*str != ',')
    return FALSE;

  str += 1;

  /* luminance */
  skip_whitespace (&str);
  number = g_ascii_strtod (str, &str);
  skip_whitespace (&str);
  if (*str != '%')
    return FALSE;

  str += 1;

  l = CLAMP (number / 100.0, 0.0, 1.0);
  skip_whitespace (&str);

  /* alpha (optional); since the alpha channel value can only
   * be between 0 and 1 we don't use the parse_rgb_value()
   * function
   */
  if (has_alpha)
    {
      if (*str != ',')
        return FALSE;

      str += 1;

      skip_whitespace (&str);
      number = g_ascii_strtod (str, &str);

      a = CLAMP (number, 0.0, 1.0);
    }
  else
    a = 1.0;

  skip_whitespace (&str);
  if (*str != ')')
    return FALSE;

  rut_color_init_from_hls (color, h, l, s);
  color->alpha = a;

  return TRUE;
}

gboolean
rut_color_init_from_string (RutContext *ctx,
                            CoglColor *color,
                            const char *str)
{
  void *color_index_ptr;

  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (str != NULL, FALSE);

  if (strncmp (str, "rgb", 3) == 0)
    {
      char *s = (char *)str;
      gboolean res;

      if (strncmp (str, "rgba", 4) == 0)
        res = parse_rgba (color, s + 4, TRUE);
      else
        res = parse_rgba (color, s + 3, FALSE);

      return res;
    }

  if (strncmp (str, "hsl", 3) == 0)
    {
      char *s = (char *)str;
      gboolean res;

      if (strncmp (str, "hsla", 4) == 0)
        res = parse_hsla (color, s + 4, TRUE);
      else
        res = parse_hsla (color, s + 3, FALSE);

      return res;
    }

  /* if the string contains a color encoded using the hexadecimal
   * notations (#rrggbbaa or #rgba) we attempt a rough pass at
   * parsing the color ourselves, as we need the alpha channel that
   * Pango can't retrieve.
   */
  if (str[0] == '#' && str[1] != '\0')
    {
      guint8 red, green, blue, alpha;
      size_t length = strlen (str + 1);
      unsigned int result;

      if (sscanf (str + 1, "%x", &result) == 1)
        {
          switch (length)
            {
            case 8: /* rrggbbaa */
              red   = (result >> 24) & 0xff;
              green = (result >> 16) & 0xff;
              blue  = (result >>  8) & 0xff;

              alpha = result & 0xff;

              cogl_color_init_from_4ub (color, red, green, blue, alpha);

              return TRUE;

            case 6: /* #rrggbb */
              red   = (result >> 16) & 0xff;
              green = (result >>  8) & 0xff;
              blue  = result & 0xff;

              alpha = 0xff;

              cogl_color_init_from_4ub (color, red, green, blue, alpha);

              return TRUE;

            case 4: /* #rgba */
              red   = ((result >> 12) & 0xf);
              green = ((result >>  8) & 0xf);
              blue  = ((result >>  4) & 0xf);
              alpha = result & 0xf;

              red   = (red   << 4) | red;
              green = (green << 4) | green;
              blue  = (blue  << 4) | blue;
              alpha = (alpha << 4) | alpha;

              cogl_color_init_from_4ub (color, red, green, blue, alpha);

              return TRUE;

            case 3: /* #rgb */
              red   = ((result >>  8) & 0xf);
              green = ((result >>  4) & 0xf);
              blue  = result & 0xf;

              red   = (red   << 4) | red;
              green = (green << 4) | green;
              blue  = (blue  << 4) | blue;

              alpha = 0xff;

              cogl_color_init_from_4ub (color, red, green, blue, alpha);

              return TRUE;

            default:
              return FALSE;
            }
        }
    }

  /* fall back to X11-style named colors; see:
   *
   *   http://en.wikipedia.org/wiki/X11_color_names
   */

  if (!ctx->colors_hash)
    {
      int i, n_colors;

      ctx->colors_hash = g_hash_table_new (g_direct_hash, g_int_equal);

      n_colors = G_N_ELEMENTS (color_names);
      for (i = 0; i < n_colors; i++)
        {
          const char *interned = g_intern_string (color_names[i]);
          g_hash_table_insert (ctx->colors_hash, (gpointer)interned, GINT_TO_POINTER (i + 1));
        }
    }

  color_index_ptr = g_hash_table_lookup (ctx->colors_hash,
                                         g_intern_string (str));
  if (color_index_ptr)
    {
      /* Since we can't store 0 in the hash table without creating an ambiguity
       * when retrieving the value back the indices stored are all offset by
       * one. */
      int color_index = GPOINTER_TO_INT (color_index_ptr) - 1;
      cogl_color_init_from_4ub (color,
                               color_entries[color_index].red,
                               color_entries[color_index].green,
                               color_entries[color_index].blue,
                               255);
      return TRUE;
    }

  return FALSE;
}

void
rut_color_init_from_uint32 (CoglColor *color, uint32_t value)
{
  color->red = RUT_UINT32_RED_AS_FLOAT (value);
  color->green = RUT_UINT32_GREEN_AS_FLOAT (value);
  color->blue = RUT_UINT32_BLUE_AS_FLOAT (value);
  color->alpha = RUT_UINT32_ALPHA_AS_FLOAT (value);
}

void
rut_color_add (const CoglColor *a,
               const CoglColor *b,
               CoglColor *result)
{
  g_return_if_fail (a != NULL);
  g_return_if_fail (b != NULL);
  g_return_if_fail (result != NULL);

  result->red   = CLAMP (a->red   + b->red,   0, 255);
  result->green = CLAMP (a->green + b->green, 0, 255);
  result->blue  = CLAMP (a->blue  + b->blue,  0, 255);

  result->alpha = MAX (a->alpha, b->alpha);
}

void
rut_color_subtract (const CoglColor *a,
                    const CoglColor *b,
                    CoglColor *result)
{
  g_return_if_fail (a != NULL);
  g_return_if_fail (b != NULL);
  g_return_if_fail (result != NULL);

  result->red   = CLAMP (a->red   - b->red,   0, 255);
  result->green = CLAMP (a->green - b->green, 0, 255);
  result->blue  = CLAMP (a->blue  - b->blue,  0, 255);

  result->alpha = MIN (a->alpha, b->alpha);
}

void
rut_color_lighten (const CoglColor *color,
                   CoglColor *result)
{
  rut_color_shade (color, 1.3, result);
}

void
rut_color_darken (const CoglColor *color,
                  CoglColor *result)
{
  rut_color_shade (color, 0.7, result);
}

/**
 * rut_color_to_hls:
 * @color: a #CoglColor
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
rut_color_to_hls (const CoglColor *color,
                  float *hue,
                  float *luminance,
                  float *saturation)
{
  float red, green, blue;
  float min, max, delta;
  float h, l, s;

  g_return_if_fail (color != NULL);

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
rut_color_shade (const CoglColor *color,
                 float factor,
                 CoglColor *result)
{
  float h, l, s;

  g_return_if_fail (color != NULL);
  g_return_if_fail (result != NULL);

  rut_color_to_hls (color, &h, &l, &s);

  l *= factor;
  if (l > 1.0)
    l = 1.0;
  else if (l < 0)
    l = 0;

  s *= factor;
  if (s > 1.0)
    s = 1.0;
  else if (s < 0)
    s = 0;

  rut_color_init_from_hls (result, h, l, s);

  result->alpha = color->alpha;
}

gchar *
rut_color_to_string (const CoglColor *color)
{
  g_return_val_if_fail (color != NULL, NULL);

  return g_strdup_printf ("#%02x%02x%02x%02x",
                          (uint8_t)(color->red * 255.0),
                          (uint8_t)(color->green * 255.0),
                          (uint8_t)(color->blue * 255.0),
                          (uint8_t)(color->alpha * 255.0));
}

void
rut_color_interpolate (const CoglColor *initial,
                       const CoglColor *final,
                       float progress,
                       CoglColor *result)
{
  g_return_if_fail (initial != NULL);
  g_return_if_fail (final != NULL);
  g_return_if_fail (result != NULL);

  result->red   = initial->red   + (final->red   - initial->red)   * progress;
  result->green = initial->green + (final->green - initial->green) * progress;
  result->blue  = initial->blue  + (final->blue  - initial->blue)  * progress;
  result->alpha = initial->alpha + (final->alpha - initial->alpha) * progress;
}
