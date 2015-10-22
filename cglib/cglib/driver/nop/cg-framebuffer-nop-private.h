/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
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
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef _CG_FRAMEBUFFER_NOP_PRIVATE_H_
#define _CG_FRAMEBUFFER_NOP_PRIVATE_H_

#include "cg-types.h"
#include "cg-device-private.h"

bool _cg_offscreen_nop_allocate(cg_offscreen_t *offscreen, cg_error_t **error);

void _cg_offscreen_nop_free(cg_offscreen_t *offscreen);

void _cg_framebuffer_nop_flush_state(cg_framebuffer_t *draw_buffer,
                                     cg_framebuffer_t *read_buffer,
                                     cg_framebuffer_state_t state);

void _cg_framebuffer_nop_clear(cg_framebuffer_t *framebuffer,
                               unsigned long buffers,
                               float red,
                               float green,
                               float blue,
                               float alpha);

void _cg_framebuffer_nop_query_bits(cg_framebuffer_t *framebuffer,
                                    cg_framebuffer_bits_t *bits);

void _cg_framebuffer_nop_finish(cg_framebuffer_t *framebuffer);

void _cg_framebuffer_nop_discard_buffers(cg_framebuffer_t *framebuffer,
                                         unsigned long buffers);

void _cg_framebuffer_nop_draw_attributes(cg_framebuffer_t *framebuffer,
                                         cg_pipeline_t *pipeline,
                                         cg_vertices_mode_t mode,
                                         int first_vertex,
                                         int n_vertices,
                                         cg_attribute_t **attributes,
                                         int n_attributes,
                                         int n_instances,
                                         cg_draw_flags_t flags);

void _cg_framebuffer_nop_draw_indexed_attributes(cg_framebuffer_t *framebuffer,
                                                 cg_pipeline_t *pipeline,
                                                 cg_vertices_mode_t mode,
                                                 int first_vertex,
                                                 int n_vertices,
                                                 cg_indices_t *indices,
                                                 cg_attribute_t **attributes,
                                                 int n_attributes,
                                                 int n_instances,
                                                 cg_draw_flags_t flags);

bool _cg_framebuffer_nop_read_pixels_into_bitmap(cg_framebuffer_t *framebuffer,
                                                 int x,
                                                 int y,
                                                 cg_read_pixels_flags_t source,
                                                 cg_bitmap_t *bitmap,
                                                 cg_error_t **error);

#endif /* _CG_FRAMEBUFFER_NOP_PRIVATE_H_ */
