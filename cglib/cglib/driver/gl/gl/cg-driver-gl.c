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

#include "cg-private.h"
#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-feature-private.h"
#include "cg-renderer-private.h"
#include "cg-error-private.h"
#include "cg-framebuffer-gl-private.h"
#include "cg-texture-2d-gl-private.h"
#include "cg-attribute-gl-private.h"
#include "cg-clip-stack-gl-private.h"
#include "cg-buffer-gl-private.h"

static bool
_cg_driver_pixel_format_from_gl_internal(cg_device_t *dev,
                                         GLenum gl_int_format,
                                         cg_pixel_format_t *out_format)
{
    /* It doesn't really matter we convert to exact same
       format (some have no cg match anyway) since format
       is re-matched against cg when getting or setting
       texture image data.
     */

    switch (gl_int_format) {
    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
    /* CGlib only supports one single-component texture so if we have
     * ended up with a red texture then it is probably being used as
     * a component-alpha texture */
    case GL_RED:

        *out_format = CG_PIXEL_FORMAT_A_8;
        return true;

    case GL_RG:
        *out_format = CG_PIXEL_FORMAT_RG_88;
        return true;

    case GL_RGB:
    case GL_RGB4:
    case GL_RGB5:
    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16:
    case GL_R3_G3_B2:

        *out_format = CG_PIXEL_FORMAT_RGB_888;
        return true;

    case GL_RGBA:
    case GL_RGBA2:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGBA8:
    case GL_RGB10_A2:
    case GL_RGBA12:
    case GL_RGBA16:

        *out_format = CG_PIXEL_FORMAT_RGBA_8888;
        return true;
    }

    return false;
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
_cg_pixel_format_get_internal_gl_format(cg_pixel_format_t format)
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

        /* We emulate alpha textures as red component textures with
         * a swizzle */
        glformat = GL_RED;
        glintformat = _cg_pixel_format_get_internal_gl_format(format);
        gltype = _cg_pixel_format_get_gl_type(format);
        break;

    case CG_PIXEL_FORMAT_RG_88:
    case CG_PIXEL_FORMAT_RG_88SN:
    case CG_PIXEL_FORMAT_RG_1616U:
    case CG_PIXEL_FORMAT_RG_1616F:
    case CG_PIXEL_FORMAT_RG_3232U:
    case CG_PIXEL_FORMAT_RG_3232F:
        glformat = GL_RG;
        glintformat = _cg_pixel_format_get_internal_gl_format(format);
        gltype = _cg_pixel_format_get_gl_type(format);
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
        glformat = GL_RGB;
        glintformat = _cg_pixel_format_get_internal_gl_format(format);
        gltype = _cg_pixel_format_get_gl_type(format);
        break;

    case CG_PIXEL_FORMAT_BGR_888:
    case CG_PIXEL_FORMAT_BGR_888SN:
    case CG_PIXEL_FORMAT_BGR_161616U:
    case CG_PIXEL_FORMAT_BGR_161616F:
    case CG_PIXEL_FORMAT_BGR_323232U:
    case CG_PIXEL_FORMAT_BGR_323232F:
        glformat = GL_BGR;
        glintformat = _cg_pixel_format_get_internal_gl_format(format);
        gltype = _cg_pixel_format_get_gl_type(format);
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
        glformat = GL_RGBA;
        glintformat = _cg_pixel_format_get_internal_gl_format(format);
        gltype = _cg_pixel_format_get_gl_type(format);
        break;

    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:
    case CG_PIXEL_FORMAT_BGRA_8888SN:
    case CG_PIXEL_FORMAT_BGRA_16161616U:
    case CG_PIXEL_FORMAT_BGRA_16161616F:
    case CG_PIXEL_FORMAT_BGRA_16161616F_PRE:
    case CG_PIXEL_FORMAT_BGRA_32323232U:
    case CG_PIXEL_FORMAT_BGRA_32323232F:
    case CG_PIXEL_FORMAT_BGRA_32323232F_PRE:
        glformat = GL_BGRA;
        glintformat = _cg_pixel_format_get_internal_gl_format(format);
        gltype = _cg_pixel_format_get_gl_type(format);
        break;

    /* The following two types of channel ordering
     * have no GL equivalent unless defined using
     * system word byte ordering */
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ARGB_8888_PRE:
        glintformat = GL_RGBA;
        glformat = GL_BGRA;
#if C_BYTE_ORDER == C_LITTLE_ENDIAN
        gltype = GL_UNSIGNED_INT_8_8_8_8;
#else
        gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
        break;

    case CG_PIXEL_FORMAT_ABGR_8888:
    case CG_PIXEL_FORMAT_ABGR_8888_PRE:
        glintformat = GL_RGBA;
        glformat = GL_RGBA;
#if C_BYTE_ORDER == C_LITTLE_ENDIAN
        gltype = GL_UNSIGNED_INT_8_8_8_8;
#else
        gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
        break;

    case CG_PIXEL_FORMAT_RGBA_1010102:
    case CG_PIXEL_FORMAT_RGBA_1010102_PRE:
        glintformat = GL_RGBA;
        glformat = GL_RGBA;
        gltype = GL_UNSIGNED_INT_10_10_10_2;
        break;

    case CG_PIXEL_FORMAT_BGRA_1010102:
    case CG_PIXEL_FORMAT_BGRA_1010102_PRE:
        glintformat = GL_RGBA;
        glformat = GL_BGRA;
        gltype = GL_UNSIGNED_INT_10_10_10_2;
        break;

    case CG_PIXEL_FORMAT_ABGR_2101010:
    case CG_PIXEL_FORMAT_ABGR_2101010_PRE:
        glintformat = GL_RGBA;
        glformat = GL_RGBA;
        gltype = GL_UNSIGNED_INT_2_10_10_10_REV;
        break;

    case CG_PIXEL_FORMAT_ARGB_2101010:
    case CG_PIXEL_FORMAT_ARGB_2101010_PRE:
        glintformat = GL_RGBA;
        glformat = GL_BGRA;
        gltype = GL_UNSIGNED_INT_2_10_10_10_REV;
        break;

    case CG_PIXEL_FORMAT_DEPTH_16:
        glintformat = GL_DEPTH_COMPONENT16;
        glformat = GL_DEPTH_COMPONENT;
        gltype = GL_UNSIGNED_SHORT;
        break;
    case CG_PIXEL_FORMAT_DEPTH_32:
        glintformat = GL_DEPTH_COMPONENT32;
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

    return _cg_gl_util_parse_gl_version(version_string, major_out, minor_out);
}

