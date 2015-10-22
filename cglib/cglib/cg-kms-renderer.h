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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_KMS_RENDERER_H__
#define __CG_KMS_RENDERER_H__

#include <cglib/cg-types.h>
#include <cglib/cg-renderer.h>

CG_BEGIN_DECLS

/**
 * cg_kms_renderer_set_kms_fd:
 * @renderer: A #cg_renderer_t
 * @fd: The fd to kms to use
 *
 * Sets the file descriptor CGlib should use to communicate
 * to the kms driver. If -1 (the default), then CGlib will
 * open its own FD by trying to open "/dev/dri/card0".
 *
 * Stability: unstable
 */
void cg_kms_renderer_set_kms_fd(cg_renderer_t *renderer, int fd);

/**
 * cg_kms_renderer_get_kms_fd:
 * @renderer: A #cg_renderer_t
 *
 * Queries the file descriptor CGlib is using internally for
 * communicating with the kms driver.
 *
 * Return value: The kms file descriptor or -1 if no kms file
 *               desriptor has been opened by CGlib.
 * Stability: unstable
 */
int cg_kms_renderer_get_kms_fd(cg_renderer_t *renderer);

CG_END_DECLS
#endif /* __CG_KMS_RENDERER_H__ */
