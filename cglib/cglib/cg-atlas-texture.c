/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009,2010,2011 Intel Corporation.
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

#include "cg-debug.h"
#include "cg-util.h"
#include "cg-texture-private.h"
#include "cg-atlas-texture-private.h"
#include "cg-texture-2d-private.h"
#include "cg-sub-texture-private.h"
#include "cg-device-private.h"
#include "cg-object-private.h"
#include "cg-texture-driver.h"
#include "cg-rectangle-map.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-atlas-set-private.h"
#include "cg-atlas-private.h"
#include "cg-sub-texture.h"
#include "cg-error-private.h"
#include "cg-texture-gl-private.h"
#include "cg-private.h"

#include <stdlib.h>

static void _cg_atlas_texture_free(cg_atlas_texture_t *sub_tex);

CG_TEXTURE_DEFINE(AtlasTexture, atlas_texture);

static const cg_texture_vtable_t cg_atlas_texture_vtable;

static cg_sub_texture_t *
_cg_atlas_texture_create_sub_texture(cg_texture_t *full_texture,
                                     const cg_atlas_allocation_t *allocation)
{
    cg_device_t *dev = full_texture->dev;
    /* Create a subtexture for the given rectangle not including the
       1-pixel border */
    return cg_sub_texture_new(dev,
                              full_texture,
                              allocation->x + 1,
                              allocation->y + 1,
                              allocation->width - 2,
                              allocation->height - 2);
}

static void
_cg_atlas_texture_allocate_cb(cg_atlas_t *atlas,
                              cg_texture_t *texture,
                              const cg_atlas_allocation_t *allocation,
                              void *allocation_data,
                              void *user_data)
{
    cg_atlas_texture_t *atlas_tex = allocation_data;

    /* Update the sub texture */
    if (atlas_tex->sub_texture)
        cg_object_unref(atlas_tex->sub_texture);
    atlas_tex->sub_texture =
        CG_TEXTURE(_cg_atlas_texture_create_sub_texture(texture, allocation));

    /* Update the position */
    atlas_tex->allocation = *allocation;
    atlas_tex->atlas = cg_object_ref(atlas);
}

static void
_cg_atlas_texture_pre_reorganize_foreach_cb(
    cg_atlas_t *atlas,
    const cg_atlas_allocation_t *allocation,
    void *allocation_data,
    void *user_data)
{
    cg_atlas_texture_t *atlas_tex = allocation_data;

    /* Keep a reference to the texture because we don't want it to be
       destroyed during the reorganization */
    cg_object_ref(atlas_tex);
}

static void
_cg_atlas_texture_pre_reorganize_cb(cg_atlas_t *atlas,
                                    void *user_data)
{
    cg_device_t *dev = user_data;

    /* We don't know if any batched rendering currently depend on
     * texture coordinates that would be invalidated by reorganizing
     * this atlas so we flush everything.
     *
     * We are assuming that texture atlas migration never happens
     * during a flush so we don't have to consider recursion here.
     */
#warning "XXX: it looks like we might need to fully sync with the gpu before allowing cg atlas reorgs"
    /* On the other hand if we know the reorganisation itself will
     * be implicitly synchronized with prior work since the reorg
     * will be handled via render to texture commands then maybe
     * we don't even need a flush?
     */
    _cg_flush(dev);

    cg_atlas_foreach(atlas, _cg_atlas_texture_pre_reorganize_foreach_cb, NULL);
}

typedef struct {
    cg_atlas_texture_t **textures;
    /* Number of textures found so far */
    int n_textures;
} cg_atlas_texture_get_allocations_data_t;

static void
_cg_atlas_texture_get_allocations_cb(cg_atlas_t *atlas,
                                     const cg_atlas_allocation_t *allocation,
                                     void *allocation_data,
                                     void *user_data)
{
    cg_atlas_texture_get_allocations_data_t *data = user_data;

    data->textures[data->n_textures++] = allocation_data;
}

