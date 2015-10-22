/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2011 Intel Corporation.
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

#include "cg-atlas-private.h"
#include "cg-rectangle-map.h"
#include "cg-device-private.h"
#include "cg-texture-private.h"
#include "cg-texture-2d-private.h"
#include "cg-texture-2d-sliced.h"
#include "cg-texture-driver.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-debug.h"
#include "cg-framebuffer-private.h"
#include "cg-blit.h"
#include "cg-private.h"

#include <stdlib.h>

static void _cg_atlas_free(cg_atlas_t *atlas);

CG_OBJECT_DEFINE(Atlas, atlas);

cg_atlas_t *
_cg_atlas_new(cg_device_t *dev,
              cg_pixel_format_t internal_format,
              cg_atlas_flags_t flags)
{
    cg_atlas_t *atlas = c_new(cg_atlas_t, 1);

    atlas->dev = dev;
    atlas->map = NULL;
    atlas->texture = NULL;
    atlas->flags = flags;
    atlas->internal_format = internal_format;

    c_list_init(&atlas->allocate_closures);

    c_list_init(&atlas->pre_reorganize_closures);
    c_list_init(&atlas->post_reorganize_closures);

    return _cg_atlas_object_new(atlas);
}

cg_atlas_allocate_closure_t *
cg_atlas_add_allocate_callback(cg_atlas_t *atlas,
                               cg_atlas_allocate_callback_t callback,
                               void *user_data,
                               cg_user_data_destroy_callback_t destroy)
{
    return _cg_closure_list_add(
        &atlas->allocate_closures, callback, user_data, destroy);
}

void
cg_atlas_remove_allocate_callback(cg_atlas_t *atlas,
                                  cg_atlas_allocate_closure_t *closure)
{
    _cg_closure_disconnect(closure);
}

static void
_cg_atlas_free(cg_atlas_t *atlas)
{
    CG_NOTE(ATLAS, "%p: Atlas destroyed", atlas);

    if (atlas->texture)
        cg_object_unref(atlas->texture);
    if (atlas->map)
        _cg_rectangle_map_free(atlas->map);

    _cg_closure_list_disconnect_all(&atlas->allocate_closures);

    _cg_closure_list_disconnect_all(&atlas->pre_reorganize_closures);
    _cg_closure_list_disconnect_all(&atlas->post_reorganize_closures);

    c_free(atlas);
}

typedef struct _cg_atlas_reposition_data_t {
    /* The current user data for this texture */
    void *allocation_data;

    /* The old and new positions of the texture */
    cg_rectangle_map_entry_t old_position;
    cg_rectangle_map_entry_t new_position;
} cg_atlas_reposition_data_t;

static void
_cg_atlas_migrate(cg_atlas_t *atlas,
                  int n_textures,
                  cg_atlas_reposition_data_t *textures,
                  cg_texture_t *old_texture,
                  cg_texture_t *new_texture,
                  void *skip_allocation_data)
{
    int i;
    cg_blit_data_t blit_data;

    /* If the 'disable migrate' flag is set then we won't actually copy
       the textures to their new location. Instead we'll just invoke the
       callback to update the position */
    if ((atlas->flags & CG_ATLAS_DISABLE_MIGRATION))
        for (i = 0; i < n_textures; i++) {
            cg_atlas_allocation_t *allocation =
                (cg_atlas_allocation_t *)&textures[i].new_position;

            /* Update the texture position */
            _cg_closure_list_invoke(&atlas->allocate_closures,
                                    cg_atlas_allocate_callback_t,
                                    atlas,
                                    new_texture,
                                    allocation,
                                    textures[i].allocation_data);
        }
    else {
        _cg_blit_begin(&blit_data, new_texture, old_texture);

        for (i = 0; i < n_textures; i++) {
            cg_atlas_allocation_t *allocation =
                (cg_atlas_allocation_t *)&textures[i].new_position;

            /* Skip the texture that is being added because it doesn't contain
               any data yet */
            if (textures[i].allocation_data != skip_allocation_data)
                _cg_blit(&blit_data,
                         textures[i].old_position.x,
                         textures[i].old_position.y,
                         textures[i].new_position.x,
                         textures[i].new_position.y,
                         textures[i].new_position.width,
                         textures[i].new_position.height);

            /* Update the texture position */
            _cg_closure_list_invoke(&atlas->allocate_closures,
                                    cg_atlas_allocate_callback_t,
                                    atlas,
                                    new_texture,
                                    allocation,
                                    textures[i].allocation_data);
        }

        _cg_blit_end(&blit_data);
    }
}

