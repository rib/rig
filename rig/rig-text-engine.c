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


#ifdef __ANDROID__
#include <android/asset_manager.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <clib.h>

#include <unicode/ubidi.h>
#include <unicode/udata.h>
#include <unicode/ustring.h>

#include "usc_impl.h"

#include <hb.h>
#include <hb-ft.h>
#include <hb-icu.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-text-engine-funcs.h"
#include "rig-text-engine.h"
#include "rig-text-engine-private.h"

typedef struct _itemizer_t itemizer_t;

struct _itemizer_t {
    rig_text_engine_state_t *state;

    UChar *para_utf16_text;
    rig_text_run_t para_run;

    UBiDi *ubidi;

    rig_sized_face_set_t *faceset;

    /* State machine */
    hb_script_t script;
    hb_direction_t direction;
    rig_sized_face_t *face;

    void (*run_callback)(itemizer_t *itemizer,
                         const rig_text_run_t *run,
                         void *user_data);
    void *callback_data;
};

typedef struct _shape_context_t {
    rig_text_engine_state_t *state;
    rig_text_engine_t *text_engine;

    const char *utf8_text;
    int utf8_text_len;

    hb_buffer_t *hb_buf;

    /* TODO: support rich text formatting. For now everything
     * is draw with a single faceset... */
    rig_sized_face_set_t *faceset;

    c_list_t shaped_paras;
    rig_shaped_paragraph_t *current_para;

} shape_context_t;

typedef enum _alignment_t {
    ALIGNMENT_LEFT,
    ALIGNMENT_RIGHT,
    ALIGNMENT_CENTER
} alignment_t;

typedef struct _wrap_state_t {
    rig_text_engine_state_t *state;
    rig_text_engine_t *text_engine;

    rig_shaped_paragraph_t *para;
    // int utf16_para_start;
    // int utf16_para_len;

    hb_position_t wrap_width;

    hb_direction_t default_direction;
    alignment_t alignment;

    alignment_t effective_alignment;
    bool invert;

    c_list_t unaligned;

    hb_position_t line_advance;
    hb_position_t max_leading;

    hb_position_t x;
    hb_position_t baseline;

    rig_wrapped_paragraph_t *wrapped_para;
} wrap_state_t;

struct _rig_size_markup_t {
    rut_object_base_t _base;
    rig_markup_t markup;
    int size;
};

struct _rig_family_markup_t {
    rut_object_base_t _base;
    rig_markup_t markup;
    char *family;
};


static rig_sized_face_t *faceset_get_face(rig_text_engine_state_t *state,
                                          rig_sized_face_set_t *set,
                                          int i);

int rig_markup_trait_id;

static FcPattern *
get_pattern_singleton(rig_text_engine_state_t *state,
                      FcPattern *pattern)
{
    FcPattern *singleton =
        c_hash_table_lookup(state->pattern_singletons, pattern);

    if (singleton)
        return singleton;

    FcPatternReference(pattern);
    c_hash_table_insert(state->pattern_singletons, pattern, pattern);

    return pattern;
}

FcPattern *
lookup_pattern(rig_text_engine_state_t *state,
               rig_text_engine_t *text_engine)
{
    const char *font_family = "sans";
    char **families;
    FcPattern *pattern = FcPatternCreate();
    FcPattern *singleton;
    int size = 12;
    c_llist_t *l;
    int i;

    /* XXX: Hack:
     * We just lookup the first size and family markup to use for the whole
     * paragraph
     *
     * TODO: split up text according to markup changes
     */
    for (l = text_engine->markup; l; l = l->next) {
        rut_object_t *markup = l->data;
        if (rut_object_get_type(markup) == &rig_size_markup_type) {
            size = ((rig_size_markup_t *)markup)->size;
            break;
        }
    }

    for (l = text_engine->markup; l; l = l->next) {
        rut_object_t *markup = l->data;
        if (rut_object_get_type(markup) == &rig_family_markup_type) {
            font_family = ((rig_family_markup_t *)markup)->family;
            break;
        }
    }

    families = c_strsplit(font_family, ",", -1);

    FcPatternAddDouble(pattern, FC_SIZE, size);

    for (i = 0; families[i]; i++)
        FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)families[i]);
    c_strfreev(families);

    FcConfigSubstitute(state->fc_config, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    singleton = get_pattern_singleton(state, pattern);
    FcPatternDestroy(pattern);

    return singleton;
}

static rig_sized_face_set_t *
lookup_faceset(rig_text_engine_state_t *state,
               rig_text_engine_t *text_engine)
{
    FcPattern *pattern = lookup_pattern(state, text_engine);
    rig_sized_face_set_t *faceset =
        c_hash_table_lookup(state->facesets_hash, pattern);
    FcResult result;

    if (faceset)
        return faceset;

    faceset = c_slice_new0(rig_sized_face_set_t);

    faceset->pattern = pattern;

    /* Note: A freetype fontset refers to an array of FcPatterns that
     * themselves refer to specific faces within a font. */
    faceset->fontset =
        FcFontSort(state->fc_config, pattern, FcTrue, NULL, &result);
    if (!faceset->fontset) {
        c_warning("Failed to create font set for a given pattern");
        faceset->fontset = FcFontSetCreate();
    }

    /* Note: we only lookup corresponding rig_sized_face_t lazily as needed */
    faceset->faces = c_malloc0(sizeof(void *) * faceset->fontset->nfont);

    c_hash_table_insert(state->facesets_hash, pattern, faceset);

    {
        rig_sized_face_t *face;
        face = faceset_get_face(state, faceset, 0);
        c_debug("face filename=%s\n", face->shared->filename);
    }

    return faceset;
}

static void
faceset_free(void *data)
{
    rig_sized_face_set_t *faceset = data;

    c_free(faceset->faces);
    FcFontSetDestroy(faceset->fontset);

    c_slice_free(rig_sized_face_set_t, faceset);
}

rig_shared_face_t *
get_shared_face(rig_text_engine_state_t *state,
                FcPattern *pattern)
{
    rig_shared_face_t key;
    rig_shared_face_t *shared_face;

    if (FcPatternGetString(pattern, FC_FILE, 0, (FcChar8 **)&key.filename) !=
        FcResultMatch)
        return NULL;

    if (FcPatternGetInteger(pattern, FC_INDEX, 0, &key.face_index) !=
        FcResultMatch)
        return NULL;

    shared_face = c_hash_table_lookup(state->sized_face_hash, &key);
    if (shared_face)
        return shared_face;

    shared_face = c_slice_new0(rig_shared_face_t);

    shared_face->filename = key.filename;
    shared_face->face_index = key.face_index;

    FcPatternGetCharSet(pattern, FC_CHARSET, 0, &shared_face->char_set);

    shared_face->reference_pattern = pattern;
    FcPatternReference(pattern);

    return shared_face;
}

static rig_sized_face_t *
faceset_get_face(rig_text_engine_state_t *state,
                 rig_sized_face_set_t *set,
                 int i)
{
    FcPattern *pattern;
    FcPattern *singleton_pattern;
    rig_sized_face_t *sized_face = NULL;
    double size;
    FcMatrix *fc_matrix;
    FcBool fc_hinting;
    FcBool fc_autohint;
    FcBool fc_antialias;

    if (set->faces[i])
        return set->faces[i];

    pattern = FcFontRenderPrepare(
        state->fc_config, set->pattern, set->fontset->fonts[i]);

    singleton_pattern = get_pattern_singleton(state, pattern);
    FcPatternDestroy(pattern);

    set->faces[i] =
        c_hash_table_lookup(state->sized_face_hash, singleton_pattern);
    if (set->faces[i])
        return set->faces[i];

    sized_face = c_slice_new0(rig_sized_face_t);

    if (FcPatternGetDouble(set->pattern, FC_SIZE, 0, &size) != FcResultMatch) {
        c_warning("Spurious missing FC_SIZE property on pattern");
        size = 10;
    }
    sized_face->size = size;

    if (FcPatternGetMatrix(pattern, FC_MATRIX, 0, &fc_matrix) ==
        FcResultMatch) {
        if (fc_matrix->xy || fc_matrix->yx || fc_matrix->xx != 1.0 ||
            fc_matrix->yy != 1.0) {
            sized_face->ft_matrix.xx = 0x10000 * fc_matrix->xx;
            sized_face->ft_matrix.yy = 0x10000 * fc_matrix->yy;
            sized_face->ft_matrix.xy = 0x10000 * fc_matrix->xy;
            sized_face->ft_matrix.yx = 0x10000 * fc_matrix->yx;
            sized_face->is_transformed = true;
        }
    }

    if (FcPatternGetBool(pattern, FC_HINTING, 0, &fc_hinting) ==
        FcResultMatch &&
        !fc_hinting) {
        sized_face->ft_load_flags |= FT_LOAD_NO_HINTING;
    }

    if (FcPatternGetBool(pattern, FC_AUTOHINT, 0, &fc_autohint) ==
        FcResultMatch &&
        fc_autohint) {
        sized_face->ft_load_flags |= FT_LOAD_FORCE_AUTOHINT;
    }

    if (FcPatternGetBool(pattern, FC_ANTIALIAS, 0, &fc_antialias) !=
        FcResultMatch)
        fc_antialias = FcTrue;

    if (fc_antialias)
        sized_face->ft_load_flags |= FT_LOAD_NO_BITMAP;
    else
        sized_face->ft_load_flags |= FT_LOAD_TARGET_MONO;

    sized_face->prepared_pattern = singleton_pattern;
    FcPatternReference(singleton_pattern);

    sized_face->shared = get_shared_face(state, singleton_pattern);

    set->faces[i] = sized_face;

    return set->faces[i];
}

