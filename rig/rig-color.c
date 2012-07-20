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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

#include <glib.h>

#include <cogl/cogl.h>

#include "color-table.h"
#include "rig-context.h"

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

static void
_rig_color_init_from_hls (CoglColor *color,
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

  cogl_color_init_from_4f (color, clr[0], clr[1], clr[2], 1.0);
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
   * clutter_color_from_hls() will do it for us
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

  _rig_color_init_from_hls (color, h, l, s);
  cogl_color_set_alpha (color, a);

  return TRUE;
}

gboolean
rig_util_parse_color (RigContext *ctx,
                      const char *str,
                      CoglColor *color)
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
