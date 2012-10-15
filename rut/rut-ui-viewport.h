/*
 * Rut
 *
 * A tiny toolkit
 *
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
                     float height,
                     ...);

void
rut_ui_viewport_set_width (RutUIViewport *ui_viewport, float width);

void
rut_ui_viewport_set_height (RutUIViewport *ui_viewport, float height);

void
rut_ui_viewport_set_size (RutUIViewport *ui_viewport,
                          float width,
                          float height);

void
rut_ui_viewport_get_size (RutUIViewport *ui_viewport,
                          float *width,
                          float *height);

void
rut_ui_viewport_set_doc_x (RutUIViewport *ui_viewport, float doc_x);

void
rut_ui_viewport_set_doc_y (RutUIViewport *ui_viewport, float doc_y);

void
rut_ui_viewport_set_doc_width (RutUIViewport *ui_viewport, float doc_width);

void
rut_ui_viewport_set_doc_height (RutUIViewport *ui_viewport, float doc_height);

void
rut_ui_viewport_set_doc_scale_x (RutUIViewport *ui_viewport, float doc_scale_x);

void
rut_ui_viewport_set_doc_scale_y (RutUIViewport *ui_viewport, float doc_scale_y);

float
rut_ui_viewport_get_width (RutUIViewport *ui_viewport);

float
rut_ui_viewport_get_height (RutUIViewport *ui_viewport);

float
rut_ui_viewport_get_doc_x (RutUIViewport *ui_viewport);

float
rut_ui_viewport_get_doc_y (RutUIViewport *ui_viewport);

float
rut_ui_viewport_get_doc_scale_x (RutUIViewport *ui_viewport);

float
rut_ui_viewport_get_doc_scale_y (RutUIViewport *ui_viewport);

const CoglMatrix *
rut_ui_viewport_get_doc_matrix (RutUIViewport *ui_viewport);

RutObject *
rut_ui_viewport_get_doc_node (RutUIViewport *ui_viewport);

void
rut_ui_viewport_set_x_pannable (RutUIViewport *ui_viewport,
                                CoglBool pannable);

CoglBool
rut_ui_viewport_get_x_pannable (RutUIViewport *ui_viewport);

void
rut_ui_viewport_set_y_pannable (RutUIViewport *ui_viewport,
                                CoglBool pannable);

CoglBool
rut_ui_viewport_get_y_pannable (RutUIViewport *ui_viewport);

#endif /* _RUT_UI_VIEWPORT_H_ */