bool
sized_face_covers(rig_sized_face_t *sized_face, uint32_t codepoint)
{
    rig_shared_face_t *shared_face = sized_face->shared;

    if (!shared_face->char_set) {
        if (FcPatternGetCharSet(shared_face->reference_pattern,
                                FC_CHARSET,
                                0,
                                &shared_face->char_set) != FcResultMatch)
            return false;
    }

    return FcCharSetHasChar(shared_face->char_set, codepoint);
}

FT_Face
rig_sized_face_get_freetype_face(rig_text_engine_state_t *state,
                                 rig_sized_face_t *face)
{
    rig_shared_face_t *shared = face->shared;

    if (!shared->ft_face) {
        FT_Error ft_error = FT_New_Face(state->ft_library,
                                        shared->filename,
                                        shared->face_index,
                                        &shared->ft_face);
        if (ft_error) {
            c_warning("Failed to create freetype font face");
            return NULL;
        }
    }

    /* Note: An FT_Face also contains size information that isn't
     * conceptually part of the rig_shared_face_t state and may need
     * updating...
     */
    if (shared->size_state_of != face) {
        FT_F26Dot6 ft_size;

        if (face->is_transformed) {
            FT_Set_Transform(
                face->shared->ft_face, &face->ft_matrix, NULL); /* no delta */
        } else {
            FT_Set_Transform(face->shared->ft_face,
                             NULL, /* identity */
                             NULL); /* no delta */
        }

        ft_size = face->size * 64;
        FT_Set_Char_Size(face->shared->ft_face,
                         0, /* width in points (26.6) */
                         ft_size, /* height in points (26.6) */
                         376, /* dpi FIXME: this is just the dpi of my phone */
                         376);

        shared->size_state_of = face;
    }

    return shared->ft_face;
}

hb_font_t *
get_harfbuzz_font(rig_text_engine_state_t *state,
                  rig_sized_face_t *face)
{
    FT_Face ft_face;

    if (face->hb_font)
        return face->hb_font;

    ft_face = rig_sized_face_get_freetype_face(state, face);
    if (ft_face)
        face->hb_font = hb_ft_font_create(ft_face, NULL /* destroy notify */);

    return face->hb_font;
}

static unsigned int
shared_face_hash(const void *data)
{
    const rig_shared_face_t *shared = data;
    return c_str_hash(shared->filename) ^ shared->face_index;
}

static bool
shared_face_equal(const void *a, const void *b)
{
    const rig_shared_face_t *shared_a = a;
    const rig_shared_face_t *shared_b = b;

    if (shared_a == shared_b)
        return true;

    if (shared_a->face_index == shared_b->face_index &&
        strcmp(shared_a->filename, shared_b->filename) == 0)
        return true;

    return false;
}

static void
shared_face_free(void *data)
{
    rig_shared_face_t *shared = data;

    if (shared->index_cache)
        c_free(shared->index_cache);

    if (shared->ft_face)
        FT_Done_Face(shared->ft_face);

    FcPatternDestroy(shared->reference_pattern);

    c_slice_free(rig_shared_face_t, shared);
}

static void
sized_face_free(void *data)
{
    rig_sized_face_t *state = data;

    if (state->hb_font)
        hb_font_destroy(state->hb_font);

    FcPatternDestroy(state->prepared_pattern);

    c_slice_free(rig_sized_face_t, state);
}

static uint32_t
next_utf16(const uint16_t *utf16_text,
           int32_t *cursor,
           int32_t end,
           uint32_t invalid_replacement)
{
    uint16_t unit = utf16_text[(*cursor)++];

    /* Return unit if it's not a high surrogate */
    if (unit < 0xD800 || unit > 0xDBFF)
        return unit;

    if (*cursor == end) {
        /* Mising low surrogate */
        return invalid_replacement;
    }

    if (utf16_text[*cursor] < 0xDC00 || utf16_text[*cursor] > 0xDFFF) {
        /* Invalid low surrogate; don't advance cursor */
        return invalid_replacement;
    }

    return (((uint32_t)unit & 0x3FF) << 10) +
           (utf16_text[(*cursor)++] & 0x3FF) + 0x10000;
}

static void
print_utf16(const UChar *utf16_text, int len)
{
    char *utf8_text = c_utf16_to_utf8(utf16_text, len, NULL, NULL, NULL);
    c_debug("%s", utf8_text);
    c_free(utf8_text);
}

static rig_sized_face_t *
select_sized_face(rig_text_engine_state_t *state,
                  rig_sized_face_set_t *faceset,
                  uint32_t codepoint)
{
    int n_faces = faceset->fontset->nfont;
    int i = 0;

    for (i = 0; i < n_faces; i++) {
        rig_sized_face_t *sized_face = faceset_get_face(state, faceset, i++);
        if (sized_face)
            return sized_face;
    }

    c_warning("Failed to find face in set for codepoint");

    return NULL;
}

/* Takes a script run and further splits it by font face */
static void
itemize_script_run(itemizer_t *itemizer, const rig_text_run_t *run)
{
    UChar *utf16_text = itemizer->para_utf16_text;
    uint32_t codepoint;
    uint32_t last_codepoint = 0xFFFFFFFF; /* invalid unicode */
    int32_t cursor;
    int32_t pos;
    rig_text_run_t font_run;

    cursor = font_run.start = run->start;
    do {
        bool split = false;

        pos = cursor;
        codepoint =
            next_utf16(utf16_text, &cursor, run->end, 0xFFFD); /* "REPLACEMENT
                                                                  CHARACTER" */

        /* Split either side of tabs to shape manually and individually */
        if (codepoint == '\t' || last_codepoint == '\t')
            split = true;
        else if (cursor == run->end)
            split = true;
        else if (!u_isUWhiteSpace((UChar32)codepoint)) {
            rig_sized_face_t *face = select_sized_face(
                itemizer->state, itemizer->faceset, codepoint);

            if (!itemizer->face)
                itemizer->face = face;

            if (face != itemizer->face)
                split = true;
        }

        if (split && pos > font_run.start) {
            font_run.end = cursor;

            /* Note: we avoid selecting a font face for whitespace
             * characters but it's possible we've reached the end of our
             * first run and not yet selected a face... */
            if (!itemizer->face)
                itemizer->face = select_sized_face(
                    itemizer->state, itemizer->faceset, last_codepoint);

            itemizer->run_callback(
                itemizer, &font_run, itemizer->callback_data);

            font_run.start = font_run.end;
        }

        last_codepoint = codepoint;
    } while (cursor < run->end);
}

/* Takes a BiDi run and further splits it by script and font changes */
static void
itemize_bidi_run(itemizer_t *itemizer, const rig_text_run_t *run)
{
    UErrorCode uerror = U_ZERO_ERROR;
    int len = text_run_len(run);
    UScriptRun script_itemizer;
    rig_text_run_t script_run;
    UScriptCode script;

    rig_uscript_initRun(
        &script_itemizer, itemizer->para_utf16_text + run->start, len, &uerror);

    if (U_FAILURE(uerror)) {
        c_warning("Failed to split run according to script");
        itemizer->script = HB_SCRIPT_UNKNOWN;
        itemize_script_run(itemizer, run);
        return;
    }

    while (rig_uscript_nextRun(
               &script_itemizer, &script_run.start, &script_run.end, &script)) {
        itemizer->script = hb_icu_script_to_script(script);

        script_run.start += run->start;
        script_run.end += run->start;
        itemize_script_run(itemizer, &script_run);
    }

    rig_uscript_closeRun(&script_itemizer);
}

