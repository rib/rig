/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008 OpenedHand
 * Copyright (C) 2012 Intel Corporation.
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
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 *   Matthew Allum  <mallum@openedhand.com>
 */

#ifndef __CG_PANGO_H__
#define __CG_PANGO_H__

#include <glib-object.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

/* XXX: Currently this header may be included both as an internal
 * header (within the cogl-pango implementation) and as a public
 * header.
 *
 * Since <cogl/cogl.h> should not be included for internal use we
 * determine the current context and switch between including cogl.h
 * or specific internal cogl headers here...
 */
#ifndef CG_COMPILATION
#include <cogl/cogl.h>
#else
#include "cogl/cogl-context.h"
#endif

CG_BEGIN_DECLS

/* It's too difficult to actually subclass the pango cairo font
 * map. Instead we just make a fake set of macros that actually just
 * directly use the original type
 */
#define CG_PANGO_TYPE_FONT_MAP PANGO_TYPE_CAIRO_FONT_MAP
#define CG_PANGO_FONT_MAP(obj)                                                 \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), CG_PANGO_TYPE_FONT_MAP, CgPangoFontMap))
#define CG_PANGO_IS_FONT_MAP(obj)                                              \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), CG_PANGO_TYPE_FONT_MAP))

typedef PangoCairoFontMap CgPangoFontMap;

/**
 * cg_pango_font_map_new:
 * @context: A #cg_context_t
 *
 * Creates a new font map.
 *
 * Return value: (transfer full): the newly created #PangoFontMap
 *
 */
PangoFontMap *cg_pango_font_map_new(cg_context_t *context);

/**
 * cg_pango_font_map_set_resolution:
 * @font_map: a #CgPangoFontMap
 * @dpi: The resolution in "dots per inch". (Physical inches aren't
 *       actually involved; the terminology is conventional.)
 *
 * Sets the resolution for the @font_map. This is a scale factor
 * between points specified in a #PangoFontDescription and Cogl units.
 * The default value is %96, meaning that a 10 point font will be 13
 * units high. (10 * 96. / 72. = 13.3).
 *
 */
void cg_pango_font_map_set_resolution(CgPangoFontMap *font_map, double dpi);

/**
 * cg_pango_font_map_clear_glyph_cache:
 * @font_map: a #CgPangoFontMap
 *
 * Clears the glyph cache for @font_map.
 *
 */
void cg_pango_font_map_clear_glyph_cache(CgPangoFontMap *font_map);

/**
 * cg_pango_ensure_glyph_cache_for_layout:
 * @layout: A #PangoLayout
 *
 * This updates any internal glyph cache textures as necessary to be
 * able to render the given @layout.
 *
 * This api should be used to avoid mid-scene modifications of
 * glyph-cache textures which can lead to undefined rendering results.
 *
 */
void cg_pango_ensure_glyph_cache_for_layout(PangoLayout *layout);

/**
 * cg_pango_font_map_set_use_mipmapping:
 * @font_map: a #CgPangoFontMap
 * @value: %true to enable the use of mipmapping
 *
 * Sets whether the renderer for the passed font map should use
 * mipmapping when rendering a #PangoLayout.
 *
 */
void cg_pango_font_map_set_use_mipmapping(CgPangoFontMap *font_map, bool value);

/**
 * cg_pango_font_map_get_use_mipmapping:
 * @font_map: a #CgPangoFontMap
 *
 * Retrieves whether the #CgPangoRenderer used by @font_map will use
 * mipmapping when rendering the glyphs.
 *
 * Return value: %true if mipmapping is used, %false otherwise.
 *
 */
bool cg_pango_font_map_get_use_mipmapping(CgPangoFontMap *font_map);

/**
 * cg_pango_show_layout:
 * @framebuffer: A #cg_framebuffer_t to draw too.
 * @layout: a #PangoLayout
 * @x: X coordinate to render the layout at
 * @y: Y coordinate to render the layout at
 * @color: color to use when rendering the layout
 *
 * Draws a solidly coloured @layout on the given @framebuffer at (@x,
 * @y) within the @framebuffer<!-- -->'s current model-view coordinate
 * space.
 *
 */
void cg_pango_show_layout(cg_framebuffer_t *framebuffer,
                          PangoLayout *layout,
                          float x,
                          float y,
                          const cg_color_t *color);

/**
 * cg_pango_render_layout_line:
 * @framebuffer: A #cg_framebuffer_t to draw too.
 * @line: a #PangoLayoutLine
 * @x: X coordinate to render the line at
 * @y: Y coordinate to render the line at
 * @color: color to use when rendering the line
 *
 * Draws a solidly coloured @line on the given @framebuffer at (@x,
 * @y) within the @framebuffer<!-- -->'s current model-view coordinate
 * space.
 *
 */
void cg_pango_show_layout_line(cg_framebuffer_t *framebuffer,
                               PangoLayoutLine *line,
                               float x,
                               float y,
                               const cg_color_t *color);

CG_END_DECLS

#endif /* __CG_PANGO_H__ */
