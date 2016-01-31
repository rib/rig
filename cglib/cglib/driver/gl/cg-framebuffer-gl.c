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

#include <cglib-config.h>

#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-framebuffer-private.h"
#include "cg-framebuffer-gl-private.h"
#include "cg-buffer-gl-private.h"
#include "cg-error-private.h"
#include "cg-texture-gl-private.h"
#include "cg-texture-private.h"

#include <clib.h>
#include <string.h>

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER 0x8D41
#endif
#ifndef GL_STENCIL_ATTACHMENT
#define GL_STENCIL_ATTACHMENT 0x8D00
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_STENCIL_INDEX8
#define GL_STENCIL_INDEX8 0x8D48
#endif
#ifndef GL_DEPTH_STENCIL
#define GL_DEPTH_STENCIL 0x84F9
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16 0x81A5
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE 0x8212
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE 0x8213
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE 0x8214
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE 0x8215
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE 0x8216
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE 0x8217
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif
#ifndef GL_TEXTURE_SAMPLES_IMG
#define GL_TEXTURE_SAMPLES_IMG 0x9136
#endif
#ifndef GL_PACK_INVERT_MESA
#define GL_PACK_INVERT_MESA 0x8758
#endif

#ifndef GL_COLOR
#define GL_COLOR 0x1800
#endif
#ifndef GL_DEPTH
#define GL_DEPTH 0x1801
#endif
#ifndef GL_STENCIL
#define GL_STENCIL 0x1802
#endif

static void
_cg_framebuffer_gl_flush_viewport_state(cg_framebuffer_t *framebuffer)
{
    float gl_viewport_y;

    c_assert(framebuffer->viewport_width >= 0 &&
             framebuffer->viewport_height >= 0);

    /* Convert the CGlib viewport y offset to an OpenGL viewport y offset
     * NB: OpenGL defines its window and viewport origins to be bottom
     * left, while CGlib defines them to be top left.
     * NB: We render upside down to offscreen framebuffers so we don't
     * need to convert the y offset in this case. */
    if (cg_is_offscreen(framebuffer))
        gl_viewport_y = framebuffer->viewport_y;
    else
        gl_viewport_y = framebuffer->height - (framebuffer->viewport_y +
                                               framebuffer->viewport_height);

    CG_NOTE(OPENGL,
            "Calling glViewport(%f, %f, %f, %f)",
            framebuffer->viewport_x,
            gl_viewport_y,
            framebuffer->viewport_width,
            framebuffer->viewport_height);

    GE(framebuffer->dev,
       glViewport(framebuffer->viewport_x,
                  gl_viewport_y,
                  framebuffer->viewport_width,
                  framebuffer->viewport_height));
}

static void
_cg_framebuffer_gl_flush_clip_state(cg_framebuffer_t *framebuffer)
{
    _cg_clip_stack_flush(framebuffer->clip_stack, framebuffer);
}

static void
_cg_framebuffer_gl_flush_dither_state(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;

    if (dev->current_gl_dither_enabled != framebuffer->dither_enabled) {
        if (framebuffer->dither_enabled)
            GE(dev, glEnable(GL_DITHER));
        else
            GE(dev, glDisable(GL_DITHER));
        dev->current_gl_dither_enabled = framebuffer->dither_enabled;
    }
}

static void
_cg_framebuffer_gl_flush_modelview_state(cg_framebuffer_t *framebuffer)
{
    cg_matrix_entry_t *modelview_entry =
        _cg_framebuffer_get_modelview_entry(framebuffer);
    _cg_device_set_current_modelview_entry(framebuffer->dev, modelview_entry);
}

static void
_cg_framebuffer_gl_flush_projection_state(cg_framebuffer_t *framebuffer)
{
    cg_matrix_entry_t *projection_entry =
        _cg_framebuffer_get_projection_entry(framebuffer);
    _cg_device_set_current_projection_entry(framebuffer->dev, projection_entry);
}

static void
_cg_framebuffer_gl_flush_color_mask_state(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;

    /* The color mask state is really owned by a cg_pipeline_t so to
     * ensure the color mask is updated the next time we draw something
     * we need to make sure the logic ops for the pipeline are
     * re-flushed... */
    dev->current_pipeline_changes_since_flush |=
        CG_PIPELINE_STATE_LOGIC_OPS;
    dev->current_pipeline_age--;
}

static void
_cg_framebuffer_gl_flush_front_face_winding_state(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;
    cg_pipeline_cull_face_mode_t mode;

    /* NB: The face winding state is actually owned by the current
     * cg_pipeline_t.
     *
     * If we don't have a current pipeline then we can just assume that
     * when we later do flush a pipeline we will check the current
     * framebuffer to know how to setup the winding */
    if (!dev->current_pipeline)
        return;

    mode = cg_pipeline_get_cull_face_mode(dev->current_pipeline);

    /* If the current cg_pipeline_t has a culling mode that doesn't care
     * about the winding we can avoid forcing an update of the state and
     * bail out. */
    if (mode == CG_PIPELINE_CULL_FACE_MODE_NONE ||
        mode == CG_PIPELINE_CULL_FACE_MODE_BOTH)
        return;

    /* Since the winding state is really owned by the current pipeline
     * the way we "flush" an updated winding is to dirty the pipeline
     * state... */
    dev->current_pipeline_changes_since_flush |=
        CG_PIPELINE_STATE_CULL_FACE;
    dev->current_pipeline_age--;
}

void
_cg_framebuffer_gl_bind(cg_framebuffer_t *framebuffer, GLenum target)
{
    cg_device_t *dev = framebuffer->dev;

    if (framebuffer->type == CG_FRAMEBUFFER_TYPE_OFFSCREEN) {
        cg_offscreen_t *offscreen = CG_OFFSCREEN(framebuffer);
        GE(dev,
           glBindFramebuffer(target, offscreen->gl_framebuffer.fbo_handle));
    } else {
        const cg_winsys_vtable_t *winsys =
            _cg_framebuffer_get_winsys(framebuffer);

        winsys->onscreen_bind(CG_ONSCREEN(framebuffer));

        GE(dev, glBindFramebuffer(target, 0));

        /* Initialise the glDrawBuffer state the first time the context
         * is bound to the default framebuffer. If the winsys is using a
         * surfaceless context for the initial make current then the
         * default draw buffer will be GL_NONE so we need to correct
         * that. We can't do it any earlier because binding GL_BACK when
         * there is no default framebuffer won't work */
        if (!dev->was_bound_to_onscreen) {
            if (dev->glDrawBuffer) {
                GE(dev, glDrawBuffer(GL_BACK));
            } else if (dev->glDrawBuffers) {
                /* glDrawBuffer isn't available on GLES 3.0 so we need
                 * to be able to use glDrawBuffers as well. On GLES 2
                 * neither is available but the state should always be
                 * GL_BACK anyway so we don't need to set anything. On
                 * desktop GL this must be GL_BACK_LEFT instead of
                 * GL_BACK but as this code path will only be hit for
                 * GLES we can just use GL_BACK. */
                static const GLenum buffers[] = { GL_BACK };

                GE(dev, glDrawBuffers(C_N_ELEMENTS(buffers), buffers));
            }

            dev->was_bound_to_onscreen = true;
        }
    }
}

