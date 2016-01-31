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

#include <string.h>

#include "cg-debug.h"
#include "cg-device-private.h"
#include "cg-display-private.h"
#include "cg-renderer-private.h"
#include "cg-object-private.h"
#include "cg-util.h"
#include "cg-texture-private.h"
#include "cg-framebuffer-private.h"
#include "cg-onscreen-template-private.h"
#include "cg-clip-stack.h"
#include "cg-winsys-private.h"
#include "cg-pipeline-state-private.h"
#include "cg-primitive-private.h"
#include "cg-offscreen.h"
#include "cg-private.h"
#include "cg-error-private.h"
#include "cg-texture-gl-private.h"
#include "cg-primitive-texture.h"

#define _MATRIX_DEBUG_PRINT(MATRIX)                         \
    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_MATRICES))) {  \
        c_print("%s:\n", C_STRFUNC);                        \
        c_matrix_print(MATRIX);                            \
    }

extern cg_object_class_t _cg_onscreen_class;

static void _cg_offscreen_free(cg_offscreen_t *offscreen);

CG_OBJECT_DEFINE_WITH_CODE(Offscreen,
                           offscreen,
                           _cg_offscreen_class.virt_unref =
                               _cg_framebuffer_unref);

/* XXX:
 * The cg_object_t macros don't support any form of inheritance, so for
 * now we implement the cg_object_t support for the cg_framebuffer_t
 * abstract class manually.
 */

uint32_t
cg_framebuffer_error_domain(void)
{
    return c_quark_from_static_string("cg-framebuffer-error-quark");
}

bool
cg_is_framebuffer(void *object)
{
    cg_object_t *obj = object;

    if (obj == NULL)
        return false;

    return (obj->klass == &_cg_onscreen_class ||
            obj->klass == &_cg_offscreen_class);
}

void
_cg_framebuffer_init(cg_framebuffer_t *framebuffer,
                     cg_device_t *dev,
                     cg_framebuffer_type_t type,
                     int width,
                     int height)
{
    framebuffer->dev = dev;

    framebuffer->type = type;
    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->internal_format = CG_PIXEL_FORMAT_RGBA_8888_PRE;
    framebuffer->viewport_x = 0;
    framebuffer->viewport_y = 0;
    framebuffer->viewport_width = width;
    framebuffer->viewport_height = height;
    framebuffer->viewport_age = 0;
    framebuffer->viewport_age_for_scissor_workaround = -1;
    framebuffer->dither_enabled = true;
    framebuffer->depth_writing_enabled = true;

    framebuffer->modelview_stack = cg_matrix_stack_new(dev);
    framebuffer->projection_stack = cg_matrix_stack_new(dev);

    framebuffer->dirty_bitmasks = true;

    framebuffer->color_mask = CG_COLOR_MASK_ALL;

    framebuffer->samples_per_pixel = 0;

    framebuffer->clip_stack = NULL;

    dev->framebuffers = c_llist_prepend(dev->framebuffers, framebuffer);
}

void
_cg_framebuffer_set_internal_format(cg_framebuffer_t *framebuffer,
                                    cg_pixel_format_t internal_format)
{
    framebuffer->internal_format = internal_format;
}

void
_cg_framebuffer_free(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;

    _cg_fence_cancel_fences_for_framebuffer(framebuffer);

    _cg_clip_stack_unref(framebuffer->clip_stack);

    cg_object_unref(framebuffer->modelview_stack);
    framebuffer->modelview_stack = NULL;

    cg_object_unref(framebuffer->projection_stack);
    framebuffer->projection_stack = NULL;

    if (dev->viewport_scissor_workaround_framebuffer == framebuffer)
        dev->viewport_scissor_workaround_framebuffer = NULL;

    dev->framebuffers = c_llist_remove(dev->framebuffers, framebuffer);

    if (dev->current_draw_buffer == framebuffer)
        dev->current_draw_buffer = NULL;
    if (dev->current_read_buffer == framebuffer)
        dev->current_read_buffer = NULL;
}

const cg_winsys_vtable_t *
_cg_framebuffer_get_winsys(cg_framebuffer_t *framebuffer)
{
    return framebuffer->dev->display->renderer->winsys_vtable;
}

/* This api bypasses flushing the framebuffer state */
void
_cg_framebuffer_clear_without_flush4f(cg_framebuffer_t *framebuffer,
                                      cg_buffer_bit_t buffers,
                                      float red,
                                      float green,
                                      float blue,
                                      float alpha)
{
    cg_device_t *dev = framebuffer->dev;

    if (!buffers) {
        static bool shown = false;

        if (!shown) {
            c_warning("You should specify at least one auxiliary buffer "
                      "when calling cg_framebuffer_clear");
        }

        return;
    }

    dev->driver_vtable->framebuffer_clear(
        framebuffer, buffers, red, green, blue, alpha);
}

void
_cg_framebuffer_mark_mid_scene(cg_framebuffer_t *framebuffer)
{
    framebuffer->mid_scene = true;
}

void
cg_framebuffer_clear4f(cg_framebuffer_t *framebuffer,
                       cg_buffer_bit_t buffers,
                       float red,
                       float green,
                       float blue,
                       float alpha)
{
    CG_NOTE(DRAW, "Clear begin");

    /* NB: _cg_framebuffer_flush_state may disrupt various state (such
     * as the pipeline state) when flushing the clip stack, so should
     * always be done first when preparing to draw. */
    _cg_framebuffer_flush_state(
        framebuffer, framebuffer, CG_FRAMEBUFFER_STATE_ALL);

    _cg_framebuffer_clear_without_flush4f(
        framebuffer, buffers, red, green, blue, alpha);

    CG_NOTE(DRAW, "Clear end");

    _cg_framebuffer_mark_mid_scene(framebuffer);
}

/* Note: the 'buffers' and 'color' arguments were switched around on
 * purpose compared to the original cg_clear API since it was odd
 * that you would be expected to specify a color before even
 * necessarily choosing to clear the color buffer.
 */
void
cg_framebuffer_clear(cg_framebuffer_t *framebuffer,
                     cg_buffer_bit_t buffers,
                     const cg_color_t *color)
{
    cg_framebuffer_clear4f(framebuffer,
                           buffers,
                           color->red,
                           color->green,
                           color->blue,
                           color->alpha);
}

/* We will lazily allocate framebuffers if necessary when querying
 * their size/viewport but note we need to be careful in the case of
 * onscreen framebuffers that are instantiated with an initial request
 * size that we don't trigger an allocation when this is queried since
 * that would lead to a recursion when the winsys backend queries this
 * requested size during allocation. */
static void
ensure_size_initialized(cg_framebuffer_t *framebuffer)
{
    /* In the case of offscreen framebuffers backed by a texture then
     * until that texture has been allocated we might not know the size
     * of the framebuffer */
    if (framebuffer->width < 0) {
        /* Currently we assume the size is always initialized for
         * onscreen framebuffers. */
        c_return_if_fail(cg_is_offscreen(framebuffer));

        /* We also assume the size would have been initialized if the
         * framebuffer were allocated. */
        c_return_if_fail(!framebuffer->allocated);

        cg_framebuffer_allocate(framebuffer, NULL);
    }
}

int
cg_framebuffer_get_width(cg_framebuffer_t *framebuffer)
{
    ensure_size_initialized(framebuffer);
    return framebuffer->width;
}

int
cg_framebuffer_get_height(cg_framebuffer_t *framebuffer)
{
    ensure_size_initialized(framebuffer);
    return framebuffer->height;
}

cg_clip_stack_t *
_cg_framebuffer_get_clip_stack(cg_framebuffer_t *framebuffer)
{
    return framebuffer->clip_stack;
}

void
_cg_framebuffer_set_clip_stack(cg_framebuffer_t *framebuffer,
                               cg_clip_stack_t *stack)
{
    _cg_clip_stack_ref(stack);
    _cg_clip_stack_unref(framebuffer->clip_stack);
    framebuffer->clip_stack = stack;
}

void
cg_framebuffer_set_viewport(
    cg_framebuffer_t *framebuffer, float x, float y, float width, float height)
{
    cg_device_t *dev = framebuffer->dev;

    c_return_if_fail(width > 0 && height > 0);

    if (framebuffer->viewport_x == x && framebuffer->viewport_y == y &&
        framebuffer->viewport_width == width &&
        framebuffer->viewport_height == height)
        return;

    framebuffer->viewport_x = x;
    framebuffer->viewport_y = y;
    framebuffer->viewport_width = width;
    framebuffer->viewport_height = height;
    framebuffer->viewport_age++;

    if (dev->current_draw_buffer == framebuffer) {
        dev->current_draw_buffer_changes |= CG_FRAMEBUFFER_STATE_VIEWPORT;

        if (dev->needs_viewport_scissor_workaround)
            dev->current_draw_buffer_changes |= CG_FRAMEBUFFER_STATE_CLIP;
    }
}

