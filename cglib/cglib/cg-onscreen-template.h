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
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_ONSCREEN_TEMPLATE_H__
#define __CG_ONSCREEN_TEMPLATE_H__

CG_BEGIN_DECLS

typedef struct _cg_onscreen_template_t cg_onscreen_template_t;

#define CG_ONSCREEN_TEMPLATE(OBJECT) ((cg_onscreen_template_t *)OBJECT)

cg_onscreen_template_t *cg_onscreen_template_new(void);

/**
 * cg_onscreen_template_set_samples_per_pixel:
 * @onscreen_template: A #cg_onscreen_template_t template framebuffer
 * @n: The minimum number of samples per pixel
 *
 * Requires that any future cg_onscreen_t framebuffers derived from
 * this template must support making at least @n samples per pixel
 * which will all contribute to the final resolved color for that
 * pixel.
 *
 * By default this value is usually set to 0 and that is referred to
 * as "single-sample" rendering. A value of 1 or greater is referred
 * to as "multisample" rendering.
 *
 * <note>There are some semantic differences between single-sample
 * rendering and multisampling with just 1 point sample such as it
 * being redundant to use the cg_framebuffer_resolve_samples() and
 * cg_framebuffer_resolve_samples_region() apis with single-sample
 * rendering.</note>
 *
 * Stability: unstable
 */
void cg_onscreen_template_set_samples_per_pixel(
    cg_onscreen_template_t *onscreen_template, int n);

/**
 * cg_onscreen_template_set_swap_throttled:
 * @onscreen_template: A #cg_onscreen_template_t template framebuffer
 * @throttled: Whether throttling should be enabled
 *
 * Requests that any future #cg_onscreen_t framebuffers derived from this
 * template should enable or disable swap throttling according to the given
 * @throttled argument.
 *
 * Stability: unstable
 */
void cg_onscreen_template_set_swap_throttled(
    cg_onscreen_template_t *onscreen_template, bool throttled);

/**
 * cg_onscreen_template_set_has_alpha:
 * @onscreen_template: A #cg_onscreen_template_t template framebuffer
 * @has_alpha: Whether an alpha channel is required
 *
 * Requests that any future #cg_onscreen_t framebuffers derived from
 * this template should have an alpha channel if @has_alpha is %true.
 * If @has_alpha is false then future framebuffers derived from this
 * template aren't required to have an alpha channel, although CGlib
 * may choose to ignore this and allocate a redundant alpha channel.
 *
 * By default a template does not request an alpha component.
 *
 * Stability: unstable
 */
void
cg_onscreen_template_set_has_alpha(cg_onscreen_template_t *onscreen_template,
                                   bool has_alpha);

/**
 * cg_is_onscreen_template:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a #cg_onscreen_template_t.
 *
 * Return value: %true if the object references a #cg_onscreen_template_t
 *   and %false otherwise.
 * Stability: unstable
 */
bool cg_is_onscreen_template(void *object);

CG_END_DECLS

#endif /* __CG_ONSCREEN_TEMPLATE_H__ */