typedef struct _cg_atlas_get_rectangles_data_t {
    cg_atlas_reposition_data_t *textures;
    /* Number of textures found so far */
    int n_textures;
} cg_atlas_get_rectangles_data_t;

static void
_cg_atlas_get_rectangles_cb(
    const cg_rectangle_map_entry_t *rectangle, void *rect_data, void *user_data)
{
    cg_atlas_get_rectangles_data_t *data = user_data;

    data->textures[data->n_textures].old_position = *rectangle;
    data->textures[data->n_textures++].allocation_data = rect_data;
}

static void
_cg_atlas_get_next_size(int *map_width, int *map_height)
{
    /* Double the size of the texture by increasing whichever dimension
       is smaller */
    if (*map_width < *map_height)
        *map_width <<= 1;
    else
        *map_height <<= 1;
}

static void
_cg_atlas_get_initial_size(cg_atlas_t *atlas, int *map_width, int *map_height)
{
    cg_device_t *dev = atlas->dev;
    unsigned int size;
    GLenum gl_intformat;
    GLenum gl_format;
    GLenum gl_type;

    dev->driver_vtable->pixel_format_to_gl(dev, atlas->internal_format,
                                           &gl_intformat, &gl_format,
                                           &gl_type);

    /* At least on Intel hardware, the texture size will be rounded up
       to at least 1MB so we might as well try to aim for that as an
       initial minimum size. If the format is only 1 byte per pixel we
       can use 1024x1024, otherwise we'll assume it will take 4 bytes
       per pixel and use 512x512. */
    if (_cg_pixel_format_get_bytes_per_pixel(atlas->internal_format) == 1)
        size = 1024;
    else
        size = 512;

    /* Some platforms might not support this large size so we'll
       decrease the size until it can */
    while (
        size > 1 &&
        !dev->texture_driver->size_supported(dev, GL_TEXTURE_2D, gl_intformat, gl_format, gl_type, size, size))
        size >>= 1;

    *map_width = size;
    *map_height = size;
}

static cg_rectangle_map_t *
_cg_atlas_create_map(cg_atlas_t *atlas,
                     int map_width,
                     int map_height,
                     int n_textures,
                     cg_atlas_reposition_data_t *textures)
{
    cg_device_t *dev = atlas->dev;
    GLenum gl_intformat;
    GLenum gl_format;
    GLenum gl_type;

    dev->driver_vtable->pixel_format_to_gl(dev, atlas->internal_format,
                                           &gl_intformat, &gl_format,
                                           &gl_type);

    /* Keep trying increasingly larger atlases until we can fit all of
       the textures */
    while (dev->texture_driver->size_supported(dev,
                                               GL_TEXTURE_2D,
                                               gl_intformat,
                                               gl_format,
                                               gl_type,
                                               map_width,
                                               map_height)) {
        cg_rectangle_map_t *new_atlas =
            _cg_rectangle_map_new(map_width, map_height, NULL);
        int i;

        CG_NOTE(ATLAS,
                "Trying to resize the atlas to %ux%u",
                map_width,
                map_height);

        /* Add all of the textures and keep track of the new position */
        for (i = 0; i < n_textures; i++)
            if (!_cg_rectangle_map_add(new_atlas,
                                       textures[i].old_position.width,
                                       textures[i].old_position.height,
                                       textures[i].allocation_data,
                                       &textures[i].new_position))
                break;

        /* If the atlas can contain all of the textures then we have a
           winner */
        if (i >= n_textures)
            return new_atlas;
        else
            CG_NOTE(ATLAS,
                    "Atlas size abandoned after trying "
                    "%u out of %u textures",
                    i,
                    n_textures);

        _cg_rectangle_map_free(new_atlas);
        _cg_atlas_get_next_size(&map_width, &map_height);
    }

    /* If we get here then there's no atlas that can accommodate all of
       the rectangles */

    return NULL;
}

