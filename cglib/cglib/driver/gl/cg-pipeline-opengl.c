/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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

#include <cglib-config.h>

#include "cg-debug.h"
#include "cg-util-gl-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-pipeline-private.h"
#include "cg-device-private.h"
#include "cg-texture-private.h"
#include "cg-framebuffer-private.h"
#include "cg-offscreen.h"
#include "cg-texture-gl-private.h"

#include "cg-pipeline-progend-glsl-private.h"

#include <test-fixtures/test-cg-fixtures.h>

#include <clib.h>
#include <string.h>

/*
 * GL/GLES compatability defines for pipeline thingies:
 */

/* These aren't defined in the GLES headers */
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif
#ifndef GL_COORD_REPLACE
#define GL_COORD_REPLACE 0x8862
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812d
#endif
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif

static void
texture_unit_init(cg_device_t *dev, cg_texture_unit_t *unit, int index_)
{
    unit->index = index_;
    unit->enabled_gl_target = 0;
    unit->gl_texture = 0;
    unit->gl_target = 0;
    unit->is_foreign = false;
    unit->dirty_gl_texture = false;
    unit->matrix_stack = cg_matrix_stack_new(dev);

    unit->layer = NULL;
    unit->layer_changes_since_flush = 0;
    unit->texture_storage_changed = false;
}

static void
texture_unit_free(cg_texture_unit_t *unit)
{
    if (unit->layer)
        cg_object_unref(unit->layer);
    cg_object_unref(unit->matrix_stack);
}

cg_texture_unit_t *
_cg_get_texture_unit(cg_device_t *dev, int index_)
{
    if (dev->texture_units->len < (index_ + 1)) {
        int i;
        int prev_len = dev->texture_units->len;
        dev->texture_units = c_array_set_size(dev->texture_units, index_ + 1);
        for (i = prev_len; i <= index_; i++) {
            cg_texture_unit_t *unit =
                &c_array_index(dev->texture_units, cg_texture_unit_t, i);

            texture_unit_init(dev, unit, i);
        }
    }

    return &c_array_index(dev->texture_units, cg_texture_unit_t, index_);
}

void
_cg_destroy_texture_units(cg_device_t *dev)
{
    int i;

    for (i = 0; i < dev->texture_units->len; i++) {
        cg_texture_unit_t *unit =
            &c_array_index(dev->texture_units, cg_texture_unit_t, i);
        texture_unit_free(unit);
    }
    c_array_free(dev->texture_units, true);
}

static void
set_active_texture_unit(cg_device_t *dev, int unit_index)
{
    if (dev->active_texture_unit != unit_index) {
        GE(dev, glActiveTexture(GL_TEXTURE0 + unit_index));
        dev->active_texture_unit = unit_index;
    }
}

/* Note: _cg_bind_gl_texture_transient conceptually has slightly
 * different semantics to OpenGL's glBindTexture because CGlib never
 * cares about tracking multiple textures bound to different targets
 * on the same texture unit.
 *
 * glBindTexture lets you bind multiple textures to a single texture
 * unit if they are bound to different targets. So it does something
 * like:
 *   unit->current_texture[target] = texture;
 *
 * CGlib only lets you associate one texture with the currently active
 * texture unit, so the target is basically a redundant parameter
 * that's implicitly set on that texture.
 *
 * Technically this is just a thin wrapper around glBindTexture so
 * actually it does have the GL semantics but it seems worth
 * mentioning the conceptual difference in case anyone wonders why we
 * don't associate the gl_texture with a gl_target in the
 * cg_texture_unit_t.
 */
void
_cg_bind_gl_texture_transient(GLenum gl_target,
                              GLuint gl_texture,
                              bool is_foreign)
{
    cg_texture_unit_t *unit;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    /* We choose to always make texture unit 1 active for transient
     * binds so that in the common case where multitexturing isn't used
     * we can simply ignore the state of this texture unit. Notably we
     * didn't use a large texture unit (.e.g. (GL_MAX_TEXTURE_UNITS - 1)
     * in case the driver doesn't have a sparse data structure for
     * texture units.
     */
    set_active_texture_unit(dev, 1);
    unit = _cg_get_texture_unit(dev, 1);

    /* NB: If we have previously bound a foreign texture to this texture
     * unit we don't know if that texture has since been deleted and we
     * are seeing the texture name recycled */
    if (unit->gl_texture == gl_texture && !unit->dirty_gl_texture &&
        !unit->is_foreign)
        return;

    GE(dev, glBindTexture(gl_target, gl_texture));

    unit->dirty_gl_texture = true;
    unit->is_foreign = is_foreign;
}

void
_cg_delete_gl_texture(cg_device_t *dev, GLuint gl_texture)
{
    int i;

    for (i = 0; i < dev->texture_units->len; i++) {
        cg_texture_unit_t *unit =
            &c_array_index(dev->texture_units, cg_texture_unit_t, i);

        if (unit->gl_texture == gl_texture) {
            unit->gl_texture = 0;
            unit->gl_target = 0;
            unit->dirty_gl_texture = false;
        }
    }

    GE(dev, glDeleteTextures(1, &gl_texture));
}

