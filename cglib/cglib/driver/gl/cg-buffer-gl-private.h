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

#ifndef _CG_BUFFER_GL_PRIVATE_H_
#define _CG_BUFFER_GL_PRIVATE_H_

#include "cg-types.h"
#include "cg-device.h"
#include "cg-buffer.h"
#include "cg-buffer-private.h"

void _cg_buffer_gl_create(cg_buffer_t *buffer);

void _cg_buffer_gl_destroy(cg_buffer_t *buffer);

void *_cg_buffer_gl_map_range(cg_buffer_t *buffer,
                              size_t offset,
                              size_t size,
                              cg_buffer_access_t access,
                              cg_buffer_map_hint_t hints,
                              cg_error_t **error);

void _cg_buffer_gl_unmap(cg_buffer_t *buffer);

bool _cg_buffer_gl_set_data(cg_buffer_t *buffer,
                            unsigned int offset,
                            const void *data,
                            unsigned int size,
                            cg_error_t **error);

void *_cg_buffer_gl_bind(cg_buffer_t *buffer,
                         cg_buffer_bind_target_t target,
                         cg_error_t **error);

void _cg_buffer_gl_unbind(cg_buffer_t *buffer);

#endif /* _CG_BUFFER_GL_PRIVATE_H_ */
