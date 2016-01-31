/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __CG_FRAMEBUFFER_PRIVATE_H
#define __CG_FRAMEBUFFER_PRIVATE_H

#include "cg-object-private.h"
#include "cg-matrix-stack-private.h"
#include "cg-winsys-private.h"
#include "cg-attribute-private.h"
#include "cg-offscreen.h"
#include "cg-gl-header.h"
#include "cg-clip-stack.h"

#ifdef CG_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#endif

#ifdef CG_HAS_GLX_SUPPORT
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

typedef enum _cg_framebuffer_type_t {
    CG_FRAMEBUFFER_TYPE_ONSCREEN,
    CG_FRAMEBUFFER_TYPE_OFFSCREEN
} cg_framebuffer_type_t;

typedef struct {
    bool has_alpha;
    bool need_stencil;
    int samples_per_pixel;
    bool swap_throttled;
    bool depth_texture_enabled;
} cg_framebuffer_config_t;

/* Flags to pass to _cg_offscreen_new_with_texture_full */
typedef enum {
    CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL = 1
} cg_offscreen_flags_t;

/* XXX: The order of these indices determines the order they are
 * flushed.
 *
 * Flushing clip state may trash the modelview and projection matrices
 * so we must do it before flushing the matrices.
 */
typedef enum _cg_framebuffer_state_index_t {
    CG_FRAMEBUFFER_STATE_INDEX_BIND = 0,
    CG_FRAMEBUFFER_STATE_INDEX_VIEWPORT = 1,
    CG_FRAMEBUFFER_STATE_INDEX_CLIP = 2,
    CG_FRAMEBUFFER_STATE_INDEX_DITHER = 3,
    CG_FRAMEBUFFER_STATE_INDEX_MODELVIEW = 4,
    CG_FRAMEBUFFER_STATE_INDEX_PROJECTION = 5,
    CG_FRAMEBUFFER_STATE_INDEX_COLOR_MASK = 6,
    CG_FRAMEBUFFER_STATE_INDEX_FRONT_FACE_WINDING = 7,
    CG_FRAMEBUFFER_STATE_INDEX_DEPTH_WRITE = 8,
    CG_FRAMEBUFFER_STATE_INDEX_MAX = 9
} cg_framebuffer_state_index_t;

typedef enum _cg_framebuffer_state_t {
    CG_FRAMEBUFFER_STATE_BIND = 1 << 0,
    CG_FRAMEBUFFER_STATE_VIEWPORT = 1 << 1,
    CG_FRAMEBUFFER_STATE_CLIP = 1 << 2,
    CG_FRAMEBUFFER_STATE_DITHER = 1 << 3,
    CG_FRAMEBUFFER_STATE_MODELVIEW = 1 << 4,
    CG_FRAMEBUFFER_STATE_PROJECTION = 1 << 5,
    CG_FRAMEBUFFER_STATE_COLOR_MASK = 1 << 6,
    CG_FRAMEBUFFER_STATE_FRONT_FACE_WINDING = 1 << 7,
    CG_FRAMEBUFFER_STATE_DEPTH_WRITE = 1 << 8
} cg_framebuffer_state_t;

#define CG_FRAMEBUFFER_STATE_ALL ((1 << CG_FRAMEBUFFER_STATE_INDEX_MAX) - 1)

/* Private flags that can internally be added to cg_read_pixels_flags_t */
typedef enum {
    /* If this is set then the data will not be flipped to compensate
       for GL's upside-down coordinate system but instead will be left
       in whatever order GL gives us (which will depend on whether the
       framebuffer is offscreen or not) */
    CG_READ_PIXELS_NO_FLIP = 1L << 30
} cg_private_read_pixels_flags_t;

typedef struct {
    int red;
    int blue;
    int green;
    int alpha;
    int depth;
    int stencil;
} cg_framebuffer_bits_t;

struct _cg_framebuffer_t {
    cg_object_t _parent;
    cg_device_t *dev;
    cg_framebuffer_type_t type;

    /* The user configuration before allocation... */
    cg_framebuffer_config_t config;

    int width;
    int height;
    /* Format of the pixels in the framebuffer (including the expected
       premult state) */
    cg_pixel_format_t internal_format;
    bool allocated;

