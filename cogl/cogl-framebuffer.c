/*
 * Cogl
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-debug.h"
#include "cogl-device-private.h"
#include "cogl-display-private.h"
#include "cogl-renderer-private.h"
#include "cogl-object-private.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-clip-stack.h"
#include "cogl-journal-private.h"
#include "cogl-winsys-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-matrix-private.h"
#include "cogl-primitive-private.h"
#include "cogl-offscreen.h"
#include "cogl-private.h"
#include "cogl-primitives-private.h"
#include "cogl-error-private.h"
#include "cogl-texture-gl-private.h"

extern cg_object_class_t _cg_onscreen_class;

#ifdef CG_ENABLE_DEBUG
static cg_user_data_key_t wire_pipeline_key;
#endif

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
    return c_quark_from_static_string("cogl-framebuffer-error-quark");
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

    framebuffer->journal = _cg_journal_new(framebuffer);

    /* Ensure we know the framebuffer->clear_color* members can't be
     * referenced for our fast-path read-pixel optimization (see
     * _cg_journal_try_read_pixel()) until some region of the
     * framebuffer is initialized.
     */
    framebuffer->clear_clip_dirty = true;

    /* XXX: We have to maintain a central list of all framebuffers
     * because at times we need to be able to flush all known journals.
     *
     * Examples where we need to flush all journals are:
     * - because journal entries can reference OpenGL texture
     *   coordinates that may not survive texture-atlas reorganization
     *   so we need the ability to flush those entries.
     * - because although we generally advise against modifying
     *   pipelines after construction we have to handle that possibility
     *   and since pipelines may be referenced in journal entries we
     *   need to be able to flush them before allowing the pipelines to
     *   be changed.
     *
     * Note we don't maintain a list of journals and associate
     * framebuffers with journals by e.g. having a journal->framebuffer
     * reference since that would introduce a circular reference.
     *
     * Note: As a future change to try and remove the need to index all
     * journals it might be possible to defer resolving of OpenGL
     * texture coordinates for rectangle primitives until we come to
     * flush a journal. This would mean for instance that a single
     * rectangle entry in a journal could later be expanded into
     * multiple quad primitives to handle sliced textures but would mean
     * we don't have to worry about retaining references to OpenGL
     * texture coordinates that may later become invalid.
     */
    dev->framebuffers = c_list_prepend(dev->framebuffers, framebuffer);
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

    cg_object_unref(framebuffer->journal);

    if (dev->viewport_scissor_workaround_framebuffer == framebuffer)
        dev->viewport_scissor_workaround_framebuffer = NULL;

    dev->framebuffers = c_list_remove(dev->framebuffers, framebuffer);

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

