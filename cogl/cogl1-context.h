/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_1_CONTEXT_H__
#define __COGL_1_CONTEXT_H__

#include <glib.h>

#include <cogl/cogl-types.h>
#include <cogl/cogl-texture.h>
#include <cogl/cogl-framebuffer.h>

G_BEGIN_DECLS

/**
 * cogl_set_framebuffer:
 * @buffer: A #CoglFramebuffer object, either onscreen or offscreen.
 *
 * This redirects all subsequent drawing to the specified framebuffer. This can
 * either be an offscreen buffer created with cogl_offscreen_new_to_texture ()
 * or in the future it may be an onscreen framebuffers too.
 *
 * Since: 1.2
 */
void
cogl_set_framebuffer (CoglFramebuffer *buffer);

/**
 * cogl_push_framebuffer:
 * @buffer: A #CoglFramebuffer object, either onscreen or offscreen.
 *
 * Redirects all subsequent drawing to the specified framebuffer. This can
 * either be an offscreen buffer created with cogl_offscreen_new_to_texture ()
 * or in the future it may be an onscreen framebuffer too.
 *
 * You should understand that a framebuffer owns the following state:
 * <itemizedlist>
 *  <listitem><simpara>The projection matrix</simpara></listitem>
 *  <listitem><simpara>The modelview matrix stack</simpara></listitem>
 *  <listitem><simpara>The viewport</simpara></listitem>
 *  <listitem><simpara>The clip stack</simpara></listitem>
 * </itemizedlist>
 * So these items will automatically be saved and restored when you
 * push and pop between different framebuffers.
 *
 * Also remember a newly allocated framebuffer will have an identity matrix for
 * the projection and modelview matrices which gives you a coordinate space
 * like OpenGL with (-1, -1) corresponding to the top left of the viewport,
 * (1, 1) corresponding to the bottom right and +z coming out towards the
 * viewer.
 *
 * If you want to set up a coordinate space like Clutter does with (0, 0)
 * corresponding to the top left and (framebuffer_width, framebuffer_height)
 * corresponding to the bottom right you can do so like this:
 *
 * |[
 * static void
 * setup_viewport (unsigned int width,
 *                 unsigned int height,
 *                 float fovy,
 *                 float aspect,
 *                 float z_near,
 *                 float z_far)
 * {
 *   float z_camera;
 *   CoglMatrix projection_matrix;
 *   CoglMatrix mv_matrix;
 *
 *   cogl_set_viewport (0, 0, width, height);
 *   cogl_perspective (fovy, aspect, z_near, z_far);
 *
 *   cogl_get_projection_matrix (&amp;projection_matrix);
 *   z_camera = 0.5 * projection_matrix.xx;
 *
 *   cogl_matrix_init_identity (&amp;mv_matrix);
 *   cogl_matrix_translate (&amp;mv_matrix, -0.5f, -0.5f, -z_camera);
 *   cogl_matrix_scale (&amp;mv_matrix, 1.0f / width, -1.0f / height, 1.0f / width);
 *   cogl_matrix_translate (&amp;mv_matrix, 0.0f, -1.0 * height, 0.0f);
 *   cogl_set_modelview_matrix (&amp;mv_matrix);
 * }
 *
 * static void
 * my_init_framebuffer (ClutterStage *stage,
 *                      CoglFramebuffer *framebuffer,
 *                      unsigned int framebuffer_width,
 *                      unsigned int framebuffer_height)
 * {
 *   ClutterPerspective perspective;
 *
 *   clutter_stage_get_perspective (stage, &perspective);
 *
 *   cogl_push_framebuffer (framebuffer);
 *   setup_viewport (framebuffer_width,
 *                   framebuffer_height,
 *                   perspective.fovy,
 *                   perspective.aspect,
 *                   perspective.z_near,
 *                   perspective.z_far);
 * }
 * ]|
 *
 * The previous framebuffer can be restored by calling cogl_pop_framebuffer()
 *
 * Since: 1.2
 */
void
cogl_push_framebuffer (CoglFramebuffer *buffer);

/**
 * cogl_pop_framebuffer:
 *
 * Restores the framebuffer that was previously at the top of the stack.
 * All subsequent drawing will be redirected to this framebuffer.
 *
 * Since: 1.2
 */
void
cogl_pop_framebuffer (void);

#ifndef COGL_DISABLE_DEPRECATED

/**
 * cogl_push_draw_buffer:
 *
 * Save cogl_set_draw_buffer() state.
 *
 * Deprecated: 1.2: The draw buffer API was replaced with a framebuffer API
 */
void
cogl_push_draw_buffer (void) G_GNUC_DEPRECATED;

/**
 * cogl_pop_draw_buffer:
 *
 * Restore cogl_set_draw_buffer() state.
 *
 * Deprecated: 1.2: The draw buffer API was replaced with a framebuffer API
 */
void
cogl_pop_draw_buffer (void) G_GNUC_DEPRECATED;

#endif /* COGL_DISABLE_DEPRECATED */

/**
 * cogl_flush:
 *
 * This function should only need to be called in exceptional circumstances.
 *
 * As an optimization Cogl drawing functions may batch up primitives
 * internally, so if you are trying to use raw GL outside of Cogl you stand a
 * better chance of being successful if you ask Cogl to flush any batched
 * geometry before making your state changes.
 *
 * It only ensure that the underlying driver is issued all the commands
 * necessary to draw the batched primitives. It provides no guarantees about
 * when the driver will complete the rendering.
 *
 * This provides no guarantees about the GL state upon returning and to avoid
 * confusing Cogl you should aim to restore any changes you make before
 * resuming use of Cogl.
 *
 * If you are making state changes with the intention of affecting Cogl drawing
 * primitives you are 100% on your own since you stand a good chance of
 * conflicting with Cogl internals. For example clutter-gst which currently
 * uses direct GL calls to bind ARBfp programs will very likely break when Cogl
 * starts to use ARBfb programs itself for the material API.
 *
 * Since: 1.0
 */
void
cogl_flush (void);

G_END_DECLS

#endif /* __COGL_1_CONTEXT_H__ */
