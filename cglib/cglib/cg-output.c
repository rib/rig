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

#include "cg-output-private.h"

#include <string.h>

static void _cg_output_free(cg_output_t *output);

CG_OBJECT_DEFINE(Output, output);

cg_output_t *
_cg_output_new(const char *name)
{
    cg_output_t *output;

    output = c_slice_new0(cg_output_t);
    output->name = c_strdup(name);

    return _cg_output_object_new(output);
}

static void
_cg_output_free(cg_output_t *output)
{
    c_free(output->name);

    c_slice_free(cg_output_t, output);
}

bool
_cg_output_values_equal(cg_output_t *output, cg_output_t *other)
{
    return memcmp((const char *)output + C_STRUCT_OFFSET(cg_output_t, x),
                  (const char *)other + C_STRUCT_OFFSET(cg_output_t, x),
                  sizeof(cg_output_t) - C_STRUCT_OFFSET(cg_output_t, x)) == 0;
}

int
cg_output_get_x(cg_output_t *output)
{
    return output->x;
}

int
cg_output_get_y(cg_output_t *output)
{
    return output->y;
}

int
cg_output_get_width(cg_output_t *output)
{
    return output->width;
}

int
cg_output_get_height(cg_output_t *output)
{
    return output->height;
}

int
cg_output_get_mm_width(cg_output_t *output)
{
    return output->mm_width;
}

int
cg_output_get_mm_height(cg_output_t *output)
{
    return output->mm_height;
}

cg_subpixel_order_t
cg_output_get_subpixel_order(cg_output_t *output)
{
    return output->subpixel_order;
}

float
cg_output_get_refresh_rate(cg_output_t *output)
{
    return output->refresh_rate;
}