float
cg_framebuffer_get_viewport_x(cg_framebuffer_t *framebuffer)
{
    return framebuffer->viewport_x;
}

float
cg_framebuffer_get_viewport_y(cg_framebuffer_t *framebuffer)
{
    return framebuffer->viewport_y;
}

float
cg_framebuffer_get_viewport_width(cg_framebuffer_t *framebuffer)
{
    ensure_size_initialized(framebuffer);
    return framebuffer->viewport_width;
}

float
cg_framebuffer_get_viewport_height(cg_framebuffer_t *framebuffer)
{
    ensure_size_initialized(framebuffer);
    return framebuffer->viewport_height;
}

void
cg_framebuffer_get_viewport4fv(cg_framebuffer_t *framebuffer,
                               float *viewport)
{
    ensure_size_initialized(framebuffer);

    viewport[0] = framebuffer->viewport_x;
    viewport[1] = framebuffer->viewport_y;
    viewport[2] = framebuffer->viewport_width;
    viewport[3] = framebuffer->viewport_height;
}

cg_matrix_stack_t *
_cg_framebuffer_get_modelview_stack(cg_framebuffer_t *framebuffer)
{
    return framebuffer->modelview_stack;
}

cg_matrix_stack_t *
_cg_framebuffer_get_projection_stack(cg_framebuffer_t *framebuffer)
{
    return framebuffer->projection_stack;
}

void
_cg_framebuffer_add_dependency(cg_framebuffer_t *framebuffer,
                               cg_framebuffer_t *dependency)
{
    c_llist_t *l;

    for (l = framebuffer->deps; l; l = l->next) {
        cg_framebuffer_t *existing_dep = l->data;
        if (existing_dep == dependency)
            return;
    }

    /* TODO: generalize the primed-array type structure we e.g. use for
     * cg_object_set_user_data or for pipeline children as a way to
     * avoid quite a lot of mid-scene micro allocations here... */
    framebuffer->deps =
        c_llist_prepend(framebuffer->deps, cg_object_ref(dependency));
}

void
_cg_framebuffer_remove_all_dependencies(cg_framebuffer_t *framebuffer)
{
    c_llist_t *l;
    for (l = framebuffer->deps; l; l = l->next)
        cg_object_unref(l->data);
    c_llist_free(framebuffer->deps);
    framebuffer->deps = NULL;
}

void
_cg_framebuffer_flush(cg_framebuffer_t *framebuffer)
{
    /* Currently a NOP */
}

cg_offscreen_t *
_cg_offscreen_new(cg_device_t *dev,
                  int width, /* -1 derived from attached texture */
                  int height, /* -1 derived from attached texture */
                  cg_offscreen_flags_t create_flags)
{
    cg_offscreen_t *offscreen;
    cg_framebuffer_t *fb;
    cg_offscreen_t *ret;

    offscreen = c_new0(cg_offscreen_t, 1);
    offscreen->create_flags = create_flags;

    fb = CG_FRAMEBUFFER(offscreen);
 
    _cg_framebuffer_init(fb,
                         dev,
                         CG_FRAMEBUFFER_TYPE_OFFSCREEN,
                         width,
                         height);

    ret = _cg_offscreen_object_new(offscreen);

    return ret;
}

cg_offscreen_t *
cg_offscreen_new(cg_device_t *dev,
                 int width, /* -1 derived from attached texture */
                 int height) /* -1 derived from attached texture */
{
    return _cg_offscreen_new(dev, width, height,
                             CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL);
}

void
cg_offscreen_attach_color_texture(cg_offscreen_t *offscreen,
                                  cg_texture_t *texture,
                                  int level)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(offscreen);

    c_return_if_fail(framebuffer->allocated == false);
    c_return_if_fail(cg_is_texture(texture));

    if (offscreen->texture) {
        cg_object_unref(offscreen->texture);
        offscreen->texture = NULL;
    }

    /* It's possible to create an offscreen framebuffer with no
     * associated color  */
    if (texture) {
        offscreen->texture = cg_object_ref(texture);
        offscreen->texture_level = level;
    }
}

void
cg_offscreen_attach_depth_texture(cg_offscreen_t *offscreen,
                                  cg_texture_t *texture,
                                  int level)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(offscreen);

    c_return_if_fail(framebuffer->allocated == false);
    c_return_if_fail(cg_is_texture(texture));

    if (offscreen->depth_texture) {
        cg_object_unref(offscreen->depth_texture);
        offscreen->depth_texture = NULL;
        offscreen->depth_texture_level = 0;
    }

    if (texture) {
        offscreen->depth_texture = cg_object_ref(texture);
        offscreen->depth_texture_level = level;
    }
}

cg_offscreen_t *
_cg_offscreen_new_with_texture_full(cg_texture_t *color_texture,
                                    cg_offscreen_flags_t create_flags,
                                    int level)
{
    cg_device_t *dev = color_texture->dev;

    /* NB: we can't assume we can query the texture's width yet, since
     * it might not have been allocated yet and for example if the
     * texture is being loaded from a file then the file might not
     * have been read yet. */

    cg_offscreen_t *offscreen = _cg_offscreen_new(dev,
                                                  -1, /* width from attached texture */
                                                  -1, /* height from attached texture */
                                                  create_flags);

    cg_offscreen_attach_color_texture(offscreen,
                                      color_texture,
                                      level);

    return offscreen;
}

cg_offscreen_t *
cg_offscreen_new_with_texture(cg_texture_t *texture)
{
    return _cg_offscreen_new_with_texture_full(texture, 0, 0);
}

static void
_cg_offscreen_free(cg_offscreen_t *offscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(offscreen);
    cg_device_t *dev = framebuffer->dev;

    dev->driver_vtable->offscreen_free(offscreen);

    /* Chain up to parent */
    _cg_framebuffer_free(framebuffer);

    if (offscreen->texture != NULL)
        cg_object_unref(offscreen->texture);

    if (offscreen->depth_texture != NULL)
        cg_object_unref(offscreen->depth_texture);

    c_free(offscreen);
}

static bool
_offscreen_allocate(cg_framebuffer_t *framebuffer, cg_error_t **error)
{
    cg_device_t *dev = framebuffer->dev;
    cg_offscreen_t *offscreen = CG_OFFSCREEN(framebuffer);
    bool allocated_depth_tex = false;

    /* TODO: generalise the handling of framebuffer attachments...
    */

    if (offscreen->texture) {
        int level_width;
        int level_height;

        if (!cg_texture_allocate(offscreen->texture, error))
            return false;

        /* NB: it's only after allocating the texture that we will
         * determine whether a texture needs slicing... */
        if (cg_texture_is_sliced(offscreen->texture)) {
            _cg_set_error(error,
                          CG_SYSTEM_ERROR,
                          CG_SYSTEM_ERROR_UNSUPPORTED,
                          "Can't create offscreen framebuffer from "
                          "sliced texture");
            return false;
        }

        /* Now that the texture has been allocated we can determine a
         * size for the framebuffer... */

        c_return_val_if_fail(offscreen->texture_level <
                             _cg_texture_get_n_levels(offscreen->texture),
                             false);

        _cg_texture_get_level_size(offscreen->texture,
                                   offscreen->texture_level,
                                   &level_width,
                                   &level_height,
                                   NULL);

        if (framebuffer->width < 0)
            framebuffer->width = level_width;
        else if (framebuffer->width != level_width) {
            _cg_set_error(error,
                          CG_SYSTEM_ERROR,
                          CG_SYSTEM_ERROR_UNSUPPORTED,
                          "Conflicting given size and calculated texture "
                          "level size");
            return false;
        }
        if (framebuffer->height < 0)
            framebuffer->height = level_height;
        else if (framebuffer->width != level_width) {
            _cg_set_error(error,
                          CG_SYSTEM_ERROR,
                          CG_SYSTEM_ERROR_UNSUPPORTED,
                          "Conflicting given size and calculated texture "
                          "level size");
            return false;
        }

        /* Forward the texture format as the internal format of the
         * framebuffer */
        framebuffer->internal_format =
            _cg_texture_get_format(offscreen->texture);
    }

    if (framebuffer->config.depth_texture_enabled) {
        int level_width;
        int level_height;

        if (offscreen->depth_texture == NULL) {
            int width = framebuffer->width;
            int height = framebuffer->height;

            c_return_val_if_fail(width > 0, false);
            c_return_val_if_fail(height > 0, false);
            c_return_val_if_fail(offscreen->depth_texture_level == 0, false);

            offscreen->depth_texture = (cg_texture_t *)
                cg_texture_2d_new_with_size(dev, width, height);

            cg_texture_set_components(offscreen->depth_texture,
                                      CG_TEXTURE_COMPONENTS_DEPTH_STENCIL);

            allocated_depth_tex = true;
        }

        if (!cg_texture_allocate(offscreen->depth_texture, error))
            goto error;

        c_return_val_if_fail(offscreen->depth_texture_level <
                             _cg_texture_get_n_levels(offscreen->depth_texture),
                             false);

        _cg_texture_get_level_size(offscreen->depth_texture,
                                   offscreen->depth_texture_level,
                                   &level_width,
                                   &level_height,
                                   NULL);

        if (framebuffer->width < 0)
            framebuffer->width = level_width;
        else if (framebuffer->width != level_width) {
            _cg_set_error(error,
                          CG_SYSTEM_ERROR,
                          CG_SYSTEM_ERROR_UNSUPPORTED,
                          "Conflicting pre-determined size and calculated "
                          "depth texture level size");
            goto error;
        }
        if (framebuffer->height < 0)
            framebuffer->height = level_height;
        else if (framebuffer->width != level_width) {
            _cg_set_error(error,
                          CG_SYSTEM_ERROR,
                          CG_SYSTEM_ERROR_UNSUPPORTED,
                          "Conflicting pre-determined size and calculated "
                          "depth texture level size");
            goto error;
        }
    }

    c_warn_if_fail(framebuffer->width > 0);
    c_warn_if_fail(framebuffer->height > 0);

    framebuffer->viewport_width = framebuffer->width;
    framebuffer->viewport_height = framebuffer->height;

    if (!dev->driver_vtable->offscreen_allocate(offscreen, error))
        goto error;

    if (offscreen->texture)
        _cg_texture_associate_framebuffer(offscreen->texture, framebuffer);
    if (offscreen->depth_texture)
        _cg_texture_associate_framebuffer(offscreen->depth_texture, framebuffer);

    return true;

error:
    if (allocated_depth_tex) {
        cg_object_unref(offscreen->depth_texture);
        offscreen->depth_texture = NULL;
    }
    return false;
}