static void
itemize_paragraph(itemizer_t *itemizer, const rig_text_run_t *run)
{
    UErrorCode uerror = U_ZERO_ERROR;
    UBiDi *ubidi = itemizer->ubidi;

    ubidi_setPara(ubidi,
                  itemizer->para_utf16_text,
                  run->end - run->start,
                  UBIDI_DEFAULT_LTR,
                  NULL,
                  &uerror);

    if (U_FAILURE(uerror)) {
        c_warning("Failed to run bidi algorithm over text");
        itemize_bidi_run(itemizer, run);
        return;
    }

#warning "disable hack to force BiDi code path"
    if (1) // U_SUCCESS (uerror) && ubidi_getDirection (para) == UBIDI_MIXED)
    {
        int32_t para_len = ubidi_getLength(ubidi);
        UBiDiLevel level;
        rig_text_run_t bidi_run;

        bidi_run.start = 0;
        for (ubidi_getLogicalRun(ubidi, bidi_run.start, &bidi_run.end, &level);
             bidi_run.start < para_len;
             ubidi_getLogicalRun(ubidi, bidi_run.end, &bidi_run.end, &level)) {
            itemizer->direction =
                ((level & 0x1) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);

            itemize_bidi_run(itemizer, &bidi_run);

            bidi_run.start = bidi_run.end;
        }
    } else {
        itemizer->direction =
            ((ubidi_getDirection(ubidi) & 0x1) ? HB_DIRECTION_RTL
             : HB_DIRECTION_LTR);
        itemize_bidi_run(itemizer, run);
    }
}

#define INDEX_CACHE_SIZE 256
#define INDEX_CACHE_MASK 0xff

static uint32_t
unicode_to_glyph_index(rig_text_engine_state_t *state,
                       rig_sized_face_t *face,
                       uint32_t unicode)
{
    rig_shared_face_t *shared_face = face->shared;
    rig_glyph_index_entry_t *index_cache = shared_face->index_cache;
    uint32_t index;
    rig_glyph_index_entry_t *glyph_index;
    FT_Face ft_face;

    if (!index_cache) {
        shared_face->index_cache = index_cache =
                                       c_malloc0(sizeof(rig_glyph_index_entry_t) * INDEX_CACHE_SIZE);
    }

    index = unicode & INDEX_CACHE_MASK;
    glyph_index = &index_cache[index];

    if (glyph_index->unicode == unicode)
        return glyph_index->glyph_index;

    glyph_index->unicode = unicode;

    ft_face = rig_sized_face_get_freetype_face(state, face);
    glyph_index->glyph_index = FcFreeTypeCharIndex(ft_face, unicode);

    return glyph_index->glyph_index;
}

static hb_bool_t
harfbuzz_font_func_get_glyph_cb(hb_font_t *font,
                                void *font_data,
                                hb_codepoint_t unicode,
                                hb_codepoint_t variation_selector,
                                hb_codepoint_t *glyph,
                                void *user_data)
{
    itemizer_t *itemizer = user_data;

    *glyph = unicode_to_glyph_index(itemizer->state, itemizer->face, unicode);
    return !!*glyph;
}

static void
shape_run_cb(itemizer_t *itemizer, const rig_text_run_t *run, void *user_data)
{
    rig_text_engine_state_t *state = itemizer->state;
    shape_context_t *ctx = user_data;
    int32_t len = text_run_len(run);
    rig_sized_face_set_t *faceset;
    rig_sized_face_t *face;
    FT_Face ft_face;
    hb_font_t *hb_font;
    bool hinting;
    unsigned int x_ppem;
    unsigned int y_ppem;
    hb_glyph_info_t *glyph_info;
    hb_glyph_position_t *glyph_pos;
    unsigned int glyph_count;
    rig_glyph_info_t *glyphs;
    rig_shaped_run_t *shaped_run;
    int first_glyph;
    int dir;
    int i;
    int c;

    hb_buffer_clear_contents(ctx->hb_buf);

    hb_buffer_add_utf16(ctx->hb_buf,
                        itemizer->para_utf16_text + run->start,
                        len,
                        0, /* item offset */
                        len);

    hb_buffer_set_direction(ctx->hb_buf, itemizer->direction);
    hb_buffer_set_script(ctx->hb_buf, itemizer->script);

    face = itemizer->face;
    ft_face = rig_sized_face_get_freetype_face(state, face);
    faceset = itemizer->faceset;
    hb_font = get_harfbuzz_font(state, face);

    hb_font_funcs_set_glyph_func(state->hb_font_funcs,
                                 harfbuzz_font_func_get_glyph_cb,
                                 itemizer, /* user data */
                                 NULL); /* destroy */

    hb_font_set_funcs(
        hb_font, state->hb_font_funcs, ft_face, NULL); /* destroy */

    hinting = !(face->ft_load_flags & FT_LOAD_NO_HINTING);
    if (hinting)
        x_ppem = y_ppem = 0;
    else {
        x_ppem = ft_face->size->metrics.x_ppem;
        y_ppem = ft_face->size->metrics.y_ppem;
    }

    hb_font_set_ppem(hb_font, x_ppem, y_ppem);

    hb_shape(hb_font,
             ctx->hb_buf,
             NULL, /* features */
             0 /* n features */);

    glyph_info = hb_buffer_get_glyph_infos(ctx->hb_buf, &glyph_count);
    glyph_pos = hb_buffer_get_glyph_positions(ctx->hb_buf, &glyph_count);

    shaped_run = c_malloc(sizeof(rig_shaped_run_t) +
                          glyph_count * sizeof(rig_glyph_info_t));
    shaped_run->faceset = faceset; /* FIXME: ref-count */
    shaped_run->face = face; /* FIXME: ref-count */
    shaped_run->direction = itemizer->direction;
    shaped_run->text_run = *run;
    shaped_run->glyph_run.n_glyphs = glyph_count;
    shaped_run->glyph_run.glyphs = (rig_glyph_info_t *)shaped_run->data;

    c_list_insert(ctx->current_para->shaped_runs.prev, &shaped_run->link);

#if 0
    /* Harbuzz will maintain glyphs in logical order but we want to build
     * up glyph-runs in visual order... */
    if (HB_DIRECTION_IS_FORWARD (itemizer->direction))
    {
        first_glyph = 0;
        dir = 1;
    }
    else
    {
        first_glyph = glyph_count - 1;
        dir = -1;
    }
#endif
    /* XXX: actually for now we are keeping the glyphs in logical order
     * instead since logical order is more useful for word wrapping */
    first_glyph = 0;
    dir = 1;

    glyphs = shaped_run->glyph_run.glyphs;
    for (i = first_glyph, c = 0; c < glyph_count; i += dir, c++) {
        glyphs[i].glyph_index = glyph_info[i].codepoint;

        /* Note: In addition to delimiting grapheme clusters, Harfbuzz's
         * hg_glyph_info_t->cluster member is guaranteed to map back to
         * the cluster value of the original unicode codepoints it was
         * given. By default when we add text to Harfbuzz buffer then
         * each codepoint in our text is given a cluster value that is
         * the byte offset to the start of that codepoint and so we can
         * use cluster values to map back into our original text.
         */
        glyphs[i].utf16_pos = run->start + glyph_info[i].cluster;
        glyphs[i].x_advance = glyph_pos[i].x_advance;
        glyphs[i].y_advance = glyph_pos[i].y_advance;
        if (hinting) {
            glyphs[i].x_advance = round_26_6(glyphs[i].x_advance);
            glyphs[i].y_advance = round_26_6(glyphs[i].y_advance);
        }
        glyphs[i].x_offset = glyph_pos[i].x_offset;
        glyphs[i].y_offset = glyph_pos[i].y_offset;

        c_debug("glyph %d: clust=%d idx=%lu x_adv=%f, y_adv=%f, x_off=%d, "
                "y_off=%d\n",
                i,
                glyphs[i].utf16_pos,
                (unsigned long)glyphs[i].glyph_index,
                glyphs[i].x_advance / 64.,
                glyphs[i].y_advance / 64.,
                glyphs[i].x_offset / 64,
                glyphs[i].y_offset / 64);
    }
}

static void
shaped_run_free(rig_shaped_run_t *run)
{
    c_free(run);
}

static void
shaped_paragraph_free(rig_shaped_paragraph_t *para)
{
    rig_shaped_run_t *run, *tmp;
    c_llist_t *l;

    for (l = para->markup; l; l = l->next)
        rut_object_unref(l->data);
    c_llist_free(para->markup);

    c_list_for_each_safe(run, tmp, &para->shaped_runs, link)
    {
        shaped_run_free(run);
    }

    if (para->wrap_metrics)
        c_free(para->wrap_metrics);

    c_free(para->utf16_text);

    c_slice_free(rig_shaped_paragraph_t, para);
}

