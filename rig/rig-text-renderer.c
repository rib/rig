/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation
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
 */

#include <rig-config.h>

#include <clib.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <unicode/ustring.h>

#include <hb.h>

#include <cglib/cglib.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-text-renderer.h"
#include "rig-text-pipeline-cache.h"
#include "components/rig-text.h"
#include "rig-text-engine-private.h"

typedef struct _rig_glyph_cache_t rig_glyph_cache_t;

struct _rig_text_renderer_state_t {
    rig_text_engine_state_t *engine_state;

    rig_glyph_cache_t *glyph_cache;
    rig_text_pipeline_cache_t *pipeline_cache;

    // cg_pipeline_t *debug_pipeline;
};

typedef struct _rig_glyph_cache_key_t rig_glyph_cache_key_t;

typedef struct _atlas_closure_state_t {
    c_list_t list_node;

    cg_atlas_t *atlas;
    cg_atlas_reorganize_closure_t *reorganize_closure;
    cg_atlas_allocate_closure_t *allocate_closure;
} atlas_closure_state_t;

struct _rig_glyph_cache_t {
    cg_device_t *dev;

    /* Hash table to quickly check whether a particular glyph in a
       particular font is already cached */
    c_hash_table_t *hash_table;

    /* Set of cg_atlas_tes */
    cg_atlas_set_t *atlas_set;

    c_list_t atlas_closures;

    /* callbacks to invoke when an atlas is reorganized */
    c_list_t reorganize_callbacks;

    /* True if some of the glyphs are dirty. This is used as an
       optimization in __glyph_cache_set_dirty_glyphs to avoid
       iterating the hash table if we know none of them are dirty */
    bool has_dirty_glyphs;

    /* Whether mipmapping is being used for this cache. This only
       affects whether we decide to put the glyph in the global atlas */
    bool use_mipmapping;
};

struct _rig_glyph_cache_key_t {
    rig_sized_face_t *face;
    uint32_t glyph_index;
};

typedef struct _rig_glyph_cache_value_t rig_glyph_cache_value_t;

struct _rig_glyph_cache_value_t {
    cg_atlas_t *atlas;
    cg_texture_t *texture;

    float tx1;
    float ty1;
    float tx2;
    float ty2;

    int tx_pixel;
    int ty_pixel;

    int draw_x;
    int draw_y;
    int draw_width;
    int draw_height;

    /* This will be set to true when the glyph atlas is reorganized
       which means the glyph will need to be redrawn */
    bool dirty;
};

typedef void (*rig_glyph_cache_dirty_func_t)(rig_sized_face_t *face,
                                       uint32_t glyph_index,
                                       rig_glyph_cache_value_t *value,
                                       void *user_data);

static void
glyph_cache_value_free(rig_glyph_cache_value_t *value)
{
    if (value->texture) {
        cg_object_unref(value->texture);
        cg_object_unref(value->atlas);
    }
    c_slice_free(rig_glyph_cache_value_t, value);
}

static void
glyph_cache_key_free(rig_glyph_cache_key_t *key)
{
    // rut_object_unref (key->face);
    c_slice_free(rig_glyph_cache_key_t, key);
}

static unsigned int
glyph_cache_hash_func(const void *key)
{
    const rig_glyph_cache_key_t *cache_key = (const rig_glyph_cache_key_t *)key;

    /* Generate a number affected by both the face and the glyph
       number. We can safely directly compare the pointers because the
       key holds a reference to the font so it is not possible that a
       different face will have the same memory address */
    return C_POINTER_TO_UINT(cache_key->face) ^ cache_key->glyph_index;
}

static bool
glyph_cache_equal_func(const void *a, const void *b)
{
    const rig_glyph_cache_key_t *key_a = (const rig_glyph_cache_key_t *)a;
    const rig_glyph_cache_key_t *key_b = (const rig_glyph_cache_key_t *)b;

    /* We can safely directly compare the pointers for the fonts because
       the key holds a reference to the font so it is not possible that
       a different font will have the same memory address */
    return key_a->face == key_b->face &&
           key_a->glyph_index == key_b->glyph_index;
}