bool
cg_framebuffer_allocate(cg_framebuffer_t *framebuffer, cg_error_t **error)
{
    if (framebuffer->allocated)
        return true;

    if (framebuffer->type == CG_FRAMEBUFFER_TYPE_ONSCREEN) {
        cg_onscreen_t *onscreen = CG_ONSCREEN(framebuffer);
        const cg_winsys_vtable_t *winsys =
            _cg_framebuffer_get_winsys(framebuffer);
        cg_device_t *dev = framebuffer->dev;

        if (framebuffer->config.depth_texture_enabled) {
            _cg_set_error(error,
                          CG_FRAMEBUFFER_ERROR,
                          CG_FRAMEBUFFER_ERROR_ALLOCATE,
                          "Can't allocate onscreen framebuffer with a "
                          "texture based depth buffer");
            return false;
        }

        if (!winsys->onscreen_init(onscreen, error))
            return false;

        /* If the winsys doesn't support dirty events then we'll report
         * one on allocation so that if the application only paints in
         * response to dirty events then it will at least paint once to
         * start */
        if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_DIRTY_EVENTS))
            _cg_onscreen_queue_full_dirty(onscreen);
    } else {
        return _offscreen_allocate(framebuffer, error);
    }

    framebuffer->allocated = true;

    return true;
}

static unsigned long
_cg_framebuffer_compare_viewport_state(cg_framebuffer_t *a,
                                       cg_framebuffer_t *b)
{
    if (a->viewport_x != b->viewport_x || a->viewport_y != b->viewport_y ||
        a->viewport_width != b->viewport_width ||
        a->viewport_height != b->viewport_height ||
        /* NB: we render upside down to offscreen framebuffers and that
         * can affect how we setup the GL viewport... */
        a->type != b->type) {
        unsigned long differences = CG_FRAMEBUFFER_STATE_VIEWPORT;
        cg_device_t *dev = a->dev;

        /* XXX: ONGOING BUG: Intel viewport scissor
         *
         * Intel gen6 drivers don't currently correctly handle offset
         * viewports, since primitives aren't clipped within the bounds of
         * the viewport.  To workaround this we push our own clip for the
         * viewport that will use scissoring to ensure we clip as expected.
         *
         * This workaround implies that a change in viewport state is
         * effectively also a change in the clipping state.
         *
         * TODO: file a bug upstream!
         */
        if (C_UNLIKELY(dev->needs_viewport_scissor_workaround))
            differences |= CG_FRAMEBUFFER_STATE_CLIP;

        return differences;
    } else
        return 0;
}

static unsigned long
_cg_framebuffer_compare_clip_state(cg_framebuffer_t *a,
                                   cg_framebuffer_t *b)
{
    if (a->clip_stack != b->clip_stack)
        return CG_FRAMEBUFFER_STATE_CLIP;
    else
        return 0;
}

static unsigned long
_cg_framebuffer_compare_dither_state(cg_framebuffer_t *a,
                                     cg_framebuffer_t *b)
{
    return a->dither_enabled != b->dither_enabled ? CG_FRAMEBUFFER_STATE_DITHER
           : 0;
}

static unsigned long
_cg_framebuffer_compare_modelview_state(cg_framebuffer_t *a,
                                        cg_framebuffer_t *b)
{
    /* We always want to flush the modelview state. All this does is set
       the current modelview stack on the context to the framebuffer's
       stack. */
    return CG_FRAMEBUFFER_STATE_MODELVIEW;
}

static unsigned long
_cg_framebuffer_compare_projection_state(cg_framebuffer_t *a,
                                         cg_framebuffer_t *b)
{
    /* We always want to flush the projection state. All this does is
       set the current projection stack on the context to the
       framebuffer's stack. */
    return CG_FRAMEBUFFER_STATE_PROJECTION;
}

static unsigned long
_cg_framebuffer_compare_color_mask_state(cg_framebuffer_t *a,
                                         cg_framebuffer_t *b)
{
    if (cg_framebuffer_get_color_mask(a) != cg_framebuffer_get_color_mask(b))
        return CG_FRAMEBUFFER_STATE_COLOR_MASK;
    else
        return 0;
}

static unsigned long
_cg_framebuffer_compare_front_face_winding_state(cg_framebuffer_t *a,
                                                 cg_framebuffer_t *b)
{
    if (a->type != b->type)
        return CG_FRAMEBUFFER_STATE_FRONT_FACE_WINDING;
    else
        return 0;
}

static unsigned long
_cg_framebuffer_compare_depth_write_state(cg_framebuffer_t *a,
                                          cg_framebuffer_t *b)
{
    return a->depth_writing_enabled != b->depth_writing_enabled
           ? CG_FRAMEBUFFER_STATE_DEPTH_WRITE
           : 0;
}

unsigned long
_cg_framebuffer_compare(cg_framebuffer_t *a,
                        cg_framebuffer_t *b,
                        unsigned long state)
{
    unsigned long differences = 0;
    int bit;

    if (state & CG_FRAMEBUFFER_STATE_BIND) {
        differences |= CG_FRAMEBUFFER_STATE_BIND;
        state &= ~CG_FRAMEBUFFER_STATE_BIND;
    }

    CG_FLAGS_FOREACH_START(&state, 1, bit)
    {
        /* XXX: We considered having an array of callbacks for each state index
         * that we'd call here but decided that this way the compiler is more
         * likely going to be able to in-line the comparison functions and use
         * the index to jump straight to the required code. */
        switch (bit) {
        case CG_FRAMEBUFFER_STATE_INDEX_VIEWPORT:
            differences |= _cg_framebuffer_compare_viewport_state(a, b);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_CLIP:
            differences |= _cg_framebuffer_compare_clip_state(a, b);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_DITHER:
            differences |= _cg_framebuffer_compare_dither_state(a, b);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_MODELVIEW:
            differences |= _cg_framebuffer_compare_modelview_state(a, b);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_PROJECTION:
            differences |= _cg_framebuffer_compare_projection_state(a, b);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_COLOR_MASK:
            differences |= _cg_framebuffer_compare_color_mask_state(a, b);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_FRONT_FACE_WINDING:
            differences |=
                _cg_framebuffer_compare_front_face_winding_state(a, b);
            break;
        case CG_FRAMEBUFFER_STATE_INDEX_DEPTH_WRITE:
            differences |= _cg_framebuffer_compare_depth_write_state(a, b);
            break;
        default:
            c_warn_if_reached();
        }
    }
    CG_FLAGS_FOREACH_END;

    return differences;
}

void
_cg_framebuffer_flush_state(cg_framebuffer_t *draw_buffer,
                            cg_framebuffer_t *read_buffer,
                            cg_framebuffer_state_t state)
{
    cg_device_t *dev = draw_buffer->dev;

    dev->driver_vtable->framebuffer_flush_state(
        draw_buffer, read_buffer, state);
}

