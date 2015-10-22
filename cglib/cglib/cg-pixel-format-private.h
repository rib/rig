/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2013 Intel Corporation.
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

#ifndef __CG_PIXEL_FORMAT_PRIVATE_H__
#define __CG_PIXEL_FORMAT_PRIVATE_H__

#include <clib.h>

#include "cg-types.h"
#include "cg-texture.h"

CG_BEGIN_DECLS

/*
 * _cg_pixel_format_get_bytes_per_pixel:
 * @format: a #cg_pixel_format_t
 *
 * Queries how many bytes a pixel of the given @format takes.
 *
 * Return value: The number of bytes taken for a pixel of the given
 *               @format.
 */
int _cg_pixel_format_get_bytes_per_pixel(cg_pixel_format_t format);

/*
 * _cg_pixel_format_has_aligned_components:
 * @format: a #cg_pixel_format_t
 *
 * Queries whether the ordering of the components for the given
 * @format depend on the endianness of the host CPU or if the
 * components can be accessed using bit shifting and bitmasking by
 * loading a whole pixel into a word.
 *
 * XXX: If we ever consider making something like this public we
 * should really try to think of a better name and come up with
 * much clearer documentation since it really depends on what
 * point of view you consider this from whether a format like
 * CG_PIXEL_FORMAT_RGBA_8888 is endian dependent. E.g. If you
 * read an RGBA_8888 pixel into a uint32
 * it's endian dependent how you mask out the different channels.
 * But If you already have separate color components and you want
 * to write them to an RGBA_8888 pixel then the bytes can be
 * written sequentially regardless of the endianness.
 *
 * Return value: %true if you need to consider the host CPU
 *               endianness when dealing with the given @format
 *               else %false.
 */
bool _cg_pixel_format_is_endian_dependant(cg_pixel_format_t format);

bool
_cg_pixel_format_is_premultiplied(cg_pixel_format_t format);

bool
_cg_pixel_format_has_alpha(cg_pixel_format_t format);

bool
_cg_pixel_format_has_depth(cg_pixel_format_t format);

/*
 * _cg_pixel_format_can_be_premultiplied:
 * @format: a #cg_pixel_format_t
 *
 * Returns true if the pixel format can be premultiplied. This is
 * currently true for all formats that have an alpha channel except
 * %CG_PIXEL_FORMAT_A_8 (because that doesn't have any other
 * components to multiply by the alpha).
 */
bool
_cg_pixel_format_can_be_premultiplied(cg_pixel_format_t format);

/* If the format is premultiplied, return the closest non-premultiplied format */
cg_pixel_format_t
_cg_pixel_format_premult_stem(cg_pixel_format_t format);

cg_pixel_format_t
_cg_pixel_format_premultiply(cg_pixel_format_t format);

cg_texture_components_t
_cg_pixel_format_get_components(cg_pixel_format_t format);

cg_pixel_format_t
_cg_pixel_format_toggle_premult_status(cg_pixel_format_t format);

cg_pixel_format_t
_cg_pixel_format_flip_rgb_order(cg_pixel_format_t format);

cg_pixel_format_t
_cg_pixel_format_flip_alpha_position(cg_pixel_format_t format);

CG_END_DECLS

#endif /* __CG_PIXEL_FORMAT_PRIVATE_H__ */
