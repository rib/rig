/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-feature-private.h"
#include "cg-renderer-private.h"
#include "cg-private.h"
#include "cg-framebuffer-gl-private.h"
#include "cg-texture-2d-gl-private.h"
#include "cg-attribute-gl-private.h"
#include "cg-clip-stack-gl-private.h"
#include "cg-buffer-gl-private.h"

#ifndef GL_UNSIGNED_INT_24_8
#define GL_UNSIGNED_INT_24_8 0x84FA
#endif
#ifndef GL_DEPTH_STENCIL
#define GL_DEPTH_STENCIL 0x84F9
#endif
#ifndef GL_RG
#define GL_RG 0x8227
#endif
#ifndef GL_RG8
#define GL_RG8 0x822B
#endif
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT 0x140B
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_R8_SNORM
#define GL_R8_SNORM 0x8F94
#endif
#ifndef GL_R16UI
#define GL_R16UI 0x8234
#endif
#ifndef GL_R16F
#define GL_R16F 0x822D
#endif
#ifndef GL_R32UI
#define GL_R32UI 0x8236
#endif
#ifndef GL_R32F
#define GL_R32F 0x822E
#endif
#ifndef GL_RG8_SNORM
#define GL_RG8_SNORM 0x8F95
#endif
#ifndef GL_RG16UI
#define GL_RG16UI 0x823A
#endif
#ifndef GL_RG16F
#define GL_RG16F 0x822F
#endif
#ifndef GL_R32UI
#define GL_R32UI 0x8236
#endif
#ifndef GL_R32F
#define GL_R32F 0x822E
#endif
#ifndef GL_RG8_SNORM
#define GL_RG8_SNORM 0x8F95
#endif
#ifndef GL_RG16UI
#define GL_RG16UI 0x823A
#endif
#ifndef GL_RG16F
#define GL_RG16F 0x822F
#endif
#ifndef GL_RG32UI
#define GL_RG32UI 0x823C
#endif
#ifndef GL_RG32F
#define GL_RG32F 0x8230
#endif
#ifndef GL_RGB8
#define GL_RGB8 0x8051
#endif
#ifndef GL_RGB8_SNORM
#define GL_RGB8_SNORM 0x8F96
#endif
#ifndef GL_RGB16UI
#define GL_RGB16UI 0x8D77
#endif
#ifndef GL_RGB16F
#define GL_RGB16F 0x881B
#endif
#ifndef GL_RGB32UI
#define GL_RGB32UI 0x8D71
#endif
#ifndef GL_RGB32F
#define GL_RGB32F 0x8815
#endif
#ifndef GL_RGB8
#define GL_RGB8 0x8051
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_RGBA8_SNORM
#define GL_RGBA8_SNORM 0x8F97
#endif
#ifndef GL_RGBA16UI
#define GL_RGBA16UI 0x8D76
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_RGBA32UI
#define GL_RGBA32UI 0x8D70
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_RGBA8_SNORM
#define GL_RGBA8_SNORM 0x8F97
#endif
#ifndef GL_RGBA16UI
#define GL_RGBA16UI 0x8D76
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_RGBA32UI
#define GL_RGBA32UI 0x8D70
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif

static bool
_cg_driver_pixel_format_from_gl_internal(cg_device_t *dev,
                                         GLenum gl_int_format,
                                         cg_pixel_format_t *out_format)
{
    return true;
}