void
_cg_framebuffer_gl_flush_state(cg_framebuffer_t *draw_buffer,
                               cg_framebuffer_t *read_buffer,
                               cg_framebuffer_state_t state)
{
    cg_device_t *dev = draw_buffer->dev;
    unsigned long differences;
    int bit;

    /* We can assume that any state that has changed for the current
     * framebuffer is different to the currently flushed value. */
    differences = dev->current_draw_buffer_changes;

    /* Any state of the current framebuffer that hasn't already been
     * flushed is assumed to be unknown so we will always flush that
     * state if asked. */
    differences |= ~dev->current_draw_buffer_state_flushed;

    /* We only need to consider the state we've been asked to flush */
    differences &= state;

#warning "HACK - force flush all framebuffer state"
    differences = CG_FRAMEBUFFER_STATE_ALL;
    dev->current_draw_buffer = NULL;

    if (dev->current_draw_buffer != draw_buffer) {
        /* If the previous draw buffer is NULL then we'll assume
           everything has changed. This can happen if a framebuffer is
           destroyed while it is the last flushed draw buffer. In that
           case the framebuffer destructor will set
           dev->current_draw_buffer to NULL */
        if (dev->current_draw_buffer == NULL)
            differences |= state;
        else
            /* NB: we only need to compare the state we're being asked to flush
             * and we don't need to compare the state we've already decided
             * we will definitely flush... */
            differences |= _cg_framebuffer_compare(dev->current_draw_buffer,
                                                   draw_buffer,
                                                   state & ~differences);

        /* NB: we don't take a reference here, to avoid a circular
         * reference. */
        dev->current_draw_buffer = draw_buffer;
        dev->current_draw_buffer_state_flushed = 0;
    }

    if (dev->current_read_buffer != read_buffer &&
        state & CG_FRAMEBUFFER_STATE_BIND) {
        differences |= CG_FRAMEBUFFER_STATE_BIND;
        /* NB: we don't take a reference here, to avoid a circular
         * reference. */
        dev->current_read_buffer = read_buffer;
    }

    if (!differences)
        return;

    /* Lazily ensure the framebuffers have been allocated */
    if (C_UNLIKELY(!draw_buffer->allocated))
        cg_framebuffer_allocate(draw_buffer, NULL);
    if (C_UNLIKELY(!read_buffer->allocated))
        cg_framebuffer_allocate(read_buffer, NULL);

    /* We handle buffer binding separately since the method depends on whether
     * we are binding the same buffer for read and write or not unlike all
     * other state that only relates to the draw_buffer. */
    if (differences & CG_FRAMEBUFFER_STATE_BIND) {
        if (draw_buffer == read_buffer)
            _cg_framebuffer_gl_bind(draw_buffer, GL_FRAMEBUFFER);
        else {
            /* NB: Currently we only take advantage of binding separate
             * read/write buffers for offscreen framebuffer blit
             * purposes.  */
            c_return_if_fail(_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_OFFSCREEN_BLIT));
            c_return_if_fail(draw_buffer->type ==
                               CG_FRAMEBUFFER_TYPE_OFFSCREEN);
            c_return_if_fail(read_buffer->type ==
                               CG_FRAMEBUFFER_TYPE_OFFSCREEN);

            _cg_framebuffer_gl_bind(draw_buffer, GL_DRAW_FRAMEBUFFER);
            _cg_framebuffer_gl_bind(read_buffer, GL_READ_FRAMEBUFFER);
        }

        differences &= ~CG_FRAMEBUFFER_STATE_BIND;
    }

    CG_FLAGS_FOREACH_START(&differences, 1, bit)
    {
        /* XXX: We considered having an array of callbacks for each state index
         * that we'd call here but decided that this way the compiler is more
         * likely going to be able to in-line the flush functions and use the
         * index to jump straight to the required code. */
        switch (bit) {
        case CG_FRAMEBUFFER_STATE_INDEX_VIEWPORT:
            _cg_framebuffer_gl_flush_viewport_state(draw_buffer);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_CLIP:
            _cg_framebuffer_gl_flush_clip_state(draw_buffer);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_DITHER:
            _cg_framebuffer_gl_flush_dither_state(draw_buffer);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_MODELVIEW:
            _cg_framebuffer_gl_flush_modelview_state(draw_buffer);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_PROJECTION:
            _cg_framebuffer_gl_flush_projection_state(draw_buffer);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_COLOR_MASK:
            _cg_framebuffer_gl_flush_color_mask_state(draw_buffer);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_FRONT_FACE_WINDING:
            _cg_framebuffer_gl_flush_front_face_winding_state(draw_buffer);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_DEPTH_WRITE:
            /* Nothing to do for depth write state change; the state will always
             * be taken into account when flushing the pipeline's depth state.
             */
            break;
        default:
            c_warn_if_reached();
        }
    }
    CG_FLAGS_FOREACH_END;

    dev->current_draw_buffer_state_flushed |= state;
    dev->current_draw_buffer_changes &= ~state;
}

