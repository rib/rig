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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __CG_FRAMEBUFFER_H
#define __CG_FRAMEBUFFER_H

#ifdef CG_HAS_WIN32_SUPPORT
#include <windows.h>
#endif /* CG_HAS_WIN32_SUPPORT */

#include <clib.h>

/* We forward declare the cg_framebuffer_t type here to avoid some circular
 * dependency issues with the following headers.
 */
#ifdef __CG_H_INSIDE__
/* For the public C api we typedef interface types as void to avoid needing
 * lots of casting in code and instead we will rely on runtime type checking
 * for these objects. */
typedef void cg_framebuffer_t;
#else
typedef struct _cg_framebuffer_t cg_framebuffer_t;
#define CG_FRAMEBUFFER(X) ((cg_framebuffer_t *)(X))
#endif

#include <cglib/cg-pipeline.h>
#include <cglib/cg-indices.h>
#include <cglib/cg-bitmap.h>
#include <cglib/cg-texture.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-framebuffer
 * @short_description: A common interface for manipulating framebuffers
 *
 * Framebuffers are a collection of buffers that can be rendered too.
 * A framebuffer may be comprised of one or more color buffers, an
 * optional depth buffer and an optional stencil buffer. Other
 * configuration parameters are associated with framebuffers too such
 * as whether the framebuffer supports multi-sampling (an anti-aliasing
 * technique) or dithering.
 *
 * There are two kinds of framebuffer in CGlib, #cg_onscreen_t
 * framebuffers and #cg_offscreen_t framebuffers. As the names imply
 * offscreen framebuffers are for rendering something offscreen
 * (perhaps to a texture which is bound as one of the color buffers).
 * The exact semantics of onscreen framebuffers depends on the window
 * system backend that you are using, but typically you can expect
 * rendering to a #cg_onscreen_t framebuffer will be immediately
 * visible to the user.
 *
 * If you want to create a new framebuffer then you should start by
 * looking at the #cg_onscreen_t and #cg_offscreen_t constructor
 * functions, such as cg_offscreen_new_with_texture() or
 * cg_onscreen_new(). The #cg_framebuffer_t interface deals with
 * all aspects that are common between those two types of framebuffer.
 *
 * Setup of a new cg_framebuffer_t happens in two stages. There is a
 * configuration stage where you specify all the options and ancillary
 * buffers you want associated with your framebuffer and then when you
 * are happy with the configuration you can "allocate" the framebuffer
 * using cg_framebuffer_allocate(). Technically explicitly calling
 * cg_framebuffer_allocate() is optional for convenience and the
 * framebuffer will automatically be allocated when you first try to
 * draw to it, but if you do the allocation manually then you can
 * also catch any possible errors that may arise from your
 * configuration.
 */

/**
 * cg_framebuffer_allocate:
 * @framebuffer: A #cg_framebuffer_t
 * @error: A pointer to a #cg_error_t for returning exceptions.
 *
 * Explicitly allocates a configured #cg_framebuffer_t allowing developers to
 * check and handle any errors that might arise from an unsupported
 * configuration so that fallback configurations may be tried.
 *
 * <note>Many applications don't support any fallback options at least when
 * they are initially developed and in that case the don't need to use this API
 * since CGlib will automatically allocate a framebuffer when it first gets
 * used.  The disadvantage of relying on automatic allocation is that the
 * program will abort with an error message if there is an error during
 * automatic allocation.</note>
 *
 * Return value: %true if there were no error allocating the framebuffer, else
 *%false.
 * Stability: unstable
 */
bool cg_framebuffer_allocate(cg_framebuffer_t *framebuffer, cg_error_t **error);

/**
 * cg_framebuffer_get_width:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Queries the current width of the given @framebuffer.
 *
 * Return value: The width of @framebuffer.
 * Stability: unstable
 */