static GLenum
_cg_pixel_format_get_gl_type(cg_pixel_format_t format)
{
    switch (format) {
    case CG_PIXEL_FORMAT_A_8:
    case CG_PIXEL_FORMAT_RG_88:
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
        return GL_UNSIGNED_BYTE;

    case CG_PIXEL_FORMAT_A_8SN:
    case CG_PIXEL_FORMAT_RG_88SN:
    case CG_PIXEL_FORMAT_RGB_888SN:
    case CG_PIXEL_FORMAT_BGR_888SN:
    case CG_PIXEL_FORMAT_RGBA_8888SN:
    case CG_PIXEL_FORMAT_BGRA_8888SN:
        return GL_BYTE;

    case CG_PIXEL_FORMAT_A_16U:
    case CG_PIXEL_FORMAT_RG_1616U:
    case CG_PIXEL_FORMAT_RGB_161616U:
    case CG_PIXEL_FORMAT_BGR_161616U:
    case CG_PIXEL_FORMAT_RGBA_16161616U:
    case CG_PIXEL_FORMAT_BGRA_16161616U:
        return GL_UNSIGNED_SHORT;

    case CG_PIXEL_FORMAT_A_16F:
    case CG_PIXEL_FORMAT_RG_1616F:
    case CG_PIXEL_FORMAT_RGB_161616F:
    case CG_PIXEL_FORMAT_BGR_161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F:
    case CG_PIXEL_FORMAT_BGRA_16161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F_PRE:
    case CG_PIXEL_FORMAT_BGRA_16161616F_PRE:
        return GL_HALF_FLOAT;

    case CG_PIXEL_FORMAT_A_32U:
    case CG_PIXEL_FORMAT_RG_3232U:
    case CG_PIXEL_FORMAT_RGB_323232U:
    case CG_PIXEL_FORMAT_BGR_323232U:
    case CG_PIXEL_FORMAT_RGBA_32323232U:
    case CG_PIXEL_FORMAT_BGRA_32323232U:
        return GL_UNSIGNED_INT;

    case CG_PIXEL_FORMAT_A_32F:
    case CG_PIXEL_FORMAT_RG_3232F:
    case CG_PIXEL_FORMAT_RGB_323232F:
    case CG_PIXEL_FORMAT_BGR_323232F:
    case CG_PIXEL_FORMAT_RGBA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F:
    case CG_PIXEL_FORMAT_RGBA_32323232F_PRE:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:
        return GL_FLOAT;

    case CG_PIXEL_FORMAT_RGB_565:
    case CG_PIXEL_FORMAT_RGBA_4444:
    case CG_PIXEL_FORMAT_RGBA_4444_PRE:
    case CG_PIXEL_FORMAT_RGBA_5551:
    case CG_PIXEL_FORMAT_RGBA_5551_PRE:
    case CG_PIXEL_FORMAT_RGBA_1010102:
    case CG_PIXEL_FORMAT_BGRA_1010102:
    case CG_PIXEL_FORMAT_ARGB_2101010:
    case CG_PIXEL_FORMAT_ABGR_2101010:
    case CG_PIXEL_FORMAT_RGBA_1010102_PRE:
    case CG_PIXEL_FORMAT_BGRA_1010102_PRE:
    case CG_PIXEL_FORMAT_ARGB_2101010_PRE:
    case CG_PIXEL_FORMAT_ABGR_2101010_PRE:
    case CG_PIXEL_FORMAT_DEPTH_16:
    case CG_PIXEL_FORMAT_DEPTH_32:
    case CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
    case CG_PIXEL_FORMAT_ANY:

        /* These should be handled carefully */
        c_assert_not_reached();
        return 0;
    }

    c_assert_not_reached();
    return 0;
}