static void
_cg_atlas_texture_post_reorganize_cb(cg_atlas_t *atlas,
                                     void *user_data)
{
    int n_allocations = cg_atlas_get_n_allocations(atlas);

    if (n_allocations) {
        cg_atlas_texture_get_allocations_data_t data;
        int i;

        data.textures = c_alloca(sizeof(cg_atlas_texture_t *) * n_allocations);
        data.n_textures = 0;

        /* We need to remove all of the references that we took during
         * the preorganize callback. We have to get a separate array of
         * the textures because cg_atlas_t doesn't support removing
         * allocations during iteration */
        cg_atlas_foreach(atlas, _cg_atlas_texture_get_allocations_cb, &data);

        for (i = 0; i < data.n_textures; i++) {
            /* Ignore textures that don't have an atlas yet. This will
               happen when a new texture is added because we allocate
               the structure for the texture so that it can get stored
               in the atlas but it isn't a valid object yet */
            if (data.textures[i]->atlas)
                cg_object_unref(data.textures[i]);
        }
    }
}

static void
_cg_atlas_texture_foreach_sub_texture_in_region(
    cg_texture_t *tex,
    float virtual_tx_1,
    float virtual_ty_1,
    float virtual_tx_2,
    float virtual_ty_2,
    cg_meta_texture_callback_t callback,
    void *user_data)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);
    cg_meta_texture_t *meta_texture = CG_META_TEXTURE(atlas_tex->sub_texture);

    /* Forward on to the sub texture */
    cg_meta_texture_foreach_in_region(meta_texture,
                                      virtual_tx_1,
                                      virtual_ty_1,
                                      virtual_tx_2,
                                      virtual_ty_2,
                                      CG_PIPELINE_WRAP_MODE_REPEAT,
                                      CG_PIPELINE_WRAP_MODE_REPEAT,
                                      callback,
                                      user_data);
}

static void
_cg_atlas_texture_gl_flush_legacy_texobj_wrap_modes(cg_texture_t *tex,
                                                    GLenum wrap_mode_s,
                                                    GLenum wrap_mode_t,
                                                    GLenum wrap_mode_p)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);

    /* Forward on to the sub texture */
    _cg_texture_gl_flush_legacy_texobj_wrap_modes(
        atlas_tex->sub_texture, wrap_mode_s, wrap_mode_t, wrap_mode_p);
}

static void
_cg_atlas_texture_remove_from_atlas(cg_atlas_texture_t *atlas_tex)
{
    if (atlas_tex->atlas) {
        _cg_atlas_remove(atlas_tex->atlas,
                         atlas_tex->allocation.x,
                         atlas_tex->allocation.y,
                         atlas_tex->allocation.width,
                         atlas_tex->allocation.height);

        cg_object_unref(atlas_tex->atlas);
        atlas_tex->atlas = NULL;
    }
}

static void
_cg_atlas_texture_free(cg_atlas_texture_t *atlas_tex)
{
    _cg_atlas_texture_remove_from_atlas(atlas_tex);

    if (atlas_tex->sub_texture)
        cg_object_unref(atlas_tex->sub_texture);

    /* Chain up */
    _cg_texture_free(CG_TEXTURE(atlas_tex));
}

static bool
_cg_atlas_texture_is_sliced(cg_texture_t *tex)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);

    /* Forward on to the sub texture */
    return cg_texture_is_sliced(atlas_tex->sub_texture);
}

static bool
_cg_atlas_texture_can_hardware_repeat(cg_texture_t *tex)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);

    /* Forward on to the sub texture */
    return _cg_texture_can_hardware_repeat(atlas_tex->sub_texture);
}

static bool
_cg_atlas_texture_get_gl_texture(cg_texture_t *tex,
                                 GLuint *out_gl_handle,
                                 GLenum *out_gl_target)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);

    /* Forward on to the sub texture */
    return cg_texture_get_gl_texture(
        atlas_tex->sub_texture, out_gl_handle, out_gl_target);
}

static void
_cg_atlas_texture_gl_flush_legacy_texobj_filters(cg_texture_t *tex,
                                                 GLenum min_filter,
                                                 GLenum mag_filter)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);

    /* Forward on to the sub texture */
    _cg_texture_gl_flush_legacy_texobj_filters(
        atlas_tex->sub_texture, min_filter, mag_filter);
}