static rig_shaped_paragraph_t *
shaped_paragraph_new(shape_context_t *ctx,
                     UChar *utf16_para,
                     int utf16_para_len)
{
    rig_shaped_paragraph_t *shaped_para = c_slice_new(rig_shaped_paragraph_t);
    itemizer_t itemizer;

    shaped_para->utf16_text = utf16_para;

    shaped_para->text_run.start = 0;
    shaped_para->text_run.end = utf16_para_len;

    shaped_para->markup = NULL;

    c_list_init(&shaped_para->shaped_runs);

    shaped_para->wrap_metrics = NULL;

    /* TODO: maybe factor out into an itemizer_init() function */

    itemizer.state = ctx->state;
    itemizer.para_utf16_text = utf16_para;
    itemizer.para_run.start = 0;
    itemizer.para_run.end = utf16_para_len;

    itemizer.ubidi = ubidi_open();

    /* Fixed formatting currently... */
    itemizer.faceset = ctx->faceset;

    /* State machine... */
    itemizer.direction = HB_DIRECTION_LTR;
    itemizer.script = HB_SCRIPT_UNKNOWN;
    itemizer.face = NULL;

    itemizer.run_callback = shape_run_cb;
    itemizer.callback_data = ctx;

    ctx->current_para = shaped_para;

    itemize_paragraph(&itemizer, &itemizer.para_run);

    ubidi_close(itemizer.ubidi);

    return shaped_para;
}

/* For a given run of glyphs, map each glyph cluster back to the
 * original utf16_text and output the cumulative x_advance for all
 * text up until that point.
 *
 * This log can then be used during word wrapping to quickly map word
 * boundary offsets in the utf16 string to an x_advance.
 *
 * The logged total_advance corresponds to the end of the offset, or
 * the start of the next offset.
 */
static void
accumulate_cluster_metrics_cb(rig_shaped_run_t *shaped_run,
                              void *user_data)
{
    rig_cumulative_metric_t *metrics = user_data;
    int n_glyphs = shaped_run->glyph_run.n_glyphs;
    rig_glyph_info_t *glyphs = shaped_run->glyph_run.glyphs;
    int utf16_start = shaped_run->text_run.start;
    hb_position_t total_advance = 0;
    int glyph_pos;
    int utf16_pos;

    /* NB: we can have an N:M relationship betweed utf codepoints and
     * glyphs that belong to the same cluster.
     *
     * The following loop iterates through the glyphs[] array in
     * lockstep with iterating through the text_run array, such that for
     * each grapheme cluster identified in the glyphs[] array we write
     * out metrics for the text_run range that corresponds to that
     * cluster.
     */

    glyph_pos = 0;
    utf16_pos = utf16_start;

    while (glyph_pos < n_glyphs) {
        int32_t glyph_utf16_pos = glyphs[glyph_pos].utf16_pos;
        int32_t next_cluster_utf16_pos;
        hb_position_t cluster_advance = 0;
        hb_position_t cluster_width = 0;
        hb_position_t fraction;
        int len;
        int i;

        /* Scan ahead to the next cluster within glyphs[] */
        for (i = glyph_pos;
             i < n_glyphs && glyphs[i].utf16_pos == glyph_utf16_pos;
             i++) {
            cluster_width += glyphs[i].x_advance;
        }

        if (i < n_glyphs)
            next_cluster_utf16_pos = glyphs[i].utf16_pos;
        else
            next_cluster_utf16_pos = shaped_run->text_run.end;

        glyph_pos = i;

        len = next_cluster_utf16_pos - utf16_pos;
        fraction = cluster_width / len;

        for (i = utf16_pos; i < next_cluster_utf16_pos; i++) {
            metrics[utf16_pos].width = fraction;
            metrics[utf16_pos].total_advance = total_advance + cluster_advance;

            cluster_advance += fraction;
        }

        total_advance += cluster_width;

        utf16_pos = i;
    }
}

static void
get_accumulated_cluster_metrics(rig_shaped_paragraph_t *shaped_para)
{
    rig_cumulative_metric_t *metrics = shaped_para->wrap_metrics;
    rig_shaped_run_t *run;

    c_list_for_each(run, &shaped_para->shaped_runs, link)
    {
        accumulate_cluster_metrics_cb(run, metrics);
    }
}

static hb_position_t
shaped_para_get_utf16_start_px(rig_shaped_paragraph_t *para, int32_t offset)
{
    rig_cumulative_metric_t *metric = &para->wrap_metrics[offset];
    return metric->total_advance - metric->width;
}

static hb_position_t
shaped_para_get_utf16_end_px(rig_shaped_paragraph_t *para,
                             int32_t offset)
{
    rig_cumulative_metric_t *metric = &para->wrap_metrics[offset];
    return metric->total_advance;
}

/* For a given rig_shaped_run_t map a utf16 paragraph offset to a
 * corresponding glyph offset.
 *
 * XXX: The returned index will be out of range if no grapheme cluster
 * corresponding to that offset can be found!
 */
static int
shaped_run_utf16_pos_to_glyph_pos(rig_shaped_run_t *shaped_run,
                                  int32_t utf16_para_pos,
                                  int first_glyph)
{
    rig_glyph_info_t *glyphs = shaped_run->glyph_run.glyphs;
    int start = first_glyph;
    int end = shaped_run->glyph_run.n_glyphs;
    int n_glyphs = end - start;
    int pos;
    int i;

    c_return_val_if_fail(n_glyphs, start);

    if (n_glyphs <= 1)
        return start;

    pos = start + n_glyphs / 2;

    for (;; ) {
        if (glyphs[pos].utf16_pos < utf16_para_pos) {
            /* Scan forward to the next grapheme cluster */
            int32_t current_glyph_pos = glyphs[pos].utf16_pos;
            for (i = pos + 1;
                 i < n_glyphs && glyphs[i].utf16_pos == current_glyph_pos;
                 i++)
                ;
            start = i;
        } else {
            /* Scan back to the start of the grapheme cluster */
            for (i = pos;
                 pos >= 0 && glyphs[i].utf16_pos == glyphs[pos].utf16_pos;
                 i--)
                ;
            pos = i + 1;

            if (glyphs[pos].utf16_pos == utf16_para_pos)
                return pos;
            else
                end = pos;
        }

        pos = start + (end - start) / 2;

        if (pos == start) {
            /* There is no exact mapping so we return the end position */
            return end;
        }
    }
}

/* For a given wrap position P that we return, the exact break point
 * precedes P - i.e. is between [P-1] and [P]. The returned pixel
 * advance measures the glyphs up to and including [P-1], not
 * including P. P may overrun the length of the utf16 text to
 * represent a break at the very end of the text.
 */
static void
shaped_run_find_next_wrap_pos(wrap_state_t *wrap_state,
                              int32_t *utf16_para_pos,
                              hb_position_t *advance_px_pos)
{
    rig_text_engine_state_t *text_state = wrap_state->state;
    rig_shaped_paragraph_t *para = wrap_state->para;
    rig_cumulative_metric_t *metrics = para->wrap_metrics;
    int current_utf16_pos = *utf16_para_pos;
    int head_utf16_pos = current_utf16_pos;
    int limit_px = *advance_px_pos + wrap_state->wrap_width;
    int tail_utf16_pos = para->text_run.end - 1;
    int full_advance_px = shaped_para_get_utf16_end_px(para, tail_utf16_pos);
    int boundary;

    c_return_if_fail(current_utf16_pos >= para->text_run.start &&
                     current_utf16_pos < para->text_run.end);

    if (full_advance_px <= limit_px) {
        *utf16_para_pos = tail_utf16_pos + 1;
        *advance_px_pos = full_advance_px;
        return;
    }

    current_utf16_pos += ((tail_utf16_pos + 1) - head_utf16_pos) / 2;
    for (;; ) {
        rig_cumulative_metric_t *metric = &metrics[current_utf16_pos];
        int advance_px = metric->total_advance;
        int len;

        if (advance_px > limit_px)
            tail_utf16_pos = current_utf16_pos;
        else if (advance_px < limit_px)
            head_utf16_pos = current_utf16_pos + 1;
        else
            break;

        len = tail_utf16_pos + 1 - head_utf16_pos;
        if (len <= 2) {
            current_utf16_pos = tail_utf16_pos;
            break;
        }
        current_utf16_pos = head_utf16_pos + len / 2;
    }

    boundary = ubrk_preceding(text_state->word_iterator, current_utf16_pos);

    if (boundary == UBRK_DONE || boundary <= *utf16_para_pos) {
        /* In this case we couldn't find a suitable word boundary
         * for wrapping and so we're forced to overrun the wrap
         * width and look forwards for a boundary.
         *
         * NB: As a side effect ubrk_isBoundary() will advance to
         * the next word boundary if the given offset is not a boundary.
         */
        if (!ubrk_isBoundary(text_state->word_iterator, current_utf16_pos))
            current_utf16_pos = ubrk_current(text_state->word_iterator);

        if (current_utf16_pos == UBRK_DONE) {
            *utf16_para_pos = para->text_run.end;
            *advance_px_pos = full_advance_px;
        } else {
            *utf16_para_pos = current_utf16_pos;
            *advance_px_pos =
                shaped_para_get_utf16_start_px(para, *utf16_para_pos);
        }
    } else {
        *utf16_para_pos = boundary;
        *advance_px_pos = shaped_para_get_utf16_start_px(para, boundary);
    }
}

