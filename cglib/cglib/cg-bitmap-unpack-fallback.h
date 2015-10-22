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

/* Unpacking to RGBA */

inline static void
C_PASTE(_cg_unpack_a_8sn_, component_size) (const uint8_t *src,
                                            component_type *dst,
                                            int width)
{
    while (width-- > 0) {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = X_FROM_SN8(*src);
        dst += 4;
        src++;
    }
}

inline static void
C_PASTE(_cg_unpack_a_16u_, component_size) (const uint8_t *src,
                                            component_type *dst,
                                            int width)
{
    const uint16_t *c = (const uint16_t *)src;
    while (width-- > 0) {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = X_FROM_U16(*c);
        dst += 4;
        c++;
    }
}

inline static void
C_PASTE(_cg_unpack_a_16s_, component_size) (const uint8_t *src,
                                            component_type *dst,
                                            int width)
{
    const int16_t *c = (const int16_t *)src;
    while (width-- > 0) {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = X_FROM_S16(*c);
        dst += 4;
        c++;
    }
}

inline static void
C_PASTE(_cg_unpack_a_32u_, component_size) (const uint8_t *src,
                                            component_type *dst,
                                            int width)
{
    const uint32_t *c = (const uint32_t *)src;
    while (width-- > 0) {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = X_FROM_U32(*c);
        dst += 4;
        c++;
    }
}

inline static void
C_PASTE(_cg_unpack_a_32s_, component_size) (const uint8_t *src,
                                            component_type *dst,
                                            int width)
{
    const int32_t *c = (const int32_t *)src;
    while (width-- > 0) {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = X_FROM_S32(*c);
        dst += 4;
        c++;
    }
}

inline static void
C_PASTE(_cg_unpack_a_32f_, component_size) (const uint8_t *src,
                                            component_type *dst,
                                            int width)
{
    const float *c = (const float *)src;
    while (width-- > 0) {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = X_FROM_FLOAT(*c);
        dst += 4;
        c++;
    }
}