/* Whenever the underlying GL texture storage of a cg_texture_t is
 * changed (e.g. due to migration out of a texture atlas) then we are
 * notified. This lets us ensure that we reflush that texture's state
 * if it is reused again with the same texture unit.
 */
void
_cg_pipeline_texture_storage_change_notify(cg_texture_t *texture)
{
    cg_device_t *dev = texture->dev;
    int i;

    for (i = 0; i < dev->texture_units->len; i++) {
        cg_texture_unit_t *unit =
            &c_array_index(dev->texture_units, cg_texture_unit_t, i);

        if (unit->layer &&
            _cg_pipeline_layer_get_texture(unit->layer) == texture)
            unit->texture_storage_changed = true;

        /* NB: the texture may be bound to multiple texture units so
         * we continue to check the rest */
    }
}

void
_cg_gl_use_program(cg_device_t *dev, GLuint gl_program)
{
    if (dev->current_gl_program != gl_program) {
        GLenum gl_error;

        while ((gl_error = dev->glGetError()) != GL_NO_ERROR)
            ;
        dev->glUseProgram(gl_program);
        if (dev->glGetError() == GL_NO_ERROR)
            dev->current_gl_program = gl_program;
        else {
            GE(dev, glUseProgram(0));
            dev->current_gl_program = 0;
        }
    }
}

#if defined(CG_HAS_GLES2_SUPPORT) || defined(CG_HAS_GL_SUPPORT)

static bool
blend_factor_uses_constant(GLenum blend_factor)
{
    return (blend_factor == GL_CONSTANT_COLOR ||
            blend_factor == GL_ONE_MINUS_CONSTANT_COLOR ||
            blend_factor == GL_CONSTANT_ALPHA ||
            blend_factor == GL_ONE_MINUS_CONSTANT_ALPHA);
}

#endif

static void
flush_depth_state(cg_device_t *dev, cg_depth_state_t *depth_state)
{
    bool depth_writing_enabled = depth_state->write_enabled;

    if (dev->current_draw_buffer)
        depth_writing_enabled &=
            dev->current_draw_buffer->depth_writing_enabled;

    if (dev->depth_test_enabled_cache != depth_state->test_enabled) {
        if (depth_state->test_enabled == true)
            GE(dev, glEnable(GL_DEPTH_TEST));
        else
            GE(dev, glDisable(GL_DEPTH_TEST));
        dev->depth_test_enabled_cache = depth_state->test_enabled;
    }

    if (dev->depth_test_function_cache != depth_state->test_function &&
        depth_state->test_enabled == true) {
        GE(dev, glDepthFunc(depth_state->test_function));
        dev->depth_test_function_cache = depth_state->test_function;
    }

    if (dev->depth_writing_enabled_cache != depth_writing_enabled) {
        GE(dev, glDepthMask(depth_writing_enabled ? GL_TRUE : GL_FALSE));
        dev->depth_writing_enabled_cache = depth_writing_enabled;
    }

    if (dev->depth_range_near_cache != depth_state->range_near ||
        dev->depth_range_far_cache != depth_state->range_far) {
        if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_GL_EMBEDDED))
            GE(dev,
               glDepthRangef(depth_state->range_near, depth_state->range_far));
        else
            GE(dev,
               glDepthRange(depth_state->range_near, depth_state->range_far));

        dev->depth_range_near_cache = depth_state->range_near;
        dev->depth_range_far_cache = depth_state->range_far;
    }
}

TEST(check_gl_blend_enable)
{
    cg_pipeline_t *pipeline;

    test_cg_init();

    pipeline = cg_pipeline_new(test_dev);

    /* By default blending should be disabled */
    c_assert_cmpint(test_dev->gl_blend_enable_cache, ==, 0);

    cg_framebuffer_draw_rectangle(test_fb, pipeline, 0, 0, 1, 1);
    _cg_framebuffer_flush(test_fb);

    /* After drawing an opaque rectangle blending should still be
     * disabled */
    c_assert_cmpint(test_dev->gl_blend_enable_cache, ==, 0);

    cg_pipeline_set_color4f(pipeline, 0, 0, 0, 0);
    cg_framebuffer_draw_rectangle(test_fb, pipeline, 0, 0, 1, 1);
    _cg_framebuffer_flush(test_fb);

    /* After drawing a transparent rectangle blending should be enabled */
    c_assert_cmpint(test_dev->gl_blend_enable_cache, ==, 1);

    cg_pipeline_set_blend(pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);
    cg_framebuffer_draw_rectangle(test_fb, pipeline, 0, 0, 1, 1);
    _cg_framebuffer_flush(test_fb);

    /* After setting a blend string that effectively disables blending
     * then blending should be disabled */
    c_assert_cmpint(test_dev->gl_blend_enable_cache, ==, 0);

    test_cg_fini();
}