int
cg_framebuffer_get_red_bits(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;
    cg_framebuffer_bits_t bits;

    dev->driver_vtable->framebuffer_query_bits(framebuffer, &bits);

    return bits.red;
}

int
cg_framebuffer_get_green_bits(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;
    cg_framebuffer_bits_t bits;

    dev->driver_vtable->framebuffer_query_bits(framebuffer, &bits);

    return bits.green;
}

int
cg_framebuffer_get_blue_bits(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;
    cg_framebuffer_bits_t bits;

    dev->driver_vtable->framebuffer_query_bits(framebuffer, &bits);

    return bits.blue;
}

int
cg_framebuffer_get_alpha_bits(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;
    cg_framebuffer_bits_t bits;

    dev->driver_vtable->framebuffer_query_bits(framebuffer, &bits);

    return bits.alpha;
}

int
cg_framebuffer_get_depth_bits(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;
    cg_framebuffer_bits_t bits;

    dev->driver_vtable->framebuffer_query_bits(framebuffer, &bits);

    return bits.depth;
}

int
_cg_framebuffer_get_stencil_bits(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;
    cg_framebuffer_bits_t bits;

    dev->driver_vtable->framebuffer_query_bits(framebuffer, &bits);

    return bits.stencil;
}

cg_color_mask_t
cg_framebuffer_get_color_mask(cg_framebuffer_t *framebuffer)
{
    return framebuffer->color_mask;
}

void
cg_framebuffer_set_color_mask(cg_framebuffer_t *framebuffer,
                              cg_color_mask_t color_mask)
{
    if (framebuffer->color_mask == color_mask)
        return;

    framebuffer->color_mask = color_mask;

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_COLOR_MASK;
}

bool
cg_framebuffer_get_depth_write_enabled(cg_framebuffer_t *framebuffer)
{
    return framebuffer->depth_writing_enabled;
}

void
cg_framebuffer_set_depth_write_enabled(cg_framebuffer_t *framebuffer,
                                       bool depth_write_enabled)
{
    if (framebuffer->depth_writing_enabled == depth_write_enabled)
        return;

    framebuffer->depth_writing_enabled = depth_write_enabled;

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_DEPTH_WRITE;
}

bool
cg_framebuffer_get_dither_enabled(cg_framebuffer_t *framebuffer)
{
    return framebuffer->dither_enabled;
}

void
cg_framebuffer_set_dither_enabled(cg_framebuffer_t *framebuffer,
                                  bool dither_enabled)
{
    if (framebuffer->dither_enabled == dither_enabled)
        return;

    framebuffer->dither_enabled = dither_enabled;

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_DITHER;
}

void
cg_framebuffer_set_depth_texture_enabled(cg_framebuffer_t *framebuffer,
                                         bool enabled)
{
    c_return_if_fail(!framebuffer->allocated);

    framebuffer->config.depth_texture_enabled = enabled;
}

bool
cg_framebuffer_get_depth_texture_enabled(cg_framebuffer_t *framebuffer)
{
    return framebuffer->config.depth_texture_enabled;
}

cg_texture_t *
cg_framebuffer_get_depth_texture(cg_framebuffer_t *framebuffer)
{
    /* lazily allocate the framebuffer... */
    if (!cg_framebuffer_allocate(framebuffer, NULL))
        return NULL;

    c_return_val_if_fail(cg_is_offscreen(framebuffer), NULL);
    return CG_OFFSCREEN(framebuffer)->depth_texture;
}

int
cg_framebuffer_get_samples_per_pixel(cg_framebuffer_t *framebuffer)
{
    if (framebuffer->allocated)
        return framebuffer->samples_per_pixel;
    else
        return framebuffer->config.samples_per_pixel;
}

void
cg_framebuffer_set_samples_per_pixel(cg_framebuffer_t *framebuffer,
                                     int samples_per_pixel)
{
    c_return_if_fail(!framebuffer->allocated);

    framebuffer->config.samples_per_pixel = samples_per_pixel;
}

void
cg_framebuffer_resolve_samples(cg_framebuffer_t *framebuffer)
{
    cg_framebuffer_resolve_samples_region(
        framebuffer, 0, 0, framebuffer->width, framebuffer->height);

    /* TODO: Make this happen implicitly when the resolve texture next gets used
     * as a source, either via cg_texture_get_data(), via cg_read_pixels() or
     * if used as a source for rendering. We would also implicitly resolve if
     * necessary before freeing a cg_framebuffer_t.
     *
     * This API should still be kept but it is optional, only necessary
     * if the user wants to explicitly control when the resolve happens e.g.
     * to ensure it's done in advance of it being used as a source.
     *
     * Every texture should have a cg_framebuffer_t *needs_resolve member
     * internally. When the texture gets validated before being used as a source
     * we should first check the needs_resolve pointer and if set we'll
     * automatically call cg_framebuffer_resolve_samples ().
     *
     * Calling cg_framebuffer_resolve_samples() or
     * cg_framebuffer_resolve_samples_region() should reset the textures
     * needs_resolve pointer to NULL.
     *
     * Rendering anything to a framebuffer will cause the corresponding
     * texture's ->needs_resolve pointer to be set.
     *
     * XXX: Note: we only need to address this TODO item when adding support for
     * EXT_framebuffer_multisample because currently we only support hardware
     * that resolves implicitly anyway.
     */
}

void
cg_framebuffer_resolve_samples_region(
    cg_framebuffer_t *framebuffer, int x, int y, int width, int height)
{
    /* NOP for now since we don't support EXT_framebuffer_multisample yet which
     * requires an explicit resolve. */
}

cg_device_t *
cg_framebuffer_get_context(cg_framebuffer_t *framebuffer)
{
    c_return_val_if_fail(framebuffer != NULL, NULL);

    return framebuffer->dev;
}

bool
cg_framebuffer_read_pixels_into_bitmap(cg_framebuffer_t *framebuffer,
                                       int x,
                                       int y,
                                       cg_read_pixels_flags_t source,
                                       cg_bitmap_t *bitmap,
                                       cg_error_t **error)
{
    c_return_val_if_fail(source & CG_READ_PIXELS_COLOR_BUFFER, false);
    c_return_val_if_fail(cg_is_framebuffer(framebuffer), false);

    if (!cg_framebuffer_allocate(framebuffer, error))
        return false;

    _cg_framebuffer_flush(framebuffer);

    return framebuffer->dev->driver_vtable->framebuffer_read_pixels_into_bitmap(
        framebuffer, x, y, source, bitmap, error);
}

bool
cg_framebuffer_read_pixels(cg_framebuffer_t *framebuffer,
                           int x,
                           int y,
                           int width,
                           int height,
                           cg_pixel_format_t format,
                           uint8_t *pixels)
{
    int bpp = _cg_pixel_format_get_bytes_per_pixel(format);
    cg_bitmap_t *bitmap;
    bool ret;

    bitmap = cg_bitmap_new_for_data(framebuffer->dev,
                                    width,
                                    height,
                                    format,
                                    bpp * width, /* rowstride */
                                    pixels);

    /* Note: we don't try and catch errors here since we created the
     * bitmap storage up-front and can assume we wont hit an
     * out-of-memory error which should be the only exception
     * this api throws.
     */
    ret = cg_framebuffer_read_pixels_into_bitmap(
        framebuffer, x, y, CG_READ_PIXELS_COLOR_BUFFER, bitmap, NULL);
    cg_object_unref(bitmap);

    return ret;
}

void
_cg_blit_framebuffer(cg_framebuffer_t *src,
                     cg_framebuffer_t *dest,
                     int src_x,
                     int src_y,
                     int dst_x,
                     int dst_y,
                     int width,
                     int height)
{
    cg_device_t *dev = src->dev;

    c_return_if_fail(
        _cg_has_private_feature(dev, CG_PRIVATE_FEATURE_OFFSCREEN_BLIT));

    /* We can only support blitting between offscreen buffers because
       otherwise we would need to mirror the image and GLES2.0 doesn't
       support this */
    c_return_if_fail(cg_is_offscreen(src));
    c_return_if_fail(cg_is_offscreen(dest));
    /* The buffers must be the same format */
    c_return_if_fail(src->internal_format == dest->internal_format);

    /* Make sure the current framebuffers are bound. We explicitly avoid
       flushing the clip state so we can bind our own empty state */
    _cg_framebuffer_flush_state(
        dest, src, CG_FRAMEBUFFER_STATE_ALL & ~CG_FRAMEBUFFER_STATE_CLIP);

    /* Flush any empty clip stack because glBlitFramebuffer is affected
       by the scissor and we want to hide this feature for the CGlib API
       because it's not obvious to an app how the clip state will affect
       the scissor */
    _cg_clip_stack_flush(NULL, dest);

    /* XXX: Because we are manually flushing clip state here we need to
     * make sure that the clip state gets updated the next time we flush
     * framebuffer state by marking the current framebuffer's clip state
     * as changed */
    dev->current_draw_buffer_changes |= CG_FRAMEBUFFER_STATE_CLIP;

    dev->glBlitFramebuffer(src_x,
                           src_y,
                           src_x + width,
                           src_y + height,
                           dst_x,
                           dst_y,
                           dst_x + width,
                           dst_y + height,
                           GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);
}

