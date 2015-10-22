/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2014 Intel Corporation.
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

#include "cg-private.h"
#include "cg-bitmap-private.h"
#include "cg-device-private.h"
#include "cg-texture-private.h"
#include "cg-error-private.h"

#include <string.h>


/* These are generalized descriptions of component mappings
 * so that we have less macros to re-define for the variations
 * we want.
 */

#define X_FROM_1_BIT(b)   X_FROM_NORMALIZED_RANGE(b, 1)
#define X_FROM_2_BITS(b)  X_FROM_NORMALIZED_RANGE(b, 3)
#define X_FROM_4_BITS(b)  X_FROM_NORMALIZED_RANGE(b, 15)
#define X_FROM_UN8(b)     X_FROM_NORMALIZED_RANGE(b, 255)

/* XXX: Note: The maximum values for 5/6/10 bits don't factor as
 * nicely so to round-to-nearest we + 0.5 before dividing */
#define X_FROM_5_BITS(b)  X_FROM_NORMALIZED_RANGE_NEAREST(b, 31)
#define X_FROM_6_BITS(b)  X_FROM_NORMALIZED_RANGE_NEAREST(b, 63)
#define X_FROM_10_BITS(b) X_FROM_NORMALIZED_RANGE_NEAREST(b, 1023)

#define X_TO_1_BIT(x)   X_TO_NORMALIZED_RANGE(x, 1)
#define X_TO_2_BITS(x)  X_TO_NORMALIZED_RANGE(x, 3)
#define X_TO_4_BITS(x)  X_TO_NORMALIZED_RANGE(x, 15)
#define X_TO_5_BITS(x)  X_TO_NORMALIZED_RANGE(x, 31)
#define X_TO_6_BITS(x)  X_TO_NORMALIZED_RANGE(x, 63)
#define X_TO_UN8(x)     X_TO_NORMALIZED_RANGE(x, 255)
#define X_TO_10_BITS(x) ((uint16_t)(X_TO_FLOAT(x) * 1023))



/*
 * unsigned 8 bit, normalized mappings
 */
#define X_FROM_NORMALIZED_RANGE(b, max)         ((b) * (255 / max))
#define X_FROM_NORMALIZED_RANGE_NEAREST(b, max) ((b) * ((uint16_t)255 + (max / 2)) / max)

#define X_FROM_SN8(s)     ((int8_t)(s) <= 0 ? 0 : (uint8_t)(((int8_t)(s)) * (255.0f/127.0f) + 0.5))
#define X_FROM_S16(s)     ((s) >= 1 ? 255 : 0)
#define X_FROM_U16(u)     ((u) >= 1 ? 255 : 0)
#define X_FROM_U32(u)     ((u) >= 1 ? 255 : 0)
#define X_FROM_S32(s)     ((s) >= 1 ? 255 : 0)
#define X_FROM_FLOAT(f)   CLAMP((((f) * 255.0f) + 0.5f), 0, 255)

#define X_TO_NORMALIZED_RANGE(x, max) \
    ((((uint16_t)(x)) + ((255 / max) / 2)) / (255 / max))
#define X_TO_SN8(x)     ((int8_t)(((x) * (127.0f/255.0f)) + 0.5))
#define X_TO_U16(x)     ((x) < (255 / 2) ? 0 : 1)
#define X_TO_S16(x)     ((x) < (255 / 2) ? 0 : 1)
#define X_TO_U32(x)     ((x) < (255 / 2) ? 0 : 1)
#define X_TO_S32(x)     ((x) < (255 / 2) ? 0 : 1)
#define X_TO_FLOAT(x)   ((x) / 255.0f)

#define X_ONE 255

#define COMPONENT_UNSIGNED
#define component_type  uint8_t
#define component_size  8
#include "cg-bitmap-unpack-unsigned-normalized.h"
#include "cg-bitmap-pack.h"
#undef component_size
#undef component_type
#undef COMPONENT_UNSIGNED

#undef X_ONE

#undef X_TO_NORMALIZED_RANGE
#undef X_TO_SN8
#undef X_TO_U16
#undef X_TO_S16
#undef X_TO_U32
#undef X_TO_S32
#undef X_TO_FLOAT

#undef X_FROM_SN8
#undef X_FROM_U16
#undef X_FROM_S16
#undef X_FROM_U32
#undef X_FROM_S32
#undef X_FROM_FLOAT