static cg_texture_2d_t *
_cg_atlas_create_texture(cg_atlas_t *atlas, int width, int height)
{
    cg_device_t *dev = atlas->dev;
    cg_texture_2d_t *tex;
    cg_error_t *ignore_error = NULL;

    if ((atlas->flags & CG_ATLAS_CLEAR_TEXTURE)) {
        uint8_t *clear_data;
        cg_bitmap_t *clear_bmp;
        int bpp = _cg_pixel_format_get_bytes_per_pixel(atlas->internal_format);

        /* Create a buffer of zeroes to initially clear the texture */
        clear_data = c_malloc0(width * height * bpp);
        clear_bmp = cg_bitmap_new_for_data(dev,
                                           width,
                                           height,
                                           atlas->internal_format,
                                           width * bpp,
                                           clear_data);

        tex = cg_texture_2d_new_from_bitmap(clear_bmp);

        _cg_texture_set_internal_format(CG_TEXTURE(tex),
                                        atlas->internal_format);

        if (!cg_texture_allocate(CG_TEXTURE(tex), &ignore_error)) {
            cg_error_free(ignore_error);
            cg_object_unref(tex);
            tex = NULL;
        }

        cg_object_unref(clear_bmp);

        c_free(clear_data);
    } else {
        tex = cg_texture_2d_new_with_size(dev, width, height);

        _cg_texture_set_internal_format(CG_TEXTURE(tex),
                                        atlas->internal_format);

        if (!cg_texture_allocate(CG_TEXTURE(tex), &ignore_error)) {
            cg_error_free(ignore_error);
            cg_object_unref(tex);
            tex = NULL;
        }
    }

    return tex;
}

static int
_cg_atlas_compare_size_cb(const void *a, const void *b)
{
    const cg_atlas_reposition_data_t *ta = a;
    const cg_atlas_reposition_data_t *tb = b;
    unsigned int a_size, b_size;

    a_size = ta->old_position.width * ta->old_position.height;
    b_size = tb->old_position.width * tb->old_position.height;

    return a_size < b_size ? 1 : a_size > b_size ? -1 : 0;
}

