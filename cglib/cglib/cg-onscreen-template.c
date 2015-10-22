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
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-object.h"

#include "cg-framebuffer-private.h"
#include "cg-onscreen-template-private.h"

#include <stdlib.h>

static void
_cg_onscreen_template_free(cg_onscreen_template_t *onscreen_template);

CG_OBJECT_DEFINE(OnscreenTemplate, onscreen_template);

static void
_cg_onscreen_template_free(cg_onscreen_template_t *onscreen_template)
{
    c_slice_free(cg_onscreen_template_t, onscreen_template);
}

cg_onscreen_template_t *
cg_onscreen_template_new(void)
{
    cg_onscreen_template_t *onscreen_template =
        c_slice_new0(cg_onscreen_template_t);
    char *user_config;

    onscreen_template->config.swap_throttled = true;
    onscreen_template->config.need_stencil = true;
    onscreen_template->config.samples_per_pixel = 0;

    user_config = getenv("CG_POINT_SAMPLES_PER_PIXEL");
    if (user_config) {
        unsigned long samples_per_pixel = strtoul(user_config, NULL, 10);
        if (samples_per_pixel != ULONG_MAX)
            onscreen_template->config.samples_per_pixel = samples_per_pixel;
    }

    return _cg_onscreen_template_object_new(onscreen_template);
}

void
cg_onscreen_template_set_samples_per_pixel(
    cg_onscreen_template_t *onscreen_template, int samples_per_pixel)
{
    onscreen_template->config.samples_per_pixel = samples_per_pixel;
}

void
cg_onscreen_template_set_swap_throttled(
    cg_onscreen_template_t *onscreen_template, bool throttled)
{
    onscreen_template->config.swap_throttled = throttled;
}

void
cg_onscreen_template_set_has_alpha(cg_onscreen_template_t *onscreen_template,
                                   bool has_alpha)
{
    onscreen_template->config.has_alpha = has_alpha;
}