static void
shape_paragraph_cb(UChar *utf16_para, int utf16_para_len, void *user_data)
{
    shape_context_t *ctx = user_data;
    rig_text_engine_t *text_engine = ctx->text_engine;
    rig_shaped_paragraph_t *shaped_para;

    shaped_para = shaped_paragraph_new(ctx, utf16_para, utf16_para_len);
    c_list_insert(text_engine->shaped_paras.prev, &shaped_para->link);
}

static void
foreach_paragraph(rig_text_engine_state_t *state,
                  const char *utf8_text,
                  int utf8_text_len,
                  void (*paragraph_callback)(UChar *utf16_para,
                                             int utf16_para_len,
                                             void *user_data),
                  void *user_data)
{
    int utf8_para_start = 0;
    int32_t utf8_cursor = 0;

    while (utf8_cursor < utf8_text_len) {
        uint32_t codepoint;

        U8_NEXT_OR_FFFD(utf8_text, utf8_cursor, utf8_text_len, codepoint);

        /* XXX: Should we squash multiple paragraph separators? */
        if (u_getIntPropertyValue(codepoint, UCHAR_GENERAL_CATEGORY) ==
            U_PARAGRAPH_SEPARATOR ||
            utf8_cursor == utf8_text_len) {
            /* ICU uses UTF-16 internally so first we need to convert from UTF-8
             */
            UChar *utf16_text;
            int32_t utf16_text_len = 0;
            UErrorCode uerror = U_ZERO_ERROR;

            u_strFromUTF8WithSub(NULL,
                                 0, /* pre-flight */
                                 &utf16_text_len,
                                 utf8_text + utf8_para_start,
                                 utf8_cursor - utf8_para_start,
                                 0xFFFD, /* unicode "REPLACEMENT CHARACTER" */
                                 NULL, /* ignore substitutions */
                                 &uerror);

            /* XXX: We ignore uerror, because ICU will report an
             * overflow even though we were only measuring utf8_text */

            if (!utf16_text_len) {
                c_warning("Failed to calculate UTF-16 length of UTF-8 text");
                continue;
            }

            utf16_text = c_malloc(utf16_text_len * sizeof(UChar));
            uerror = U_ZERO_ERROR;
            u_strFromUTF8WithSub(utf16_text,
                                 utf16_text_len,
                                 NULL,
                                 utf8_text + utf8_para_start,
                                 utf8_cursor - utf8_para_start,
                                 0xFFFD, /* unicode "REPLACEMENT CHARACTER" */
                                 NULL, /* ignore substitutions */
                                 &uerror);

            if (U_FAILURE(uerror)) {
                c_warning("Failed to convert UTF-8 string to UTF-16");
                c_free(utf16_text);
                continue;
            }

            paragraph_callback(utf16_text, utf16_text_len, user_data);

            utf8_para_start = utf8_cursor;
        }
    }
}

/* This splits a shaped paragraph into runs of text that will fit into
 * a single line. */
static void
shaped_para_foreach_line(wrap_state_t *wrap_state,
                         void (*run_callback)(wrap_state_t *wrap_state,
                                              rig_shaped_run_t *shaped_run,
                                              rig_text_run_t *text_run,
                                              rig_glyph_run_t *glyph_run,
                                              hb_position_t advance_px),
                         void (*newline_callback)(wrap_state_t *wrap_state))
{
    rig_shaped_paragraph_t *para = wrap_state->para;
    rig_shaped_run_t *shaped_run;
    rig_shaped_run_t *full_shaped_run;
    int32_t utf16_line_end_pos = 0;
    int glyph_pos = 0;
    hb_position_t advance_px = 0;
    UErrorCode uerror = U_ZERO_ERROR;

    wrap_state->x = 0;
    wrap_state->baseline = 0;
    wrap_state->max_leading = 0;

    ubrk_setText(wrap_state->state->word_iterator,
                 para->utf16_text + para->text_run.start,
                 para->text_run.end - para->text_run.start,
                 &uerror);

    if (U_FAILURE(uerror))
        return;

    shaped_run = c_container_of(para->shaped_runs.next,
                                rig_shaped_run_t, link);

    do {
        int32_t utf16_line_start_pos = utf16_line_end_pos;
        rig_glyph_run_t glyph_run;
        rig_text_run_t text_run;

        shaped_run_find_next_wrap_pos(
            wrap_state, &utf16_line_end_pos, &advance_px);

        /* first handle fitting any partial shaped-run into the line
         * (handled separately because it's extra work to lookup the
         *  end glyph_pos) */
        if (utf16_line_start_pos > shaped_run->text_run.start) {
            int start_glyph_pos = glyph_pos;
            hb_position_t start_px;
            hb_position_t end_px;

            c_warn_if_fail(start_glyph_pos != 0);

            text_run.start = utf16_line_start_pos;
            if (shaped_run->text_run.end <= utf16_line_end_pos) {
                text_run.end = shaped_run->text_run.end;
                glyph_pos = shaped_run->glyph_run.n_glyphs;
            } else {
                text_run.end = utf16_line_end_pos;
                glyph_pos = shaped_run_utf16_pos_to_glyph_pos(
                    shaped_run, text_run.end, glyph_pos);
            }

            glyph_run.glyphs = shaped_run->glyph_run.glyphs + start_glyph_pos;
            glyph_run.n_glyphs = glyph_pos - start_glyph_pos;

            start_px =
                shaped_para_get_utf16_start_px(para, utf16_line_start_pos);
            end_px = shaped_para_get_utf16_end_px(para, text_run.end - 1);

            run_callback(wrap_state,
                         shaped_run,
                         &text_run,
                         &glyph_run,
                         end_px - start_px);

            if (text_run.end == shaped_run->text_run.end) {
                shaped_run =
                    rut_container_of(shaped_run->link.next, shaped_run, link);
                glyph_pos = 0;
            }
        }

        /* If we didn't fill the line then iterate through as many *full*
         * shaped-runs as will fit (these are the cheapest to handle since we
         * don't need to find intra-run glyph positions) */
        for (full_shaped_run = shaped_run;
             (&full_shaped_run->link != &para->shaped_runs &&
              full_shaped_run->text_run.end <= utf16_line_end_pos);
             full_shaped_run = rut_container_of(
                 full_shaped_run->link.next, full_shaped_run, link)) {
            hb_position_t start_px = shaped_para_get_utf16_start_px(
                para, full_shaped_run->text_run.start);
            hb_position_t end_px = shaped_para_get_utf16_end_px(
                para, full_shaped_run->text_run.end - 1);

            glyph_pos = 0;

            run_callback(wrap_state,
                         full_shaped_run,
                         &full_shaped_run->text_run,
                         &full_shaped_run->glyph_run,
                         end_px - start_px);
        }

        /* lastly handle any partial run at the end of the line... */
        shaped_run = full_shaped_run; /* (could be misnomer at this point) */
        if (&shaped_run->link != &para->shaped_runs &&
            shaped_run->text_run.end > utf16_line_end_pos && glyph_pos == 0) {
            hb_position_t start_px = shaped_para_get_utf16_start_px(
                para, shaped_run->text_run.start);

            text_run.start = shaped_run->text_run.start;
            text_run.end = utf16_line_end_pos;

            glyph_pos = shaped_run_utf16_pos_to_glyph_pos(
                shaped_run, utf16_line_end_pos, 0);

            glyph_run.glyphs = shaped_run->glyph_run.glyphs;
            glyph_run.n_glyphs = glyph_pos;

            run_callback(wrap_state,
                         full_shaped_run,
                         &text_run,
                         &glyph_run,
                         advance_px - start_px);
        }

        newline_callback(wrap_state);
    } while (utf16_line_end_pos != para->text_run.end);
}

static void
line_run_cb(wrap_state_t *wrap_state,
            rig_shaped_run_t *shaped_run,
            rig_text_run_t *text_run,
            rig_glyph_run_t *glyph_run,
            hb_position_t advance_px)
{
    rig_fixed_run_t *fixed_run = c_slice_new(rig_fixed_run_t);
    rig_sized_face_t *face = shaped_run->face;
    FT_Face ft_face = rig_sized_face_get_freetype_face(wrap_state->state, face);
    int ppem = ft_face->size->metrics.y_ppem;
    hb_position_t leading;

    fixed_run->shaped_run = shaped_run;
    fixed_run->text_run = *text_run;
    fixed_run->glyph_run = *glyph_run;
    fixed_run->width = advance_px;

    c_debug("fixed run %p:\n");
    c_debug("> text start=%d, end=%d, len=%d\n",
            fixed_run->text_run.start,
            fixed_run->text_run.end,
            fixed_run->text_run.end - fixed_run->text_run.start);
    c_debug("> glyph base=%p, start=%d, end=%d, len=%d\n",
            shaped_run->glyph_run.glyphs,
            (fixed_run->glyph_run.glyphs - shaped_run->glyph_run.glyphs),
            (fixed_run->glyph_run.glyphs - shaped_run->glyph_run.glyphs +
             fixed_run->glyph_run.n_glyphs),
            fixed_run->glyph_run.n_glyphs);

    c_list_insert(wrap_state->unaligned.prev, &fixed_run->link);

    wrap_state->line_advance += advance_px;

    /* Looking at InDesign, they use a default leading of 120% of
     * the em size so hopefully that's a reasonable default for us
     * too... */
    leading = (ppem * 64 * 120) / 100;

    if (leading > wrap_state->max_leading)
        wrap_state->max_leading = leading;
}