inline static void
C_PASTE(_cg_unpack_rg_88sn_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_SN8(src[0]);
        dst[1] = X_FROM_SN8(src[1]);
        dst[2] = 0;
        dst[3] = X_ONE;
        dst += 4;
        src += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rg_1616u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint16_t *c = (const uint16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U16(c[0]);
        dst[1] = X_FROM_U16(c[1]);
        dst[2] = 0;
        dst[3] = X_ONE;
        dst += 4;
        c += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rg_1616s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int16_t *c = (const int16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S16(c[0]);
        dst[1] = X_FROM_S16(c[1]);
        dst[2] = 0;
        dst[3] = X_ONE;
        dst += 4;
        c += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rg_3232u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint32_t *c = (const uint32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U32(c[0]);
        dst[1] = X_FROM_U32(c[1]);
        dst[2] = 0;
        dst[3] = X_ONE;
        dst += 4;
        c += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rg_3232f_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const float *c = (const float *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_FLOAT(c[0]);
        dst[1] = X_FROM_FLOAT(c[1]);
        dst[2] = 0;
        dst[3] = X_ONE;
        dst += 4;
        c += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rgb_888sn_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_SN8(src[0]);
        dst[1] = X_FROM_SN8(src[1]);
        dst[2] = X_FROM_SN8(src[2]);
        dst[3] = X_ONE;
        dst += 4;
        src += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_rgb_161616u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint16_t *c = (const uint16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U16(c[0]);
        dst[1] = X_FROM_U16(c[1]);
        dst[2] = X_FROM_U16(c[2]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_rgb_161616s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int16_t *c = (const int16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S16(c[0]);
        dst[1] = X_FROM_S16(c[1]);
        dst[2] = X_FROM_S16(c[2]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_rgb_323232u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint32_t *c = (const uint32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U32(c[0]);
        dst[1] = X_FROM_U32(c[1]);
        dst[2] = X_FROM_U32(c[2]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_rgb_323232s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int32_t *c = (const int32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S32(c[0]);
        dst[1] = X_FROM_S32(c[1]);
        dst[2] = X_FROM_S32(c[2]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_rgb_323232f_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const float *c = (const float *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_FLOAT(c[0]);
        dst[1] = X_FROM_FLOAT(c[1]);
        dst[2] = X_FROM_FLOAT(c[2]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_bgr_888sn_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_SN8(src[2]);
        dst[1] = X_FROM_SN8(src[1]);
        dst[2] = X_FROM_SN8(src[0]);
        dst[3] = X_ONE;
        dst += 4;
        src += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_bgr_161616u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint16_t *c = (const uint16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U16(c[2]);
        dst[1] = X_FROM_U16(c[1]);
        dst[2] = X_FROM_U16(c[0]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_bgr_161616s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int16_t *c = (const int16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S16(c[2]);
        dst[1] = X_FROM_S16(c[1]);
        dst[2] = X_FROM_S16(c[0]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_bgr_323232u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint32_t *c = (const uint32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U32(c[2]);
        dst[1] = X_FROM_U32(c[1]);
        dst[2] = X_FROM_U32(c[0]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_bgr_323232s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int32_t *c = (const int32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S32(c[2]);
        dst[1] = X_FROM_S32(c[1]);
        dst[2] = X_FROM_S32(c[0]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_bgr_323232f_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const float *c = (const float *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_FLOAT(c[2]);
        dst[1] = X_FROM_FLOAT(c[1]);
        dst[2] = X_FROM_FLOAT(c[0]);
        dst[3] = X_ONE;
        dst += 4;
        c += 3;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_8888sn_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_SN8(src[0]);
        dst[1] = X_FROM_SN8(src[1]);
        dst[2] = X_FROM_SN8(src[2]);
        dst[3] = X_FROM_SN8(src[3]);
        dst += 4;
        src += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_bgra_8888sn_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_SN8(src[2]);
        dst[1] = X_FROM_SN8(src[1]);
        dst[2] = X_FROM_SN8(src[0]);
        dst[3] = X_FROM_SN8(src[3]);
        dst += 4;
        src += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_argb_8888sn_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_SN8(src[1]);
        dst[1] = X_FROM_SN8(src[2]);
        dst[2] = X_FROM_SN8(src[3]);
        dst[3] = X_FROM_SN8(src[0]);
        dst += 4;
        src += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_abgr_8888sn_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        dst[0] = X_FROM_SN8(src[3]);
        dst[1] = X_FROM_SN8(src[2]);
        dst[2] = X_FROM_SN8(src[1]);
        dst[3] = X_FROM_SN8(src[0]);
        dst += 4;
        src += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_1010102_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        uint32_t v = *(const uint32_t *)src;

        dst[0] = X_FROM_10_BITS(v >> 22);
        dst[1] = X_FROM_10_BITS((v >> 12) & 1023);
        dst[2] = X_FROM_10_BITS((v >> 2) & 1023);
        dst[3] = X_FROM_2_BITS(v & 3);
        dst += 4;
        src += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_bgra_1010102_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        uint32_t v = *(const uint32_t *)src;

        dst[2] = X_FROM_10_BITS(v >> 22);
        dst[1] = X_FROM_10_BITS((v >> 12) & 1023);
        dst[0] = X_FROM_10_BITS((v >> 2) & 1023);
        dst[3] = X_FROM_2_BITS(v & 3);
        dst += 4;
        src += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_argb_2101010_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        uint32_t v = *(const uint32_t *)src;

        dst[3] = X_FROM_2_BITS(v >> 30);
        dst[0] = X_FROM_10_BITS((v >> 20) & 1023);
        dst[1] = X_FROM_10_BITS((v >> 10) & 1023);
        dst[2] = X_FROM_10_BITS(v & 1023);
        dst += 4;
        src += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_abgr_2101010_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    while (width-- > 0) {
        uint32_t v = *(const uint32_t *)src;

        dst[3] = X_FROM_2_BITS(v >> 30);
        dst[2] = X_FROM_10_BITS((v >> 20) & 1023);
        dst[1] = X_FROM_10_BITS((v >> 10) & 1023);
        dst[0] = X_FROM_10_BITS(v & 1023);
        dst += 4;
        src += 2;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_16161616u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint16_t *c = (const uint16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U16(c[0]);
        dst[1] = X_FROM_U16(c[1]);
        dst[2] = X_FROM_U16(c[2]);
        dst[3] = X_FROM_U16(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_bgra_16161616u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint16_t *c = (const uint16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U16(c[2]);
        dst[1] = X_FROM_U16(c[1]);
        dst[2] = X_FROM_U16(c[0]);
        dst[3] = X_FROM_U16(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_argb_16161616u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint16_t *c = (const uint16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U16(c[1]);
        dst[1] = X_FROM_U16(c[2]);
        dst[2] = X_FROM_U16(c[3]);
        dst[3] = X_FROM_U16(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_abgr_16161616u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint16_t *c = (const uint16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U16(c[3]);
        dst[1] = X_FROM_U16(c[2]);
        dst[2] = X_FROM_U16(c[1]);
        dst[3] = X_FROM_U16(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_16161616s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int16_t *c = (const int16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S16(c[0]);
        dst[1] = X_FROM_S16(c[1]);
        dst[2] = X_FROM_S16(c[2]);
        dst[3] = X_FROM_S16(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_bgra_16161616s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int16_t *c = (const int16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S16(c[2]);
        dst[1] = X_FROM_S16(c[1]);
        dst[2] = X_FROM_S16(c[0]);
        dst[3] = X_FROM_S16(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_argb_16161616s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int16_t *c = (const int16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S16(c[1]);
        dst[1] = X_FROM_S16(c[2]);
        dst[2] = X_FROM_S16(c[3]);
        dst[3] = X_FROM_S16(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_abgr_16161616s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int16_t *c = (const int16_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S16(c[3]);
        dst[1] = X_FROM_S16(c[2]);
        dst[2] = X_FROM_S16(c[1]);
        dst[3] = X_FROM_S16(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_32323232u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint32_t *c = (const uint32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U32(c[0]);
        dst[1] = X_FROM_U32(c[1]);
        dst[2] = X_FROM_U32(c[2]);
        dst[3] = X_FROM_U32(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_bgra_32323232u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint32_t *c = (const uint32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U32(c[2]);
        dst[1] = X_FROM_U32(c[1]);
        dst[2] = X_FROM_U32(c[0]);
        dst[3] = X_FROM_U32(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_argb_32323232u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint32_t *c = (const uint32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U32(c[1]);
        dst[1] = X_FROM_U32(c[2]);
        dst[2] = X_FROM_U32(c[3]);
        dst[3] = X_FROM_U32(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_abgr_32323232u_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const uint32_t *c = (const uint32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_U32(c[3]);
        dst[1] = X_FROM_U32(c[2]);
        dst[2] = X_FROM_U32(c[1]);
        dst[3] = X_FROM_U32(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_32323232s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int32_t *c = (const int32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S32(c[0]);
        dst[1] = X_FROM_S32(c[1]);
        dst[2] = X_FROM_S32(c[2]);
        dst[3] = X_FROM_S32(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_bgra_32323232s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int32_t *c = (const int32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S32(c[2]);
        dst[1] = X_FROM_S32(c[1]);
        dst[2] = X_FROM_S32(c[0]);
        dst[3] = X_FROM_S32(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_argb_32323232s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int32_t *c = (const int32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S32(c[1]);
        dst[1] = X_FROM_S32(c[2]);
        dst[2] = X_FROM_S32(c[3]);
        dst[3] = X_FROM_S32(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_abgr_32323232s_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const int32_t *c = (const int32_t *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_S32(c[3]);
        dst[1] = X_FROM_S32(c[2]);
        dst[2] = X_FROM_S32(c[1]);
        dst[3] = X_FROM_S32(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_rgba_32323232f_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const float *c = (const float *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_FLOAT(c[0]);
        dst[1] = X_FROM_FLOAT(c[1]);
        dst[2] = X_FROM_FLOAT(c[2]);
        dst[3] = X_FROM_FLOAT(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_bgra_32323232f_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const float *c = (const float *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_FLOAT(c[2]);
        dst[1] = X_FROM_FLOAT(c[1]);
        dst[2] = X_FROM_FLOAT(c[0]);
        dst[3] = X_FROM_FLOAT(c[3]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_argb_32323232f_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const float *c = (const float *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_FLOAT(c[1]);
        dst[1] = X_FROM_FLOAT(c[2]);
        dst[2] = X_FROM_FLOAT(c[3]);
        dst[3] = X_FROM_FLOAT(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_abgr_32323232f_,
        component_size) (const uint8_t *src, component_type *dst, int width)
{
    const float *c = (const float *)src;
    while (width-- > 0) {
        dst[0] = X_FROM_FLOAT(c[3]);
        dst[1] = X_FROM_FLOAT(c[2]);
        dst[2] = X_FROM_FLOAT(c[1]);
        dst[3] = X_FROM_FLOAT(c[0]);
        dst += 4;
        c += 4;
    }
}

inline static void
C_PASTE(_cg_unpack_fallback_,
        component_size) (cg_pixel_format_t format,
                         const uint8_t *src,
                         component_type *dst,
                         int width)
{
    switch (format) {
    case CG_PIXEL_FORMAT_A_8:
    case CG_PIXEL_FORMAT_RG_88:
    case CG_PIXEL_FORMAT_RGB_565:
    case CG_PIXEL_FORMAT_RGB_888:
    case CG_PIXEL_FORMAT_BGR_888:
    case CG_PIXEL_FORMAT_RGBA_4444:
    case CG_PIXEL_FORMAT_RGBA_4444_PRE:
    case CG_PIXEL_FORMAT_RGBA_5551:
    case CG_PIXEL_FORMAT_RGBA_5551_PRE:
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_RGBA_8888_PRE:
    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ARGB_8888_PRE:
    case CG_PIXEL_FORMAT_ABGR_8888:
    case CG_PIXEL_FORMAT_ABGR_8888_PRE:
        /* Should be handled in _cg_unpack_8() */
        c_assert_not_reached();
        break;

    case CG_PIXEL_FORMAT_A_8SN:
        C_PASTE(_cg_unpack_a_8sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_A_16U:
        C_PASTE(_cg_unpack_a_16u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_A_32U:
        C_PASTE(_cg_unpack_a_32u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_A_32F:
        C_PASTE(_cg_unpack_a_32f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_88SN:
        C_PASTE(_cg_unpack_rg_88sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_1616U:
        C_PASTE(_cg_unpack_rg_1616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_3232U:
        C_PASTE(_cg_unpack_rg_3232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RG_3232F:
        C_PASTE(_cg_unpack_rg_3232f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_888SN:
        C_PASTE(_cg_unpack_rgb_888sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_888SN:
        C_PASTE(_cg_unpack_bgr_888sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_161616U:
        C_PASTE(_cg_unpack_rgb_161616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_161616U:
        C_PASTE(_cg_unpack_bgr_161616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_323232U:
        C_PASTE(_cg_unpack_rgb_323232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_323232U:
        C_PASTE(_cg_unpack_bgr_323232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGB_323232F:
        C_PASTE(_cg_unpack_rgb_323232f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGR_323232F:
        C_PASTE(_cg_unpack_bgr_323232f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_8888SN:
        C_PASTE(_cg_unpack_rgba_8888sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_8888SN:
        C_PASTE(_cg_unpack_bgra_8888sn_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_1010102:
    case CG_PIXEL_FORMAT_RGBA_1010102_PRE:
        C_PASTE(_cg_unpack_rgba_1010102_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_1010102:
    case CG_PIXEL_FORMAT_BGRA_1010102_PRE:
        C_PASTE(_cg_unpack_bgra_1010102_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_ARGB_2101010:
    case CG_PIXEL_FORMAT_ARGB_2101010_PRE:
        C_PASTE(_cg_unpack_argb_2101010_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_ABGR_2101010:
    case CG_PIXEL_FORMAT_ABGR_2101010_PRE:
        C_PASTE(_cg_unpack_abgr_2101010_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_16161616U:
        C_PASTE(_cg_unpack_rgba_16161616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_16161616U:
        C_PASTE(_cg_unpack_bgra_16161616u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_32323232U:
        C_PASTE(_cg_unpack_rgba_32323232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_32323232U:
        C_PASTE(_cg_unpack_bgra_32323232u_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_RGBA_32323232F:
    case CG_PIXEL_FORMAT_RGBA_32323232F_PRE:
        C_PASTE(_cg_unpack_rgba_32323232f_, component_size) (src, dst, width);
        break;
    case CG_PIXEL_FORMAT_BGRA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:
        C_PASTE(_cg_unpack_bgra_32323232f_, component_size) (src, dst, width);
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
