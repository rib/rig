/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009, 2011 Intel Corporation.
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

/* This is included multiple times with different definitions for
 * these macros. The macros are given the following arguments:
 *
 * CG_EXT_BEGIN:
 *
 * @name: a unique symbol name for this feature
 *
 * @min_gl_major: the major part of the minimum GL version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 * @min_gl_minor: the minor part of the minimum GL version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 *
 * @gles_availability: flags to specify which versions of GLES the
 * functions are available in. This is a leftover from when we
 * supported GLES1 and currently the only value that can go here is
 * CG_EXT_IN_GLES2.
 *
 * @extension_suffixes: A zero-separated list of suffixes in a
 * string. These are appended to the extension name to get a complete
 * extension name to try. The suffix is also appended to all of the
 * function names. The suffix can optionally include a ':' to specify
 * an alternate suffix for the function names.
 *
 * @extension_names: A list of extension names to try. If any of these
 * extensions match then it will be used.
 */

/* These are the core GL functions which we assume will always be
   available */
CG_EXT_BEGIN(core, 0, 0, CG_EXT_IN_GLES2, "\0", "\0")
CG_EXT_FUNCTION(void, glBindTexture, (GLenum target, GLuint texture))
CG_EXT_FUNCTION(void, glBlendFunc, (GLenum sfactor, GLenum dfactor))
CG_EXT_FUNCTION(void, glClear, (GLbitfield mask))
CG_EXT_FUNCTION(void,
                glClearColor,
                (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha))
CG_EXT_FUNCTION(void, glClearStencil, (GLint s))
CG_EXT_FUNCTION(
    void,
    glColorMask,
    (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha))
CG_EXT_FUNCTION(void,
                glCopyTexSubImage2D,
                (GLenum target,
                 GLint level,
                 GLint xoffset,
                 GLint yoffset,
                 GLint x,
                 GLint y,
                 GLsizei width,
                 GLsizei height))
CG_EXT_FUNCTION(void, glDeleteTextures, (GLsizei n, const GLuint *textures))
CG_EXT_FUNCTION(void, glDepthFunc, (GLenum func))
CG_EXT_FUNCTION(void, glDepthMask, (GLboolean flag))
CG_EXT_FUNCTION(void, glDisable, (GLenum cap))
CG_EXT_FUNCTION(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))
CG_EXT_FUNCTION(
    void,
    glDrawElements,
    (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices))
CG_EXT_FUNCTION(void, glEnable, (GLenum cap))
CG_EXT_FUNCTION(void, glFinish, (void))
CG_EXT_FUNCTION(void, glFlush, (void))
CG_EXT_FUNCTION(void, glFrontFace, (GLenum mode))
CG_EXT_FUNCTION(void, glCullFace, (GLenum mode))
CG_EXT_FUNCTION(void, glGenTextures, (GLsizei n, GLuint *textures))
CG_EXT_FUNCTION(GLenum, glGetError, (void))
CG_EXT_FUNCTION(void, glGetIntegerv, (GLenum pname, GLint *params))
CG_EXT_FUNCTION(void, glGetBooleanv, (GLenum pname, GLboolean *params))
CG_EXT_FUNCTION(void, glGetFloatv, (GLenum pname, GLfloat *params))
CG_EXT_FUNCTION(const GLubyte *, glGetString, (GLenum name))
CG_EXT_FUNCTION(void, glHint, (GLenum target, GLenum mode))
CG_EXT_FUNCTION(GLboolean, glIsTexture, (GLuint texture))
CG_EXT_FUNCTION(void, glPixelStorei, (GLenum pname, GLint param))
CG_EXT_FUNCTION(void,
                glReadPixels,
                (GLint x,
                 GLint y,
                 GLsizei width,
                 GLsizei height,
                 GLenum format,
                 GLenum type,
                 GLvoid *pixels))
CG_EXT_FUNCTION(void,
                glScissor,
                (GLint x, GLint y, GLsizei width, GLsizei height))
CG_EXT_FUNCTION(void, glStencilFunc, (GLenum func, GLint ref, GLuint mask))
CG_EXT_FUNCTION(void, glStencilMask, (GLuint mask))
CG_EXT_FUNCTION(void, glStencilOp, (GLenum fail, GLenum zfail, GLenum zpass))
CG_EXT_FUNCTION(void,
                glTexImage2D,
                (GLenum target,
                 GLint level,
                 GLint internalformat,
                 GLsizei width,
                 GLsizei height,
                 GLint border,
                 GLenum format,
                 GLenum type,
                 const GLvoid *pixels))
CG_EXT_FUNCTION(void,
                glTexParameterf,
                (GLenum target, GLenum pname, GLfloat param))
CG_EXT_FUNCTION(void,
                glTexParameterfv,
                (GLenum target, GLenum pname, const GLfloat *params))
CG_EXT_FUNCTION(void,
                glTexParameteri,
                (GLenum target, GLenum pname, GLint param))
CG_EXT_FUNCTION(void,
                glTexParameteriv,
                (GLenum target, GLenum pname, const GLint *params))
CG_EXT_FUNCTION(void,
                glGetTexParameterfv,
                (GLenum target, GLenum pname, GLfloat *params))
CG_EXT_FUNCTION(void,
                glGetTexParameteriv,
                (GLenum target, GLenum pname, GLint *params))
CG_EXT_FUNCTION(void,
                glTexSubImage2D,
                (GLenum target,
                 GLint level,
                 GLint xoffset,
                 GLint yoffset,
                 GLsizei width,
                 GLsizei height,
                 GLenum format,
                 GLenum type,
                 const GLvoid *pixels))
CG_EXT_FUNCTION(void,
                glCopyTexImage2D,
                (GLenum target,
                 GLint level,
                 GLenum internalformat,
                 GLint x,
                 GLint y,
                 GLsizei width,
                 GLsizei height,
                 GLint border))
CG_EXT_FUNCTION(void,
                glViewport,
                (GLint x, GLint y, GLsizei width, GLsizei height))
CG_EXT_FUNCTION(GLboolean, glIsEnabled, (GLenum cap))
CG_EXT_FUNCTION(void, glLineWidth, (GLfloat width))
CG_EXT_FUNCTION(void, glPolygonOffset, (GLfloat factor, GLfloat units))
CG_EXT_END()
