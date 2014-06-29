/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2013 Intel Corporation.
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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef _CG_ATLAS_TEXTURE_H_
#define _CG_ATLAS_TEXTURE_H_

#include <cogl/cogl-context.h>

CG_BEGIN_DECLS

/**
 * SECTION:cogl-atlas-texture
 * @short_description: Functions for managing textures in Cogl's global
 *                     set of texture atlases
 *
 * A texture atlas is a texture that contains many smaller images that
 * an application is interested in. These are packed together as a way
 * of optimizing drawing with those images by avoiding the costs of
 * repeatedly telling the hardware to change what texture it should
 * sample from.  This can enable more geometry to be batched together
 * into few draw calls.
 *
 * Each #cg_context_t has an shared, pool of texture atlases that are
 * are managed by Cogl.
 *
 * This api lets applications upload texture data into one of Cogl's
 * shared texture atlases using a high-level #cg_atlas_texture_t which
 * represents a sub-region of one of these atlases.
 *
 * <note>A #cg_atlas_texture_t is a high-level meta texture which has
 * some limitations to be aware of. Please see the documentation for
 * #cg_meta_texture_t for more details.</note>
 */

typedef struct _cg_atlas_texture_t cg_atlas_texture_t;
#define CG_ATLAS_TEXTURE(tex) ((cg_atlas_texture_t *)tex)

/**
 * cg_atlas_texture_new_with_size:
 * @ctx: A #cg_context_t
 * @width: The width of your atlased texture.
 * @height: The height of your atlased texture.
 *
 * Creates a #cg_atlas_texture_t with a given @width and @height. A
 * #cg_atlas_texture_t represents a sub-region within one of Cogl's
 * shared texture atlases.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or let Cogl automatically allocate
 * storage lazily.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cg_texture_set_components() and
 * cg_texture_set_premultiplied().
 *
 * <note>Allocate call can fail if Cogl considers the internal
 * format to be incompatible with the format of its internal
 * atlases.</note>
 *
 * <note>The returned #cg_atlas_texture_t is a high-level meta-texture
 * with some limitations. See the documentation for #cg_meta_texture_t
 * for more details.</note>
 *
 * Returns: (transfer full): A new #cg_atlas_texture_t object.
 * Since: 1.16
 * Stability: unstable
 */
cg_atlas_texture_t *
cg_atlas_texture_new_with_size(cg_context_t *ctx, int width, int height);

/**
 * cg_atlas_texture_new_from_file:
 * @ctx: A #cg_context_t
 * @filename: the file to load
 * @error: A #cg_error_t to catch exceptional errors or %NULL
 *
 * Creates a #cg_atlas_texture_t from an image file. A #cg_atlas_texture_t
 * represents a sub-region within one of Cogl's shared texture
 * atlases.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or let Cogl automatically allocate
 * storage lazily.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cg_texture_set_components() and
 * cg_texture_set_premultiplied().
 *
 * <note>Allocate call can fail if Cogl considers the internal
 * format to be incompatible with the format of its internal
 * atlases.</note>
 *
 * <note>The returned #cg_atlas_texture_t is a high-level meta-texture
 * with some limitations. See the documentation for #cg_meta_texture_t
 * for more details.</note>
 *
 * Return value: (transfer full): A new #cg_atlas_texture_t object or
 *          %NULL on failure and @error will be updated.
 * Since: 1.16
 * Stability: unstable
 */
cg_atlas_texture_t *cg_atlas_texture_new_from_file(cg_context_t *ctx,
                                                   const char *filename,
                                                   cg_error_t **error);

/**
 * cg_atlas_texture_new_from_data:
 * @ctx: A #cg_context_t
 * @width: width of texture in pixels
 * @height: height of texture in pixels
 * @format: the #cg_pixel_format_t the buffer is stored in in RAM
 * @rowstride: the memory offset in bytes between the start of each
 *    row in @data. A value of 0 will make Cogl automatically
 *    calculate @rowstride from @width and @format.
 * @data: pointer to the memory region where the source buffer resides
 * @error: A #cg_error_t to catch exceptional errors or %NULL
 *
 * Creates a new #cg_atlas_texture_t texture based on data residing in
 * memory. A #cg_atlas_texture_t represents a sub-region within one of
 * Cogl's shared texture atlases.
 *
 * <note>This api will always immediately allocate GPU memory for the
 * texture and upload the given data so that the @data pointer does
 * not need to remain valid once this function returns. This means it
 * is not possible to configure the texture before it is allocated. If
 * you do need to configure the texture before allocation (to specify
 * constraints on the internal format for example) then you can
 * instead create a #cg_bitmap_t for your data and use
 * cg_atlas_texture_new_from_bitmap() or use
 * cg_atlas_texture_new_with_size() and then upload data using
 * cg_texture_set_data()</note>
 *
 * <note>Allocate call can fail if Cogl considers the internal
 * format to be incompatible with the format of its internal
 * atlases.</note>
 *
 * <note>The returned #cg_atlas_texture_t is a high-level
 * meta-texture with some limitations. See the documentation for
 * #cg_meta_texture_t for more details.</note>
 *
 * Return value: (transfer full): A new #cg_atlas_texture_t object or
 *          %NULL on failure and @error will be updated.
 * Since: 1.16
 * Stability: unstable
 */
cg_atlas_texture_t *cg_atlas_texture_new_from_data(cg_context_t *ctx,
                                                   int width,
                                                   int height,
                                                   cg_pixel_format_t format,
                                                   int rowstride,
                                                   const uint8_t *data,
                                                   cg_error_t **error);

/**
 * cg_atlas_texture_new_from_bitmap:
 * @bitmap: A #cg_bitmap_t
 *
 * Creates a new #cg_atlas_texture_t texture based on data residing in a
 * @bitmap. A #cg_atlas_texture_t represents a sub-region within one of
 * Cogl's shared texture atlases.
 *
 * The storage for the texture is not allocated before this function
 * returns. You can call cg_texture_allocate() to explicitly
 * allocate the underlying storage or preferably let Cogl
 * automatically allocate storage lazily when it may know more about
 * how the texture is being used and can optimize how it is allocated.
 *
 * The texture is still configurable until it has been allocated so
 * for example you can influence the internal format of the texture
 * using cg_texture_set_components() and
 * cg_texture_set_premultiplied().
 *
 * <note>Allocate call can fail if Cogl considers the internal
 * format to be incompatible with the format of its internal
 * atlases.</note>
 *
 * <note>The returned #cg_atlas_texture_t is a high-level meta-texture
 * with some limitations. See the documentation for #cg_meta_texture_t
 * for more details.</note>
 *
 * Returns: (transfer full): A new #cg_atlas_texture_t object.
 * Since: 1.16
 * Stability: unstable
 */
cg_atlas_texture_t *cg_atlas_texture_new_from_bitmap(cg_bitmap_t *bmp);

/**
 * cg_is_atlas_texture:
 * @object: a #cg_object_t
 *
 * Checks whether the given object references a #cg_atlas_texture_t
 *
 * Return value: %true if the passed object represents an atlas
 *   texture and %false otherwise
 *
 * Since: 1.16
 * Stability: Unstable
 */
bool cg_is_atlas_texture(void *object);

CG_END_DECLS

#endif /* _CG_ATLAS_TEXTURE_H_ */