int cg_framebuffer_get_width(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_height:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Queries the current height of the given @framebuffer.
 *
 * Return value: The height of @framebuffer.
 * Stability: unstable
 */
int cg_framebuffer_get_height(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_set_viewport:
 * @framebuffer: A #cg_framebuffer_t
 * @x: The top-left x coordinate of the viewport origin (only integers
 *     supported currently)
 * @y: The top-left y coordinate of the viewport origin (only integers
 *     supported currently)
 * @width: The width of the viewport (only integers supported currently)
 * @height: The height of the viewport (only integers supported currently)
 *
 * Defines a scale and offset for everything rendered relative to the
 * top-left of the destination framebuffer.
 *
 * By default the viewport has an origin of (0,0) and width and height
 * that match the framebuffer's size. Assuming a default projection and
 * modelview matrix then you could translate the contents of a window
 * down and right by leaving the viewport size unchanged by moving the
 * offset to (10,10). The viewport coordinates are measured in pixels.
 * If you left the x and y origin as (0,0) you could scale the windows
 * contents down by specify and width and height that's half the real
 * size of the framebuffer.
 *
 * <note>Although the function takes floating point arguments, existing
 * drivers only allow the use of integer values. In the future floating
 * point values will be exposed via a checkable feature.</note>
 *
 * Stability: unstable
 */
void cg_framebuffer_set_viewport(
    cg_framebuffer_t *framebuffer, float x, float y, float width, float height);

/**
 * cg_framebuffer_get_viewport_x:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Queries the x coordinate of the viewport origin as set using
 * cg_framebuffer_set_viewport()
 * or the default value which is 0.
 *
 * Return value: The x coordinate of the viewport origin.
 * Stability: unstable
 */
float cg_framebuffer_get_viewport_x(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_viewport_y:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Queries the y coordinate of the viewport origin as set using
 * cg_framebuffer_set_viewport()
 * or the default value which is 0.
 *
 * Return value: The y coordinate of the viewport origin.
 * Stability: unstable
 */
float cg_framebuffer_get_viewport_y(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_viewport_width:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Queries the width of the viewport as set using cg_framebuffer_set_viewport()
 * or the default value which is the width of the framebuffer.
 *
 * Return value: The width of the viewport.
 * Stability: unstable
 */
float cg_framebuffer_get_viewport_width(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_viewport_height:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Queries the height of the viewport as set using cg_framebuffer_set_viewport()
 * or the default value which is the height of the framebuffer.
 *
 * Return value: The height of the viewport.
 * Stability: unstable
 */
float cg_framebuffer_get_viewport_height(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_viewport4fv:
 * @framebuffer: A #cg_framebuffer_t
 * @viewport: (out caller-allocates) (array fixed-size=4): A pointer to an
 *            array of 4 floats to receive the (x, y, width, height)
 *            components of the current viewport.
 *
 * Queries the x, y, width and height components of the current viewport as set
 * using cg_framebuffer_set_viewport() or the default values which are 0, 0,
 * framebuffer_width and framebuffer_height.  The values are written into the
 * given @viewport array.
 *
 * Stability: unstable
 */
void cg_framebuffer_get_viewport4fv(cg_framebuffer_t *framebuffer,
                                    float *viewport);

/**
 * cg_framebuffer_push_matrix:
 * @framebuffer: A #cg_framebuffer_t pointer
 *
 * Copies the current model-view matrix onto the matrix stack. The matrix
 * can later be restored with cg_framebuffer_pop_matrix().
 *
 */
void cg_framebuffer_push_matrix(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_pop_matrix:
 * @framebuffer: A #cg_framebuffer_t pointer
 *
 * Restores the model-view matrix on the top of the matrix stack.
 *
 */
void cg_framebuffer_pop_matrix(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_identity_matrix:
 * @framebuffer: A #cg_framebuffer_t pointer
 *
 * Resets the current model-view matrix to the identity matrix.
 *
 * Stability: unstable
 */
void cg_framebuffer_identity_matrix(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_scale:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @x: Amount to scale along the x-axis
 * @y: Amount to scale along the y-axis
 * @z: Amount to scale along the z-axis
 *
 * Multiplies the current model-view matrix by one that scales the x,
 * y and z axes by the given values.
 *
 * Stability: unstable
 */
void
cg_framebuffer_scale(cg_framebuffer_t *framebuffer, float x, float y, float z);

/**
 * cg_framebuffer_translate:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @x: Distance to translate along the x-axis
 * @y: Distance to translate along the y-axis
 * @z: Distance to translate along the z-axis
 *
 * Multiplies the current model-view matrix by one that translates the
 * model along all three axes according to the given values.
 *
 * Stability: unstable
 */
void cg_framebuffer_translate(cg_framebuffer_t *framebuffer,
                              float x,
                              float y,
                              float z);

/**
 * cg_framebuffer_rotate:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @angle: Angle in degrees to rotate.
 * @x: X-component of vertex to rotate around.
 * @y: Y-component of vertex to rotate around.
 * @z: Z-component of vertex to rotate around.
 *
 * Multiplies the current model-view matrix by one that rotates the
 * model around the axis-vector specified by @x, @y and @z. The
 * rotation follows the right-hand thumb rule so for example rotating
 * by 10 degrees about the axis-vector (0, 0, 1) causes a small
 * counter-clockwise rotation.
 *
 * Stability: unstable
 */
void cg_framebuffer_rotate(
    cg_framebuffer_t *framebuffer, float angle, float x, float y, float z);

/**
 * cg_framebuffer_rotate_quaternion:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @quaternion: A #c_quaternion_t
 *
 * Multiplies the current model-view matrix by one that rotates
 * according to the rotation described by @quaternion.
 *
 * Stability: unstable
 */
void cg_framebuffer_rotate_quaternion(cg_framebuffer_t *framebuffer,
                                      const c_quaternion_t *quaternion);

/**
 * cg_framebuffer_rotate_euler:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @euler: A #c_euler_t
 *
 * Multiplies the current model-view matrix by one that rotates
 * according to the rotation described by @euler.
 *
 * Stability: unstable
 */
void cg_framebuffer_rotate_euler(cg_framebuffer_t *framebuffer,
                                 const c_euler_t *euler);

/**
 * cg_framebuffer_transform:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @matrix: the matrix to multiply with the current model-view
 *
 * Multiplies the current model-view matrix by the given matrix.
 *
 * Stability: unstable
 */
void cg_framebuffer_transform(cg_framebuffer_t *framebuffer,
                              const c_matrix_t *matrix);

/**
 * cg_framebuffer_get_modelview_matrix:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @matrix: (out): return location for the model-view matrix
 *
 * Stores the current model-view matrix in @matrix.
 *
 * Stability: unstable
 */
void cg_framebuffer_get_modelview_matrix(cg_framebuffer_t *framebuffer,
                                         c_matrix_t *matrix);

/**
 * cg_framebuffer_set_modelview_matrix:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @matrix: the new model-view matrix
 *
 * Sets @matrix as the new model-view matrix.
 *
 * Stability: unstable
 */
void cg_framebuffer_set_modelview_matrix(cg_framebuffer_t *framebuffer,
                                         const c_matrix_t *matrix);

/**
 * cg_framebuffer_perspective:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @fov_y: Vertical field of view angle in degrees.
 * @aspect: The (width over height) aspect ratio for display
 * @z_near: The distance to the near clipping plane (Must be positive,
 *   and must not be 0)
 * @z_far: The distance to the far clipping plane (Must be positive)
 *
 * Replaces the current projection matrix with a perspective matrix
 * based on the provided values.
 *
 * <note>You should be careful not to have to great a @z_far / @z_near
 * ratio since that will reduce the effectiveness of depth testing
 * since there wont be enough precision to identify the depth of
 * objects near to each other.</note>
 *
 * Stability: unstable
 */
void cg_framebuffer_perspective(cg_framebuffer_t *framebuffer,
                                float fov_y,
                                float aspect,
                                float z_near,
                                float z_far);

/**
 * cg_framebuffer_frustum:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @left: X position of the left clipping plane where it
 *   intersects the near clipping plane
 * @right: X position of the right clipping plane where it
 *   intersects the near clipping plane
 * @bottom: Y position of the bottom clipping plane where it
 *   intersects the near clipping plane
 * @top: Y position of the top clipping plane where it intersects
 *   the near clipping plane
 * @z_near: The distance to the near clipping plane (Must be positive)
 * @z_far: The distance to the far clipping plane (Must be positive)
 *
 * Replaces the current projection matrix with a perspective matrix
 * for a given viewing frustum defined by 4 side clip planes that
 * all cross through the origin and 2 near and far clip planes.
 *
 * Stability: unstable
 */
void cg_framebuffer_frustum(cg_framebuffer_t *framebuffer,
                            float left,
                            float right,
                            float bottom,
                            float top,
                            float z_near,
                            float z_far);

/**
 * cg_framebuffer_orthographic:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @x_1: The x coordinate for the first vertical clipping plane
 * @y_1: The y coordinate for the first horizontal clipping plane
 * @x_2: The x coordinate for the second vertical clipping plane
 * @y_2: The y coordinate for the second horizontal clipping plane
 * @near: The <emphasis>distance</emphasis> to the near clipping
 *   plane (will be <emphasis>negative</emphasis> if the plane is
 *   behind the viewer)
 * @far: The <emphasis>distance</emphasis> to the far clipping
 *   plane (will be <emphasis>negative</emphasis> if the plane is
 *   behind the viewer)
 *
 * Replaces the current projection matrix with an orthographic projection
 * matrix.
 *
 * Stability: unstable
 */
void cg_framebuffer_orthographic(cg_framebuffer_t *framebuffer,
                                 float x_1,
                                 float y_1,
                                 float x_2,
                                 float y_2,
                                 float near,
                                 float far);

/**
 * cg_framebuffer_get_projection_matrix:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @matrix: (out): return location for the projection matrix
 *
 * Stores the current projection matrix in @matrix.
 *
 * Stability: unstable
 */
void cg_framebuffer_get_projection_matrix(cg_framebuffer_t *framebuffer,
                                          c_matrix_t *matrix);

/**
 * cg_framebuffer_set_projection_matrix:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @matrix: the new projection matrix
 *
 * Sets @matrix as the new projection matrix.
 *
 * Stability: unstable
 */
void cg_framebuffer_set_projection_matrix(cg_framebuffer_t *framebuffer,
                                          const c_matrix_t *matrix);

/**
 * cg_framebuffer_push_scissor_clip:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @x: left edge of the clip rectangle in window coordinates
 * @y: top edge of the clip rectangle in window coordinates
 * @width: width of the clip rectangle
 * @height: height of the clip rectangle
 *
 * Specifies a rectangular clipping area for all subsequent drawing
 * operations. Any drawing commands that extend outside the rectangle
 * will be clipped so that only the portion inside the rectangle will
 * be displayed. The rectangle dimensions are not transformed by the
 * current model-view matrix.
 *
 * The rectangle is intersected with the current clip region. To undo
 * the effect of this function, call cg_framebuffer_pop_clip().
 *
 * Stability: unstable
 */
void cg_framebuffer_push_scissor_clip(
    cg_framebuffer_t *framebuffer, int x, int y, int width, int height);

/**
 * cg_framebuffer_push_rectangle_clip:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @x_1: x coordinate for top left corner of the clip rectangle
 * @y_1: y coordinate for top left corner of the clip rectangle
 * @x_2: x coordinate for bottom right corner of the clip rectangle
 * @y_2: y coordinate for bottom right corner of the clip rectangle
 *
 * Specifies a modelview transformed rectangular clipping area for all
 * subsequent drawing operations. Any drawing commands that extend
 * outside the rectangle will be clipped so that only the portion
 * inside the rectangle will be displayed. The rectangle dimensions
 * are transformed by the current model-view matrix.
 *
 * The rectangle is intersected with the current clip region. To undo
 * the effect of this function, call cg_framebuffer_pop_clip().
 *
 * Stability: unstable
 */
void cg_framebuffer_push_rectangle_clip(
    cg_framebuffer_t *framebuffer, float x_1, float y_1, float x_2, float y_2);

/**
 * cg_framebuffer_push_primitive_clip:
 * @framebuffer: A #cg_framebuffer_t pointer
 * @primitive: A #cg_primitive_t describing a flat 2D shape
 * @bounds_x1: x coordinate for the top-left corner of the primitives
 *             bounds
 * @bounds_y1: y coordinate for the top-left corner of the primitives
 *             bounds
 * @bounds_x2: x coordinate for the bottom-right corner of the
 *             primitives bounds.
 * @bounds_y2: y coordinate for the bottom-right corner of the
 *             primitives bounds.
 *
 * Sets a new clipping area using a 2D shaped described with a
 * #cg_primitive_t. The shape must not contain self overlapping
 * geometry and must lie on a single 2D plane. A bounding box of the
 * 2D shape in local coordinates (the same coordinates used to
 * describe the shape) must be given. It is acceptable for the bounds
 * to be larger than the true bounds but behaviour is undefined if the
 * bounds are smaller than the true bounds.
 *
 * The primitive is transformed by the current model-view matrix and
 * the silhouette is intersected with the previous clipping area.  To
 * restore the previous clipping area, call
 * cg_framebuffer_pop_clip().
 *
 * Stability: unstable
 */
void cg_framebuffer_push_primitive_clip(cg_framebuffer_t *framebuffer,
                                        cg_primitive_t *primitive,
                                        float bounds_x1,
                                        float bounds_y1,
                                        float bounds_x2,
                                        float bounds_y2);

/**
 * cg_framebuffer_pop_clip:
 * @framebuffer: A #cg_framebuffer_t pointer
 *
 * Reverts the clipping region to the state before the last call to
 * cg_framebuffer_push_scissor_clip(), cg_framebuffer_push_rectangle_clip()
 * cg_framebuffer_push_path_clip(), or cg_framebuffer_push_primitive_clip().
 *
 * Stability: unstable
 */
void cg_framebuffer_pop_clip(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_red_bits:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 *
 * Retrieves the number of red bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Stability: unstable
 */
int cg_framebuffer_get_red_bits(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_green_bits:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 *
 * Retrieves the number of green bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Stability: unstable
 */
int cg_framebuffer_get_green_bits(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_blue_bits:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 *
 * Retrieves the number of blue bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Stability: unstable
 */
int cg_framebuffer_get_blue_bits(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_alpha_bits:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 *
 * Retrieves the number of alpha bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Stability: unstable
 */
int cg_framebuffer_get_alpha_bits(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_depth_bits:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 *
 * Retrieves the number of depth bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Stability: unstable
 */
int cg_framebuffer_get_depth_bits(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_dither_enabled:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 *
 * Returns whether dithering has been requested for the given @framebuffer.
 * See cg_framebuffer_set_dither_enabled() for more details about dithering.
 *
 * <note>This may return %true even when the underlying @framebuffer
 * display pipeline does not support dithering. This value only represents
 * the user's request for dithering.</note>
 *
 * Return value: %true if dithering has been requested or %false if not.
 * Stability: unstable
 */
bool cg_framebuffer_get_dither_enabled(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_set_dither_enabled:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 * @dither_enabled: %true to enable dithering or %false to disable
 *
 * Enables or disabled dithering if supported by the hardware.
 *
 * Dithering is a hardware dependent technique to increase the visible
 * color resolution beyond what the underlying hardware supports by playing
 * tricks with the colors placed into the framebuffer to give the illusion
 * of other colors. (For example this can be compared to half-toning used
 * by some news papers to show varying levels of grey even though their may
 * only be black and white are available).
 *
 * If the current display pipeline for @framebuffer does not support dithering
 * then this has no affect.
 *
 * Dithering is enabled by default.
 *
 * Stability: unstable
 */
void cg_framebuffer_set_dither_enabled(cg_framebuffer_t *framebuffer,
                                       bool dither_enabled);

/**
 * cg_framebuffer_get_depth_write_enabled:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 *
 * Queries whether depth buffer writing is enabled for @framebuffer. This
 * can be controlled via cg_framebuffer_set_depth_write_enabled().
 *
 * Return value: %true if depth writing is enabled or %false if not.
 * Stability: unstable
 */
bool cg_framebuffer_get_depth_write_enabled(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_set_depth_write_enabled:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 * @depth_write_enabled: %true to enable depth writing or %false to disable
 *
 * Enables or disables depth buffer writing when rendering to @framebuffer.
 * If depth writing is enabled for both the framebuffer and the rendering
 * pipeline, and the framebuffer has an associated depth buffer, depth
 * information will be written to this buffer during rendering.
 *
 * Depth buffer writing is enabled by default.
 *
 * Stability: unstable
 */
void cg_framebuffer_set_depth_write_enabled(cg_framebuffer_t *framebuffer,
                                            bool depth_write_enabled);

/**
 * cg_framebuffer_get_color_mask:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 *
 * Gets the current #cg_color_mask_t of which channels would be written to the
 * current framebuffer. Each bit set in the mask means that the
 * corresponding color would be written.
 *
 * Returns: A #cg_color_mask_t
 * Stability: unstable
 */
cg_color_mask_t cg_framebuffer_get_color_mask(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_set_color_mask:
 * @framebuffer: a pointer to a #cg_framebuffer_t
 * @color_mask: A #cg_color_mask_t of which color channels to write to
 *              the current framebuffer.
 *
 * Defines a bit mask of which color channels should be written to the
 * given @framebuffer. If a bit is set in @color_mask that means that
 * color will be written.
 *
 * Stability: unstable
 */
void cg_framebuffer_set_color_mask(cg_framebuffer_t *framebuffer,
                                   cg_color_mask_t color_mask);

/**
 * cg_framebuffer_get_color_format:
 * @framebuffer: A #cg_framebuffer_t framebuffer
 *
 * Queries the common #cg_pixel_format_t of all color buffers attached
 * to this framebuffer. For an offscreen framebuffer created with
 * cg_offscreen_new_with_texture() this will correspond to the format
 * of the texture.
 *
 * Stability: unstable
 */
cg_pixel_format_t
cg_framebuffer_get_color_format(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_set_depth_texture_enabled:
 * @framebuffer: A #cg_framebuffer_t
 * @enabled: true or false
 *
 * If @enabled is #true, the depth buffer used when rendering to @framebuffer
 * is available as a texture. You can retrieve the texture with
 * cg_framebuffer_get_depth_texture().
 *
 * <note>It's possible that your GPU does not support depth textures. You
 * should check the %CG_FEATURE_ID_DEPTH_TEXTURE feature before using this
 * function.</note>
 * <note>It's not valid to call this function after the framebuffer has been
 * allocated as the creation of the depth texture is done at allocation time.
 * </note>
 *
 */
void cg_framebuffer_set_depth_texture_enabled(cg_framebuffer_t *framebuffer,
                                              bool enabled);

/**
 * cg_framebuffer_get_depth_texture_enabled:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Queries whether texture based depth buffer has been enabled via
 * cg_framebuffer_set_depth_texture_enabled().
 *
 * Return value: %true if a depth texture has been enabled, else
 *               %false.
 *
 */
bool cg_framebuffer_get_depth_texture_enabled(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_get_depth_texture:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Retrieves the depth buffer of @framebuffer as a #cg_texture_t. You
 * must have called cg_framebuffer_set_depth_texture(fb, true);
 * before using this function.
 *
 * If the returned texture is subsequently added to a pipeline layer
 * and sampled from, then depth values should be read from the red
 * component of the texture. The values of other components are
 * undefined.
 *
 * <note>Calling this function implicitly allocates the
 * framebuffer.</note>
 *
 * <note>The returned texture pointer is only guaranteed to remain
 * valid as long as @framebuffer stays valid, but it's safe to keep
 * the texture alive for longer by taking a reference on the returned
 * texture.</note>
 *
 * Returns: (transfer none): the depth texture
 *
 */
cg_texture_t *cg_framebuffer_get_depth_texture(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_set_samples_per_pixel:
 * @framebuffer: A #cg_framebuffer_t framebuffer
 * @samples_per_pixel: The minimum number of samples per pixel
 *
 * Requires that when rendering to @framebuffer then @n point samples
 * should be made per pixel which will all contribute to the final
 * resolved color for that pixel. The idea is that the hardware aims
 * to get quality similar to what you would get if you rendered
 * everything twice as big (for 4 samples per pixel) and then scaled
 * that image back down with filtering. It can effectively remove the
 * jagged edges of polygons and should be more efficient than if you
 * were to manually render at a higher resolution and downscale
 * because the hardware is often able to take some shortcuts. For
 * example the GPU may only calculate a single texture sample for all
 * points of a single pixel, and for tile based architectures all the
 * extra sample data (such as depth and stencil samples) may be
 * handled on-chip and so avoid increased demand on system memory
 * bandwidth.
 *
 * By default this value is usually set to 0 and that is referred to
 * as "single-sample" rendering. A value of 1 or greater is referred
 * to as "multisample" rendering.
 *
 * <note>There are some semantic differences between single-sample
 * rendering and multisampling with just 1 point sample such as it
 * being redundant to use the cg_framebuffer_resolve_samples() and
 * cg_framebuffer_resolve_samples_region() apis with single-sample
 * rendering.</note>
 *
 * <note>It's recommended that
 * cg_framebuffer_resolve_samples_region() be explicitly used at the
 * end of rendering to a point sample buffer to minimize the number of
 * samples that get resolved. By default CGlib will implicitly resolve
 * all framebuffer samples but if only a small region of a
 * framebuffer has changed this can lead to redundant work being
 * done.</note>
 *
 * Stability: unstable
 */
void cg_framebuffer_set_samples_per_pixel(cg_framebuffer_t *framebuffer,
                                          int samples_per_pixel);

/**
 * cg_framebuffer_get_samples_per_pixel:
 * @framebuffer: A #cg_framebuffer_t framebuffer
 *
 * Gets the number of points that are sampled per-pixel when
 * rasterizing geometry. Usually by default this will return 0 which
 * means that single-sample not multisample rendering has been chosen.
 * When using a GPU supporting multisample rendering it's possible to
 * increase the number of samples per pixel using
 * cg_framebuffer_set_samples_per_pixel().
 *
 * Calling cg_framebuffer_get_samples_per_pixel() before the
 * framebuffer has been allocated will simply return the value set
 * using cg_framebuffer_set_samples_per_pixel(). After the
 * framebuffer has been allocated the value will reflect the actual
 * number of samples that will be made by the GPU.
 *
 * Returns: The number of point samples made per pixel when
 *          rasterizing geometry or 0 if single-sample rendering
 *          has been chosen.
 *
 * Stability: unstable
 */
int cg_framebuffer_get_samples_per_pixel(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_resolve_samples:
 * @framebuffer: A #cg_framebuffer_t framebuffer
 *
 * When point sample rendering (also known as multisample rendering)
 * has been enabled via cg_framebuffer_set_samples_per_pixel()
 * then you can optionally call this function (or
 * cg_framebuffer_resolve_samples_region()) to explicitly resolve
 * the point samples into values for the final color buffer.
 *
 * Some GPUs will implicitly resolve the point samples during
 * rendering and so this function is effectively a nop, but with other
 * architectures it is desirable to defer the resolve step until the
 * end of the frame.
 *
 * Since CGlib will automatically ensure samples are resolved if the
 * target color buffer is used as a source this API only needs to be
 * used if explicit control is desired - perhaps because you want to
 * ensure that the resolve is completed in advance to avoid later
 * having to wait for the resolve to complete.
 *
 * If you are performing incremental updates to a framebuffer you
 * should consider using cg_framebuffer_resolve_samples_region()
 * instead to avoid resolving redundant pixels.
 *
 * Stability: unstable
 */
void cg_framebuffer_resolve_samples(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_resolve_samples_region:
 * @framebuffer: A #cg_framebuffer_t framebuffer
 * @x: top-left x coordinate of region to resolve
 * @y: top-left y coordinate of region to resolve
 * @width: width of region to resolve
 * @height: height of region to resolve
 *
 * When point sample rendering (also known as multisample rendering)
 * has been enabled via cg_framebuffer_set_samples_per_pixel()
 * then you can optionally call this function (or
 * cg_framebuffer_resolve_samples()) to explicitly resolve the point
 * samples into values for the final color buffer.
 *
 * Some GPUs will implicitly resolve the point samples during
 * rendering and so this function is effectively a nop, but with other
 * architectures it is desirable to defer the resolve step until the
 * end of the frame.
 *
 * Use of this API is recommended if incremental, small updates to
 * a framebuffer are being made because by default CGlib will
 * implicitly resolve all the point samples of the framebuffer which
 * can result in redundant work if only a small number of samples have
 * changed.
 *
 * Because some GPUs implicitly resolve point samples this function
 * only guarantees that at-least the region specified will be resolved
 * and if you have rendered to a larger region then it's possible that
 * other samples may be implicitly resolved.
 *
 * Stability: unstable
 */
void cg_framebuffer_resolve_samples_region(
    cg_framebuffer_t *framebuffer, int x, int y, int width, int height);

/**
 * cg_framebuffer_get_context:
 * @framebuffer: A #cg_framebuffer_t
 *
 * Can be used to query the #cg_device_t a given @framebuffer was
 * instantiated within. This is the #cg_device_t that was passed to
 * cg_onscreen_new() for example.
 *
 * Return value: (transfer none): The #cg_device_t that the given
 *               @framebuffer was instantiated within.
 * Stability: unstable
 */
cg_device_t *cg_framebuffer_get_context(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_clear:
 * @framebuffer: A #cg_framebuffer_t
 * @buffers: A mask of #cg_buffer_bit_t<!-- -->'s identifying which auxiliary
 *   buffers to clear
 * @color: The color to clear the color buffer too if specified in
 *         @buffers.
 *
 * Clears all the auxiliary buffers identified in the @buffers mask, and if
 * that includes the color buffer then the specified @color is used.
 *
 * Stability: unstable
 */
void cg_framebuffer_clear(cg_framebuffer_t *framebuffer,
                          cg_buffer_bit_t buffers,
                          const cg_color_t *color);

/**
 * cg_framebuffer_clear4f:
 * @framebuffer: A #cg_framebuffer_t
 * @buffers: A mask of #cg_buffer_bit_t<!-- -->'s identifying which auxiliary
 *   buffers to clear
 * @red: The red component of color to clear the color buffer too if
 *       specified in @buffers.
 * @green: The green component of color to clear the color buffer too if
 *         specified in @buffers.
 * @blue: The blue component of color to clear the color buffer too if
 *        specified in @buffers.
 * @alpha: The alpha component of color to clear the color buffer too if
 *         specified in @buffers.
 *
 * Clears all the auxiliary buffers identified in the @buffers mask, and if
 * that includes the color buffer then the specified @color is used.
 *
 * Stability: unstable
 */
void cg_framebuffer_clear4f(cg_framebuffer_t *framebuffer,
                            cg_buffer_bit_t buffers,
                            float red,
                            float green,
                            float blue,
                            float alpha);

/**
 * cg_framebuffer_draw_rectangle:
 * @framebuffer: A destination #cg_framebuffer_t
 * @pipeline: A #cg_pipeline_t state object
 * @x_1: X coordinate of the top-left corner
 * @y_1: Y coordinate of the top-left corner
 * @x_2: X coordinate of the bottom-right corner
 * @y_2: Y coordinate of the bottom-right corner
 *
 * Draws a rectangle to @framebuffer with the given @pipeline state
 * and with the top left corner positioned at (@x_1, @y_1) and the
 * bottom right corner positioned at (@x_2, @y_2).
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * <note>If you want to describe a rectangle with a texture mapped on
 * it then you can use
 * cg_framebuffer_draw_textured_rectangle().</note>
 *
 * Stability: unstable
 */
void cg_framebuffer_draw_rectangle(cg_framebuffer_t *framebuffer,
                                   cg_pipeline_t *pipeline,
                                   float x_1,
                                   float y_1,
                                   float x_2,
                                   float y_2);

/**
 * cg_framebuffer_draw_textured_rectangle:
 * @framebuffer: A destination #cg_framebuffer_t
 * @pipeline: A #cg_pipeline_t state object
 * @x_1: x coordinate upper left on screen.
 * @y_1: y coordinate upper left on screen.
 * @x_2: x coordinate lower right on screen.
 * @y_2: y coordinate lower right on screen.
 * @s_1: S texture coordinate of the top-left coorner
 * @t_1: T texture coordinate of the top-left coorner
 * @s_2: S texture coordinate of the bottom-right coorner
 * @t_2: T texture coordinate of the bottom-right coorner
 *
 * Draws a textured rectangle to @framebuffer using the given
 * @pipeline state with the top left corner positioned at (@x_1, @y_1)
 * and the bottom right corner positioned at (@x_2, @y_2). The top
 * left corner will have texture coordinates of (@s_1, @t_1) and the
 * bottom right corner will have texture coordinates of (@s_2, @t_2).
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * This is a high level drawing api that can handle any kind of
 * #cg_meta_texture_t texture such as #cg_texture_2d_sliced_t textures
 * which may internally be comprised of multiple low-level textures.
 * This is unlike low-level drawing apis such as cg_primitive_draw()
 * which only support low level texture types that are directly
 * supported by GPUs such as #cg_texture_2d_t.
 *
 * <note>The given texture coordinates will only be used for the first
 * texture layer of the pipeline and if your pipeline has more than
 * one layer then all other layers will have default texture
 * coordinates of @s_1=0.0 @t_1=0.0 @s_2=1.0 @t_2=1.0 </note>
 *
 * The given texture coordinates should always be normalized such that
 * (0, 0) corresponds to the top left and (1, 1) corresponds to the
 * bottom right. To map an entire texture across the rectangle pass
 * in @s_1=0, @t_1=0, @s_2=1, @t_2=1.
 *
 * Stability: unstable
 */
void cg_framebuffer_draw_textured_rectangle(cg_framebuffer_t *framebuffer,
                                            cg_pipeline_t *pipeline,
                                            float x_1,
                                            float y_1,
                                            float x_2,
                                            float y_2,
                                            float s_1,
                                            float t_1,
                                            float s_2,
                                            float t_2);

/**
 * cg_framebuffer_draw_multitextured_rectangle:
 * @framebuffer: A destination #cg_framebuffer_t
 * @pipeline: A #cg_pipeline_t state object
 * @x_1: x coordinate upper left on screen.
 * @y_1: y coordinate upper left on screen.
 * @x_2: x coordinate lower right on screen.
 * @y_2: y coordinate lower right on screen.
 * @tex_coords: (in) (array) (transfer none): An array containing groups of
 *   4 float values: [s_1, t_1, s_2, t_2] that are interpreted as two texture
 *   coordinates; one for the top left texel, and one for the bottom right
 *   texel. Each value should be between 0.0 and 1.0, where the coordinate
 *   (0.0, 0.0) represents the top left of the texture, and (1.0, 1.0) the
 *   bottom right.
 * @tex_coords_len: The length of the @tex_coords array. (For one layer
 *   and one group of texture coordinates, this would be 4)
 *
 * Draws a textured rectangle to @framebuffer with the given @pipeline
 * state with the top left corner positioned at (@x_1, @y_1) and the
 * bottom right corner positioned at (@x_2, @y_2). As a pipeline may
 * contain multiple texture layers this interface lets you supply
 * texture coordinates for each layer of the pipeline.
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * This is a high level drawing api that can handle any kind of
 * #cg_meta_texture_t texture for the first layer such as
 * #cg_texture_2d_sliced_t textures which may internally be comprised of
 * multiple low-level textures.  This is unlike low-level drawing apis
 * such as cg_primitive_draw() which only support low level texture
 * types that are directly supported by GPUs such as #cg_texture_2d_t.
 *
 * <note>This api can not currently handle multiple high-level meta
 * texture layers. The first layer may be a high level meta texture
 * such as #cg_texture_2d_sliced_t but all other layers must be low
 * level textures such as #cg_texture_2d_t.</note>
 *
 * The top left texture coordinate for layer 0 of any pipeline will be
 * (tex_coords[0], tex_coords[1]) and the bottom right coordinate will
 * be (tex_coords[2], tex_coords[3]). The coordinates for layer 1
 * would be (tex_coords[4], tex_coords[5]) (tex_coords[6],
 * tex_coords[7]) and so on...
 *
 * The given texture coordinates should always be normalized such that
 * (0, 0) corresponds to the top left and (1, 1) corresponds to the
 * bottom right. To map an entire texture across the rectangle pass
 * in tex_coords[0]=0, tex_coords[1]=0, tex_coords[2]=1,
 * tex_coords[3]=1.
 *
 * The first pair of coordinates are for the first layer (with the
 * smallest layer index) and if you supply less texture coordinates
 * than there are layers in the current source material then default
 * texture coordinates (0.0, 0.0, 1.0, 1.0) are generated.
 *
 * Stability: unstable
 */
void cg_framebuffer_draw_multitextured_rectangle(cg_framebuffer_t *framebuffer,
                                                 cg_pipeline_t *pipeline,
                                                 float x_1,
                                                 float y_1,
                                                 float x_2,
                                                 float y_2,
                                                 const float *tex_coords,
                                                 int tex_coords_len);

/**
 * cg_framebuffer_draw_rectangles:
 * @framebuffer: A destination #cg_framebuffer_t
 * @pipeline: A #cg_pipeline_t state object
 * @coordinates: (in) (array) (transfer none): an array of coordinates
 *   containing groups of 4 float values: [x_1, y_1, x_2, y_2] that are
 *   interpreted as two position coordinates; one for the top left of
 *   the rectangle (x1, y1), and one for the bottom right of the
 *   rectangle (x2, y2).
 * @n_rectangles: number of rectangles defined in @coordinates.
 *
 * Draws a series of rectangles to @framebuffer with the given
 * @pipeline state in the same way that
 * cg_framebuffer_draw_rectangle() does.
 *
 * The top left corner of the first rectangle is positioned at
 * (coordinates[0], coordinates[1]) and the bottom right corner is
 * positioned at (coordinates[2], coordinates[3]). The positions for
 * the second rectangle are (coordinates[4], coordinates[5]) and
 * (coordinates[6], coordinates[7]) and so on...
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * As a general rule for better performance its recommended to use
 * this this API instead of calling
 * cg_framebuffer_draw_textured_rectangle() separately for multiple
 * rectangles if all of the rectangles will be drawn together with the
 * same @pipeline state.
 *
 * Stability: unstable
 */
void cg_framebuffer_draw_rectangles(cg_framebuffer_t *framebuffer,
                                    cg_pipeline_t *pipeline,
                                    const float *coordinates,
                                    unsigned int n_rectangles);

/**
 * cg_framebuffer_draw_textured_rectangles:
 * @framebuffer: A destination #cg_framebuffer_t
 * @pipeline: A #cg_pipeline_t state object
 * @coordinates: (in) (array) (transfer none): an array containing
 *   groups of 8 float values: [x_1, y_1, x_2, y_2, s_1, t_1, s_2, t_2]
 *   that have the same meaning as the arguments for
 *   cg_framebuffer_draw_textured_rectangle().
 * @n_rectangles: number of rectangles to @coordinates to draw
 *
 * Draws a series of rectangles to @framebuffer with the given
 * @pipeline state in the same way that
 * cg_framebuffer_draw_textured_rectangle() does.
 *
 * <note>The position is the position before the rectangle has been
 * transformed by the model-view matrix and the projection
 * matrix.</note>
 *
 * This is a high level drawing api that can handle any kind of
 * #cg_meta_texture_t texture such as #cg_texture_2d_sliced_t textures
 * which may internally be comprised of multiple low-level textures.
 * This is unlike low-level drawing apis such as cg_primitive_draw()
 * which only support low level texture types that are directly
 * supported by GPUs such as #cg_texture_2d_t.
 *
 * The top left corner of the first rectangle is positioned at
 * (coordinates[0], coordinates[1]) and the bottom right corner is
 * positioned at (coordinates[2], coordinates[3]). The top left
 * texture coordinate is (coordinates[4], coordinates[5]) and the
 * bottom right texture coordinate is (coordinates[6],
 * coordinates[7]). The coordinates for subsequent rectangles
 * are defined similarly by the subsequent coordinates.
 *
 * As a general rule for better performance its recommended to use
 * this this API instead of calling
 * cg_framebuffer_draw_textured_rectangle() separately for multiple
 * rectangles if all of the rectangles will be drawn together with the
 * same @pipeline state.
 *
 * The given texture coordinates should always be normalized such that
 * (0, 0) corresponds to the top left and (1, 1) corresponds to the
 * bottom right. To map an entire texture across the rectangle pass
 * in tex_coords[0]=0, tex_coords[1]=0, tex_coords[2]=1,
 * tex_coords[3]=1.
 *
 * Stability: unstable
 */
void cg_framebuffer_draw_textured_rectangles(cg_framebuffer_t *framebuffer,
                                             cg_pipeline_t *pipeline,
                                             const float *coordinates,
                                             unsigned int n_rectangles);

/* XXX: Should we take an n_buffers + buffer id array instead of using
 * the cg_buffer_bit_ts type which doesn't seem future proof? */
/**
 * cg_framebuffer_discard_buffers:
 * @framebuffer: A #cg_framebuffer_t
 * @buffers: A #cg_buffer_bit_t mask of which ancillary buffers you want
 *           to discard.
 *
 * Declares that the specified @buffers no longer need to be referenced
 * by any further rendering commands. This can be an important
 * optimization to avoid subsequent frames of rendering depending on
 * the results of a previous frame.
 *
 * For example; some tile-based rendering GPUs are able to avoid allocating and
 * accessing system memory for the depth and stencil buffer so long as these
 * buffers are not required as input for subsequent frames and that can save a
 * significant amount of memory bandwidth used to save and restore their
 * contents to system memory between frames.
 *
 * It is currently considered an error to try and explicitly discard the color
 * buffer by passing %CG_BUFFER_BIT_COLOR. This is because the color buffer is
 * already implicitly discard when you finish rendering to a #cg_onscreen_t
 * framebuffer, and it's not meaningful to try and discard the color buffer of
 * a #cg_offscreen_t framebuffer since they are single-buffered.
 *
 *
 * Stability: unstable
 */
void cg_framebuffer_discard_buffers(cg_framebuffer_t *framebuffer,
                                    cg_buffer_bit_t buffers);

/**
 * cg_framebuffer_finish:
 * @framebuffer: A #cg_framebuffer_t pointer
 *
 * This blocks the CPU until all pending rendering associated with the
 * specified framebuffer has completed. It's very rare that developers should
 * ever need this level of synchronization with the GPU and should never be
 * used unless you clearly understand why you need to explicitly force
 * synchronization.
 *
 * One example might be for benchmarking purposes to be sure timing
 * measurements reflect the time that the GPU is busy for not just the time it
 * takes to queue rendering commands.
 *
 * Stability: unstable
 */
void cg_framebuffer_finish(cg_framebuffer_t *framebuffer);

/**
 * cg_framebuffer_read_pixels_into_bitmap:
 * @framebuffer: A #cg_framebuffer_t
 * @x: The x position to read from
 * @y: The y position to read from
 * @source: Identifies which auxillary buffer you want to read
 *          (only CG_READ_PIXELS_COLOR_BUFFER supported currently)
 * @bitmap: The bitmap to store the results in.
 * @error: A #cg_error_t to catch exceptional errors
 *
 * This reads a rectangle of pixels from the given framebuffer where
 * position (0, 0) is the top left. The pixel at (x, y) is the first
 * read, and a rectangle of pixels with the same size as the bitmap is
 * read right and downwards from that point.
 *
 * Currently CGlib assumes that the framebuffer is in a premultiplied
 * format so if the format of @bitmap is non-premultiplied it will
 * convert it. To read the pixel values without any conversion you
 * should either specify a format that doesn't use an alpha channel or
 * use one of the formats ending in PRE.
 *
 * Return value: %true if the read succeeded or %false otherwise. The
 *  function is only likely to fail if the bitmap points to a pixel
 *  buffer and it could not be mapped.
 * Stability: unstable
 */
bool cg_framebuffer_read_pixels_into_bitmap(cg_framebuffer_t *framebuffer,
                                            int x,
                                            int y,
                                            cg_read_pixels_flags_t source,
                                            cg_bitmap_t *bitmap,
                                            cg_error_t **error);

/**
 * cg_framebuffer_read_pixels:
 * @framebuffer: A #cg_framebuffer_t
 * @x: The x position to read from
 * @y: The y position to read from
 * @width: The width of the region of rectangles to read
 * @height: The height of the region of rectangles to read
 * @format: The pixel format to store the data in
 * @pixels: The address of the buffer to store the data in
 *
 * This is a convenience wrapper around
 * cg_framebuffer_read_pixels_into_bitmap() which allocates a
 * temporary #cg_bitmap_t to read pixel data directly into the given
 * buffer. The rowstride of the buffer is assumed to be the width of
 * the region times the bytes per pixel of the format. The source for
 * the data is always taken from the color buffer. If you want to use
 * any other rowstride or source, please use the
 * cg_framebuffer_read_pixels_into_bitmap() function directly.
 *
 * The implementation of the function looks like this:
 *
 * |[
 * bitmap = cg_bitmap_new_for_data (context,
 *                                    width, height,
 *                                    format,
 *                                    /<!-- -->* rowstride *<!-- -->/
 *                                    bpp * width,
 *                                    pixels);
 * cg_framebuffer_read_pixels_into_bitmap (framebuffer,
 *                                           x, y,
 *                                           CG_READ_PIXELS_COLOR_BUFFER,
 *                                           bitmap);
 * cg_object_unref (bitmap);
 * ]|
 *
 * Return value: %true if the read succeeded or %false otherwise.
 * Stability: unstable
 */
bool cg_framebuffer_read_pixels(cg_framebuffer_t *framebuffer,
                                int x,
                                int y,
                                int width,
                                int height,
                                cg_pixel_format_t format,
                                uint8_t *pixels);

uint32_t cg_framebuffer_error_domain(void);

/**
 * CG_FRAMEBUFFER_ERROR:
 *
 * An error domain for reporting #cg_framebuffer_t exceptions
 */
#define CG_FRAMEBUFFER_ERROR (cg_framebuffer_error_domain())

typedef enum { /*< prefix=CG_FRAMEBUFFER_ERROR >*/
    CG_FRAMEBUFFER_ERROR_ALLOCATE
} cg_framebuffer_error_t;

/**
 * cg_is_framebuffer:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a #cg_framebuffer_t.
 *
 * Return value: %true if the object references a #cg_framebuffer_t
 *   and %false otherwise.
 * Stability: unstable
 */
bool cg_is_framebuffer(void *object);

cg_device_t *cg_framebuffer_get_device(cg_framebuffer_t *fb);

CG_END_DECLS

#endif /* __CG_FRAMEBUFFER_H */
