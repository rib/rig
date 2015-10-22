/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012,2014 Intel Corporation.
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

/* This file is included multiple times with different definitions for
 * the intermediate component_type type we are unpacking too
 * (e.g. uint8_t or double).
 */

/* Unpacking to RGBA */

inline static void
C_PASTE(_cg_unpack_a_8_, component_size) (const uint8_t *src,
                                          component_type *dst,
                                          int width)
{
    while (width-- > 0) {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = X_FROM_UN8(*src);
        dst += 4;
        src++;
    }
}

inline static void
C_PASTE(_cg_unpack_rg_88_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_UN8(src[0]);
        dst[1] = X_FROM_UN8(src[1]);
        dst[2] = 0;
        dst[3] = X_ONE;
        dst += 4;
        src += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rgb_565_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        uint16_t v = *(const uint16_t *)src;

        dst[0] = X_FROM_5_BITS(v >> 11);
        dst[1] = X_FROM_6_BITS((v >> 5) & 63);
        dst[2] = X_FROM_5_BITS(v & 31);
        dst[3] = X_ONE;
        dst += 4;
        src += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rgb_888_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_UN8(src[0]);
        dst[1] = X_FROM_UN8(src[1]);
        dst[2] = X_FROM_UN8(src[2]);
        dst[3] = X_ONE;
        dst += 4;
        src += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_bgr_888_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_UN8(src[2]);
        dst[1] = X_FROM_UN8(src[1]);
        dst[2] = X_FROM_UN8(src[0]);
        dst[3] = X_ONE;
        dst += 4;
        src += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_4444_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        uint16_t v = *(const uint16_t *)src;

        dst[0] = X_FROM_4_BITS(v >> 12);
        dst[1] = X_FROM_4_BITS((v >> 8) & 15);
        dst[2] = X_FROM_4_BITS((v >> 4) & 15);
        dst[3] = X_FROM_4_BITS(v & 15);
        dst += 4;
        src += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_5551_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        uint16_t v = *(const uint16_t *)src;

        dst[0] = X_FROM_5_BITS(v >> 11);
        dst[1] = X_FROM_5_BITS((v >> 6) & 31);
        dst[2] = X_FROM_5_BITS((v >> 1) & 31);
        dst[3] = X_FROM_1_BIT(v & 1);
        dst += 4;
        src += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_8888_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_UN8(src[0]);
        dst[1] = X_FROM_UN8(src[1]);
        dst[2] = X_FROM_UN8(src[2]);
        dst[3] = X_FROM_UN8(src[3]);
        dst += 4;
        src += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_bgra_8888_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_UN8(src[2]);
        dst[1] = X_FROM_UN8(src[1]);
        dst[2] = X_FROM_UN8(src[0]);
        dst[3] = X_FROM_UN8(src[3]);
        dst += 4;
        src += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_argb_8888_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_UN8(src[1]);
        dst[1] = X_FROM_UN8(src[2]);
        dst[2] = X_FROM_UN8(src[3]);
        dst[3] = X_FROM_UN8(src[0]);
        dst += 4;
        src += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_abgr_8888_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_UN8(src[3]);
        dst[1] = X_FROM_UN8(src[2]);
        dst[2] = X_FROM_UN8(src[1]);
        dst[3] = X_FROM_UN8(src[0]);
        dst += 4;
        src += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_,
        component_size) (cg_pixel_format_t format,
                         const uint8_t *src,
                         component_type *dst,
                         int width)
{
    switch (format) {
    case CG_PIXEL_FORMAT_A_8:
        C_PASTE(_cg_unpack_a_8_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_88:
        C_PASTE(_cg_unpack_rg_88_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_565:
        C_PASTE(_cg_unpack_rgb_565_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_888:
        C_PASTE(_cg_unpack_rgb_888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_888:
        C_PASTE(_cg_unpack_bgr_888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_4444:
    case CG_PIXEL_FORMAT_RGBA_4444_PRE:
        C_PASTE(_cg_unpack_rgba_4444_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_5551:
    case CG_PIXEL_FORMAT_RGBA_5551_PRE:
        C_PASTE(_cg_unpack_rgba_5551_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_RGBA_8888_PRE:
        C_PASTE(_cg_unpack_rgba_8888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:
        C_PASTE(_cg_unpack_bgra_8888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ARGB_8888_PRE:
        C_PASTE(_cg_unpack_argb_8888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_ABGR_8888:
    case CG_PIXEL_FORMAT_ABGR_8888_PRE:
        C_PASTE(_cg_unpack_abgr_8888_, component_size) (src, dst, width);
        break;

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
    case CG_PIXEL_FORMAT_RGBA_32323232U:
    case CG_PIXEL_FORMAT_BGRA_32323232U:
    case CG_PIXEL_FORMAT_RGBA_16161616F:
    case CG_PIXEL_FORMAT_BGRA_16161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F_PRE:
    case CG_PIXEL_FORMAT_BGRA_16161616F_PRE:
    case CG_PIXEL_FORMAT_RGBA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F:
    case CG_PIXEL_FORMAT_RGBA_32323232F_PRE:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:

#if component_size == 64
        _cg_unpack_fallback_64 (format, src, dst, width);
#else
        c_assert_not_reached();
#endif
        break;

    case CG_PIXEL_FORMAT_DEPTH_16:
    case CG_PIXEL_FORMAT_DEPTH_32:
    case CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
    case CG_PIXEL_FORMAT_ANY:
        c_assert_not_reached();
    }
}
