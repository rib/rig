/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010,2011,2012 Intel Corporation.
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
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-clip-stack-gl-private.h"
#include "cg-primitive-private.h"

static void
add_stencil_clip_rectangle(cg_framebuffer_t *framebuffer,
                           cg_matrix_entry_t *modelview_entry,
                           float x_1,
                           float y_1,
                           float x_2,
                           float y_2,
                           bool first)
{
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);
    cg_device_t *dev = cg_framebuffer_get_context(framebuffer);

    /* NB: This can be called while flushing for rendering so we need
     * to be very conservative with what state we change.
     */

    _cg_device_set_current_projection_entry(dev, projection_stack->last_entry);
    _cg_device_set_current_modelview_entry(dev, modelview_entry);

    if (first) {
        GE(dev, glEnable(GL_STENCIL_TEST));

        /* Initially disallow everything */
        GE(dev, glClearStencil(0));
        GE(dev, glClear(GL_STENCIL_BUFFER_BIT));

        /* Punch out a hole to allow the rectangle */
        GE(dev, glStencilFunc(GL_NEVER, 0x1, 0x1));
        GE(dev, glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE));

        _cg_rectangle_immediate(
            framebuffer, dev->stencil_pipeline, x_1, y_1, x_2, y_2);
    } else {
        /* Add one to every pixel of the stencil buffer in the
           rectangle */
        GE(dev, glStencilFunc(GL_NEVER, 0x1, 0x3));
        GE(dev, glStencilOp(GL_INCR, GL_INCR, GL_INCR));
        _cg_rectangle_immediate(
            framebuffer, dev->stencil_pipeline, x_1, y_1, x_2, y_2);

        /* Subtract one from all pixels in the stencil buffer so that
           only pixels where both the original stencil buffer and the
           rectangle are set will be valid */
        GE(dev, glStencilOp(GL_DECR, GL_DECR, GL_DECR));

        _cg_device_set_current_projection_entry(dev, &dev->identity_entry);
        _cg_device_set_current_modelview_entry(dev, &dev->identity_entry);

        _cg_rectangle_immediate(
            framebuffer, dev->stencil_pipeline, -1.0, -1.0, 1.0, 1.0);
    }

    /* Restore the stencil mode */
    GE(dev, glStencilFunc(GL_EQUAL, 0x1, 0x1));
    GE(dev, glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
}

typedef void (*silhouette_paint_callback_t)(cg_framebuffer_t *framebuffer,
                                            cg_pipeline_t *pipeline,
                                            void *user_data);

