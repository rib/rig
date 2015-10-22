/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
 * the intermediate component_type that we unpacked too (e.g. uint8_t
 * or double).
 */

/* Packing from RGBA */

inline static void
C_PASTE(_cg_pack_a_8_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        *dst = X_TO_UN8(src[3]);
        src += 4;
        dst++;
    }
}

inline static void
C_PASTE(_cg_pack_a_8sn_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        *dst = X_TO_SN8(src[3]);
        src += 4;
        dst++;
    }
}

inline static void
C_PASTE(_cg_pack_a_16u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint16_t *p = (uint16_t *)dst;
    while (width-- > 0) {
        *p = X_TO_U16(src[3]);
        src += 4;
        p++;
    }
}

inline static void
C_PASTE(_cg_pack_a_32u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint32_t *p = (uint32_t *)dst;
    while (width-- > 0) {
        *p = X_TO_U32(src[3]);
        src += 4;
        p++;
    }
}

inline static void
C_PASTE(_cg_pack_a_32f_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    float *p = (float *)dst;
    while (width-- > 0) {
        *p = X_TO_FLOAT(src[3]);
        src += 4;
        p++;
    }
}

inline static void
C_PASTE(_cg_pack_rg_88_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_TO_UN8(src[0]);
        dst[1] = X_TO_UN8(src[1]);
        src += 4;
        dst += 2;
    }
}

inline static void
C_PASTE(_cg_pack_rg_88sn_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_TO_SN8(src[0]);
        dst[1] = X_TO_SN8(src[1]);
        src += 4;
        dst += 2;
    }
}

inline static void
C_PASTE(_cg_pack_rg_1616u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint16_t *p = (uint16_t *)dst;
    while (width-- > 0) {
        p[0] = X_TO_U16(src[0]);
        p[1] = X_TO_U16(src[1]);
        src += 4;
        p += 2;
    }
}

inline static void
C_PASTE(_cg_pack_rg_3232u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint32_t *p = (uint32_t *)dst;
    while (width-- > 0) {
        p[0] = X_TO_U32(src[0]);
        p[1] = X_TO_U32(src[1]);
        src += 4;
        p += 2;
    }
}

inline static void
C_PASTE(_cg_pack_rg_3232f_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    float *p = (float *)dst;
    while (width-- > 0) {
        p[0] = X_TO_FLOAT(src[0]);
        p[1] = X_TO_FLOAT(src[1]);
        src += 4;
        p += 2;
    }
}

inline static void
C_PASTE(_cg_pack_rgb_565_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        uint16_t *v = (uint16_t *)dst;

        *v = ((X_TO_5_BITS(src[0]) << 11) | (X_TO_6_BITS(src[1]) << 5) | X_TO_5_BITS(src[2]));
        src += 4;
        dst += 2;
    }
}

inline static void
C_PASTE(_cg_pack_rgb_888_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_TO_UN8(src[0]);
        dst[1] = X_TO_UN8(src[1]);
        dst[2] = X_TO_UN8(src[2]);
        src += 4;
        dst += 3;
    }
}

inline static void
C_PASTE(_cg_pack_bgr_888_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[2] = X_TO_UN8(src[0]);
        dst[1] = X_TO_UN8(src[1]);
        dst[0] = X_TO_UN8(src[2]);
        src += 4;
        dst += 3;
    }
}

inline static void
C_PASTE(_cg_pack_rgb_888sn_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_TO_SN8(src[0]);
        dst[1] = X_TO_SN8(src[1]);
        dst[2] = X_TO_SN8(src[2]);
        src += 4;
        dst += 3;
    }
}

inline static void
C_PASTE(_cg_pack_bgr_888sn_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[2] = X_TO_SN8(src[0]);
        dst[1] = X_TO_SN8(src[1]);
        dst[0] = X_TO_SN8(src[2]);
        src += 4;
        dst += 3;
    }
}

inline static void
C_PASTE(_cg_pack_rgb_161616u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint16_t *p = (uint16_t *)dst;
    while (width-- > 0) {
        p[0] = X_TO_U16(src[0]);
        p[1] = X_TO_U16(src[1]);
        p[2] = X_TO_U16(src[2]);
        src += 4;
        p += 3;
    }
}

