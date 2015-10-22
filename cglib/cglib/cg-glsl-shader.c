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
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-glsl-shader-private.h"
#include "cg-glsl-shader-boilerplate.h"

#include <string.h>

#include <clib.h>

void
_cg_glsl_shader_set_source_with_boilerplate(cg_device_t *dev,
                                            GLuint shader_gl_handle,
                                            GLenum shader_gl_type,
                                            GLsizei count_in,
                                            const char **strings_in,
                                            const GLint *lengths_in)
{
    const char *vertex_boilerplate;
    const char *fragment_boilerplate;

    const char **strings = c_alloca(sizeof(char *) * (count_in + 6));
    GLint *lengths = c_alloca(sizeof(GLint) * (count_in + 6));
    char *version_string;
    int count = 0;

    vertex_boilerplate = _CG_VERTEX_SHADER_BOILERPLATE;
    fragment_boilerplate = _CG_FRAGMENT_SHADER_BOILERPLATE;

    version_string =
        c_strdup_printf("#version %i\n\n", dev->glsl_version_to_use);

    strings[count] = version_string;
    lengths[count++] = -1;

    if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_GL_EMBEDDED) &&
        cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_3D)) {
        static const char texture_3d_extension[] =
            "#extension GL_OES_texture_3D : enable\n";
        strings[count] = texture_3d_extension;
        lengths[count++] = sizeof(texture_3d_extension) - 1;
    }

    if (shader_gl_type == GL_VERTEX_SHADER) {
        if (dev->glsl_version_to_use < 130) {
            strings[count] = "#define in attribute\n"
                             "#define out varying\n";
            lengths[count++] = -1;
        } else {
            /* To support source compatibility with glsl >= 1.3 which has
             * replaced
             * all of the texture sampler functions with one polymorphic
             * texture()
             * function we use the preprocessor to map the old names onto the
             * new
             * name...
             */
            strings[count] = "#define texture2D texture\n"
                             "#define texture3D texture\n"
                             "#define textureRect texture\n";
            lengths[count++] = -1;
        }

        strings[count] = vertex_boilerplate;
        lengths[count++] = strlen(vertex_boilerplate);
    } else if (shader_gl_type == GL_FRAGMENT_SHADER) {
        if (dev->glsl_version_to_use < 130) {
            strings[count] = "#define in varying\n"
                             "\n"
                             "#define cg_color_out gl_FragColor\n"
                             "#define cg_depth_out gl_FragDepth\n";
            lengths[count++] = -1;
        }

        strings[count] = fragment_boilerplate;
        lengths[count++] = strlen(fragment_boilerplate);

        if (dev->glsl_version_to_use >= 130) {
            /* FIXME: Support cg_depth_out. */
            /* To support source compatibility with glsl >= 1.3 which has
             * replaced
             * all of the texture sampler functions with one polymorphic
             * texture()
             * function we use the preprocessor to map the old names onto the
             * new
             * name...
             */
            strings[count] = "#define texture2D texture\n"
                             "#define texture3D texture\n"
                             "#define textureRect texture\n"
                             "\n"
                             "out vec4 cg_color_out;\n";
            //"out vec4 cg_depth_out\n";
            lengths[count++] = -1;
        }
    }

    memcpy(strings + count, strings_in, sizeof(char *) * count_in);
    if (lengths_in)
        memcpy(lengths + count, lengths_in, sizeof(GLint) * count_in);
    else {
        int i;

        for (i = 0; i < count_in; i++)
            lengths[count + i] = -1; /* null terminated */
    }
    count += count_in;

    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_SHOW_SOURCE))) {
        c_string_t *buf = c_string_new(NULL);
        int i;

        c_string_append_printf(buf,
                               "%s shader:\n",
                               shader_gl_type == GL_VERTEX_SHADER ? "vertex"
                               : "fragment");
        for (i = 0; i < count; i++)
            if (lengths[i] != -1)
                c_string_append_len(buf, strings[i], lengths[i]);
            else
                c_string_append(buf, strings[i]);

        c_message("%s", buf->str);

        c_string_free(buf, true);
    }

    GE(dev,
       glShaderSource(
           shader_gl_handle, count, (const char **)strings, lengths));

    c_free(version_string);
}
