/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_META_TEXTURE_H__
#define __CG_META_TEXTURE_H__

#include <cglib/cg-pipeline-layer-state.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-meta-texture
 * @short_description: Interface for high-level textures built from
 *                     low-level textures like #cg_texture_2d_t and
 *                     #cg_texture_3d_t.
 *
 * CGlib helps to make it easy to deal with high level textures such
 * as #cg_atlas_texture_t<!-- -->s, #cg_sub_texture_t<!-- -->s,
 * #cg_texture_pixmap_x11_t textures and #cg_texture_2d_sliced_t textures
 * consistently.
 *
 * A #cg_meta_texture_t is a texture that might internally be
 * represented by one or more low-level #cg_texture_t<!-- -->s
 * such as #cg_texture_2d_t or #cg_texture_3d_t. These low-level textures
 * are the only ones that a GPU really understands but because
 * applications often want more high-level texture abstractions
 * (such as storing multiple textures inside one larger "atlas"
 * texture) it's desirable to be able to deal with these
 * using a common interface.
 *
 * For example the GPU is not able to automatically handle repeating a
 * texture that is part of a larger atlas texture but if you use
 * %CG_PIPELINE_WRAP_MODE_REPEAT with an atlas texture when drawing
 * with cg_framebuffer_draw_rectangle() you should see that it "Just
 * Works™" - at least if you don't use multi-texturing. The reason
 * this works is because cg_framebuffer_draw_rectangle() internally
 * understands the #cg_meta_texture_t interface and is able to manually
 * resolve the low-level textures using this interface and by making
 * multiple draw calls it can emulate the texture repeat modes.
 *
 * CGlib doesn't aim to pretend that meta-textures are just like real
 * textures because it would get extremely complex to try and emulate
 * low-level GPU semantics transparently for these textures.  The low
 * level drawing APIs of CGlib, such as cg_primitive_draw() don't
 * actually know anything about the #cg_meta_texture_t interface and its
 * the developer's responsibility to resolve all textures referenced
 * by a #cg_pipeline_t to low-level textures before drawing.
 *
 * If you want to develop custom primitive APIs like
 * cg_framebuffer_draw_rectangle() and you want to support drawing
 * with #cg_atlas_texture_t<!-- -->s or #cg_sub_texture_t<!-- -->s for
 * example, then you will need to use this #cg_meta_texture_t interface
 * to be able to resolve high-level textures into low-level textures
 * before drawing with CGlib's low-level drawing APIs such as
 * cg_primitive_draw().
 *
 * <note>Most developers won't need to use this interface directly
 * but still it is worth understanding the distinction between
 * low-level and meta textures because you may find other references
 * in the documentation that detail limitations of using
 * meta-textures.</note>
 */

#ifdef __CG_H_INSIDE__
/* For the public C api we typedef interface types as void to avoid needing
 * lots of casting in code and instead we will rely on runtime type checking
 * for these objects. */
typedef void cg_meta_texture_t;
#else
typedef struct _cg_meta_texture_t cg_meta_texture_t;
#define CG_META_TEXTURE(X) ((cg_meta_texture_t *)X)
#endif

/**
 * cg_meta_texture_callback_t:
 * @sub_texture: A low-level #cg_texture_t making up part of a
 *               #cg_meta_texture_t.
 * @sub_texture_coords: A float 4-tuple ordered like
 *                      (tx1,ty1,tx2,ty2) defining what region of the
 *                      current @sub_texture maps to a sub-region of a
 *                      #cg_meta_texture_t. (tx1,ty1) is the top-left
 *                      sub-region coordinate and (tx2,ty2) is the
 *                      bottom-right. These are low-level texture
 *                      coordinates.
 * @meta_coords: A float 4-tuple ordered like (tx1,ty1,tx2,ty2)
 *               defining what sub-region of a #cg_meta_texture_t this
 *               low-level @sub_texture maps too. (tx1,ty1) is
 *               the top-left sub-region coordinate and (tx2,ty2) is
 *               the bottom-right. These are high-level meta-texture
 *               coordinates.
 * @user_data: A private pointer passed to
 *             cg_meta_texture_foreach_in_region().
 *
 * A callback used with cg_meta_texture_foreach_in_region() to
 * retrieve details of all the low-level #cg_texture_t<!-- -->s that
 * make up a given #cg_meta_texture_t.
 *
 * Stability: unstable
 */
typedef void (*cg_meta_texture_callback_t)(cg_texture_t *sub_texture,
                                           const float *sub_texture_coords,
                                           const float *meta_coords,
                                           void *user_data);

/**
 * cg_meta_texture_foreach_in_region:
 * @meta_texture: An object implementing the #cg_meta_texture_t
 *                interface.
 * @tx_1: The top-left x coordinate of the region to iterate
 * @ty_1: The top-left y coordinate of the region to iterate
 * @tx_2: The bottom-right x coordinate of the region to iterate
 * @ty_2: The bottom-right y coordinate of the region to iterate
 * @wrap_s: The wrap mode for the x-axis
 * @wrap_t: The wrap mode for the y-axis
 * @callback: A #cg_meta_texture_callback_t pointer to be called
 *            for each low-level texture within the specified region.
 * @user_data: A private pointer that is passed to @callback.
 *
 * Allows you to manually iterate the low-level textures that define a
 * given region of a high-level #cg_meta_texture_t.
 *
 * For example cg_texture_2d_sliced_new_with_size() can be used to
 * create a meta texture that may slice a large image into multiple,
 * smaller power-of-two sized textures. These high level textures are
 * not directly understood by a GPU and so this API must be used to
 * manually resolve the underlying textures for drawing.
 *
 * All high level textures (#cg_atlas_texture_t, #cg_sub_texture_t,
 * #cg_texture_pixmap_x11_t, and #cg_texture_2d_sliced_t) can be handled
 * consistently using this interface which greately simplifies
 * implementing primitives that support all texture types.
 *
 * For example if you use the cg_framebuffer_draw_rectangle() API
 * then CGlib will internally use this API to resolve the low level
 * textures of any meta textures you have associated with cg_pipeline_t
 * layers.
 *
 * <note>The low level drawing APIs such as cg_primitive_draw()
 * don't understand the #cg_meta_texture_t interface and so it is your
 * responsibility to use this API to resolve all cg_pipeline_t textures
 * into low-level textures before drawing.</note>
 *
 * For each low-level texture that makes up part of the given region
 * of the @meta_texture, @callback is called specifying how the
 * low-level texture maps to the original region.
 *
 * Stability: unstable
 */
void cg_meta_texture_foreach_in_region(cg_meta_texture_t *meta_texture,
                                       float tx_1,
                                       float ty_1,
                                       float tx_2,
                                       float ty_2,
                                       cg_pipeline_wrap_mode_t wrap_s,
                                       cg_pipeline_wrap_mode_t wrap_t,
                                       cg_meta_texture_callback_t callback,
                                       void *user_data);

CG_END_DECLS

#endif /* __CG_META_TEXTURE_H__ */