static void
atlas_reorganize_cb(cg_atlas_t *atlas, void *user_data)
{
    rig_glyph_cache_t *cache = user_data;

    rut_closure_list_invoke_no_args(&cache->reorganize_callbacks);
}

static void
allocate_glyph_cb(cg_atlas_t *atlas,
                  cg_texture_t *texture,
                  const cg_atlas_allocation_t *allocation,
                  void *allocation_data,
                  void *user_data)
{
    rig_glyph_cache_value_t *value = allocation_data;
    float tex_width, tex_height;

    if (value->texture) {
        cg_object_unref(value->texture);
        cg_object_unref(value->atlas);
    }

    value->atlas = cg_object_ref(atlas);
    value->texture = cg_object_ref(texture);

    tex_width = cg_texture_get_width(texture);
    tex_height = cg_texture_get_height(texture);

    value->tx1 = allocation->x / tex_width;
    value->ty1 = allocation->y / tex_height;
    value->tx2 = (allocation->x + value->draw_width) / tex_width;
    value->ty2 = (allocation->y + value->draw_height) / tex_height;

    value->tx_pixel = allocation->x;
    value->ty_pixel = allocation->y;

    /* The glyph has changed position so it will need to be redrawn */
    value->dirty = true;
}

static void
atlas_callback(cg_atlas_set_t *set,
               cg_atlas_t *atlas,
               cg_atlas_set_event_t event,
               void *user_data)
{
    rig_glyph_cache_t *cache = user_data;
    atlas_closure_state_t *state;

    switch (event) {
    case CG_ATLAS_SET_EVENT_ADDED:
        state = c_slice_new(atlas_closure_state_t);
        state->atlas = atlas;
        state->reorganize_closure = cg_atlas_add_post_reorganize_callback(
            atlas, atlas_reorganize_cb, cache, NULL); /* destroy */
        state->allocate_closure = cg_atlas_add_allocate_callback(
            atlas, allocate_glyph_cb, cache, NULL); /* destroy */

        c_list_insert(cache->atlas_closures.prev, &state->list_node);
        break;
    case CG_ATLAS_SET_EVENT_REMOVED:
        break;
    }
}

rig_glyph_cache_t *
rig_glyph_cache_new(cg_device_t *dev, bool use_mipmapping)
{
    rig_glyph_cache_t *cache;

    cache = c_malloc(sizeof(rig_glyph_cache_t));

    /* Note: as a rule we don't take references to a cg_device_t
     * internally since */
    cache->dev = dev;

    cache->hash_table =
        c_hash_table_new_full(glyph_cache_hash_func,
                              glyph_cache_equal_func,
                              (c_destroy_func_t)glyph_cache_key_free,
                              (c_destroy_func_t)glyph_cache_value_free);

    c_list_init(&cache->atlas_closures);

    cache->atlas_set = cg_atlas_set_new(dev);

    cg_atlas_set_set_components(cache->atlas_set, CG_TEXTURE_COMPONENTS_A);

    cg_atlas_set_set_migration_enabled(cache->atlas_set, false);
    cg_atlas_set_set_clear_enabled(cache->atlas_set, true);

    /* We want to be notified when new atlases are added to our local
     * atlas set so they can be monitored for being re-arranged... */
    cg_atlas_set_add_atlas_callback(
        cache->atlas_set, atlas_callback, cache, NULL); /* destroy */

    c_list_init(&cache->reorganize_callbacks);

    cache->has_dirty_glyphs = false;

    cache->use_mipmapping = use_mipmapping;

    return cache;
}

static void
glyph_cache_clear(rig_glyph_cache_t *cache)
{
    cache->has_dirty_glyphs = false;

    c_hash_table_remove_all(cache->hash_table);
}

