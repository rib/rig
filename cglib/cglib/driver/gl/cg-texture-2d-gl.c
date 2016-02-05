/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009,2010,2011,2012 Intel Corporation.
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
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include <string.h>

#include "cg-private.h"
#include "cg-texture-private.h"
#include "cg-texture-2d-gl.h"
#include "cg-texture-2d-gl-private.h"
#include "cg-texture-2d-private.h"
#include "cg-texture-gl-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-error-private.h"
#include "cg-util-gl-private.h"
#include "cg-webgl-private.h"

#define GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL 0x9241

void
_cg_texture_2d_gl_free(cg_texture_2d_t *tex_2d)
{
    if (!tex_2d->is_foreign && tex_2d->gl_texture) {
        cg_texture_t *tex = CG_TEXTURE(tex_2d);
        cg_device_t *dev = tex->dev;

        _cg_delete_gl_texture(dev, tex_2d->gl_texture);
    }
}

bool
_cg_texture_2d_gl_can_create(cg_device_t *dev,
                             int width,
                             int height,
                             cg_pixel_format_t internal_format)
{
    GLenum gl_intformat;
    GLenum gl_format;
    GLenum gl_type;

#if defined(C_PLATFORM_WEB)
    /* XXX: Although webgl has limited supported from NOP textures, this
     * is a hack to help find places to try and convert over to using
     * NPOT textures... */
    if (!(_cg_util_is_pot(width) && _cg_util_is_pot(height))) {
        c_warning("WARNING: NPOT texture: width=%d height=%d", width, height);
        _c_web_console_trace();
    }
#endif

    /* If NPOT textures aren't supported then the size must be a power
       of two */
    if (!cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_NPOT_BASIC) &&
        (!_cg_util_is_pot(width) || !_cg_util_is_pot(height)))
        return false;

    dev->driver_vtable->pixel_format_to_gl(dev, internal_format,
                                           &gl_intformat, &gl_format,
                                           &gl_type);

    /* Check that the driver can create a texture with that size */
    if (!dev->texture_driver->size_supported(dev,
                                             GL_TEXTURE_2D,
                                             gl_intformat,
                                             gl_format,
                                             gl_type,
                                             width,
                                             height))
        return false;

    return true;
}

void
_cg_texture_2d_gl_init(cg_texture_2d_t *tex_2d)
{
    tex_2d->gl_texture = 0;

    /* We default to GL_LINEAR for both filters */
    tex_2d->gl_legacy_texobj_min_filter = GL_LINEAR;
    tex_2d->gl_legacy_texobj_mag_filter = GL_LINEAR;

    /* Wrap mode not yet set */
    tex_2d->gl_legacy_texobj_wrap_mode_s = false;
    tex_2d->gl_legacy_texobj_wrap_mode_t = false;
}

static bool
allocate_with_size(cg_texture_2d_t *tex_2d,
                   cg_texture_loader_t *loader,
                   cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(tex_2d);
    cg_pixel_format_t internal_format;
    int width = loader->src.sized.width;
    int height = loader->src.sized.height;
    cg_device_t *dev = tex->dev;
    GLenum gl_intformat;
    GLenum gl_format;
    GLenum gl_type;
    GLenum gl_error;
    GLenum gl_texture;

    internal_format =
        _cg_texture_determine_internal_format(tex, CG_PIXEL_FORMAT_ANY);

    if (!_cg_texture_2d_gl_can_create(dev, width, height, internal_format)) {
        _cg_set_error(error,
                      CG_TEXTURE_ERROR,
                      CG_TEXTURE_ERROR_SIZE,
                      "Failed to create texture 2d due to size/format"
                      " constraints");
        return false;
    }

    dev->driver_vtable->pixel_format_to_gl(dev, internal_format,
                                           &gl_intformat, &gl_format,
                                           &gl_type);

    gl_texture = dev->texture_driver->gen(dev, GL_TEXTURE_2D,
                                          internal_format);

    tex_2d->gl_internal_format = gl_intformat;

    _cg_bind_gl_texture_transient(
        GL_TEXTURE_2D, gl_texture, tex_2d->is_foreign);

    /* Clear any GL errors */
    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;

    dev->glTexImage2D(GL_TEXTURE_2D,
                      0,
                      gl_intformat,
                      width,
                      height,
                      0,
                      gl_format,
                      gl_type,
                      NULL);

    if (_cg_gl_util_catch_out_of_memory(dev, error)) {
        GE(dev, glDeleteTextures(1, &gl_texture));
        return false;
    }

    tex_2d->gl_texture = gl_texture;
    tex_2d->gl_internal_format = gl_intformat;

    tex_2d->internal_format = internal_format;

    _cg_texture_set_allocated(tex, internal_format, width, height);

    return true;
}

