/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2015 Intel Corporation.
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

#ifndef _CG_WEBGL_PRIVATE_H_
#define _CG_WEBGL_PRIVATE_H_

#include "cg-webgl.h"
#include "cg-closure-list-private.h"
#include "cg-object-private.h"

typedef int cg_webgl_image_handle_t;

struct _cg_webgl_image
{
  cg_object_t _parent;

  c_list_t onload_closures;
  c_list_t onerror_closures;

  cg_webgl_image_handle_t image_handle;
};

typedef void
(* cg_webgl_image_handle_callback_t)(cg_webgl_image_handle_t image_handle,
                                     void *user_data);

cg_webgl_image_handle_t
_cg_webgl_image_create(const char *url,
                       cg_webgl_image_handle_callback_t onload,
                       cg_webgl_image_handle_callback_t onerror,
                       void *user_data);

void
_cg_webgl_image_destroy(cg_webgl_image_handle_t image_handle);

int
_cg_webgl_image_get_width(cg_webgl_image_handle_t image_handle);

int
_cg_webgl_image_get_height(cg_webgl_image_handle_t image_handle);

char *
_cg_webgl_tex_image_2d_with_image(int target,
                                  int level,
                                  int internalformat,
                                  int format,
                                  int type,
                                  cg_webgl_image_handle_t image_handle);

#endif /* _CG_WEBGL_PRIVATE_H_ */

