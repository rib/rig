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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_KMS_DISPLAY_H__
#define __CG_KMS_DISPLAY_H__

#include <cglib/cg-types.h>
#include <cglib/cg-display.h>

#include <xf86drmMode.h>

CG_BEGIN_DECLS

/**
 * cg_kms_display_queue_modes_reset:
 * @display: A #cg_display_t
 *
 * Asks CGlib to explicitly reset the crtc output modes at the next
 * #cg_onscreen_t swap_buffers request. For applications that support
 * VT switching they may want to re-assert the output modes when
 * switching back to the applications VT since the modes are often not
 * correctly restored automatically.
 *
 * <note>The @display must have been either explicitly setup via
 * cg_display_setup() or implicitily setup by having created a
 * context using the @display</note>
 *
 * Stability: unstable
 */
void cg_kms_display_queue_modes_reset(cg_display_t *display);

typedef struct {
    uint32_t id;
    uint32_t x, y;
    drmModeModeInfo mode;

    uint32_t *connectors;
    uint32_t count;
} cg_kms_crtc_t;

/**
 * cg_kms_display_set_layout:
 * @onscreen: a #cg_display_t
 * @width: the framebuffer width
 * @height: the framebuffer height
 * @crtcs: the array of #cg_kms_crtc_t structure with the desired CRTC layout
 *
 * Configures @display to use a framebuffer sized @width x @height, covering
 * the CRTCS in @crtcs.
 * @width and @height must be within the driver framebuffer limits, and @crtcs
 * must be valid KMS API IDs.
 *
 * Calling this function overrides the automatic mode setting done by CGlib,
 * and for this reason must be called before the first call to
 * cg_onscreen_swap_buffers().
 *
 * If you want to restore the default behaviour, you can call this function
 * with @width and @height set to -1.
 *
 * Stability: unstable
 */
bool cg_kms_display_set_layout(cg_display_t *display,
                               int width,
                               int height,
                               cg_kms_crtc_t **crtcs,
                               int n_crtcs,
                               cg_error_t **error);

CG_END_DECLS
#endif /* __CG_KMS_DISPLAY_H__ */