static cg_texture_t *
attach_depth_texture(cg_device_t *dev,
                     cg_texture_t *depth_texture,
                     int depth_texture_level,
                     cg_offscreen_allocate_flags_t flags)
{
    GLuint tex_gl_handle;
    GLenum tex_gl_target;

    if (flags & CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL) {
        /* attach a GL_DEPTH_STENCIL texture to the GL_DEPTH_ATTACHMENT and
         * GL_STENCIL_ATTACHMENT attachement points */
        c_assert(_cg_texture_get_format(depth_texture) ==
                 CG_PIXEL_FORMAT_DEPTH_24_STENCIL_8);

        cg_texture_get_gl_texture(depth_texture, &tex_gl_handle, &tex_gl_target);

#ifdef HAVE_CG_WEBGL
        GE(dev,
           glFramebufferTexture2D(GL_FRAMEBUFFER,
                                  GL_DEPTH_STENCIL_ATTACHMENT,
                                  tex_gl_target,
                                  tex_gl_handle,
                                  depth_texture_level));
#else
        GE(dev,
           glFramebufferTexture2D(GL_FRAMEBUFFER,
                                  GL_DEPTH_ATTACHMENT,
                                  tex_gl_target,
                                  tex_gl_handle,
                                  depth_texture_level));
        GE(dev,
           glFramebufferTexture2D(GL_FRAMEBUFFER,
                                  GL_STENCIL_ATTACHMENT,
                                  tex_gl_target,
                                  tex_gl_handle,
                                  depth_texture_level));
#endif
    } else if (flags & CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH) {
        /* attach a newly created GL_DEPTH_COMPONENT16 texture to the
         * GL_DEPTH_ATTACHMENT attachement point */
        c_assert(_cg_texture_get_format(depth_texture) ==
                 CG_PIXEL_FORMAT_DEPTH_16);

        cg_texture_get_gl_texture(depth_texture, &tex_gl_handle, &tex_gl_target);

        GE(dev,
           glFramebufferTexture2D(GL_FRAMEBUFFER,
                                  GL_DEPTH_ATTACHMENT,
                                  tex_gl_target,
                                  tex_gl_handle,
                                  depth_texture_level));
    }

    return CG_TEXTURE(depth_texture);
}

static c_llist_t *
try_creating_renderbuffers(cg_device_t *dev,
                           int width,
                           int height,
                           cg_offscreen_allocate_flags_t flags,
                           int n_samples)
{
    c_llist_t *renderbuffers = NULL;
    GLuint gl_depth_stencil_handle;

    if (flags & CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL) {
        GLenum format;

/* WebGL adds a GL_DEPTH_STENCIL_ATTACHMENT and requires that we
 * use the GL_DEPTH_STENCIL format. */
#ifdef HAVE_CG_WEBGL
        format = GL_DEPTH_STENCIL;
#else
        /* Although GL_OES_packed_depth_stencil is mostly equivalent to
         * GL_EXT_packed_depth_stencil, one notable difference is that
         * GL_OES_packed_depth_stencil doesn't allow GL_DEPTH_STENCIL to
         * be passed as an internal format to glRenderbufferStorage.
         */
        if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL))
            format = GL_DEPTH_STENCIL;
        else {
            c_return_val_if_fail(
                _cg_has_private_feature(dev, CG_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL),
                NULL);
            format = GL_DEPTH24_STENCIL8;
        }
#endif

        /* Create a renderbuffer for depth and stenciling */
        GE(dev, glGenRenderbuffers(1, &gl_depth_stencil_handle));
        GE(dev, glBindRenderbuffer(GL_RENDERBUFFER, gl_depth_stencil_handle));
        if (n_samples)
            GE(dev,
               glRenderbufferStorageMultisampleIMG(
                   GL_RENDERBUFFER, n_samples, format, width, height));
        else
            GE(dev,
               glRenderbufferStorage(GL_RENDERBUFFER, format, width, height));
        GE(dev, glBindRenderbuffer(GL_RENDERBUFFER, 0));

#ifdef HAVE_CG_WEBGL
        GE(dev,
           glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                     GL_DEPTH_STENCIL_ATTACHMENT,
                                     GL_RENDERBUFFER,
                                     gl_depth_stencil_handle));
#else
        GE(dev,
           glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                     GL_STENCIL_ATTACHMENT,
                                     GL_RENDERBUFFER,
                                     gl_depth_stencil_handle));
        GE(dev,
           glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                     GL_DEPTH_ATTACHMENT,
                                     GL_RENDERBUFFER,
                                     gl_depth_stencil_handle));
#endif
        renderbuffers = c_llist_prepend(
            renderbuffers, C_UINT_TO_POINTER(gl_depth_stencil_handle));
    }

    if (flags & CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH) {
        GLuint gl_depth_handle;

        GE(dev, glGenRenderbuffers(1, &gl_depth_handle));
        GE(dev, glBindRenderbuffer(GL_RENDERBUFFER, gl_depth_handle));
        /* For now we just ask for GL_DEPTH_COMPONENT16 since this is all that's
         * available under GLES */
        if (n_samples)
            GE(dev,
               glRenderbufferStorageMultisampleIMG(GL_RENDERBUFFER,
                                                   n_samples,
                                                   GL_DEPTH_COMPONENT16,
                                                   width,
                                                   height));
        else
            GE(dev,
               glRenderbufferStorage(
                   GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height));
        GE(dev, glBindRenderbuffer(GL_RENDERBUFFER, 0));
        GE(dev,
           glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                     GL_DEPTH_ATTACHMENT,
                                     GL_RENDERBUFFER,
                                     gl_depth_handle));
        renderbuffers =
            c_llist_prepend(renderbuffers, C_UINT_TO_POINTER(gl_depth_handle));
    }

    if (flags & CG_OFFSCREEN_ALLOCATE_FLAG_STENCIL) {
        GLuint gl_stencil_handle;

        GE(dev, glGenRenderbuffers(1, &gl_stencil_handle));
        GE(dev, glBindRenderbuffer(GL_RENDERBUFFER, gl_stencil_handle));
        if (n_samples)
            GE(dev,
               glRenderbufferStorageMultisampleIMG(GL_RENDERBUFFER,
                                                   n_samples,
                                                   GL_STENCIL_INDEX8,
                                                   width,
                                                   height));
        else
            GE(dev,
               glRenderbufferStorage(
                   GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height));
        GE(dev, glBindRenderbuffer(GL_RENDERBUFFER, 0));
        GE(dev,
           glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                     GL_STENCIL_ATTACHMENT,
                                     GL_RENDERBUFFER,
                                     gl_stencil_handle));
        renderbuffers =
            c_llist_prepend(renderbuffers, C_UINT_TO_POINTER(gl_stencil_handle));
    }

    return renderbuffers;
}

static void
delete_renderbuffers(cg_device_t *dev, c_llist_t *renderbuffers)
{
    c_llist_t *l;

    for (l = renderbuffers; l; l = l->next) {
        GLuint renderbuffer = C_POINTER_TO_UINT(l->data);
        GE(dev, glDeleteRenderbuffers(1, &renderbuffer));
    }

    c_llist_free(renderbuffers);
}