static int
get_max_activateable_texture_units(cg_device_t *dev)
{
    if (C_UNLIKELY(dev->max_activateable_texture_units == -1)) {
        GLint values[3];
        int n_values = 0;
        int i;

#ifdef CG_HAS_GL_SUPPORT
        if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_GL_EMBEDDED)) {
            /* GL_MAX_TEXTURE_COORDS is provided for GLSL. It defines
             * the number of texture coordinates that can be uploaded
             * (but doesn't necessarily relate to how many texture
             *  images can be sampled) */
            if (cg_has_feature(dev, CG_FEATURE_ID_GLSL)) {
                /* Previously this code subtracted the value by one but there
                   was no explanation for why it did this and it doesn't seem
                   to make sense so it has been removed */
                GE(dev,
                   glGetIntegerv(GL_MAX_TEXTURE_COORDS, values + n_values++));

                /* GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS is defined for GLSL */
                GE(dev,
                   glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                                 values + n_values++));
            }
        }
#endif /* CG_HAS_GL_SUPPORT */

#ifdef CG_HAS_GLES2_SUPPORT
        if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_GL_EMBEDDED)) {
            GE(dev, glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, values + n_values));
            /* Two of the vertex attribs need to be used for the position
               and color */
            values[n_values++] -= 2;

            GE(dev,
               glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                             values + n_values++));
        }
#endif

        c_assert(n_values <= C_N_ELEMENTS(values) && n_values > 0);

        /* Use the maximum value */
        dev->max_activateable_texture_units = values[0];
        for (i = 1; i < n_values; i++)
            dev->max_activateable_texture_units =
                MAX(values[i], dev->max_activateable_texture_units);
    }

    return dev->max_activateable_texture_units;
}

typedef struct {
    cg_device_t *dev;
    int i;
    unsigned long *layer_differences;
} cg_pipeline_flush_layer_state_t;

static bool
flush_layers_common_gl_state_cb(cg_pipeline_layer_t *layer,
                                void *user_data)
{
    cg_pipeline_flush_layer_state_t *flush_state = user_data;
    cg_device_t *dev = flush_state->dev;
    int unit_index = flush_state->i;
    cg_texture_unit_t *unit = _cg_get_texture_unit(dev, unit_index);
    unsigned long layers_difference =
        flush_state->layer_differences[unit_index];

    /* There may not be enough texture units so we can bail out if
     * that's the case...
     */
    if (C_UNLIKELY(unit_index >= get_max_activateable_texture_units(dev))) {
        static bool shown_warning = false;

        if (!shown_warning) {
            c_warning("Your hardware does not have enough texture units"
                      "to handle this many texture layers");
            shown_warning = true;
        }
        return false;
    }

    if (layers_difference & CG_PIPELINE_LAYER_STATE_TEXTURE_DATA) {
        cg_texture_t *texture = _cg_pipeline_layer_get_texture_real(layer);
        GLuint gl_texture;
        GLenum gl_target;

        if (texture == NULL)
            switch (_cg_pipeline_layer_get_texture_type(layer)) {
            case CG_TEXTURE_TYPE_2D:
                texture = CG_TEXTURE(dev->default_gl_texture_2d_tex);
                break;
            case CG_TEXTURE_TYPE_3D:
                texture = CG_TEXTURE(dev->default_gl_texture_3d_tex);
                break;
            }

        cg_texture_get_gl_texture(texture, &gl_texture, &gl_target);

        set_active_texture_unit(dev, unit_index);

        /* NB: There are several CGlib components and some code in
         * Clutter that will temporarily bind arbitrary GL textures to
         * query and modify texture object parameters. If you look at
         * _cg_bind_gl_texture_transient() you can see we make sure
         * that such code always binds to texture unit 1 which means we
         * can't rely on the unit->gl_texture state if unit->index == 1.
         *
         * Because texture unit 1 is a bit special we actually defer any
         * necessary glBindTexture for it until the end of
         * _cg_pipeline_flush_gl_state().
         *
         * NB: we get notified whenever glDeleteTextures is used (see
         * _cg_delete_gl_texture()) where we invalidate
         * unit->gl_texture references to deleted textures so it's safe
         * to compare unit->gl_texture with gl_texture.  (Without the
         * hook it would be possible to delete a GL texture and create a
         * new one with the same name and comparing unit->gl_texture and
         * gl_texture wouldn't detect that.)
         *
         * NB: for foreign textures we don't know how the deletion of
         * the GL texture objects correspond to the deletion of the
         * cg_texture_ts so if there was previously a foreign texture
         * associated with the texture unit then we can't assume that we
         * aren't seeing a recycled texture name so we have to bind.
         */
        if (unit->gl_texture != gl_texture || unit->is_foreign) {
            if (unit_index == 1)
                unit->dirty_gl_texture = true;
            else
                GE(dev, glBindTexture(gl_target, gl_texture));
            unit->gl_texture = gl_texture;
            unit->gl_target = gl_target;
        }

        unit->is_foreign = _cg_texture_is_foreign(texture);

        /* The texture_storage_changed boolean indicates if the
         * cg_texture_t's underlying GL texture storage has changed since
         * it was flushed to the texture unit. We've just flushed the
         * latest state so we can reset this. */
        unit->texture_storage_changed = false;
    }

    if ((layers_difference & CG_PIPELINE_LAYER_STATE_SAMPLER) &&
        _cg_has_private_feature(dev, CG_PRIVATE_FEATURE_SAMPLER_OBJECTS)) {
        const cg_sampler_cache_entry_t *sampler_state;

        sampler_state = _cg_pipeline_layer_get_sampler_state(layer);

        GE(dev, glBindSampler(unit_index, sampler_state->sampler_object));
    }

    cg_object_ref(layer);
    if (unit->layer != NULL)
        cg_object_unref(unit->layer);

    unit->layer = layer;
    unit->layer_changes_since_flush = 0;

    flush_state->i++;

    return true;
}