#undef X_FROM_NORMALIZED_RANGE
#undef X_FROM_NORMALIZED_RANGE_NEAREST



/*
 * double precision, floating point, un-normalized mappings
 */

#define X_FROM_NORMALIZED_RANGE(u, max)         ((u) * (1.0f / max))
#define X_FROM_NORMALIZED_RANGE_NEAREST(u, max) ((u) * (1.0f / max))

/* Equation taken from GL spec... */
#define X_FROM_SN8(s)     MAX(((int8_t)(s)) / 127.0, -1.0)

#define X_FROM_U16(u)     (u)
#define X_FROM_S16(s)     (s)
#define X_FROM_U32(u)     (u)
#define X_FROM_S32(s)     (s)
#define X_FROM_FLOAT(f)   (f)

/* XXX: Note the GL spec doesn't round to nearest, maybe we shouldn't either? */
#define X_TO_NORMALIZED_RANGE(x, max)   (uint16_t)((CLAMP(x, 0, 1) * (double)max) + 0.5)
#define X_TO_SN8(x)                     ((int8_t)c_nearbyint(CLAMP(x, -1, 1) * 127.0))
#define X_TO_U16(x)                     ((uint16_t)((x) + 0.5))
#define X_TO_S16(x)                     ((uint16_t)(c_nearbyint(x)))
#define X_TO_U32(x)                     ((uint32_t)((x) + 0.5))
#define X_TO_S32(x)                     ((uint32_t)(c_nearbyint(x)))
#define X_TO_FLOAT(x)                   (x)

#define X_ONE 1.0

#define COMPONENT_SIGNED
#define component_type  double
#define component_size  64
#include "cg-bitmap-unpack-fallback.h"
#include "cg-bitmap-unpack-unsigned-normalized.h"
#include "cg-bitmap-pack.h"
#undef component_type
#undef component_size
#undef COMPONENT_SIGNED


/* XXX: How should we handle signed int components and half-float
 * components? */

/* (Un)Premultiplication */

inline static void
_cg_unpremult_alpha_0(uint8_t *dst)
{
    dst[0] = 0;
    dst[1] = 0;
    dst[2] = 0;
    dst[3] = 0;
}

inline static void
_cg_unpremult_alpha_last(uint8_t *dst)
{
    uint8_t alpha = dst[3];

    dst[0] = (dst[0] * 255) / alpha;
    dst[1] = (dst[1] * 255) / alpha;
    dst[2] = (dst[2] * 255) / alpha;
}

inline static void
_cg_unpremult_alpha_first(uint8_t *dst)
{
    uint8_t alpha = dst[0];

    dst[1] = (dst[1] * 255) / alpha;
    dst[2] = (dst[2] * 255) / alpha;
    dst[3] = (dst[3] * 255) / alpha;
}

/* No division form of floor((c*a + 128)/255) (I first encountered
 * this in the RENDER implementation in the X server.) Being exact
 * is important for a == 255 - we want to get exactly c.
 */
#define MULT(d, a, t)                                                          \
    C_STMT_START                                                               \
    {                                                                          \
        t = d * a + 128;                                                       \
        d = ((t >> 8) + t) >> 8;                                               \
    }                                                                          \
    C_STMT_END

inline static void
_cg_premult_alpha_last(uint8_t *dst)
{
    uint8_t alpha = dst[3];
    /* Using a separate temporary per component has given slightly better
     * code generation with GCC in the past; it shouldn't do any worse in
     * any case.
     */
    unsigned int t1, t2, t3;
    MULT(dst[0], alpha, t1);
    MULT(dst[1], alpha, t2);
    MULT(dst[2], alpha, t3);
}

inline static void
_cg_premult_alpha_first(uint8_t *dst)
{
    uint8_t alpha = dst[0];
    unsigned int t1, t2, t3;

    MULT(dst[1], alpha, t1);
    MULT(dst[2], alpha, t2);
    MULT(dst[3], alpha, t3);
}

#undef MULT

/* Use the SSE optimized version to premult four pixels at once when
   it is available. The same assembler code works for x86 and x86-64
   because it doesn't refer to any non-SSE registers directly */
#if defined(__SSE2__) && defined(__GNUC__) &&                                  \
    (defined(__x86_64) || defined(__i386))