bool
_cg_atlas_allocate_space(cg_atlas_t *atlas,
                         int width,
                         int height,
                         void *allocation_data)
{
    cg_atlas_get_rectangles_data_t data;
    cg_rectangle_map_t *new_map;
    cg_texture_2d_t *new_tex;
    int map_width, map_height;
    bool ret;
    cg_atlas_allocation_t new_allocation;

    /* Check if we can fit the rectangle into the existing map */
    if (atlas->map &&
        _cg_rectangle_map_add(atlas->map,
                              width,
                              height,
                              allocation_data,
                              (cg_rectangle_map_entry_t *)&new_allocation)) {
        CG_NOTE(ATLAS,
                "%p: Atlas is %ix%i, has %i textures and is %i%% waste",
                atlas,
                _cg_rectangle_map_get_width(atlas->map),
                _cg_rectangle_map_get_height(atlas->map),
                _cg_rectangle_map_get_n_rectangles(atlas->map),
                /* waste as a percentage */
                _cg_rectangle_map_get_remaining_space(atlas->map) * 100 /
                (_cg_rectangle_map_get_width(atlas->map) *
                 _cg_rectangle_map_get_height(atlas->map)));

        _cg_closure_list_invoke(&atlas->allocate_closures,
                                cg_atlas_allocate_callback_t,
                                atlas,
                                atlas->texture,
                                &new_allocation,
                                allocation_data);

        return true;
    }

    /* If we make it here then we need to reorganize the atlas. First
       we'll notify any users of the atlas that this is going to happen
       so that for example in cg_atlas_texture_t it can notify that the
       storage has changed and cause a flush */
    _cg_closure_list_invoke(
        &atlas->pre_reorganize_closures, cg_atlas_reorganize_callback_t, atlas);

    /* Get an array of all the textures currently in the atlas. */
    data.n_textures = 0;
    if (atlas->map == NULL)
        data.textures = c_malloc(sizeof(cg_atlas_reposition_data_t));
    else {
        int n_rectangles = _cg_rectangle_map_get_n_rectangles(atlas->map);
        data.textures =
            c_malloc(sizeof(cg_atlas_reposition_data_t) * (n_rectangles + 1));
        _cg_rectangle_map_foreach(
            atlas->map, _cg_atlas_get_rectangles_cb, &data);
    }

    /* Add the new rectangle as a dummy texture so that it can be
       positioned with the rest */
    data.textures[data.n_textures].old_position.x = 0;
    data.textures[data.n_textures].old_position.y = 0;
    data.textures[data.n_textures].old_position.width = width;
    data.textures[data.n_textures].old_position.height = height;
    data.textures[data.n_textures++].allocation_data = allocation_data;

    /* The atlasing algorithm works a lot better if the rectangles are
       added in decreasing order of size so we'll first sort the
       array */
    qsort(data.textures,
          data.n_textures,
          sizeof(cg_atlas_reposition_data_t),
          _cg_atlas_compare_size_cb);

    /* Try to create a new atlas that can contain all of the textures */
    if (atlas->map) {
        map_width = _cg_rectangle_map_get_width(atlas->map);
        map_height = _cg_rectangle_map_get_height(atlas->map);

        /* If there is enough space in for the new rectangle in the
           existing atlas with at least 6% waste we'll start with the
           same size, otherwise we'll immediately double it */
        if ((map_width * map_height -
             _cg_rectangle_map_get_remaining_space(atlas->map) +
             width * height) *
            53 / 50 >
            map_width * map_height)
            _cg_atlas_get_next_size(&map_width, &map_height);
    } else
        _cg_atlas_get_initial_size(atlas, &map_width, &map_height);

    new_map = _cg_atlas_create_map(
        atlas, map_width, map_height, data.n_textures, data.textures);

    /* If we can't create a map with the texture then give up */
    if (new_map == NULL) {
        CG_NOTE(ATLAS, "%p: Could not fit texture in the atlas", atlas);
        ret = false;
    }
    /* We need to migrate the existing textures into a new texture */
    else if ((new_tex = _cg_atlas_create_texture(
                  atlas,
                  _cg_rectangle_map_get_width(new_map),
                  _cg_rectangle_map_get_height(new_map))) == NULL) {
        CG_NOTE(ATLAS, "%p: Could not create a cg_texture_2d_t", atlas);
        _cg_rectangle_map_free(new_map);
        ret = false;
    } else {
        int waste;

        CG_NOTE(ATLAS,
                "%p: Atlas %s with size %ix%i",
                atlas,
                atlas->map == NULL ||
                _cg_rectangle_map_get_width(atlas->map) !=
                _cg_rectangle_map_get_width(new_map) ||
                _cg_rectangle_map_get_height(atlas->map) !=
                _cg_rectangle_map_get_height(new_map)
                ? "resized"
                : "reorganized",
                _cg_rectangle_map_get_width(new_map),
                _cg_rectangle_map_get_height(new_map));

        if (atlas->map) {
            /* Move all the textures to the right position in the new
               texture. This will also update the texture's rectangle */
            _cg_atlas_migrate(atlas,
                              data.n_textures,
                              data.textures,
                              atlas->texture,
                              CG_TEXTURE(new_tex),
                              allocation_data);
            _cg_rectangle_map_free(atlas->map);
            cg_object_unref(atlas->texture);
        } else {
            cg_atlas_allocation_t *allocation =
                (cg_atlas_allocation_t *)&data.textures[0].new_position;

            /* We know there's only one texture so we can just directly
               update the rectangle from its new position */
            _cg_closure_list_invoke(&atlas->allocate_closures,
                                    cg_atlas_allocate_callback_t,
                                    atlas,
                                    CG_TEXTURE(new_tex),
                                    allocation,
                                    data.textures[0].allocation_data);
        }

        atlas->map = new_map;
        atlas->texture = CG_TEXTURE(new_tex);

        waste = (_cg_rectangle_map_get_remaining_space(atlas->map) * 100 /
                 (_cg_rectangle_map_get_width(atlas->map) *
                  _cg_rectangle_map_get_height(atlas->map)));

        CG_NOTE(ATLAS,
                "%p: Atlas is %ix%i, has %i textures and is %i%% waste",
                atlas,
                _cg_rectangle_map_get_width(atlas->map),
                _cg_rectangle_map_get_height(atlas->map),
                _cg_rectangle_map_get_n_rectangles(atlas->map),
                waste);

        ret = true;
    }

    c_free(data.textures);

    _cg_closure_list_invoke(
        &atlas->pre_reorganize_closures, cg_atlas_reorganize_callback_t, atlas);

    return ret;
}

void
_cg_atlas_remove(cg_atlas_t *atlas, int x, int y, int width, int height)
{
    cg_rectangle_map_entry_t rectangle = { x, y, width, height };

    _cg_rectangle_map_remove(atlas->map, &rectangle);

    CG_NOTE(ATLAS,
            "%p: Removed rectangle sized %ix%i",
            atlas,
            rectangle.width,
            rectangle.height);
    CG_NOTE(ATLAS,
            "%p: Atlas is %ix%i, has %i textures and is %i%% waste",
            atlas,
            _cg_rectangle_map_get_width(atlas->map),
            _cg_rectangle_map_get_height(atlas->map),
            _cg_rectangle_map_get_n_rectangles(atlas->map),
            _cg_rectangle_map_get_remaining_space(atlas->map) * 100 /
            (_cg_rectangle_map_get_width(atlas->map) *
             _cg_rectangle_map_get_height(atlas->map)));
};

cg_texture_t *
cg_atlas_get_texture(cg_atlas_t *atlas)
{
    return atlas->texture;
}