/* This version of cg_clear can be used internally as an alternative
 * to avoid flushing the journal or the framebuffer state. This is
 * needed when doing operations that may be called whiling flushing
 * the journal */
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
_cg_framebuffer_mark_clear_clip_dirty(cg_framebuffer_t *framebuffer)
{
    framebuffer->clear_clip_dirty = true;
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
    cg_clip_stack_t *clip_stack = _cg_framebuffer_get_clip_stack(framebuffer);
    int scissor_x0;
    int scissor_y0;
    int scissor_x1;
    int scissor_y1;

    _cg_clip_stack_get_bounds(
        clip_stack, &scissor_x0, &scissor_y0, &scissor_x1, &scissor_y1);

    /* NB: the previous clear could have had an arbitrary clip.
     * NB: everything for the last frame might still be in the journal
     *     but we can't assume anything about how each entry was
     *     clipped.
     * NB: Clutter will scissor its pick renders which would mean all
     *     journal entries have a common ClipStack entry, but without
     *     a layering violation Cogl has to explicitly walk the journal
     *     entries to determine if this is the case.
     * NB: We have a software only read-pixel optimization in the
     *     journal that determines the color at a given framebuffer
     *     coordinate for simple scenes without rendering with the GPU.
     *     When Clutter is hitting this fast-path we can expect to
     *     receive calls to clear the framebuffer with an un-flushed
     *     journal.
     * NB: To fully support software based picking for Clutter we
     *     need to be able to reliably detect when the contents of a
     *     journal can be discarded and when we can skip the call to
     *     glClear because it matches the previous clear request.
     */

    /* Note: we don't check for the stencil buffer being cleared here
     * since there isn't any public cogl api to manipulate the stencil
     * buffer.
     *
     * Note: we check for an exact clip match here because
     * 1) a smaller clip could mean existing journal entries may
     *    need to contribute to regions outside the new clear-clip
     * 2) a larger clip would mean we need to issue a real
     *    glClear and we only care about cases avoiding a
     *    glClear.
     *
     * Note: Comparing without an epsilon is considered
     * appropriate here.
     */
    if (buffers & CG_BUFFER_BIT_COLOR && buffers & CG_BUFFER_BIT_DEPTH &&
        !framebuffer->clear_clip_dirty && framebuffer->clear_color_red == red &&
        framebuffer->clear_color_green == green &&
        framebuffer->clear_color_blue == blue &&
        framebuffer->clear_color_alpha == alpha &&
        scissor_x0 == framebuffer->clear_clip_x0 &&
        scissor_y0 == framebuffer->clear_clip_y0 &&
        scissor_x1 == framebuffer->clear_clip_x1 &&
        scissor_y1 == framebuffer->clear_clip_y1) {
        /* NB: We only have to consider the clip state of journal
         * entries if the current clear is clipped since otherwise we
         * know every pixel of the framebuffer is affected by the clear
         * and so all journal entries become redundant and can simply be
         * discarded.
         */
        if (clip_stack) {
            /*
             * Note: the function for checking the journal entries is
             * quite strict. It avoids detailed checking of all entry
             * clip_stacks by only checking the details of the first
             * entry and then it only verifies that the remaining
             * entries share the same clip_stack ancestry. This means
             * it's possible for some false negatives here but that will
             * just result in us falling back to a real clear.
             */
            if (_cg_journal_all_entries_within_bounds(framebuffer->journal,
                                                      scissor_x0,
                                                      scissor_y0,
                                                      scissor_x1,
                                                      scissor_y1)) {
                _cg_journal_discard(framebuffer->journal);
                goto cleared;
            }
        } else {
            _cg_journal_discard(framebuffer->journal);
            goto cleared;
        }
    }

    CG_NOTE(DRAW, "Clear begin");

    _cg_framebuffer_flush_journal(framebuffer);

    /* NB: _cg_framebuffer_flush_state may disrupt various state (such
     * as the pipeline state) when flushing the clip stack, so should
     * always be done first when preparing to draw. */
    _cg_framebuffer_flush_state(
        framebuffer, framebuffer, CG_FRAMEBUFFER_STATE_ALL);

    _cg_framebuffer_clear_without_flush4f(
        framebuffer, buffers, red, green, blue, alpha);

    /* This is a debugging variable used to visually display the quad
     * batches from the journal. It is reset here to increase the
     * chances of getting the same colours for each frame during an
     * animation */
    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_RECTANGLES)) &&
        buffers & CG_BUFFER_BIT_COLOR) {
        framebuffer->dev->journal_rectangles_color = 1;
    }

    CG_NOTE(DRAW, "Clear end");

