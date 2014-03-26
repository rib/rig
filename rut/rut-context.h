/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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

#ifndef _RUT_CONTEXT_H_
#define _RUT_CONTEXT_H_

#include <cogl-pango/cogl-pango.h>

#include "rut-object.h"
#include "rut-shell.h"
#include "rut-property.h"
#include "rut-closure.h"
#include "rut-matrix-stack.h"

/* TODO: This header needs to be split up, since most of the APIs here
 * don't relate directly to the RutContext type. */

#define RUT_UINT32_RED_AS_FLOAT(COLOR)   (((COLOR & 0xff000000) >> 24) / 255.0)
#define RUT_UINT32_GREEN_AS_FLOAT(COLOR) (((COLOR & 0xff0000) >> 16) / 255.0)
#define RUT_UINT32_BLUE_AS_FLOAT(COLOR)  (((COLOR & 0xff00) >> 8) / 255.0)
#define RUT_UINT32_ALPHA_AS_FLOAT(COLOR) ((COLOR & 0xff) / 255.0)


extern uint8_t _rut_nine_slice_indices_data[54];

/*
 * Note: The size and padding for this circle texture have been carefully
 * chosen so it has a power of two size and we have enough padding to scale
 * down the circle to a size of 2 pixels and still have a 1 texel transparent
 * border which we rely on for anti-aliasing.
 */
#define CIRCLE_TEX_RADIUS 256
#define CIRCLE_TEX_PADDING 256

typedef enum
{
  RUT_TEXT_DIRECTION_LEFT_TO_RIGHT = 1,
  RUT_TEXT_DIRECTION_RIGHT_TO_LEFT
} RutTextDirection;

typedef struct _RutSettings RutSettings;

/* TODO Make internals private */
struct _RutContext
{
  RutObjectBase _base;

  /* If true then this process does not handle input events directly
   * or output graphics directly. */
  bool headless;

  RutShell *shell;

  RutSettings *settings;

  RutMatrixEntry identity_entry;

  CoglContext *cogl_context;

  CoglMatrix identity_matrix;

  char *assets_location;

  GHashTable *texture_cache;

  CoglIndices *nine_slice_indices;

  CoglTexture *circle_texture;

  GHashTable *colors_hash;

  CoglPangoFontMap *pango_font_map;
  PangoContext *pango_context;
  PangoFontDescription *pango_font_desc;

  RutPropertyContext property_ctx;

  CoglPipeline *single_texture_2d_template;

  GSList *timelines;
};

G_BEGIN_DECLS

RutContext *
rut_context_new (RutShell *shell /* optional */);

void
rut_context_init (RutContext *context);

RutTextDirection
rut_get_text_direction (RutContext *context);

void
rut_set_assets_location (RutContext *context,
                         const char *assets_location);

typedef void (*RutSettingsChangedCallback) (RutSettings *settings,
                                            void *user_data);

void
rut_settings_add_changed_callback (RutSettings *settings,
                                   RutSettingsChangedCallback callback,
                                   GDestroyNotify destroy_notify,
                                   void *user_data);

void
rut_settings_remove_changed_callback (RutSettings *settings,
                                      RutSettingsChangedCallback callback);

unsigned int
rut_settings_get_password_hint_time (RutSettings *settings);

char *
rut_settings_get_font_name (RutSettings *settings);

CoglTexture *
rut_load_texture (RutContext *ctx, const char *filename, CoglError **error);

CoglTexture *
rut_load_texture_from_data_file (RutContext *ctx,
                                 const char *filename,
                                 GError **error);

char *
rut_find_data_file (const char *base_filename);

CoglTexture *
_rut_load_texture (RutContext *ctx, const char *filename, CoglError **error);

G_END_DECLS

#endif /* _RUT_CONTEXT_H_ */