static GLenum
_cg_pixel_format_get_internal_gles3_format(cg_pixel_format_t format)
{
    switch (format) {
    case CG_PIXEL_FORMAT_A_8:
        return GL_R8;
    case CG_PIXEL_FORMAT_A_8SN:
        return GL_R8_SNORM;
    case CG_PIXEL_FORMAT_A_16U:
        return GL_R16UI;
    case CG_PIXEL_FORMAT_A_16F:
        return GL_R16F;
    case CG_PIXEL_FORMAT_A_32U:
        return GL_R32UI;
    case CG_PIXEL_FORMAT_A_32F:
        return GL_R32F;
    case CG_PIXEL_FORMAT_RG_88:
        return GL_RG8;
    case CG_PIXEL_FORMAT_RG_88SN:
        return GL_RG8_SNORM;
    case CG_PIXEL_FORMAT_RG_1616U:
        return GL_RG16UI;
    case CG_PIXEL_FORMAT_RG_1616F:
        return GL_RG16F;
    case CG_PIXEL_FORMAT_RG_3232U:
        return GL_RG32UI;
    case CG_PIXEL_FORMAT_RG_3232F:
        return GL_RG32F;
    case CG_PIXEL_FORMAT_RGB_888:
        return GL_RGB8;
    case CG_PIXEL_FORMAT_RGB_888SN:
        return GL_RGB8_SNORM;
    case CG_PIXEL_FORMAT_RGB_161616U:
        return GL_RGB16UI;
    case CG_PIXEL_FORMAT_RGB_161616F:
        return GL_RGB16F;
    case CG_PIXEL_FORMAT_RGB_323232U:
        return GL_RGB32UI;
    case CG_PIXEL_FORMAT_RGB_323232F:
        return GL_RGB32F;
    case CG_PIXEL_FORMAT_BGR_888:
        return GL_RGB8;
    case CG_PIXEL_FORMAT_BGR_888SN:
        return GL_RGB8_SNORM;
    case CG_PIXEL_FORMAT_BGR_161616U:
        return GL_RGB16UI;
    case CG_PIXEL_FORMAT_BGR_161616F:
        return GL_RGB16F;
    case CG_PIXEL_FORMAT_BGR_323232U:
        return GL_RGB32F;
    case CG_PIXEL_FORMAT_BGR_323232F:
        return GL_RGB32F;
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_RGBA_8888_PRE:
        return GL_RGBA8;
    case CG_PIXEL_FORMAT_RGBA_8888SN:
        return GL_RGBA8_SNORM;
    case CG_PIXEL_FORMAT_RGBA_16161616U:
        return GL_RGBA16UI;
    case CG_PIXEL_FORMAT_RGBA_16161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F_PRE:
        return GL_RGBA16F;
    case CG_PIXEL_FORMAT_RGBA_32323232U:
        return GL_RGBA32UI;
    case CG_PIXEL_FORMAT_RGBA_32323232F:
    case CG_PIXEL_FORMAT_RGBA_32323232F_PRE:
        return GL_RGBA32F;
    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:
        return GL_RGBA8;
    case CG_PIXEL_FORMAT_BGRA_8888SN:
        return GL_RGBA8_SNORM;
    case CG_PIXEL_FORMAT_BGRA_16161616U:
        return GL_RGBA16UI;
    case CG_PIXEL_FORMAT_BGRA_16161616F:
    case CG_PIXEL_FORMAT_BGRA_16161616F_PRE:
        return GL_RGBA16F;
    case CG_PIXEL_FORMAT_BGRA_32323232U:
        return GL_RGBA32UI;
    case CG_PIXEL_FORMAT_BGRA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:
        return GL_RGBA32F;

    case CG_PIXEL_FORMAT_RGB_565:
    case CG_PIXEL_FORMAT_RGBA_4444:
    case CG_PIXEL_FORMAT_RGBA_4444_PRE:
    case CG_PIXEL_FORMAT_RGBA_5551:
    case CG_PIXEL_FORMAT_RGBA_5551_PRE:
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ABGR_8888:
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
    case CG_PIXEL_FORMAT_DEPTH_16:
    case CG_PIXEL_FORMAT_DEPTH_32:
    case CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
    case CG_PIXEL_FORMAT_ANY:
        /* These formats need to be handled carefully */
        c_assert_not_reached();
    }

    c_assert_not_reached();
    return GL_RGBA;
}