static void
position_rtl_runs(wrap_state_t *wrap_state,
                  c_list_t *fixed_runs,
                  rig_fixed_run_t **pos)
{
    rig_fixed_run_t *start = *pos;
    rig_fixed_run_t *fixed_run;
    bool invert = wrap_state->invert;
    hb_position_t wrap_width = wrap_state->wrap_width;
    hb_position_t x;

    for (fixed_run = start;
         (&fixed_run->link != fixed_runs &&
          fixed_run->shaped_run->direction == HB_DIRECTION_RTL);
         fixed_run = rut_container_of(fixed_run->link.next, fixed_run, link)) {
        wrap_state->x += fixed_run->width;
    }

    x = wrap_state->x;
    for (fixed_run = start;
         (&fixed_run->link != fixed_runs &&
          fixed_run->shaped_run->direction == HB_DIRECTION_RTL);
         fixed_run = rut_container_of(fixed_run->link.next, fixed_run, link)) {
        hb_position_t run_width = fixed_run->width;

        x -= run_width;
        fixed_run->x = x;
        fixed_run->baseline = wrap_state->baseline;

        if (invert)
            fixed_run->x = wrap_width - fixed_run->x - run_width;

        *pos = fixed_run;
    }
}

#if 0
/* XXX: What special consideration for rtl text is needed here? */
/* For left alignment we want to ignore leading whitespace */
static void
measure_leading_whitespace()
{
    /* To handle alignment nicely we want to be able to strip white
     * space from the start and end of a line. */
    current_utf16_pos = *utf16_pos;
    do
    {
        uint32_t codepoint;
        U16_PREV_UNSAFE (para->utf16_text, current_utf16_pos, codepoint);

        if (!u_isspace (codepoint) ||
            u_charType (codepoint) == U_CONTROL_CHAR ||
            u_charType (codepoint) == U_NON_SPACING_MARK)
        {
            tail_ink_pos = current_utf16_pos;
            break;
        }
    }
    while (current_utf16_pos > shaped_text_run->start);
}
#endif

/* XXX: What special consideration for rtl text is needed here? */
/* For right alignment we want to ignore trailing whitespace */
static void
measure_trailing_whitespace()
{
}

/* Only at the end of each line do we align all of the runs of text... */
static void
newline_cb(wrap_state_t *wrap_state)
{
    hb_position_t wrap_width = wrap_state->wrap_width;
    bool invert = wrap_state->invert;
    rig_fixed_run_t *fixed_run;

    switch (wrap_state->effective_alignment) {
    case ALIGNMENT_LEFT:
        wrap_state->x = 0;
        break;
    case ALIGNMENT_RIGHT:
        wrap_state->x = wrap_state->wrap_width;
        break;
    case ALIGNMENT_CENTER:
        wrap_state->x = (wrap_state->wrap_width - wrap_state->line_advance) / 2;
        break;
    }

    wrap_state->baseline += wrap_state->max_leading;
    wrap_state->text_engine->height += wrap_state->max_leading;

    c_list_for_each(fixed_run, &wrap_state->unaligned, link)
    {
        hb_direction_t direction = fixed_run->shaped_run->direction;

        if (wrap_state->invert)
            direction = HB_DIRECTION_REVERSE(direction);

        if (HB_DIRECTION_IS_BACKWARD(direction)) {
            position_rtl_runs(wrap_state, &wrap_state->unaligned, &fixed_run);
            /* At this point @fixed_run will correspond to the last
             * RTL run that was sequentially adjacent */
            continue;
        } else {
            hb_position_t run_width = fixed_run->width;

            fixed_run->x = wrap_state->x;
            fixed_run->baseline = wrap_state->baseline;

            if (invert)
                fixed_run->x = wrap_width - fixed_run->x - run_width;

            wrap_state->x += fixed_run->width;
        }
    }

    c_debug("wrapped line: ");
    c_list_for_each(fixed_run, &wrap_state->unaligned, link)
    {
        c_debug("fixed (x=%d): \"", (int)fixed_run->x / 64);
        print_utf16(wrap_state->para->utf16_text + fixed_run->text_run.start,
                    fixed_run->text_run.end - fixed_run->text_run.start);
        c_debug("\"");
    }
    c_debug("\n");

    c_list_insert_list(&wrap_state->wrapped_para->fixed_runs,
                         &wrap_state->unaligned);
    c_list_init(&wrap_state->unaligned);

    wrap_state->max_leading = 0;
    wrap_state->line_advance = 0;
}

static rig_wrapped_paragraph_t *
wrap_paragraph(rig_text_engine_t *text_engine,
               rig_text_engine_state_t *text_state,
               rig_shaped_paragraph_t *para,
               float wrap_width)
{
    rig_wrapped_paragraph_t *wrapped_para;
    wrap_state_t wrap_state;
    int para_len = para->text_run.end - para->text_run.start;
    rig_shaped_run_t *first_run;
    int i;

    wrap_state.state = text_state;
    wrap_state.text_engine = text_engine;

    if (!para->wrap_metrics) {
        para->wrap_metrics =
            c_malloc(sizeof(rig_cumulative_metric_t) * para_len);

        get_accumulated_cluster_metrics(para);
    }

    c_debug("cluster metrics:\n");
    for (i = 0; i < para_len; i++) {
        rig_cumulative_metric_t *metric = &para->wrap_metrics[i];

        if (metric->total_advance)
            c_debug("%i: advance = %d\n", i, (int)(metric->total_advance / 64));
        else
            c_debug("%i: <empty>\n", i);
    }

    /* As we handle each line we build up a list of rig_fixed_run_ts that fit
     * in to the available wrap_width, but it's not until we have a
     * complete list of runs that fit that we can handle alignment */
    c_list_init(&wrap_state.unaligned);

    wrap_state.para = para;

    first_run = c_container_of(para->shaped_runs.next,
                               rig_shaped_run_t, link);
    wrap_state.default_direction = first_run->direction;

    /* For now the only way to affect the alignment is by the initial
     * text direction but later it should be possible to explicitly
     * control this... */
    if (HB_DIRECTION_IS_FORWARD(wrap_state.default_direction))
        wrap_state.alignment = ALIGNMENT_LEFT;
    else
        wrap_state.alignment = ALIGNMENT_RIGHT;

    /* XXX: We consider RTL, right aligned text to be an inversion of
     * LTR, left aligned text, as well as RTL, left aligned being an
     * inversion of LTR, right aligned text. We normalize the effective
     * alignment here and maintain an inversion state.
     */
    if (HB_DIRECTION_IS_BACKWARD(wrap_state.default_direction)) {
        wrap_state.invert = true;
        if (wrap_state.effective_alignment == ALIGNMENT_RIGHT)
            wrap_state.effective_alignment = ALIGNMENT_LEFT;
        else
            wrap_state.effective_alignment = ALIGNMENT_RIGHT;
    } else {
        wrap_state.invert = false;
        wrap_state.effective_alignment = wrap_state.alignment;
    }

    wrap_state.wrap_width = wrap_width * 64;

    wrapped_para = c_slice_new(rig_wrapped_paragraph_t);
    wrapped_para->shaped_para = para;
    wrapped_para->wrap_width = wrap_width;

    c_list_init(&wrapped_para->fixed_runs);

    wrap_state.wrapped_para = wrapped_para;

    shaped_para_foreach_line(&wrap_state, line_run_cb, newline_cb);

    return wrapped_para;
}

static void
fixed_run_free(rig_fixed_run_t *run)
{
    c_slice_free(rig_fixed_run_t, run);
}

static void
wrapped_paragraph_free(rig_wrapped_paragraph_t *para)
{
    rig_fixed_run_t *run, *tmp;

    c_list_for_each_safe(run, tmp, &para->fixed_runs, link)
    {
        fixed_run_free(run);
    }

    c_slice_free(rig_wrapped_paragraph_t, para);
}

static void
queue_wrap(rig_text_engine_t *text_engine)
{
    rig_wrapped_paragraph_t *wrapped_para, *tmp_wrapped_para;

    c_list_for_each_safe(wrapped_para, tmp_wrapped_para,
                           &text_engine->wrapped_paras, link)
    {
        wrapped_paragraph_free(wrapped_para);
    }

    text_engine->needs_wrap = 1;
}