static void
_cg_atlas_texture_migrate_out_of_atlas(cg_atlas_texture_t *atlas_tex)
{
    cg_texture_t *standalone_tex;
    cg_device_t *dev;

    /* Make sure this texture is not in the atlas */
    if (!atlas_tex->atlas)
        return;

    dev = CG_TEXTURE(atlas_tex)->dev;

    CG_NOTE(ATLAS, "Migrating texture out of the atlas");

    /* We don't know if any batched rendering currently depend on
     * texture coordinates that would be invalidated by migrating
     * textures in this atlas so we flush everything before migrating.
     *
     * We are assuming that texture atlas migration never happens
     * during a flush so we don't have to consider recursion here.
     */
    _cg_flush(dev);

    standalone_tex =
        _cg_atlas_migrate_allocation(atlas_tex->atlas,
                                     atlas_tex->allocation.x + 1,
                                     atlas_tex->allocation.y + 1,
                                     atlas_tex->allocation.width - 2,
                                     atlas_tex->allocation.height - 2,
                                     atlas_tex->internal_format);
    /* Note: we simply silently ignore failures to migrate a texture
     * out (most likely due to lack of memory) and hope for the
     * best.
     *
     * Maybe we should find a way to report the problem back to the
     * app.
     */
    if (!standalone_tex)
        return;

    /* Notify cg-pipeline.c that the texture's underlying GL texture
     * storage is changing so it knows it may need to bind a new texture
     * if the cg_texture_t is reused with the same texture unit. */
    _cg_pipeline_texture_storage_change_notify(CG_TEXTURE(atlas_tex));

    /* We need to unref the sub texture after doing the copy because
       the copy can involve rendering which might cause the texture
       to be used if it is used from a layer that is left in a
       texture unit */
    cg_object_unref(atlas_tex->sub_texture);
    atlas_tex->sub_texture = standalone_tex;

    _cg_atlas_texture_remove_from_atlas(atlas_tex);
}

static void
_cg_atlas_texture_pre_paint(cg_texture_t *tex,
                            cg_texture_pre_paint_flags_t flags)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);

    if ((flags & CG_TEXTURE_NEEDS_MIPMAP))
        /* Mipmaps do not work well with the current atlas so instead
           we'll just migrate the texture out and use a regular texture */
        _cg_atlas_texture_migrate_out_of_atlas(atlas_tex);

    /* Forward on to the sub texture */
    _cg_texture_pre_paint(atlas_tex->sub_texture, flags);
}

static bool
_cg_atlas_texture_set_region_with_border(cg_atlas_texture_t *atlas_tex,
                                         int src_x,
                                         int src_y,
                                         int dst_x,
                                         int dst_y,
                                         int dst_width,
                                         int dst_height,
                                         cg_bitmap_t *bmp,
                                         cg_error_t **error)
{
    cg_atlas_t *atlas = atlas_tex->atlas;

    /* Copy the central data */
    if (!cg_texture_set_region_from_bitmap(atlas->texture,
                                           src_x,
                                           src_y,
                                           dst_width,
                                           dst_height,
                                           bmp,
                                           dst_x + atlas_tex->allocation.x + 1,
                                           dst_y + atlas_tex->allocation.y + 1,
                                           0, /* level 0 */
                                           error))
        return false;

    /* Update the left edge pixels */
    if (dst_x == 0 &&
        !cg_texture_set_region_from_bitmap(atlas->texture,
                                           src_x,
                                           src_y,
                                           1,
                                           dst_height,
                                           bmp,
                                           atlas_tex->allocation.x,
                                           dst_y + atlas_tex->allocation.y + 1,
                                           0, /* level 0 */
                                           error))
        return false;
    /* Update the right edge pixels */
    if (dst_x + dst_width == atlas_tex->allocation.width - 2 &&
        !cg_texture_set_region_from_bitmap(atlas->texture,
                                           src_x + dst_width - 1,
                                           src_y,
                                           1,
                                           dst_height,
                                           bmp,
                                           atlas_tex->allocation.x +
                                           atlas_tex->allocation.width - 1,
                                           dst_y + atlas_tex->allocation.y + 1,
                                           0, /* level 0 */
                                           error))
        return false;
    /* Update the top edge pixels */
    if (dst_y == 0 &&
        !cg_texture_set_region_from_bitmap(atlas->texture,
                                           src_x,
                                           src_y,
                                           dst_width,
                                           1,
                                           bmp,
                                           dst_x + atlas_tex->allocation.x + 1,
                                           atlas_tex->allocation.y,
                                           0, /* level 0 */
                                           error))
        return false;
    /* Update the bottom edge pixels */
    if (dst_y + dst_height == atlas_tex->allocation.height - 2 &&
        !cg_texture_set_region_from_bitmap(atlas->texture,
                                           src_x,
                                           src_y + dst_height - 1,
                                           dst_width,
                                           1,
                                           bmp,
                                           dst_x + atlas_tex->allocation.x + 1,
                                           atlas_tex->allocation.y +
                                           atlas_tex->allocation.height - 1,
                                           0, /* level 0 */
                                           error))
        return false;

    return true;
}

