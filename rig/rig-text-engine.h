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

#ifndef _RIG_TEXT_ENGINE_H_
#define _RIG_TEXT_ENGINE_H_

typedef struct _rig_text_engine_t rig_text_engine_t;

#include "rut-object.h"
#include "components/rig-text.h"

/* A TextEngine is text processing engine that can handle splitting
 * text into runs with a consistent language, script, format direction
 * etc, before shaping those runs and handling wrapping and alignment.
 */

/* State, such as caches that are shared by all text engines... */
typedef struct _rig_text_engine_state_t rig_text_engine_state_t;

typedef struct _rig_glyph_index_entry_t rig_glyph_index_entry_t;

typedef struct _rig_glyph_info_t rig_glyph_info_t;

typedef struct _rig_sized_face_t rig_sized_face_t;

typedef struct _rig_shared_face_t rig_shared_face_t;

/* A requested font-family + weight + size etc gets turned into a
 * fontconfig pattern that's used to find a set of specific faces than
 * can be used to handle the requested style.
 */
typedef struct _rig_sized_face_set_t rig_sized_face_set_t;

/* A glyph run refers to a run of shaped glyphs.
 *
 * Note that a glyph run might refer to a shorter run of glyphs
 * within another glyph run.
 *
 * For example the glyph runs calculated when shaping text may later
 * need to be split into shorter runs when wrapping text.
 */
typedef struct rig_glyph_run_t {
    rig_glyph_info_t *glyphs;
    int n_glyphs;
} rig_glyph_run_t;

/* text runs should all be in logical, not visual order so end should
 * always be greater than start */
#define text_run_len(X) (X->end - X->start)

/* A text run delimits a run of text between two offsets within a
 * larger utf string. */
typedef struct _rig_text_run_t {
    int32_t start;
    int32_t end;
} rig_text_run_t;

/* An item relates a run of shaped glyphs to the original input
 * text. This can be referenced when handling wrapping if we need
 * to look for word boundaries in the original text. */
typedef struct _rig_shaped_run_t rig_shaped_run_t;

typedef struct _rig_cumulative_metric_t rig_cumulative_metric_t;

/* This is a paragraph of text that has been split into runs of
 * text with a common BiDi direction, script and font and each run
 * has been shaped with Harfbuzz giving a list of positioned glyphs
 *
 * No consideration of layouting and wrapping has been made yet.
 */
typedef struct _rig_shaped_paragraph_t rig_shaped_paragraph_t;

typedef struct _rig_fixed_run_t rig_fixed_run_t;

/* This is a shaped paragraph that has been layed out by wrapping the
 * text within a fixed width, resulting in a list of positioned glyph
 * runs. */
typedef struct _rig_wrapped_paragraph_t rig_wrapped_paragraph_t;

#define round_26_6(X) ((X + 32) & ~63)
#define floor_26_6(X) (X & ~63)
#define ceil_26_6(X) (X + 63) & ~63

extern rut_type_t rig_text_engine_type;

rig_text_engine_state_t *rig_text_engine_state_new(rig_engine_t *engine);

void rig_text_engine_state_destroy(rig_text_engine_state_t *text_state);

rig_text_engine_t *rig_text_engine_new(rig_text_engine_state_t *text_state);

void rig_text_engine_set_utf8_static(rig_text_engine_t *text_engine,
                                     const char *utf8_text,
                                     int len);

void rig_text_engine_set_wrap_width(rig_text_engine_t *text_engine,
                                    int width);
void rig_text_engine_wrap(rig_text_engine_state_t *text_state,
                          rig_text_engine_t *text_engine);

typedef struct _rig_markup_t {
    int start;
    int end;
} rig_markup_t;

extern int rig_markup_trait_id;

typedef struct _rig_family_markup_t rig_family_markup_t;
extern rut_type_t rig_family_markup_type;

rig_family_markup_t *
rig_family_markup_new(int start,
                      int end,
                      const char *family);

typedef struct _rig_size_markup_t rig_size_markup_t;
extern rut_type_t rig_size_markup_type;

rig_size_markup_t *
rig_size_markup_new(int start,
                    int end,
                    int size);

void
rig_text_engine_add_markup(rig_text_engine_t *text_engine,
                           rut_object_t *markup);

void
rig_shaped_paragraph_add_markup(rig_shaped_paragraph_t *para,
                                rut_object_t *markup);

typedef void (*rig_text_engine_on_wrap_callback_t)(rig_text_engine_t *engine,
                                                   void *user_data);

rut_closure_t *
rig_text_engine_add_on_wrap_callback(rig_text_engine_t *text_engine,
                                     rig_text_engine_on_wrap_callback_t callback,
                                     void *user_data,
                                     rut_closure_destroy_callback_t destroy);

#endif /* _RIG_TEXT_ENGINE_H_ */