static bool
check_gl_version(cg_device_t *dev, char **gl_extensions, cg_error_t **error)
{
    int major, minor;

    if (!_cg_get_gl_version(dev, &major, &minor)) {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_UNKNOWN_VERSION,
                      "The OpenGL version could not be determined");
        return false;
    }

    /* GL 1.3 supports all of the required functionality in core */
    if (CG_CHECK_GL_VERSION(major, minor, 1, 3))
        return true;

    /* OpenGL 1.2 is only supported if we have the multitexturing
       extension */
    if (!_cg_check_extension("GL_ARB_multitexture", gl_extensions)) {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_INVALID_VERSION,
                      "The OpenGL driver is missing "
                      "the GL_ARB_multitexture extension");
        return false;
    }

    /* OpenGL 1.2 is required */
    if (!CG_CHECK_GL_VERSION(major, minor, 1, 2)) {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_INVALID_VERSION,
                      "The OpenGL version of your driver (%i.%i) "
                      "is not compatible with CGlib",
                      major,
                      minor);
        return false;
    }

    return true;
}

static bool
_cg_driver_update_features(cg_device_t *dev, cg_error_t **error)
{
    unsigned long private_features
    [CG_FLAGS_N_LONGS_FOR_SIZE(CG_N_PRIVATE_FEATURES)] = { 0 };
    char **gl_extensions;
    int gl_major = 0, gl_minor = 0;
    int min_glsl_major = 0, min_glsl_minor = 0;
    int i;

    /* We have to special case getting the pointer to the glGetString*
       functions because we need to use them to determine what functions
       we can expect */
    dev->glGetString = (void *)_cg_renderer_get_proc_address(dev->display->renderer,
                                                             "glGetString",
                                                             true);
    dev->glGetStringi = (void *)_cg_renderer_get_proc_address(dev->display->renderer,
                                                              "glGetStringi",
                                                              true);
    dev->glGetIntegerv = (void *)_cg_renderer_get_proc_address(dev->display->renderer,
                                                               "glGetIntegerv",
                                                               true);

    gl_extensions = _cg_device_get_gl_extensions(dev);

    if (!check_gl_version(dev, gl_extensions, error))
        return false;

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

    _cg_get_gl_version(dev, &gl_major, &gl_minor);

    _cg_gpc_info_init(dev, &dev->gpu);

    dev->glsl_major = 1;
    dev->glsl_minor = 1;

    if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 2, 0)) {
        const char *glsl_version =
            (char *)dev->glGetString(GL_SHADING_LANGUAGE_VERSION);
        _cg_gl_util_parse_gl_version(
            glsl_version, &dev->glsl_major, &dev->glsl_minor);
    }

    if (gl_major < 3) {
        min_glsl_major = 1;
        min_glsl_minor = 1;

        if (CG_CHECK_GL_VERSION(dev->glsl_major, dev->glsl_minor, 1, 2)) {
            /* We want to use version 120 if it is available so that the
             * gl_PointCoord can be used. */
            min_glsl_major = 1;
            min_glsl_minor = 2;
        }
    } else {
        /* When we're using GL 3 we always ask for a 3.1 core profile
         * context which corresponds to supporting glsl >= 1.3 */
        min_glsl_major = 1;
        min_glsl_minor = 3;
    }

    dev->glsl_version_to_use = min_glsl_major * 100 + min_glsl_minor * 10;

    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_UNSIGNED_INT_INDICES, true);

    if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 1, 4))
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_MIRRORED_REPEAT, true);

    _cg_feature_check_ext_functions(dev, gl_major, gl_minor, gl_extensions);

    if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 2, 0) ||
        _cg_check_extension("GL_ARB_texture_non_power_of_two", gl_extensions)) {
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT, true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_BASIC, true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_MIPMAP, true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_NPOT_REPEAT, true);
    }

    if (_cg_check_extension("GL_MESA_pack_invert", gl_extensions))
        CG_FLAGS_SET(
            private_features, CG_PRIVATE_FEATURE_MESA_PACK_INVERT, true);

    if (!dev->glGenRenderbuffers) {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
                      "Framebuffer Object support is required to "
                      "use the OpenGL driver");
        return false;
    }
    CG_FLAGS_SET(
        private_features, CG_PRIVATE_FEATURE_QUERY_FRAMEBUFFER_BITS, true);

    if (dev->glBlitFramebuffer)
        CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_OFFSCREEN_BLIT, true);

    if (dev->glRenderbufferStorageMultisampleIMG)
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_OFFSCREEN_MULTISAMPLE,
                     true);

    if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 3, 0) ||
        _cg_check_extension("GL_ARB_depth_texture", gl_extensions))
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_DEPTH_TEXTURE, true);

    if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 2, 1) ||
        _cg_check_extension("GL_EXT_pixel_buffer_object", gl_extensions))
        CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_PBOS, true);

    if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 1, 4) ||
        _cg_check_extension("GL_EXT_blend_color", gl_extensions))
        CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_BLEND_CONSTANT, true);

    if (!dev->glCreateProgram) {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
                      "GLSL support is required to use the OpenGL driver");
        return false;
    }
    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_GLSL, true);

    if ((CG_CHECK_GL_VERSION(gl_major, gl_minor, 2, 0) ||
         _cg_check_extension("GL_ARB_point_sprite", gl_extensions)) &&

        /* If GLSL is supported then we only enable point sprite support
         * too if we have glsl >= 1.2 otherwise we don't have the
         * gl_PointCoord builtin which we depend on in the glsl backend.
         */
        (!CG_FLAGS_GET(dev->features, CG_FEATURE_ID_GLSL) ||
         CG_CHECK_GL_VERSION(dev->glsl_major, dev->glsl_minor, 1, 2))) {
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_POINT_SPRITE, true);
    }

    if (dev->glGenBuffers) {
        CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_VBOS, true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_MAP_BUFFER_FOR_READ, true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_MAP_BUFFER_FOR_WRITE, true);
    }

    if (dev->glTexImage3D)
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_3D, true);

    if (dev->glEGLImageTargetTexture2D)
        CG_FLAGS_SET(private_features,
                     CG_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE,
                     true);

    if (_cg_check_extension("GL_EXT_packed_depth_stencil", gl_extensions))
        CG_FLAGS_SET(private_features,
                     CG_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL,
                     true);

    if (!dev->glGenSamplers) {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
                      "Sampler Object support is required to use the "
                      "OpenGL driver");
        return false;
    }
    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_SAMPLER_OBJECTS, true);

    if (!CG_CHECK_GL_VERSION(gl_major, gl_minor, 3, 3) &&
        !(_cg_check_extension("GL_ARB_texture_swizzle", gl_extensions) ||
         _cg_check_extension("GL_EXT_texture_swizzle", gl_extensions)))
    {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
                      "The GL_ARB_texture_swizzle or GL_EXT_texture_swizzle "
                      "extension is required to use the OpenGL driver");
        return false;
    }
    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_TEXTURE_SWIZZLE, true);

    /* The per-vertex point size is only available via GLSL with the
     * gl_PointSize builtin. This is only available in GL 2.0 (not the
     * GLSL extensions) */
    if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 2, 0)) {
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_PER_VERTEX_POINT_SIZE,
                     true);
        CG_FLAGS_SET(private_features,
                     CG_PRIVATE_FEATURE_ENABLE_PROGRAM_POINT_SIZE,
                     true);
    }

    if (dev->driver == CG_DRIVER_GL) {
        /* Not available in GL 3 */
        CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_QUADS, true);
    }

    CG_FLAGS_SET(
        private_features, CG_PRIVATE_FEATURE_READ_PIXELS_ANY_FORMAT, true);
    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_ANY_GL, true);
    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_FORMAT_CONVERSION, true);
    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_BLEND_CONSTANT, true);
    CG_FLAGS_SET(
        private_features, CG_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM, true);
    CG_FLAGS_SET(
        private_features, CG_PRIVATE_FEATURE_QUERY_TEXTURE_PARAMETERS, true);
    CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_TEXTURE_MAX_LEVEL, true);

    if (dev->glFenceSync)
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_FENCE, true);

    if (dev->glDrawArraysInstanced)
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_INSTANCES, true);

    if (!CG_CHECK_GL_VERSION(gl_major, gl_minor, 3, 0) &&
        !_cg_check_extension("GL_ARB_texture_rg", gl_extensions))
    {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
                      "The GL_ARB_texture_rg extension is required "
                      "to use the OpenGL driver");
        return false;
    }

    /* Cache features */
    for (i = 0; i < C_N_ELEMENTS(private_features); i++)
        dev->private_features[i] |= private_features[i];

    c_strfreev(gl_extensions);

    return true;
}

const cg_driver_vtable_t _cg_driver_gl = {
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
    _cg_texture_2d_gl_get_data,
    _cg_gl_flush_attributes_state,
    _cg_clip_stack_gl_flush,
    _cg_buffer_gl_create,
    _cg_buffer_gl_destroy,
    _cg_buffer_gl_map_range,
    _cg_buffer_gl_unmap,
    _cg_buffer_gl_set_data,
};