static cg_bitmap_t *
_cg_atlas_texture_convert_bitmap_for_upload(cg_atlas_texture_t *atlas_tex,
                                            cg_bitmap_t *bmp,
                                            cg_pixel_format_t internal_format,
                                            bool can_convert_in_place,
                                            cg_error_t **error)
{
    cg_bitmap_t *upload_bmp;
    cg_bitmap_t *override_bmp;

    /* We'll prepare to upload using the format of the actual texture of
       the atlas texture instead of the format reported by
       _cg_texture_get_format which would be the original internal
       format specified when the texture was created. However we'll
       preserve the premult status of the internal format because the
       images are all stored in the original premult format of the
       orignal format so we do need to trigger the conversion */

    if (_cg_pixel_format_is_premultiplied(internal_format))
        internal_format = CG_PIXEL_FORMAT_RGBA_8888_PRE;
    else
        internal_format = CG_PIXEL_FORMAT_RGBA_8888;

    upload_bmp = _cg_bitmap_convert_for_upload(
        bmp, internal_format, can_convert_in_place, error);
    if (upload_bmp == NULL)
        return NULL;

    /* We'll create another bitmap which uses the same data but
       overrides the format to remove the premult flag so that uploads
       to the atlas texture won't trigger the conversion again */

    override_bmp = _cg_bitmap_new_shared(upload_bmp,
                                         _cg_pixel_format_premult_stem(cg_bitmap_get_format(upload_bmp)),
                                         cg_bitmap_get_width(upload_bmp),
                                         cg_bitmap_get_height(upload_bmp),
                                         cg_bitmap_get_rowstride(upload_bmp));

    cg_object_unref(upload_bmp);

    return override_bmp;
}

static bool
_cg_atlas_texture_set_region(cg_texture_t *tex,
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
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);

    if (level != 0 && atlas_tex->atlas)
        _cg_atlas_texture_migrate_out_of_atlas(atlas_tex);

    /* If the texture is in the atlas then we need to copy the edge
       pixels to the border */
    if (atlas_tex->atlas) {
        bool ret;
        cg_bitmap_t *upload_bmp = _cg_atlas_texture_convert_bitmap_for_upload(
            atlas_tex,
            bmp,
            atlas_tex->internal_format,
            false, /* can't convert
                      in place */
            error);
        if (!upload_bmp)
            return false;

        /* Upload the data ignoring the premult bit */
        ret = _cg_atlas_texture_set_region_with_border(atlas_tex,
                                                       src_x,
                                                       src_y,
                                                       dst_x,
                                                       dst_y,
                                                       dst_width,
                                                       dst_height,
                                                       upload_bmp,
                                                       error);

        cg_object_unref(upload_bmp);

        return ret;
    } else
        /* Otherwise we can just forward on to the sub texture */
        return cg_texture_set_region_from_bitmap(atlas_tex->sub_texture,
                                                 src_x,
                                                 src_y,
                                                 dst_width,
                                                 dst_height,
                                                 bmp,
                                                 dst_x,
                                                 dst_y,
                                                 level,
                                                 error);
}

