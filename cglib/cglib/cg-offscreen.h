/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2012 Intel Corporation.
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

#ifndef __CG_OFFSCREEN_H__
#define __CG_OFFSCREEN_H__

#include <cglib/cg-types.h>
#include <cglib/cg-texture.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-offscreen
 * @short_description: Functions for creating and manipulating offscreen
 *                     framebuffers.
 *
 * CGlib allows creating and operating on offscreen framebuffers.
 */

typedef struct _cg_offscreen_t cg_offscreen_t;

#define CG_OFFSCREEN(X) ((cg_offscreen_t *)X)

/* Offscreen api */

/**
 * cg_offscreen_new_with_texture:
 * @texture: A #cg_texture_t pointer
 *
 * This creates an offscreen framebuffer object using the given
 * @texture as the primary color buffer. It doesn't just initialize
 * the contents of the offscreen buffer with the @texture; they are
 * tightly bound so that drawing to the offscreen buffer effectively
 * updates the contents of the given texture. You don't need to
 * destroy the offscreen buffer before you can use the @texture again.
 *
 * <note>This api only works with low-level #cg_texture_t types such as
 * #cg_texture_2d_t, #cg_texture_3d_t and not with meta-texture types such
 * as #cg_texture_2d_sliced_t.</note>
 *
 * The storage for the framebuffer is actually allocated lazily
 * so this function will never return %NULL to indicate a runtime
 * error. This means it is still possible to configure the framebuffer
 * before it is really allocated.
 *
 * Simple applications without full error handling can simply rely on
 * CGlib to lazily allocate the storage of framebuffers but you should
 * be aware that if CGlib encounters an error (such as running out of
 * GPU memory) then your application will simply abort with an error
 * message. If you need to be able to catch such exceptions at runtime
 * then you can explicitly allocate your framebuffer when you have
 * finished configuring it by calling cg_framebuffer_allocate() and
 * passing in a #cg_error_t argument to catch any exceptions.
 *
 * Return value: (transfer full): a newly instantiated #cg_offscreen_t
 *   framebuffer.
 */
cg_offscreen_t *cg_offscreen_new_with_texture(cg_texture_t *texture);

cg_offscreen_t *
cg_offscreen_new(cg_device_t *dev,
                 int width, /* -1 derived from attached texture */
                 int height); /* -1 derived from attached texture */

void
cg_offscreen_attach_color_texture(cg_offscreen_t *offscreen,
                                  cg_texture_t *texture,
                                  int level);

void
cg_offscreen_attach_depth_texture(cg_offscreen_t *offscreen,
                                  cg_texture_t *texture,
                                  int level);

/**
 * cg_is_offscreen:
 * @object: A pointer to a #cg_object_t
 *
 * Determines whether the given #cg_object_t references an offscreen
 * framebuffer object.
 *
 * Returns: %true if @object is a #cg_offscreen_t framebuffer,
 *          %false otherwise
 */
bool cg_is_offscreen(void *object);

CG_END_DECLS

#endif /* __CG_OFFSCREEN_H__ */