static cg_pixel_format_t
_cg_driver_pixel_format_to_gl(cg_device_t *dev,
                              cg_pixel_format_t format,
                              GLenum *out_glintformat,
                              GLenum *out_glformat,
                              GLenum *out_gltype)
{
    cg_pixel_format_t required_format;
    GLenum glintformat;
    GLenum glformat = 0;
    GLenum gltype;

    required_format = format;

    /* Find GL equivalents */
    switch (format) {
    case CG_PIXEL_FORMAT_A_8:
    case CG_PIXEL_FORMAT_A_8SN:
    case CG_PIXEL_FORMAT_A_16U:
    case CG_PIXEL_FORMAT_A_16F:
    case CG_PIXEL_FORMAT_A_32U:
    case CG_PIXEL_FORMAT_A_32F:

        if (dev->driver == CG_DRIVER_GLES2) {
            glformat = GL_ALPHA;
            glintformat = GL_ALPHA;
            gltype = GL_UNSIGNED_BYTE;
            required_format = CG_PIXEL_FORMAT_A_8;
        } else {
            /* We emulate alpha textures as red component textures with
             * a swizzle */
            glformat = GL_RED;
            glintformat = _cg_pixel_format_get_internal_gles3_format(format);
            gltype = _cg_pixel_format_get_gl_type(format);
        }
        break;

    case CG_PIXEL_FORMAT_RG_88:
    case CG_PIXEL_FORMAT_RG_88SN:
    case CG_PIXEL_FORMAT_RG_1616U:
    case CG_PIXEL_FORMAT_RG_1616F:
    case CG_PIXEL_FORMAT_RG_3232U:
    case CG_PIXEL_FORMAT_RG_3232F:

        if (dev->driver == CG_DRIVER_GLES2) {
            if (cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_RG)) {
                glformat = GL_RG;
                glintformat = GL_RG;
                required_format = CG_PIXEL_FORMAT_RG_88;
            } else {
                /* If red-green textures aren't supported then we'll use RGB
                 * as an internal format. Note this should only end up
                 * mattering for downloading the data because CGlib will
                 * refuse to allocate a texture with RG components if RG
                 * textures aren't supported */
                glintformat = GL_RGB;
                glformat = GL_RGB;
                required_format = CG_PIXEL_FORMAT_RGB_888;
            }
            gltype = GL_UNSIGNED_BYTE;
        } else {
            glformat = GL_RG;
            glintformat = _cg_pixel_format_get_internal_gles3_format(format);
            gltype = _cg_pixel_format_get_gl_type(format);
        }
        break;

    case CG_PIXEL_FORMAT_RGB_565:
        glintformat = GL_RGB;
        glformat = GL_RGB;
        gltype = GL_UNSIGNED_SHORT_5_6_5;
        break;

    case CG_PIXEL_FORMAT_RGB_888:
    case CG_PIXEL_FORMAT_RGB_888SN:
    case CG_PIXEL_FORMAT_RGB_161616U:
    case CG_PIXEL_FORMAT_RGB_161616F:
    case CG_PIXEL_FORMAT_RGB_323232U:
    case CG_PIXEL_FORMAT_RGB_323232F:

        if (dev->driver == CG_DRIVER_GLES2) {
            glformat = GL_RGB;
            glintformat = GL_RGB;
            gltype = GL_UNSIGNED_BYTE;
            required_format = CG_PIXEL_FORMAT_RGB_888;
        } else {
            glformat = GL_RGB;
            glintformat = _cg_pixel_format_get_internal_gles3_format(format);
            gltype = _cg_pixel_format_get_gl_type(format);
        }
        break;

    case CG_PIXEL_FORMAT_BGR_888:
    case CG_PIXEL_FORMAT_BGR_888SN:
    case CG_PIXEL_FORMAT_BGR_161616U:
    case CG_PIXEL_FORMAT_BGR_161616F:
    case CG_PIXEL_FORMAT_BGR_323232U:
    case CG_PIXEL_FORMAT_BGR_323232F:

        if (dev->driver == CG_DRIVER_GLES2) {
            /* Just one 24-bit ordering supported */
            glformat = GL_RGB;
            glintformat = GL_RGB;
            gltype = GL_UNSIGNED_BYTE;
            required_format = CG_PIXEL_FORMAT_RGB_888;
        } else {
            /* Only RGB order accepted by gles */
            glformat = GL_RGB;
            required_format = _cg_pixel_format_flip_rgb_order(format);
            glintformat = _cg_pixel_format_get_internal_gles3_format(required_format);
            gltype = _cg_pixel_format_get_gl_type(required_format);
        }
        break;

    case CG_PIXEL_FORMAT_RGBA_4444:
    case CG_PIXEL_FORMAT_RGBA_4444_PRE:
        glintformat = GL_RGBA;
        glformat = GL_RGBA;
        gltype = GL_UNSIGNED_SHORT_4_4_4_4;
        break;
    case CG_PIXEL_FORMAT_RGBA_5551:
    case CG_PIXEL_FORMAT_RGBA_5551_PRE:
        glintformat = GL_RGBA;
        glformat = GL_RGBA;
        gltype = GL_UNSIGNED_SHORT_5_5_5_1;
        break;

    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_RGBA_8888_PRE:
    case CG_PIXEL_FORMAT_RGBA_8888SN:
    case CG_PIXEL_FORMAT_RGBA_16161616U:
    case CG_PIXEL_FORMAT_RGBA_16161616F:
    case CG_PIXEL_FORMAT_RGBA_16161616F_PRE:
    case CG_PIXEL_FORMAT_RGBA_32323232U:
    case CG_PIXEL_FORMAT_RGBA_32323232F:
    case CG_PIXEL_FORMAT_RGBA_32323232F_PRE:

        if (dev->driver == CG_DRIVER_GLES2) {
            glintformat = GL_RGBA;
            glformat = GL_RGBA;
            gltype = GL_UNSIGNED_BYTE;
            required_format = CG_PIXEL_FORMAT_RGBA_8888;
        } else {
            glformat = GL_RGBA;
            glintformat = _cg_pixel_format_get_internal_gles3_format(format);
            gltype = _cg_pixel_format_get_gl_type(format);
        }
        break;

    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:

        /* There is an extension to support this format */
        if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_TEXTURE_FORMAT_BGRA8888)) {
            /* For some reason the extension says you have to specify
               BGRA for the internal format too */
            glintformat = GL_BGRA_EXT;
            glformat = GL_BGRA_EXT;
            gltype = GL_UNSIGNED_BYTE;
            break;
        }

        /* fall through */

    case CG_PIXEL_FORMAT_BGRA_8888SN:
    case CG_PIXEL_FORMAT_BGRA_16161616U:
    case CG_PIXEL_FORMAT_BGRA_16161616F:
    case CG_PIXEL_FORMAT_BGRA_16161616F_PRE:
    case CG_PIXEL_FORMAT_BGRA_32323232U:
    case CG_PIXEL_FORMAT_BGRA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:

        if (dev->driver == CG_DRIVER_GLES2) {
            glintformat = GL_RGBA;
            glformat = GL_RGBA;
            gltype = GL_UNSIGNED_BYTE;
            required_format = CG_PIXEL_FORMAT_RGBA_8888;
        } else {
            /* Only RGBA order accepted by gles */
            glformat = GL_RGBA;
            required_format = _cg_pixel_format_flip_rgb_order(format);
            glintformat = _cg_pixel_format_get_internal_gles3_format(required_format);
            gltype = _cg_pixel_format_get_gl_type(required_format);
        }
        break;

    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ARGB_8888_PRE:
    case CG_PIXEL_FORMAT_ABGR_8888:
    case CG_PIXEL_FORMAT_ABGR_8888_PRE:
    case CG_PIXEL_FORMAT_RGBA_1010102:
    case CG_PIXEL_FORMAT_RGBA_1010102_PRE:
    case CG_PIXEL_FORMAT_BGRA_1010102:
    case CG_PIXEL_FORMAT_BGRA_1010102_PRE:
    case CG_PIXEL_FORMAT_ABGR_2101010:
    case CG_PIXEL_FORMAT_ABGR_2101010_PRE:
    case CG_PIXEL_FORMAT_ARGB_2101010:
    case CG_PIXEL_FORMAT_ARGB_2101010_PRE:
        glintformat = GL_RGBA;
        glformat = GL_RGBA;
        gltype = GL_UNSIGNED_BYTE;
        required_format = CG_PIXEL_FORMAT_RGBA_8888;
        break;

    case CG_PIXEL_FORMAT_DEPTH_16:
        glintformat = GL_DEPTH_COMPONENT;
        glformat = GL_DEPTH_COMPONENT;
        gltype = GL_UNSIGNED_SHORT;
        break;
    case CG_PIXEL_FORMAT_DEPTH_32:
        glintformat = GL_DEPTH_COMPONENT;
        glformat = GL_DEPTH_COMPONENT;
        gltype = GL_UNSIGNED_INT;
        break;

    case CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
        glintformat = GL_DEPTH_STENCIL;
        glformat = GL_DEPTH_STENCIL;
        gltype = GL_UNSIGNED_INT_24_8;
        break;

    case CG_PIXEL_FORMAT_ANY:
        c_assert_not_reached();
        break;
    }

    /* Preserve the premult status of @format */
    if (_cg_pixel_format_can_be_premultiplied(required_format))
    {
        required_format = _cg_pixel_format_premult_stem(required_format);
        if (_cg_pixel_format_is_premultiplied(format))
            required_format = _cg_pixel_format_premultiply(required_format);
    }

    /* All of the pixel formats are handled above so if this hits then
       we've been given an invalid pixel format */
    c_assert(glformat != 0);

    if (out_glintformat != NULL)
        *out_glintformat = glintformat;
    if (out_glformat != NULL)
        *out_glformat = glformat;
    if (out_gltype != NULL)
        *out_gltype = gltype;

    return required_format;
}