static cg_pixel_format_t
_cg_atlas_texture_get_format(cg_texture_t *tex)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);

    /* We don't want to forward this on the sub-texture because it isn't
       the necessarily the same format. This will happen if the texture
       isn't pre-multiplied */
    return atlas_tex->internal_format;
}

static GLenum
_cg_atlas_texture_get_gl_format(cg_texture_t *tex)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);

    /* Forward on to the sub texture */
    return _cg_texture_gl_get_format(atlas_tex->sub_texture);
}

static bool
_cg_atlas_texture_can_use_format(cg_pixel_format_t format)
{
    cg_texture_components_t components = _cg_pixel_format_get_components(format);
    int bpp = _cg_pixel_format_get_bytes_per_pixel(format);

    /* We don't care about the ordering or the premult status and we can
       accept RGBA or RGB textures. Although we could also accept
       luminance and alpha only textures or 16-bit formats it seems that
       if the application is explicitly using these formats then they've
       got a reason to want the lower memory requirements so putting
       them in the atlas might not be a good idea */

    if ((components == CG_TEXTURE_COMPONENTS_RGB && bpp == 24) ||
        (components == CG_TEXTURE_COMPONENTS_RGBA && bpp == 32))
    {
        return true;
    }
    else
        return false;
}

void
_cg_atlas_texture_atlas_event_handler(cg_atlas_set_t *set,
                                      cg_atlas_t *atlas,
                                      cg_atlas_set_event_t event,
                                      void *user_data)
{
    switch (event) {
    case CG_ATLAS_SET_EVENT_ADDED: {
        cg_atlas_reorganize_callback_t pre_callback =
            _cg_atlas_texture_pre_reorganize_cb;
        cg_atlas_reorganize_callback_t post_callback =
            _cg_atlas_texture_post_reorganize_cb;

        cg_atlas_add_allocate_callback(atlas,
                                       _cg_atlas_texture_allocate_cb,
                                       NULL, /* user data */
                                       NULL); /* destroy */
        cg_atlas_add_pre_reorganize_callback(
            atlas, pre_callback, set->dev, NULL); /* destroy */
        cg_atlas_add_post_reorganize_callback(
            atlas, post_callback, set->dev, NULL); /* destroy */
        break;
    }
    case CG_ATLAS_SET_EVENT_REMOVED:
        break;
    }
}

static cg_atlas_texture_t *
_cg_atlas_texture_create_base(cg_device_t *dev,
                              int width,
                              int height,
                              cg_pixel_format_t internal_format,
                              cg_texture_loader_t *loader)
{
    cg_atlas_texture_t *atlas_tex;

    CG_NOTE(ATLAS, "Adding texture of size %ix%i", width, height);

    /* We need to allocate the texture now because we need the pointer
       to set as the data for the rectangle in the atlas */
    atlas_tex = c_new0(cg_atlas_texture_t, 1);
    /* Mark it as having no atlas so we don't try to unref it in
       _cg_atlas_texture_post_reorganize_cb */
    atlas_tex->atlas = NULL;

    _cg_texture_init(CG_TEXTURE(atlas_tex),
                     dev,
                     width,
                     height,
                     internal_format,
                     loader,
                     &cg_atlas_texture_vtable);

    atlas_tex->sub_texture = NULL;

    atlas_tex->atlas = NULL;

    return _cg_atlas_texture_object_new(atlas_tex);
}

cg_atlas_texture_t *
cg_atlas_texture_new_with_size(cg_device_t *dev, int width, int height)
{
    cg_texture_loader_t *loader;

    /* We can't atlas zero-sized textures because it breaks the atlas
     * data structure */
    c_return_val_if_fail(width > 0 && height > 0, NULL);

    loader = _cg_texture_create_loader(dev);
    loader->src_type = CG_TEXTURE_SOURCE_TYPE_SIZED;
    loader->src.sized.width = width;
    loader->src.sized.height = height;

    return _cg_atlas_texture_create_base(dev, width, height,
                                         CG_PIXEL_FORMAT_RGBA_8888_PRE,
                                         loader);
}