void
rig_glyph_cache_free(rig_glyph_cache_t *cache)
{
    atlas_closure_state_t *state, *tmp;

    c_list_for_each_safe(state, tmp, &cache->atlas_closures, list_node)
    {
        cg_atlas_remove_post_reorganize_callback(state->atlas,
                                                   state->reorganize_closure);
        cg_atlas_remove_allocate_callback(state->atlas,
                                            state->allocate_closure);
        c_list_remove(&state->list_node);
        c_slice_free(atlas_closure_state_t, state);
    }

    glyph_cache_clear(cache);

    c_hash_table_destroy(cache->hash_table);

    rut_closure_list_remove_all(&cache->reorganize_callbacks);

    c_free(cache);
}

static bool
glyph_cache_add_to_local_atlas(rig_glyph_cache_t *cache,
                               rig_sized_face_t *face,
                               uint32_t glyph_index,
                               rig_glyph_cache_value_t *value)
{
    cg_atlas_t *atlas;

    /* Add two pixels for the border
     * FIXME: two pixels isn't enough if mipmapping is in use
     */
    atlas = cg_atlas_set_allocate_space(
        cache->atlas_set, value->draw_width + 2, value->draw_height + 2, value);
    if (!atlas)
        return false;

    return true;
}

typedef struct _PseudoRenderState {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
} PseudoRenderState;

static void
pseudo_render_spanner(int y, int n_spans, const FT_Span *spans, void *user_data)
{
    PseudoRenderState *state = user_data;
    int i;

    if (y < state->min_y)
        state->min_y = y;
    if (y > state->max_y)
        state->max_y = y;

    for (i = 0; i < n_spans; i++) {
        if (spans[i].x < state->min_x)
            state->min_x = spans[i].x;
        if (spans[i].x + spans[i].len > state->max_x)
            state->max_x = spans[i].x + spans[i].len;
    }
}

static void
get_ft_glyph_bbox(rig_text_renderer_state_t *render_state,
                  FT_Face ft_face,
                  FT_UInt glyph_index,
                  int *x,
                  int *y,
                  int *width,
                  int *height)
{
    FT_Raster_Params ftr_params;
    PseudoRenderState pseudo_render_state;
    FT_Error ft_error;

    ft_error = FT_Load_Glyph(ft_face, glyph_index, 0);
    if (ft_error) {
        c_warning("Failed to load glyph %08x freetype error = %d",
                  glyph_index,
                  ft_error);
        *x = *y = *width = *height = 0;
        return;
    }

    memset(&ftr_params, 0, sizeof(ftr_params));

    ftr_params.flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA;
    ftr_params.gray_spans = pseudo_render_spanner;
    ftr_params.user = &pseudo_render_state;

    pseudo_render_state.min_x = INT_MAX;
    pseudo_render_state.max_x = INT_MIN;
    pseudo_render_state.min_y = INT_MAX;
    pseudo_render_state.max_y = INT_MIN;

    ft_error = FT_Outline_Render(render_state->engine_state->ft_library,
                                 &ft_face->glyph->outline,
                                 &ftr_params);
    if (ft_error) {
        c_warning("Failed to pseudo render glyph %08x to measure bounding box. "
                  "Freetype error = %d",
                  glyph_index,
                  ft_error);
        *x = *y = *width = *height = 0;
        return;
    }

    /* An empty glyph... */
    if (pseudo_render_state.min_x == INT_MAX) {
        *x = *y = *width = *height = 0;
        return;
    }

    *x = pseudo_render_state.min_x;
    *y = pseudo_render_state.min_y;
    *width = pseudo_render_state.max_x - pseudo_render_state.min_x + 1;
    *height = pseudo_render_state.max_y - pseudo_render_state.min_y + 1;
}