static bool
allocate_from_bitmap(cg_texture_2d_t *tex_2d,
                     cg_texture_loader_t *loader,
                     cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(tex_2d);
    cg_bitmap_t *bmp = loader->src.bitmap.bitmap;
    cg_device_t *dev = _cg_bitmap_get_context(bmp);
    cg_pixel_format_t internal_format;
    int width = cg_bitmap_get_width(bmp);
    int height = cg_bitmap_get_height(bmp);
    bool can_convert_in_place = loader->src.bitmap.can_convert_in_place;
    cg_bitmap_t *upload_bmp;
    GLenum gl_intformat;
    GLenum gl_format;
    GLenum gl_type;
    GLenum gl_texture;

    internal_format =
        _cg_texture_determine_internal_format(tex, cg_bitmap_get_format(bmp));

    if (!_cg_texture_2d_gl_can_create(dev, width, height, internal_format)) {
        _cg_set_error(error,
                      CG_TEXTURE_ERROR,
                      CG_TEXTURE_ERROR_SIZE,
                      "Failed to create texture 2d due to size/format"
                      " constraints");
        return false;
    }

    upload_bmp = _cg_bitmap_convert_for_upload(
        bmp, internal_format, can_convert_in_place, error);
    if (upload_bmp == NULL)
        return false;

    dev->driver_vtable->pixel_format_to_gl(dev,
                                           cg_bitmap_get_format(upload_bmp),
                                           NULL, /* internal format */
                                           &gl_format,
                                           &gl_type);
    dev->driver_vtable->pixel_format_to_gl(dev, internal_format,
                                           &gl_intformat, NULL, NULL);

    gl_texture = dev->texture_driver->gen(dev, GL_TEXTURE_2D, internal_format);
    if (!dev->texture_driver->upload_to_gl(dev,
                                           GL_TEXTURE_2D,
                                           gl_texture,
                                           false,
                                           upload_bmp,
                                           gl_intformat,
                                           gl_format,
                                           gl_type,
                                           error)) {
        cg_object_unref(upload_bmp);
        GE(dev, glDeleteTextures(1, &gl_texture));
        return false;
    }

    tex_2d->gl_texture = gl_texture;
    tex_2d->gl_internal_format = gl_intformat;

    cg_object_unref(upload_bmp);

    tex_2d->internal_format = internal_format;

    _cg_texture_set_allocated(tex, internal_format, width, height);

    return true;
}

#if defined(CG_HAS_EGL_SUPPORT) && defined(EGL_KHR_image_base)
static bool
allocate_from_egl_image(cg_texture_2d_t *tex_2d,
                        cg_texture_loader_t *loader,
                        cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(tex_2d);
    cg_device_t *dev = tex->dev;
    cg_pixel_format_t internal_format = loader->src.egl_image.format;
    GLenum gl_error;
    GLenum gl_texture;

    gl_texture = dev->texture_driver->gen(dev, GL_TEXTURE_2D, internal_format);
    _cg_bind_gl_texture_transient(GL_TEXTURE_2D, gl_texture, false);

    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;
    dev->glEGLImageTargetTexture2D(GL_TEXTURE_2D, loader->src.egl_image.image);
    if (dev->glGetError() != GL_NO_ERROR) {
        _cg_set_error(error,
                      CG_TEXTURE_ERROR,
                      CG_TEXTURE_ERROR_BAD_PARAMETER,
                      "Could not create a cg_texture_2d_t from a given "
                      "EGLImage");
        GE(dev, glDeleteTextures(1, &gl_texture));
        return false;
    }

    tex_2d->gl_texture = gl_texture;

    tex_2d->internal_format = internal_format;

    _cg_texture_set_allocated(tex,
                              internal_format,
                              loader->src.egl_image.width,
                              loader->src.egl_image.height);

    return true;
}
#endif

