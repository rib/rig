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
 *
 *
 */

#ifndef __CG_DISPLAY_GLX_PRIVATE_H
#define __CG_DISPLAY_GLX_PRIVATE_H

#include "cg-object-private.h"

typedef struct _cg_glx_cached_config_t {
    /* This will be -1 if there is no cached config in this slot */
    int depth;
    bool found;
    GLXFBConfig fb_config;
    bool can_mipmap;
} cg_glx_cached_config_t;

#define CG_GLX_N_CACHED_CONFIGS 3

typedef struct _cg_glx_display_t {
    cg_glx_cached_config_t glx_cached_configs[CG_GLX_N_CACHED_CONFIGS];

    bool found_fbconfig;
    bool fbconfig_has_rgba_visual;
    GLXFBConfig fbconfig;

    /* Single context for all wins */
    GLXContext glx_context;
    GLXWindow dummy_glxwin;
    Window dummy_xwin;
} cg_glx_display_t;

#endif /* __CG_DISPLAY_GLX_PRIVATE_H */
