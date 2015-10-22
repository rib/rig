/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 *
 * Authors:
 *   Owen Taylor <otaylor@redhat.com>
 */
#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_FRAME_INFO_H
#define __CG_FRAME_INFO_H

#include <cglib/cg-types.h>
#include <cglib/cg-output.h>

CG_BEGIN_DECLS

typedef struct _cg_frame_info_t cg_frame_info_t;
#define CG_FRAME_INFO(X) ((cg_frame_info_t *)(X))

/**
 * cg_is_frame_info:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a #cg_frame_info_t.
 *
 * Return value: %true if the object references a #cg_frame_info_t
 *   and %false otherwise.
 * Stability: unstable
 */
bool cg_is_frame_info(void *object);

/**
 * cg_frame_info_get_frame_counter:
 * @info: a #cg_frame_info_t object
 *
 * Gets the frame counter for the #cg_onscreen_t that corresponds
 * to this frame.
 *
 * Return value: The frame counter value
 * Stability: unstable
 */
int64_t cg_frame_info_get_frame_counter(cg_frame_info_t *info);

/**
 * cg_frame_info_get_presentation_time:
 * @info: a #cg_frame_info_t object
 *
 * Gets the presentation time for the frame. This is the time at which
 * the frame became visible to the user.
 *
 * The presentation time measured in nanoseconds is based on a
 * monotonic time source. The time source is not necessarily
 * correlated with system/wall clock time and may represent the time
 * elapsed since some undefined system event such as when the system
 * last booted.
 *
 * <note>Linux kernel version less that 3.8 can result in
 * non-monotonic timestamps being reported when using a drm based
 * OpenGL driver. Also some buggy Mesa drivers up to 9.0.1 may also
 * incorrectly report non-monotonic timestamps.</note>
 *
 * Return value: the presentation time for the frame
 * Stability: unstable
 */
int64_t cg_frame_info_get_presentation_time(cg_frame_info_t *info);

/**
 * cg_frame_info_get_refresh_rate:
 * @info: a #cg_frame_info_t object
 *
 * Gets the refresh rate in Hertz for the output that the frame was on
 * at the time the frame was presented.
 *
 * <note>Some platforms can't associate a #cg_output_t with a
 * #cg_frame_info_t object but are able to report a refresh rate via
 * this api. Therefore if you need this information then this api is
 * more reliable than using cg_frame_info_get_output() followed by
 * cg_output_get_refresh_rate().</note>
 *
 * Return value: the refresh rate in Hertz
 * Stability: unstable
 */
float cg_frame_info_get_refresh_rate(cg_frame_info_t *info);

/**
 * cg_frame_info_get_output:
 * @info: a #cg_frame_info_t object
 *
 * Gets the #cg_output_t that the swapped frame was presented to.
 *
 * Return value: (transfer none): The #cg_output_t that the frame was
 *        presented to, or %NULL if this could not be determined.
 * Stability: unstable
 */
cg_output_t *cg_frame_info_get_output(cg_frame_info_t *info);

CG_END_DECLS

#endif /* __CG_FRAME_INFO_H */
