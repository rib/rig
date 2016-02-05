/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-private.h"
#include "cg-util.h"
#include "cg-texture-private.h"
#include "cg-texture-3d-private.h"
#include "cg-texture-3d.h"
#include "cg-texture-gl-private.h"
#include "cg-texture-driver.h"
#include "cg-device-private.h"
#include "cg-object-private.h"
#include "cg-pipeline-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-error-private.h"
#include "cg-util-gl-private.h"

#include <string.h>
#include <math.h>

/* These might not be defined on GLES */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif
#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R 0x8072
#endif

static void _cg_texture_3d_free(cg_texture_3d_t *tex_3d);

CG_TEXTURE_DEFINE(Texture3D, texture_3d);

static const cg_texture_vtable_t cg_texture_3d_vtable;

static void
_cg_texture_3d_gl_flush_legacy_texobj_wrap_modes(cg_texture_t *tex,
                                                 GLenum wrap_mode_s,
                                                 GLenum wrap_mode_t,
                                                 GLenum wrap_mode_p)
{
    cg_texture_3d_t *tex_3d = CG_TEXTURE_3D(tex);
    cg_device_t *dev = tex->dev;

    /* Only set the wrap mode if it's different from the current value
       to avoid too many GL calls. */
    if (tex_3d->gl_legacy_texobj_wrap_mode_s != wrap_mode_s ||
        tex_3d->gl_legacy_texobj_wrap_mode_t != wrap_mode_t ||
        tex_3d->gl_legacy_texobj_wrap_mode_p != wrap_mode_p) {
        _cg_bind_gl_texture_transient(GL_TEXTURE_3D, tex_3d->gl_texture, false);
        GE(dev,
           glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, wrap_mode_s));
        GE(dev,
           glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, wrap_mode_t));
        GE(dev,
           glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, wrap_mode_p));

        tex_3d->gl_legacy_texobj_wrap_mode_s = wrap_mode_s;
        tex_3d->gl_legacy_texobj_wrap_mode_t = wrap_mode_t;
        tex_3d->gl_legacy_texobj_wrap_mode_p = wrap_mode_p;
    }
}

static void
_cg_texture_3d_free(cg_texture_3d_t *tex_3d)
{
    cg_texture_t *tex = CG_TEXTURE(tex_3d);

    if (tex_3d->gl_texture) 
        _cg_delete_gl_texture(tex->dev, tex_3d->gl_texture);

    /* Chain up */
    _cg_texture_free(tex);
}

static void
_cg_texture_3d_set_auto_mipmap(cg_texture_t *tex, bool value)
{
    cg_texture_3d_t *tex_3d = CG_TEXTURE_3D(tex);

    tex_3d->auto_mipmap = value;
}

static cg_texture_3d_t *
_cg_texture_3d_create_base(cg_device_t *dev,
                           int width,
                           int height,
                           int depth,
                           cg_pixel_format_t internal_format,
                           cg_texture_loader_t *loader)
{
    cg_texture_3d_t *tex_3d = c_new(cg_texture_3d_t, 1);
    cg_texture_t *tex = CG_TEXTURE(tex_3d);

    _cg_texture_init(tex,
                     dev,
                     width,
                     height,
                     internal_format,
                     loader,
                     &cg_texture_3d_vtable);

    tex_3d->gl_texture = 0;

    tex_3d->depth = depth;
    tex_3d->mipmaps_dirty = true;
    tex_3d->auto_mipmap = true;

    /* We default to GL_LINEAR for both filters */
    tex_3d->gl_legacy_texobj_min_filter = GL_LINEAR;
    tex_3d->gl_legacy_texobj_mag_filter = GL_LINEAR;

    /* Wrap mode not yet set */
    tex_3d->gl_legacy_texobj_wrap_mode_s = false;
    tex_3d->gl_legacy_texobj_wrap_mode_t = false;
    tex_3d->gl_legacy_texobj_wrap_mode_p = false;

    return _cg_texture_3d_object_new(tex_3d);
}

cg_texture_3d_t *
cg_texture_3d_new_with_size(cg_device_t *dev, int width, int height,
                            int depth)
{
    cg_texture_loader_t *loader = _cg_texture_create_loader(dev);
    loader->src_type = CG_TEXTURE_SOURCE_TYPE_SIZED;
    loader->src.sized.width = width;
    loader->src.sized.height = height;
    loader->src.sized.depth = depth;

    return _cg_texture_3d_create_base(dev, width, height, depth,
                                      CG_PIXEL_FORMAT_RGBA_8888_PRE, loader);
}