static bool
allocate_space(cg_atlas_texture_t *atlas_tex,
               int width,
               int height,
               cg_pixel_format_t internal_format,
               cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(atlas_tex);
    cg_device_t *dev = tex->dev;
    cg_atlas_t *atlas;

    /* If the texture is in a strange format then we won't use it */
    if (!_cg_atlas_texture_can_use_format(internal_format)) {
        CG_NOTE(ATLAS,
                "Texture can not be added because the "
                "format is unsupported");
        _cg_set_error(error,
                      CG_TEXTURE_ERROR,
                      CG_TEXTURE_ERROR_FORMAT,
                      "Texture format unsuitable for atlasing");
        return false;
    }

    /* Add two pixels for the border
     * FIXME: two pixels isn't enough if mipmapping is in use
     */
    atlas = cg_atlas_set_allocate_space(dev->atlas_set, tex->width + 2,
                                        tex->height + 2, atlas_tex);
    if (!atlas) {
        _cg_set_error(error,
                      CG_SYSTEM_ERROR,
                      CG_SYSTEM_ERROR_NO_MEMORY,
                      "Not enough memory to atlas texture");
        return false;
    }

    atlas_tex->internal_format = internal_format;

    return true;
}

static bool
allocate_with_size(cg_atlas_texture_t *atlas_tex,
                   cg_texture_loader_t *loader,
                   cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(atlas_tex);
    cg_pixel_format_t internal_format =
        _cg_texture_determine_internal_format(tex, CG_PIXEL_FORMAT_ANY);

    if (allocate_space(atlas_tex,
                       loader->src.sized.width,
                       loader->src.sized.height,
                       internal_format,
                       error)) {
        _cg_texture_set_allocated(CG_TEXTURE(atlas_tex),
                                  internal_format,
                                  loader->src.sized.width,
                                  loader->src.sized.height);
        return true;
    } else
        return false;
}

static bool
allocate_from_bitmap(cg_atlas_texture_t *atlas_tex,
                     cg_texture_loader_t *loader,
                     cg_error_t **error)
{
    cg_texture_t *tex = CG_TEXTURE(atlas_tex);
    cg_bitmap_t *bmp = loader->src.bitmap.bitmap;
    cg_pixel_format_t bmp_format = cg_bitmap_get_format(bmp);
    int width = cg_bitmap_get_width(bmp);
    int height = cg_bitmap_get_height(bmp);
    bool can_convert_in_place = loader->src.bitmap.can_convert_in_place;
    cg_pixel_format_t internal_format;
    cg_bitmap_t *upload_bmp;

    c_return_val_if_fail(atlas_tex->atlas == NULL, false);

    internal_format = _cg_texture_determine_internal_format(tex, bmp_format);

    upload_bmp = _cg_atlas_texture_convert_bitmap_for_upload(
        atlas_tex, bmp, internal_format, can_convert_in_place, error);
    if (upload_bmp == NULL)
        return false;

    if (!allocate_space(atlas_tex, width, height, internal_format, error)) {
        cg_object_unref(upload_bmp);
        return false;
    }

    /* Defer to set_region so that we can share the code for copying the
       edge pixels to the border. */
    if (!_cg_atlas_texture_set_region_with_border(atlas_tex,
                                                  0, /* src_x */
                                                  0, /* src_y */
                                                  0, /* dst_x */
                                                  0, /* dst_y */
                                                  width, /* dst_width */
                                                  height, /* dst_height */
                                                  upload_bmp,
                                                  error)) {
        _cg_atlas_texture_remove_from_atlas(atlas_tex);
        cg_object_unref(upload_bmp);
        return false;
    }

    cg_object_unref(upload_bmp);

    _cg_texture_set_allocated(tex, internal_format, width, height);

    return true;
}

static bool
_cg_atlas_texture_allocate(cg_texture_t *tex, cg_error_t **error)
{
    cg_atlas_texture_t *atlas_tex = CG_ATLAS_TEXTURE(tex);
    cg_texture_loader_t *loader = tex->loader;

    c_return_val_if_fail(loader, false);

    switch (loader->src_type) {
    case CG_TEXTURE_SOURCE_TYPE_SIZED:
        return allocate_with_size(atlas_tex, loader, error);
    case CG_TEXTURE_SOURCE_TYPE_BITMAP:
        return allocate_from_bitmap(atlas_tex, loader, error);
    default:
        break;
    }

    c_return_val_if_reached(false);
}