rig_glyph_cache_value_t *
glyph_cache_lookup(rig_text_renderer_state_t *render_state,
                   rig_glyph_cache_t *cache,
                   bool create,
                   rig_sized_face_set_t *faceset,
                   rig_sized_face_t *face,
                   uint32_t glyph_index)
{
    rig_glyph_cache_key_t lookup_key;
    rig_glyph_cache_value_t *value;

    lookup_key.face = face;
    lookup_key.glyph_index = glyph_index;

    value = c_hash_table_lookup(cache->hash_table, &lookup_key);

    if (create && value == NULL) {
        rig_glyph_cache_key_t *key;
        FT_Face ft_face =
            rig_sized_face_get_freetype_face(render_state->engine_state, face);

        value = c_slice_new(rig_glyph_cache_value_t);
        value->texture = NULL;

        get_ft_glyph_bbox(render_state,
                          ft_face,
                          glyph_index,
                          &value->draw_x,
                          &value->draw_y,
                          &value->draw_width,
                          &value->draw_height);

        /* If the glyph is zero-sized then we don't need to reserve any
           space for it and we can just avoid painting anything */
        if (value->draw_width < 1 || value->draw_height < 1)
            value->dirty = false;
        else {
            if (!glyph_cache_add_to_local_atlas(
                    cache, face, glyph_index, value)) {
                glyph_cache_value_free(value);
                return NULL;
            }

            value->dirty = true;
            cache->has_dirty_glyphs = true;
        }

        key = c_slice_new(rig_glyph_cache_key_t);
        key->face = face; // rut_object_ref (face);
        key->glyph_index = glyph_index;

        c_hash_table_insert(cache->hash_table, key, value);
    }

    return value;
}

typedef struct _glyph_cache_foreach_t {
    rig_glyph_cache_dirty_func_t func;
    void *user_data;
} glyph_cache_foreach_t;

static void
glyph_cache_set_dirty_glyphs_cb(void *key_ptr, void *value_ptr, void *user_data)
{
    rig_glyph_cache_key_t *key = key_ptr;
    rig_glyph_cache_value_t *value = value_ptr;

    if (value->dirty) {
        glyph_cache_foreach_t *state = user_data;
        state->func(key->face, key->glyph_index, value, state->user_data);
        value->dirty = false;
    }
}

void
_glyph_cache_set_dirty_glyphs(rig_glyph_cache_t *cache,
                              rig_glyph_cache_dirty_func_t func,
                              void *user_data)
{
    glyph_cache_foreach_t state;

    /* If we know that there are no dirty glyphs then we can shortcut
     * out early */
    if (!cache->has_dirty_glyphs)
        return;

    state.func = func;
    state.user_data = user_data;

    c_hash_table_foreach(
        cache->hash_table, glyph_cache_set_dirty_glyphs_cb, &state);

    cache->has_dirty_glyphs = false;
}

void
_glyph_cache_add_reorganize_closure(rig_glyph_cache_t *cache,
                                    rut_closure_t *closure)
{
    rut_closure_list_add(&cache->reorganize_callbacks, closure);
}

static void
print_utf16(const UChar *utf16_text, int len)
{
    char *utf8_text = c_utf16_to_utf8(utf16_text, len, NULL, NULL, NULL);
    c_debug("%s", utf8_text);
    c_free(utf8_text);
}

static void
wrapped_para_ensure_glyphs(rig_text_renderer_state_t *render_state,
                           rig_wrapped_paragraph_t *para)
{
    rig_glyph_cache_t *glyph_cache = render_state->glyph_cache;
    rig_fixed_run_t *run;

    c_list_for_each(run, &para->fixed_runs, link)
    {
        rig_glyph_info_t *glyphs = run->glyph_run.glyphs;
        int n_glyphs = run->glyph_run.n_glyphs;
        rig_shaped_run_t *shaped_run = run->shaped_run;
        rig_sized_face_set_t *faceset = shaped_run->faceset;
        rig_sized_face_t *face = shaped_run->face;
        int i;

        for (i = 0; i < n_glyphs; i++) {
            glyph_cache_lookup(render_state,
                               glyph_cache,
                               true, /* create */
                               faceset,
                               face,
                               glyphs[i].glyph_index);
        }
    }
}

