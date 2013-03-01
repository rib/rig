/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ONSCREEN_TEMPLATE_H__
#define __COGL_ONSCREEN_TEMPLATE_H__

COGL_BEGIN_DECLS

typedef struct _CoglOnscreenTemplate	      CoglOnscreenTemplate;

#define COGL_ONSCREEN_TEMPLATE(OBJECT) ((CoglOnscreenTemplate *)OBJECT)

CoglOnscreenTemplate *
cogl_onscreen_template_new (void);

/**
 * cogl_onscreen_template_set_samples_per_pixel:
 * @onscreen_template: A #CoglOnscreenTemplate template framebuffer
 * @n: The minimum number of samples per pixel
 *
 * Requires that any future CoglOnscreen framebuffers derived from
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
 * being redundant to use the cogl_framebuffer_resolve_samples() and
 * cogl_framebuffer_resolve_samples_region() apis with single-sample
 * rendering.</note>
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_onscreen_template_set_samples_per_pixel (
                                          CoglOnscreenTemplate *onscreen_template,
                                          int n);

/**
 * cogl_onscreen_template_set_swap_throttled:
 * @onscreen_template: A #CoglOnscreenTemplate template framebuffer
 * @throttled: Whether throttling should be enabled
 *
 * Requests that any future #CoglOnscreen framebuffers derived from this
 * template should enable or disable swap throttling according to the given
 * @throttled argument.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_onscreen_template_set_swap_throttled (
                                          CoglOnscreenTemplate *onscreen_template,
                                          CoglBool throttled);

/**
 * cogl_onscreen_template_set_has_alpha:
 * @onscreen_template: A #CoglOnscreenTemplate template framebuffer
 * @has_alpha: Whether an alpha channel is required
 *
 * Requests that any future #CoglOnscreen framebuffers derived from
 * this template should have an alpha channel if @has_alpha is %TRUE.
 * If @has_alpha is FALSE then future framebuffers derived from this
 * template aren't required to have an alpha channel, although Cogl
 * may choose to ignore this and allocate a redundant alpha channel.
 *
 * By default a template does not request an alpha component.
 *
 * Since: 1.16
 * Stability: unstable
 */
void
cogl_onscreen_template_set_has_alpha (CoglOnscreenTemplate *onscreen_template,
                                      CoglBool has_alpha);

/**
 * cogl_is_onscreen_template:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglOnscreenTemplate.
 *
 * Return value: %TRUE if the object references a #CoglOnscreenTemplate
 *   and %FALSE otherwise.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_is_onscreen_template (void *object);

COGL_END_DECLS

#endif /* __COGL_ONSCREEN_TEMPLATE_H__ */
