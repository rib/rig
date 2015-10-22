/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2014 Intel Corporation.
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

#ifndef __CG_UV_H__
#define __CG_UV_H__

#include <uv.h>

#include <cglib/cg-defines.h>
#include <cglib/cg-device.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-uv
 * @short_description: Functions for integrating CGlib with a libuv
 *   main loop
 *
 * CGlib needs to integrate with the application's main loop so that it
 * can internally handle some events from the driver. libuv provides
 * an efficient, portable mainloop api that many projects use and
 * so this is a convenience API that can be used instead of the
 * more low level cg-loop api.
 */

/**
 * cg_uv_set_mainloop:
 * @device: A CGlib device
 * @mainloop: A libuv main loop
 *
 * Associates a libuv @mainloop with the given CGlib @device so that
 * CGlib will be able to use the mainloop to handle internal events.
 */
void
cg_uv_set_mainloop(cg_device_t *device, uv_loop_t *mainloop);

CG_END_DECLS

#endif /* __CG_UV_H__ */