void
cg_framebuffer_discard_buffers(cg_framebuffer_t *framebuffer,
                               cg_buffer_bit_t buffers)
{
    cg_device_t *dev = framebuffer->dev;

    c_return_if_fail(buffers & CG_BUFFER_BIT_COLOR);

    dev->driver_vtable->framebuffer_discard_buffers(framebuffer, buffers);
}

void
cg_framebuffer_finish(cg_framebuffer_t *framebuffer)
{
    cg_device_t *dev = framebuffer->dev;

    _cg_framebuffer_flush(framebuffer);

    dev->driver_vtable->framebuffer_finish(framebuffer);
}

void
cg_framebuffer_push_matrix(cg_framebuffer_t *framebuffer)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_push(modelview_stack);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cg_framebuffer_pop_matrix(cg_framebuffer_t *framebuffer)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_pop(modelview_stack);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cg_framebuffer_identity_matrix(cg_framebuffer_t *framebuffer)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_load_identity(modelview_stack);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cg_framebuffer_scale(cg_framebuffer_t *framebuffer, float x, float y, float z)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_scale(modelview_stack, x, y, z);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cg_framebuffer_translate(cg_framebuffer_t *framebuffer,
                         float x,
                         float y,
                         float z)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_translate(modelview_stack, x, y, z);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cg_framebuffer_rotate(
    cg_framebuffer_t *framebuffer, float angle, float x, float y, float z)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_rotate(modelview_stack, angle, x, y, z);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cg_framebuffer_rotate_quaternion(cg_framebuffer_t *framebuffer,
                                 const c_quaternion_t *quaternion)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_rotate_quaternion(modelview_stack, quaternion);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cg_framebuffer_rotate_euler(cg_framebuffer_t *framebuffer,
                            const c_euler_t *euler)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_rotate_euler(modelview_stack, euler);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cg_framebuffer_transform(cg_framebuffer_t *framebuffer,
                         const c_matrix_t *matrix)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_multiply(modelview_stack, matrix);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;
}

void
cg_framebuffer_perspective(cg_framebuffer_t *framebuffer,
                           float fov_y,
                           float aspect,
                           float z_near,
                           float z_far)
{
    float ymax = z_near * tanf(fov_y * C_PI / 360.0);

    cg_framebuffer_frustum(framebuffer,
                           -ymax * aspect, /* left */
                           ymax * aspect, /* right */
                           -ymax, /* bottom */
                           ymax, /* top */
                           z_near,
                           z_far);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_PROJECTION;
}

void
cg_framebuffer_frustum(cg_framebuffer_t *framebuffer,
                       float left,
                       float right,
                       float bottom,
                       float top,
                       float z_near,
                       float z_far)
{
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);

    cg_matrix_stack_load_identity(projection_stack);

    cg_matrix_stack_frustum(
        projection_stack, left, right, bottom, top, z_near, z_far);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_PROJECTION;
}

void
cg_framebuffer_orthographic(cg_framebuffer_t *framebuffer,
                            float x_1,
                            float y_1,
                            float x_2,
                            float y_2,
                            float near,
                            float far)
{
    c_matrix_t ortho;
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);

    c_matrix_init_identity(&ortho);
    c_matrix_orthographic(&ortho, x_1, y_1, x_2, y_2, near, far);
    cg_matrix_stack_set(projection_stack, &ortho);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_PROJECTION;
}

void
_cg_framebuffer_push_projection(cg_framebuffer_t *framebuffer)
{
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);
    cg_matrix_stack_push(projection_stack);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_PROJECTION;
}

void
_cg_framebuffer_pop_projection(cg_framebuffer_t *framebuffer)
{
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);
    cg_matrix_stack_pop(projection_stack);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_PROJECTION;
}

void
cg_framebuffer_get_modelview_matrix(cg_framebuffer_t *framebuffer,
                                    c_matrix_t *matrix)
{
    cg_matrix_entry_t *modelview_entry =
        _cg_framebuffer_get_modelview_entry(framebuffer);
    cg_matrix_entry_get(modelview_entry, matrix);
    _MATRIX_DEBUG_PRINT(matrix);
}

void
cg_framebuffer_set_modelview_matrix(cg_framebuffer_t *framebuffer,
                                    const c_matrix_t *matrix)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_set(modelview_stack, matrix);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;

    _MATRIX_DEBUG_PRINT(matrix);
}

void
cg_framebuffer_get_projection_matrix(cg_framebuffer_t *framebuffer,
                                     c_matrix_t *matrix)
{
    cg_matrix_entry_t *projection_entry =
        _cg_framebuffer_get_projection_entry(framebuffer);
    cg_matrix_entry_get(projection_entry, matrix);
    _MATRIX_DEBUG_PRINT(matrix);
}

void
cg_framebuffer_set_projection_matrix(cg_framebuffer_t *framebuffer,
                                     const c_matrix_t *matrix)
{
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);

    cg_matrix_stack_set(projection_stack, matrix);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_PROJECTION;

    _MATRIX_DEBUG_PRINT(matrix);
}

void
cg_framebuffer_push_scissor_clip(
    cg_framebuffer_t *framebuffer, int x, int y, int width, int height)
{
    framebuffer->clip_stack = _cg_clip_stack_push_window_rectangle(
        framebuffer->clip_stack, x, y, width, height);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_CLIP;
}

void
cg_framebuffer_push_rectangle_clip(
    cg_framebuffer_t *framebuffer, float x_1, float y_1, float x_2, float y_2)
{
    cg_matrix_entry_t *modelview_entry =
        _cg_framebuffer_get_modelview_entry(framebuffer);
    cg_matrix_entry_t *projection_entry =
        _cg_framebuffer_get_projection_entry(framebuffer);
    /* XXX: It would be nicer if we stored the private viewport as a
     * vec4 so we could avoid this redundant copy. */
    float viewport[] = { framebuffer->viewport_x,
                         framebuffer->viewport_y,
                         framebuffer->viewport_width,
                         framebuffer->viewport_height };

    framebuffer->clip_stack =
        _cg_clip_stack_push_rectangle(framebuffer->clip_stack,
                                      x_1,
                                      y_1,
                                      x_2,
                                      y_2,
                                      modelview_entry,
                                      projection_entry,
                                      viewport);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_CLIP;
}

void
cg_framebuffer_push_primitive_clip(cg_framebuffer_t *framebuffer,
                                   cg_primitive_t *primitive,
                                   float bounds_x1,
                                   float bounds_y1,
                                   float bounds_x2,
                                   float bounds_y2)
{
    cg_matrix_entry_t *modelview_entry =
        _cg_framebuffer_get_modelview_entry(framebuffer);
    cg_matrix_entry_t *projection_entry =
        _cg_framebuffer_get_projection_entry(framebuffer);
    /* XXX: It would be nicer if we stored the private viewport as a
     * vec4 so we could avoid this redundant copy. */
    float viewport[] = { framebuffer->viewport_x,
                         framebuffer->viewport_y,
                         framebuffer->viewport_width,
                         framebuffer->viewport_height };

    framebuffer->clip_stack =
        _cg_clip_stack_push_primitive(framebuffer->clip_stack,
                                      primitive,
                                      bounds_x1,
                                      bounds_y1,
                                      bounds_x2,
                                      bounds_y2,
                                      modelview_entry,
                                      projection_entry,
                                      viewport);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_CLIP;
}

void
cg_framebuffer_pop_clip(cg_framebuffer_t *framebuffer)
{
    framebuffer->clip_stack = _cg_clip_stack_pop(framebuffer->clip_stack);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_CLIP;
}

void
_cg_framebuffer_unref(cg_framebuffer_t *framebuffer)
{
    /* Chain-up */
    _cg_object_default_unref(framebuffer);
}

#ifdef CG_ENABLE_DEBUG
static int
get_index(void *indices, cg_indices_type_t type, int _index)
{
    if (!indices)
        return _index;

    switch (type) {
    case CG_INDICES_TYPE_UNSIGNED_BYTE:
        return ((uint8_t *)indices)[_index];
    case CG_INDICES_TYPE_UNSIGNED_SHORT:
        return ((uint16_t *)indices)[_index];
    case CG_INDICES_TYPE_UNSIGNED_INT:
        return ((uint32_t *)indices)[_index];
    }

    c_return_val_if_reached(0);
}

