/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009  Intel Corporation.
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

#ifndef __CG_PANGO_DISPLAY_LIST_H__
#define __CG_PANGO_DISPLAY_LIST_H__

#include <glib.h>
#include "cogl-pango-pipeline-cache.h"

CG_BEGIN_DECLS

typedef struct _cg_pango_display_list_t cg_pango_display_list_t;

cg_pango_display_list_t *
_cg_pango_display_list_new(cg_pango_pipeline_cache_t *);

void _cg_pango_display_list_set_color_override(cg_pango_display_list_t *dl,
                                               const cg_color_t *color);

void _cg_pango_display_list_remove_color_override(cg_pango_display_list_t *dl);

void _cg_pango_display_list_add_texture(cg_pango_display_list_t *dl,
                                        cg_texture_t *texture,
                                        float x_1,
                                        float y_1,
                                        float x_2,
                                        float y_2,
                                        float tx_1,
                                        float ty_1,
                                        float tx_2,
                                        float ty_2);

void _cg_pango_display_list_add_rectangle(
    cg_pango_display_list_t *dl, float x_1, float y_1, float x_2, float y_2);

void _cg_pango_display_list_add_trapezoid(cg_pango_display_list_t *dl,
                                          float y_1,
                                          float x_11,
                                          float x_21,
                                          float y_2,
                                          float x_12,
                                          float x_22);

void _cg_pango_display_list_render(cg_framebuffer_t *framebuffer,
                                   cg_pango_display_list_t *dl,
                                   const cg_color_t *color);

void _cg_pango_display_list_clear(cg_pango_display_list_t *dl);

void _cg_pango_display_list_free(cg_pango_display_list_t *dl);

CG_END_DECLS

#endif /* __CG_PANGO_DISPLAY_LIST_H__ */