    cg_matrix_stack_t *modelview_stack;
    cg_matrix_stack_t *projection_stack;
    float viewport_x;
    float viewport_y;
    float viewport_width;
    float viewport_height;
    int viewport_age;
    int viewport_age_for_scissor_workaround;

    cg_clip_stack_t *clip_stack;

    bool dither_enabled;
    bool depth_writing_enabled;
    cg_color_mask_t color_mask;

    /* The scene of a given framebuffer may depend on images in other
     * framebuffers... */
    c_llist_t *deps;

    /* Whether something has been drawn to the buffer since the last
     * swap buffers or swap region. */
    bool mid_scene;

    /* driver specific */
    bool dirty_bitmasks;
    cg_framebuffer_bits_t bits;

    int samples_per_pixel;
};

typedef enum {
    CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL = 1L << 0,
    CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH = 1L << 1,
    CG_OFFSCREEN_ALLOCATE_FLAG_STENCIL = 1L << 2
} cg_offscreen_allocate_flags_t;

typedef struct _cg_gl_framebuffer_t {
    GLuint fbo_handle;
    c_llist_t *renderbuffers;
    int samples_per_pixel;
} cg_gl_framebuffer_t;

struct _cg_offscreen_t {
    cg_framebuffer_t _parent;

    cg_gl_framebuffer_t gl_framebuffer;

    /* TODO: generalise the handling of framebuffer attachments... */

    cg_texture_t *texture;
    int texture_level;

    cg_texture_t *depth_texture;
    int depth_texture_level;

    cg_offscreen_allocate_flags_t allocation_flags;

    /* FIXME: _cg_offscreen_new_with_texture_full should be made to use
     * fb->config to configure if we want a depth or stencil buffer so
     * we can get rid of these flags */
    cg_offscreen_flags_t create_flags;
};

void _cg_framebuffer_init(cg_framebuffer_t *framebuffer,
                          cg_device_t *dev,
                          cg_framebuffer_type_t type,
                          int width,
                          int height);

/* XXX: For a public api we might instead want a way to explicitly
 * set the _premult status of a framebuffer or what components we
 * care about instead of exposing the cg_pixel_format_t
 * internal_format.
 *
 * The current use case for this api is where we create an offscreen
 * framebuffer for a shared atlas texture that has a format of
 * RGBA_8888 disregarding the premultiplied alpha status for
 * individual atlased textures or whether the alpha component is being
 * discarded. We want to overried the internal_format that will be
 * derived from the texture.
 */
void _cg_framebuffer_set_internal_format(cg_framebuffer_t *framebuffer,
                                         cg_pixel_format_t internal_format);

void _cg_framebuffer_free(cg_framebuffer_t *framebuffer);

const cg_winsys_vtable_t *
_cg_framebuffer_get_winsys(cg_framebuffer_t *framebuffer);

/* This api bypasses flushing the framebuffer state */
void _cg_framebuffer_clear_without_flush4f(cg_framebuffer_t *framebuffer,
                                           cg_buffer_bit_t buffers,
                                           float red,
                                           float green,
                                           float blue,
                                           float alpha);

void _cg_framebuffer_mark_mid_scene(cg_framebuffer_t *framebuffer);

/*
 * _cg_framebuffer_get_clip_stack:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Gets a pointer to the current clip stack. This can be used to later
 * return to the same clip stack state with
 * _cg_framebuffer_set_clip_stack(). A reference is not taken on the
 * stack so if you want to keep it you should call
 * _cg_clip_stack_ref().
 *
 * Return value: a pointer to the @framebuffer clip stack.
 */
cg_clip_stack_t *_cg_framebuffer_get_clip_stack(cg_framebuffer_t *framebuffer);

/*
 * _cg_framebuffer_set_clip_stack:
 * @framebuffer: A #cg_framebuffer_t
 * @stack: a pointer to the replacement clip stack
 *
 * Replaces the @framebuffer clip stack with @stack.
 */
void _cg_framebuffer_set_clip_stack(cg_framebuffer_t *framebuffer,
                                    cg_clip_stack_t *stack);

