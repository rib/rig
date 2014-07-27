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
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 *   Matthew Allum  <mallum@openedhand.com>
 */

#ifndef __CG_PANGO_PRIVATE_H__
#define __CG_PANGO_PRIVATE_H__

#include "cogl-pango.h"

CG_BEGIN_DECLS

#define CG_PANGO_TYPE_RENDERER (_cg_pango_renderer_get_type())
#define CG_PANGO_RENDERER(obj)                                                 \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), CG_PANGO_TYPE_RENDERER, CgPangoRenderer))
#define CG_PANGO_RENDERER_CLASS(klass)                                         \
    (G_TYPE_CHECK_CLASS_CAST(                                                  \
         (klass), CG_PANGO_TYPE_RENDERER, CgPangoRendererClass))
#define CG_PANGO_IS_RENDERER(obj)                                              \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), CG_PANGO_TYPE_RENDERER))
#define CG_PANGO_IS_RENDERER_CLASS(klass)                                      \
    (G_TYPE_CHECK_CLASS_TYPE((klass), CG_PANGO_TYPE_RENDERER))
#define CG_PANGO_RENDERER_GET_CLASS(obj)                                       \
    (G_TYPE_INSTANCE_GET_CLASS(                                                \
         (obj), CG_PANGO_TYPE_RENDERER, CgPangoRendererClass))

/* opaque types */
typedef struct _CgPangoRenderer CgPangoRenderer;
typedef struct _CgPangoRendererClass CgPangoRendererClass;

GType _cg_pango_renderer_get_type(void) G_GNUC_CONST;

PangoRenderer *_cg_pango_renderer_new(cg_device_t *context);

void _cg_pango_renderer_clear_glyph_cache(CgPangoRenderer *renderer);

void _cg_pango_renderer_set_use_mipmapping(CgPangoRenderer *renderer,
                                           bool value);
bool _cg_pango_renderer_get_use_mipmapping(CgPangoRenderer *renderer);

cg_device_t *_cg_pango_font_map_get_cg_device(CgPangoFontMap *fm);

PangoRenderer *_cg_pango_font_map_get_renderer(CgPangoFontMap *fm);

CG_END_DECLS

#endif /* __CG_PANGO_PRIVATE_H__ */