static void
add_stencil_clip_silhouette(cg_framebuffer_t *framebuffer,
                            silhouette_paint_callback_t silhouette_callback,
                            cg_matrix_entry_t *modelview_entry,
                            float bounds_x1,
                            float bounds_y1,
                            float bounds_x2,
                            float bounds_y2,
                            bool merge,
                            bool need_clear,
                            void *user_data)
{
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);
    cg_device_t *dev = cg_framebuffer_get_context(framebuffer);

    /* NB: This can be called while flushing for rendering so we need
     * to be very conservative with what state we change.
     */

    _cg_device_set_current_projection_entry(dev, projection_stack->last_entry);
    _cg_device_set_current_modelview_entry(dev, modelview_entry);

    _cg_pipeline_flush_gl_state(dev, dev->stencil_pipeline, framebuffer,
                                false, false);

    GE(dev, glEnable(GL_STENCIL_TEST));

    GE(dev, glColorMask(false, false, false, false));
    GE(dev, glDepthMask(false));

    if (merge) {
        GE(dev, glStencilMask(2));
        GE(dev, glStencilFunc(GL_LEQUAL, 0x2, 0x6));
    } else {
        /* If we're not using the stencil buffer for clipping then we
           don't need to clear the whole stencil buffer, just the area
           that will be drawn */
        if (need_clear)
            /* If this is being called from the clip stack code then it
               will have set up a scissor for the minimum bounding box of
               all of the clips. That box will likely mean that this
               _cg_clear won't need to clear the entire
               buffer. _cg_framebuffer_clear_without_flush4f to avoid
               recursion if we are currently flushing for rendering */
            _cg_framebuffer_clear_without_flush4f(
                framebuffer, CG_BUFFER_BIT_STENCIL, 0, 0, 0, 0);
        else {
            /* Just clear the bounding box */
            GE(dev, glStencilMask(~(GLuint)0));
            GE(dev, glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO));
            _cg_rectangle_immediate(framebuffer,
                                    dev->stencil_pipeline,
                                    bounds_x1,
                                    bounds_y1,
                                    bounds_x2,
                                    bounds_y2);
        }
        GE(dev, glStencilMask(1));
        GE(dev, glStencilFunc(GL_LEQUAL, 0x1, 0x3));
    }

    GE(dev, glStencilOp(GL_INVERT, GL_INVERT, GL_INVERT));

    silhouette_callback(framebuffer, dev->stencil_pipeline, user_data);

    if (merge) {
        /* Now we have the new stencil buffer in bit 1 and the old
           stencil buffer in bit 0 so we need to intersect them */
        GE(dev, glStencilMask(3));
        GE(dev, glStencilFunc(GL_NEVER, 0x2, 0x3));
        GE(dev, glStencilOp(GL_DECR, GL_DECR, GL_DECR));
        /* Decrement all of the bits twice so that only pixels where the
           value is 3 will remain */

        _cg_device_set_current_projection_entry(dev, &dev->identity_entry);
        _cg_device_set_current_modelview_entry(dev, &dev->identity_entry);

        _cg_rectangle_immediate(
            framebuffer, dev->stencil_pipeline, -1.0, -1.0, 1.0, 1.0);
        _cg_rectangle_immediate(
            framebuffer, dev->stencil_pipeline, -1.0, -1.0, 1.0, 1.0);
    }

    GE(dev, glStencilMask(~(GLuint)0));
    GE(dev, glDepthMask(true));
    GE(dev, glColorMask(true, true, true, true));

    GE(dev, glStencilFunc(GL_EQUAL, 0x1, 0x1));
    GE(dev, glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
}

static void
paint_primitive_silhouette(cg_framebuffer_t *framebuffer,
                           cg_pipeline_t *pipeline,
                           void *user_data)
{
    _cg_primitive_draw(user_data,
                       framebuffer,
                       pipeline,
                       1, /* n instances */
                       CG_DRAW_SKIP_FRAMEBUFFER_FLUSH);
}

static void
add_stencil_clip_primitive(cg_framebuffer_t *framebuffer,
                           cg_matrix_entry_t *modelview_entry,
                           cg_primitive_t *primitive,
                           float bounds_x1,
                           float bounds_y1,
                           float bounds_x2,
                           float bounds_y2,
                           bool merge,
                           bool need_clear)
{
    add_stencil_clip_silhouette(framebuffer,
                                paint_primitive_silhouette,
                                modelview_entry,
                                bounds_x1,
                                bounds_y1,
                                bounds_x2,
                                bounds_y2,
                                merge,
                                need_clear,
                                primitive);
}

