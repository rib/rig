/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef __RUT_INPUT_REGION_H__
#define __RUT_INPUT_REGION_H__

#include <stdbool.h>

#include <cglib/cglib.h>

#include "rut-types.h"
#include "rut-shell.h"

typedef rut_input_event_status_t (*rut_input_region_callback_t)(
    rut_input_region_t *region, rut_input_event_t *event, void *user_data);

rut_input_region_t *
rut_input_region_new_rectangle(float x0,
                               float y0,
                               float x1,
                               float y1,
                               rut_input_region_callback_t callback,
                               void *user_data);

rut_input_region_t *
rut_input_region_new_circle(float x,
                            float y,
                            float radius,
                            rut_input_region_callback_t callback,
                            void *user_data);

void rut_input_region_set_transform(rut_input_region_t *region,
                                    c_matrix_t *matrix);

void rut_input_region_set_rectangle(
    rut_input_region_t *region, float x0, float y0, float x1, float y1);

void rut_input_region_set_circle(rut_input_region_t *region,
                                 float x0,
                                 float y0,
                                 float radius);

#endif /* __RUT_INPUT_REGION_H__ */
