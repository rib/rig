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

/* These are the core GL functions which are available with the GL
 * compatibility profile */
CG_EXT_BEGIN(fixed_function_core,
             0,
             0,
             0, /* Not in GLES */
             "\0",
             "\0")
CG_EXT_FUNCTION(void, glAlphaFunc, (GLenum func, GLclampf ref))
CG_EXT_FUNCTION(void, glFogf, (GLenum pname, GLfloat param))
CG_EXT_FUNCTION(void, glFogfv, (GLenum pname, const GLfloat *params))
CG_EXT_FUNCTION(void, glLoadMatrixf, (const GLfloat *m))
CG_EXT_FUNCTION(void,
                glMaterialfv,
                (GLenum face, GLenum pname, const GLfloat *params))
CG_EXT_FUNCTION(void, glPointSize, (GLfloat size))
CG_EXT_FUNCTION(void,
                glTexEnvfv,
                (GLenum target, GLenum pname, const GLfloat *params))
CG_EXT_FUNCTION(void,
                glColor4ub,
                (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha))
CG_EXT_FUNCTION(void,
                glColor4f,
                (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))
CG_EXT_FUNCTION(
    void,
    glColorPointer,
    (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))
CG_EXT_FUNCTION(void, glDisableClientState, (GLenum array))
CG_EXT_FUNCTION(void, glEnableClientState, (GLenum array))
CG_EXT_FUNCTION(void, glLoadIdentity, (void))
CG_EXT_FUNCTION(void, glMatrixMode, (GLenum mode))
CG_EXT_FUNCTION(void, glNormal3f, (GLfloat x, GLfloat y, GLfloat z))
CG_EXT_FUNCTION(void,
                glNormalPointer,
                (GLenum type, GLsizei stride, const GLvoid *pointer))
CG_EXT_FUNCTION(void,
                glMultiTexCoord4f,
                (GLfloat s, GLfloat t, GLfloat r, GLfloat q))
CG_EXT_FUNCTION(
    void,
    glTexCoordPointer,
    (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))
CG_EXT_FUNCTION(void, glTexEnvi, (GLenum target, GLenum pname, GLint param))
CG_EXT_FUNCTION(void, glVertex4f, (GLfloat x, GLfloat y, GLfloat z, GLfloat w))
CG_EXT_FUNCTION(
    void,
    glVertexPointer,
    (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))
CG_EXT_END()
