/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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
 */

#include <cglib-config.h>

#include "cg-private.h"
#include "cg-util.h"
#include "cg-texture-private.h"
#include "cg-texture-2d-private.h"
#include "cg-texture-2d-gl-private.h"
#include "cg-texture-driver.h"
#include "cg-device-private.h"
#include "cg-object-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-framebuffer-private.h"
#include "cg-error-private.h"
#ifdef CG_HAS_EGL_SUPPORT
#include "cg-winsys-egl-private.h"
#endif

#include <string.h>
#include <math.h>

#ifdef CG_HAS_WAYLAND_EGL_SERVER_SUPPORT
#include "cg-wayland-server.h"
#endif

static void _cg_texture_2d_free(cg_texture_2d_t *tex_2d);

CG_TEXTURE_DEFINE(Texture2D, texture_2d);

static const cg_texture_vtable_t cg_texture_2d_vtable;

typedef struct _cg_texture_2d_manual_repeat_data_t {
    cg_texture_2d_t *tex_2d;
    cg_meta_texture_callback_t callback;
    void *user_data;
} cg_texture_2d_manual_repeat_data_t;

static void
_cg_texture_2d_free(cg_texture_2d_t *tex_2d)
{
    cg_device_t *dev = CG_TEXTURE(tex_2d)->dev;

    dev->driver_vtable->texture_2d_free(tex_2d);

    /* Chain up */
    _cg_texture_free(CG_TEXTURE(tex_2d));
}

void
_cg_texture_2d_set_auto_mipmap(cg_texture_t *tex, bool value)
{
    cg_texture_2d_t *tex_2d = CG_TEXTURE_2D(tex);

    tex_2d->auto_mipmap = value;
}

cg_texture_2d_t *
_cg_texture_2d_create_base(cg_device_t *dev,
                           int width,
                           int height,
                           cg_pixel_format_t internal_format,
                           cg_texture_loader_t *loader)
{
    cg_texture_2d_t *tex_2d = c_new(cg_texture_2d_t, 1);
    cg_texture_t *tex = CG_TEXTURE(tex_2d);

    _cg_texture_init(tex,
                     dev,
                     width,
                     height,
                     internal_format,
                     loader,
                     &cg_texture_2d_vtable);

    tex_2d->mipmaps_dirty = true;
    tex_2d->auto_mipmap = true;

    tex_2d->is_foreign = false;

    dev->driver_vtable->texture_2d_init(tex_2d);

    return _cg_texture_2d_object_new(tex_2d);
}

cg_texture_2d_t *
cg_texture_2d_new_with_size(cg_device_t *dev, int width, int height)
{
    cg_texture_loader_t *loader;

    loader = _cg_texture_create_loader(dev);
    loader->src_type = CG_TEXTURE_SOURCE_TYPE_SIZED;
    loader->src.sized.width = width;
    loader->src.sized.height = height;

    return _cg_texture_2d_create_base(dev, width, height,
                                      CG_PIXEL_FORMAT_RGBA_8888_PRE, loader);
}

static bool
_cg_texture_2d_allocate(cg_texture_t *tex, cg_error_t **error)
{
    cg_device_t *dev = tex->dev;

    return dev->driver_vtable->texture_2d_allocate(tex, error);
}

static cg_texture_2d_t *
_cg_texture_2d_new_from_bitmap(cg_bitmap_t *bmp, bool can_convert_in_place)
{
    cg_texture_loader_t *loader;

    c_return_val_if_fail(bmp != NULL, NULL);

    loader = _cg_texture_create_loader(bmp->dev);
    loader->src_type = CG_TEXTURE_SOURCE_TYPE_BITMAP;
    loader->src.bitmap.bitmap = cg_object_ref(bmp);
    loader->src.bitmap.can_convert_in_place = can_convert_in_place;

    return _cg_texture_2d_create_base(_cg_bitmap_get_context(bmp),
                                      cg_bitmap_get_width(bmp),
                                      cg_bitmap_get_height(bmp),
                                      cg_bitmap_get_format(bmp),
                                      loader);
}

cg_texture_2d_t *
cg_texture_2d_new_from_bitmap(cg_bitmap_t *bmp)
{
    return _cg_texture_2d_new_from_bitmap(bmp,
                                          false); /* can't convert in place */
}

cg_texture_2d_t *
cg_texture_2d_new_from_file(cg_device_t *dev,
                            const char *filename,
                            cg_error_t **error)
{
    cg_bitmap_t *bmp;
    cg_texture_2d_t *tex_2d = NULL;

    c_return_val_if_fail(error == NULL || *error == NULL, NULL);

    bmp = cg_bitmap_new_from_file(dev, filename, error);
    if (bmp == NULL)
        return NULL;

    tex_2d =
        _cg_texture_2d_new_from_bitmap(bmp, true); /* can convert in-place */

    cg_object_unref(bmp);

    return tex_2d;
}

