/*
 * CGlib
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

/**
 * SECTION:cg-color
 * @short_description: A generic color definition
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_COLOR_H__
#define __CG_COLOR_H__

#include <cglib/cg-types.h>

CG_BEGIN_DECLS

/**
 * cg_color_init_from_4ub:
 * @color: A pointer to a #cg_color_t to initialize
 * @red: value of the red channel, between 0 and 255
 * @green: value of the green channel, between 0 and 255
 * @blue: value of the blue channel, between 0 and 255
 * @alpha: value of the alpha channel, between 0 and 255
 *
 * Sets the values of the passed channels into a #cg_color_t.
 *
 */
void cg_color_init_from_4ub(
    cg_color_t *color, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);

/**
 * cg_color_init_from_4f:
 * @color: A pointer to a #cg_color_t to initialize
 * @red: value of the red channel, between 0 and 1.0
 * @green: value of the green channel, between 0 and 1.0
 * @blue: value of the blue channel, between 0 and 1.0
 * @alpha: value of the alpha channel, between 0 and 1.0
 *
 * Sets the values of the passed channels into a #cg_color_t
 *
 */
void cg_color_init_from_4f(
    cg_color_t *color, float red, float green, float blue, float alpha);

/**
 * cg_color_init_from_4fv:
 * @color: A pointer to a #cg_color_t to initialize
 * @color_array: a pointer to an array of 4 float color components
 *
 * Sets the values of the passed channels into a #cg_color_t
 *
 */
void cg_color_init_from_4fv(cg_color_t *color, const float *color_array);

/**
 * cg_color_get_red_byte:
 * @color: a #cg_color_t
 *
 * Retrieves the red channel of @color as a byte value
 * between 0 and 255
 *
 * Return value: the red channel of the passed color
 *
 */
uint8_t cg_color_get_red_byte(const cg_color_t *color);

/**
 * cg_color_get_green_byte:
 * @color: a #cg_color_t
 *
 * Retrieves the green channel of @color as a byte value
 * between 0 and 255
 *
 * Return value: the green channel of the passed color
 *
 */
uint8_t cg_color_get_green_byte(const cg_color_t *color);

/**
 * cg_color_get_blue_byte:
 * @color: a #cg_color_t
 *
 * Retrieves the blue channel of @color as a byte value
 * between 0 and 255
 *
 * Return value: the blue channel of the passed color
 *
 */
uint8_t cg_color_get_blue_byte(const cg_color_t *color);

/**
 * cg_color_get_alpha_byte:
 * @color: a #cg_color_t
 *
 * Retrieves the alpha channel of @color as a byte value
 * between 0 and 255
 *
 * Return value: the alpha channel of the passed color
 *
 */
uint8_t cg_color_get_alpha_byte(const cg_color_t *color);

/**
 * cg_color_set_red_byte:
 * @color: a #cg_color_t
 * @red: a byte value between 0 and 255
 *
 * Sets the red channel of @color to @red.
 *
 */
void cg_color_set_red_byte(cg_color_t *color, uint8_t red);

/**
 * cg_color_set_green_byte:
 * @color: a #cg_color_t
 * @green: a byte value between 0 and 255
 *
 * Sets the green channel of @color to @green.
 *
 */
void cg_color_set_green_byte(cg_color_t *color, uint8_t green);

/**
 * cg_color_set_blue_byte:
 * @color: a #cg_color_t
 * @blue: a byte value between 0 and 255
 *
 * Sets the blue channel of @color to @blue.
 *
 */
void cg_color_set_blue_byte(cg_color_t *color, uint8_t blue);

/**
 * cg_color_set_alpha_byte:
 * @color: a #cg_color_t
 * @alpha: a byte value between 0 and 255
 *
 * Sets the alpha channel of @color to @alpha.
 *
 */
void cg_color_set_alpha_byte(cg_color_t *color, uint8_t alpha);

/**
 * cg_color_premultiply:
 * @color: the color to premultiply
 *
 * Converts a non-premultiplied color to a pre-multiplied color. For
 * example, semi-transparent red is (1.0, 0, 0, 0.5) when non-premultiplied
 * and (0.5, 0, 0, 0.5) when premultiplied.
 *
 */
void cg_color_premultiply(cg_color_t *color);

/**
 * cg_color_unpremultiply:
 * @color: the color to unpremultiply
 *
 * Converts a pre-multiplied color to a non-premultiplied color. For
 * example, semi-transparent red is (0.5, 0, 0, 0.5) when premultiplied
 * and (1.0, 0, 0, 0.5) when non-premultiplied.
 *
 */
void cg_color_unpremultiply(cg_color_t *color);

/**
 * cg_color_equal:
 * @v1: a #cg_color_t
 * @v2: a #cg_color_t
 *
 * Compares two #cg_color_t<!-- -->s and checks if they are the same.
 *
 * This function can be passed to c_hash_table_new() as the @key_equal_func
 * parameter, when using #cg_color_t<!-- -->s as keys in a #c_hash_table_t.
 *
 * Return value: %true if the two colors are the same.
 *
 */
bool cg_color_equal(const void *v1, const void *v2);

/**
 * cg_color_copy:
 * @color: the color to copy
 *
 * Creates a copy of @color
 *
 * Return value: a newly-allocated #cg_color_t. Use cg_color_free()
 *   to free the allocate resources
 *
 */
cg_color_t *cg_color_copy(const cg_color_t *color);

/**
 * cg_color_free:
 * @color: the color to free
 *
 * Frees the resources allocated by cg_color_copy().
 *
 */
void cg_color_free(cg_color_t *color);

/**
 * cg_color_to_hsl:
 * @color: a #cg_color_t
 * @hue: (out): return location for the hue value or %NULL
 * @saturation: (out): return location for the saturation value or %NULL
 * @luminance: (out): return location for the luminance value or %NULL
 *
 * Converts @color to the HLS format.
 *
 * The @hue value is in the 0 .. 360 range. The @luminance and
 * @saturation values are in the 0 .. 1 range.
 *
 */
void cg_color_to_hsl(const cg_color_t *color,
                     float *hue,
                     float *saturation,
                     float *luminance);

/**
 * cg_color_init_from_hsl:
 * @color: (out): return location for a #cg_color_t
 * @hue: hue value, in the 0 .. 360 range
 * @saturation: saturation value, in the 0 .. 1 range
 * @luminance: luminance value, in the 0 .. 1 range
 *
 * Converts a color expressed in HLS (hue, luminance and saturation)
 * values into a #cg_color_t.
 *
 */
void cg_color_init_from_hsl(cg_color_t *color,
                            float hue,
                            float saturation,
                            float luminance);

CG_END_DECLS

#endif /* __CG_COLOR_H__ */