cleared:

    _cg_framebuffer_mark_mid_scene(framebuffer);
    _cg_framebuffer_mark_clear_clip_dirty(framebuffer);

    if (buffers & CG_BUFFER_BIT_COLOR && buffers & CG_BUFFER_BIT_DEPTH) {
        /* For our fast-path for reading back a single pixel of simple
         * scenes where the whole frame is in the journal we need to
         * track the cleared color of the framebuffer in case the point
         * read doesn't intersect any of the journal rectangles. */
        framebuffer->clear_clip_dirty = false;
        framebuffer->clear_color_red = red;
        framebuffer->clear_color_green = green;
        framebuffer->clear_color_blue = blue;
        framebuffer->clear_color_alpha = alpha;

        /* NB: A clear may be scissored so we need to track the extents
         * that the clear is applicable too... */
        if (clip_stack) {
            _cg_clip_stack_get_bounds(clip_stack,
                                      &framebuffer->clear_clip_x0,
                                      &framebuffer->clear_clip_y0,
                                      &framebuffer->clear_clip_x1,
                                      &framebuffer->clear_clip_y1);
        } else {
            /* FIXME: set degenerate clip */
        }
    }
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

    _cg_framebuffer_flush_journal(framebuffer);

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
    c_list_t *l;

    for (l = framebuffer->deps; l; l = l->next) {
        cg_framebuffer_t *existing_dep = l->data;
        if (existing_dep == dependency)
            return;
    }

    /* TODO: generalize the primed-array type structure we e.g. use for
     * cg_object_set_user_data or for pipeline children as a way to
     * avoid quite a lot of mid-scene micro allocations here... */
    framebuffer->deps =
        c_list_prepend(framebuffer->deps, cg_object_ref(dependency));
}

void
_cg_framebuffer_remove_all_dependencies(cg_framebuffer_t *framebuffer)
{
    c_list_t *l;
    for (l = framebuffer->deps; l; l = l->next)
        cg_object_unref(l->data);
    c_list_free(framebuffer->deps);
    framebuffer->deps = NULL;
}

void
_cg_framebuffer_flush_journal(cg_framebuffer_t *framebuffer)
{
    _cg_journal_flush(framebuffer->journal);
}

void
_cg_framebuffer_flush(cg_framebuffer_t *framebuffer)
{
    _cg_framebuffer_flush_journal(framebuffer);
}

void
_cg_framebuffer_flush_dependency_journals(cg_framebuffer_t *framebuffer)
{
    c_list_t *l;
    for (l = framebuffer->deps; l; l = l->next)
        _cg_framebuffer_flush_journal(l->data);
    _cg_framebuffer_remove_all_dependencies(framebuffer);
}

