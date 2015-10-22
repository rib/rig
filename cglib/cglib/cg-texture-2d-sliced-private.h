/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 *
 */

#ifndef __CG_TEXTURE_2D_SLICED_PRIVATE_H
#define __CG_TEXTURE_2D_SLICED_PRIVATE_H

#include "cg-bitmap-private.h"
#include "cg-pipeline-private.h"
#include "cg-texture-private.h"
#include "cg-texture-2d-sliced.h"

#include <clib.h>

struct _cg_texture_2d_sliced_t {
    cg_texture_t _parent;

    c_array_t *slice_x_spans;
    c_array_t *slice_y_spans;
    c_array_t *slice_textures;
    int max_waste;
    cg_pixel_format_t internal_format;
};

#endif /* __CG_TEXTURE_2D_SLICED_PRIVATE_H */
