/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008 OpenedHand
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

#ifndef __CG_PANGO_GLYPH_CACHE_H__
#define __CG_PANGO_GLYPH_CACHE_H__

#include <glib.h>
#include <pango/pango-font.h>

#include "cogl/cogl-texture.h"
#include "cogl/cogl-atlas.h"

CG_BEGIN_DECLS

typedef struct _cg_pango_glyph_cache_t cg_pango_glyph_cache_t;
typedef struct _cg_pango_glyph_cache_value_t cg_pango_glyph_cache_value_t;

struct _cg_pango_glyph_cache_value_t {
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

typedef void (*cg_pango_glyph_cache_dirty_func_t)(
    PangoFont *font, PangoGlyph glyph, cg_pango_glyph_cache_value_t *value);

cg_pango_glyph_cache_t *cg_pango_glyph_cache_new(cg_device_t *dev,
                                                 bool use_mipmapping);

void cg_pango_glyph_cache_free(cg_pango_glyph_cache_t *cache);

cg_pango_glyph_cache_value_t *
cg_pango_glyph_cache_lookup(cg_pango_glyph_cache_t *cache,
                            bool create,
                            PangoFont *font,
                            PangoGlyph glyph);

void cg_pango_glyph_cache_clear(cg_pango_glyph_cache_t *cache);

void _cg_pango_glyph_cache_add_reorganize_callback(
    cg_pango_glyph_cache_t *cache, GHookFunc func, void *user_data);

void _cg_pango_glyph_cache_remove_reorganize_callback(
    cg_pango_glyph_cache_t *cache, GHookFunc func, void *user_data);

void
_cg_pango_glyph_cache_set_dirty_glyphs(cg_pango_glyph_cache_t *cache,
                                       cg_pango_glyph_cache_dirty_func_t func);

CG_END_DECLS

#endif /* __CG_PANGO_GLYPH_CACHE_H__ */