#define CG_USE_PREMULT_SSE2
#endif

#ifdef CG_USE_PREMULT_SSE2

inline static void
_cg_premult_alpha_last_four_pixels_sse2(uint8_t *p)
{
    /* 8 copies of 128 used below */
    static const int16_t eight_halves[8] __attribute__((aligned(16))) = { 128, 128, 128, 128, 128, 128, 128, 128 };
    /* Mask of the rgb components of the four pixels */
    static const int8_t just_rgb[16]
    __attribute__((aligned(16))) = { 0xff, 0xff, 0xff, 0x00, 0xff, 0xff,
                                     0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
                                     0xff, 0xff, 0xff, 0x00 };
    /* Each SSE register only holds two pixels because we need to work
       with 16-bit intermediate values. We still do four pixels by
       interleaving two registers in the hope that it will pipeline
       better */
    __asm__( /* Load eight_halves into xmm5 for later */
            "movdqa (%1), %%xmm5\n"
            /* Clear xmm3 */
            "pxor %%xmm3, %%xmm3\n"
            /* Load two pixels from p into the low half of xmm0 */
            "movlps (%0), %%xmm0\n"
            /* Load the next set of two pixels from p into the low half of xmm1 */
            "movlps 8(%0), %%xmm1\n"
            /* Unpack 8 bytes from the low quad-words in each register to 8
               16-bit values */
            "punpcklbw %%xmm3, %%xmm0\n"
            "punpcklbw %%xmm3, %%xmm1\n"
            /* Copy alpha values of the first pixel in xmm0 to all
               components of the first pixel in xmm2 */
            "pshuflw $255, %%xmm0, %%xmm2\n"
            /* same for xmm1 and xmm3 */
            "pshuflw $255, %%xmm1, %%xmm3\n"
            /* The above also copies the second pixel directly so we now
               want to replace the RGB components with copies of the alpha
               components */
            "pshufhw $255, %%xmm2, %%xmm2\n"
            "pshufhw $255, %%xmm3, %%xmm3\n"
            /* Multiply the rgb components by the alpha */
            "pmullw %%xmm2, %%xmm0\n"
            "pmullw %%xmm3, %%xmm1\n"
            /* Add 128 to each component */
            "paddw %%xmm5, %%xmm0\n"
            "paddw %%xmm5, %%xmm1\n"
            /* Copy the results to temporary registers xmm4 and xmm5 */
            "movdqa %%xmm0, %%xmm4\n"
            "movdqa %%xmm1, %%xmm5\n"
            /* Divide the results by 256 */
            "psrlw $8, %%xmm0\n"
            "psrlw $8, %%xmm1\n"
            /* Add the temporaries back in */
            "paddw %%xmm4, %%xmm0\n"
            "paddw %%xmm5, %%xmm1\n"
            /* Divide again */
            "psrlw $8, %%xmm0\n"
            "psrlw $8, %%xmm1\n"
            /* Pack the results back as bytes */
            "packuswb %%xmm1, %%xmm0\n"
            /* Load just_rgb into xmm3 for later */
            "movdqa (%2), %%xmm3\n"
            /* Reload all four pixels into xmm2 */
            "movups (%0), %%xmm2\n"
            /* Mask out the alpha from the results */
            "andps %%xmm3, %%xmm0\n"
            /* Mask out the RGB from the original four pixels */
            "andnps %%xmm2, %%xmm3\n"
            /* Combine the two to get the right alpha values */
            "orps %%xmm3, %%xmm0\n"
            /* Write to memory */
            "movdqu %%xmm0, (%0)\n"
            : /* no outputs */
            : "r" (p), "r" (eight_halves), "r" (just_rgb)
            : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}

#endif /* CG_USE_PREMULT_SSE2 */

static void
_cg_bitmap_premult_unpacked_span_8(uint8_t *data, int width)
{
#ifdef CG_USE_PREMULT_SSE2

    /* Process 4 pixels at a time */
    while (width >= 4) {
        _cg_premult_alpha_last_four_pixels_sse2(data);
        data += 4 * 4;
        width -= 4;
    }

/* If there are any pixels left we will fall through and
   handle them below */

#endif /* CG_USE_PREMULT_SSE2 */

    while (width-- > 0) {
        _cg_premult_alpha_last(data);
        data += 4;
    }
}

static void
_cg_bitmap_unpremult_unpacked_span_8(uint8_t *data, int width)
{
    int x;

    for (x = 0; x < width; x++) {
        if (data[3] == 0)
            _cg_unpremult_alpha_0(data);
        else
            _cg_unpremult_alpha_last(data);
        data += 4;
    }
}

static void
_cg_bitmap_premult_unpacked_span_64f(double *data, int width)
{
    while (width-- > 0) {
        double alpha = data[3];

        data[0] *= alpha;
        data[1] *= alpha;
        data[2] *= alpha;
        data += 4;
    }
}

static void
_cg_bitmap_unpremult_unpacked_span_64f(double *data, int width)
{
    while (width-- > 0) {
        double alpha = data[3];

        if (alpha == 0)
            memset(data, 0, sizeof(double) * 3);
        else {
            data[0] /= alpha;
            data[1] /= alpha;
            data[2] /= alpha;
        }
        data += 4;
    }
}

static bool
_cg_bitmap_can_fast_premult(cg_pixel_format_t format)
{
    switch (_cg_pixel_format_premult_stem(format)) {
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ABGR_8888:
        return true;

    default:
        return false;
    }
}

enum tmp_fmt_t {
    _TMP_FMT_NONE,
    _TMP_FMT_8,
    _TMP_FMT_DOUBLE
};

static enum tmp_fmt_t
get_tmp_fmt(cg_pixel_format_t format)
{
    /* If the format is using more than 8 bits per component or isn't
     * normalized [0,1] then we'll unpack into a double per component
     * buffer instead so we won't lose precision. */

    switch (format) {
    case CG_PIXEL_FORMAT_DEPTH_16:
    case CG_PIXEL_FORMAT_DEPTH_32:
    case CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
    case CG_PIXEL_FORMAT_ANY:
        c_assert_not_reached();
        return _TMP_FMT_NONE;

    case CG_PIXEL_FORMAT_A_8:
    case CG_PIXEL_FORMAT_RG_88:
    case CG_PIXEL_FORMAT_RGB_565:
    case CG_PIXEL_FORMAT_RGBA_4444:
    case CG_PIXEL_FORMAT_RGBA_5551:
    case CG_PIXEL_FORMAT_RGB_888:
    case CG_PIXEL_FORMAT_BGR_888:
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ABGR_8888:
    case CG_PIXEL_FORMAT_RGBA_8888_PRE:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:
    case CG_PIXEL_FORMAT_ARGB_8888_PRE:
    case CG_PIXEL_FORMAT_ABGR_8888_PRE:
    case CG_PIXEL_FORMAT_RGBA_4444_PRE:
    case CG_PIXEL_FORMAT_RGBA_5551_PRE:
        return _TMP_FMT_8;

    case CG_PIXEL_FORMAT_A_8SN:
    case CG_PIXEL_FORMAT_A_16U:
    case CG_PIXEL_FORMAT_A_16F:
    case CG_PIXEL_FORMAT_A_32U:
    case CG_PIXEL_FORMAT_A_32F:
    case CG_PIXEL_FORMAT_RG_88SN:
    case CG_PIXEL_FORMAT_RG_1616U:
    case CG_PIXEL_FORMAT_RG_1616F:
    case CG_PIXEL_FORMAT_RG_3232U:
    case CG_PIXEL_FORMAT_RG_3232F:
    case CG_PIXEL_FORMAT_RGB_888SN:
    case CG_PIXEL_FORMAT_BGR_888SN:
    case CG_PIXEL_FORMAT_RGB_161616U:
    case CG_PIXEL_FORMAT_BGR_161616U:
    case CG_PIXEL_FORMAT_RGB_161616F:
    case CG_PIXEL_FORMAT_BGR_161616F:
    case CG_PIXEL_FORMAT_RGB_323232U:
    case CG_PIXEL_FORMAT_BGR_323232U:
    case CG_PIXEL_FORMAT_RGB_323232F:
    case CG_PIXEL_FORMAT_BGR_323232F:
    case CG_PIXEL_FORMAT_RGBA_8888SN:
    case CG_PIXEL_FORMAT_BGRA_8888SN:
    case CG_PIXEL_FORMAT_RGBA_1010102:
    case CG_PIXEL_FORMAT_BGRA_1010102:
    case CG_PIXEL_FORMAT_ARGB_2101010:
    case CG_PIXEL_FORMAT_ABGR_2101010:
    case CG_PIXEL_FORMAT_RGBA_1010102_PRE:
    case CG_PIXEL_FORMAT_BGRA_1010102_PRE:
    case CG_PIXEL_FORMAT_ARGB_2101010_PRE:
    case CG_PIXEL_FORMAT_ABGR_2101010_PRE:
    case CG_PIXEL_FORMAT_RGBA_16161616U:
    case CG_PIXEL_FORMAT_BGRA_16161616U:
    case CG_PIXEL_FORMAT_RGBA_16161616F:
    case CG_PIXEL_FORMAT_BGRA_16161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F_PRE:
    case CG_PIXEL_FORMAT_BGRA_16161616F_PRE:
    case CG_PIXEL_FORMAT_RGBA_32323232U:
    case CG_PIXEL_FORMAT_BGRA_32323232U:
    case CG_PIXEL_FORMAT_RGBA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F:
    case CG_PIXEL_FORMAT_RGBA_32323232F_PRE:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:
        return _TMP_FMT_DOUBLE;
    }

    c_assert_not_reached();
    return _TMP_FMT_NONE;
}

static bool
uses_half_floats(cg_pixel_format_t format)
{
    switch (format) {
    case CG_PIXEL_FORMAT_A_16F:
    case CG_PIXEL_FORMAT_RG_1616F:
    case CG_PIXEL_FORMAT_RGB_161616F:
    case CG_PIXEL_FORMAT_BGR_161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F:
    case CG_PIXEL_FORMAT_BGRA_16161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F_PRE:
    case CG_PIXEL_FORMAT_BGRA_16161616F_PRE:
        return true;

    case CG_PIXEL_FORMAT_A_8:
    case CG_PIXEL_FORMAT_A_8SN:
    case CG_PIXEL_FORMAT_A_16U:
    case CG_PIXEL_FORMAT_A_32U:
    case CG_PIXEL_FORMAT_A_32F:
    case CG_PIXEL_FORMAT_RG_88:
    case CG_PIXEL_FORMAT_RG_88SN:
    case CG_PIXEL_FORMAT_RG_1616U:
    case CG_PIXEL_FORMAT_RG_3232U:
    case CG_PIXEL_FORMAT_RG_3232F:
    case CG_PIXEL_FORMAT_RGB_565:
    case CG_PIXEL_FORMAT_RGB_888:
    case CG_PIXEL_FORMAT_BGR_888:
    case CG_PIXEL_FORMAT_RGB_888SN:
    case CG_PIXEL_FORMAT_BGR_888SN:
    case CG_PIXEL_FORMAT_RGB_161616U:
    case CG_PIXEL_FORMAT_BGR_161616U:
    case CG_PIXEL_FORMAT_RGB_323232U:
    case CG_PIXEL_FORMAT_BGR_323232U:
    case CG_PIXEL_FORMAT_RGB_323232F:
    case CG_PIXEL_FORMAT_BGR_323232F:
    case CG_PIXEL_FORMAT_RGBA_8888SN:
    case CG_PIXEL_FORMAT_BGRA_8888SN:
    case CG_PIXEL_FORMAT_RGBA_4444:
    case CG_PIXEL_FORMAT_RGBA_4444_PRE:
    case CG_PIXEL_FORMAT_RGBA_5551:
    case CG_PIXEL_FORMAT_RGBA_5551_PRE:
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ABGR_8888:
    case CG_PIXEL_FORMAT_RGBA_8888_PRE:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:
    case CG_PIXEL_FORMAT_ARGB_8888_PRE:
    case CG_PIXEL_FORMAT_ABGR_8888_PRE:
    case CG_PIXEL_FORMAT_RGBA_1010102:
    case CG_PIXEL_FORMAT_BGRA_1010102:
    case CG_PIXEL_FORMAT_ARGB_2101010:
    case CG_PIXEL_FORMAT_ABGR_2101010:
    case CG_PIXEL_FORMAT_RGBA_1010102_PRE:
    case CG_PIXEL_FORMAT_BGRA_1010102_PRE:
    case CG_PIXEL_FORMAT_ARGB_2101010_PRE:
    case CG_PIXEL_FORMAT_ABGR_2101010_PRE:
    case CG_PIXEL_FORMAT_RGBA_16161616U:
    case CG_PIXEL_FORMAT_BGRA_16161616U:
    case CG_PIXEL_FORMAT_RGBA_32323232U:
    case CG_PIXEL_FORMAT_BGRA_32323232U:
    case CG_PIXEL_FORMAT_RGBA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F:
    case CG_PIXEL_FORMAT_RGBA_32323232F_PRE:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:
    case CG_PIXEL_FORMAT_DEPTH_16:
    case CG_PIXEL_FORMAT_DEPTH_32:
    case CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
    case CG_PIXEL_FORMAT_ANY:
        return false;
    }

    c_assert_not_reached();
    return false;
}
static bool
involves_half_floats(cg_pixel_format_t src_format,
                     cg_pixel_format_t dst_format)
{
    return uses_half_floats(src_format) || uses_half_floats(dst_format);
}

bool
_cg_bitmap_convert_into_bitmap(cg_bitmap_t *src_bmp,
                               cg_bitmap_t *dst_bmp,
                               cg_error_t **error)
{
    uint8_t *src_data;
    uint8_t *dst_data;
    uint8_t *src;
    uint8_t *dst;
    void *tmp_row;
    int src_rowstride;
    int dst_rowstride;
    int y;
    int width, height;
    cg_pixel_format_t src_format;
    cg_pixel_format_t dst_format;
    bool need_multiply;
    bool ret = true;

    src_format = cg_bitmap_get_format(src_bmp);
    src_rowstride = cg_bitmap_get_rowstride(src_bmp);
    dst_format = cg_bitmap_get_format(dst_bmp);
    dst_rowstride = cg_bitmap_get_rowstride(dst_bmp);
    width = cg_bitmap_get_width(src_bmp);
    height = cg_bitmap_get_height(src_bmp);

    c_return_val_if_fail(width == cg_bitmap_get_width(dst_bmp), false);
    c_return_val_if_fail(height == cg_bitmap_get_height(dst_bmp), false);

    need_multiply =
        (_cg_pixel_format_has_alpha(src_format) &&
         _cg_pixel_format_has_alpha(dst_format) &&
         src_format != CG_PIXEL_FORMAT_A_8 &&
         dst_format != CG_PIXEL_FORMAT_A_8 &&
         (_cg_pixel_format_is_premultiplied(src_format) !=
          _cg_pixel_format_is_premultiplied(dst_format)));

    /* If the base format is the same then we can just copy the bitmap
       instead */
    if (_cg_pixel_format_premult_stem(src_format) == _cg_pixel_format_premult_stem(dst_format) &&
        (!need_multiply || _cg_bitmap_can_fast_premult(dst_format))) {
        if (!_cg_bitmap_copy_subregion(src_bmp,
                                       dst_bmp,
                                       0,
                                       0, /* src_x / src_y */
                                       0,
                                       0, /* dst_x / dst_y */
                                       width,
                                       height,
                                       error))
            return false;

        if (need_multiply) {
            if (_cg_pixel_format_is_premultiplied(dst_format)) {
                if (!_cg_bitmap_premult(dst_bmp, error))
                    return false;
            } else {
                if (!_cg_bitmap_unpremult(dst_bmp, error))
                    return false;
            }
        }

        return true;
    }

    if (involves_half_floats(src_format, dst_format)) {
        _cg_set_error(error,
                      CG_SYSTEM_ERROR,
                      CG_SYSTEM_ERROR_UNSUPPORTED,
                      "Failed to convert to/from half-float format");
        return false;
    }

    src_data = _cg_bitmap_map(src_bmp, CG_BUFFER_ACCESS_READ, 0, error);
    if (src_data == NULL)
        return false;
    dst_data = _cg_bitmap_map(
        dst_bmp, CG_BUFFER_ACCESS_WRITE, CG_BUFFER_MAP_HINT_DISCARD, error);
    if (dst_data == NULL) {
        _cg_bitmap_unmap(src_bmp);
        return false;
    }

    switch (get_tmp_fmt(dst_format))
    {
    case _TMP_FMT_8:
        tmp_row = c_malloc(width * 4);
        for (y = 0; y < height; y++) {
            src = src_data + y * src_rowstride;
            dst = dst_data + y * dst_rowstride;

            _cg_unpack_8(src_format, src, tmp_row, width);

            /* Handle premultiplication */
            if (need_multiply) {
                if (_cg_pixel_format_is_premultiplied(dst_format))
                    _cg_bitmap_premult_unpacked_span_8(tmp_row, width);
                else
                    _cg_bitmap_unpremult_unpacked_span_8(tmp_row, width);
            }

            _cg_pack_8(dst_format, tmp_row, dst, width);
        }
        break;
    case _TMP_FMT_DOUBLE:
        tmp_row = c_malloc(width * 8 * 4);
        for (y = 0; y < height; y++) {
            src = src_data + y * src_rowstride;
            dst = dst_data + y * dst_rowstride;

            _cg_unpack_64(src_format, src, tmp_row, width);

            /* Handle premultiplication */
            if (need_multiply) {
                if (_cg_pixel_format_is_premultiplied(dst_format))
                    _cg_bitmap_premult_unpacked_span_64f(tmp_row, width);
                else
                    _cg_bitmap_unpremult_unpacked_span_64f(tmp_row, width);
            }

            _cg_pack_64(dst_format, tmp_row, dst, width);
        }
        break;
    case _TMP_FMT_NONE:
        c_assert_not_reached();
        break;
    }

    _cg_bitmap_unmap(src_bmp);
    _cg_bitmap_unmap(dst_bmp);

    c_free(tmp_row);

    return ret;
}

cg_bitmap_t *
_cg_bitmap_convert(cg_bitmap_t *src_bmp,
                   cg_pixel_format_t dst_format,
                   cg_error_t **error)
{
    cg_device_t *dev = src_bmp->dev;
    cg_bitmap_t *dst_bmp;
    int width, height;

    width = cg_bitmap_get_width(src_bmp);
    height = cg_bitmap_get_height(src_bmp);

    dst_bmp = _cg_bitmap_new_with_malloc_buffer(dev, width, height,
                                                dst_format, error);
    if (!dst_bmp)
        return NULL;

    if (!_cg_bitmap_convert_into_bitmap(src_bmp, dst_bmp, error)) {
        cg_object_unref(dst_bmp);
        return NULL;
    }

    return dst_bmp;
}

static bool
driver_can_convert(cg_device_t *dev,
                   cg_pixel_format_t src_format,
                   cg_pixel_format_t internal_format)
{
    if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_FORMAT_CONVERSION))
        return false;

    if (src_format == internal_format)
        return true;

    /* If the driver doesn't natively support alpha textures then it
     * won't work correctly to convert to/from component-alpha
     * textures */
    if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_ALPHA_TEXTURES) &&
        (src_format == CG_PIXEL_FORMAT_A_8 ||
         internal_format == CG_PIXEL_FORMAT_A_8))
        return false;

    /* Same for red-green textures. If red-green textures aren't
    * supported then the internal format should never be RG_88 but we
    * should still be able to convert from an RG source image */
    if (!cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_RG) &&
        src_format == CG_PIXEL_FORMAT_RG_88)
        return false;

    return true;
}