static void
add_line(uint32_t *line_indices,
         int base,
         void *user_indices,
         cg_indices_type_t user_indices_type,
         int index0,
         int index1,
         int *pos)
{
    index0 = get_index(user_indices, user_indices_type, index0);
    index1 = get_index(user_indices, user_indices_type, index1);

    line_indices[(*pos)++] = base + index0;
    line_indices[(*pos)++] = base + index1;
}

static int
get_line_count(cg_vertices_mode_t mode, int n_vertices)
{
    if (mode == CG_VERTICES_MODE_TRIANGLES && (n_vertices % 3) == 0) {
        return n_vertices;
    } else if (mode == CG_VERTICES_MODE_TRIANGLE_FAN && n_vertices >= 3) {
        return 2 * n_vertices - 3;
    } else if (mode == CG_VERTICES_MODE_TRIANGLE_STRIP && n_vertices >= 3) {
        return 2 * n_vertices - 3;
    }

    c_return_val_if_reached(0);
}

static cg_indices_t *
get_wire_line_indices(cg_device_t *dev,
                      cg_vertices_mode_t mode,
                      int first_vertex,
                      int n_vertices_in,
                      cg_indices_t *user_indices,
                      int *n_indices)
{
    int n_lines;
    uint32_t *line_indices;
    cg_index_buffer_t *index_buffer;
    void *indices;
    cg_indices_type_t indices_type;
    int base = first_vertex;
    int pos;
    int i;
    cg_indices_t *ret;

    if (user_indices) {
        index_buffer = cg_indices_get_buffer(user_indices);
        indices = cg_buffer_map(
            CG_BUFFER(index_buffer), CG_BUFFER_ACCESS_READ, 0, NULL);
        indices_type = cg_indices_get_type(user_indices);
    } else {
        index_buffer = NULL;
        indices = NULL;
        indices_type = CG_INDICES_TYPE_UNSIGNED_BYTE;
    }

    n_lines = get_line_count(mode, n_vertices_in);

    /* Note: we are using CG_INDICES_TYPE_UNSIGNED_INT so 4 bytes per index. */
    line_indices = c_malloc(4 * n_lines * 2);

    pos = 0;

    if (mode == CG_VERTICES_MODE_TRIANGLES && (n_vertices_in % 3) == 0) {
        for (i = 0; i < n_vertices_in; i += 3) {
            add_line(line_indices, base, indices, indices_type, i, i + 1, &pos);
            add_line(
                line_indices, base, indices, indices_type, i + 1, i + 2, &pos);
            add_line(line_indices, base, indices, indices_type, i + 2, i, &pos);
        }
    } else if (mode == CG_VERTICES_MODE_TRIANGLE_FAN && n_vertices_in >= 3) {
        add_line(line_indices, base, indices, indices_type, 0, 1, &pos);
        add_line(line_indices, base, indices, indices_type, 1, 2, &pos);
        add_line(line_indices, base, indices, indices_type, 0, 2, &pos);

        for (i = 3; i < n_vertices_in; i++) {
            add_line(line_indices, base, indices, indices_type, i - 1, i, &pos);
            add_line(line_indices, base, indices, indices_type, 0, i, &pos);
        }
    } else if (mode == CG_VERTICES_MODE_TRIANGLE_STRIP && n_vertices_in >= 3) {
        add_line(line_indices, base, indices, indices_type, 0, 1, &pos);
        add_line(line_indices, base, indices, indices_type, 1, 2, &pos);
        add_line(line_indices, base, indices, indices_type, 0, 2, &pos);

        for (i = 3; i < n_vertices_in; i++) {
            add_line(line_indices, base, indices, indices_type, i - 1, i, &pos);
            add_line(line_indices, base, indices, indices_type, i - 2, i, &pos);
        }
    }

    if (user_indices)
        cg_buffer_unmap(CG_BUFFER(index_buffer));

    *n_indices = n_lines * 2;

    ret = cg_indices_new(dev, CG_INDICES_TYPE_UNSIGNED_INT, line_indices,
                         *n_indices);

    c_free(line_indices);

    return ret;
}

static bool
remove_layer_cb(cg_pipeline_t *pipeline, int layer_index, void *user_data)
{
    cg_pipeline_remove_layer(pipeline, layer_index);
    return true;
}

static void
draw_wireframe(cg_device_t *dev,
               cg_framebuffer_t *framebuffer,
               cg_pipeline_t *pipeline,
               cg_vertices_mode_t mode,
               int first_vertex,
               int n_vertices,
               cg_attribute_t **attributes,
               int n_attributes,
               cg_indices_t *indices,
               cg_draw_flags_t flags)
{
    cg_indices_t *wire_indices;
    cg_pipeline_t *wire_pipeline;
    int n_indices;

    wire_indices = get_wire_line_indices(dev, mode, first_vertex,
                                         n_vertices, indices, &n_indices);

    wire_pipeline = cg_pipeline_copy(pipeline);

    /* If we have glsl then the pipeline may have an associated
     * vertex program and since we'd like to see the results of the
     * vertex program in the wireframe we just add a final clobber
     * of the wire color leaving the rest of the state untouched. */
    if (cg_has_feature(framebuffer->dev, CG_FEATURE_ID_GLSL)) {
        static cg_snippet_t *snippet = NULL;

        /* The snippet is cached so that it will reuse the program
         * from the pipeline cache if possible */
        if (snippet == NULL) {
            snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT, NULL, NULL);
            cg_snippet_set_replace(snippet,
                                   "cg_color_out = "
                                   "vec4 (0.0, 1.0, 0.0, 1.0);\n");
        }

        cg_pipeline_add_snippet(wire_pipeline, snippet);
    } else {
        cg_pipeline_foreach_layer(wire_pipeline, remove_layer_cb, NULL);
        cg_pipeline_set_color4f(wire_pipeline, 0, 1, 0, 1);
    }

    /* temporarily disable the wireframe to avoid recursion! */
    flags |= CG_DRAW_SKIP_DEBUG_WIREFRAME;
    _cg_framebuffer_draw_indexed_attributes(framebuffer,
                                            wire_pipeline,
                                            CG_VERTICES_MODE_LINES,
                                            0,
                                            n_indices,
                                            wire_indices,
                                            attributes,
                                            n_attributes,
                                            1,
                                            flags);

    cg_object_unref(wire_pipeline);
    cg_object_unref(wire_indices);
}
#endif

/* Drawing with this api will bypass the framebuffer flush and
 * pipeline validation. */
void
_cg_framebuffer_draw_attributes(cg_framebuffer_t *framebuffer,
                                cg_pipeline_t *pipeline,
                                cg_vertices_mode_t mode,
                                int first_vertex,
                                int n_vertices,
                                cg_attribute_t **attributes,
                                int n_attributes,
                                int n_instances,
                                cg_draw_flags_t flags)
{
#ifdef CG_ENABLE_DEBUG
    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_WIREFRAME) &&
                   (flags & CG_DRAW_SKIP_DEBUG_WIREFRAME) == 0) &&
        mode != CG_VERTICES_MODE_LINES && mode != CG_VERTICES_MODE_LINE_LOOP &&
        mode != CG_VERTICES_MODE_LINE_STRIP)
        draw_wireframe(framebuffer->dev,
                       framebuffer,
                       pipeline,
                       mode,
                       first_vertex,
                       n_vertices,
                       attributes,
                       n_attributes,
                       NULL,
                       flags);
    else
#endif
    {
        cg_device_t *dev = framebuffer->dev;

        dev->driver_vtable->framebuffer_draw_attributes(framebuffer,
                                                        pipeline,
                                                        mode,
                                                        first_vertex,
                                                        n_vertices,
                                                        attributes,
                                                        n_attributes,
                                                        n_instances,
                                                        flags);
    }
}

void
_cg_framebuffer_draw_indexed_attributes(cg_framebuffer_t *framebuffer,
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
#ifdef CG_ENABLE_DEBUG
    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_WIREFRAME) &&
                   (flags & CG_DRAW_SKIP_DEBUG_WIREFRAME) == 0) &&
        mode != CG_VERTICES_MODE_LINES && mode != CG_VERTICES_MODE_LINE_LOOP &&
        mode != CG_VERTICES_MODE_LINE_STRIP)
        draw_wireframe(framebuffer->dev,
                       framebuffer,
                       pipeline,
                       mode,
                       first_vertex,
                       n_vertices,
                       attributes,
                       n_attributes,
                       indices,
                       flags);
    else
#endif
    {
        cg_device_t *dev = framebuffer->dev;

        dev->driver_vtable->framebuffer_draw_indexed_attributes(framebuffer,
                                                                pipeline,
                                                                mode,
                                                                first_vertex,
                                                                n_vertices,
                                                                indices,
                                                                attributes,
                                                                n_attributes,
                                                                n_instances,
                                                                flags);
    }
}