cg_offscreen_t *
_cg_offscreen_new_with_texture_full(
    cg_texture_t *texture, cg_offscreen_flags_t create_flags, int level)
{
    cg_device_t *dev = texture->dev;
    cg_offscreen_t *offscreen;
    cg_framebuffer_t *fb;
    cg_offscreen_t *ret;

    c_return_val_if_fail(cg_is_texture(texture), NULL);

    offscreen = c_new0(cg_offscreen_t, 1);
    offscreen->texture = cg_object_ref(texture);
    offscreen->texture_level = level;
    offscreen->create_flags = create_flags;

    fb = CG_FRAMEBUFFER(offscreen);

    /* NB: we can't assume we can query the texture's width yet, since
     * it might not have been allocated yet and for example if the
     * texture is being loaded from a file then the file might not
     * have been read yet. */

    _cg_framebuffer_init(fb,
                         dev,
                         CG_FRAMEBUFFER_TYPE_OFFSCREEN,
                         -1, /* unknown width, until allocation */
                         -1); /* unknown height until allocation */

    ret = _cg_offscreen_object_new(offscreen);

    _cg_texture_associate_framebuffer(texture, fb);

    return ret;
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

bool
cg_framebuffer_allocate(cg_framebuffer_t *framebuffer, cg_error_t **error)
{
    cg_onscreen_t *onscreen = CG_ONSCREEN(framebuffer);
    const cg_winsys_vtable_t *winsys = _cg_framebuffer_get_winsys(framebuffer);
    cg_device_t *dev = framebuffer->dev;

    if (framebuffer->allocated)
        return true;

    if (framebuffer->type == CG_FRAMEBUFFER_TYPE_ONSCREEN) {
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
        cg_offscreen_t *offscreen = CG_OFFSCREEN(framebuffer);

        if (!cg_has_feature(dev, CG_FEATURE_ID_OFFSCREEN)) {
            _cg_set_error(error,
                          CG_SYSTEM_ERROR,
                          CG_SYSTEM_ERROR_UNSUPPORTED,
                          "Offscreen framebuffers not supported by system");
            return false;
        }

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
        framebuffer->width = cg_texture_get_width(offscreen->texture);
        framebuffer->height = cg_texture_get_height(offscreen->texture);
        framebuffer->viewport_width = framebuffer->width;
        framebuffer->viewport_height = framebuffer->height;

        /* Forward the texture format as the internal format of the
         * framebuffer */
        framebuffer->internal_format =
            _cg_texture_get_format(offscreen->texture);

        if (!dev->driver_vtable->offscreen_allocate(offscreen, error))
            return false;
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

    /* XXX: Currently color mask changes don't go through the journal */
    _cg_framebuffer_flush_journal(framebuffer);

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

    /* XXX: Currently depth write changes don't go through the journal */
    _cg_framebuffer_flush_journal(framebuffer);

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

    _cg_framebuffer_flush_journal(framebuffer);

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

static bool
_cg_framebuffer_try_fast_read_pixel(cg_framebuffer_t *framebuffer,
                                    int x,
                                    int y,
                                    cg_read_pixels_flags_t source,
                                    cg_bitmap_t *bitmap)
{
    bool found_intersection;
    cg_pixel_format_t format;

    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_FAST_READ_PIXEL)))
        return false;

    if (source != CG_READ_PIXELS_COLOR_BUFFER)
        return false;

    format = cg_bitmap_get_format(bitmap);

    if (format != CG_PIXEL_FORMAT_RGBA_8888_PRE &&
        format != CG_PIXEL_FORMAT_RGBA_8888)
        return false;

    if (!_cg_journal_try_read_pixel(
            framebuffer->journal, x, y, bitmap, &found_intersection))
        return false;

    /* If we can't determine the color from the primitives in the
     * journal then see if we can use the last recorded clear color
     */

    /* If _cg_journal_try_read_pixel() failed even though there was an
     * intersection of the given point with a primitive in the journal
     * then we can't fallback to the framebuffer's last clear color...
     * */
    if (found_intersection)
        return true;

    /* If the framebuffer has been rendered too since it was last
    * cleared then we can't return the last known clear color. */
    if (framebuffer->clear_clip_dirty)
        return false;

    if (x >= framebuffer->clear_clip_x0 && x < framebuffer->clear_clip_x1 &&
        y >= framebuffer->clear_clip_y0 && y < framebuffer->clear_clip_y1) {
        uint8_t *pixel;
        cg_error_t *ignore_error = NULL;

        /* we currently only care about cases where the premultiplied or
         * unpremultipled colors are equivalent... */
        if (framebuffer->clear_color_alpha != 1.0)
            return false;

        pixel = _cg_bitmap_map(bitmap,
                               CG_BUFFER_ACCESS_WRITE,
                               CG_BUFFER_MAP_HINT_DISCARD,
                               &ignore_error);
        if (pixel == NULL) {
            cg_error_free(ignore_error);
            return false;
        }

        pixel[0] = framebuffer->clear_color_red * 255.0;
        pixel[1] = framebuffer->clear_color_green * 255.0;
        pixel[2] = framebuffer->clear_color_blue * 255.0;
        pixel[3] = framebuffer->clear_color_alpha * 255.0;

        _cg_bitmap_unmap(bitmap);

        return true;
    }

    return false;
}

