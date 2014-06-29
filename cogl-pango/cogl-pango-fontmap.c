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

/**
 * SECTION:cogl-pango
 * @short_description: COGL-based text rendering using Pango
 *
 * FIXME
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* This is needed to get the Pango headers to export stuff needed to
   subclass */
#ifndef PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_BACKEND 1
#endif

#include <pango/pango-fontmap.h>
#include <pango/pangocairo.h>
#include <pango/pango-renderer.h>

#include "cogl-pango.h"
#include "cogl-pango-private.h"
#include "cogl-util.h"

static GQuark cg_pango_font_map_get_priv_key(void) G_GNUC_CONST;

typedef struct _CgPangoFontMapPriv {
    cg_context_t *ctx;
    PangoRenderer *renderer;
} CgPangoFontMapPriv;

static void
free_priv(gpointer data)
{
    CgPangoFontMapPriv *priv = data;

    g_object_unref(priv->renderer);

    g_free(priv);
}

PangoFontMap *
cg_pango_font_map_new(cg_context_t *context)
{
    PangoFontMap *fm = pango_cairo_font_map_new();
    CgPangoFontMapPriv *priv = g_new0(CgPangoFontMapPriv, 1);

    priv->ctx = context;

    /* XXX: The public pango api doesn't let us sub-class
     * PangoCairoFontMap so we attach our own private data using qdata
     * for now. */
    g_object_set_qdata_full(
        G_OBJECT(fm), cg_pango_font_map_get_priv_key(), priv, free_priv);

    return fm;
}

static CgPangoFontMapPriv *
_cg_pango_font_map_get_priv(CgPangoFontMap *fm)
{
    return g_object_get_qdata(G_OBJECT(fm), cg_pango_font_map_get_priv_key());
}

PangoRenderer *
_cg_pango_font_map_get_renderer(CgPangoFontMap *fm)
{
    CgPangoFontMapPriv *priv = _cg_pango_font_map_get_priv(fm);
    if (G_UNLIKELY(!priv->renderer))
        priv->renderer = _cg_pango_renderer_new(priv->ctx);
    return priv->renderer;
}

cg_context_t *
_cg_pango_font_map_get_cg_context(CgPangoFontMap *fm)
{
    CgPangoFontMapPriv *priv = _cg_pango_font_map_get_priv(fm);
    return priv->ctx;
}

void
cg_pango_font_map_set_resolution(CgPangoFontMap *font_map, double dpi)
{
    _CG_RETURN_IF_FAIL(CG_PANGO_IS_FONT_MAP(font_map));

    pango_cairo_font_map_set_resolution(PANGO_CAIRO_FONT_MAP(font_map), dpi);
}

void
cg_pango_font_map_clear_glyph_cache(CgPangoFontMap *fm)
{
    PangoRenderer *renderer = _cg_pango_font_map_get_renderer(fm);

    _cg_pango_renderer_clear_glyph_cache(CG_PANGO_RENDERER(renderer));
}

void
cg_pango_font_map_set_use_mipmapping(CgPangoFontMap *fm, bool value)
{
    PangoRenderer *renderer = _cg_pango_font_map_get_renderer(fm);

    _cg_pango_renderer_set_use_mipmapping(CG_PANGO_RENDERER(renderer), value);
}

bool
cg_pango_font_map_get_use_mipmapping(CgPangoFontMap *fm)
{
    PangoRenderer *renderer = _cg_pango_font_map_get_renderer(fm);

    return _cg_pango_renderer_get_use_mipmapping(CG_PANGO_RENDERER(renderer));
}

static GQuark
cg_pango_font_map_get_priv_key(void)
{
    static GQuark priv_key = 0;

    if (G_UNLIKELY(priv_key == 0))
        priv_key = g_quark_from_static_string("CgPangoFontMap");

    return priv_key;
}
