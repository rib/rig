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
#include "rut-context.h"

extern RutType rut_number_slider_type;
typedef struct _RutNumberSlider RutNumberSlider;

RutNumberSlider *
rut_number_slider_new (RutContext *ctx);

void
rut_number_slider_set_markup_label (RutNumberSlider *slider,
                                    const char *markup);

void
rut_number_slider_set_min_value (RutNumberSlider *slider,
                                 float min_value);

void
rut_number_slider_set_max_value (RutNumberSlider *slider,
                                 float max_value);

void
rut_number_slider_set_value (RutObject *slider,
                             float value);

float
rut_number_slider_get_value (RutNumberSlider *slider);

void
rut_number_slider_set_step (RutNumberSlider *slider,
                            float step);

void
rut_number_slider_set_decimal_places (RutNumberSlider *slider,
                                      int decimal_places);

int
rut_number_slider_get_decimal_places (RutNumberSlider *slider);

#endif /* _RUT_NUMBER_SLIDER_H_ */