#ifdef CG_HAS_WEBGL_SUPPORT
static bool
allocate_from_webgl_image(cg_texture_2d_t *tex_2d,
                          cg_texture_loader_t *loader,
                          cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(tex_2d);
    cg_device_t *dev = tex->dev;
    cg_webgl_image_t *image = loader->src.webgl_image.image;
    cg_pixel_format_t internal_format =
        _cg_texture_determine_internal_format(tex, loader->src.webgl_image.format);
    int width = cg_webgl_image_get_width(image);
    int height = cg_webgl_image_get_height(image);
    GLenum gl_texture;
    GLenum gl_error;
    char *js_error;

    if (!_cg_texture_2d_gl_can_create(dev, width, height, internal_format)) {
        _cg_set_error(error,
                      CG_TEXTURE_ERROR,
                      CG_TEXTURE_ERROR_SIZE,
                      "Failed to create texture 2d due to size/format"
                      " constraints");
        return false;
    }

    gl_texture = dev->texture_driver->gen(dev, GL_TEXTURE_2D, internal_format);
    _cg_bind_gl_texture_transient(GL_TEXTURE_2D, gl_texture, false);

    if (_cg_pixel_format_is_premultiplied(internal_format))
        GE( dev, glPixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL, true));
    else
        GE( dev, glPixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL, false));

    /* Setup gl alignment to match rowstride and top-left corner */
    dev->texture_driver->prep_gl_for_pixels_upload (dev,
                                                    1 /* fake rowstride */,
                                                    1 /* fake bpp */);

    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;

    js_error = _cg_webgl_tex_image_2d_with_image(GL_TEXTURE_2D,
                                                 0,
                                                 GL_RGBA,
                                                 GL_RGBA,
                                                 GL_UNSIGNED_BYTE,
                                                 image->image_handle);

    if (js_error) {
        _cg_set_error(error,
                      CG_TEXTURE_ERROR,
                      CG_TEXTURE_ERROR_BAD_PARAMETER,
                      "Could not create a cg_texture_2d_t from a given "
                      "HTMLImageElement: %s", js_error);
        c_free(js_error);
        GE(dev, glDeleteTextures(1, &gl_texture));
        return false;
    }

    if (dev->glGetError() != GL_NO_ERROR) {
        _cg_set_error(error,
                      CG_TEXTURE_ERROR,
                      CG_TEXTURE_ERROR_BAD_PARAMETER,
                      "Could not create a cg_texture_2d_t from a given "
                      "HTMLImageElement");
        GE(dev, glDeleteTextures(1, &gl_texture));
        return false;
    }

    tex_2d->gl_texture = gl_texture;

    tex_2d->internal_format = internal_format;

    _cg_texture_set_allocated(tex,
                              internal_format,
                              width,
                              height);

    return true;
}
#endif

