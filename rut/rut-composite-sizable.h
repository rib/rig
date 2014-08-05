/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013 Intel Corporation.
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

#include <config.h>

#ifndef _RUT_COMPOSITE_SIZABLE_
#define _RUT_COMPOSITE_SIZABLE_

void rut_composite_sizable_get_preferred_width(void *sizable,
                                               float for_height,
                                               float *min_width_p,
                                               float *natural_width_p);

void rut_composite_sizable_get_preferred_height(void *sizable,
                                                float for_width,
                                                float *min_height_p,
                                                float *natural_height_p);

rut_closure_t *rut_composite_sizable_add_preferred_size_callback(
    void *object,
    rut_sizeable_preferred_size_callback_t cb,
    void *user_data,
    rut_closure_destroy_callback_t destroy);

void rut_composite_sizable_set_size(void *object, float width, float height);

void rut_composite_sizable_get_size(void *object, float *width, float *height);

#endif /* _RUT_COMPOSITE_SIZABLE_ */
