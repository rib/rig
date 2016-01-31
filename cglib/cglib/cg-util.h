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

#ifndef __CG_UTIL_H
#define __CG_UTIL_H

#include <clib.h>
#include <math.h>

#include <cglib/cg-defines.h>
#include "cg-types.h"

#ifndef CG_HAS_GLIB_SUPPORT
#include <stdio.h>
#endif

/* Double check that clib-config.h has been included */
#if !defined(_CG_CONFIG_H) && !defined(_CG_IN_TEST_BITMASK)
#error "cglib-config.h must be included before including cg-util.h"
#endif

int _cg_util_next_p2(int a);

/* The signbit macro is defined by ISO C99 so it should be available,
   however if it's not we can fallback to an evil hack */
#ifdef signbit
#define cg_util_float_signbit(x) signbit(x)
#else
/* This trick was stolen from here:
   http://lists.boost.org/Archives/boost/2006/08/108731.php

   It xors the integer reinterpretations of -1.0f and 1.0f. In theory
   they should only differ by the signbit so that gives a mask for the
   sign which we can just test against the value */
static inline bool
cg_util_float_signbit(float x)
{
    static const union {
        float f;
        uint32_t i;
    } negative_one = { -1.0f };
    static const union {
        float f;
        uint32_t i;
    } positive_one = { +1.0f };
    union {
        float f;
        uint32_t i;
    } value = { x };

    return !!((negative_one.i ^ positive_one.i) & value.i);
}
#endif

/* Returns whether the given integer is a power of two */
static inline bool
_cg_util_is_pot(unsigned int num)
{
    /* Make sure there is only one bit set */
    return (num & (num - 1)) == 0;
}

/* Split Bob Jenkins' One-at-a-Time hash
 *
 * This uses the One-at-a-Time hash algorithm designed by Bob Jenkins
 * but the mixing step is split out so the function can be used in a
 * more incremental fashion.
 */
static inline unsigned int
_cg_util_one_at_a_time_hash(unsigned int hash, const void *key, size_t bytes)
{
    const unsigned char *p = key;

    for (unsigned int i = 0; i < bytes; i++) {
        hash += p[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    return hash;
}

unsigned int _cg_util_one_at_a_time_mix(unsigned int hash);

/* The 'ffs' function is part of C99 so it isn't always available */
#ifdef CG_HAVE_FFS
#define _cg_util_ffs ffs
#else
int _cg_util_ffs(int num);
#endif

/* The 'ffsl' function is non-standard but GCC has a builtin for it
   since 3.4 which we can use */
#ifdef CG_HAVE_BUILTIN_FFSL
#define _cg_util_ffsl __builtin_ffsl
#else
/* If ints and longs are the same size we can just use ffs. Hopefully
   the compiler will optimise away this conditional */
#define _cg_util_ffsl(x)                                                       \
    (sizeof(long int) == sizeof(int) ? _cg_util_ffs((int)x)                    \
     : _cg_util_ffsl_wrapper(x))
int _cg_util_ffsl_wrapper(long int num);
#endif /* CG_HAVE_BUILTIN_FFSL */

static inline unsigned int
_cg_util_fls(unsigned int n)
{
#ifdef CG_HAVE_BUILTIN_CLZ
    return n == 0 ? 0 : sizeof(unsigned int) * 8 - __builtin_clz(n);
#else
    unsigned int v = 1;

    if (n == 0)
        return 0;

    while (n >>= 1)
        v++;

    return v;
#endif
}

#ifdef CG_HAVE_BUILTIN_POPCOUNTL
#define _cg_util_popcountl __builtin_popcountl
#else
extern const unsigned char _cg_util_popcount_table[256];

/* There are many ways of doing popcount but doing a table lookup
   seems to be the most robust against different sizes for long. Some
   pages seem to claim it's the fastest method anyway. */
static inline int
_cg_util_popcountl(unsigned long num)
{
    int i;
    int sum = 0;

    /* Let's hope GCC will unroll this loop.. */
    for (i = 0; i < sizeof(num); i++)
        sum += _cg_util_popcount_table[(num >> (i * 8)) & 0xff];

    return sum;
}

#endif /* CG_HAVE_BUILTIN_POPCOUNTL */

/* Match a cg_pixel_format_t according to channel masks, color depth,
 * bits per pixel and byte order. These information are provided by
 * the Visual and XImage structures.
 *
 * If no specific pixel format could be found, CG_PIXEL_FORMAT_ANY
 * is returned.
 */
cg_pixel_format_t
_cg_util_pixel_format_from_masks(unsigned long r_mask,
                                 unsigned long c_mask,
                                 unsigned long b_mask,
                                 int depth,
                                 int bpp,
                                 bool byte_order_is_lsb_first);

static inline void
_cg_util_scissor_intersect(int rect_x0,
                           int rect_y0,
                           int rect_x1,
                           int rect_y1,
                           int *scissor_x0,
                           int *scissor_y0,
                           int *scissor_x1,
                           int *scissor_y1)
{
    *scissor_x0 = MAX(*scissor_x0, rect_x0);
    *scissor_y0 = MAX(*scissor_y0, rect_y0);
    *scissor_x1 = MIN(*scissor_x1, rect_x1);
    *scissor_y1 = MIN(*scissor_y1, rect_y1);
}

#endif /* __CG_UTIL_H */