/*
 * NB: This function may be called with a standalone GLES2 context
 * bound so we can create a shadow framebuffer that wraps the same
 * cg_texture_t as the given cg_offscreen_t. This function shouldn't
 * modify anything in
 */
static bool
try_creating_fbo(cg_device_t *dev,
                 int width,
                 int height,
                 cg_texture_t *texture,
                 int texture_level,
                 cg_texture_t *depth_texture,
                 int depth_texture_level,
                 cg_framebuffer_config_t *config,
                 cg_offscreen_allocate_flags_t flags,
                 cg_gl_framebuffer_t *gl_framebuffer)
{
    GLuint tex_gl_handle;
    GLenum tex_gl_target;
    GLenum status;
    int n_samples = 0;

    /* We are about to generate and bind a new fbo, so we pretend to
     * change framebuffer state so that the old framebuffer will be
     * rebound again before drawing. */
    dev->current_draw_buffer_changes |= CG_FRAMEBUFFER_STATE_BIND;

    /* Generate framebuffer */
    dev->glGenFramebuffers(1, &gl_framebuffer->fbo_handle);
    GE(dev, glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer->fbo_handle));

    gl_framebuffer->renderbuffers = NULL;

    if (texture) {
        if (!cg_texture_get_gl_texture(texture, &tex_gl_handle, &tex_gl_target))
            goto error;

        if (tex_gl_target != GL_TEXTURE_2D)
            goto error;

        if (config->samples_per_pixel) {
            if (!dev->glFramebufferTexture2DMultisampleIMG)
                goto error;
            n_samples = config->samples_per_pixel;
        }

        if (n_samples) {
            GE(dev,
               glFramebufferTexture2DMultisampleIMG(GL_FRAMEBUFFER,
                                                    GL_COLOR_ATTACHMENT0,
                                                    tex_gl_target,
                                                    tex_gl_handle,
                                                    n_samples,
                                                    texture_level));
        } else {
            GE(dev,
               glFramebufferTexture2D(GL_FRAMEBUFFER,
                                      GL_COLOR_ATTACHMENT0,
                                      tex_gl_target,
                                      tex_gl_handle,
                                      texture_level));
        }
    }

    /* attach either a depth/stencil texture, a depth texture or render buffers
     * depending on what we've been asked to provide */

    if (depth_texture && flags & (CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL |
                                  CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH)) {
        attach_depth_texture(dev,
                             depth_texture,
                             depth_texture_level,
                             flags);

        /* Let's clear the flags that are now fulfilled as we might need to
         * create renderbuffers (for the ALLOCATE_FLAG_DEPTH |
         * ALLOCATE_FLAG_STENCIL case) */
        flags &= ~(CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL |
                   CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH);
    }

    if (flags) {
        gl_framebuffer->renderbuffers = try_creating_renderbuffers(dev,
                                                                   width,
                                                                   height,
                                                                   flags,
                                                                   n_samples);
    }

    /* Make sure it's complete */
    status = dev->glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
        goto error;

    /* Update the real number of samples_per_pixel now that we have a
     * complete framebuffer */
    if (n_samples) {
        GLenum attachment = GL_COLOR_ATTACHMENT0;
        GLenum pname = GL_TEXTURE_SAMPLES_IMG;
        int texture_samples;

        GE(dev,
           glGetFramebufferAttachmentParameteriv(
               GL_FRAMEBUFFER, attachment, pname, &texture_samples));
        gl_framebuffer->samples_per_pixel = texture_samples;
    }

    return true;

error:
    if (gl_framebuffer->renderbuffers) {
        delete_renderbuffers(dev, gl_framebuffer->renderbuffers);
        gl_framebuffer->renderbuffers = NULL;
    }

    GE(dev, glDeleteFramebuffers(1, &gl_framebuffer->fbo_handle));

    return false;
}

bool
_cg_framebuffer_try_creating_gl_fbo(cg_device_t *dev,
                                    int width,
                                    int height,
                                    cg_texture_t *texture,
                                    int texture_level,
                                    cg_texture_t *depth_texture,
                                    int depth_texture_level,
                                    cg_framebuffer_config_t *config,
                                    cg_offscreen_allocate_flags_t flags,
                                    cg_gl_framebuffer_t *gl_framebuffer)
{
    return try_creating_fbo(dev,
                            width,
                            height,
                            texture,
                            texture_level,
                            depth_texture,
                            depth_texture_level,
                            config,
                            flags,
                            gl_framebuffer);
}

