/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008 OpenedHand
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 *   Matthew Allum  <mallum@openedhand.com>
 */

#ifndef __COGL_PANGO_PRIVATE_H__
#define __COGL_PANGO_PRIVATE_H__

#include "cogl-pango.h"

COGL_BEGIN_DECLS

#define COGL_PANGO_TYPE_RENDERER                (_cogl_pango_renderer_get_type ())
#define COGL_PANGO_RENDERER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), COGL_PANGO_TYPE_RENDERER, CoglPangoRenderer))
#define COGL_PANGO_RENDERER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), COGL_PANGO_TYPE_RENDERER, CoglPangoRendererClass))
#define COGL_PANGO_IS_RENDERER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), COGL_PANGO_TYPE_RENDERER))
#define COGL_PANGO_IS_RENDERER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass),  COGL_PANGO_TYPE_RENDERER))
#define COGL_PANGO_RENDERER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), COGL_PANGO_TYPE_RENDERER, CoglPangoRendererClass))

/* opaque types */
typedef struct _CoglPangoRenderer      CoglPangoRenderer;
typedef struct _CoglPangoRendererClass CoglPangoRendererClass;

GType
_cogl_pango_renderer_get_type (void) G_GNUC_CONST;

PangoRenderer *
_cogl_pango_renderer_new (CoglContext *context);

void
_cogl_pango_renderer_clear_glyph_cache  (CoglPangoRenderer *renderer);

void
_cogl_pango_renderer_set_use_mipmapping (CoglPangoRenderer *renderer,
                                         CoglBool value);
CoglBool
_cogl_pango_renderer_get_use_mipmapping (CoglPangoRenderer *renderer);



CoglContext *
_cogl_pango_font_map_get_cogl_context (CoglPangoFontMap *fm);

PangoRenderer *
_cogl_pango_font_map_get_renderer (CoglPangoFontMap *fm);

COGL_END_DECLS

#endif /* __COGL_PANGO_PRIVATE_H__ */