static bool
_cg_get_gl_version(cg_device_t *dev, int *major_out, int *minor_out)
{
    const char *version_string;

    /* Get the OpenGL version number */
    if ((version_string = _cg_device_get_gl_version(dev)) == NULL)
        return false;

    if (!c_str_has_prefix(version_string, "OpenGL ES "))
        return false;

    return _cg_gl_util_parse_gl_version(
        version_string + 10, major_out, minor_out);
}

static bool
_cg_driver_update_features(cg_device_t *dev,
                           cg_error_t **error)
{
    unsigned long private_features
    [CG_FLAGS_N_LONGS_FOR_SIZE(CG_N_PRIVATE_FEATURES)] = { 0 };
    char **gl_extensions;
    int gl_major, gl_minor;
    int i;

    /* We have to special case getting the pointer to the glGetString
       function because we need to use it to determine what functions we
       can expect */
    dev->glGetString = (void *)_cg_renderer_get_proc_address(dev->display->renderer,
                                                             "glGetString",
                                                             true);

    gl_extensions = _cg_device_get_gl_extensions(dev);

    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_WINSYS))) {
        char *all_extensions = c_strjoinv(" ", gl_extensions);

        CG_NOTE(WINSYS,
                "Checking features\n"
                "  GL_VENDOR: %s\n"
                "  GL_RENDERER: %s\n"
                "  GL_VERSION: %s\n"
                "  GL_EXTENSIONS: %s",
                dev->glGetString(GL_VENDOR),
                dev->glGetString(GL_RENDERER),
                _cg_device_get_gl_version(dev),
                all_extensions);

        c_free(all_extensions);
    }

    dev->glsl_major = 1;
    dev->glsl_minor = 0;
    dev->glsl_version_to_use = 100;

    _cg_gpc_info_init(dev, &dev->gpu);

    if (!_cg_get_gl_version(dev, &gl_major, &gl_minor)) {
        gl_major = 1;
        gl_minor = 1;
    }

    _cg_feature_check_ext_functions(dev, gl_major, gl_minor, gl_extensions);

    /* Note GLES 2 core doesn't support mipmaps for npot textures or
     * repeat modes other than CLAMP_TO_EDGE. */
    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_GLSL, true);
    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_BASIC, true);
    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_MIRRORED_REPEAT, true);
    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_PER_VERTEX_POINT_SIZE,
                 true);

    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_BLEND_CONSTANT, true);

    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_VBOS, true);
    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_ANY_GL, true);
    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_ALPHA_TEXTURES, true);

    /* GLES 2.0 supports point sprites in core */
    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_POINT_SPRITE, true);

    if (dev->glBlitFramebuffer)
        CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_OFFSCREEN_BLIT, true);

    if (_cg_check_extension("GL_OES_element_index_uint", gl_extensions))
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_UNSIGNED_INT_INDICES, true);