bool
_cg_offscreen_gl_allocate(cg_offscreen_t *offscreen, cg_error_t **error)
{
    cg_framebuffer_t *fb = CG_FRAMEBUFFER(offscreen);
    cg_device_t *dev = fb->dev;
    cg_offscreen_allocate_flags_t flags;
    cg_gl_framebuffer_t *gl_framebuffer = &offscreen->gl_framebuffer;
    int width = fb->width;
    int height = fb->height;

    c_return_val_if_fail(width > 0, false);
    c_return_val_if_fail(height > 0, false);

    /* XXX: The framebuffer_object spec isn't clear in defining
     * whether attaching a texture as a renderbuffer with mipmap
     * filtering enabled while the mipmaps have not been uploaded
     * should result in an incomplete framebuffer object. (different
     * drivers make different decisions)
     *
     * To avoid an error with drivers that do consider this a problem
     * we explicitly set non mipmapped filters here. These will later
     * be reset when the texture is actually used for rendering
     * according to the filters set on the corresponding
     * cg_pipeline_t.
     */
    if (offscreen->texture)
        _cg_texture_gl_flush_legacy_texobj_filters(
            offscreen->texture, GL_NEAREST, GL_NEAREST);

    if (offscreen->depth_texture)
        _cg_texture_gl_flush_legacy_texobj_filters(
            offscreen->depth_texture, GL_NEAREST, GL_NEAREST);

    if (((offscreen->create_flags & CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL) &&
         try_creating_fbo(dev,
                          width,
                          height,
                          offscreen->texture,
                          offscreen->texture_level,
                          offscreen->depth_texture,
                          offscreen->depth_texture_level,
                          &fb->config,
                          flags = 0,
                          gl_framebuffer)) ||
        (dev->have_last_offscreen_allocate_flags &&
         try_creating_fbo(dev,
                          width,
                          height,
                          offscreen->texture,
                          offscreen->texture_level,
                          offscreen->depth_texture,
                          offscreen->depth_texture_level,
                          &fb->config,
                          flags = dev->last_offscreen_allocate_flags,
                          gl_framebuffer)) ||
        (
/* NB: WebGL introduces a DEPTH_STENCIL_ATTACHMENT and doesn't
 * need an extension to handle _FLAG_DEPTH_STENCIL */
#ifndef HAVE_CG_WEBGL
            (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL) ||
             _cg_has_private_feature(dev, CG_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL)) &&
#endif
            try_creating_fbo(dev,
                             width,
                             height,
                             offscreen->texture,
                             offscreen->texture_level,
                             offscreen->depth_texture,
                             offscreen->depth_texture_level,
                             &fb->config,
                             flags = CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL,
                             gl_framebuffer)) ||
        try_creating_fbo(dev,
                         width,
                         height,
                         offscreen->texture,
                         offscreen->texture_level,
                         offscreen->depth_texture,
                         offscreen->depth_texture_level,
                         &fb->config,
                         flags = CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH |
                                 CG_OFFSCREEN_ALLOCATE_FLAG_STENCIL,
                         gl_framebuffer) ||
        try_creating_fbo(dev,
                         width,
                         height,
                         offscreen->texture,
                         offscreen->texture_level,
                         offscreen->depth_texture,
                         offscreen->depth_texture_level,
                         &fb->config,
                         flags = CG_OFFSCREEN_ALLOCATE_FLAG_STENCIL,
                         gl_framebuffer) ||
        try_creating_fbo(dev,
                         width,
                         height,
                         offscreen->texture,
                         offscreen->texture_level,
                         offscreen->depth_texture,
                         offscreen->depth_texture_level,
                         &fb->config,
                         flags = CG_OFFSCREEN_ALLOCATE_FLAG_DEPTH,
                         gl_framebuffer) ||
        try_creating_fbo(dev,
                         width,
                         height,
                         offscreen->texture,
                         offscreen->texture_level,
                         offscreen->depth_texture,
                         offscreen->depth_texture_level,
                         &fb->config,
                         flags = 0,
                         gl_framebuffer)) {
        fb->samples_per_pixel = gl_framebuffer->samples_per_pixel;

        if (!offscreen->create_flags & CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL) {
            /* Record that the last set of flags succeeded so that we can
               try that set first next time */
            dev->last_offscreen_allocate_flags = flags;
            dev->have_last_offscreen_allocate_flags = true;
        }

        /* Save the flags we managed to successfully allocate the
         * renderbuffers with in case we need to make renderbuffers for a
         * GLES2 context later */
        offscreen->allocation_flags = flags;

        return true;
    } else {
        _cg_set_error(error,
                      CG_FRAMEBUFFER_ERROR,
                      CG_FRAMEBUFFER_ERROR_ALLOCATE,
                      "Failed to create an OpenGL framebuffer object");
        return false;
    }
}

void
_cg_offscreen_gl_free(cg_offscreen_t *offscreen)
{
    cg_device_t *dev = CG_FRAMEBUFFER(offscreen)->dev;

    delete_renderbuffers(dev, offscreen->gl_framebuffer.renderbuffers);

    GE(dev, glDeleteFramebuffers(1, &offscreen->gl_framebuffer.fbo_handle));
}

void
_cg_framebuffer_gl_clear(cg_framebuffer_t *framebuffer,
                         unsigned long buffers,
                         float red,
                         float green,
                         float blue,
                         float alpha)
{
    cg_device_t *dev = framebuffer->dev;
    GLbitfield gl_buffers = 0;

    if (buffers & CG_BUFFER_BIT_COLOR) {
        GE(dev, glClearColor(red, green, blue, alpha));
        gl_buffers |= GL_COLOR_BUFFER_BIT;

        if (dev->current_gl_color_mask != framebuffer->color_mask) {
            cg_color_mask_t color_mask = framebuffer->color_mask;
            GE(dev,
               glColorMask(!!(color_mask & CG_COLOR_MASK_RED),
                           !!(color_mask & CG_COLOR_MASK_GREEN),
                           !!(color_mask & CG_COLOR_MASK_BLUE),
                           !!(color_mask & CG_COLOR_MASK_ALPHA)));
            dev->current_gl_color_mask = color_mask;
            /* Make sure the ColorMask is updated when the next primitive is
             * drawn */
            dev->current_pipeline_changes_since_flush |=
                CG_PIPELINE_STATE_LOGIC_OPS;
            dev->current_pipeline_age--;
        }
    }

    if (buffers & CG_BUFFER_BIT_DEPTH) {
        gl_buffers |= GL_DEPTH_BUFFER_BIT;

        if (dev->depth_writing_enabled_cache !=
            framebuffer->depth_writing_enabled) {
            GE(dev, glDepthMask(framebuffer->depth_writing_enabled));

            dev->depth_writing_enabled_cache =
                framebuffer->depth_writing_enabled;

            /* Make sure the DepthMask is updated when the next primitive is
             * drawn */
            dev->current_pipeline_changes_since_flush |=
                CG_PIPELINE_STATE_DEPTH;
            dev->current_pipeline_age--;
        }
    }

    if (buffers & CG_BUFFER_BIT_STENCIL)
        gl_buffers |= GL_STENCIL_BUFFER_BIT;

    GE(dev, glClear(gl_buffers));
}

static inline void
_cg_framebuffer_init_bits(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;

    if (C_LIKELY(!framebuffer->dirty_bitmasks))
        return;

    cg_framebuffer_allocate(framebuffer, NULL);

    _cg_framebuffer_flush_state(
        framebuffer, framebuffer, CG_FRAMEBUFFER_STATE_BIND);

#ifdef CG_HAS_GL_SUPPORT
    if (_cg_has_private_feature(dev,
                                CG_PRIVATE_FEATURE_QUERY_FRAMEBUFFER_BITS) &&
        framebuffer->type == CG_FRAMEBUFFER_TYPE_OFFSCREEN) {
        static const struct {
            GLenum attachment, pname;
            size_t offset;
        } params[] = {
            { GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,
              offsetof(cg_framebuffer_bits_t, red) },
            { GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
              offsetof(cg_framebuffer_bits_t, green) },
            { GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,
              offsetof(cg_framebuffer_bits_t, blue) },
            { GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
              offsetof(cg_framebuffer_bits_t, alpha) },
            { GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
              offsetof(cg_framebuffer_bits_t, depth) },
            { GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
              offsetof(cg_framebuffer_bits_t, stencil) },
        };

        for (unsigned i = 0; i < C_N_ELEMENTS(params); i++) {
            int *value =
                (int *)((uint8_t *)&framebuffer->bits + params[i].offset);
            GE(dev,
               glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                                     params[i].attachment,
                                                     params[i].pname,
                                                     value));
        }
    } else
