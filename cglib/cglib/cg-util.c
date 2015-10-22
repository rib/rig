/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#include <cglib-config.h>

#include <string.h>

#include <clib.h>

#include "cg-util.h"
#include "cg-private.h"

/*
 * cg_util_next_p2:
 * @a: Value to get the next power of two
 *
 * Calculates the next power of two greater than or equal to @a.
 *
 * Return value: @a if @a is already a power of two, otherwise returns
 *   the next nearest power of two.
 */
int
_cg_util_next_p2(int a)
{
    int rval = 1;

    while (rval < a)
        rval <<= 1;

    return rval;
}

unsigned int
_cg_util_one_at_a_time_mix(unsigned int hash)
{
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

/* The 'ffs' function is part of C99 so it isn't always available */
#ifndef CG_HAVE_FFS

int
_cg_util_ffs(int num)
{
    int i = 1;

    if (num == 0)
        return 0;

    while ((num & 1) == 0) {
        num >>= 1;
        i++;
    }

    return i;
}
#endif /* CG_HAVE_FFS */

/* The 'ffsl' is non-standard but when building with GCC we'll use its
   builtin instead */
#ifndef CG_HAVE_BUILTIN_FFSL

int
_cg_util_ffsl_wrapper(long int num)
{
    int i = 1;

    if (num == 0)
        return 0;

    while ((num & 1) == 0) {
        num >>= 1;
        i++;
    }

    return i;
}

#endif /* CG_HAVE_BUILTIN_FFSL */

#ifndef CG_HAVE_BUILTIN_POPCOUNTL

const unsigned char _cg_util_popcount_table[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5,
    3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

#endif /* CG_HAVE_BUILTIN_POPCOUNTL */

/* tests/conform/test-bitmask.c tests some cg internals and includes this
 * file directly but since these functions depend on other internal CGlib
 * symbols we hide them from test-bitmask.c
 *
 * XXX: maybe there's a better way for us to handle internal testing
 * to avoid needing hacks like this.
 */
#ifndef _CG_IN_TEST_BITMASK

/* Given a set of red, green and blue component masks, a depth and
 * bits per pixel this function tries to determine a corresponding
 * cg_pixel_format_t.
 *
 * The depth is measured in bits not including padding for un-used
 * alpha. The bits per pixel (bpp) does include padding for un-used
 * alpha.
 *
 * This function firstly aims to match formats with RGB ordered
 * components and only considers alpha coming first, in the most
 * significant bits. If the function fails to match then it recurses
 * by either switching the r and b masks around to check for BGR
 * ordered formats or it recurses with the masks shifted to check for
 * formats where the alpha component is the least significant bits.
 */
static cg_pixel_format_t
_cg_util_pixel_format_from_masks_real(unsigned long r_mask,
                                      unsigned long c_mask,
                                      unsigned long b_mask,
                                      int depth,
                                      int bpp,
                                      bool check_bgr,
                                      bool check_afirst,
                                      int recursion_depth)
{
    cg_pixel_format_t image_format;

    if (depth == 24 && bpp == 24 && r_mask == 0xff0000 && c_mask == 0xff00 &&
        b_mask == 0xff) {
        return CG_PIXEL_FORMAT_RGB_888;
    } else if ((depth == 24 || depth == 32) && bpp == 32 &&
               r_mask == 0xff0000 && c_mask == 0xff00 && b_mask == 0xff) {
        return CG_PIXEL_FORMAT_ARGB_8888_PRE;
    } else if ((depth == 30 || depth == 32) && r_mask == 0x3ff00000 &&
               c_mask == 0xffc00 && b_mask == 0x3ff) {
        return CG_PIXEL_FORMAT_ARGB_2101010_PRE;
    } else if (depth == 16 && bpp == 16 && r_mask == 0xf800 &&
               c_mask == 0x7e0 && b_mask == 0x1f) {
        return CG_PIXEL_FORMAT_RGB_565;
    }

    if (recursion_depth == 2)
        return 0;

    /* Check for BGR ordering if we didn't find a match */
    if (check_bgr) {
        image_format =
            _cg_util_pixel_format_from_masks_real(b_mask,
                                                  c_mask,
                                                  r_mask,
                                                  depth,
                                                  bpp,
                                                  false,
                                                  true,
                                                  recursion_depth + 1);
        if (image_format)
            return _cg_pixel_format_flip_rgb_order(image_format);
    }

    /* Check for alpha in the least significant bits if we still
     * haven't found a match... */
    if (check_afirst && depth != bpp) {
        int shift = bpp - depth;

        image_format =
            _cg_util_pixel_format_from_masks_real(r_mask >> shift,
                                                  c_mask >> shift,
                                                  b_mask >> shift,
                                                  depth,
                                                  bpp,
                                                  true,
                                                  false,
                                                  recursion_depth + 1);
        if (image_format)
            return _cg_pixel_format_flip_alpha_position(image_format);
    }

    return 0;
}

cg_pixel_format_t
_cg_util_pixel_format_from_masks(unsigned long r_mask,
                                 unsigned long c_mask,
                                 unsigned long b_mask,
                                 int depth,
                                 int bpp,
                                 bool byte_order_is_lsb_first)
{
    cg_pixel_format_t image_format = _cg_util_pixel_format_from_masks_real(
        r_mask, c_mask, b_mask, depth, bpp, true, true, 0);

    if (!image_format) {
        const char *byte_order[] = { "MSB first", "LSB first" };
        c_warning("Could not find a matching pixel format for red mask=0x%lx,"
                  "green mask=0x%lx, blue mask=0x%lx at depth=%d, bpp=%d "
                  "and byte order=%s\n",
                  r_mask,
                  c_mask,
                  b_mask,
                  depth,
                  bpp,
                  byte_order[!!byte_order_is_lsb_first]);
        return 0;
    }

    /* If the image is in little-endian then the order in memory is
       reversed */
    if (byte_order_is_lsb_first &&
        _cg_pixel_format_is_endian_dependant(image_format)) {
        image_format = _cg_pixel_format_flip_rgb_order(image_format);
        if (_cg_pixel_format_has_alpha(image_format))
            image_format = _cg_pixel_format_flip_alpha_position(image_format);
    }

    return image_format;
}

#endif /* _CG_IN_TEST_BITMASK */