cg_texture_2d_t *
cg_texture_2d_new_from_data(cg_device_t *dev,
                            int width,
                            int height,
                            cg_pixel_format_t format,
                            int rowstride,
                            const uint8_t *data,
                            cg_error_t **error)
{
    cg_bitmap_t *bmp;
    cg_texture_2d_t *tex_2d;

    c_return_val_if_fail(format != CG_PIXEL_FORMAT_ANY, NULL);
    c_return_val_if_fail(data != NULL, NULL);

    /* Rowstride from width if not given */
    if (rowstride == 0)
        rowstride = width * _cg_pixel_format_get_bytes_per_pixel(format);

    /* Wrap the data into a bitmap */
    bmp = cg_bitmap_new_for_data(dev, width, height, format, rowstride,
                                 (uint8_t *)data);

    tex_2d = cg_texture_2d_new_from_bitmap(bmp);

    cg_object_unref(bmp);

    if (tex_2d && !cg_texture_allocate(CG_TEXTURE(tex_2d), error)) {
        cg_object_unref(tex_2d);
        return NULL;
    }

    return tex_2d;
}

#if defined(CG_HAS_EGL_SUPPORT) && defined(EGL_KHR_image_base)
/* NB: The reason we require the width, height and format to be passed
 * even though they may seem redundant is because GLES 1/2 don't
 * provide a way to query these properties. */
cg_texture_2d_t *
_cg_egl_texture_2d_new_from_image(cg_device_t *dev,
                                  int width,
                                  int height,
                                  cg_pixel_format_t format,
                                  EGLImageKHR image,
                                  cg_error_t **error)
{
    cg_texture_loader_t *loader;
    cg_texture_2d_t *tex;

    c_return_val_if_fail(_cg_device_get_winsys(dev)->constraints &
                           CG_RENDERER_CONSTRAINT_USES_EGL,
                           NULL);

    c_return_val_if_fail(
        _cg_has_private_feature(dev,
                                CG_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE),
        NULL);

    loader = _cg_texture_create_loader(dev);
    loader->src_type = CG_TEXTURE_SOURCE_TYPE_EGL_IMAGE;
    loader->src.egl_image.image = image;
    loader->src.egl_image.width = width;
    loader->src.egl_image.height = height;
    loader->src.egl_image.format = format;

    tex = _cg_texture_2d_create_base(dev, width, height, format, loader);

    if (!cg_texture_allocate(CG_TEXTURE(tex), error)) {
        cg_object_unref(tex);
        return NULL;
    }

    return tex;
}
#endif /* defined (CG_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base) */

#ifdef CG_HAS_WEBGL_SUPPORT
cg_texture_2d_t *
cg_webgl_texture_2d_new_from_image(cg_device_t *dev, cg_webgl_image_t *image)
{
    cg_texture_loader_t *loader;
    cg_texture_2d_t *tex;

    loader = _cg_texture_create_loader(dev);
    loader->src_type = CG_TEXTURE_SOURCE_TYPE_WEBGL_IMAGE;
    loader->src.webgl_image.image = cg_object_ref(image);
    loader->src.webgl_image.format = CG_PIXEL_FORMAT_RGBA_8888_PRE;

    /* XXX: I don't see any way to determine a format for a
     * HTMLImageElement so we don't currently seem to have any choice
     * but to assume it is premultiplied RGBA */
    tex = _cg_texture_2d_create_base(dev,
                                     cg_webgl_image_get_width(image),
                                     cg_webgl_image_get_height(image),
                                     CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                     loader);
    return tex;
}
#endif

#ifdef CG_HAS_WAYLAND_EGL_SERVER_SUPPORT
static void
shm_buffer_get_cg_pixel_format(struct wl_shm_buffer *shm_buffer,
                               cg_pixel_format_t *format_out,
                               cg_texture_components_t *components_out)
{
    cg_pixel_format_t format;
    cg_texture_components_t components = CG_TEXTURE_COMPONENTS_RGBA;

    switch (wl_shm_buffer_get_format(shm_buffer)) {
#if C_BYTE_ORDER == C_BIG_ENDIAN
    case WL_SHM_FORMAT_ARGB8888:
        format = CG_PIXEL_FORMAT_ARGB_8888_PRE;
        break;
    case WL_SHM_FORMAT_XRGB8888:
        format = CG_PIXEL_FORMAT_ARGB_8888;
        components = CG_TEXTURE_COMPONENTS_RGB;
        break;
#elif C_BYTE_ORDER == C_LITTLE_ENDIAN
    case WL_SHM_FORMAT_ARGB8888:
        format = CG_PIXEL_FORMAT_BGRA_8888_PRE;
        break;
    case WL_SHM_FORMAT_XRGB8888:
        format = CG_PIXEL_FORMAT_BGRA_8888;
        components = CG_TEXTURE_COMPONENTS_RGB;
        break;
#endif
    default:
        c_warn_if_reached();
        format = CG_PIXEL_FORMAT_ARGB_8888;
    }

    if (format_out)
        *format_out = format;
    if (components_out)
        *components_out = components;
}