cg_texture_3d_t *
cg_texture_3d_new_from_bitmap(cg_bitmap_t *bmp, int height, int depth)
{
    cg_texture_loader_t *loader;

    c_return_val_if_fail(bmp, NULL);

    loader = _cg_texture_create_loader(bmp->dev);
    loader->src_type = CG_TEXTURE_SOURCE_TYPE_BITMAP;
    loader->src.bitmap.bitmap = cg_object_ref(bmp);
    loader->src.bitmap.height = height;
    loader->src.bitmap.depth = depth;
    loader->src.bitmap.can_convert_in_place = false; /* TODO add api for this */

    return _cg_texture_3d_create_base(_cg_bitmap_get_context(bmp),
                                      cg_bitmap_get_width(bmp),
                                      height,
                                      depth,
                                      cg_bitmap_get_format(bmp),
                                      loader);
}

cg_texture_3d_t *
cg_texture_3d_new_from_data(cg_device_t *dev,
                            int width,
                            int height,
                            int depth,
                            cg_pixel_format_t format,
                            int rowstride,
                            int image_stride,
                            const uint8_t *data,
                            cg_error_t **error)
{
    cg_bitmap_t *bitmap;
    cg_texture_3d_t *ret;

    c_return_val_if_fail(data, NULL);
    c_return_val_if_fail(format != CG_PIXEL_FORMAT_ANY, NULL);

    /* Rowstride from width if not given */
    if (rowstride == 0)
        rowstride = width * _cg_pixel_format_get_bytes_per_pixel(format);
    /* Image stride from height and rowstride if not given */
    if (image_stride == 0)
        image_stride = height * rowstride;

    if (image_stride < rowstride * height)
        return NULL;

    /* GL doesn't support uploading when the image_stride isn't a
       multiple of the rowstride. If this happens we'll just pack the
       image into a new bitmap. The documentation for this function
       recommends avoiding this situation. */
    if (image_stride % rowstride != 0) {
        uint8_t *bmp_data;
        int bmp_rowstride;
        int z, y;

        bitmap = _cg_bitmap_new_with_malloc_buffer(dev, width,
                                                   depth * height, format,
                                                   error);
        if (!bitmap)
            return NULL;

        bmp_data = _cg_bitmap_map(
            bitmap, CG_BUFFER_ACCESS_WRITE, CG_BUFFER_MAP_HINT_DISCARD, error);

        if (bmp_data == NULL) {
            cg_object_unref(bitmap);
            return NULL;
        }

        bmp_rowstride = cg_bitmap_get_rowstride(bitmap);

        /* Copy all of the images in */
        for (z = 0; z < depth; z++)
            for (y = 0; y < height; y++)
                memcpy(bmp_data +
                       (z * bmp_rowstride * height + bmp_rowstride * y),
                       data + z * image_stride + rowstride * y,
                       bmp_rowstride);

        _cg_bitmap_unmap(bitmap);
    } else
        bitmap = cg_bitmap_new_for_data(dev,
                                        width,
                                        image_stride / rowstride * depth,
                                        format,
                                        rowstride,
                                        (uint8_t *)data);

    ret = cg_texture_3d_new_from_bitmap(bitmap, height, depth);

    cg_object_unref(bitmap);

    if (ret && !cg_texture_allocate(CG_TEXTURE(ret), error)) {
        cg_object_unref(ret);
        return NULL;
    }

    return ret;
}

static bool
_cg_texture_3d_can_create(cg_device_t *dev,
                          int width,
                          int height,
                          int depth,
                          cg_pixel_format_t internal_format,
                          cg_error_t **error)
{
    GLenum gl_intformat;
    GLenum gl_type;

    /* This should only happen on GLES */
    if (!cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_3D)) {
        _cg_set_error(error,
                      CG_SYSTEM_ERROR,
                      CG_SYSTEM_ERROR_UNSUPPORTED,
                      "3D textures are not supported by the GPU");
        return false;
    }

    /* If NPOT textures aren't supported then the size must be a power
       of two */
    if (!cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_NPOT) &&
        (!_cg_util_is_pot(width) || !_cg_util_is_pot(height) ||
         !_cg_util_is_pot(depth))) {
        _cg_set_error(error,
                      CG_SYSTEM_ERROR,
                      CG_SYSTEM_ERROR_UNSUPPORTED,
                      "A non-power-of-two size was requested but this is not "
                      "supported by the GPU");
        return false;
    }

    dev->driver_vtable->pixel_format_to_gl(dev, internal_format,
                                           &gl_intformat, NULL, &gl_type);

    /* Check that the driver can create a texture with that size */
    if (!dev->texture_driver->size_supported_3d(dev, GL_TEXTURE_3D, gl_intformat, gl_type, width, height, depth)) {
        _cg_set_error(error,
                      CG_SYSTEM_ERROR,
                      CG_SYSTEM_ERROR_UNSUPPORTED,
                      "The requested dimensions are not supported by the GPU");
        return false;
    }

    return true;
}