inline static void
C_PASTE(_cg_pack_bgr_161616u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint16_t *p = (uint16_t *)dst;
    while (width-- > 0) {
        p[2] = X_TO_U16(src[0]);
        p[1] = X_TO_U16(src[1]);
        p[0] = X_TO_U16(src[2]);
        src += 4;
        p += 3;
    }
}

inline static void
C_PASTE(_cg_pack_rgb_323232u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint32_t *p = (uint32_t *)dst;
    while (width-- > 0) {
        p[0] = X_TO_U32(src[0]);
        p[1] = X_TO_U32(src[1]);
        p[2] = X_TO_U32(src[2]);
        src += 4;
        p += 3;
    }
}

inline static void
C_PASTE(_cg_pack_bgr_323232u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint32_t *p = (uint32_t *)dst;
    while (width-- > 0) {
        p[2] = X_TO_U32(src[0]);
        p[1] = X_TO_U32(src[1]);
        p[0] = X_TO_U32(src[2]);
        src += 4;
        p += 3;
    }
}

inline static void
C_PASTE(_cg_pack_rgb_323232f_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    float *p = (float *)dst;
    while (width-- > 0) {
        p[0] = X_TO_FLOAT(src[0]);
        p[1] = X_TO_FLOAT(src[1]);
        p[2] = X_TO_FLOAT(src[2]);
        src += 4;
        p += 3;
    }
}

inline static void
C_PASTE(_cg_pack_bgr_323232f_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    float *p = (float *)dst;
    while (width-- > 0) {
        p[2] = X_TO_FLOAT(src[0]);
        p[1] = X_TO_FLOAT(src[1]);
        p[0] = X_TO_FLOAT(src[2]);
        src += 4;
        p += 3;
    }
}

inline static void
C_PASTE(_cg_pack_rgba_4444_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        uint16_t *v = (uint16_t *)dst;

        *v = ((X_TO_4_BITS(src[0]) << 12) | (X_TO_4_BITS(src[1]) << 8) |
              (X_TO_4_BITS(src[2]) << 4) | X_TO_4_BITS(src[3]));
        src += 4;
        dst += 2;
    }
}

inline static void
C_PASTE(_cg_pack_rgba_5551_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        uint16_t *v = (uint16_t *)dst;

        *v = ((X_TO_5_BITS(src[0]) << 11) | (X_TO_5_BITS(src[1]) << 6) |
              (X_TO_5_BITS(src[2]) << 1) | X_TO_1_BIT(src[3]));
        src += 4;
        dst += 2;
    }
}

inline static void
C_PASTE(_cg_pack_rgba_8888_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_TO_UN8(src[0]);
        dst[1] = X_TO_UN8(src[1]);
        dst[2] = X_TO_UN8(src[2]);
        dst[3] = X_TO_UN8(src[3]);
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_bgra_8888_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[2] = X_TO_UN8(src[0]);
        dst[1] = X_TO_UN8(src[1]);
        dst[0] = X_TO_UN8(src[2]);
        dst[3] = X_TO_UN8(src[3]);
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_argb_8888_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[1] = X_TO_UN8(src[0]);
        dst[2] = X_TO_UN8(src[1]);
        dst[3] = X_TO_UN8(src[2]);
        dst[0] = X_TO_UN8(src[3]);
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_abgr_8888_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[3] = X_TO_UN8(src[0]);
        dst[2] = X_TO_UN8(src[1]);
        dst[1] = X_TO_UN8(src[2]);
        dst[0] = X_TO_UN8(src[3]);
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_rgba_8888sn_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_TO_SN8(src[0]);
        dst[1] = X_TO_SN8(src[1]);
        dst[2] = X_TO_SN8(src[2]);
        dst[3] = X_TO_SN8(src[3]);
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_bgra_8888sn_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        dst[2] = X_TO_SN8(src[0]);
        dst[1] = X_TO_SN8(src[1]);
        dst[0] = X_TO_SN8(src[2]);
        dst[3] = X_TO_SN8(src[3]);
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_rgba_1010102_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        uint32_t *v = (uint32_t *)dst;

        *v = ((X_TO_10_BITS(src[0]) << 22) | (X_TO_10_BITS(src[1]) << 12) |
              (X_TO_10_BITS(src[2]) << 2) | X_TO_2_BITS(src[3]));
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_bgra_1010102_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        uint32_t *v = (uint32_t *)dst;

        *v = ((X_TO_10_BITS(src[2]) << 22) | (X_TO_10_BITS(src[1]) << 12) |
              (X_TO_10_BITS(src[0]) << 2) | X_TO_2_BITS(src[3]));
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_argb_2101010_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        uint32_t *v = (uint32_t *)dst;

        *v = ((X_TO_2_BITS(src[3]) << 30) | (X_TO_10_BITS(src[0]) << 20) |
              (X_TO_10_BITS(src[1]) << 10) | X_TO_10_BITS(src[2]));
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_abgr_2101010_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    while (width-- > 0) {
        uint32_t *v = (uint32_t *)dst;

        *v = ((X_TO_2_BITS(src[3]) << 30) | (X_TO_10_BITS(src[2]) << 20) |
              (X_TO_10_BITS(src[1]) << 10) | X_TO_10_BITS(src[0]));
        src += 4;
        dst += 4;
    }
}

