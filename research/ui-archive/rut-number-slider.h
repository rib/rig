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

#ifndef _RUT_NUMBER_SLIDER_H_
#define _RUT_NUMBER_SLIDER_H_

#include "rut-object.h"
#include "rut-shell.h"

extern rut_type_t rut_number_slider_type;
typedef struct _rut_number_slider_t rut_number_slider_t;

rut_number_slider_t *rut_number_slider_new(rut_shell_t *shell);

void rut_number_slider_set_markup_label(rut_number_slider_t *slider,
                                        const char *markup);

void rut_number_slider_set_min_value(rut_number_slider_t *slider,
                                     float min_value);

void rut_number_slider_set_max_value(rut_number_slider_t *slider,
                                     float max_value);

void rut_number_slider_set_value(rut_object_t *slider, float value);

float rut_number_slider_get_value(rut_number_slider_t *slider);

void rut_number_slider_set_step(rut_number_slider_t *slider, float step);

void rut_number_slider_set_decimal_places(rut_number_slider_t *slider,
                                          int decimal_places);

int rut_number_slider_get_decimal_places(rut_number_slider_t *slider);

#endif /* _RUT_NUMBER_SLIDER_H_ */