static void
_cg_pipeline_flush_common_gl_state(cg_device_t *dev,
                                   cg_pipeline_t *pipeline,
                                   unsigned long pipelines_difference,
                                   unsigned long *layer_differences,
                                   bool with_color_attrib)
{
    cg_pipeline_flush_layer_state_t state;

    if (pipelines_difference & CG_PIPELINE_STATE_BLEND) {
        cg_pipeline_t *authority =
            _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_BLEND);
        cg_pipeline_blend_state_t *blend_state =
            &authority->big_state->blend_state;

        if (blend_factor_uses_constant(blend_state->blend_src_factor_rgb) ||
            blend_factor_uses_constant(blend_state->blend_src_factor_alpha) ||
            blend_factor_uses_constant(blend_state->blend_dst_factor_rgb) ||
            blend_factor_uses_constant(blend_state->blend_dst_factor_alpha)) {
            float red = blend_state->blend_constant.red;
            float green = blend_state->blend_constant.green;
            float blue = blend_state->blend_constant.blue;
            float alpha = blend_state->blend_constant.alpha;

            GE(dev, glBlendColor(red, green, blue, alpha));
        }

        if (dev->glBlendEquationSeparate &&
            blend_state->blend_equation_rgb !=
            blend_state->blend_equation_alpha)
            GE(dev,
               glBlendEquationSeparate(blend_state->blend_equation_rgb,
                                       blend_state->blend_equation_alpha));
        else
            GE(dev, glBlendEquation(blend_state->blend_equation_rgb));

        if (dev->glBlendFuncSeparate &&
            (blend_state->blend_src_factor_rgb !=
             blend_state->blend_src_factor_alpha ||
             (blend_state->blend_dst_factor_rgb !=
              blend_state->blend_dst_factor_alpha)))
            GE(dev,
               glBlendFuncSeparate(blend_state->blend_src_factor_rgb,
                                   blend_state->blend_dst_factor_rgb,
                                   blend_state->blend_src_factor_alpha,
                                   blend_state->blend_dst_factor_alpha));
        else
            GE(dev,
               glBlendFunc(blend_state->blend_src_factor_rgb,
                           blend_state->blend_dst_factor_rgb));
    }

    if (pipelines_difference & CG_PIPELINE_STATE_DEPTH) {
        cg_pipeline_t *authority =
            _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_DEPTH);
        cg_depth_state_t *depth_state = &authority->big_state->depth_state;

        flush_depth_state(dev, depth_state);
    }

    if (pipelines_difference & CG_PIPELINE_STATE_LOGIC_OPS) {
        cg_pipeline_t *authority =
            _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_LOGIC_OPS);
        cg_pipeline_logic_ops_state_t *logic_ops_state =
            &authority->big_state->logic_ops_state;
        cg_color_mask_t color_mask = logic_ops_state->color_mask;

        if (dev->current_draw_buffer)
            color_mask &= dev->current_draw_buffer->color_mask;

        GE(dev,
           glColorMask(!!(color_mask & CG_COLOR_MASK_RED),
                       !!(color_mask & CG_COLOR_MASK_GREEN),
                       !!(color_mask & CG_COLOR_MASK_BLUE),
                       !!(color_mask & CG_COLOR_MASK_ALPHA)));
        dev->current_gl_color_mask = color_mask;
    }

    if (pipelines_difference & CG_PIPELINE_STATE_CULL_FACE) {
        cg_pipeline_t *authority =
            _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_CULL_FACE);
        cg_pipeline_cull_face_state_t *cull_face_state =
            &authority->big_state->cull_face_state;

        if (cull_face_state->mode == CG_PIPELINE_CULL_FACE_MODE_NONE)
            GE(dev, glDisable(GL_CULL_FACE));
        else {
            bool invert_winding;

            GE(dev, glEnable(GL_CULL_FACE));

            switch (cull_face_state->mode) {
            case CG_PIPELINE_CULL_FACE_MODE_NONE:
                c_assert_not_reached();

            case CG_PIPELINE_CULL_FACE_MODE_FRONT:
                GE(dev, glCullFace(GL_FRONT));
                break;

            case CG_PIPELINE_CULL_FACE_MODE_BACK:
                GE(dev, glCullFace(GL_BACK));
                break;

            case CG_PIPELINE_CULL_FACE_MODE_BOTH:
                GE(dev, glCullFace(GL_FRONT_AND_BACK));
                break;
            }

            /* If we are painting to an offscreen framebuffer then we
               need to invert the winding of the front face because
               everything is painted upside down */
            invert_winding = cg_is_offscreen(dev->current_draw_buffer);

            switch (cull_face_state->front_winding) {
            case CG_WINDING_CLOCKWISE:
                GE(dev, glFrontFace(invert_winding ? GL_CCW : GL_CW));
                break;

            case CG_WINDING_COUNTER_CLOCKWISE:
                GE(dev, glFrontFace(invert_winding ? GL_CW : GL_CCW));
                break;
            }
        }
    }