inline static void
C_PASTE(_cg_pack_rgba_16161616u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint16_t *p = (uint16_t *)dst;
    while (width-- > 0) {
        p[0] = X_TO_U16(src[0]);
        p[1] = X_TO_U16(src[1]);
        p[2] = X_TO_U16(src[2]);
        p[3] = X_TO_U16(src[3]);
        src += 4;
        p += 4;
    }
}

inline static void
C_PASTE(_cg_pack_bgra_16161616u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint16_t *p = (uint16_t *)dst;
    while (width-- > 0) {
        p[2] = X_TO_U16(src[0]);
        p[1] = X_TO_U16(src[1]);
        p[0] = X_TO_U16(src[2]);
        p[3] = X_TO_U16(src[3]);
        src += 4;
        p += 4;
    }
}

inline static void
C_PASTE(_cg_pack_rgba_32323232u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint32_t *p = (uint32_t *)dst;
    while (width-- > 0) {
        p[0] = X_TO_U32(src[0]);
        p[1] = X_TO_U32(src[1]);
        p[2] = X_TO_U32(src[2]);
        p[3] = X_TO_U32(src[3]);
        src += 4;
        p += 4;
    }
}

inline static void
C_PASTE(_cg_pack_bgra_32323232u_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    uint32_t *p = (uint32_t *)dst;
    while (width-- > 0) {
        p[2] = X_TO_U32(src[0]);
        p[1] = X_TO_U32(src[1]);
        p[0] = X_TO_U32(src[2]);
        p[3] = X_TO_U32(src[3]);
        src += 4;
        p += 4;
    }
}

inline static void
C_PASTE(_cg_pack_rgba_32323232f_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    float *p = (float *)dst;
    while (width-- > 0) {
        p[0] = X_TO_FLOAT(src[0]);
        p[1] = X_TO_FLOAT(src[1]);
        p[2] = X_TO_FLOAT(src[2]);
        p[3] = X_TO_FLOAT(src[3]);
        src += 4;
        p += 4;
    }
}

inline static void
C_PASTE(_cg_pack_bgra_32323232f_,
        component_size) (const component_type *src, uint8_t *dst, int width)
{
    float *p = (float *)dst;
    while (width-- > 0) {
        p[2] = X_TO_FLOAT(src[0]);
        p[1] = X_TO_FLOAT(src[1]);
        p[0] = X_TO_FLOAT(src[2]);
        p[3] = X_TO_FLOAT(src[3]);
        src += 4;
        p += 4;
    }
}

