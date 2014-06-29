/*
 * Rut
 *
 * Rig Utilities
 *
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
 */

#ifndef _RUT_UI_VIEWPORT_H_
#define _RUT_UI_VIEWPORT_H_

#include <cogl/cogl.h>

#include "rut-type.h"

typedef struct _RutUIViewport RutUIViewport;
#define RUT_UI_VIEWPORT(X) ((RutUIViewport *)X)

extern RutType rut_ui_viewport_type;

RutUIViewport *
rut_ui_viewport_new (RutContext *ctx,
                     float width,
                     float height);

void
rut_ui_viewport_add (RutUIViewport *ui_viewport,
                     RutObject *child);

void
rut_ui_viewport_get_size (RutUIViewport *ui_viewport,
                          float *width,
                          float *height);

void
rut_ui_viewport_set_doc_x (RutObject *ui_viewport, float doc_x);

void
rut_ui_viewport_set_doc_y (RutObject *ui_viewport, float doc_y);

void
rut_ui_viewport_set_doc_width (RutObject *ui_viewport, float doc_width);

void
rut_ui_viewport_set_doc_height (RutObject *ui_viewport, float doc_height);

void
rut_ui_viewport_set_doc_scale_x (RutUIViewport *ui_viewport, float doc_scale_x);

void
rut_ui_viewport_set_doc_scale_y (RutUIViewport *ui_viewport, float doc_scale_y);

float
rut_ui_viewport_get_width (RutUIViewport *ui_viewport);

float
rut_ui_viewport_get_height (RutUIViewport *ui_viewport);

float
rut_ui_viewport_get_doc_x (RutObject *ui_viewport);

float
rut_ui_viewport_get_doc_y (RutObject *ui_viewport);

float
rut_ui_viewport_get_doc_scale_x (RutUIViewport *ui_viewport);

float
rut_ui_viewport_get_doc_scale_y (RutUIViewport *ui_viewport);

const cg_matrix_t *
rut_ui_viewport_get_doc_matrix (RutUIViewport *ui_viewport);

void
rut_ui_viewport_set_x_pannable (RutObject *ui_viewport,
                                bool pannable);

bool
rut_ui_viewport_get_x_pannable (RutObject *ui_viewport);

void
rut_ui_viewport_set_y_pannable (RutObject *ui_viewport,
                                bool pannable);

bool
rut_ui_viewport_get_y_pannable (RutObject *ui_viewport);

/**
 * rut_ui_viewport_set_sync_widget:
 * @ui_viewport: The viewport object
 * @widget: An object implementing the RutSizable interface
 *
 * Sets a widget to use to specify the doc size. The viewport will
 * track the preferred size of the widget and set the doc to the same
 * size whenever it changes.
 *
 * If the viewport is not pannable on the x-axis then the width of
 * this widget will kept in sync with the width of the viewport.
 * Similarly if the viewport is not pannable on the y-axis then the
 * height of this widget will kept in sync with the height of the
 * viewport.
 *
 * The sync widget should typically be a child of the doc.
 */
void
rut_ui_viewport_set_sync_widget (RutObject *ui_viewport,
                                 RutObject *widget);

void
rut_ui_viewport_set_scroll_bar_color (RutUIViewport *ui_viewport,
                                      const cg_color_t *color);

#endif /* _RUT_UI_VIEWPORT_H_ */