typedef struct _SpannerRenderState {
    int width;
    int height;
    uint8_t *data;
    int x_offset;
    int y_offset;
} SpannerRenderState;

static void
render_spanner(int y, int n_spans, const FT_Span *spans, void *user_data)
{
    SpannerRenderState *spanner_state = user_data;
    uint8_t *scanline;
    int i;

    y += spanner_state->y_offset;

    /* Note: Freetype uses coordinates with a bottom left origin so we
     * flip vertically... */
    y = spanner_state->height - y - 1;

    /* We should have already validated that all spans will fit within
     * our render target... */
    c_return_if_fail(y >= 0);

    scanline = spanner_state->data + y * spanner_state->width;

    for (i = 0; i < n_spans; i++) {
        int x = spans[i].x + spanner_state->x_offset;
        int len = spans[i].len;
        uint8_t coverage = spans[i].coverage;
        int j;

        /* Again, we should have already validated that all spans will
         * fit within our render target... */
        c_return_if_fail((x + len) <= spanner_state->width);

        for (j = 0; j < len; j++)
            scanline[x++] = coverage;
    }
}

static void
render_dirty_glyph_to_cache_cb(rig_sized_face_t *face,
                               uint32_t glyph_index,
                               rig_glyph_cache_value_t *value,
                               void *user_data)
{
    rig_text_renderer_state_t *render_state = user_data;
    rig_text_engine_state_t *engine_state = render_state->engine_state;
    FT_Raster_Params ftr_params;
    int data_len;
    FT_Error ft_error;
    SpannerRenderState spanner_state;
    FT_Face ft_face = rig_sized_face_get_freetype_face(engine_state, face);

    ft_error = FT_Load_Glyph(ft_face, glyph_index, 0);
    if (ft_error) {
        c_warning("Failed to load glyph %08x freetype error = %d",
                  glyph_index,
                  ft_error);
        return;
    }

    /* set rendering spanner */
    memset(&ftr_params, 0, sizeof(ftr_params));

    ftr_params.flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA;
    ftr_params.gray_spans = render_spanner;
    ftr_params.user = &spanner_state;

    spanner_state.width = value->draw_width;
    spanner_state.height = value->draw_height;
    spanner_state.x_offset = -value->draw_x;
    spanner_state.y_offset = -value->draw_y;
    data_len = spanner_state.width * value->draw_height;
    spanner_state.data = c_malloc(data_len);

    memset(spanner_state.data, 0, data_len);

    ft_error = FT_Outline_Render(
        engine_state->ft_library, &ft_face->glyph->outline, &ftr_params);
    if (ft_error) {
        c_warning("Failed to render glyph %08x to measure bounding box. "
                  "Freetype error = %d",
                  glyph_index,
                  ft_error);
    }

    /* Copy the glyph to the texture */
    cg_texture_set_region(value->texture,
                            value->draw_width,
                            value->draw_height,
                            CG_PIXEL_FORMAT_A_8,
                            spanner_state.width, /* stride */
                            spanner_state.data,
                            value->tx_pixel, /* dst_x */
                            value->ty_pixel, /* dst_y */
                            0, /* mipmap level */
                            NULL); /* don't catch errors */

    c_free(spanner_state.data);
}