static void
queue_shape(rig_text_engine_t *text_engine)
{
    rig_shaped_paragraph_t *shaped_para, *tmp_shaped_para;

    queue_wrap(text_engine);

    c_list_for_each_safe(shaped_para, tmp_shaped_para,
                           &text_engine->shaped_paras, link)
    {
        shaped_paragraph_free(shaped_para);
    }

    text_engine->needs_shape = 1;
}

static void
_rig_text_engine_free(void *object)
{
    rig_text_engine_t *text_engine = object;

    queue_shape(text_engine);

    rut_closure_list_disconnect_all_FIXME(&text_engine->on_wrap_closures);

    rut_object_free(rig_text_engine_t, text_engine);
}

rut_type_t rig_text_engine_type;

static void
_rig_text_engine_init_type(void)
{
    rut_type_init(&rig_text_engine_type,
                  "rig_text_engine_t", _rig_text_engine_free);
}

rig_text_engine_t *
rig_text_engine_new(rig_text_engine_state_t *text_state)
{
    rig_text_engine_t *text_engine =
      rut_object_alloc(rig_text_engine_t, &rig_text_engine_type,
                       _rig_text_engine_init_type);

    text_engine->utf8_text = "";
    text_engine->utf8_text_len = 0;

    text_engine->markup = NULL;
    text_engine->wrap_width = 512; /* XXX: arbitrary default
                                    * should we default to -1 unwrapped? */

    text_engine->width = text_engine->wrap_width;
    text_engine->height = 0;

    c_list_init(&text_engine->shaped_paras);
    c_list_init(&text_engine->wrapped_paras);

    c_list_init(&text_engine->on_wrap_closures);

    text_engine->needs_shape = 1;
    text_engine->needs_wrap = 1;

    return text_engine;
}

void
rig_text_engine_set_utf8_static(rig_text_engine_t *text_engine,
                                const char *utf8_text,
                                int len)
{
    text_engine->utf8_text = utf8_text;
    text_engine->utf8_text_len = len >= 0 ? len : strlen(utf8_text);
}

void
rig_text_engine_shape(rig_text_engine_state_t *text_state,
                      rig_text_engine_t *text_engine)
{
    shape_context_t ctx;

    if (!text_engine->needs_shape)
        return;

    ctx.state = text_state;
    ctx.text_engine = text_engine;

    ctx.faceset = lookup_faceset(text_state, text_engine);
    c_return_if_fail(ctx.faceset);

    c_list_init(&ctx.shaped_paras);

    ctx.hb_buf = hb_buffer_create();

    foreach_paragraph(ctx.state,
                      text_engine->utf8_text, text_engine->utf8_text_len,
                      shape_paragraph_cb, &ctx);

    hb_buffer_destroy(ctx.hb_buf);

    text_engine->needs_shape = 0;
}

void
rig_text_engine_wrap(rig_text_engine_state_t *text_state,
                     rig_text_engine_t *text_engine)
{
    rig_shaped_paragraph_t *shaped_para;

    if (text_engine->needs_shape)
        rig_text_engine_shape(text_state, text_engine);

    if (!text_engine->needs_wrap)
        return;

    text_engine->width = text_engine->wrap_width;
    text_engine->height = 0;

    c_list_for_each(shaped_para, &text_engine->shaped_paras, link)
    {
        rig_wrapped_paragraph_t *wrapped_para =
            wrap_paragraph(text_engine, text_state,
                           shaped_para, text_engine->wrap_width);
        c_list_insert(text_engine->wrapped_paras.prev, &wrapped_para->link);
    }

    text_engine->needs_wrap = 0;

    rut_closure_list_invoke(&text_engine->on_wrap_closures,
                            rig_text_engine_on_wrap_callback_t,
                            text_engine);
}

struct icu_data_header {
    uint16_t size;
    uint8_t magic0;
    uint8_t magic1;
    UDataInfo info;
};

static bool
check_icu_item(const struct icu_data_header *header,
               UDataMemoryIsAcceptable *is_acceptable_callback,
               void *is_acceptable_callback_data,
               const char *type,
               const char *name)
{
    if(header->magic0 == 0xda &&
       header->magic1 == 0x27 &&
        (is_acceptable_callback == NULL ||
         is_acceptable_callback(is_acceptable_callback_data,
                                type, name, &header->info)))
    {
        return true;
    } else
        return false;
}

struct icu_common_entry
{
    c_quark_t id;
    const void *header;
    size_t len;
};

static void
icu_common_entry_free_cb(void *data)
{
    struct icu_common_entry *entry = data;

    munmap((void *)entry->header, entry->len);

    c_slice_free(struct icu_common_entry, entry);
}

#ifdef __ANDROID__
static const void *
android_asset_manager_open(rig_text_engine_state_t *state,
                           const char *filename)
{
    AAssetManager *manager = state->asset_manager;
    AAsset *asset = AAssetManager_open(manager, filename, AASSET_MODE_BUFFER);
    const void *common_data;

    if (!asset)
        return NULL;

    common_data = AAsset_getBuffer(asset);
    if (!common_data) {
        AAsset_close(asset);
        return NULL;
    }

    return common_data;
}

#else /* __ANDROID__ */

static void *
mmap_open(const char *filename)
{
    struct stat st;
    int fd = open(filename, O_RDONLY | O_CLOEXEC);
    void *common_header = NULL;

    if (fd == -1)
        goto error;

    if (fstat(fd, &st) < 0 || st.st_size <= 0)
        goto error;

    common_header = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!common_header)
        goto error;

    close(fd);
    fd = -1;

    return common_header;
error:

    if (common_header)
        munmap(common_header, st.st_size);

    if (fd > 0)
        close(fd);

    return NULL;
}
#endif /* __ANDROID__ */

static const void *
open_icu_common_data(rig_text_engine_state_t *state,
                     const char *filename)
{
    struct icu_common_entry *common_entry;
    c_quark_t id = c_quark_from_string(filename);
    const void *common_header = NULL;

    common_entry = c_hash_table_lookup(state->icu_common_cache,
                                       C_UINT_TO_POINTER(id));
    if (common_entry)
        return common_entry->header;

#ifdef __ANDROID__
    common_header = android_asset_manager_open(state, filename);
#else
    common_header = mmap_open(filename);
#endif

    common_entry = c_slice_new(struct icu_common_entry);
    common_entry->id = id;
    common_entry->header = common_header;

    c_hash_table_insert(state->icu_common_cache,
                        C_UINT_TO_POINTER(common_entry->id),
                        common_entry);

    return common_header;
}

struct icu_item_entry
{
    c_quark_t id;
    const void *header;
    int32_t len;
};

static void
icu_item_entry_free_cb(void *data)
{
    struct icu_item_entry *entry = data;
    c_slice_free(struct icu_item_entry, entry);
}

static const void *
open_icu_item_data(rig_text_engine_state_t *state,
                   const void *common_header,
                   const char *toc_entry_name,
                   int32_t *len)
{
    struct icu_item_entry *item_entry;
    const void *item_header;
    c_quark_t id = c_quark_from_string(toc_entry_name);
    UErrorCode error_code = U_ZERO_ERROR;

    item_entry = c_hash_table_lookup(state->icu_item_cache,
                                      C_UINT_TO_POINTER(id));
    if (item_entry) {
        *len = item_entry->len;
        return item_entry->header;
    }

    item_header = udata_commonDataLookup(common_header,
                                         toc_entry_name,
                                         len, &error_code);
    if (U_FAILURE(error_code) || !item_header)
        return NULL;

    item_entry = c_slice_new(struct icu_item_entry);
    item_entry->id = id;
    item_entry->header = item_header;
    item_entry->len = *len;

    c_hash_table_insert(state->icu_item_cache,
                        C_UINT_TO_POINTER(id),
                        item_entry);

    return item_header;
}

static void
icu_load_data_cb(UBool is_icu_data,
                 const char *pkg_name,
                 const char *data_path,
                 const char *toc_entry_path_suffix,
                 const char *toc_entry_name,
                 const char *path,
                 const char *type,
                 const char *name,
                 UDataMemoryIsAcceptable *is_acceptable_callback,
                 void *is_acceptable_callback_data,
                 void *user_data,
                 UDataExternalMemory *ext_mem_ret,
                 UErrorCode *error_code)
{
    rig_text_engine_state_t *state = user_data;
    char *dat_name;
    char *filename;
    const void *common_header = NULL;
    const void *item_header = NULL;
    int len = 0;

    if (U_FAILURE(*error_code))
        return;

    dat_name = c_strconcat(pkg_name, ".dat", NULL);
    filename = c_build_filename(ICU_DATA_DIR, dat_name, NULL);
    c_free(dat_name);

    common_header = open_icu_common_data(state, filename);
    if (!common_header)
        goto error;

    item_header = open_icu_item_data(state, common_header,
                                     toc_entry_name,
                                     &len);
    if (!item_header)
        goto error;

    if (!check_icu_item(item_header,
                        is_acceptable_callback,
                        is_acceptable_callback_data,
                        type,
                        name))
    {
        goto error;
    }

    ext_mem_ret->header = item_header;
    ext_mem_ret->length = len;
    ext_mem_ret->destroyCallback = NULL;
    ext_mem_ret->destroyData = NULL;

    return;

error:
    c_free(filename);

    *error_code = U_FILE_ACCESS_ERROR;
    return;
}