inline static void
C_PASTE(_cg_pack_, component_size) (cg_pixel_format_t format,
                                    const component_type *src,
                                    uint8_t *dst,
                                    int width)
{
    switch (format) {
    case CG_PIXEL_FORMAT_A_8:
        C_PASTE(_cg_pack_a_8_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_A_8SN:
        C_PASTE(_cg_pack_a_8sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_A_16U:
        C_PASTE(_cg_pack_a_16u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_A_32U:
        C_PASTE(_cg_pack_a_32u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_A_32F:
        C_PASTE(_cg_pack_a_32f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_88:
        C_PASTE(_cg_pack_rg_88_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_88SN:
        C_PASTE(_cg_pack_rg_88sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_1616U:
        C_PASTE(_cg_pack_rg_1616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_3232U:
        C_PASTE(_cg_pack_rg_3232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_3232F:
        C_PASTE(_cg_pack_rg_3232f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_565:
        C_PASTE(_cg_pack_rgb_565_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_888:
        C_PASTE(_cg_pack_rgb_888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_888:
        C_PASTE(_cg_pack_bgr_888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_888SN:
        C_PASTE(_cg_pack_rgb_888sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_888SN:
        C_PASTE(_cg_pack_bgr_888sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_161616U:
        C_PASTE(_cg_pack_rgb_161616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_161616U:
        C_PASTE(_cg_pack_bgr_161616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_323232U:
        C_PASTE(_cg_pack_rgb_323232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_323232F:
        C_PASTE(_cg_pack_bgr_323232f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_323232F:
        C_PASTE(_cg_pack_rgb_323232f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_323232U:
        C_PASTE(_cg_pack_bgr_323232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_4444:
    case CG_PIXEL_FORMAT_RGBA_4444_PRE:
        C_PASTE(_cg_pack_rgba_4444_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_5551:
    case CG_PIXEL_FORMAT_RGBA_5551_PRE:
        C_PASTE(_cg_pack_rgba_5551_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_RGBA_8888_PRE:
        C_PASTE(_cg_pack_rgba_8888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:
        C_PASTE(_cg_pack_bgra_8888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ARGB_8888_PRE:
        C_PASTE(_cg_pack_argb_8888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_ABGR_8888:
    case CG_PIXEL_FORMAT_ABGR_8888_PRE:
        C_PASTE(_cg_pack_abgr_8888_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_8888SN:
        C_PASTE(_cg_pack_rgba_8888sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_8888SN:
        C_PASTE(_cg_pack_bgra_8888sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_1010102:
    case CG_PIXEL_FORMAT_RGBA_1010102_PRE:
        C_PASTE(_cg_pack_rgba_1010102_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_1010102:
    case CG_PIXEL_FORMAT_BGRA_1010102_PRE:
        C_PASTE(_cg_pack_bgra_1010102_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_ARGB_2101010:
    case CG_PIXEL_FORMAT_ARGB_2101010_PRE:
        C_PASTE(_cg_pack_argb_2101010_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_ABGR_2101010:
    case CG_PIXEL_FORMAT_ABGR_2101010_PRE:
        C_PASTE(_cg_pack_abgr_2101010_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_16161616U:
        C_PASTE(_cg_pack_rgba_16161616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_16161616U:
        C_PASTE(_cg_pack_bgra_16161616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_32323232U:
        C_PASTE(_cg_pack_rgba_32323232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_32323232U:
        C_PASTE(_cg_pack_bgra_32323232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_32323232F:
    case CG_PIXEL_FORMAT_RGBA_32323232F_PRE:
        C_PASTE(_cg_pack_rgba_32323232f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:
        C_PASTE(_cg_pack_bgra_32323232f_, component_size) (src, dst, width);
        break;

    case CG_PIXEL_FORMAT_A_16F:
    case CG_PIXEL_FORMAT_RG_1616F:
    case CG_PIXEL_FORMAT_RGB_161616F:
    case CG_PIXEL_FORMAT_BGR_161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F:
    case CG_PIXEL_FORMAT_BGRA_16161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F_PRE:
    case CG_PIXEL_FORMAT_BGRA_16161616F_PRE:
        c_assert_not_reached(); /* TODO */
        break;
    case CG_PIXEL_FORMAT_DEPTH_16:
    case CG_PIXEL_FORMAT_DEPTH_32:
    case CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
    case CG_PIXEL_FORMAT_ANY:
        c_assert_not_reached();
    }
}
