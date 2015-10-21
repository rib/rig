/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef __RUT_GAUSSIAN_BLURRER_H__
#define __RUT_GAUSSIAN_BLURRER_H__

#include <cglib/cglib.h>

#include "rut-shell.h"

typedef struct _rut_gaussian_blurrer_t {
    rut_shell_t *shell;

    int n_taps;

    int width, height;
    cg_texture_components_t components;

    cg_framebuffer_t *x_pass_fb;
    cg_texture_t *x_pass;
    cg_pipeline_t *x_pass_pipeline;

    cg_framebuffer_t *y_pass_fb;
    cg_texture_t *y_pass;
    cg_texture_t *destination;
    cg_pipeline_t *y_pass_pipeline;
} rut_gaussian_blurrer_t;

rut_gaussian_blurrer_t *rut_gaussian_blurrer_new(rut_shell_t *shell,
                                                 int n_taps);

void rut_gaussian_blurrer_free(rut_gaussian_blurrer_t *blurrer);

cg_texture_t *rut_gaussian_blurrer_blur(rut_gaussian_blurrer_t *blurrer,
                                        cg_texture_t *source);

#endif /* __RUT_GAUSSIAN_BLURRER_H__ */
