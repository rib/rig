/*
 * Cogl
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-private.h"
#include "cogl-device-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-feature-private.h"
#include "cogl-renderer-private.h"
#include "cogl-error-private.h"
#include "cogl-framebuffer-gl-private.h"
#include "cogl-texture-2d-gl-private.h"
#include "cogl-attribute-gl-private.h"
#include "cogl-clip-stack-gl-private.h"
#include "cogl-buffer-gl-private.h"

static bool
_cg_driver_pixel_format_from_gl_internal(cg_device_t *dev,
                                         GLenum gl_int_format,
                                         cg_pixel_format_t *out_format)
{
    /* It doesn't really matter we convert to exact same
       format (some have no cogl match anyway) since format
       is re-matched against cogl when getting or setting
       texture image data.
     */

    switch (gl_int_format) {
    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
    /* Cogl only supports one single-component texture so if we have
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
        /* If the driver doesn't natively support alpha textures then we
         * will use a red component texture with a swizzle to implement
         * the texture */
        if (_cg_has_private_feature(dev,
                                    CG_PRIVATE_FEATURE_ALPHA_TEXTURES) == 0) {
            glintformat = GL_RED;
            glformat = GL_RED;
        } else {
            glintformat = GL_ALPHA;
            glformat = GL_ALPHA;
        }
        gltype = GL_UNSIGNED_BYTE;
        break;

    case CG_PIXEL_FORMAT_RG_88:
        if (cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_RG)) {
            glintformat = GL_RG;
            glformat = GL_RG;
        } else {
            /* If red-green textures aren't supported then we'll use RGB
             * as an internal format. Note this should only end up
             * mattering for downloading the data because Cogl will
             * refuse to allocate a texture with RG components if RG
             * textures aren't supported */
            glintformat = GL_RGB;
            glformat = GL_RGB;
            required_format = CG_PIXEL_FORMAT_RGB_888;
        }
        gltype = GL_UNSIGNED_BYTE;
        break;

    case CG_PIXEL_FORMAT_RGB_888:
        glintformat = GL_RGB;
        glformat = GL_RGB;
        gltype = GL_UNSIGNED_BYTE;
        break;
    case CG_PIXEL_FORMAT_BGR_888:
        glintformat = GL_RGB;
        glformat = GL_BGR;
        gltype = GL_UNSIGNED_BYTE;
        break;
    case CG_PIXEL_FORMAT_RGBA_8888:
    case CG_PIXEL_FORMAT_RGBA_8888_PRE:
        glintformat = GL_RGBA;
        glformat = GL_RGBA;
        gltype = GL_UNSIGNED_BYTE;
        break;
    case CG_PIXEL_FORMAT_BGRA_8888:
    case CG_PIXEL_FORMAT_BGRA_8888_PRE:
        glintformat = GL_RGBA;
        glformat = GL_BGRA;
        gltype = GL_UNSIGNED_BYTE;
        break;

    /* The following two types of channel ordering
     * have no GL equivalent unless defined using
     * system word byte ordering */
    case CG_PIXEL_FORMAT_ARGB_8888:
    case CG_PIXEL_FORMAT_ARGB_8888_PRE:
        glintformat = GL_RGBA;
        glformat = GL_BGRA;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        gltype = GL_UNSIGNED_INT_8_8_8_8;
#else
        gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
        break;

    case CG_PIXEL_FORMAT_ABGR_8888:
    case CG_PIXEL_FORMAT_ABGR_8888_PRE:
        glintformat = GL_RGBA;
        glformat = GL_RGBA;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
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

    /* The following three types of channel ordering
     * are always defined using system word byte
     * ordering (even according to GLES spec) */
    case CG_PIXEL_FORMAT_RGB_565:
        glintformat = GL_RGB;
        glformat = GL_RGB;
        gltype = GL_UNSIGNED_SHORT_5_6_5;
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
                      "is not compatible with Cogl",
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
    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_DEPTH_RANGE, true);

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

    if (dev->glGenRenderbuffers) {
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_OFFSCREEN, true);
        CG_FLAGS_SET(
            private_features, CG_PRIVATE_FEATURE_QUERY_FRAMEBUFFER_BITS, true);
    }

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

    if (dev->glCreateProgram)
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_GLSL, true);
    else {
        /* If all of the old GLSL extensions are available then we can fake
        * the GL 2.0 GLSL support by diverting to the old function names */
        if (dev->glCreateProgramObject && /* GL_ARB_shader_objects */
            dev->glVertexAttribPointer && /* GL_ARB_vertex_shader */
            _cg_check_extension("GL_ARB_fragment_shader", gl_extensions)) {
            dev->glCreateShader = dev->glCreateShaderObject;
            dev->glCreateProgram = dev->glCreateProgramObject;
            dev->glDeleteShader = dev->glDeleteObject;
            dev->glDeleteProgram = dev->glDeleteObject;
            dev->glAttachShader = dev->glAttachObject;
            dev->glUseProgram = dev->glUseProgramObject;
            dev->glGetProgramInfoLog = dev->glGetInfoLog;
            dev->glGetShaderInfoLog = dev->glGetInfoLog;
            dev->glGetShaderiv = dev->glGetObjectParameteriv;
            dev->glGetProgramiv = dev->glGetObjectParameteriv;
            dev->glDetachShader = dev->glDetachObject;
            dev->glGetAttachedShaders = dev->glGetAttachedObjects;
            /* FIXME: there doesn't seem to be an equivalent for glIsShader
             * and glIsProgram. This doesn't matter for now because Cogl
             * doesn't use these but if we add support for simulating a
             * GLES2 context on top of regular GL then we'll need to do
             * something here */

            CG_FLAGS_SET(dev->features, CG_FEATURE_ID_GLSL, true);
        }
    }

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

    if (dev->glGenSamplers)
        CG_FLAGS_SET(
            private_features, CG_PRIVATE_FEATURE_SAMPLER_OBJECTS, true);

    if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 3, 3) ||
        _cg_check_extension("GL_ARB_texture_swizzle", gl_extensions) ||
        _cg_check_extension("GL_EXT_texture_swizzle", gl_extensions))
        CG_FLAGS_SET(
            private_features, CG_PRIVATE_FEATURE_TEXTURE_SWIZZLE, true);

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
        /* Features which are not available in GL 3 */
        CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_QUADS, true);
        CG_FLAGS_SET(private_features, CG_PRIVATE_FEATURE_ALPHA_TEXTURES, true);
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

    if (CG_CHECK_GL_VERSION(gl_major, gl_minor, 3, 0) ||
        _cg_check_extension("GL_ARB_texture_rg", gl_extensions))
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_TEXTURE_RG, true);

    /* Cache features */
    for (i = 0; i < C_N_ELEMENTS(private_features); i++)
        dev->private_features[i] |= private_features[i];

    c_strfreev(gl_extensions);

    if (!CG_FLAGS_GET(private_features, CG_PRIVATE_FEATURE_ALPHA_TEXTURES) &&
        !CG_FLAGS_GET(private_features, CG_PRIVATE_FEATURE_TEXTURE_SWIZZLE)) {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
                      "The GL_ARB_texture_swizzle extension is required "
                      "to use the GL3 driver");
        return false;
    }

    if (!CG_FLAGS_GET(dev->features, CG_FEATURE_ID_OFFSCREEN)) {
        _cg_set_error(error,
                      CG_DRIVER_ERROR,
                      CG_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
                      "Cogl requires framebuffer object support "
                      "to use the GL driver");
        return false;
    }

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