#ifdef CG_HAS_GL_SUPPORT
    if (_cg_has_private_feature(dev,
                                CG_PRIVATE_FEATURE_ENABLE_PROGRAM_POINT_SIZE) &&
        (pipelines_difference & CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE)) {
        unsigned long state = CG_PIPELINE_STATE_PER_VERTEX_POINT_SIZE;
        cg_pipeline_t *authority = _cg_pipeline_get_authority(pipeline, state);

        if (authority->big_state->per_vertex_point_size)
            GE(dev, glEnable(GL_PROGRAM_POINT_SIZE));
        else
            GE(dev, glDisable(GL_PROGRAM_POINT_SIZE));
    }
#endif

    if (pipeline->real_blend_enable != dev->gl_blend_enable_cache) {
        if (pipeline->real_blend_enable)
            GE(dev, glEnable(GL_BLEND));
        else
            GE(dev, glDisable(GL_BLEND));
        /* XXX: we shouldn't update any other blend state if blending
         * is disabled! */
        dev->gl_blend_enable_cache = pipeline->real_blend_enable;
    }

    state.dev = dev;
    state.i = 0;
    state.layer_differences = layer_differences;
    _cg_pipeline_foreach_layer_internal(
        pipeline, flush_layers_common_gl_state_cb, &state);
}

/* Re-assert the layer's wrap modes on the given cg_texture_t.
 *
 * Note: we don't simply forward the wrap modes to layer->texture
 * since the actual texture being used may have been overridden.
 */
static void
_cg_pipeline_layer_forward_wrap_modes(cg_pipeline_layer_t *layer,
                                      cg_texture_t *texture)
{
    cg_sampler_cache_wrap_mode_t wrap_mode_s, wrap_mode_t, wrap_mode_p;
    GLenum gl_wrap_mode_s, gl_wrap_mode_t, gl_wrap_mode_p;

    if (texture == NULL)
        return;

    _cg_pipeline_layer_get_wrap_modes(
        layer, &wrap_mode_s, &wrap_mode_t, &wrap_mode_p);

    /* Update the wrap mode on the texture object. The texture backend
       should cache the value so that it will be a no-op if the object
       already has the same wrap mode set. The backend is best placed to
       do this because it knows how many of the coordinates will
       actually be used (ie, a 1D texture only cares about the 's'
       coordinate but a 3D texture would use all three). GL uses the
       wrap mode as part of the texture object state but we are
       pretending it's part of the per-layer environment state. This
       will break if the application tries to use different modes in
       different layers using the same texture. */

    gl_wrap_mode_s = wrap_mode_s;
    gl_wrap_mode_t = wrap_mode_t;
    gl_wrap_mode_p = wrap_mode_p;

    _cg_texture_gl_flush_legacy_texobj_wrap_modes(
        texture, gl_wrap_mode_s, gl_wrap_mode_t, gl_wrap_mode_p);
}

/* OpenGL associates the min/mag filters and repeat modes with the
 * texture object not the texture unit so we always have to re-assert
 * the filter and repeat modes whenever we use a texture since it may
 * be referenced by multiple pipelines with different modes.
 *
 * This function is bypassed in favour of sampler objects if
 * GL_ARB_sampler_objects is advertised. This fallback won't work if
 * the same texture is bound to multiple layers with different sampler
 * state.
 */
static void
foreach_texture_unit_update_filter_and_wrap_modes(cg_device_t *dev)
{
    int i;

    for (i = 0; i < dev->texture_units->len; i++) {
        cg_texture_unit_t *unit =
            &c_array_index(dev->texture_units, cg_texture_unit_t, i);

        if (unit->layer) {
            cg_texture_t *texture = _cg_pipeline_layer_get_texture(unit->layer);

            if (texture != NULL) {
                cg_pipeline_filter_t min;
                cg_pipeline_filter_t mag;

                _cg_pipeline_layer_get_filters(unit->layer, &min, &mag);
                _cg_texture_gl_flush_legacy_texobj_filters(texture, min, mag);

                _cg_pipeline_layer_forward_wrap_modes(unit->layer, texture);
            }
        }
    }
}

