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

#include <cglib-config.h>

#include "cg-types.h"
#include "cg-device-private.h"
#include "cg-error-private.h"
#include "cg-util-gl-private.h"

#ifdef CG_GL_DEBUG
/* GL error to string conversion */
static const struct {
    GLuint error_code;
    const char *error_string;
} gl_errors[] = {
    { GL_NO_ERROR, "No error" },
    { GL_INVALID_ENUM, "Invalid enumeration value" },
    { GL_INVALID_VALUE, "Invalid value" },
    { GL_INVALID_OPERATION, "Invalid operation" },
#ifdef CG_HAS_GL_SUPPORT
    { GL_STACK_OVERFLOW, "Stack overflow" },
    { GL_STACK_UNDERFLOW, "Stack underflow" },
#endif
    { GL_OUT_OF_MEMORY, "Out of memory" },

#ifdef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
    { GL_INVALID_FRAMEBUFFER_OPERATION_EXT, "Invalid framebuffer operation" }
#endif
};

static const unsigned int n_gl_errors = C_N_ELEMENTS(gl_errors);

const char *
_cg_gl_error_to_string(GLenum error_code)
{
    int i;

    for (i = 0; i < n_gl_errors; i++) {
        if (gl_errors[i].error_code == error_code)
            return gl_errors[i].error_string;
    }

    return "Unknown GL error";
}
#endif /* CG_GL_DEBUG */

bool
_cg_gl_util_catch_out_of_memory(cg_device_t *dev, cg_error_t **error)
{
    GLenum gl_error;
    bool out_of_memory = false;

    while ((gl_error = dev->glGetError()) != GL_NO_ERROR) {
        if (gl_error == GL_OUT_OF_MEMORY)
            out_of_memory = true;
#ifdef CG_GL_DEBUG
        else {
            c_warning("%s: GL error (%d): %s\n",
                      C_STRLOC,
                      gl_error,
                      _cg_gl_error_to_string(gl_error));
        }
#endif
    }

    if (out_of_memory) {
        _cg_set_error(
            error, CG_SYSTEM_ERROR, CG_SYSTEM_ERROR_NO_MEMORY, "Out of memory");
        return true;
    }

    return false;
}

void
_cg_gl_util_get_texture_target_string(cg_texture_type_t texture_type,
                                      const char **target_string_out,
                                      const char **swizzle_out)
{
    const char *target_string, *tex_coord_swizzle;

    switch (texture_type) {
    case CG_TEXTURE_TYPE_2D:
        target_string = "2D";
        tex_coord_swizzle = "st";
        break;

    case CG_TEXTURE_TYPE_3D:
        target_string = "3D";
        tex_coord_swizzle = "stp";
        break;
    }

    if (target_string_out)
        *target_string_out = target_string;
    if (swizzle_out)
        *swizzle_out = tex_coord_swizzle;
}

bool
_cg_gl_util_parse_gl_version(const char *version_string,
                             int *major_out,
                             int *minor_out)
{
    const char *major_end, *minor_end;
    int major = 0, minor = 0;

    /* Extract the major number */
    for (major_end = version_string; *major_end >= '0' && *major_end <= '9';
         major_end++)
        major = (major * 10) + *major_end - '0';
    /* If there were no digits or the major number isn't followed by a
       dot then it is invalid */
    if (major_end == version_string || *major_end != '.')
        return false;

    /* Extract the minor number */
    for (minor_end = major_end + 1; *minor_end >= '0' && *minor_end <= '9';
         minor_end++)
        minor = (minor * 10) + *minor_end - '0';
    /* If there were no digits or there is an unexpected character then
       it is invalid */
    if (minor_end == major_end + 1 ||
        (*minor_end && *minor_end != ' ' && *minor_end != '.'))
        return false;

    *major_out = major;
    *minor_out = minor;

    return true;
}
