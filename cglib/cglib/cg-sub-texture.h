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
 *   Neil Roberts <neil@linux.intel.com>
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_SUB_TEXTURE_H
#define __CG_SUB_TEXTURE_H

CG_BEGIN_DECLS

/**
 * SECTION:cg-sub-texture
 * @short_description: Functions for creating and manipulating
 *                     sub-textures.
 *
 * These functions allow high-level textures to be created that
 * represent a sub-region of another texture. For example these
 * can be used to implement custom texture atlasing schemes.
 */

#define CG_SUB_TEXTURE(tex) ((cg_sub_texture_t *)tex)
typedef struct _cg_sub_texture_t cg_sub_texture_t;

/**
 * cg_sub_texture_new:
 * @dev: A #cg_device_t pointer
 * @parent_texture: The full texture containing a sub-region you want
 *                  to make a #cg_sub_texture_t from.
 * @sub_x: The top-left x coordinate of the parent region to make
 *         a texture from.
 * @sub_y: The top-left y coordinate of the parent region to make
 *         a texture from.
 * @sub_width: The width of the parent region to make a texture from.
 * @sub_height: The height of the parent region to make a texture
 *              from.
 *
 * Creates a high-level #cg_sub_texture_t representing a sub-region of
 * any other #cg_texture_t. The sub-region must strictly lye within the
 * bounds of the @parent_texture. The returned texture implements the
 * #cg_meta_texture_t interface because it's not a low level texture
 * that hardware can understand natively.
 *
 * <note>Remember: Unless you are using high level drawing APIs such
 * as cg_framebuffer_draw_rectangle() or other APIs documented to
 * understand the #cg_meta_texture_t interface then you need to use the
 * #cg_meta_texture_t interface to resolve a #cg_sub_texture_t into a
 * low-level texture before drawing.</note>
 *
 * Return value: (transfer full): A newly allocated #cg_sub_texture_t
 *          representing a sub-region of @parent_texture.
 *
 * Stability: unstable
 */
cg_sub_texture_t *cg_sub_texture_new(cg_device_t *ctx,
                                     cg_texture_t *parent_texture,
                                     int sub_x,
                                     int sub_y,
                                     int sub_width,
                                     int sub_height);

/**
 * cg_sub_texture_get_parent:
 * @sub_texture: A pointer to a #cg_sub_texture_t
 *
 * Retrieves the parent texture that @sub_texture derives its content
 * from.  This is the texture that was passed to
 * cg_sub_texture_new() as the parent_texture argument.
 *
 * Return value: (transfer none): The parent texture that @sub_texture
 *               derives its content from.
 * Stability: unstable
 */
cg_texture_t *cg_sub_texture_get_parent(cg_sub_texture_t *sub_texture);

/**
 * cg_is_sub_texture:
 * @object: a #cg_object_t
 *
 * Checks whether @object is a #cg_sub_texture_t.
 *
 * Return value: %true if the passed @object represents a
 *               #cg_sub_texture_t and %false otherwise.
 *
 * Stability: unstable
 */
bool cg_is_sub_texture(void *object);

CG_END_DECLS

#endif /* __CG_SUB_TEXTURE_H */
