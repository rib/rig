/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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

#ifndef __CG_BLIT_H
#define __CG_BLIT_H

#include <clib.h>
#include "cg-object-private.h"
#include "cg-texture.h"
#include "cg-framebuffer.h"

/* This structures and functions are used when a series of blits needs
   to be performed between two textures. In this case there are
   multiple methods we can use, most of which involve transferring
   between an FBO bound to the texture. */

typedef struct _cg_blit_data_t cg_blit_data_t;

typedef bool (*cg_blit_begin_func_t)(cg_blit_data_t *data);
typedef void (*cg_blit_end_func_t)(cg_blit_data_t *data);

typedef void (*cg_blit_func_t)(cg_blit_data_t *data,
                               int src_x,
                               int src_y,
                               int dst_x,
                               int dst_y,
                               int width,
                               int height);

typedef struct {
    const char *name;
    cg_blit_begin_func_t begin_func;
    cg_blit_func_t blit_func;
    cg_blit_end_func_t end_func;
} cg_blit_mode_t;

struct _cg_blit_data_t {
    cg_texture_t *src_tex, *dst_tex;

    unsigned int src_width;
    unsigned int src_height;

    const cg_blit_mode_t *blit_mode;

    /* If we're not using an FBO then we c_malloc a buffer and copy the
       complete texture data in */
    unsigned char *image_data;
    cg_pixel_format_t format;

    int bpp;

    cg_framebuffer_t *src_fb;
    cg_framebuffer_t *dest_fb;
    cg_pipeline_t *pipeline;
};

void _cg_blit_begin(cg_blit_data_t *data,
                    cg_texture_t *dst_tex,
                    cg_texture_t *src_tex);

void _cg_blit(cg_blit_data_t *data,
              int src_x,
              int src_y,
              int dst_x,
              int dst_y,
              int width,
              int height);

void _cg_blit_end(cg_blit_data_t *data);

#endif /* __CG_BLIT_H */
