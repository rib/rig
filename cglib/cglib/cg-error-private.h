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
 */

#ifndef __CG_ERROR_PRIVATE_H__
#define __CG_ERROR_PRIVATE_H__

#include "cg-error.h"

#ifdef CG_HAS_GLIB_SUPPORT
#include <glib.h>
#endif

void _cg_set_error(cg_error_t **error,
                   uint32_t domain,
                   int code,
                   const char *format,
                   ...) C_GNUC_PRINTF(4, 5);

void _cg_set_error_literal(cg_error_t **error,
                           uint32_t domain,
                           int code,
                           const char *message);

void _cg_propagate_error(cg_error_t **dest, cg_error_t *src);

#ifdef CG_HAS_GLIB_SUPPORT
void _cg_propagate_gerror(cg_error_t **dest, GError *src);
#endif /* CG_HAS_GLIB_SUPPORT */

#define _cg_clear_error(X) c_clear_error((GError **)X)

#endif /* __CG_ERROR_PRIVATE_H__ */