#endif /* CG_HAS_GL_SUPPORT */
    {
        GE(dev, glGetIntegerv(GL_RED_BITS, &framebuffer->bits.red));
        GE(dev, glGetIntegerv(GL_GREEN_BITS, &framebuffer->bits.green));
        GE(dev, glGetIntegerv(GL_BLUE_BITS, &framebuffer->bits.blue));
        GE(dev, glGetIntegerv(GL_ALPHA_BITS, &framebuffer->bits.alpha));
        GE(dev, glGetIntegerv(GL_DEPTH_BITS, &framebuffer->bits.depth));
        GE(dev, glGetIntegerv(GL_STENCIL_BITS, &framebuffer->bits.stencil));
    }

    /* If we don't have alpha textures then the alpha bits are actually
     * stored in the red component */
    if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_ALPHA_TEXTURES) &&
        framebuffer->type == CG_FRAMEBUFFER_TYPE_OFFSCREEN &&
        framebuffer->internal_format == CG_PIXEL_FORMAT_A_8) {
        framebuffer->bits.alpha = framebuffer->bits.red;
        framebuffer->bits.red = 0;
    }

    CG_NOTE(OFFSCREEN,
            "RGBA/D/S Bits for framebuffer[%p, %s]: %d, %d, %d, %d, %d, %d",
            framebuffer,
            framebuffer->type == CG_FRAMEBUFFER_TYPE_OFFSCREEN ? "offscreen"
            : "onscreen",
            framebuffer->bits.red,
            framebuffer->bits.blue,
            framebuffer->bits.green,
            framebuffer->bits.alpha,
            framebuffer->bits.depth,
            framebuffer->bits.stencil);

    framebuffer->dirty_bitmasks = false;
}

void
_cg_framebuffer_gl_query_bits(cg_framebuffer_t *framebuffer,
                              cg_framebuffer_bits_t *bits)
{
    _cg_framebuffer_init_bits(framebuffer);

    /* TODO: cache these in some driver specific location not
     * directly as part of cg_framebuffer_t. */
    *bits = framebuffer->bits;
}

void
_cg_framebuffer_gl_finish(cg_framebuffer_t *framebuffer)
{
    GE(framebuffer->dev, glFinish());
}

void
_cg_framebuffer_gl_discard_buffers(cg_framebuffer_t *framebuffer,
                                   unsigned long buffers)
{
    cg_device_t *dev = framebuffer->dev;

    if (dev->glDiscardFramebuffer) {
        GLenum attachments[3];
        int i = 0;

        if (framebuffer->type == CG_FRAMEBUFFER_TYPE_ONSCREEN) {
            if (buffers & CG_BUFFER_BIT_COLOR)
                attachments[i++] = GL_COLOR;
            if (buffers & CG_BUFFER_BIT_DEPTH)
                attachments[i++] = GL_DEPTH;
            if (buffers & CG_BUFFER_BIT_STENCIL)
                attachments[i++] = GL_STENCIL;
        } else {
            if (buffers & CG_BUFFER_BIT_COLOR)
                attachments[i++] = GL_COLOR_ATTACHMENT0;
            if (buffers & CG_BUFFER_BIT_DEPTH)
                attachments[i++] = GL_DEPTH_ATTACHMENT;
            if (buffers & CG_BUFFER_BIT_STENCIL)
                attachments[i++] = GL_STENCIL_ATTACHMENT;
        }

        _cg_framebuffer_flush_state(
            framebuffer, framebuffer, CG_FRAMEBUFFER_STATE_BIND);
        GE(dev, glDiscardFramebuffer(GL_FRAMEBUFFER, i, attachments));
    }
}

void
_cg_framebuffer_gl_draw_attributes(cg_framebuffer_t *framebuffer,
                                   cg_pipeline_t *pipeline,
                                   cg_vertices_mode_t mode,
                                   int first_vertex,
                                   int n_vertices,
                                   cg_attribute_t **attributes,
                                   int n_attributes,
                                   int n_instances,
                                   cg_draw_flags_t flags)
{
    _cg_flush_attributes_state(
        framebuffer, pipeline, flags, attributes, n_attributes);

    if (framebuffer->dev->glDrawArraysInstanced) {
        GE(framebuffer->dev,
           glDrawArraysInstanced((GLenum)mode, first_vertex,
                                 n_vertices, n_instances));
    } else {
        GE(framebuffer->dev,
           glDrawArrays((GLenum)mode, first_vertex, n_vertices));
    }
}

static size_t
sizeof_index_type(cg_indices_type_t type)
{
    switch (type) {
    case CG_INDICES_TYPE_UNSIGNED_BYTE:
        return 1;
    case CG_INDICES_TYPE_UNSIGNED_SHORT:
        return 2;
    case CG_INDICES_TYPE_UNSIGNED_INT:
        return 4;
    }
    c_return_val_if_reached(0);
}

void
_cg_framebuffer_gl_draw_indexed_attributes(cg_framebuffer_t *framebuffer,
                                           cg_pipeline_t *pipeline,
                                           cg_vertices_mode_t mode,
                                           int first_vertex,
                                           int n_vertices,
                                           cg_indices_t *indices,
                                           cg_attribute_t **attributes,
                                           int n_attributes,
                                           int n_instances,
                                           cg_draw_flags_t flags)
{
    cg_buffer_t *buffer;
    uint8_t *base;
    size_t buffer_offset;
    size_t index_size;
    GLenum indices_gl_type = 0;

    _cg_flush_attributes_state(
        framebuffer, pipeline, flags, attributes, n_attributes);

    buffer = CG_BUFFER(cg_indices_get_buffer(indices));

    /* Note: we don't try and catch errors with binding the index buffer
     * here since OOM errors at this point indicate that nothing has yet
     * been uploaded to the indices buffer which we consider to be a
     * programmer error.
     */
    base = _cg_buffer_gl_bind(buffer, CG_BUFFER_BIND_TARGET_INDEX_BUFFER, NULL);
    buffer_offset = cg_indices_get_offset(indices);
    index_size = sizeof_index_type(cg_indices_get_type(indices));

    switch (cg_indices_get_type(indices)) {
    case CG_INDICES_TYPE_UNSIGNED_BYTE:
        indices_gl_type = GL_UNSIGNED_BYTE;
        break;
    case CG_INDICES_TYPE_UNSIGNED_SHORT:
        indices_gl_type = GL_UNSIGNED_SHORT;
        break;
    case CG_INDICES_TYPE_UNSIGNED_INT:
        indices_gl_type = GL_UNSIGNED_INT;
        break;
    }

    if (framebuffer->dev->glDrawElementsInstanced) {
        GE(framebuffer->dev,
           glDrawElementsInstanced((GLenum)mode,
                                   n_vertices,
                                   indices_gl_type,
                                   base + buffer_offset + index_size * first_vertex,
                                   n_instances));
    } else {
        GE(framebuffer->dev,
           glDrawElements((GLenum)mode,
                          n_vertices,
                          indices_gl_type,
                          base + buffer_offset + index_size * first_vertex));
    }

    _cg_buffer_gl_unbind(buffer);
}

