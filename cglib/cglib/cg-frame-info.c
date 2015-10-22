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
 */

#include <cglib-config.h>

#include "cg-frame-info-private.h"

static void _cg_frame_info_free(cg_frame_info_t *info);

CG_OBJECT_DEFINE(FrameInfo, frame_info);

cg_frame_info_t *
_cg_frame_info_new(void)
{
    cg_frame_info_t *info;

    info = c_slice_new0(cg_frame_info_t);

    return _cg_frame_info_object_new(info);
}

static void
_cg_frame_info_free(cg_frame_info_t *info)
{
    c_slice_free(cg_frame_info_t, info);
}

int64_t
cg_frame_info_get_frame_counter(cg_frame_info_t *info)
{
    return info->frame_counter;
}

int64_t
cg_frame_info_get_presentation_time(cg_frame_info_t *info)
{
    return info->presentation_time;
}

float
cg_frame_info_get_refresh_rate(cg_frame_info_t *info)
{
    return info->refresh_rate;
}

cg_output_t *
cg_frame_info_get_output(cg_frame_info_t *info)
{
    return info->output;
}
