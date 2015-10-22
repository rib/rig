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

#include "cg-util.h"
#include "cg-texture-private.h"
#include "cg-sub-texture-private.h"
#include "cg-sub-texture.h"
#include "cg-device-private.h"
#include "cg-object.h"
#include "cg-texture-driver.h"
#include "cg-texture-2d.h"
#include "cg-texture-gl-private.h"

#include <string.h>
#include <math.h>

static void _cg_sub_texture_free(cg_sub_texture_t *sub_tex);

CG_TEXTURE_DEFINE(SubTexture, sub_texture);

static const cg_texture_vtable_t cg_sub_texture_vtable;

static void
_cg_sub_texture_unmap_quad(cg_sub_texture_t *sub_tex, float *coords)
{
    cg_texture_t *tex = CG_TEXTURE(sub_tex);
    float width = cg_texture_get_width(sub_tex->full_texture);
    float height = cg_texture_get_height(sub_tex->full_texture);

    coords[0] = (coords[0] * width - sub_tex->sub_x) / tex->width;
    coords[1] = (coords[1] * height - sub_tex->sub_y) / tex->height;
    coords[2] = (coords[2] * width - sub_tex->sub_x) / tex->width;
    coords[3] = (coords[3] * height - sub_tex->sub_y) / tex->height;
}

static void
_cg_sub_texture_map_quad(cg_sub_texture_t *sub_tex, float *coords)
{
    cg_texture_t *tex = CG_TEXTURE(sub_tex);
    float width = cg_texture_get_width(sub_tex->full_texture);
    float height = cg_texture_get_height(sub_tex->full_texture);

    coords[0] = (coords[0] * tex->width + sub_tex->sub_x) / width;
    coords[1] = (coords[1] * tex->height + sub_tex->sub_y) / height;
    coords[2] = (coords[2] * tex->width + sub_tex->sub_x) / width;
    coords[3] = (coords[3] * tex->height + sub_tex->sub_y) / height;
}

typedef struct _cg_sub_texture_tforeach_data_t {
    cg_sub_texture_t *sub_tex;
    cg_meta_texture_callback_t callback;
    void *user_data;
} cg_sub_texture_tforeach_data_t;

static void
unmap_coords_cb(cg_texture_t *slice_texture,
                const float *slice_texture_coords,
                const float *meta_coords,
                void *user_data)
{
    cg_sub_texture_tforeach_data_t *data = user_data;
    float unmapped_coords[4];

    memcpy(unmapped_coords, meta_coords, sizeof(unmapped_coords));

    _cg_sub_texture_unmap_quad(data->sub_tex, unmapped_coords);

    data->callback(
        slice_texture, slice_texture_coords, unmapped_coords, data->user_data);
}

static void
_cg_sub_texture_foreach_sub_texture_in_region(
    cg_texture_t *tex,
    float virtual_tx_1,
    float virtual_ty_1,
    float virtual_tx_2,
    float virtual_ty_2,
    cg_meta_texture_callback_t callback,
    void *user_data)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);
    cg_texture_t *full_texture = sub_tex->full_texture;
    float mapped_coords[4] = { virtual_tx_1, virtual_ty_1,
                               virtual_tx_2, virtual_ty_2 };
    float virtual_coords[4] = { virtual_tx_1, virtual_ty_1,
                                virtual_tx_2, virtual_ty_2 };

    /* map the virtual coordinates to ->full_texture coordinates */
    _cg_sub_texture_map_quad(sub_tex, mapped_coords);

    /* TODO: Add something like cg_is_low_level_texture() */
    if (cg_is_texture_2d(full_texture)) {
        callback(
            sub_tex->full_texture, mapped_coords, virtual_coords, user_data);
    } else {
        cg_sub_texture_tforeach_data_t data;

        data.sub_tex = sub_tex;
        data.callback = callback;
        data.user_data = user_data;

        cg_meta_texture_foreach_in_region(CG_META_TEXTURE(full_texture),
                                          mapped_coords[0],
                                          mapped_coords[1],
                                          mapped_coords[2],
                                          mapped_coords[3],
                                          CG_PIPELINE_WRAP_MODE_REPEAT,
                                          CG_PIPELINE_WRAP_MODE_REPEAT,
                                          unmap_coords_cb,
                                          &data);
    }
}

static void
_cg_sub_texture_gl_flush_legacy_texobj_wrap_modes(cg_texture_t *tex,
                                                  GLenum wrap_mode_s,
                                                  GLenum wrap_mode_t,
                                                  GLenum wrap_mode_p)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    _cg_texture_gl_flush_legacy_texobj_wrap_modes(
        sub_tex->full_texture, wrap_mode_s, wrap_mode_t, wrap_mode_p);
}

static void
_cg_sub_texture_free(cg_sub_texture_t *sub_tex)
{
    cg_object_unref(sub_tex->next_texture);
    cg_object_unref(sub_tex->full_texture);

    /* Chain up */
    _cg_texture_free(CG_TEXTURE(sub_tex));
}

