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

#ifndef __CG_DRIVER_H
#define __CG_DRIVER_H

#include "cg-device.h"
#include "cg-offscreen.h"
#include "cg-framebuffer-private.h"
#include "cg-attribute-private.h"

typedef struct _cg_driver_vtable_t cg_driver_vtable_t;

struct _cg_driver_vtable_t {
    /* TODO: factor this out since this is OpenGL specific and
     * so can be ignored by non-OpenGL drivers. */
    bool (*pixel_format_from_gl_internal)(cg_device_t *dev,
                                          GLenum gl_int_format,
                                          cg_pixel_format_t *out_format);

    /* TODO: factor this out since this is OpenGL specific and
     * so can be ignored by non-OpenGL drivers. */
    cg_pixel_format_t (*pixel_format_to_gl)(cg_device_t *dev,
                                            cg_pixel_format_t format,
                                            GLenum *out_glintformat,
                                            GLenum *out_glformat,
                                            GLenum *out_gltype);

    bool (*update_features)(cg_device_t *dev, cg_error_t **error);

    bool (*offscreen_allocate)(cg_offscreen_t *offscreen, cg_error_t **error);

    void (*offscreen_free)(cg_offscreen_t *offscreen);

    void (*framebuffer_flush_state)(cg_framebuffer_t *draw_buffer,
                                    cg_framebuffer_t *read_buffer,
                                    cg_framebuffer_state_t state);

    void (*framebuffer_clear)(cg_framebuffer_t *framebuffer,
                              unsigned long buffers,
                              float red,
                              float green,
                              float blue,
                              float alpha);

    void (*framebuffer_query_bits)(cg_framebuffer_t *framebuffer,
                                   cg_framebuffer_bits_t *bits);

    void (*framebuffer_finish)(cg_framebuffer_t *framebuffer);

    void (*framebuffer_discard_buffers)(cg_framebuffer_t *framebuffer,
                                        unsigned long buffers);

    void (*framebuffer_draw_attributes)(cg_framebuffer_t *framebuffer,
                                        cg_pipeline_t *pipeline,
                                        cg_vertices_mode_t mode,
                                        int first_vertex,
                                        int n_vertices,
                                        cg_attribute_t **attributes,
                                        int n_attributes,
                                        int n_instances,
                                        cg_draw_flags_t flags);

    void (*framebuffer_draw_indexed_attributes)(cg_framebuffer_t *framebuffer,
                                                cg_pipeline_t *pipeline,
                                                cg_vertices_mode_t mode,
                                                int first_vertex,
                                                int n_vertices,
                                                cg_indices_t *indices,
                                                cg_attribute_t **attributes,
                                                int n_attributes,
                                                int n_instances,
                                                cg_draw_flags_t flags);

    bool (*framebuffer_read_pixels_into_bitmap)(cg_framebuffer_t *framebuffer,
                                                int x,
                                                int y,
                                                cg_read_pixels_flags_t source,
                                                cg_bitmap_t *bitmap,
                                                cg_error_t **error);

    /* Destroys any driver specific resources associated with the given
     * 2D texture. */
    void (*texture_2d_free)(cg_texture_2d_t *tex_2d);

    /* Returns true if the driver can support creating a 2D texture with
     * the given geometry and specified internal format.
     */
    bool (*texture_2d_can_create)(cg_device_t *dev,
                                  int width,
                                  int height,
                                  cg_pixel_format_t internal_format);

    /* Initializes driver private state before allocating any specific
     * storage for a 2D texture, where base texture and texture 2D
     * members will already be initialized before passing control to
     * the driver.
     */
    void (*texture_2d_init)(cg_texture_2d_t *tex_2d);

    /* Allocates (uninitialized) storage for the given texture according
     * to the configured size and format of the texture */
    bool (*texture_2d_allocate)(cg_texture_t *tex, cg_error_t **error);

    /* Initialize the specified region of storage of the given texture
     * with the contents of the specified framebuffer region
     */
    void (*texture_2d_copy_from_framebuffer)(cg_texture_2d_t *tex_2d,
                                             int src_x,
                                             int src_y,
                                             int width,
                                             int height,
                                             cg_framebuffer_t *src_fb,
                                             int dst_x,
                                             int dst_y,
                                             int level);

    /* If the given texture has a corresponding OpenGL texture handle
     * then return that.
     *
     * This is optional
     */
    unsigned int (*texture_2d_get_gl_handle)(cg_texture_2d_t *tex_2d);

    /* Update all mipmap levels > 0 */
    void (*texture_2d_generate_mipmap)(cg_texture_2d_t *tex_2d);

    /* Initialize the specified region of storage of the given texture
     * with the contents of the specified bitmap region
     *
     * Since this may need to create the underlying storage first
     * it may throw a NO_MEMORY error.
     */
    bool (*texture_2d_copy_from_bitmap)(cg_texture_2d_t *tex_2d,
                                        int src_x,
                                        int src_y,
                                        int width,
                                        int height,
                                        cg_bitmap_t *bitmap,
                                        int dst_x,
                                        int dst_y,
                                        int level,
                                        cg_error_t **error);

    /* Reads back the full contents of the given texture and write it to
     * @data in the given @format and with the given @rowstride.
     *
     * This is optional
     */
    void (*texture_2d_get_data)(cg_texture_2d_t *tex_2d,
                                cg_pixel_format_t format,
                                int rowstride,
                                uint8_t *data);

    /* Prepares for drawing by flushing the framebuffer state,
     * pipeline state and attribute state.
     */
    void (*flush_attributes_state)(cg_framebuffer_t *framebuffer,
                                   cg_pipeline_t *pipeline,
                                   cg_flush_layer_state_t *layer_state,
                                   cg_draw_flags_t flags,
                                   cg_attribute_t **attributes,
                                   int n_attributes);

    /* Flushes the clip stack to the GPU using a combination of the
     * stencil buffer, scissor and clip plane state.
     */
    void (*clip_stack_flush)(cg_clip_stack_t *stack,
                             cg_framebuffer_t *framebuffer);

    /* Enables the driver to create some meta data to represent a buffer
     * but with no corresponding storage allocated yet.
     */
    void (*buffer_create)(cg_buffer_t *buffer);

    void (*buffer_destroy)(cg_buffer_t *buffer);

    /* Maps a buffer into the CPU */
    void *(*buffer_map_range)(cg_buffer_t *buffer,
                              size_t offset,
                              size_t size,
                              cg_buffer_access_t access,
                              cg_buffer_map_hint_t hints,
                              cg_error_t **error);

    /* Unmaps a buffer */
    void (*buffer_unmap)(cg_buffer_t *buffer);

    /* Uploads data to the buffer without needing to map it necessarily
     */
    bool (*buffer_set_data)(cg_buffer_t *buffer,
                            unsigned int offset,
                            const void *data,
                            unsigned int size,
                            cg_error_t **error);
};

#define CG_DRIVER_ERROR (_cg_driver_error_domain())

typedef enum { /*< prefix=CG_DRIVER_ERROR >*/
    CG_DRIVER_ERROR_UNKNOWN_VERSION,
    CG_DRIVER_ERROR_INVALID_VERSION,
    CG_DRIVER_ERROR_NO_SUITABLE_DRIVER_FOUND,
    CG_DRIVER_ERROR_FAILED_TO_LOAD_LIBRARY
} cg_driver_error_t;

uint32_t _cg_driver_error_domain(void);

#endif /* __CG_DRIVER_H */
