/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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

/* This can be included multiple times with different definitions for
 * the CG_WINSYS_FEATURE_* functions.
 */

/* Macro prototypes:
 * CG_WINSYS_FEATURE_BEGIN (major_glx_version, minor_glx_version,
 *                            name, namespaces, extension_names,
 *                            implied_winsys_feature)
 * CG_WINSYS_FEATURE_FUNCTION (return_type, function_name,
 *                               (arguments))
 * ...
 * CG_WINSYS_FEATURE_END ()
 *
 * Note: You can list multiple namespace and extension names if the
 * corresponding _FEATURE_FUNCTIONS have the same semantics accross
 * the different extension variants.
 *
 * XXX: NB: Don't add a trailing semicolon when using these macros
 */

/* Base functions that we assume are always available */
CG_WINSYS_FEATURE_BEGIN(0,
                        0, /* always available */
                        base_glx_functions,
                        "\0",
                        "\0",
                        0 /* no winsys feature */)
CG_WINSYS_FEATURE_FUNCTION(void,
                           glXDestroyContext,
                           (Display *dpy, GLXContext ctx))
CG_WINSYS_FEATURE_FUNCTION(void,
                           glXSwapBuffers,
                           (Display *dpy, GLXDrawable drawable))
CG_WINSYS_FEATURE_FUNCTION(Bool, glXIsDirect, (Display *dpy, GLXContext ctx))
CG_WINSYS_FEATURE_FUNCTION(
    int,
    glXGetFBConfigAttrib,
    (Display *dpy, GLXFBConfig config, int attribute, int *value))
CG_WINSYS_FEATURE_FUNCTION(
    GLXWindow,
    glXCreateWindow,
    (Display *dpy, GLXFBConfig config, Window win, const int *attribList))
CG_WINSYS_FEATURE_FUNCTION(void,
                           glXDestroyWindow,
                           (Display *dpy, GLXWindow window))
CG_WINSYS_FEATURE_FUNCTION(
    GLXPixmap,
    glXCreatePixmap,
    (Display *dpy, GLXFBConfig config, Pixmap pixmap, const int *attribList))
CG_WINSYS_FEATURE_FUNCTION(void,
                           glXDestroyPixmap,
                           (Display *dpy, GLXPixmap pixmap))
CG_WINSYS_FEATURE_FUNCTION(GLXContext,
                           glXCreateNewContext,
                           (Display *dpy,
                            GLXFBConfig config,
                            int renderType,
                            GLXContext shareList,
                            Bool direct))
CG_WINSYS_FEATURE_FUNCTION(
    Bool,
    glXMakeContextCurrent,
    (Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx))
CG_WINSYS_FEATURE_FUNCTION(void,
                           glXSelectEvent,
                           (Display *dpy,
                            GLXDrawable drawable,
                            unsigned long mask))
CG_WINSYS_FEATURE_FUNCTION(GLXFBConfig *,
                           glXGetFBConfigs,
                           (Display *dpy, int screen, int *nelements))
CG_WINSYS_FEATURE_FUNCTION(
    GLXFBConfig *,
    glXChooseFBConfig,
    (Display *dpy, int screen, const int *attrib_list, int *nelements))
CG_WINSYS_FEATURE_FUNCTION(XVisualInfo *,
                           glXGetVisualFromFBConfig,
                           (Display *dpy, GLXFBConfig config))
CG_WINSYS_FEATURE_END()

CG_WINSYS_FEATURE_BEGIN(255,
                        255,
                        texture_from_pixmap,
                        "EXT\0",
                        "texture_from_pixmap\0",
                        CG_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP)
CG_WINSYS_FEATURE_FUNCTION(
    void,
    glXBindTexImage,
    (Display *display, GLXDrawable drawable, int buffer, int *attribList))
CG_WINSYS_FEATURE_FUNCTION(void,
                           glXReleaseTexImage,
                           (Display *display, GLXDrawable drawable, int buffer))
CG_WINSYS_FEATURE_END()

CG_WINSYS_FEATURE_BEGIN(255,
                        255,
                        video_sync,
                        "SGI\0",
                        "video_sync\0",
                        CG_WINSYS_FEATURE_VBLANK_COUNTER)
CG_WINSYS_FEATURE_FUNCTION(int, glXGetVideoSync, (unsigned int *count))
CG_WINSYS_FEATURE_FUNCTION(int,
                           glXWaitVideoSync,
                           (int divisor, int remainder, unsigned int *count))
CG_WINSYS_FEATURE_END()

CG_WINSYS_FEATURE_BEGIN(255,
                        255,
                        swap_control,
                        "SGI\0",
                        "swap_control\0",
                        CG_WINSYS_FEATURE_SWAP_THROTTLE)
CG_WINSYS_FEATURE_FUNCTION(int, glXSwapInterval, (int interval))
CG_WINSYS_FEATURE_END()

CG_WINSYS_FEATURE_BEGIN(255, 255, sync_control, "OML\0", "sync_control\0", 0)
CG_WINSYS_FEATURE_FUNCTION(Bool,
                           glXGetSyncValues,
                           (Display *dpy,
                            GLXDrawable drawable,
                            int64_t *ust,
                            int64_t *msc,
                            int64_t *sbc))
CG_WINSYS_FEATURE_FUNCTION(Bool,
                           glXWaitForMsc,
                           (Display *dpy,
                            GLXDrawable drawable,
                            int64_t target_msc,
                            int64_t divisor,
                            int64_t remainder,
                            int64_t *ust,
                            int64_t *msc,
                            int64_t *sbc))
CG_WINSYS_FEATURE_END()

CG_WINSYS_FEATURE_BEGIN(255,
                        255,
                        copy_sub_buffer,
                        "MESA\0",
                        "copy_sub_buffer\0",
/* We initially assumed that copy_sub_buffer is synchronized on
 * which is only the case for a subset of GPUs for example it is not
 * synchronized on INTEL gen6 and gen7, so we remove this assumption
 * for now
 */
#if 0
                        CG_WINSYS_FEATURE_SWAP_REGION_SYNCHRONIZED)
#endif
                        0)
                        CG_WINSYS_FEATURE_FUNCTION(
                            void,
                            glXCopySubBuffer,
                            (Display *dpy, GLXDrawable drawable, int x, int y, int width, int height))
CG_WINSYS_FEATURE_END()

CG_WINSYS_FEATURE_BEGIN(255,
                        255,
                        swap_event,
                        "INTEL\0",
                        "swap_event\0",
                        CG_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT)
CG_WINSYS_FEATURE_END()

CG_WINSYS_FEATURE_BEGIN(255, 255, create_context, "ARB\0", "create_context", 0)
CG_WINSYS_FEATURE_FUNCTION(GLXContext,
                           glXCreateContextAttribs,
                           (Display *dpy,
                            GLXFBConfig config,
                            GLXContext share_context,
                            Bool direct,
                            const int *attrib_list))
CG_WINSYS_FEATURE_END()

CG_WINSYS_FEATURE_BEGIN(
    255, 255, buffer_age, "EXT\0", "buffer_age\0", CG_WINSYS_FEATURE_BUFFER_AGE)
CG_WINSYS_FEATURE_END()