typedef struct {
    cg_device_t *dev;
    int i;
    unsigned long *layer_differences;
} cg_pipeline_compare_layers_state_t;

static bool
compare_layer_differences_cb(cg_pipeline_layer_t *layer,
                             void *user_data)
{
    cg_pipeline_compare_layers_state_t *state = user_data;
    cg_texture_unit_t *unit = _cg_get_texture_unit(state->dev, state->i);

    if (unit->layer == layer)
        state->layer_differences[state->i] = unit->layer_changes_since_flush;
    else if (unit->layer) {
        state->layer_differences[state->i] = unit->layer_changes_since_flush;
        state->layer_differences[state->i] |=
            _cg_pipeline_layer_compare_differences(layer, unit->layer);
    } else
        state->layer_differences[state->i] = CG_PIPELINE_LAYER_STATE_ALL_SPARSE;

    /* XXX: There is always a possibility that a cg_texture_t's
     * underlying GL texture storage has been changed since it was last
     * bound to a texture unit which is why we have a callback into
     * _cg_pipeline_texture_storage_change_notify whenever a textures
     * underlying GL texture storage changes which will set the
     * unit->texture_intern_changed flag. If we see that's been set here
     * then we force an update of the texture state...
     */
    if (unit->texture_storage_changed)
        state->layer_differences[state->i] |=
            CG_PIPELINE_LAYER_STATE_TEXTURE_DATA;

    state->i++;

    return true;
}

typedef struct {
    cg_device_t *dev;
    cg_framebuffer_t *framebuffer;
    const cg_pipeline_vertend_t *vertend;
    const cg_pipeline_fragend_t *fragend;
    cg_pipeline_t *pipeline;
    unsigned long *layer_differences;
    bool error_adding_layer;
    bool added_layer;
} cg_pipeline_add_layer_state_t;

static bool
vertend_add_layer_cb(cg_pipeline_layer_t *layer, void *user_data)
{
    cg_pipeline_add_layer_state_t *state = user_data;
    const cg_pipeline_vertend_t *vertend = state->vertend;
    cg_pipeline_t *pipeline = state->pipeline;
    int unit_index = _cg_pipeline_layer_get_unit_index(layer);

    /* Either generate per layer code snippets or setup the
     * fixed function glTexEnv for each layer... */
    if (C_LIKELY(vertend->add_layer(state->dev,
                                    pipeline,
                                    layer,
                                    state->layer_differences[unit_index],
                                    state->framebuffer)))
        state->added_layer = true;
    else {
        state->error_adding_layer = true;
        return false;
    }

    return true;
}

static bool
fragend_add_layer_cb(cg_pipeline_layer_t *layer, void *user_data)
{
    cg_pipeline_add_layer_state_t *state = user_data;
    const cg_pipeline_fragend_t *fragend = state->fragend;
    cg_pipeline_t *pipeline = state->pipeline;
    int unit_index = _cg_pipeline_layer_get_unit_index(layer);

    /* Either generate per layer code snippets or setup the
     * fixed function glTexEnv for each layer... */
    if (C_LIKELY(fragend->add_layer(state->dev, pipeline, layer,
                                    state->layer_differences[unit_index])))
        state->added_layer = true;
    else {
        state->error_adding_layer = true;
        return false;
    }

    return true;
}

/*
 * _cg_pipeline_flush_gl_state:
 *
 * Details of override options:
 * ->fallback_mask: is a bitmask of the pipeline layers that need to be
 *    replaced with the default, fallback textures. The fallback textures are
 *    fully transparent textures so they hopefully wont contribute to the
 *    texture combining.
 *
 *    The intention of fallbacks is to try and preserve
 *    the number of layers the user is expecting so that texture coordinates
 *    they gave will mostly still correspond to the textures they intended, and
 *    have a fighting chance of looking close to their originally intended
 *    result.
 *
 * ->disable_mask: is a bitmask of the pipeline layers that will simply have
 *    texturing disabled. It's only really intended for disabling all layers
 *    > X; i.e. we'd expect to see a contiguous run of 0 starting from the LSB
 *    and at some point the remaining bits flip to 1. It might work to disable
 *    arbitrary layers; though I'm not sure a.t.m how OpenGL would take to
 *    that.
 *
 *    The intention of the disable_mask is for emitting geometry when the user
 *    hasn't supplied enough texture coordinates for all the layers and it's
 *    not possible to auto generate default texture coordinates for those
 *    layers.
 *
 * ->layer0_override_texture: forcibly tells us to bind this GL texture name for
 *    layer 0 instead of plucking the gl_texture from the cg_texture_t of layer
 *    0.
 *
 *    The intention of this is for any primitives that supports sliced textures.
 *    The code will can iterate each of the slices and re-flush the pipeline
 *    forcing the GL texture of each slice in turn.
 *
 * ->wrap_mode_overrides: overrides the wrap modes set on each
 *    layer. This is used to implement the automatic wrap mode.
 *
 * XXX: It might also help if we could specify a texture matrix for code
 *    dealing with slicing that would be multiplied with the users own matrix.
 *
 *    Normaly texture coords in the range [0, 1] refer to the extents of the
 *    texture, but when your GL texture represents a slice of the real texture
 *    (from the users POV) then a texture matrix would be a neat way of
 *    transforming the mapping for each slice.
 *
 *    Currently for textured rectangles we manually calculate the texture
 *    coords for each slice based on the users given coords, but this solution
 *    isn't ideal, and can't be used with CGlibVertexBuffers.
 */