bool
cg_framebuffer_read_pixels_into_bitmap(cg_framebuffer_t *framebuffer,
                                       int x,
                                       int y,
                                       cg_read_pixels_flags_t source,
                                       cg_bitmap_t *bitmap,
                                       cg_error_t **error)
{
    cg_device_t *dev;
    int width;
    int height;

    c_return_val_if_fail(source & CG_READ_PIXELS_COLOR_BUFFER, false);
    c_return_val_if_fail(cg_is_framebuffer(framebuffer), false);

    if (!cg_framebuffer_allocate(framebuffer, error))
        return false;

    width = cg_bitmap_get_width(bitmap);
    height = cg_bitmap_get_height(bitmap);

    if (width == 1 && height == 1 && !framebuffer->clear_clip_dirty) {
        /* If everything drawn so far for this frame is still in the
         * Journal then if all of the rectangles only have a flat
         * opaque color we have a fast-path for reading a single pixel
         * that avoids the relatively high cost of flushing primitives
         * to be drawn on the GPU (considering how simple the geometry
         * is in this case) and then blocking on the long GPU pipelines
         * for the result.
         */
        if (_cg_framebuffer_try_fast_read_pixel(
                framebuffer, x, y, source, bitmap))
            return true;
    }

    dev = cg_framebuffer_get_context(framebuffer);

    /* make sure any batched primitives get emitted to the driver
     * before issuing our read pixels...
     */
    _cg_framebuffer_flush_journal(framebuffer);

    return dev->driver_vtable->framebuffer_read_pixels_into_bitmap(
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
       by the scissor and we want to hide this feature for the Cogl API
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

    _cg_framebuffer_flush_journal(framebuffer);

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
                                 const cg_quaternion_t *quaternion)
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
                            const cg_euler_t *euler)
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
                         const cg_matrix_t *matrix)
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
    float ymax = z_near * tanf(fov_y * G_PI / 360.0);

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

    /* XXX: The projection matrix isn't currently tracked in the journal
     * so we need to flush all journaled primitives first... */
    _cg_framebuffer_flush_journal(framebuffer);

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
    cg_matrix_t ortho;
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);

    /* XXX: The projection matrix isn't currently tracked in the journal
     * so we need to flush all journaled primitives first... */
    _cg_framebuffer_flush_journal(framebuffer);

    cg_matrix_init_identity(&ortho);
    cg_matrix_orthographic(&ortho, x_1, y_1, x_2, y_2, near, far);
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
                                    cg_matrix_t *matrix)
{
    cg_matrix_entry_t *modelview_entry =
        _cg_framebuffer_get_modelview_entry(framebuffer);
    cg_matrix_entry_get(modelview_entry, matrix);
    _CG_MATRIX_DEBUG_PRINT(matrix);
}

void
cg_framebuffer_set_modelview_matrix(cg_framebuffer_t *framebuffer,
                                    const cg_matrix_t *matrix)
{
    cg_matrix_stack_t *modelview_stack =
        _cg_framebuffer_get_modelview_stack(framebuffer);
    cg_matrix_stack_set(modelview_stack, matrix);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_MODELVIEW;

    _CG_MATRIX_DEBUG_PRINT(matrix);
}

void
cg_framebuffer_get_projection_matrix(cg_framebuffer_t *framebuffer,
                                     cg_matrix_t *matrix)
{
    cg_matrix_entry_t *projection_entry =
        _cg_framebuffer_get_projection_entry(framebuffer);
    cg_matrix_entry_get(projection_entry, matrix);
    _CG_MATRIX_DEBUG_PRINT(matrix);
}