cg_sub_texture_t *
cg_sub_texture_new(cg_device_t *dev,
                   cg_texture_t *next_texture,
                   int sub_x,
                   int sub_y,
                   int sub_width,
                   int sub_height)
{
    cg_texture_t *full_texture;
    cg_sub_texture_t *sub_tex;
    cg_texture_t *tex;
    unsigned int next_width, next_height;

    next_width = cg_texture_get_width(next_texture);
    next_height = cg_texture_get_height(next_texture);

    /* The region must specify a non-zero subset of the full texture */
    c_return_val_if_fail(sub_x >= 0 && sub_y >= 0, NULL);
    c_return_val_if_fail(sub_width > 0 && sub_height > 0, NULL);
    c_return_val_if_fail(sub_x + sub_width <= next_width, NULL);
    c_return_val_if_fail(sub_y + sub_height <= next_height, NULL);

    sub_tex = c_new(cg_sub_texture_t, 1);

    tex = CG_TEXTURE(sub_tex);

    _cg_texture_init(tex,
                     dev,
                     sub_width,
                     sub_height,
                     _cg_texture_get_format(next_texture),
                     NULL, /* no loader */
                     &cg_sub_texture_vtable);

    /* If the next texture is also a sub texture we can avoid one level
       of indirection by referencing the full texture of that texture
       instead. */
    if (cg_is_sub_texture(next_texture)) {
        cg_sub_texture_t *other_sub_tex = CG_SUB_TEXTURE(next_texture);
        full_texture = other_sub_tex->full_texture;
        sub_x += other_sub_tex->sub_x;
        sub_y += other_sub_tex->sub_y;
    } else
        full_texture = next_texture;

    sub_tex->next_texture = cg_object_ref(next_texture);
    sub_tex->full_texture = cg_object_ref(full_texture);

    sub_tex->sub_x = sub_x;
    sub_tex->sub_y = sub_y;

    return _cg_sub_texture_object_new(sub_tex);
}

static bool
_cg_sub_texture_allocate(cg_texture_t *tex, cg_error_t **error)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);
    bool status = cg_texture_allocate(sub_tex->full_texture, error);

    _cg_texture_set_allocated(tex,
                              _cg_texture_get_format(sub_tex->full_texture),
                              tex->width,
                              tex->height);

    return status;
}

cg_texture_t *
cg_sub_texture_get_parent(cg_sub_texture_t *sub_texture)
{
    return sub_texture->next_texture;
}

static bool
_cg_sub_texture_is_sliced(cg_texture_t *tex)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    return cg_texture_is_sliced(sub_tex->full_texture);
}

static bool
_cg_sub_texture_can_hardware_repeat(cg_texture_t *tex)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    /* We can hardware repeat if the subtexture actually represents all of the
       of the full texture */
    return (tex->width == cg_texture_get_width(sub_tex->full_texture) &&
            tex->height == cg_texture_get_height(sub_tex->full_texture) &&
            _cg_texture_can_hardware_repeat(sub_tex->full_texture));
}

static bool
_cg_sub_texture_get_gl_texture(cg_texture_t *tex,
                               GLuint *out_gl_handle,
                               GLenum *out_gl_target)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    return cg_texture_get_gl_texture(
        sub_tex->full_texture, out_gl_handle, out_gl_target);
}

static void
_cg_sub_texture_gl_flush_legacy_texobj_filters(cg_texture_t *tex,
                                               GLenum min_filter,
                                               GLenum mag_filter)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    _cg_texture_gl_flush_legacy_texobj_filters(
        sub_tex->full_texture, min_filter, mag_filter);
}

static void
_cg_sub_texture_pre_paint(cg_texture_t *tex,
                          cg_texture_pre_paint_flags_t flags)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    _cg_texture_pre_paint(sub_tex->full_texture, flags);
}

static bool
_cg_sub_texture_set_region(cg_texture_t *tex,
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
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    if (level != 0) {
        int full_width = cg_texture_get_width(sub_tex->full_texture);
        int full_height = cg_texture_get_width(sub_tex->full_texture);

        c_return_val_if_fail(sub_tex->sub_x == 0 &&
                               cg_texture_get_width(tex) == full_width,
                               false);
        c_return_val_if_fail(sub_tex->sub_y == 0 &&
                               cg_texture_get_height(tex) == full_height,
                               false);
    }

    return cg_texture_set_region_from_bitmap(sub_tex->full_texture,
                                             src_x,
                                             src_y,
                                             dst_width,
                                             dst_height,
                                             bmp,
                                             dst_x + sub_tex->sub_x,
                                             dst_y + sub_tex->sub_y,
                                             level,
                                             error);
}

static cg_pixel_format_t
_cg_sub_texture_get_format(cg_texture_t *tex)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    return _cg_texture_get_format(sub_tex->full_texture);
}

static GLenum
_cg_sub_texture_get_gl_format(cg_texture_t *tex)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    return _cg_texture_gl_get_format(sub_tex->full_texture);
}

static cg_texture_type_t
_cg_sub_texture_get_type(cg_texture_t *tex)
{
    cg_sub_texture_t *sub_tex = CG_SUB_TEXTURE(tex);

    return _cg_texture_get_type(sub_tex->full_texture);
}

static const cg_texture_vtable_t cg_sub_texture_vtable = {
    false, /* not primitive */
    _cg_sub_texture_allocate,
    _cg_sub_texture_set_region,
    NULL, /* get_data */
    _cg_sub_texture_foreach_sub_texture_in_region,
    _cg_sub_texture_is_sliced,
    _cg_sub_texture_can_hardware_repeat,
    _cg_sub_texture_get_gl_texture,
    _cg_sub_texture_gl_flush_legacy_texobj_filters,
    _cg_sub_texture_pre_paint,
    _cg_sub_texture_gl_flush_legacy_texobj_wrap_modes,
    _cg_sub_texture_get_format,
    _cg_sub_texture_get_gl_format,
    _cg_sub_texture_get_type,
    NULL, /* is_foreign */
    NULL /* set_auto_mipmap */
};
