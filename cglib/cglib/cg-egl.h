/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifndef __CG_EGL_H__
#define __CG_EGL_H__

#include "cg-defines.h"

#ifdef CG_HAS_EGL_SUPPORT

#include "cg-egl-defines.h"

CG_BEGIN_DECLS

#define NativeDisplayType EGLNativeDisplayType
#define NativeWindowType EGLNativeWindowType

#ifndef GL_OES_EGL_image
#define GLeglImageOES void *
#endif

/**
 * cg_egl_context_get_egl_display:
 * @dev: A #cg_device_t pointer
 *
 * If you have done a runtime check to determine that CGlib is using
 * EGL internally then this API can be used to retrieve the EGLDisplay
 * handle that was setup internally. The result is undefined if CGlib
 * is not using EGL.
 *
 * Return value: The internally setup EGLDisplay handle.
 * Stability: unstable
 */
EGLDisplay cg_egl_context_get_egl_display(cg_device_t *dev);

CG_END_DECLS

#endif /* CG_HAS_EGL_SUPPORT */

#endif