void
_cg_clip_stack_gl_flush(cg_clip_stack_t *stack,
                        cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;
    bool using_stencil_buffer = false;
    int scissor_x0;
    int scissor_y0;
    int scissor_x1;
    int scissor_y1;
    cg_clip_stack_t *entry;
    int scissor_y_start;

    /* If we have already flushed this state then we don't need to do
       anything */
    if (dev->current_clip_stack_valid) {
        if (dev->current_clip_stack == stack &&
            (dev->needs_viewport_scissor_workaround == false ||
             (framebuffer->viewport_age ==
              framebuffer->viewport_age_for_scissor_workaround &&
              dev->viewport_scissor_workaround_framebuffer == framebuffer)))
            return;

        _cg_clip_stack_unref(dev->current_clip_stack);
    }

    dev->current_clip_stack_valid = true;
    dev->current_clip_stack = _cg_clip_stack_ref(stack);

    GE(dev, glDisable(GL_STENCIL_TEST));

    /* If the stack is empty then there's nothing else to do
     *
     * See comment below about dev->needs_viewport_scissor_workaround
     */
    if (stack == NULL && !dev->needs_viewport_scissor_workaround) {
        CG_NOTE(CLIPPING, "Flushed empty clip stack");

        GE(dev, glDisable(GL_SCISSOR_TEST));
        return;
    }

    /* Calculate the scissor rect first so that if we eventually have to
       clear the stencil buffer then the clear will be clipped to the
       intersection of all of the bounding boxes. This saves having to
       clear the whole stencil buffer */
    _cg_clip_stack_get_bounds(
        stack, &scissor_x0, &scissor_y0, &scissor_x1, &scissor_y1);

    /* XXX: ONGOING BUG: Intel viewport scissor
     *
     * Intel gen6 drivers don't correctly handle offset viewports, since
     * primitives aren't clipped within the bounds of the viewport.  To
     * workaround this we push our own clip for the viewport that will
     * use scissoring to ensure we clip as expected.
     *
     * TODO: file a bug upstream!
     */
    if (dev->needs_viewport_scissor_workaround) {
        _cg_util_scissor_intersect(
            framebuffer->viewport_x,
            framebuffer->viewport_y,
            framebuffer->viewport_x + framebuffer->viewport_width,
            framebuffer->viewport_y + framebuffer->viewport_height,
            &scissor_x0,
            &scissor_y0,
            &scissor_x1,
            &scissor_y1);
        framebuffer->viewport_age_for_scissor_workaround =
            framebuffer->viewport_age;
        dev->viewport_scissor_workaround_framebuffer = framebuffer;
    }

    /* Enable scissoring as soon as possible */
    if (scissor_x0 >= scissor_x1 || scissor_y0 >= scissor_y1)
        scissor_x0 = scissor_y0 = scissor_x1 = scissor_y1 = scissor_y_start = 0;
    else {
        /* We store the entry coordinates in CGlib coordinate space
         * but OpenGL requires the window origin to be the bottom
         * left so we may need to convert the incoming coordinates.
         *
         * NB: CGlib forces all offscreen rendering to be done upside
         * down so in this case no conversion is needed.
         */

        if (cg_is_offscreen(framebuffer))
            scissor_y_start = scissor_y0;
        else {
            int framebuffer_height = cg_framebuffer_get_height(framebuffer);

            scissor_y_start = framebuffer_height - scissor_y1;
        }
    }

    CG_NOTE(CLIPPING,
            "Flushing scissor to (%i, %i, %i, %i)",
            scissor_x0,
            scissor_y0,
            scissor_x1,
            scissor_y1);

    GE(dev, glEnable(GL_SCISSOR_TEST));
    GE(dev,
       glScissor(scissor_x0,
                 scissor_y_start,
                 scissor_x1 - scissor_x0,
                 scissor_y1 - scissor_y0));

    /* Add all of the entries. This will end up adding them in the
       reverse order that they were specified but as all of the clips
       are intersecting it should work out the same regardless of the
       order */
    for (entry = stack; entry; entry = entry->parent) {
        switch (entry->type) {
        case CG_CLIP_STACK_PRIMITIVE: {
            cg_clip_stack_primitive_t *primitive_entry =
                (cg_clip_stack_primitive_t *)entry;

            CG_NOTE(CLIPPING, "Adding stencil clip for primitive");

            add_stencil_clip_primitive(framebuffer,
                                       primitive_entry->matrix_entry,
                                       primitive_entry->primitive,
                                       primitive_entry->bounds_x1,
                                       primitive_entry->bounds_y1,
                                       primitive_entry->bounds_x2,
                                       primitive_entry->bounds_y2,
                                       using_stencil_buffer,
                                       true);

            using_stencil_buffer = true;
            break;
        }
        case CG_CLIP_STACK_RECT: {
            cg_clip_stack_rect_t *rect = (cg_clip_stack_rect_t *)entry;

            /* We don't need to do anything extra if the clip for this
               rectangle was entirely described by its scissor bounds */
            if (!rect->can_be_scissor) {
                CG_NOTE(CLIPPING, "Adding stencil clip for rectangle");

                add_stencil_clip_rectangle(framebuffer,
                                           rect->matrix_entry,
                                           rect->x0,
                                           rect->y0,
                                           rect->x1,
                                           rect->y1,
                                           !using_stencil_buffer);
                using_stencil_buffer = true;
            }
            break;
        }
        case CG_CLIP_STACK_WINDOW_RECT:
            break;
            /* We don't need to do anything for window space rectangles because
             * their functionality is entirely implemented by the entry bounding
             * box */
        }
    }
}
