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
 */

#ifndef __CG_WAYLAND_SERVER_H
#define __CG_WAYLAND_SERVER_H

#include <wayland-server.h>

/* NB: this is a top-level header that can be included directly but we
 * want to be careful not to define __CG_H_INSIDE__ when this is
 * included internally while building CGlib itself since
 * __CG_H_INSIDE__ is used in headers to guard public vs private api
 * definitions
 */

#ifndef CG_COMPILATION
#define __CG_H_INSIDE__
#endif

#include <cglib/cg-device.h>
#include <cglib/cg-texture-2d.h>

CG_BEGIN_DECLS

/**
 * cg_wayland_display_set_compositor_display:
 * @display: a #cg_display_t
 * @wayland_display: A compositor's Wayland display pointer
 *
 * Informs CGlib of a compositor's Wayland display pointer. This
 * enables CGlib to register private wayland extensions required to
 * pass buffers between the clients and compositor.
 *
 * Stability: unstable
 */
void
cg_wayland_display_set_compositor_display(cg_display_t *display,
                                          struct wl_display *wayland_display);

/**
 * cg_wayland_texture_2d_new_from_buffer:
 * @dev: A #cg_device_t
 * @buffer: A Wayland resource for a buffer
 * @error: A #cg_error_t for exceptions
 *
 * Uploads the @buffer referenced by the given Wayland resource to a
 * #cg_texture_2d_t. The buffer resource may refer to a wl_buffer or a
 * wl_shm_buffer.
 *
 * <note>The results are undefined for passing an invalid @buffer
 * pointer</note>
 * <note>It is undefined if future updates to @buffer outside the
 * control of CGlib will affect the allocated #cg_texture_2d_t. In some
 * cases the contents of the buffer are copied (such as shm buffers),
 * and in other cases the underlying storage is re-used directly (such
 * as drm buffers)</note>
 *
 * Returns: A newly allocated #cg_texture_2d_t, or if CGlib could not
 *          validate the @buffer in some way (perhaps because of
 *          an unsupported format) it will return %NULL and set
 *          @error.
 *
 * Stability: unstable
 */
cg_texture_2d_t *cg_wayland_texture_2d_new_from_buffer(cg_device_t *dev,
						       struct wl_resource *buffer,
						       cg_error_t **error);

/**
 * cg_wayland_texture_set_region_from_shm_buffer:
 * @texture: a #cg_texture_t
 * @width: The width of the region to copy
 * @height: The height of the region to copy
 * @shm_buffer: The source buffer
 * @src_x: The X offset within the source bufer to copy from
 * @src_y: The Y offset within the source bufer to copy from
 * @dst_x: The X offset within the texture to copy to
 * @dst_y: The Y offset within the texture to copy to
 * @level: The mipmap level of the texture to copy to
 * @error: A #cg_error_t to return exceptional errors
 *
 * Sets the pixels in a rectangular subregion of @texture from a
 * Wayland SHM buffer. Generally this would be used in response to
 * wl_surface.damage event in a compositor in order to update the
 * texture with the damaged region. This is just a convenience wrapper
 * around getting the SHM buffer pointer and calling
 * cg_texture_set_region(). See that function for a description of
 * the level parameter.
 *
 * <note>Since the storage for a #cg_texture_t is allocated lazily then
 * if the given @texture has not previously been allocated then this
 * api can return %false and throw an exceptional @error if there is
 * not enough memory to allocate storage for @texture.</note>
 *
 * Return value: %true if the subregion upload was successful, and
 *   %false otherwise
 * Stability: unstable
 */
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
                                              cg_error_t **error);

CG_END_DECLS

#ifndef CG_COMPILATION
#undef __CG_H_INSIDE__
#endif

#endif /* __CG_WAYLAND_SERVER_H */