void
cg_framebuffer_set_projection_matrix(cg_framebuffer_t *framebuffer,
                                     const cg_matrix_t *matrix)
{
    cg_matrix_stack_t *projection_stack =
        _cg_framebuffer_get_projection_stack(framebuffer);

    /* XXX: The projection matrix isn't currently tracked in the journal
     * so we need to flush all journaled primitives first... */
    _cg_framebuffer_flush_journal(framebuffer);

    cg_matrix_stack_set(projection_stack, matrix);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_PROJECTION;

    _CG_MATRIX_DEBUG_PRINT(matrix);
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
    /* The journal holds a reference to the framebuffer whenever it is
       non-empty. Therefore if the journal is non-empty and we will have
       exactly one reference then we know the journal is the only thing
       keeping the framebuffer alive. In that case we want to flush the
       journal and let the framebuffer die. It is fine at this point if
       flushing the journal causes something else to take a reference to
       it and it comes back to life */
    if (framebuffer->journal->entries->len > 0) {
        unsigned int ref_count = ((cg_object_t *)framebuffer)->ref_count;

        /* There should be at least two references - the one we are
           about to drop and the one held by the journal */
        if (ref_count < 2)
            c_warning("Inconsistent ref count on a framebuffer with journal "
                      "entries.");

        if (ref_count == 2)
            _cg_framebuffer_flush_journal(framebuffer);
    }

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
/* In the journal we are a bit sneaky and actually use GL_QUADS
 * which isn't actually a valid cg_vertices_mode_t! */
#ifdef HAVE_CG_GL
    else if (mode == GL_QUADS && (n_vertices % 4) == 0) {
        return n_vertices;
    }
#endif

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
/* In the journal we are a bit sneaky and actually use GL_QUADS
 * which isn't actually a valid cg_vertices_mode_t! */
#ifdef HAVE_CG_GL
    else if (mode == GL_QUADS && (n_vertices_in % 4) == 0) {
        for (i = 0; i < n_vertices_in; i += 4) {
            add_line(line_indices, base, indices, indices_type, i, i + 1, &pos);
            add_line(
                line_indices, base, indices, indices_type, i + 1, i + 2, &pos);
            add_line(
                line_indices, base, indices, indices_type, i + 2, i + 3, &pos);
            add_line(line_indices, base, indices, indices_type, i + 3, i, &pos);
        }
    }
#endif

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
pipeline_destroyed_cb(cg_pipeline_t *weak_pipeline, void *user_data)
{
    cg_pipeline_t *original_pipeline = user_data;

    /* XXX: I think we probably need to provide a custom unref function for
     * cg_pipeline_t because it's possible that we will reach this callback
     * because original_pipeline is being freed which means cg_object_unref
     * will have already freed any associated user data.
     *
     * Setting more user data here will *probably* succeed but that may allocate
     * a new user-data array which could be leaked.
     *
     * Potentially we could have a _cg_object_free_user_data function so
     * that a custom unref function could be written that can destroy weak
     * pipeline children before removing user data.
     */
    cg_object_set_user_data(
        CG_OBJECT(original_pipeline), &wire_pipeline_key, NULL, NULL);

    cg_object_unref(weak_pipeline);
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

    wire_pipeline =
        cg_object_get_user_data(CG_OBJECT(pipeline), &wire_pipeline_key);

    if (!wire_pipeline) {
        wire_pipeline =
            _cg_pipeline_weak_copy(pipeline, pipeline_destroyed_cb, NULL);

        cg_object_set_user_data(
            CG_OBJECT(pipeline), &wire_pipeline_key, wire_pipeline, NULL);

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
                                            flags);

    cg_object_unref(wire_indices);
}
#endif

/* This can be called directly by the cg_journal_t to draw attributes
 * skipping the implicit journal flush, the framebuffer flush and
 * pipeline validation. */
void
_cg_framebuffer_draw_attributes(cg_framebuffer_t *framebuffer,
                                cg_pipeline_t *pipeline,
                                cg_vertices_mode_t mode,
                                int first_vertex,
                                int n_vertices,
                                cg_attribute_t **attributes,
                                int n_attributes,
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
                                                                flags);
    }
}

void
cg_framebuffer_draw_rectangle(cg_framebuffer_t *framebuffer,
                              cg_pipeline_t *pipeline,
                              float x_1,
                              float y_1,
                              float x_2,
                              float y_2)
{
    const float position[4] = { x_1, y_1, x_2, y_2 };
    cg_multi_textured_rect_t rect;

    /* XXX: All the _*_rectangle* APIs normalize their input into an array of
     * _cg_multi_textured_rect_t rectangles and pass these on to our work horse;
     * _cg_framebuffer_draw_multitextured_rectangles.
     */

    rect.position = position;
    rect.tex_coords = NULL;
    rect.tex_coords_len = 0;

    _cg_framebuffer_draw_multitextured_rectangles(
        framebuffer, pipeline, &rect, 1);
}

