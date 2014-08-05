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

#ifndef _RIG_TEXT_ENGINE_PRIVATE_H_
#define _RIG_TEXT_ENGINE_PRIVATE_H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>

#include <hb.h>

#include <unicode/ubrk.h>

#ifdef __ANDROID__
#include <android/asset_manager.h>
#endif

struct _rig_text_engine_state_t {

#ifdef __ANDROID__
    AAssetManager *asset_manager;
#endif

    c_hash_table_t *icu_common_cache;
    c_hash_table_t *icu_item_cache;

    FcConfig *fc_config;
    FT_Library ft_library;

    hb_font_funcs_t *hb_font_funcs;

    /* User described font-families + weights etc get normalized into
     * a FcPattern... */
    c_hash_table_t *pattern_singletons;

    /* A FcPattern is used to lookup a list of faces that match the
     * pattern with broad unicode coverage. */
    c_hash_table_t *facesets_hash;

    c_hash_table_t *sized_face_hash;

    c_hash_table_t *shared_face_hash;

    UBreakIterator *word_iterator;
};

struct _rig_glyph_index_entry_t {
    uint32_t unicode;
    uint32_t glyph_index;
};

struct _rig_glyph_info_t {
    uint32_t glyph_index;
    int32_t utf16_pos;
    hb_position_t x_advance;
    hb_position_t y_advance;
    hb_position_t x_offset;
    hb_position_t y_offset;
};

struct _rig_sized_face_t {
    rig_shared_face_t *shared;

    float size;

    FT_Matrix ft_matrix;
    bool is_transformed;

    uint32_t ft_load_flags;

    hb_font_t *hb_font;

    FcPattern *prepared_pattern;
};

struct _rig_shared_face_t {
    char *filename;
    int face_index;

    /* Note: That the FT_Size and transform state of this ft_face aren't
     * conceptually part of this "shared" face state. When we query a
     * FT_Face associated with a rig_sized_face_t that we will use this shared
     * face and update the FT_Size state accordingly.
     *
     * @size_state_of lets us know what rig_sized_face_t the FT_Size and
     * transform state currently corresponds to.
     */
    FT_Face ft_face;
    rig_sized_face_t *size_state_of;

    FcCharSet *char_set;

    rig_glyph_index_entry_t *index_cache;

    /* Note: Considering that faces can be scaled, there may be multiple
     * sized faces that share this state each with a different
     * associated fontconfig pattern. We take a reference on the pattern
     * that is used to cache this shared state, as it owns the above
     * filename and char_set state.
     */
    FcPattern *reference_pattern;

};

struct _rig_sized_face_set_t {
    FcPattern *pattern;

    /* Note: the fontset->fonts[] (FcPaterns) are incomplete and should
     * be combined with the above pattern via:
     *
     * FcFontRenderPrepare (config, pattern, fontset->font[i])
     */
    FcFontSet *fontset;

    /* Note: we only fill these in lazily as needed */
    rig_sized_face_t **faces;

};

struct _rig_shaped_run_t {
    c_list_t link;

    rig_sized_face_set_t *faceset;
    rig_sized_face_t *face;

    hb_direction_t direction;

    rig_text_run_t text_run;
    rig_glyph_run_t glyph_run;

    uint8_t data[]; /* over-allocated for glyph_run->glyphs */
};

struct _rig_cumulative_metric_t {
    hb_position_t total_advance;
    hb_position_t width;
};

struct _rig_shaped_paragraph_t {
    c_list_t link;

    UChar *utf16_text;

    rig_text_run_t text_run;

    c_llist_t *markup;

    c_list_t shaped_runs;

    /* An array of accumulated metrics useful for wrapping; indexed by
     * utf16 cluster offsets. This allows us to iterate word boundaries
     * within the utf16 text for the whole paragraph and quickly
     * determine the total x_advance for any given utf16 cluster.
     *
     * Note: this array is sparsely filled according only at offsets
     * that correspond to grapheme clusters as determined when shapping;
     * all other offsets contain are zeroed.
     */
    rig_cumulative_metric_t *wrap_metrics;
};

struct _rig_fixed_run_t {
    c_list_t link;

    hb_position_t x;
    hb_position_t baseline;
    hb_position_t width;

    rig_text_run_t text_run;
    rig_glyph_run_t glyph_run;

    rig_shaped_run_t *shaped_run;
};

struct _rig_wrapped_paragraph_t {
    c_list_t link;

    rig_shaped_paragraph_t *shaped_para;

    float wrap_width;

    /* For left-to-right,top-to-bottom text, this is y offset for the
     * paragraph. For top-to-bottom,right-to-left text this is the
     * x offset from right to left for the paragraph. */
    hb_position_t flow_offset;

    c_list_t fixed_runs;

};

struct _rig_text_engine_t {
    const char *utf8_text;
    int utf8_text_len;

    c_llist_t *markup;
    int wrap_width;

    int width;
    int height;

    c_list_t shaped_paras;
    c_list_t wrapped_paras;

    c_list_t on_wrap_closures;

    unsigned int needs_shape:1;
    unsigned int needs_wrap:1;
};

FT_Face rig_sized_face_get_freetype_face(rig_text_engine_state_t *state,
                                         rig_sized_face_t *face);

#endif /* _RIG_TEXT_ENGINE_PRIVATE_H_ */