/* These rectangle drawing apis are horribly in-efficient but simple
 * and are only providing a stop-gap while we strip out the journal
 * from cg.
 */

void
_cg_rectangle_immediate(cg_framebuffer_t *framebuffer,
                        cg_pipeline_t *pipeline,
                        float x_1,
                        float y_1,
                        float x_2,
                        float y_2)
{
    cg_device_t *dev = framebuffer->dev;
    float vertices[8] = { x_1, y_1, x_1, y_2, x_2, y_1, x_2, y_2 };
    cg_attribute_buffer_t *attribute_buffer;
    cg_attribute_t *attributes[1];

    attribute_buffer = cg_attribute_buffer_new(dev, sizeof(vertices),
                                               vertices);
    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(float) * 2, /* stride */
                                     0, /* offset */
                                     2, /* n_components */
                                     CG_ATTRIBUTE_TYPE_FLOAT);

    _cg_framebuffer_draw_attributes(framebuffer,
                                    pipeline,
                                    CG_VERTICES_MODE_TRIANGLE_STRIP,
                                    0, /* first_index */
                                    4, /* n_vertices */
                                    attributes,
                                    1, /* n attributes */
                                    1, /* n instances */
                                    CG_DRAW_SKIP_FRAMEBUFFER_FLUSH);

    cg_object_unref(attributes[0]);
    cg_object_unref(attribute_buffer);
}

void
cg_framebuffer_draw_rectangle(cg_framebuffer_t *fb,
                              cg_pipeline_t *pipeline,
                              float x_1,
                              float y_1,
                              float x_2,
                              float y_2)
{
    cg_device_t *dev = fb->dev;
    float vertices[8] = { x_1, y_1, x_1, y_2, x_2, y_1, x_2, y_2 };
    cg_attribute_buffer_t *attribute_buffer;
    cg_attribute_t *attributes[1];

    if (cg_pipeline_get_n_layers(pipeline)) {
        cg_framebuffer_draw_textured_rectangle(fb, pipeline,
                                               x_1, y_1, x_2, y_2,
                                               0, 0, 1, 1);
        return;
    }

    attribute_buffer = cg_attribute_buffer_new(dev, sizeof(vertices),
                                               vertices);
    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(float) * 2, /* stride */
                                     0, /* offset */
                                     2, /* n_components */
                                     CG_ATTRIBUTE_TYPE_FLOAT);

    _cg_framebuffer_draw_attributes(fb,
                                    pipeline,
                                    CG_VERTICES_MODE_TRIANGLE_STRIP,
                                    0, /* first_index */
                                    4, /* n_vertices */
                                    attributes,
                                    1, /* n attributes */
                                    1, /* n instances */
                                    0); /* flags */

    cg_object_unref(attributes[0]);
    cg_object_unref(attribute_buffer);
}

static void
_cg_framebuffer_draw_textured_rectangle(cg_framebuffer_t *fb,
                                        cg_pipeline_t *pipeline,
                                        float x1,
                                        float y1,
                                        float x2,
                                        float y2,
                                        float tx1,
                                        float ty1,
                                        float tx2,
                                        float ty2)
{
    int n_layers = MAX(MIN(cg_pipeline_get_n_layers(pipeline), 8), 1);
    const char *tex_attrib_names[] = {
        "cg_tex_coord0_in",
        "cg_tex_coord1_in",
        "cg_tex_coord2_in",
        "cg_tex_coord3_in",
        "cg_tex_coord4_in",
        "cg_tex_coord5_in",
        "cg_tex_coord6_in",
        "cg_tex_coord7_in",
    };
    int vert_n_floats = 2 + 2 * n_layers;
    int rect_n_floats = vert_n_floats * 4;
    float *rect = c_alloca(rect_n_floats * sizeof(float));
    cg_attribute_buffer_t *attribute_buffer;
    int n_attributes = n_layers + 1;
    cg_attribute_t *attributes[n_attributes];
    float *vert;
    int i;

    vert = rect;
    vert[0] = x1;
    vert[1] = y1;
    vert[2] = tx1;
    vert[3] = ty1;

    vert += vert_n_floats;
    vert[0] = x1;
    vert[1] = y2;
    vert[2] = tx1;
    vert[3] = ty2;

    vert += vert_n_floats;
    vert[0] = x2;
    vert[1] = y2;
    vert[2] = tx2;
    vert[3] = ty2;

    vert += vert_n_floats;
    vert[0] = x2;
    vert[1] = y1;
    vert[2] = tx2;
    vert[3] = ty1;

    vert += vert_n_floats;
    vert[0] = tx1;
    vert[1] = ty1;
    vert[2] = tx2;
    vert[3] = ty2;

    for (i = 1; i < n_layers; i++) {
        vert = rect + 4;

        vert[0] = 0;
        vert[1] = 0;
        vert += vert_n_floats;
        vert[0] = 0;
        vert[1] = 1;
        vert += vert_n_floats;
        vert[0] = 1;
        vert[1] = 1;
        vert += vert_n_floats;
        vert[0] = 1;
        vert[1] = 0;
    }

    attribute_buffer = cg_attribute_buffer_new(fb->dev,
                                               rect_n_floats * sizeof(float),
                                               rect);

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     vert_n_floats * sizeof(float),
                                     0, /* offset */
                                     2, /* n components */
                                     CG_ATTRIBUTE_TYPE_FLOAT);

    for (i = 0; i < n_layers; i++) {
        attributes[i + 1] = cg_attribute_new(attribute_buffer,
                                             tex_attrib_names[i],
                                             vert_n_floats * sizeof(float),
                                             sizeof(float) * 2 + sizeof(float) * 2 * i,
                                             2, /* n components */
                                             CG_ATTRIBUTE_TYPE_FLOAT);
    }

    cg_object_unref(attribute_buffer);

    _cg_framebuffer_draw_attributes(fb,
                                    pipeline,
                                    CG_VERTICES_MODE_TRIANGLE_FAN,
                                    0, /* first_index */
                                    4, /* n_vertices */
                                    attributes,
                                    n_attributes,
                                    1, /* n instances */
                                    0); /* flags */

    for (i = 0; i < n_attributes; i++)
        cg_object_unref(attributes[i]);
}

struct foreach_state {
    cg_framebuffer_t *framebuffer;
    cg_pipeline_t *pipeline;
    float tex_virtual_origin_x;
    float tex_virtual_origin_y;
    float quad_origin_x;
    float quad_origin_y;
    float v_to_q_scale_x;
    float v_to_q_scale_y;
    float quad_len_x;
    float quad_len_y;
    bool flipped_x;
    bool flipped_y;
};

