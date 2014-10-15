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

#ifndef _RUT_SCROLL_BAR_H_
#define _RUT_SCROLL_BAR_H_

#include "rut-shell.h"
#include "rut-types.h"

typedef struct _rut_scroll_bar_t rut_scroll_bar_t;
#define RUT_SCROLL_BAR(X) ((rut_scroll_bar_t *)X)

extern rut_type_t rut_scroll_bar_type;

void _rut_scroll_bar_init_type(void);

rut_scroll_bar_t *rut_scroll_bar_new(rut_shell_t *shell,
                                     rut_axis_t axis,
                                     float length,
                                     float virtual_length,
                                     float virtual_viewport_length);

/* Set the length of the scroll bar widget itself */
void rut_scroll_bar_set_length(rut_object_t *scroll_bar, float length);

/* How long is virtual length of the document being scrolled */
void rut_scroll_bar_set_virtual_length(rut_object_t *scroll_bar,
                                       float virtual_length);

/* What is the length of the viewport into the document being scrolled */
void rut_scroll_bar_set_virtual_viewport(rut_object_t *scroll_bar,
                                         float viewport_length);

void rut_scroll_bar_set_virtual_offset(rut_object_t *scroll_bar,
                                       float viewport_offset);

float rut_scroll_bar_get_virtual_offset(rut_scroll_bar_t *scroll_bar);

float rut_scroll_bar_get_virtual_viewport(rut_scroll_bar_t *scroll_bar);

float rut_scroll_bar_get_thickness(rut_scroll_bar_t *scroll_bar);

void rut_scroll_bar_set_color(rut_scroll_bar_t *scroll_bar,
                              const cg_color_t *color);

#endif /* _RUT_SCROLL_BAR_H_ */
