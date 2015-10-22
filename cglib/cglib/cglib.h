/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

#pragma once

#ifndef CG_COMPILATION
#define __CG_H_INSIDE__
#endif

#include <cglib/cglib-platform.h>

#include <cglib/cg-defines.h>
#include <cglib/cg-error.h>

#include <cglib/cg-object.h>
#include <cglib/cg-bitmap.h>
#include <cglib/cg-color.h>
#include <cglib/cg-matrix-stack.h>
#include <cglib/cg-offscreen.h>
#include <cglib/cg-texture.h>
#include <cglib/cg-types.h>
#include <cglib/cg-version.h>

#include <cglib/cg-renderer.h>
#include <cglib/cg-output.h>
#include <cglib/cg-display.h>
#include <cglib/cg-device.h>
#include <cglib/cg-buffer.h>
#include <cglib/cg-pixel-buffer.h>
#include <cglib/cg-texture-2d.h>
#include <cglib/cg-texture-2d-gl.h>
#include <cglib/cg-texture-3d.h>
#include <cglib/cg-texture-2d-sliced.h>
#include <cglib/cg-sub-texture.h>
#include <cglib/cg-atlas-set.h>
#include <cglib/cg-atlas.h>
#include <cglib/cg-atlas-texture.h>
#include <cglib/cg-meta-texture.h>
#include <cglib/cg-primitive-texture.h>
#include <cglib/cg-index-buffer.h>
#include <cglib/cg-attribute-buffer.h>
#include <cglib/cg-indices.h>
#include <cglib/cg-attribute.h>
#include <cglib/cg-primitive.h>
#include <cglib/cg-depth-state.h>
#include <cglib/cg-pipeline.h>
#include <cglib/cg-pipeline-state.h>
#include <cglib/cg-pipeline-layer-state.h>
#include <cglib/cg-snippet.h>
#include <cglib/cg-framebuffer.h>
#include <cglib/cg-onscreen.h>
#include <cglib/cg-frame-info.h>
#include <cglib/cg-loop.h>
#include <cglib/cg-fence.h>
#if defined(CG_HAS_EGL_PLATFORM_KMS_SUPPORT)
#include <cglib/cg-kms-renderer.h>
#include <cglib/cg-kms-display.h>
#endif
#ifdef CG_HAS_WIN32_SUPPORT
#include <cglib/cg-win32-renderer.h>
#endif
#ifdef CG_HAS_GLIB_SUPPORT
#include <cglib/cg-glib-source.h>
#endif
#ifdef CG_HAS_UV_SUPPORT
#include <cglib/cg-uv.h>
#endif

#ifndef CG_COMPILATION
#undef __CG_H_INSIDE__
#endif