static bool
allocate_from_gl_foreign(cg_texture_2d_t *tex_2d,
                         cg_texture_loader_t *loader,
                         cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(tex_2d);
    cg_device_t *dev = tex->dev;
    cg_pixel_format_t format = loader->src.gl_foreign.format;
    GLenum gl_error = 0;
    GLint gl_compressed = false;
    GLenum gl_int_format = 0;

    /* Make sure binding succeeds */
    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;

    _cg_bind_gl_texture_transient(
        GL_TEXTURE_2D, loader->src.gl_foreign.gl_handle, true);
    if (dev->glGetError() != GL_NO_ERROR) {
        _cg_set_error(error,
                      CG_SYSTEM_ERROR,
                      CG_SYSTEM_ERROR_UNSUPPORTED,
                      "Failed to bind foreign GL_TEXTURE_2D texture");
        return false;
    }

/* Obtain texture parameters
   (only level 0 we are interested in) */

#ifdef CG_HAS_GL_SUPPORT
    if (_cg_has_private_feature(dev,
                                CG_PRIVATE_FEATURE_QUERY_TEXTURE_PARAMETERS)) {
        GE(dev,
           glGetTexLevelParameteriv(
               GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &gl_compressed));

        {
            GLint val;

            GE(dev,
               glGetTexLevelParameteriv(
                   GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &val));

            gl_int_format = val;
        }

        /* If we can query GL for the actual pixel format then we'll ignore
           the passed in format and use that. */
        if (!dev->driver_vtable->pixel_format_from_gl_internal(dev, gl_int_format, &format)) {
            _cg_set_error(error,
                          CG_SYSTEM_ERROR,
                          CG_SYSTEM_ERROR_UNSUPPORTED,
                          "Unsupported internal format for foreign texture");
            return false;
        }
    } else
#endif
    {
        /* Otherwise we'll assume we can derive the GL format from the
           passed in format */
        dev->driver_vtable->pixel_format_to_gl(dev, format, &gl_int_format,
                                               NULL, NULL);
    }

    /* Compressed texture images not supported */
    if (gl_compressed == GL_TRUE) {
        _cg_set_error(error,
                      CG_SYSTEM_ERROR,
                      CG_SYSTEM_ERROR_UNSUPPORTED,
                      "Compressed foreign textures aren't currently supported");
        return false;
    }

    /* Note: previously this code would query the texture object for
       whether it has GL_GENERATE_MIPMAP enabled to determine whether to
       auto-generate the mipmap. This doesn't make much sense any more
       since CGlib switch to using glGenerateMipmap. Ideally I think
       cg_texture_2d_new_from_foreign should take a flags parameter so
       that the application can decide whether it wants
       auto-mipmapping. To be compatible with existing code, CGlib now
       disables its own auto-mipmapping but leaves the value of
       GL_GENERATE_MIPMAP alone so that it would still work but without
       the dirtiness tracking that CGlib would do. */

    _cg_texture_2d_set_auto_mipmap(CG_TEXTURE(tex_2d), false);

    /* Setup bitmap info */
    tex_2d->is_foreign = true;
    tex_2d->mipmaps_dirty = true;

    tex_2d->gl_texture = loader->src.gl_foreign.gl_handle;
    tex_2d->gl_internal_format = gl_int_format;

    /* Unknown filter */
    tex_2d->gl_legacy_texobj_min_filter = false;
    tex_2d->gl_legacy_texobj_mag_filter = false;

    tex_2d->internal_format = format;

    _cg_texture_set_allocated(tex,
                              format,
                              loader->src.gl_foreign.width,
                              loader->src.gl_foreign.height);
    return true;
}

bool
_cg_texture_2d_gl_allocate(cg_texture_t *tex, cg_error_t **error)
{
    cg_texture_2d_t *tex_2d = CG_TEXTURE_2D(tex);
    cg_texture_loader_t *loader = tex->loader;

    c_return_val_if_fail(loader, false);

    switch (loader->src_type) {
    case CG_TEXTURE_SOURCE_TYPE_SIZED:
        return allocate_with_size(tex_2d, loader, error);
    case CG_TEXTURE_SOURCE_TYPE_BITMAP:
        return allocate_from_bitmap(tex_2d, loader, error);
    case CG_TEXTURE_SOURCE_TYPE_EGL_IMAGE:
#if defined(CG_HAS_EGL_SUPPORT) && defined(EGL_KHR_image_base)
        return allocate_from_egl_image(tex_2d, loader, error);
#else
        c_return_val_if_reached(false);
#endif
    case CG_TEXTURE_SOURCE_TYPE_WEBGL_IMAGE:
#ifdef CG_HAS_WEBGL_SUPPORT
        return allocate_from_webgl_image(tex_2d, loader, error);
#else
        c_return_val_if_reached(false);
#endif
    case CG_TEXTURE_SOURCE_TYPE_GL_FOREIGN:
        return allocate_from_gl_foreign(tex_2d, loader, error);
    }

    c_return_val_if_reached(false);
}