void
cg_framebuffer_draw_textured_rectangle(cg_framebuffer_t *framebuffer,
                                       cg_pipeline_t *pipeline,
                                       float x_1,
                                       float y_1,
                                       float x_2,
                                       float y_2,
                                       float s_1,
                                       float t_1,
                                       float s_2,
                                       float t_2)
{
    const float position[4] = { x_1, y_1, x_2, y_2 };
    const float tex_coords[4] = { s_1, t_1, s_2, t_2 };
    cg_multi_textured_rect_t rect;

    /* XXX: All the _*_rectangle* APIs normalize their input into an array of
     * cg_multi_textured_rect_t rectangles and pass these on to our work horse;
     * _cg_framebuffer_draw_multitextured_rectangles.
     */

    rect.position = position;
    rect.tex_coords = tex_coords;
    rect.tex_coords_len = 4;

    _cg_framebuffer_draw_multitextured_rectangles(
        framebuffer, pipeline, &rect, 1);
}

void
cg_framebuffer_draw_multitextured_rectangle(cg_framebuffer_t *framebuffer,
                                            cg_pipeline_t *pipeline,
                                            float x_1,
                                            float y_1,
                                            float x_2,
                                            float y_2,
                                            const float *tex_coords,
                                            int tex_coords_len)
{
    const float position[4] = { x_1, y_1, x_2, y_2 };
    cg_multi_textured_rect_t rect;

    /* XXX: All the _*_rectangle* APIs normalize their input into an array of
     * cg_multi_textured_rect_t rectangles and pass these on to our work horse;
     * _cg_framebuffer_draw_multitextured_rectangles.
     */

    rect.position = position;
    rect.tex_coords = tex_coords;
    rect.tex_coords_len = tex_coords_len;

    _cg_framebuffer_draw_multitextured_rectangles(
        framebuffer, pipeline, &rect, 1);
}

void
cg_framebuffer_draw_rectangles(cg_framebuffer_t *framebuffer,
                               cg_pipeline_t *pipeline,
                               const float *coordinates,
                               unsigned int n_rectangles)
{
    cg_multi_textured_rect_t *rects;
    int i;

    /* XXX: All the _*_rectangle* APIs normalize their input into an array of
     * cg_multi_textured_rect_t rectangles and pass these on to our work horse;
     * _cg_framebuffer_draw_multitextured_rectangles.
     */

    rects = c_alloca(n_rectangles * sizeof(cg_multi_textured_rect_t));

    for (i = 0; i < n_rectangles; i++) {
        rects[i].position = &coordinates[i * 4];
        rects[i].tex_coords = NULL;
        rects[i].tex_coords_len = 0;
    }

    _cg_framebuffer_draw_multitextured_rectangles(
        framebuffer, pipeline, rects, n_rectangles);
}

void
cg_framebuffer_draw_textured_rectangles(cg_framebuffer_t *framebuffer,
                                        cg_pipeline_t *pipeline,
                                        const float *coordinates,
                                        unsigned int n_rectangles)
{
    cg_multi_textured_rect_t *rects;
    int i;

    /* XXX: All the _*_rectangle* APIs normalize their input into an array of
     * _cg_multi_textured_rect_t rectangles and pass these on to our work horse;
     * _cg_framebuffer_draw_multitextured_rectangles.
     */

    rects = c_alloca(n_rectangles * sizeof(cg_multi_textured_rect_t));

    for (i = 0; i < n_rectangles; i++) {
        rects[i].position = &coordinates[i * 8];
        rects[i].tex_coords = &coordinates[i * 8 + 4];
        rects[i].tex_coords_len = 4;
    }

    _cg_framebuffer_draw_multitextured_rectangles(
        framebuffer, pipeline, rects, n_rectangles);
}
