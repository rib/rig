/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef __CG_PRIMITIVE_TEXTURE_H__
#define __CG_PRIMITIVE_TEXTURE_H__

#include "cg-types.h"

CG_BEGIN_DECLS

/**
 * SECTION:cg-primitive-texture
 * @short_description: Interface for low-level textures like
 *                     #cg_texture_2d_t and #cg_texture_3d_t.
 *
 * A #cg_primitive_texture_t is a texture that is directly represented
 * by a single texture on the GPU, such as #cg_texture_2d_t and
 * #cg_texture_3d_t. This is in contrast to high level meta textures
 * which may be composed of multiple primitive textures or a
 * sub-region of another texture such as #cg_atlas_texture_t and
 * #cg_texture_2d_sliced_t.
 *
 * A texture that implements this interface can be directly used with
 * the low level cg_primitive_draw() API. Other types of textures
 * need to be first resolved to primitive textures using the
 * #cg_meta_texture_t interface.
 *
 * <note>Most developers won't need to use this interface directly but
 * still it is worth understanding the distinction between high-level
 * and primitive textures because you may find other references in the
 * documentation that detail limitations of using
 * primitive textures.</note>
 */

#ifdef __CG_H_INSIDE__
/* For the public C api we typedef interface types as void to avoid needing
 * lots of casting in code and instead we will rely on runtime type checking
 * for these objects. */
typedef void cg_primitive_texture_t;
#else
typedef struct _cg_primitive_texture_t cg_primitive_texture_t;
#define CG_PRIMITIVE_TEXTURE(X) ((cg_primitive_texture_t *)X)
#endif

/**
 * cg_is_primitive_texture:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a primitive texture object.
 *
 * Return value: %true if the pointer references a primitive texture, and
 *   %false otherwise
 * Stability: unstable
 */
bool cg_is_primitive_texture(void *object);

/**
 * cg_primitive_texture_set_auto_mipmap:
 * @primitive_texture: A #cg_primitive_texture_t
 * @value: The new value for whether to auto mipmap
 *
 * Sets whether the texture will automatically update the smaller
 * mipmap levels after any part of level 0 is updated. The update will
 * only occur whenever the texture is used for drawing with a texture
 * filter that requires the lower mipmap levels. An application should
 * disable this if it wants to upload its own data for the other
 * levels. By default auto mipmapping is enabled.
 *
 * Stability: unstable
 */
void
cg_primitive_texture_set_auto_mipmap(cg_primitive_texture_t *primitive_texture,
                                     bool value);

CG_END_DECLS

#endif /* __CG_PRIMITIVE_TEXTURE_H__ */