void
_cg_texture_2d_gl_flush_legacy_texobj_filters(cg_texture_t *tex,
                                              GLenum min_filter,
                                              GLenum mag_filter)
{
    cg_texture_2d_t *tex_2d = CG_TEXTURE_2D(tex);
    cg_device_t *dev = tex->dev;

    if (min_filter == tex_2d->gl_legacy_texobj_min_filter &&
        mag_filter == tex_2d->gl_legacy_texobj_mag_filter)
        return;

    /* Store new values */
    tex_2d->gl_legacy_texobj_min_filter = min_filter;
    tex_2d->gl_legacy_texobj_mag_filter = mag_filter;

    /* Apply new filters to the texture */
    _cg_bind_gl_texture_transient(
        GL_TEXTURE_2D, tex_2d->gl_texture, tex_2d->is_foreign);
    GE(dev, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter));
    GE(dev, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter));
}

void
_cg_texture_2d_gl_flush_legacy_texobj_wrap_modes(cg_texture_t *tex,
                                                 GLenum wrap_mode_s,
                                                 GLenum wrap_mode_t,
                                                 GLenum wrap_mode_p)
{
    cg_texture_2d_t *tex_2d = CG_TEXTURE_2D(tex);
    cg_device_t *dev = tex->dev;

    /* Only set the wrap mode if it's different from the current value
       to avoid too many GL calls. Texture 2D doesn't make use of the r
       coordinate so we can ignore its wrap mode */
    if (tex_2d->gl_legacy_texobj_wrap_mode_s != wrap_mode_s ||
        tex_2d->gl_legacy_texobj_wrap_mode_t != wrap_mode_t) {
        _cg_bind_gl_texture_transient(
            GL_TEXTURE_2D, tex_2d->gl_texture, tex_2d->is_foreign);
        GE(dev,
           glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode_s));
        GE(dev,
           glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode_t));

        tex_2d->gl_legacy_texobj_wrap_mode_s = wrap_mode_s;
        tex_2d->gl_legacy_texobj_wrap_mode_t = wrap_mode_t;
    }
}

cg_texture_2d_t *
cg_texture_2d_gl_new_from_foreign(cg_device_t *dev,
                                  unsigned int gl_handle,
                                  int width,
                                  int height,
                                  cg_pixel_format_t format)
{
    cg_texture_loader_t *loader;

    /* NOTE: width, height and internal format are not queriable
     * in GLES, hence such a function prototype.
     */

    /* Note: We always trust the given width and height without querying
     * the texture object because the user may be creating a CGlib
     * texture for a texture_from_pixmap object where glTexImage2D may
     * not have been called and the texture_from_pixmap spec doesn't
     * clarify that it is reliable to query back the size from OpenGL.
     */

    /* Assert it is a valid GL texture object */
    c_return_val_if_fail(dev->glIsTexture(gl_handle), false);

    /* Validate width and height */
    c_return_val_if_fail(width > 0 && height > 0, NULL);

    loader = _cg_texture_create_loader(dev);
    loader->src_type = CG_TEXTURE_SOURCE_TYPE_GL_FOREIGN;
    loader->src.gl_foreign.gl_handle = gl_handle;
    loader->src.gl_foreign.width = width;
    loader->src.gl_foreign.height = height;
    loader->src.gl_foreign.format = format;

    return _cg_texture_2d_create_base(dev, width, height, format, loader);
}