bool
cg_wayland_texture_set_region_from_shm_buffer(cg_texture_t *texture,
                                              int src_x,
                                              int src_y,
                                              int width,
                                              int height,
                                              struct wl_shm_buffer *shm_buffer,
                                              int dst_x,
                                              int dst_y,
                                              int level,
                                              cg_error_t **error)
{
    const uint8_t *data = wl_shm_buffer_get_data(shm_buffer);
    int32_t stride = wl_shm_buffer_get_stride(shm_buffer);
    cg_pixel_format_t format;
    int bpp;

    shm_buffer_get_cg_pixel_format(shm_buffer, &format, NULL);
    bpp = _cg_pixel_format_get_bytes_per_pixel(format);

    return cg_texture_set_region(CG_TEXTURE(texture),
                                 width,
                                 height,
                                 format,
                                 stride,
                                 data + src_x * bpp + src_y * stride,
                                 dst_x,
                                 dst_y,
                                 level,
                                 error);
}

cg_texture_2d_t *
cg_wayland_texture_2d_new_from_buffer(cg_device_t *dev,
                                      struct wl_resource *buffer,
                                      cg_error_t **error)
{
    struct wl_shm_buffer *shm_buffer;
    cg_texture_2d_t *tex = NULL;

    shm_buffer = wl_shm_buffer_get(buffer);

    if (shm_buffer) {
        int stride = wl_shm_buffer_get_stride(shm_buffer);
        int width = wl_shm_buffer_get_width(shm_buffer);
        int height = wl_shm_buffer_get_height(shm_buffer);
        cg_pixel_format_t format;
        cg_texture_components_t components;
        cg_bitmap_t *bmp;

        shm_buffer_get_cg_pixel_format(shm_buffer, &format, &components);

        bmp = cg_bitmap_new_for_data(dev,
                                     width,
                                     height,
                                     format,
                                     stride,
                                     wl_shm_buffer_get_data(shm_buffer));

        tex = cg_texture_2d_new_from_bitmap(bmp);

        cg_texture_set_components(CG_TEXTURE(tex), components);

        cg_object_unref(bmp);

        if (!cg_texture_allocate(CG_TEXTURE(tex), error)) {
            cg_object_unref(tex);
            return NULL;
        } else
            return tex;
    } else {
        int format, width, height;

        if (_cg_egl_query_wayland_buffer(dev, buffer, EGL_TEXTURE_FORMAT, &format) &&
            _cg_egl_query_wayland_buffer(dev, buffer, EGL_WIDTH, &width) &&
            _cg_egl_query_wayland_buffer(dev, buffer, EGL_HEIGHT, &height)) {
            EGLImageKHR image;
            cg_pixel_format_t internal_format;

            c_return_val_if_fail(_cg_device_get_winsys(dev)->constraints &
                                   CG_RENDERER_CONSTRAINT_USES_EGL,
                                   NULL);

            switch (format) {
            case EGL_TEXTURE_RGB:
                internal_format = CG_PIXEL_FORMAT_RGB_888;
                break;
            case EGL_TEXTURE_RGBA:
                internal_format = CG_PIXEL_FORMAT_RGBA_8888_PRE;
                break;
            default:
                _cg_set_error(error,
                              CG_SYSTEM_ERROR,
                              CG_SYSTEM_ERROR_UNSUPPORTED,
                              "Can't create texture from unknown "
                              "wayland buffer format %d\n",
                              format);
                return NULL;
            }

            image =
                _cg_egl_create_image(dev, EGL_WAYLAND_BUFFER_WL, buffer,
                                     NULL);
            tex = _cg_egl_texture_2d_new_from_image(dev, width, height,
                                                    internal_format, image,
                                                    error);
            _cg_egl_destroy_image(dev, image);
            return tex;
        }
    }

    _cg_set_error(error,
                  CG_SYSTEM_ERROR,
                  CG_SYSTEM_ERROR_UNSUPPORTED,
                  "Can't create texture from unknown "
                  "wayland buffer type\n");
    return NULL;
}
#endif /* CG_HAS_WAYLAND_EGL_SERVER_SUPPORT */

void
_cg_texture_2d_externally_modified(cg_texture_t *texture)
{
    if (!cg_is_texture_2d(texture))
        return;

    CG_TEXTURE_2D(texture)->mipmaps_dirty = true;
}