static bool
mesa_46631_slow_read_pixels_workaround(cg_framebuffer_t *framebuffer,
                                       int x,
                                       int y,
                                       cg_read_pixels_flags_t source,
                                       cg_bitmap_t *bitmap,
                                       cg_error_t **error)
{
    cg_device_t *dev;
    cg_pixel_format_t format;
    cg_bitmap_t *pbo;
    int width;
    int height;
    bool res;
    uint8_t *dst;
    const uint8_t *src;

    dev = cg_framebuffer_get_context(framebuffer);

    width = cg_bitmap_get_width(bitmap);
    height = cg_bitmap_get_height(bitmap);
    format = cg_bitmap_get_format(bitmap);

    pbo = cg_bitmap_new_with_size(dev, width, height, format);

    /* Read into the pbo. We need to disable the flipping because the
       blit fast path in the driver does not work with
       GL_PACK_INVERT_MESA is set */
    res = cg_framebuffer_read_pixels_into_bitmap(
        framebuffer, x, y, source | CG_READ_PIXELS_NO_FLIP, pbo, error);
    if (!res) {
        cg_object_unref(pbo);
        return false;
    }

    /* Copy the pixels back into application's buffer */
    dst = _cg_bitmap_map(
        bitmap, CG_BUFFER_ACCESS_WRITE, CG_BUFFER_MAP_HINT_DISCARD, error);
    if (!dst) {
        cg_object_unref(pbo);
        return false;
    }

    src = _cg_bitmap_map(pbo,
                         CG_BUFFER_ACCESS_READ,
                         0, /* hints */
                         error);
    if (src) {
        int src_rowstride = cg_bitmap_get_rowstride(pbo);
        int dst_rowstride = cg_bitmap_get_rowstride(bitmap);
        int to_copy = _cg_pixel_format_get_bytes_per_pixel(format) * width;
        int y;

        /* If the framebuffer is onscreen we need to flip the
           data while copying */
        if (!cg_is_offscreen(framebuffer)) {
            src += src_rowstride * (height - 1);
            src_rowstride = -src_rowstride;
        }

        for (y = 0; y < height; y++) {
            memcpy(dst, src, to_copy);
            dst += dst_rowstride;
            src += src_rowstride;
        }

        _cg_bitmap_unmap(pbo);
    } else
        res = false;

    _cg_bitmap_unmap(bitmap);

    cg_object_unref(pbo);

    return res;
}