cg_bitmap_t *
_cg_bitmap_convert_for_upload(cg_bitmap_t *src_bmp,
                              cg_pixel_format_t internal_format,
                              bool can_convert_in_place,
                              cg_error_t **error)
{
    cg_device_t *dev = _cg_bitmap_get_context(src_bmp);
    cg_pixel_format_t src_format = cg_bitmap_get_format(src_bmp);
    cg_bitmap_t *dst_bmp;

    c_return_val_if_fail(internal_format != CG_PIXEL_FORMAT_ANY, NULL);

    /* OpenGL supports specifying a different format for the internal
       format when uploading texture data. We should use this to convert
       formats because it is likely to be faster and support more types
       than the CGlib bitmap code. However under GLES the internal format
       must be the same as the bitmap format and it only supports a
       limited number of formats so we must convert using the CGlib
       bitmap code instead */

    if (driver_can_convert(dev, src_format, internal_format)) {
        /* If the source format does not have the same premult flag as the
           internal_format then we need to copy and convert it */
        if (_cg_texture_needs_premult_conversion(src_format, internal_format)) {
            cg_pixel_format_t toggled =
                _cg_pixel_format_toggle_premult_status(src_format);

            if (can_convert_in_place) {
                if (_cg_bitmap_convert_premult_status(src_bmp, toggled, error)) {
                    dst_bmp = cg_object_ref(src_bmp);
                } else
                    return NULL;
            } else {
                dst_bmp = _cg_bitmap_convert(src_bmp, toggled, error);
                if (dst_bmp == NULL)
                    return NULL;
            }
        } else
            dst_bmp = cg_object_ref(src_bmp);
    } else {
        cg_pixel_format_t closest_format;

        closest_format = dev->driver_vtable->pixel_format_to_gl(dev,
            internal_format,
            NULL, /* ignore gl intformat */
            NULL, /* ignore gl format */
            NULL); /* ignore gl type */

        if (closest_format != src_format)
            dst_bmp = _cg_bitmap_convert(src_bmp, closest_format, error);
        else
            dst_bmp = cg_object_ref(src_bmp);
    }

    return dst_bmp;
}

