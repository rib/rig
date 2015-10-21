/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2015  Intel Corporation.
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

#pragma once

#include <cglib/cglib.h>

#include "rut-object.h"
#include "rut-shell.h"

typedef enum {
    RUT_EXTEND_NONE,
    RUT_EXTEND_REPEAT,
    RUT_EXTEND_REFLECT,
    RUT_EXTEND_PAD
} rut_extend_t;

typedef struct _rut_gradient_stop {
    cg_color_t color;
    float offset;
} rut_gradient_stop_t;

struct _rut_linear_gradient {
    rut_object_base_t _base;

    rut_extend_t extend_mode;

    /* NB: these stops will have premultiplied colors */
    int	n_stops;
    rut_gradient_stop_t *stops;

    cg_texture_2d_t *texture;
    float translate_x;
    float scale_x;
};


typedef struct _rut_linear_gradient rut_linear_gradient_t;

extern rut_type_t rut_linear_gradient_type;

/* XXX: the input stop colors should be unpremultiplied */
rut_linear_gradient_t *
rut_linear_gradient_new(rut_shell_t *shell,
                        rut_extend_t extend_mode,
                        int n_stops,
                        const rut_gradient_stop_t *stops);

bool
rut_linear_gradient_equal(const void *key_a, const void *key_b);