bool
_cg_framebuffer_gl_read_pixels_into_bitmap(cg_framebuffer_t *framebuffer,
                                           int x,
                                           int y,
                                           cg_read_pixels_flags_t source,
                                           cg_bitmap_t *bitmap,
                                           cg_error_t **error)
{
    cg_device_t *dev = framebuffer->dev;
    int framebuffer_height = cg_framebuffer_get_height(framebuffer);
    int width = cg_bitmap_get_width(bitmap);
    int height = cg_bitmap_get_height(bitmap);
    cg_pixel_format_t format = cg_bitmap_get_format(bitmap);
    cg_pixel_format_t required_format;
    GLenum gl_intformat;
    GLenum gl_format;
    GLenum gl_type;
    bool pack_invert_set;
    int status = false;

    /* Workaround for cases where its faster to read into a temporary
     * PBO. This is only worth doing if:
     *
     * • The GPU is an Intel GPU. In that case there is a known
     *   fast-path when reading into a PBO that will use the blitter
     *   instead of the Mesa fallback code. The driver bug will only be
     *   set if this is the case.
     * • We're not already reading into a PBO.
     * • The target format is BGRA. The fast-path blit does not get hit
     *   otherwise.
     * • The size of the data is not trivially small. This isn't a
     *   requirement to hit the fast-path blit but intuitively it feels
     *   like if the amount of data is too small then the cost of
     *   allocating a PBO will outweigh the cost of temporarily
     *   converting the data to floats.
     */
    if ((dev->gpu.driver_bugs &
         CG_GPU_INFO_DRIVER_BUG_MESA_46631_SLOW_READ_PIXELS) &&
        (width > 8 || height > 8) &&
        _cg_pixel_format_premult_stem(format) == CG_PIXEL_FORMAT_BGRA_8888 &&
        cg_bitmap_get_buffer(bitmap) == NULL) {
        cg_error_t *ignore_error = NULL;

        if (mesa_46631_slow_read_pixels_workaround(
                framebuffer, x, y, source, bitmap, &ignore_error))
            return true;
        else
            cg_error_free(ignore_error);
    }

    _cg_framebuffer_flush_state(
        framebuffer, framebuffer, CG_FRAMEBUFFER_STATE_BIND);

    /* The y co-ordinate should be given in OpenGL's coordinate system
     * so 0 is the bottom row
     *
     * NB: all offscreen rendering is done upside down so no conversion
     * is necissary in this case.
     */
    if (!cg_is_offscreen(framebuffer))
        y = framebuffer_height - y - height;

    required_format = dev->driver_vtable->pixel_format_to_gl(dev, format,
                                                             &gl_intformat,
                                                             &gl_format,
                                                             &gl_type);

    /* NB: All offscreen rendering is done upside down so there is no need
     * to flip in this case... */
    if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_MESA_PACK_INVERT) &&
        (source & CG_READ_PIXELS_NO_FLIP) == 0 &&
        !cg_is_offscreen(framebuffer)) {
        GE(dev, glPixelStorei(GL_PACK_INVERT_MESA, true));
        pack_invert_set = true;
    } else
        pack_invert_set = false;

    /* Under GLES only GL_RGBA with GL_UNSIGNED_BYTE as well as an
       implementation specific format under
       GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES and
       GL_IMPLEMENTATION_COLOR_READ_TYPE_OES is supported. We could try
       to be more clever and check if the requested type matches that
       but we would need some reliable functions to convert from GL
       types to CGlib types. For now, lets just always read in
       GL_RGBA/GL_UNSIGNED_BYTE and convert if necessary. We also need
       to use this intermediate buffer if the rowstride has padding
       because GLES does not support setting GL_ROW_LENGTH */
    if ((!_cg_has_private_feature(dev,
                                  CG_PRIVATE_FEATURE_READ_PIXELS_ANY_FORMAT) &&
         (gl_format != GL_RGBA || gl_type != GL_UNSIGNED_BYTE ||
          cg_bitmap_get_rowstride(bitmap) != 4 * width)) ||
        _cg_pixel_format_premult_stem(required_format) != _cg_pixel_format_premult_stem(format)) {
        cg_bitmap_t *tmp_bmp;
        cg_pixel_format_t read_format;
        int bpp, rowstride;
        uint8_t *tmp_data;
        bool succeeded;

        if (_cg_has_private_feature(dev,
                                    CG_PRIVATE_FEATURE_READ_PIXELS_ANY_FORMAT))
            read_format = required_format;
        else {
            read_format = CG_PIXEL_FORMAT_RGBA_8888;
            gl_format = GL_RGBA;
            gl_type = GL_UNSIGNED_BYTE;
        }

        if (_cg_pixel_format_can_be_premultiplied(read_format)) {
            read_format = _cg_pixel_format_premult_stem(read_format);

            if (_cg_pixel_format_is_premultiplied(framebuffer->internal_format))
                read_format = _cg_pixel_format_premultiply(read_format);
        }

        tmp_bmp = _cg_bitmap_new_with_malloc_buffer(dev, width, height,
                                                    read_format, error);
        if (!tmp_bmp)
            goto EXIT;

        bpp = _cg_pixel_format_get_bytes_per_pixel(read_format);
        rowstride = cg_bitmap_get_rowstride(tmp_bmp);

        dev->texture_driver->prep_gl_for_pixels_download(dev, rowstride,
                                                         width, bpp);

        /* Note: we don't worry about catching errors here since we know
         * we won't be lazily allocating storage for this buffer so it
         * won't fail due to lack of memory. */
        tmp_data = _cg_bitmap_gl_bind(
            tmp_bmp, CG_BUFFER_ACCESS_WRITE, CG_BUFFER_MAP_HINT_DISCARD, NULL);

        GE(dev,
           glReadPixels(x, y, width, height, gl_format, gl_type, tmp_data));

        _cg_bitmap_gl_unbind(tmp_bmp);

        succeeded = _cg_bitmap_convert_into_bitmap(tmp_bmp, bitmap, error);

        cg_object_unref(tmp_bmp);

        if (!succeeded)
            goto EXIT;
    } else {
        cg_bitmap_t *shared_bmp;
        cg_pixel_format_t bmp_format;
        int bpp, rowstride;
        bool succeeded = false;
        uint8_t *pixels;
        cg_error_t *internal_error = NULL;

        rowstride = cg_bitmap_get_rowstride(bitmap);

        /* We match the premultiplied state of the target buffer to the
         * premultiplied state of the framebuffer so that it will get
         * converted to the right format below */
        if (_cg_pixel_format_can_be_premultiplied(format)) {
            bmp_format = _cg_pixel_format_premult_stem(format);
            if (_cg_pixel_format_is_premultiplied(framebuffer->internal_format))
                bmp_format = _cg_pixel_format_premultiply(bmp_format);
        }else
            bmp_format = format;

        if (bmp_format != format)
            shared_bmp = _cg_bitmap_new_shared(
                bitmap, bmp_format, width, height, rowstride);
        else
            shared_bmp = cg_object_ref(bitmap);

        bpp = _cg_pixel_format_get_bytes_per_pixel(bmp_format);

        dev->texture_driver->prep_gl_for_pixels_download(dev, rowstride,
                                                         width, bpp);

        pixels = _cg_bitmap_gl_bind(shared_bmp,
                                    CG_BUFFER_ACCESS_WRITE,
                                    0, /* hints */
                                    &internal_error);
        /* NB: _cg_bitmap_gl_bind() can return NULL in sucessfull
         * cases so we have to explicitly check the cg error pointer
         * to know if there was a problem */
        if (internal_error) {
            cg_object_unref(shared_bmp);
            _cg_propagate_error(error, internal_error);
            goto EXIT;
        }

        GE(dev, glReadPixels(x, y, width, height, gl_format, gl_type, pixels));

        _cg_bitmap_gl_unbind(shared_bmp);

        /* Convert to the premult format specified by the caller
           in-place. This will do nothing if the premult status is already
           correct. */
        if (_cg_bitmap_convert_premult_status(shared_bmp, format, error))
            succeeded = true;

        cg_object_unref(shared_bmp);

        if (!succeeded)
            goto EXIT;
    }

    /* NB: All offscreen rendering is done upside down so there is no need
     * to flip in this case... */
    if (!cg_is_offscreen(framebuffer) &&
        (source & CG_READ_PIXELS_NO_FLIP) == 0 && !pack_invert_set) {
        uint8_t *temprow;
        int rowstride;
        uint8_t *pixels;

        rowstride = cg_bitmap_get_rowstride(bitmap);
        pixels = _cg_bitmap_map(bitmap,
                                CG_BUFFER_ACCESS_READ | CG_BUFFER_ACCESS_WRITE,
                                0, /* hints */
                                error);

        if (pixels == NULL)
            goto EXIT;

        temprow = c_alloca(rowstride * sizeof(uint8_t));

        /* vertically flip the buffer in-place */
        for (y = 0; y < height / 2; y++) {
            if (y != height - y - 1) /* skip center row */
            {
                memcpy(temprow, pixels + y * rowstride, rowstride);
                memcpy(pixels + y * rowstride,
                       pixels + (height - y - 1) * rowstride,
                       rowstride);
                memcpy(
                    pixels + (height - y - 1) * rowstride, temprow, rowstride);
            }
        }

        _cg_bitmap_unmap(bitmap);
    }

    status = true;

EXIT:

    /* Currently this function owns the pack_invert state and we don't want this
     * to interfere with other CGlib components so all other code can assume that
     * we leave the pack_invert state off. */
    if (pack_invert_set)
        GE(dev, glPixelStorei(GL_PACK_INVERT_MESA, false));

    return status;
}