rig_text_engine_state_t *
rig_text_engine_state_new(rig_engine_t *engine)
{
    rig_text_engine_state_t *state = c_slice_new0(rig_text_engine_state_t);
    UErrorCode uerror = U_ZERO_ERROR;

    state->icu_common_cache = c_hash_table_new_full(c_direct_hash,
                                                    c_direct_equal,
                                                    NULL, /* key destroy */
                                                    icu_common_entry_free_cb);
    state->icu_item_cache = c_hash_table_new_full(c_direct_hash,
                                                  c_direct_equal,
                                                  NULL, /* key destroy */
                                                  icu_item_entry_free_cb);
    udata_setLoadCallback(icu_load_data_cb, state);

    state->hb_font_funcs = hb_font_funcs_create();

    /* XXX: I think ideally Harfbuzz would let us read the freetype
     * based funcs it provides when deriving a harfbuzz font from a
     * freetype face so we could just provide our own _get_glyph()
     * function, but for now we have copied the freetype based funcs
     * from harfbuzz into rig-text-renderer-funcs.c and have to
     * explicitly set them all...
     */
    hb_font_funcs_set_glyph_contour_point_func(
        state->hb_font_funcs,
        rig_text_engine_get_glyph_contour_point,
        NULL, /* user data */
        NULL); /* destroy */
    hb_font_funcs_set_glyph_extents_func(state->hb_font_funcs,
                                         rig_text_engine_get_glyph_extents,
                                         NULL, /* user data */
                                         NULL); /* destroy */
    hb_font_funcs_set_glyph_from_name_func(state->hb_font_funcs,
                                           rig_text_engine_get_glyph_from_name,
                                           NULL, /* user data */
                                           NULL); /* destroy */
    hb_font_funcs_set_glyph_h_advance_func(state->hb_font_funcs,
                                           rig_text_engine_get_glyph_h_advance,
                                           NULL, /* user data */
                                           NULL); /* destroy */
    hb_font_funcs_set_glyph_h_kerning_func(state->hb_font_funcs,
                                           rig_text_engine_get_glyph_h_kerning,
                                           NULL, /* user data */
                                           NULL); /* destroy */
    hb_font_funcs_set_glyph_h_origin_func(state->hb_font_funcs,
                                          rig_text_engine_get_glyph_h_origin,
                                          NULL, /* user data */
                                          NULL); /* destroy */
    hb_font_funcs_set_glyph_name_func(state->hb_font_funcs,
                                      rig_text_engine_get_glyph_name,
                                      NULL, /* user data */
                                      NULL); /* destroy */
    hb_font_funcs_set_glyph_v_advance_func(state->hb_font_funcs,
                                           rig_text_engine_get_glyph_v_advance,
                                           NULL, /* user data */
                                           NULL); /* destroy */
    hb_font_funcs_set_glyph_v_kerning_func(state->hb_font_funcs,
                                           rig_text_engine_get_glyph_v_kerning,
                                           NULL, /* user data */
                                           NULL); /* destroy */
    hb_font_funcs_set_glyph_v_origin_func(state->hb_font_funcs,
                                          rig_text_engine_get_glyph_v_origin,
                                          NULL, /* user data */
                                          NULL); /* destroy */

    state->fc_config = FcInitLoadConfigAndFonts();

    if (FT_Init_FreeType(&state->ft_library)) {
        c_critical("Failed to initialize freetype");
    }

    state->pattern_singletons =
        c_hash_table_new_full((c_hash_func_t)FcPatternHash,
                              (c_equal_func_t)FcPatternEqual,
                              (c_destroy_func_t)FcPatternDestroy,
                              NULL);

    state->facesets_hash =
        c_hash_table_new_full(c_direct_hash,
                              c_direct_equal, /* compare singleton patterns */
                              NULL, /* key destroy */
                              faceset_free);

    state->sized_face_hash =
        c_hash_table_new_full(c_direct_hash,
                              c_direct_equal, /* compare singleton patterns */
                              NULL, /* key destroy */
                              sized_face_free);

    state->shared_face_hash =
        c_hash_table_new_full((c_hash_func_t)shared_face_hash,
                              (c_equal_func_t)shared_face_equal,
                              (c_destroy_func_t)shared_face_free,
                              NULL);

    state->word_iterator = ubrk_open(UBRK_LINE,
                                     0, /* locale? */
                                     NULL, /* text */
                                     -1, /* len */
                                     &uerror);

    return state;
}

void
rig_text_engine_state_destroy(rig_text_engine_state_t *state)
{
    hb_font_funcs_destroy(state->hb_font_funcs);

    c_hash_table_destroy(state->sized_face_hash);
    c_hash_table_destroy(state->facesets_hash);
    c_hash_table_destroy(state->pattern_singletons);

    ubrk_close(state->word_iterator);
    c_hash_table_destroy(state->icu_item_cache);
    c_hash_table_destroy(state->icu_common_cache);

    c_slice_free(rig_text_engine_state_t, state);
}

static void
_init_markup_type(rut_type_t *type,
                  const char *name,
                  size_t markup_offset,
                  rut_type_destructor_t destructor)
{
    rut_ensure_trait_id(&rig_markup_trait_id);

    rut_type_init(type, name, destructor);
    rut_type_add_trait(type,
                       rig_markup_trait_id,
                       markup_offset,
                       NULL); /* vtable */
}

#define init_markup_type(type) \
  _init_markup_type(&type##_type, C_STRINGIFY(type##_t), \
                    offsetof(type##_t, markup), \
                    _##type##_free)


static void
_rig_family_markup_free(void *object)
{
    rig_family_markup_t *family_markup = object;

    c_free(family_markup->family);

    rut_object_free(rig_family_markup_t, object);
}

rut_type_t rig_family_markup_type;

static void
_rig_family_markup_init_type(void)
{
    init_markup_type(rig_family_markup);
}

rig_family_markup_t *
rig_family_markup_new(int start,
                      int end,
                      const char *family)
{
    rig_family_markup_t *family_markup;

    c_return_val_if_fail(family, NULL);

    family_markup = rut_object_alloc(rig_family_markup_t,
                                     &rig_family_markup_type,
                                     _rig_family_markup_init_type);

    family_markup->markup.start = start;
    family_markup->markup.end = end;

    family_markup->family = c_strdup(family);

    return family_markup;
}

static void
_rig_size_markup_free(void *object)
{
    rut_object_free(rig_size_markup_t, object);
}

rut_type_t rig_size_markup_type;

static void
_rig_size_markup_init_type(void)
{
    init_markup_type(rig_size_markup);
}

rig_size_markup_t *
rig_size_markup_new(int start,
                    int end,
                    int size)
{
    rig_size_markup_t *size_markup;

    c_return_val_if_fail(size, NULL);

    size_markup = rut_object_alloc(rig_size_markup_t,
                                   &rig_size_markup_type,
                                   _rig_size_markup_init_type);

    size_markup->markup.start = start;
    size_markup->markup.end = end;

    size_markup->size = size;

    return size_markup;
}

static int
compare_markup_pos_cb(const void *v0, const void *v1)
{
    rig_markup_t *markup0 = rut_object_get_properties(v0, rig_markup_trait_id);
    rig_markup_t *markup1 = rut_object_get_properties(v1, rig_markup_trait_id);

    return markup0->start - markup1->start;
}

void
rig_text_engine_add_markup(rig_text_engine_t *text_engine, rut_object_t *markup)
{
    text_engine->markup = c_llist_insert_sorted(text_engine->markup,
                                               markup,
                                               compare_markup_pos_cb);
}

void
rig_shaped_paragraph_add_markup(rig_shaped_paragraph_t *para, rut_object_t *markup)
{
    para->markup = c_llist_insert_sorted(para->markup,
                                        markup,
                                        compare_markup_pos_cb);
}

rut_closure_t *
rig_text_engine_add_on_wrap_callback(rig_text_engine_t *text_engine,
                                     rig_text_engine_on_wrap_callback_t callback,
                                     void *user_data,
                                     rut_closure_destroy_callback_t destroy)
{
    return rut_closure_list_add_FIXME(&text_engine->on_wrap_closures,
                                callback, user_data, destroy);
}