bool
_cg_bitmap_unpremult(cg_bitmap_t *bmp, cg_error_t **error)
{
    uint8_t *p, *data;
    int x, y;
    cg_pixel_format_t format;
    int width, height;
    int rowstride;

    format = cg_bitmap_get_format(bmp);
    width = cg_bitmap_get_width(bmp);
    height = cg_bitmap_get_height(bmp);
    rowstride = cg_bitmap_get_rowstride(bmp);

    data = _cg_bitmap_map(bmp, (CG_BUFFER_ACCESS_READ |
                                CG_BUFFER_ACCESS_WRITE), 0, error);
    if (data == NULL)
        return false;

    switch (_cg_pixel_format_premult_stem(format)) {
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888:
        for (y = 0; y < height; y++)
            _cg_bitmap_unpremult_unpacked_span_8(data + y * rowstride, width);
        break;
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ABGR_8888:
        for (y = 0; y < height; y++) {
            p = data + y * rowstride;

            for (x = 0; x < width; x++) {
                if (p[0] == 0)
                    _cg_unpremult_alpha_0(p);
                else
                    _cg_unpremult_alpha_first(p);
                p += 4;
            }
        }
        break;
    default: {
        double *tmp_row = c_malloc(sizeof(*tmp_row) * 4 * width);

        for (y = 0; y < height; y++) {
            p = data + y * rowstride;

            _cg_unpack_64(format, p, tmp_row, width);
            _cg_bitmap_unpremult_unpacked_span_64f(tmp_row, width);
            _cg_pack_64(format, tmp_row, p, width);
        }

        c_free(tmp_row);
    }
    }

    _cg_bitmap_unmap(bmp);

    _cg_bitmap_set_format(bmp, _cg_pixel_format_premult_stem(format));

    return true;
}