static bool
allocate_with_size(cg_texture_3d_t *tex_3d,
                   cg_texture_loader_t *loader,
                   cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(tex_3d);
    cg_device_t *dev = tex->dev;
    cg_pixel_format_t internal_format;
    int width = loader->src.sized.width;
    int height = loader->src.sized.height;
    int depth = loader->src.sized.depth;
    GLenum gl_intformat;
    GLenum gl_format;
    GLenum gl_type;
    GLenum gl_error;
    GLenum gl_texture;

    internal_format =
        _cg_texture_determine_internal_format(tex, CG_PIXEL_FORMAT_ANY);

    if (!_cg_texture_3d_can_create(dev, width, height, depth, internal_format, error))
        return false;

    dev->driver_vtable->pixel_format_to_gl(dev, internal_format,
                                           &gl_intformat, &gl_format,
                                           &gl_type);

    gl_texture = dev->texture_driver->gen(dev, GL_TEXTURE_3D,
                                          internal_format);
    _cg_bind_gl_texture_transient(GL_TEXTURE_3D, gl_texture, false);
    /* Clear any GL errors */
    while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
        ;

    dev->glTexImage3D(GL_TEXTURE_3D,
                      0,
                      gl_intformat,
                      width,
                      height,
                      depth,
                      0,
                      gl_format,
                      gl_type,
                      NULL);

    if (_cg_gl_util_catch_out_of_memory(dev, error)) {
        GE(dev, glDeleteTextures(1, &gl_texture));
        return false;
    }

    tex_3d->gl_texture = gl_texture;
    tex_3d->gl_format = gl_intformat;

    tex_3d->depth = depth;

    tex_3d->internal_format = internal_format;

    _cg_texture_set_allocated(tex, internal_format, width, height);

    return true;
}

static bool
allocate_from_bitmap(cg_texture_3d_t *tex_3d,
                     cg_texture_loader_t *loader,
                     cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(tex_3d);
    cg_device_t *dev = tex->dev;
    cg_pixel_format_t internal_format;
    cg_bitmap_t *bmp = loader->src.bitmap.bitmap;
    int bmp_width = cg_bitmap_get_width(bmp);
    int height = loader->src.bitmap.height;
    int depth = loader->src.bitmap.depth;
    cg_pixel_format_t bmp_format = cg_bitmap_get_format(bmp);
    bool can_convert_in_place = loader->src.bitmap.can_convert_in_place;
    cg_bitmap_t *upload_bmp;
    cg_pixel_format_t upload_format;
    GLenum gl_intformat;
    GLenum gl_format;
    GLenum gl_type;

    internal_format = _cg_texture_determine_internal_format(tex, bmp_format);

    if (!_cg_texture_3d_can_create(dev, bmp_width, height, depth, internal_format, error))
        return false;

    upload_bmp = _cg_bitmap_convert_for_upload(
        bmp, internal_format, can_convert_in_place, error);
    if (upload_bmp == NULL)
        return false;

    upload_format = cg_bitmap_get_format(upload_bmp);

    dev->driver_vtable->pixel_format_to_gl(dev,
                                           upload_format,
                                           NULL, /* internal format */
                                           &gl_format,
                                           &gl_type);
    dev->driver_vtable->pixel_format_to_gl(dev, internal_format,
                                           &gl_intformat, NULL, NULL);

    tex_3d->gl_texture =
        dev->texture_driver->gen(dev, GL_TEXTURE_3D, internal_format);

    if (!dev->texture_driver->upload_to_gl_3d(dev,
                                              GL_TEXTURE_3D,
                                              tex_3d->gl_texture,
                                              false, /* is_foreign */
                                              height,
                                              depth,
                                              upload_bmp,
                                              gl_intformat,
                                              gl_format,
                                              gl_type,
                                              error)) {
        cg_object_unref(upload_bmp);
        return false;
    }

    tex_3d->gl_format = gl_intformat;

    cg_object_unref(upload_bmp);

    tex_3d->depth = loader->src.bitmap.depth;

    tex_3d->internal_format = internal_format;

    _cg_texture_set_allocated(
        tex, internal_format, bmp_width, loader->src.bitmap.height);

    return true;
}

static bool
_cg_texture_3d_allocate(cg_texture_t *tex, cg_error_t **error)
{
    cg_texture_3d_t *tex_3d = CG_TEXTURE_3D(tex);
    cg_texture_loader_t *loader = tex->loader;

    c_return_val_if_fail(loader, false);

    switch (loader->src_type) {
    case CG_TEXTURE_SOURCE_TYPE_SIZED:
        return allocate_with_size(tex_3d, loader, error);
    case CG_TEXTURE_SOURCE_TYPE_BITMAP:
        return allocate_from_bitmap(tex_3d, loader, error);
    default:
        break;
    }

    c_return_val_if_reached(false);
}