static cg_atlas_texture_t *
_cg_atlas_texture_new_from_bitmap(cg_bitmap_t *bmp, bool can_convert_in_place)
{
    cg_texture_loader_t *loader;

    c_return_val_if_fail(cg_is_bitmap(bmp), NULL);

    loader = _cg_texture_create_loader(bmp->dev);
    loader->src_type = CG_TEXTURE_SOURCE_TYPE_BITMAP;
    loader->src.bitmap.bitmap = cg_object_ref(bmp);
    loader->src.bitmap.can_convert_in_place = can_convert_in_place;

    return _cg_atlas_texture_create_base(_cg_bitmap_get_context(bmp),
                                         cg_bitmap_get_width(bmp),
                                         cg_bitmap_get_height(bmp),
                                         cg_bitmap_get_format(bmp),
                                         loader);
}

cg_atlas_texture_t *
cg_atlas_texture_new_from_bitmap(cg_bitmap_t *bmp)
{
    return _cg_atlas_texture_new_from_bitmap(bmp, false);
}

cg_atlas_texture_t *
cg_atlas_texture_new_from_data(cg_device_t *dev,
                               int width,
                               int height,
                               cg_pixel_format_t format,
                               int rowstride,
                               const uint8_t *data,
                               cg_error_t **error)
{
    cg_bitmap_t *bmp;
    cg_atlas_texture_t *atlas_tex;

    c_return_val_if_fail(format != CG_PIXEL_FORMAT_ANY, NULL);
    c_return_val_if_fail(data != NULL, NULL);

    /* Rowstride from width if not given */
    if (rowstride == 0)
        rowstride = width * _cg_pixel_format_get_bytes_per_pixel(format);

    /* Wrap the data into a bitmap */
    bmp = cg_bitmap_new_for_data(dev, width, height, format, rowstride,
                                 (uint8_t *)data);

    atlas_tex = cg_atlas_texture_new_from_bitmap(bmp);

    cg_object_unref(bmp);

    if (atlas_tex && !cg_texture_allocate(CG_TEXTURE(atlas_tex), error)) {
        cg_object_unref(atlas_tex);
        return NULL;
    }

    return atlas_tex;
}

cg_atlas_texture_t *
cg_atlas_texture_new_from_file(cg_device_t *dev,
                               const char *filename,
                               cg_error_t **error)
{
    cg_bitmap_t *bmp;
    cg_atlas_texture_t *atlas_tex = NULL;

    c_return_val_if_fail(error == NULL || *error == NULL, NULL);

    bmp = cg_bitmap_new_from_file(dev, filename, error);
    if (bmp == NULL)
        return NULL;

    atlas_tex =
        _cg_atlas_texture_new_from_bitmap(bmp, true); /* convert in-place */

    cg_object_unref(bmp);

    return atlas_tex;
}

static cg_texture_type_t
_cg_atlas_texture_get_type(cg_texture_t *tex)
{
    return CG_TEXTURE_TYPE_2D;
}

static const cg_texture_vtable_t cg_atlas_texture_vtable = {
    false, /* not primitive */
    _cg_atlas_texture_allocate,
    _cg_atlas_texture_set_region,
    NULL, /* get_data */
    _cg_atlas_texture_foreach_sub_texture_in_region,
    _cg_atlas_texture_is_sliced,
    _cg_atlas_texture_can_hardware_repeat,
    _cg_atlas_texture_get_gl_texture,
    _cg_atlas_texture_gl_flush_legacy_texobj_filters,
    _cg_atlas_texture_pre_paint,
    _cg_atlas_texture_gl_flush_legacy_texobj_wrap_modes,
    _cg_atlas_texture_get_format,
    _cg_atlas_texture_get_gl_format,
    _cg_atlas_texture_get_type,
    NULL, /* is_foreign */
    NULL /* set_auto_mipmap */
};
