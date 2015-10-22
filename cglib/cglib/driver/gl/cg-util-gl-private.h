/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
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
 * Authors:
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifndef _CG_UTIL_GL_PRIVATE_H_

#include "cg-types.h"
#include "cg-device.h"
#include "cg-gl-header.h"
#include "cg-texture.h"

#ifdef CG_GL_DEBUG

const char *_cg_gl_error_to_string(GLenum error_code);

#define GE(ctx, x)                                                             \
    C_STMT_START                                                               \
    {                                                                          \
        GLenum __err;                                                          \
        (ctx)->x;                                                              \
        while ((__err = (ctx)->glGetError()) != GL_NO_ERROR) {                 \
            c_warning("%s: GL error (%d): %s\n",                               \
                      C_STRLOC,                                                \
                      __err,                                                   \
                      _cg_gl_error_to_string(__err));                          \
        }                                                                      \
    }                                                                          \
    C_STMT_END

#define GE_RET(ret, ctx, x)                                                    \
    C_STMT_START                                                               \
    {                                                                          \
        GLenum __err;                                                          \
        ret = (ctx)->x;                                                        \
        while ((__err = (ctx)->glGetError()) != GL_NO_ERROR) {                 \
            c_warning("%s: GL error (%d): %s\n",                               \
                      C_STRLOC,                                                \
                      __err,                                                   \
                      _cg_gl_error_to_string(__err));                          \
        }                                                                      \
    }                                                                          \
    C_STMT_END

#else /* !CG_GL_DEBUG */

#define GE(ctx, x) ((ctx)->x)
#define GE_RET(ret, ctx, x) (ret = ((ctx)->x))

#endif /* CG_GL_DEBUG */

bool _cg_gl_util_catch_out_of_memory(cg_device_t *dev, cg_error_t **error);

void _cg_gl_util_get_texture_target_string(cg_texture_type_t texture_type,
                                           const char **target_string_out,
                                           const char **swizzle_out);

/* Parses a GL version number stored in a string. @version_string must
 * point to the beginning of the version number (ie, it can't point to
 * the "OpenGL ES" part on GLES). The version number can be followed
 * by the end of the string, a space or a full stop. Anything else
 * will be treated as invalid. Returns true and sets major_out and
 * minor_out if it is succesfully parsed or false otherwise. */
bool _cg_gl_util_parse_gl_version(const char *version_string,
                                  int *major_out,
                                  int *minor_out);

#endif /* _CG_UTIL_GL_PRIVATE_H_ */