void
_cg_texture_2d_copy_from_framebuffer(cg_texture_2d_t *tex_2d,
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

    /* Assert that the storage for this texture has been allocated */
    cg_texture_allocate(tex, NULL); /* (abort on error) */

    dev->driver_vtable->texture_2d_copy_from_framebuffer(
        tex_2d, src_x, src_y, width, height, src_fb, dst_x, dst_y, level);

    tex_2d->mipmaps_dirty = true;
}

static bool
_cg_texture_2d_is_sliced(cg_texture_t *tex)
{
    return false;
}

static bool
_cg_texture_2d_can_hardware_repeat(cg_texture_t *tex)
{
    cg_device_t *dev = tex->dev;

    if (cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_NPOT_REPEAT) ||
        (_cg_util_is_pot(tex->width) && _cg_util_is_pot(tex->height)))
        return true;
    else
        return false;
}

static bool
_cg_texture_2d_get_gl_texture(cg_texture_t *tex,
                              GLuint *out_gl_handle,
                              GLenum *out_gl_target)
{
    cg_device_t *dev = tex->dev;
    cg_texture_2d_t *tex_2d = CG_TEXTURE_2D(tex);

    if (dev->driver_vtable->texture_2d_get_gl_handle) {
        GLuint handle;

        if (out_gl_target)
            *out_gl_target = GL_TEXTURE_2D;

        handle = dev->driver_vtable->texture_2d_get_gl_handle(tex_2d);

        if (out_gl_handle)
            *out_gl_handle = handle;

        return handle ? true : false;
    } else
        return false;
}

static void
_cg_texture_2d_pre_paint(cg_texture_t *tex,
                         cg_texture_pre_paint_flags_t flags)
{
    cg_texture_2d_t *tex_2d = CG_TEXTURE_2D(tex);

    /* Only update if the mipmaps are dirty */
    if ((flags & CG_TEXTURE_NEEDS_MIPMAP) && tex_2d->auto_mipmap &&
        tex_2d->mipmaps_dirty) {
        cg_device_t *dev = tex->dev;

        dev->driver_vtable->texture_2d_generate_mipmap(tex_2d);

        tex_2d->mipmaps_dirty = false;
    }
}

static bool
_cg_texture_2d_set_region(cg_texture_t *tex,
                          int src_x,
                          int src_y,
                          int dst_x,
                          int dst_y,
                          int width,
                          int height,
                          int level,
                          cg_bitmap_t *bmp,
                          cg_error_t **error)
{
    cg_device_t *dev = tex->dev;
    cg_texture_2d_t *tex_2d = CG_TEXTURE_2D(tex);

    if (!dev->driver_vtable->texture_2d_copy_from_bitmap(tex_2d,
                                                         src_x,
                                                         src_y,
                                                         width,
                                                         height,
                                                         bmp,
                                                         dst_x,
                                                         dst_y,
                                                         level,
                                                         error)) {
        return false;
    }

    tex_2d->mipmaps_dirty = true;

    return true;
}

static bool
_cg_texture_2d_get_data(cg_texture_t *tex,
                        cg_pixel_format_t format,
                        int rowstride,
                        uint8_t *data)
{
    cg_device_t *dev = tex->dev;

    if (dev->driver_vtable->texture_2d_get_data) {
        cg_texture_2d_t *tex_2d = CG_TEXTURE_2D(tex);
        dev->driver_vtable->texture_2d_get_data(
            tex_2d, format, rowstride, data);
        return true;
    } else
        return false;
}

static cg_pixel_format_t
_cg_texture_2d_get_format(cg_texture_t *tex)
{
    return CG_TEXTURE_2D(tex)->internal_format;
}

static GLenum
_cg_texture_2d_get_gl_format(cg_texture_t *tex)
{
    return CG_TEXTURE_2D(tex)->gl_internal_format;
}

static bool
_cg_texture_2d_is_foreign(cg_texture_t *tex)
{
    return CG_TEXTURE_2D(tex)->is_foreign;
}

static cg_texture_type_t
_cg_texture_2d_get_type(cg_texture_t *tex)
{
    return CG_TEXTURE_TYPE_2D;
}

static const cg_texture_vtable_t cg_texture_2d_vtable = {
    true, /* primitive */
    _cg_texture_2d_allocate,
    _cg_texture_2d_set_region,
    _cg_texture_2d_get_data,
    NULL, /* foreach_sub_texture_in_region */
    _cg_texture_2d_is_sliced,
    _cg_texture_2d_can_hardware_repeat,
    _cg_texture_2d_get_gl_texture,
    _cg_texture_2d_gl_flush_legacy_texobj_filters,
    _cg_texture_2d_pre_paint,
    _cg_texture_2d_gl_flush_legacy_texobj_wrap_modes,
    _cg_texture_2d_get_format,
    _cg_texture_2d_get_gl_format,
    _cg_texture_2d_get_type,
    _cg_texture_2d_is_foreign,
    _cg_texture_2d_set_auto_mipmap
};