bool
_cg_bitmap_premult(cg_bitmap_t *bmp, cg_error_t **error)
{
    uint8_t *p, *data;
    int x, y;
    cg_pixel_format_t format;
    int width, height;
    int rowstride;

    format = cg_bitmap_get_format(bmp);
    width = cg_bitmap_get_width(bmp);
    height = cg_bitmap_get_height(bmp);
    rowstride = cg_bitmap_get_rowstride(bmp);

    data = _cg_bitmap_map(bmp,
                          CG_BUFFER_ACCESS_READ | CG_BUFFER_ACCESS_WRITE,
                          0, error);
    if (data == NULL)
        return false;

    switch (_cg_pixel_format_premult_stem(format)) {
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888:
        for (y = 0; y < height; y++)
            _cg_bitmap_premult_unpacked_span_8(data + y * rowstride, width);
        break;
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ABGR_8888:
        for (y = 0; y < height; y++) {
            p = data + y * rowstride;

            for (x = 0; x < width; x++) {
                _cg_premult_alpha_first(p);
                p += 4;
            }
        }
        break;
    default: {
        double *tmp_row = c_malloc(sizeof(*tmp_row) * 4 * width);

        for (y = 0; y < height; y++) {
            p = data + y * rowstride;

            _cg_unpack_64(format, p, tmp_row, width);
            _cg_bitmap_premult_unpacked_span_64f(tmp_row, width);
            _cg_pack_64(format, tmp_row, p, width);
        }

        c_free(tmp_row);
    }
    }

    _cg_bitmap_unmap(bmp);

    _cg_bitmap_set_format(bmp, _cg_pixel_format_premultiply(format));

    return true;
}
