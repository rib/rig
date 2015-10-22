/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef CG_BLEND_STRING_H
#define CG_BLEND_STRING_H

#include <stdlib.h>
#include <clib.h>

/* NB: debug stringify code will get upset if these
 * are re-ordered */
typedef enum _cg_blend_string_channel_mask_t {
    CG_BLEND_STRING_CHANNEL_MASK_RGB,
    CG_BLEND_STRING_CHANNEL_MASK_ALPHA,
    CG_BLEND_STRING_CHANNEL_MASK_RGBA
} cg_blend_string_channel_mask_t;

typedef enum _cg_blend_string_color_source_type_t {
    CG_BLEND_STRING_COLOR_SOURCE_SRC_COLOR,
    CG_BLEND_STRING_COLOR_SOURCE_DST_COLOR,
    CG_BLEND_STRING_COLOR_SOURCE_CONSTANT,
} cg_blend_string_color_source_type_t;

typedef struct _cg_blend_string_color_source_info_t {
    cg_blend_string_color_source_type_t type;
    const char *name;
    size_t name_len;
} cg_blend_string_color_source_info_t;

typedef struct _cg_blend_string_color_source_t {
    bool is_zero;
    const cg_blend_string_color_source_info_t *info;
    int texture; /* for the TEXTURE_N color source */
    bool one_minus;
    cg_blend_string_channel_mask_t mask;
} cg_blend_string_color_source_t;

typedef struct _cg_blend_string_factor_t {
    bool is_one;
    bool is_src_alpha_saturate;
    bool is_color;
    cg_blend_string_color_source_t source;
} cg_blend_string_factor_t;

typedef struct _cg_blend_string_argument_t {
    cg_blend_string_color_source_t source;
    cg_blend_string_factor_t factor;
} cg_blend_string_argument_t;

typedef enum _cg_blend_string_function_type_t {
    CG_BLEND_STRING_FUNCTION_ADD,
} cg_blend_string_function_type_t;

typedef struct _cg_blend_string_function_info_t {
    enum _cg_blend_string_function_type_t type;
    const char *name;
    size_t name_len;
    int argc;
} cg_blend_string_function_info_t;

typedef struct _cg_blend_string_statement_t {
    cg_blend_string_channel_mask_t mask;
    const cg_blend_string_function_info_t *function;
    cg_blend_string_argument_t args[3];
} cg_blend_string_statement_t;

bool _cg_blend_string_compile(cg_device_t *dev,
                              const char *string,
                              cg_blend_string_statement_t *statements,
                              cg_error_t **error);

void
_cg_blend_string_split_rgba_statement(cg_blend_string_statement_t *statement,
                                      cg_blend_string_statement_t *rgb,
                                      cg_blend_string_statement_t *a);

#endif /* CG_BLEND_STRING_H */