cg_matrix_stack_t *
_cg_framebuffer_get_modelview_stack(cg_framebuffer_t *framebuffer);

cg_matrix_stack_t *
_cg_framebuffer_get_projection_stack(cg_framebuffer_t *framebuffer);

void _cg_framebuffer_add_dependency(cg_framebuffer_t *framebuffer,
                                    cg_framebuffer_t *dependency);

void _cg_framebuffer_remove_all_dependencies(cg_framebuffer_t *framebuffer);

void _cg_framebuffer_flush_state(cg_framebuffer_t *draw_buffer,
                                 cg_framebuffer_t *read_buffer,
                                 cg_framebuffer_state_t state);

/*
 * _cg_offscreen_new_with_texture_full:
 * @texture: A #cg_texture_t pointer
 * @create_flags: Flags specifying how to create the FBO
 * @level: The mipmap level within the texture to target
 *
 * Creates a new offscreen buffer which will target the given
 * texture. By default the buffer will have a depth and stencil
 * buffer. This can be disabled by passing
 * %CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL in @create_flags.
 *
 * Return value: the new cg_offscreen_t object.
 */
cg_offscreen_t *_cg_offscreen_new_with_texture_full(
    cg_texture_t *texture, cg_offscreen_flags_t create_flags, int level);

/*
 * _cg_push_framebuffers:
 * @draw_buffer: A pointer to the buffer used for drawing
 * @read_buffer: A pointer to the buffer used for reading back pixels
 *
 * Redirects drawing and reading to the specified framebuffers as in
 * cg_push_framebuffer() except that it allows the draw and read
 * buffer to be different. The buffers are pushed as a pair so that
 * they can later both be restored with a single call to
 * cg_pop_framebuffer().
 */
void _cg_push_framebuffers(cg_framebuffer_t *draw_buffer,
                           cg_framebuffer_t *read_buffer);

/*
 * _cg_blit_framebuffer:
 * @src: The source #cg_framebuffer_t
 * @dest: The destination #cg_framebuffer_t
 * @src_x: Source x position
 * @src_y: Source y position
 * @dst_x: Destination x position
 * @dst_y: Destination y position
 * @width: Width of region to copy
 * @height: Height of region to copy
 *
 * This blits a region of the color buffer of the current draw buffer
 * to the current read buffer. The draw and read buffers can be set up
 * using _cg_push_framebuffers(). This function should only be
 * called if the CG_PRIVATE_FEATURE_OFFSCREEN_BLIT feature is
 * advertised. The two buffers must both be offscreen and have the
 * same format.
 *
 * Note that this function differs a lot from the glBlitFramebuffer
 * function provided by the GL_EXT_framebuffer_blit extension. Notably
 * it doesn't support having different sizes for the source and
 * destination rectangle. This isn't supported by the corresponding
 * GL_ANGLE_framebuffer_blit extension on GLES2.0 and it doesn't seem
 * like a particularly useful feature. If the application wanted to
 * scale the results it may make more sense to draw a primitive
 * instead.
 *
 * We can only really support blitting between two offscreen buffers
 * for this function on GLES2.0. This is because we effectively render
 * upside down to offscreen buffers to maintain CGlib's representation
 * of the texture coordinate system where 0,0 is the top left of the
 * texture. If we were to blit from an offscreen to an onscreen buffer
 * then we would need to mirror the blit along the x-axis but the GLES
 * extension does not support this.
 *
 * The GL function is documented to be affected by the scissor. This
 * function therefore ensure that an empty clip stack is flushed
 * before performing the blit which means the scissor is effectively
 * ignored.
 *
 * The function also doesn't support specifying the buffers to copy
 * and instead only the color buffer is copied. When copying the depth
 * or stencil buffers the extension on GLES2.0 only supports copying
 * the full buffer which would be awkward to document with this
 * API. If we wanted to support that feature it may be better to have
 * a separate function to copy the entire buffer for a given mask.
 */
void _cg_blit_framebuffer(cg_framebuffer_t *src,
                          cg_framebuffer_t *dest,
                          int src_x,
                          int src_y,
                          int dst_x,
                          int dst_y,
                          int width,
                          int height);

void _cg_framebuffer_push_projection(cg_framebuffer_t *framebuffer);