void
_cg_texture_2d_gl_copy_from_framebuffer(cg_texture_2d_t *tex_2d,
                                        int src_x,
                                        int src_y,
                                        int width,
                                        int height,
                                        cg_framebuffer_t *src_fb,
                                        int dst_x,
                                        int dst_y,
                                        int level)
{
    cg_texture_t *tex = CG_TEXTURE(tex_2d);
    cg_device_t *dev = tex->dev;

    /* Make sure the current framebuffers are bound, though we don't need to
     * flush the clip state here since we aren't going to draw to the
     * framebuffer. */
    _cg_framebuffer_flush_state(dev->current_draw_buffer,
                                src_fb,
                                CG_FRAMEBUFFER_STATE_ALL &
                                ~CG_FRAMEBUFFER_STATE_CLIP);

    _cg_bind_gl_texture_transient(
        GL_TEXTURE_2D, tex_2d->gl_texture, tex_2d->is_foreign);

    dev->glCopyTexSubImage2D(GL_TEXTURE_2D,
                             0, /* level */
                             dst_x,
                             dst_y,
                             src_x,
                             src_y,
                             width,
                             height);
}

unsigned int
_cg_texture_2d_gl_get_gl_handle(cg_texture_2d_t *tex_2d)
{
    return tex_2d->gl_texture;
}

void
_cg_texture_2d_gl_generate_mipmap(cg_texture_2d_t *tex_2d)
{
    _cg_texture_gl_generate_mipmaps(CG_TEXTURE(tex_2d));
}

bool
_cg_texture_2d_gl_copy_from_bitmap(cg_texture_2d_t *tex_2d,
                                   int src_x,
                                   int src_y,
                                   int width,
                                   int height,
                                   cg_bitmap_t *bmp,
                                   int dst_x,
                                   int dst_y,
                                   int level,
                                   cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(tex_2d);
    cg_device_t *dev = tex->dev;
    cg_bitmap_t *upload_bmp;
    cg_pixel_format_t upload_format;
    GLenum gl_format;
    GLenum gl_type;
    bool status = true;

    upload_bmp =
        _cg_bitmap_convert_for_upload(bmp,
                                      _cg_texture_get_format(tex),
                                      false, /* can't convert in place */
                                      error);
    if (upload_bmp == NULL)
        return false;

    upload_format = cg_bitmap_get_format(upload_bmp);

    dev->driver_vtable->pixel_format_to_gl(dev,
                                           upload_format,
                                           NULL, /* internal format */
                                           &gl_format,
                                           &gl_type);

    status = dev->texture_driver->upload_subregion_to_gl(dev,
                                                         tex,
                                                         false,
                                                         src_x,
                                                         src_y,
                                                         dst_x,
                                                         dst_y,
                                                         width,
                                                         height,
                                                         level,
                                                         upload_bmp,
                                                         gl_format,
                                                         gl_type,
                                                         error);

    cg_object_unref(upload_bmp);

    _cg_texture_gl_maybe_update_max_level(tex, level);

    return status;
}

void
_cg_texture_2d_gl_get_data(cg_texture_2d_t *tex_2d,
                           cg_pixel_format_t format,
                           int rowstride,
                           uint8_t *data)
{
    cg_device_t *dev = CG_TEXTURE(tex_2d)->dev;
    int bpp;
    int width = CG_TEXTURE(tex_2d)->width;
    GLenum gl_format;
    GLenum gl_type;

    bpp = _cg_pixel_format_get_bytes_per_pixel(format);

    dev->driver_vtable->pixel_format_to_gl(dev,
                                           format,
                                           NULL, /* internal format */
                                           &gl_format,
                                           &gl_type);

    dev->texture_driver->prep_gl_for_pixels_download(dev, rowstride, width,
                                                     bpp);

    _cg_bind_gl_texture_transient(
        GL_TEXTURE_2D, tex_2d->gl_texture, tex_2d->is_foreign);

    dev->texture_driver->gl_get_tex_image(dev, GL_TEXTURE_2D, gl_format,
                                          gl_type, data);
}