static hb_position_t
draw_wrapped_para(rig_text_renderer_state_t *render_state,
                  rig_paint_context_t *paint_ctx,
                  hb_position_t baseline_offset,
                  rig_wrapped_paragraph_t *para)
{
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(paint_ctx->_parent.camera);
    hb_position_t baseline = baseline_offset;
    rig_fixed_run_t *run;

    wrapped_para_ensure_glyphs(render_state, para);

    _glyph_cache_set_dirty_glyphs(render_state->glyph_cache,
                                  render_dirty_glyph_to_cache_cb,
                                  render_state);

    c_list_for_each(run, &para->fixed_runs, link)
    {
        rig_glyph_info_t *glyphs = run->glyph_run.glyphs;
        int n_glyphs = run->glyph_run.n_glyphs;
        rig_shaped_run_t *shaped_run = run->shaped_run;
        rig_sized_face_t *face = shaped_run->face;
        bool hinting = !(face->ft_load_flags & FT_LOAD_NO_HINTING);
        hb_position_t start_x = run->x;
        hb_position_t run_width = run->width;
        hb_position_t x_advance = 0;
        bool rtl =
            HB_DIRECTION_IS_BACKWARD(shaped_run->direction) ? true : false;
        int i;

        baseline = baseline_offset + run->baseline;

        for (i = 0; i < n_glyphs; i++) {
            rig_glyph_cache_value_t *cached_glyph;
            rig_glyph_info_t *glyph = &glyphs[i];
            cg_pipeline_t *pipeline;
            hb_position_t glyph_x;
            float x, y;

            cached_glyph = glyph_cache_lookup(render_state,
                                              render_state->glyph_cache,
                                              true, /* create */
                                              shaped_run->faceset,
                                              shaped_run->face,
                                              glyphs[i].glyph_index);

            if (rtl) {
                x_advance += glyph->x_advance;
                glyph_x = start_x + run_width - x_advance + glyph->x_offset;
            } else {
                glyph_x = start_x + x_advance + glyph->x_offset;
                x_advance += glyph->x_advance;
            }

            if (hinting) {
                x = (int)((round_26_6(glyph_x) / 64) + cached_glyph->draw_x);
                y = (int)((round_26_6(baseline) / 64) - cached_glyph->draw_y);
            } else {
                x = (glyph_x / 64.f) + cached_glyph->draw_x;
                y = (baseline / 64.f) - cached_glyph->draw_y;
            }

            pipeline = rig_text_pipeline_cache_get(render_state->pipeline_cache,
                                                   cached_glyph->texture);

            cg_framebuffer_draw_textured_rectangle(
                fb,
                pipeline,
                x,
                y - cached_glyph->draw_height,
                x + cached_glyph->draw_width,
                y,
                cached_glyph->tx1,
                cached_glyph->ty1,
                cached_glyph->tx2,
                cached_glyph->ty2);
        }
    }

    return baseline;
}

void
rig_text_renderer_draw(rig_paint_context_t *paint_ctx,
                       rig_text_renderer_state_t *render_state,
                       rig_text_t *text)
{
    rig_text_engine_t *text_engine = text->text_engine;
    hb_position_t baseline_offset = 0;
    rig_wrapped_paragraph_t *para;

    rig_text_engine_wrap(render_state->engine_state, text_engine);

    c_list_for_each(para, &text_engine->wrapped_paras, link)
    {
        baseline_offset =
            draw_wrapped_para(render_state, paint_ctx, baseline_offset, para);

        /* TODO: Add more space between paragraphs... */
    }
}

rig_text_renderer_state_t *
rig_text_renderer_state_new(rig_frontend_t *frontend)
{
    rig_engine_t *engine = frontend->engine;
    rig_text_renderer_state_t *render_state =
        c_slice_new0(rig_text_renderer_state_t);
    rut_shell_t *shell = engine->shell;

    render_state->engine_state = engine->text_state;

    // state->debug_pipeline = cg_pipeline_new (ctx->cg_device);

    render_state->glyph_cache =
        rig_glyph_cache_new(shell->cg_device, true /* use mipmapping */);
    render_state->pipeline_cache = rig_text_pipeline_cache_new(
        shell->cg_device, true /* use mipmaping */);

    return render_state;
}

void
rig_text_renderer_state_destroy(rig_text_renderer_state_t *render_state)
{
    rig_glyph_cache_free(render_state->glyph_cache);
    rig_text_pipeline_cache_free(render_state->pipeline_cache);

    c_slice_free(rig_text_renderer_state_t, render_state);
}