void
_cg_pipeline_flush_gl_state(cg_device_t *dev,
                            cg_pipeline_t *pipeline,
                            cg_framebuffer_t *framebuffer,
                            bool with_color_attrib,
                            bool unknown_color_alpha)
{
    cg_pipeline_t *current_pipeline = dev->current_pipeline;
    unsigned long pipelines_difference;
    int n_layers;
    unsigned long *layer_differences;
    int i;
    cg_texture_unit_t *unit1;
    const cg_pipeline_progend_t *progend;

    CG_STATIC_TIMER(pipeline_flush_timer,
                    "Mainloop", /* parent */
                    "Material Flush",
                    "The time spent flushing material state",
                    0 /* no application private data */);

    CG_TIMER_START(_cg_uprof_context, pipeline_flush_timer);

#warning "HACK"
    current_pipeline = NULL;

    /* Bail out asap if we've been asked to re-flush the already current
     * pipeline and we can see the pipeline hasn't changed */
    if (current_pipeline == pipeline &&
        dev->current_pipeline_age == pipeline->age &&
        dev->current_pipeline_with_color_attrib == with_color_attrib &&
        dev->current_pipeline_unknown_color_alpha == unknown_color_alpha)
        goto done;
    else {
        /* Update derived state (currently just the 'real_blend_enable'
         * state) and determine a mask of state that differs between the
         * current pipeline and the one we are flushing.
         *
         * Note updating the derived state is done before doing any
         * pipeline comparisons so that we can correctly compare the
         * 'real_blend_enable' state itself.
         */

        if (current_pipeline == pipeline) {
            pipelines_difference = dev->current_pipeline_changes_since_flush;

            if (pipelines_difference & CG_PIPELINE_STATE_AFFECTS_BLENDING ||
                pipeline->unknown_color_alpha != unknown_color_alpha) {
                bool save_real_blend_enable = pipeline->real_blend_enable;

                _cg_pipeline_update_real_blend_enable(pipeline,
                                                      unknown_color_alpha);

                if (save_real_blend_enable != pipeline->real_blend_enable)
                    pipelines_difference |= CG_PIPELINE_STATE_REAL_BLEND_ENABLE;
            }
        } else if (current_pipeline) {
            pipelines_difference = dev->current_pipeline_changes_since_flush;

            _cg_pipeline_update_real_blend_enable(pipeline,
                                                  unknown_color_alpha);

            pipelines_difference |= _cg_pipeline_compare_differences(dev->current_pipeline,
                                                                     pipeline);
        } else {
            _cg_pipeline_update_real_blend_enable(pipeline,
                                                  unknown_color_alpha);

            pipelines_difference = CG_PIPELINE_STATE_ALL;
        }
    }

    /* Get a layer_differences mask for each layer to be flushed */
    n_layers = cg_pipeline_get_n_layers(pipeline);
    if (n_layers) {
        cg_pipeline_compare_layers_state_t state;

        layer_differences = c_alloca(sizeof(unsigned long) * n_layers);
        memset(layer_differences, 0, sizeof(unsigned long) * n_layers);

        state.dev = dev;
        state.i = 0;
        state.layer_differences = layer_differences;

        _cg_pipeline_foreach_layer_internal(pipeline,
                                            compare_layer_differences_cb,
                                            &state);
    } else
        layer_differences = NULL;

    /* First flush everything that's the same regardless of which
     * pipeline backend is being used...
     *
     * 1) top level state:
     *  glColor (or skip if a vertex attribute is being used for color)
     *  blend state
     *  alpha test state (except for GLES 2.0)
     *
     * 2) then foreach layer:
     *  determine gl_target/gl_texture
     *  bind texture
     *
     *  Note: After _cg_pipeline_flush_common_gl_state you can expect
     *  all state of the layers corresponding texture unit to be
     *  updated.
     */
    _cg_pipeline_flush_common_gl_state(dev,
                                       pipeline,
                                       pipelines_difference,
                                       layer_differences,
                                       with_color_attrib);

    /* Now flush the fragment, vertex and program state according to the
     * current progend backend.
     *
     * Note: Some backends may not support the current pipeline
     * configuration and in that case it will report and error and we
     * will look for a different backend.
     *
     * NB: if pipeline->progend != CG_PIPELINE_PROGEND_UNDEFINED then
     * we have previously managed to successfully flush this pipeline
     * with the given progend so we will simply use that to avoid
     * fallback code paths.
     */
    if (pipeline->progend == CG_PIPELINE_PROGEND_UNDEFINED)
        _cg_pipeline_set_progend(pipeline, CG_PIPELINE_PROGEND_DEFAULT);

    for (i = pipeline->progend; i < CG_PIPELINE_N_PROGENDS;
         i++, _cg_pipeline_set_progend(pipeline, i)) {
        const cg_pipeline_vertend_t *vertend;
        const cg_pipeline_fragend_t *fragend;
        cg_pipeline_add_layer_state_t state;

        progend = _cg_pipeline_progends[i];

        if (C_UNLIKELY(!progend->start(dev, pipeline)))
            continue;

        vertend = _cg_pipeline_vertends[progend->vertend];

        vertend->start(dev, pipeline, n_layers, pipelines_difference);

        state.dev = dev;
        state.framebuffer = framebuffer;
        state.vertend = vertend;
        state.pipeline = pipeline;
        state.layer_differences = layer_differences;
        state.error_adding_layer = false;
        state.added_layer = false;

        _cg_pipeline_foreach_layer_internal(
            pipeline, vertend_add_layer_cb, &state);

        if (C_UNLIKELY(state.error_adding_layer))
            continue;

        if (C_UNLIKELY(!vertend->end(dev, pipeline, pipelines_difference)))
            continue;

        /* Now prepare the fragment processing state (fragend)
         *
         * NB: We can't combine the setup of the vertend and fragend
         * since the backends that do code generation share
         * dev->codegen_source_buffer as a scratch buffer.
         */

        fragend = _cg_pipeline_fragends[progend->fragend];
        state.fragend = fragend;

        fragend->start(dev, pipeline, n_layers, pipelines_difference);

        _cg_pipeline_foreach_layer_internal(
            pipeline, fragend_add_layer_cb, &state);

        if (C_UNLIKELY(state.error_adding_layer))
            continue;

        if (C_UNLIKELY(!fragend->end(dev, pipeline, pipelines_difference)))
            continue;

        if (progend->end)
            progend->end(dev, pipeline, pipelines_difference);
        break;
    }

    /* Since the NOP progend will claim to handle anything we should
     * never fall through without finding a suitable progend */
    c_assert(i != CG_PIPELINE_N_PROGENDS);

    /* FIXME: This reference is actually resulting in lots of
     * copy-on-write reparenting because one-shot pipelines end up
     * living for longer than necessary and so any later modification of
     * the parent will cause a copy-on-write.
     *
     * XXX: The issue should largely go away when we switch to using
     * weak pipelines for overrides.
     */
    cg_object_ref(pipeline);
    if (dev->current_pipeline != NULL)
        cg_object_unref(dev->current_pipeline);
    dev->current_pipeline = pipeline;
    dev->current_pipeline_changes_since_flush = 0;
    dev->current_pipeline_with_color_attrib = with_color_attrib;
    dev->current_pipeline_unknown_color_alpha = unknown_color_alpha;
    dev->current_pipeline_age = pipeline->age;

done:

    progend = _cg_pipeline_progends[pipeline->progend];

    /* We can't assume the color will be retained between flushes when
     * using the glsl progend because the generic attribute values are
     * not stored as part of the program object so they could be
     * overridden by any attribute changes in another program */
    if (pipeline->progend == CG_PIPELINE_PROGEND_GLSL && !with_color_attrib) {
        int attribute;
        cg_pipeline_t *authority =
            _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_COLOR);
        int name_index = CG_ATTRIBUTE_COLOR_NAME_INDEX;

        attribute =
            _cg_pipeline_progend_glsl_get_attrib_location(dev, pipeline,
                                                          name_index);
        if (attribute != -1)
            GE(dev,
               glVertexAttrib4f(attribute,
                                authority->color.red,
                                authority->color.green,
                                authority->color.blue,
                                authority->color.alpha));
    }

    /* Give the progend a chance to update any uniforms that might not
     * depend on the material state. This is used on GLES2 to update the
     * matrices */
    if (progend->pre_paint)
        progend->pre_paint(dev, pipeline, framebuffer);

    /* Handle the fact that OpenGL associates texture filter and wrap
     * modes with the texture objects not the texture units... */
    if (!_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_SAMPLER_OBJECTS))
        foreach_texture_unit_update_filter_and_wrap_modes(dev);

    /* If this pipeline has more than one layer then we always need
     * to make sure we rebind the texture for unit 1.
     *
     * NB: various components of CGlib may temporarily bind arbitrary
     * textures to texture unit 1 so they can query and modify texture
     * object parameters. cg-pipeline.c (See
     * _cg_bind_gl_texture_transient)
     */
    unit1 = _cg_get_texture_unit(dev, 1);
    if (cg_pipeline_get_n_layers(pipeline) > 1 && unit1->dirty_gl_texture) {
        set_active_texture_unit(dev, 1);
        GE(dev, glBindTexture(unit1->gl_target, unit1->gl_texture));
        unit1->dirty_gl_texture = false;
    }

    CG_TIMER_STOP(_cg_uprof_context, pipeline_flush_timer);
}