void _cg_framebuffer_pop_projection(cg_framebuffer_t *framebuffer);

void _cg_framebuffer_save_clip_stack(cg_framebuffer_t *framebuffer);

void _cg_framebuffer_restore_clip_stack(cg_framebuffer_t *framebuffer);

void _cg_framebuffer_unref(cg_framebuffer_t *framebuffer);

/* Drawing with this api will bypass the framebuffer flush and
 * pipeline validation. */
void _cg_framebuffer_draw_attributes(cg_framebuffer_t *framebuffer,
                                     cg_pipeline_t *pipeline,
                                     cg_vertices_mode_t mode,
                                     int first_vertex,
                                     int n_vertices,
                                     cg_attribute_t **attributes,
                                     int n_attributes,
                                     int n_instances,
                                     cg_draw_flags_t flags);

void _cg_framebuffer_draw_indexed_attributes(cg_framebuffer_t *framebuffer,
                                             cg_pipeline_t *pipeline,
                                             cg_vertices_mode_t mode,
                                             int first_vertex,
                                             int n_vertices,
                                             cg_indices_t *indices,
                                             cg_attribute_t **attributes,
                                             int n_attributes,
                                             int n_instances,
                                             cg_draw_flags_t flags);

void _cg_rectangle_immediate(cg_framebuffer_t *framebuffer,
                             cg_pipeline_t *pipeline,
                             float x_1,
                             float y_1,
                             float x_2,
                             float y_2);

bool _cg_framebuffer_try_creating_gl_fbo(cg_device_t *dev,
                                         int width,
                                         int height,
                                         cg_texture_t *texture,
                                         int texture_level,
                                         cg_texture_t *depth_texture,
                                         int depth_texture_level,
                                         cg_framebuffer_config_t *config,
                                         cg_offscreen_allocate_flags_t flags,
                                         cg_gl_framebuffer_t *gl_framebuffer);

unsigned long _cg_framebuffer_compare(cg_framebuffer_t *a,
                                      cg_framebuffer_t *b,
                                      unsigned long state);

static inline cg_matrix_entry_t *
_cg_framebuffer_get_modelview_entry(cg_framebuffer_t *framebuffer)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    return modelview_stack->last_entry;
}

static inline cg_matrix_entry_t *
_cg_framebuffer_get_projection_entry(cg_framebuffer_t *framebuffer)
{
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);
    return projection_stack->last_entry;
}

/*
 * _cg_framebuffer_flush:
 * @framebuffer: A #cg_framebuffer_t
 *
 * This function should only need to be called in exceptional circumstances.
 *
 * As an optimization CGlib drawing functions may batch up primitives
 * internally, so if you are trying to use native drawing apis
 * directly to a CGlib onscreen framebuffer or raw OpenGL outside of
 * CGlib you stand a better chance of being successful if you ask CGlib
 * to flush any batched drawing before issuing your own drawing
 * commands.
 *
 * This api only ensure that the underlying driver is issued all the
 * commands necessary to draw the batched primitives. It provides no
 * guarantees about when the driver will complete the rendering.
 *
 * This provides no guarantees about native graphics state or OpenGL
 * state upon returning and to avoid confusing CGlib you should aim to
 * save and restore any changes you make before resuming use of CGlib.
 *
 * Note: If you make OpenGL state changes with the intention of
 * affecting CGlib drawing primitives you stand a high chance of
 * conflicting with CGlib internals which can change so this is not
 * supported.
 *
 * XXX: We considered making this api public, but actually for the
 * direct use of OpenGL usecase this api isn't really enough since
 * it would leave GL in a completely undefined state which would
 * basically make it inpractical for applications to use. To really
 * support mixing raw GL with CGlib we'd probabably need to guarantee
 * that we reset all GL state to defaults.
 */
void _cg_framebuffer_flush(cg_framebuffer_t *framebuffer);

/*
 * _cg_framebuffer_get_stencil_bits:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 *
 * Retrieves the number of stencil bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Stability: unstable
 */
int _cg_framebuffer_get_stencil_bits(cg_framebuffer_t *framebuffer);

#endif /* __CG_FRAMEBUFFER_PRIVATE_H */