static cg_texture_t *
create_migration_texture(cg_device_t *dev,
                         int width,
                         int height,
                         cg_pixel_format_t internal_format)
{
    cg_texture_t *tex;
    cg_error_t *skip_error = NULL;

    if ((_cg_util_is_pot(width) && _cg_util_is_pot(height)) ||
        (cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_NPOT_BASIC) &&
         cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_NPOT_MIPMAP))) {
        /* First try creating a fast-path non-sliced texture */
        tex = CG_TEXTURE(cg_texture_2d_new_with_size(dev, width, height));

        _cg_texture_set_internal_format(tex, internal_format);

        /* TODO: instead of allocating storage here it would be better
         * if we had some api that let us just check that the size is
         * supported by the hardware so storage could be allocated
         * lazily when uploading data. */
        if (!cg_texture_allocate(tex, &skip_error)) {
            cg_error_free(skip_error);
            cg_object_unref(tex);
            tex = NULL;
        }
    } else
        tex = NULL;

    if (!tex) {
        cg_texture_2d_sliced_t *tex_2ds = cg_texture_2d_sliced_new_with_size(dev,
                                                                             width,
                                                                             height,
                                                                             CG_TEXTURE_MAX_WASTE);

        _cg_texture_set_internal_format(CG_TEXTURE(tex_2ds), internal_format);

        tex = CG_TEXTURE(tex_2ds);
    }

    return tex;
}

cg_texture_t *
_cg_atlas_migrate_allocation(cg_atlas_t *atlas,
                             int x,
                             int y,
                             int width,
                             int height,
                             cg_pixel_format_t internal_format)
{
    cg_device_t *dev = atlas->dev;
    cg_texture_t *tex;
    cg_blit_data_t blit_data;
    cg_error_t *ignore_error = NULL;

    /* Create a new texture at the right size */
    tex = create_migration_texture(dev, width, height, internal_format);
    if (!cg_texture_allocate(tex, &ignore_error)) {
        cg_error_free(ignore_error);
        cg_object_unref(tex);
        return NULL;
    }

    /* Blit the data out of the atlas to the new texture. If FBOs
       aren't available this will end up having to copy the entire
       atlas texture */
    _cg_blit_begin(&blit_data, tex, atlas->texture);
    _cg_blit(&blit_data, x, y, 0, 0, width, height);
    _cg_blit_end(&blit_data);

    return tex;
}

cg_atlas_reorganize_closure_t *
cg_atlas_add_pre_reorganize_callback(cg_atlas_t *atlas,
                                     cg_atlas_reorganize_callback_t callback,
                                     void *user_data,
                                     cg_user_data_destroy_callback_t destroy)
{
    c_return_val_if_fail(callback != NULL, NULL);

    return _cg_closure_list_add(
        &atlas->pre_reorganize_closures, callback, user_data, destroy);
}

void
cg_atlas_remove_pre_reorganize_callback(cg_atlas_t *atlas,
                                        cg_atlas_reorganize_closure_t *closure)
{
    _cg_closure_disconnect(closure);
}

cg_atlas_reorganize_closure_t *
cg_atlas_add_post_reorganize_callback(cg_atlas_t *atlas,
                                      cg_atlas_reorganize_callback_t callback,
                                      void *user_data,
                                      cg_user_data_destroy_callback_t destroy)
{
    c_return_val_if_fail(callback != NULL, NULL);

    return _cg_closure_list_add(
        &atlas->post_reorganize_closures, callback, user_data, destroy);
}

void
cg_atlas_remove_post_reorganize_callback(cg_atlas_t *atlas,
                                         cg_atlas_reorganize_closure_t *closure)
{
    _cg_closure_disconnect(closure);
}

typedef struct _foreach_state_t {
    cg_atlas_t *atlas;
    cg_atlas_foreach_callback_t callback;
    void *user_data;
} foreach_state_t;

static void
foreach_rectangle_cb(const cg_rectangle_map_entry_t *entry,
                     void *rectangle_data,
                     void *user_data)
{
    foreach_state_t *state = user_data;

    state->callback(state->atlas,
                    (cg_atlas_allocation_t *)entry,
                    rectangle_data,
                    state->user_data);
}

void
cg_atlas_foreach(cg_atlas_t *atlas,
                 cg_atlas_foreach_callback_t callback,
                 void *user_data)
{
    if (atlas->map) {
        foreach_state_t state;

        state.atlas = atlas;
        state.callback = callback;
        state.user_data = user_data;

        _cg_rectangle_map_foreach(atlas->map, foreach_rectangle_cb, &state);
    }
}

int
cg_atlas_get_n_allocations(cg_atlas_t *atlas)
{
    if (atlas->map)
        return _cg_rectangle_map_get_n_rectangles(atlas->map);
    else
        return 0;
}