static bool
_cg_texture_3d_is_sliced(cg_texture_t *tex)
{
    return false;
}

static bool
_cg_texture_3d_can_hardware_repeat(cg_texture_t *tex)
{
    return true;
}

static bool
_cg_texture_3d_get_gl_texture(cg_texture_t *tex,
                              GLuint *out_gl_handle,
                              GLenum *out_gl_target)
{
    cg_texture_3d_t *tex_3d = CG_TEXTURE_3D(tex);

    if (out_gl_handle)
        *out_gl_handle = tex_3d->gl_texture;

    if (out_gl_target)
        *out_gl_target = GL_TEXTURE_3D;

    return true;
}

static void
_cg_texture_3d_gl_flush_legacy_texobj_filters(cg_texture_t *tex,
                                              GLenum min_filter,
                                              GLenum mag_filter)
{
    cg_texture_3d_t *tex_3d = CG_TEXTURE_3D(tex);
    cg_device_t *dev = tex->dev;

    if (min_filter == tex_3d->gl_legacy_texobj_min_filter &&
        mag_filter == tex_3d->gl_legacy_texobj_mag_filter)
        return;

    /* Store new values */
    tex_3d->gl_legacy_texobj_min_filter = min_filter;
    tex_3d->gl_legacy_texobj_mag_filter = mag_filter;

    /* Apply new filters to the texture */
    _cg_bind_gl_texture_transient(GL_TEXTURE_3D, tex_3d->gl_texture, false);
    GE(dev, glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, mag_filter));
    GE(dev, glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, min_filter));
}

static void
_cg_texture_3d_pre_paint(cg_texture_t *tex,
                         cg_texture_pre_paint_flags_t flags)
{
    cg_texture_3d_t *tex_3d = CG_TEXTURE_3D(tex);

    /* Only update if the mipmaps are dirty */
    if ((flags & CG_TEXTURE_NEEDS_MIPMAP) && tex_3d->auto_mipmap &&
        tex_3d->mipmaps_dirty)
    {
        _cg_texture_gl_generate_mipmaps(tex);
        tex_3d->mipmaps_dirty = false;
    }
}

static bool
_cg_texture_3d_set_region(cg_texture_t *tex,
                          int src_x,
                          int src_y,
                          int dst_x,
                          int dst_y,
                          int dst_width,
                          int dst_height,
                          int level,
                          cg_bitmap_t *bmp,
                          cg_error_t **error)
{
    /* This function doesn't really make sense for 3D textures because
       it can't specify which image to upload to */
    _cg_set_error(error,
                  CG_SYSTEM_ERROR,
                  CG_SYSTEM_ERROR_UNSUPPORTED,
                  "Setting a 2D region on a 3D texture isn't "
                  "currently supported");

    return false;
}

static int
_cg_texture_3d_get_data(cg_texture_t *tex,
                        cg_pixel_format_t format,
                        int rowstride,
                        uint8_t *data)
{
    /* FIXME: we could probably implement this by assuming the data is
       big enough to hold all of the images and that there is no stride
       between the images. However it would be better to have an API
       that can provide an image stride and this function probably isn't
       particularly useful anyway so for now it just reports failure */
    return 0;
}

static cg_pixel_format_t
_cg_texture_3d_get_format(cg_texture_t *tex)
{
    return CG_TEXTURE_3D(tex)->internal_format;
}

static GLenum
_cg_texture_3d_get_gl_format(cg_texture_t *tex)
{
    return CG_TEXTURE_3D(tex)->gl_format;
}

static cg_texture_type_t
_cg_texture_3d_get_type(cg_texture_t *tex)
{
    return CG_TEXTURE_TYPE_3D;
}

static const cg_texture_vtable_t cg_texture_3d_vtable = {
    true, /* primitive */
    _cg_texture_3d_allocate,
    _cg_texture_3d_set_region,
    _cg_texture_3d_get_data,
    NULL, /* foreach_sub_texture_in_region */
    _cg_texture_3d_is_sliced,
    _cg_texture_3d_can_hardware_repeat,
    _cg_texture_3d_get_gl_texture,
    _cg_texture_3d_gl_flush_legacy_texobj_filters,
    _cg_texture_3d_pre_paint,
    _cg_texture_3d_gl_flush_legacy_texobj_wrap_modes,
    _cg_texture_3d_get_format,
    _cg_texture_3d_get_gl_format,
    _cg_texture_3d_get_type,
    NULL, /* is_foreign */
    _cg_texture_3d_set_auto_mipmap
};