#ifdef HAVE_CG_WEBGL
    if (_cg_check_extension("GL_WEBGL_depth_texture", gl_extensions))
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_DEPTH_TEXTURE, true);
#else
    if (_cg_check_extension("GL_OES_depth_texture", gl_extensions))
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_DEPTH_TEXTURE, true);
#endif

    if (_cg_check_extension("GL_OES_texture_npot", gl_extensions)) {
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT, true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_BASIC, true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_MIPMAP, true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_REPEAT, true);
    } else if (_cg_check_extension("GL_IMG_texture_npot", gl_extensions)) {
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_BASIC, true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_MIPMAP, true);
    }

    if (dev->glTexImage3D)
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_3D, true);

    if (dev->glMapBuffer)
        /* The GL_OES_mapbuffer extension doesn't support mapping for
           read */
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_MAP_BUFFER_FOR_WRITE, true);

    if (dev->glEGLImageTargetTexture2D)
        CG_FLAGS_SET(private_features,
                     CG_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE,
                     true);

    if (_cg_check_extension("GL_OES_packed_depth_stencil", gl_extensions))
        CG_FLAGS_SET(private_features,
                     CG_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL,
                     true);

    if (_cg_check_extension("GL_EXT_texture_format_BGRA8888", gl_extensions))
        CG_FLAGS_SET(
            private_features, CG_PRIVATE_FEATURE_TEXTURE_FORMAT_BGRA8888, true);

    if (_cg_check_extension("GL_EXT_unpack_subimage", gl_extensions))
        CG_FLAGS_SET(
            private_features, CG_PRIVATE_FEATURE_UNPACK_SUBIMAGE, true);

    /* A nameless vendor implemented the extension, but got the case wrong
     * per the spec. */
    if (_cg_check_extension("GL_OES_EGL_sync", gl_extensions) ||
        _cg_check_extension("GL_OES_egl_sync", gl_extensions))
        CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_OES_EGL_SYNC, true);

    if (_cg_check_extension("GL_EXT_texture_rg", gl_extensions))
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_RG, true);

    if (dev->glDrawArraysInstanced)
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_INSTANCES, true);

    /* Cache features */
    for (i = 0; i < C_N_ELEMENTS(private_features); i++)
        dev->private_features[i] |= private_features[i];

    c_strfreev(gl_extensions);

    return true;
}