static void
draw_rectangle_region_cb(cg_texture_t *texture,
                         const float *subtexture_coords,
                         const float *virtual_coords,
                         void *user_data)
{
    struct foreach_state *state = user_data;
    cg_framebuffer_t *framebuffer = state->framebuffer;
    cg_pipeline_t *override_pipeline = cg_pipeline_copy(state->pipeline);
    float quad_coords[4];

#define TEX_VIRTUAL_TO_QUAD(V, Q, AXIS)                                        \
    do {                                                                       \
        Q = V - state->tex_virtual_origin_##AXIS;                              \
        Q *= state->v_to_q_scale_##AXIS;                                       \
        if (state->flipped_##AXIS)                                             \
            Q = state->quad_len_##AXIS - Q;                                    \
        Q += state->quad_origin_##AXIS;                                        \
    } while (0);

    TEX_VIRTUAL_TO_QUAD(virtual_coords[0], quad_coords[0], x);
    TEX_VIRTUAL_TO_QUAD(virtual_coords[1], quad_coords[1], y);

    TEX_VIRTUAL_TO_QUAD(virtual_coords[2], quad_coords[2], x);
    TEX_VIRTUAL_TO_QUAD(virtual_coords[3], quad_coords[3], y);

#undef TEX_VIRTUAL_TO_QUAD

    cg_pipeline_set_layer_texture(override_pipeline, 0, texture);

    _cg_framebuffer_draw_textured_rectangle(framebuffer,
                                            override_pipeline,
                                            quad_coords[0],
                                            quad_coords[1],
                                            quad_coords[2],
                                            quad_coords[3],
                                            subtexture_coords[0],
                                            subtexture_coords[1],
                                            subtexture_coords[2],
                                            subtexture_coords[3]);

    cg_object_unref(override_pipeline);
}

static bool
update_layer_storage_cb(cg_pipeline_t *pipeline,
                        int layer_index,
                        void *user_data)
{
    _cg_pipeline_pre_paint_for_layer(pipeline, layer_index);
    return true; /* continue */
}

/* XXX: this one is a bit of faff because users expect to draw with
 * more than one layer and assume the additional layers will have
 * default texture coordinates of (0,0) (1,1) and this api also needs
 * to work with highlevel meta textures such as sliced textures or
 * sub-textures */
void
cg_framebuffer_draw_textured_rectangle(cg_framebuffer_t *fb,
                                       cg_pipeline_t *pipeline,
                                       float x1,
                                       float y1,
                                       float x2,
                                       float y2,
                                       float tx_1,
                                       float ty_1,
                                       float tx_2,
                                       float ty_2)
{
    cg_texture_t *tex0;

    /* Treat layer 0 specially and allow it to reference a highlevel,
     * meta texture (such as a sliced texture or sub-texture from a
     * texture atlas) */
    tex0 = cg_pipeline_get_layer_texture(pipeline, 0);
    if (tex0 && !cg_is_primitive_texture(tex0)) {
        cg_pipeline_t *override_pipeline = NULL;
        cg_pipeline_wrap_mode_t clamp_to_edge = CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
        cg_pipeline_wrap_mode_t wrap_s;
        cg_pipeline_wrap_mode_t wrap_t;
        struct foreach_state state;
        bool tex_virtual_flipped_x;
        bool tex_virtual_flipped_y;
        bool quad_flipped_x;
        bool quad_flipped_y;

        /* We don't support multi-texturing with meta textures. */
        c_return_if_fail(cg_pipeline_get_n_layers(pipeline) == 1);

        /* Before we can map the user's texture coordinates to
         * primitive texture coordinates we need to give the meta
         * texture an opportunity to update its internal storage based
         * on the pipeline we're going to use.
         *
         * For example if the pipeline requires a valid mipmap then
         * the atlas-texture backend actually migrates the texture
         * out of the atlas, instead of updating the mipmap for
         * the whole atlas and because it avoids the need for
         * significant padding between textures within an atlas to
         * avoid bleeding as the layers are scaled down.
         */
        cg_pipeline_foreach_layer(pipeline, update_layer_storage_cb, NULL);

        /* We can't use hardware repeat so we need to set clamp to edge
         * otherwise it might pull in junk pixels.
         * cg_meta_texture_foreach_in_region will emulate the original
         * repeat mode in software. */
        wrap_s = cg_pipeline_get_layer_wrap_mode_s(pipeline, 0);
        if (wrap_s != CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE) {
            override_pipeline = cg_pipeline_copy(pipeline);
            cg_pipeline_set_layer_wrap_mode_s(override_pipeline, 0, clamp_to_edge);
        }

        wrap_t = cg_pipeline_get_layer_wrap_mode_t(pipeline, 0);
        if (wrap_t != CG_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE) {
            if (!override_pipeline)
                override_pipeline = cg_pipeline_copy(pipeline);
            cg_pipeline_set_layer_wrap_mode_t(override_pipeline, 0, clamp_to_edge);
        }

        state.framebuffer = fb;
        state.pipeline = override_pipeline ? override_pipeline : pipeline;

        /* Get together the data we need to transform the virtual
         * texture coordinates of each slice into quad coordinates...
         *
         * NB: We need to consider that the quad coordinates and the
         * texture coordinates may be inverted along the x or y axis,
         * and must preserve the inversions when we emit the final
         * geometry.
         */

        tex_virtual_flipped_x = (tx_1 > tx_2) ? true : false;
        tex_virtual_flipped_y = (ty_1 > ty_2) ? true : false;
        state.tex_virtual_origin_x = tex_virtual_flipped_x ? tx_2 : tx_1;
        state.tex_virtual_origin_y = tex_virtual_flipped_y ? ty_2 : ty_1;

        quad_flipped_x = (x1 > x2) ? true : false;
        quad_flipped_y = (y1 > y2) ? true : false;
        state.quad_origin_x = quad_flipped_x ? x2 : x1;
        state.quad_origin_y = quad_flipped_y ? y2 : y1;

        /* flatten the two forms of coordinate inversion into one... */
        state.flipped_x = tex_virtual_flipped_x ^ quad_flipped_x;
        state.flipped_y = tex_virtual_flipped_y ^ quad_flipped_y;

        /* We use the _len_AXIS naming here instead of _width and
         * _height because draw_rectangle_region_cb uses a macro with
         * symbol concatenation to handle both axis, so this is more
         * convenient... */
        state.quad_len_x = fabsf(x2 - x1);
        state.quad_len_y = fabsf(y2 - y1);

        state.v_to_q_scale_x = fabsf(state.quad_len_x / (tx_2 - tx_1));
        state.v_to_q_scale_y = fabsf(state.quad_len_y / (ty_2 - ty_1));

        cg_meta_texture_foreach_in_region((cg_meta_texture_t *)tex0,
                                          tx_1,
                                          ty_1,
                                          tx_2,
                                          ty_2,
                                          wrap_s,
                                          wrap_t,
                                          draw_rectangle_region_cb,
                                          &state);

        if (override_pipeline)
            cg_object_unref(override_pipeline);

        return;
    }

    _cg_framebuffer_draw_textured_rectangle(fb,
                                            pipeline,
                                            x1, y1, x2, y2,
                                            tx_1, ty_1, tx_2, ty_2);
}


void
cg_framebuffer_draw_rectangles(cg_framebuffer_t *framebuffer,
                               cg_pipeline_t *pipeline,
                               const float *coordinates,
                               unsigned int n_rectangles)
{
    cg_vertex_p2_t *verts = c_alloca(n_rectangles * sizeof(cg_vertex_p2_t) * 4);

    c_warn_if_fail(cg_pipeline_get_n_layers(pipeline) == 0);

    for (unsigned int i = 0; i < n_rectangles; i++) {
        const float *pos = &coordinates[i * 4];
        cg_vertex_p2_t *rect = verts + 4 * i;

        rect[0].x = pos[0]; /* x1 */
        rect[0].y = pos[1]; /* y1 */

        rect[1].x = pos[0]; /* x1 */
        rect[1].y = pos[3]; /* y2 */

        rect[2].x = pos[2]; /* x2 */
        rect[2].y = pos[3]; /* y2 */

        rect[3].x = pos[2]; /* x2 */
        rect[3].y = pos[1]; /* y1 */
    }

#warning "FIXME: cg_framebuffer_draw_rectangles shouldn't need to create a cg_primitive_t"
    cg_primitive_t *prim = cg_primitive_new_p2(framebuffer->dev,
                                               CG_VERTICES_MODE_TRIANGLES,
                                               4 * n_rectangles,
                                               verts);
    cg_primitive_set_indices(prim,
                             cg_get_rectangle_indices(framebuffer->dev,
                                                      n_rectangles),
                             n_rectangles * 6);
    cg_primitive_draw(prim, framebuffer, pipeline);
    cg_object_unref(prim);
}

void
cg_framebuffer_draw_textured_rectangles(cg_framebuffer_t *framebuffer,
                                        cg_pipeline_t *pipeline,
                                        const float *coordinates,
                                        unsigned int n_rectangles)
{
    cg_vertex_p2t2_t *verts = c_alloca(n_rectangles * sizeof(cg_vertex_p2t2_t) * 4);

    c_warn_if_fail(cg_pipeline_get_n_layers(pipeline) == 1);

    for (unsigned int i = 0; i < n_rectangles; i++) {
        const float *pos = &coordinates[i * 8];
        const float *tex_coords = &coordinates[i * 8 + 4];
        cg_vertex_p2t2_t *rect = verts + 4 * i;

        rect[0].x = pos[0];
        rect[0].y = pos[1];
        rect[0].s = tex_coords[0];
        rect[0].t = tex_coords[1];

        rect[1].x = pos[0];
        rect[1].y = pos[3];
        rect[1].s = tex_coords[0];
        rect[1].t = tex_coords[3];

        rect[2].x = pos[2];
        rect[2].y = pos[3];
        rect[2].s = tex_coords[2];
        rect[2].t = tex_coords[3];

        rect[3].x = pos[2];
        rect[3].y = pos[1];
        rect[3].s = tex_coords[2];
        rect[3].t = tex_coords[1];
    }

#warning "FIXME: cg_framebuffer_draw_textured_rectangles shouldn't need to create a cg_primitive_t"
    cg_primitive_t *prim = cg_primitive_new_p2t2(framebuffer->dev,
                                                 CG_VERTICES_MODE_TRIANGLES,
                                                 4 * n_rectangles,
                                                 verts);
    cg_primitive_set_indices(prim,
                             cg_get_rectangle_indices(framebuffer->dev,
                                                      n_rectangles),
                             n_rectangles * 6);
    cg_primitive_draw(prim, framebuffer, pipeline);
    cg_object_unref(prim);
}

cg_device_t *
cg_framebuffer_get_device(cg_framebuffer_t *fb)
{
    return fb->dev;
}
