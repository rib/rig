/*
 * Cogl
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-glsl-shader-private.h"
#include "cogl-glsl-shader-boilerplate.h"

#include <string.h>

#include <ulib.h>

void
_cogl_glsl_shader_set_source_with_boilerplate (CoglContext *ctx,
                                               GLuint shader_gl_handle,
                                               GLenum shader_gl_type,
                                               GLsizei count_in,
                                               const char **strings_in,
                                               const GLint *lengths_in)
{
  const char *vertex_boilerplate;
  const char *fragment_boilerplate;

  const char **strings = u_alloca (sizeof (char *) * (count_in + 4));
  GLint *lengths = u_alloca (sizeof (GLint) * (count_in + 4));
  char *version_string;
  int count = 0;

  vertex_boilerplate = _COGL_VERTEX_SHADER_BOILERPLATE;
  fragment_boilerplate = _COGL_FRAGMENT_SHADER_BOILERPLATE;

  version_string = u_strdup_printf ("#version %i\n\n",
                                    ctx->glsl_version_to_use);
  strings[count] = version_string;
  lengths[count++] = -1;

  if (_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_GL_EMBEDDED) &&
      cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_3D))
    {
      static const char texture_3d_extension[] =
        "#extension GL_OES_texture_3D : enable\n";
      strings[count] = texture_3d_extension;
      lengths[count++] = sizeof (texture_3d_extension) - 1;
    }

  if (shader_gl_type == GL_VERTEX_SHADER)
    {
      strings[count] = vertex_boilerplate;
      lengths[count++] = strlen (vertex_boilerplate);
    }
  else if (shader_gl_type == GL_FRAGMENT_SHADER)
    {
      strings[count] = fragment_boilerplate;
      lengths[count++] = strlen (fragment_boilerplate);
    }

  memcpy (strings + count, strings_in, sizeof (char *) * count_in);
  if (lengths_in)
    memcpy (lengths + count, lengths_in, sizeof (GLint) * count_in);
  else
    {
      int i;

      for (i = 0; i < count_in; i++)
        lengths[count + i] = -1; /* null terminated */
    }
  count += count_in;

  if (U_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_SHOW_SOURCE)))
    {
      UString *buf = u_string_new (NULL);
      int i;

      u_string_append_printf (buf,
                              "%s shader:\n",
                              shader_gl_type == GL_VERTEX_SHADER ?
                              "vertex" : "fragment");
      for (i = 0; i < count; i++)
        if (lengths[i] != -1)
          u_string_append_len (buf, strings[i], lengths[i]);
        else
          u_string_append (buf, strings[i]);

      u_message ("%s", buf->str);

      u_string_free (buf, TRUE);
    }

  GE( ctx, glShaderSource (shader_gl_handle, count,
                           (const char **) strings, lengths) );

  u_free (version_string);
}