const cg_driver_vtable_t _cg_driver_gles = {
    _cg_driver_pixel_format_from_gl_internal,
    _cg_driver_pixel_format_to_gl,
    _cg_driver_update_features,
    _cg_offscreen_gl_allocate,
    _cg_offscreen_gl_free,
    _cg_framebuffer_gl_flush_state,
    _cg_framebuffer_gl_clear,
    _cg_framebuffer_gl_query_bits,
    _cg_framebuffer_gl_finish,
    _cg_framebuffer_gl_discard_buffers,
    _cg_framebuffer_gl_draw_attributes,
    _cg_framebuffer_gl_draw_indexed_attributes,
    _cg_framebuffer_gl_read_pixels_into_bitmap,
    _cg_texture_2d_gl_free,
    _cg_texture_2d_gl_can_create,
    _cg_texture_2d_gl_init,
    _cg_texture_2d_gl_allocate,
    _cg_texture_2d_gl_copy_from_framebuffer,
    _cg_texture_2d_gl_get_gl_handle,
    _cg_texture_2d_gl_generate_mipmap,
    _cg_texture_2d_gl_copy_from_bitmap,
    NULL, /* texture_2d_get_data */
    _cg_gl_flush_attributes_state,
    _cg_clip_stack_gl_flush,
    _cg_buffer_gl_create,
    _cg_buffer_gl_destroy,
    _cg_buffer_gl_map_range,
    _cg_buffer_gl_unmap,
    _cg_buffer_gl_set_data,
};
