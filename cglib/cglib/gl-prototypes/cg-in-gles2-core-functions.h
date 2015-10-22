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

CG_EXT_BEGIN(offscreen,
             3,
             0,
             CG_EXT_IN_GLES2,
             /* for some reason the ARB version of this
                extension doesn't have an ARB suffix for the
                functions */
             "ARB:\0EXT\0OES\0",
             "framebuffer_object\0")
CG_EXT_FUNCTION(void, glGenRenderbuffers, (GLsizei n, GLuint *renderbuffers))
CG_EXT_FUNCTION(void,
                glDeleteRenderbuffers,
                (GLsizei n, const GLuint *renderbuffers))
CG_EXT_FUNCTION(void, glBindRenderbuffer, (GLenum target, GLuint renderbuffer))
CG_EXT_FUNCTION(
    void,
    glRenderbufferStorage,
    (GLenum target, GLenum internalformat, GLsizei width, GLsizei height))
CG_EXT_FUNCTION(void, glGenFramebuffers, (GLsizei n, GLuint *framebuffers))
CG_EXT_FUNCTION(void, glBindFramebuffer, (GLenum target, GLuint framebuffer))
CG_EXT_FUNCTION(void,
                glFramebufferTexture2D,
                (GLenum target,
                 GLenum attachment,
                 GLenum textarget,
                 GLuint texture,
                 GLint level))
CG_EXT_FUNCTION(void,
                glFramebufferRenderbuffer,
                (GLenum target,
                 GLenum attachment,
                 GLenum renderbuffertarget,
                 GLuint renderbuffer))
CG_EXT_FUNCTION(GLboolean, glIsRenderbuffer, (GLuint renderbuffer))
CG_EXT_FUNCTION(GLenum, glCheckFramebufferStatus, (GLenum target))
CG_EXT_FUNCTION(void,
                glDeleteFramebuffers,
                (GLsizei n, const GLuint *framebuffers))
CG_EXT_FUNCTION(void, glGenerateMipmap, (GLenum target))
CG_EXT_FUNCTION(void,
                glGetFramebufferAttachmentParameteriv,
                (GLenum target, GLenum attachment, GLenum pname, GLint *params))
CG_EXT_FUNCTION(void,
                glGetRenderbufferParameteriv,
                (GLenum target, GLenum pname, GLint *params))
CG_EXT_FUNCTION(GLboolean, glIsFramebuffer, (GLuint framebuffer))
CG_EXT_END()

CG_EXT_BEGIN(blending, 1, 2, CG_EXT_IN_GLES2, "\0", "\0")
CG_EXT_FUNCTION(void, glBlendEquation, (GLenum mode))
CG_EXT_FUNCTION(void,
                glBlendColor,
                (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha))
CG_EXT_END()

/* Optional, declared in 1.4 or GLES 1.2 */
CG_EXT_BEGIN(blend_func_separate,
             1,
             4,
             CG_EXT_IN_GLES2,
             "EXT\0",
             "blend_func_separate\0")
CG_EXT_FUNCTION(
    void,
    glBlendFuncSeparate,
    (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha))
CG_EXT_END()

/* Optional, declared in 2.0 */
CG_EXT_BEGIN(blend_equation_separate,
             2,
             0,
             CG_EXT_IN_GLES2,
             "EXT\0",
             "blend_equation_separate\0")
CG_EXT_FUNCTION(void,
                glBlendEquationSeparate,
                (GLenum modeRGB, GLenum modeAlpha))
CG_EXT_END()

CG_EXT_BEGIN(
    gles2_only_api, 4, 1, CG_EXT_IN_GLES2, "ARB:\0", "ES2_compatibility\0")
CG_EXT_FUNCTION(void, glReleaseShaderCompiler, (void))
CG_EXT_FUNCTION(
    void,
    glGetShaderPrecisionFormat,
    (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision))
CG_EXT_FUNCTION(void,
                glShaderBinary,
                (GLsizei n,
                 const GLuint *shaders,
                 GLenum binaryformat,
                 const GLvoid *binary,
                 GLsizei length))
CG_EXT_FUNCTION(void, glDepthRangef, (GLfloat near_val, GLfloat far_val))
CG_EXT_FUNCTION(void, glClearDepthf, (GLclampf depth))
CG_EXT_END()

/* GL 1.3 and GLES 2.0 apis */
CG_EXT_BEGIN(only_in_gles2_and_gl_1_3, 1, 3, CG_EXT_IN_GLES2, "\0", "\0")
CG_EXT_FUNCTION(void,
                glCompressedTexImage2D,
                (GLenum target,
                 GLint level,
                 GLenum internalformat,
                 GLsizei width,
                 GLsizei height,
                 GLint border,
                 GLsizei imageSize,
                 const GLvoid *data))
CG_EXT_FUNCTION(void,
                glCompressedTexSubImage2D,
                (GLenum target,
                 GLint level,
                 GLint xoffset,
                 GLint yoffset,
                 GLsizei width,
                 GLsizei height,
                 GLenum format,
                 GLsizei imageSize,
                 const GLvoid *data))
CG_EXT_FUNCTION(void, glSampleCoverage, (GLclampf value, GLboolean invert))
CG_EXT_END()

/* Available in GL 1.3, the multitexture extension or GLES2.
 * Note: this api is a hard requirement for CGlib. */
CG_EXT_BEGIN(
    multitexture_part0, 1, 3, CG_EXT_IN_GLES2, "ARB\0", "multitexture\0")
CG_EXT_FUNCTION(void, glActiveTexture, (GLenum texture))
CG_EXT_END()

/* GL 1.5 and GLES 2.0 apis */
CG_EXT_BEGIN(only_in_gles2_and_gl_1_5, 1, 5, CG_EXT_IN_GLES2, "\0", "\0")
CG_EXT_FUNCTION(void,
                glGetBufferParameteriv,
                (GLenum target, GLenum pname, GLint *params))
CG_EXT_END()

CG_EXT_BEGIN(vbos, 1, 5, CG_EXT_IN_GLES2, "ARB\0", "vertex_buffer_object\0")
CG_EXT_FUNCTION(void, glGenBuffers, (GLsizei n, GLuint *buffers))
CG_EXT_FUNCTION(void, glBindBuffer, (GLenum target, GLuint buffer))
CG_EXT_FUNCTION(
    void,
    glBufferData,
    (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage))
CG_EXT_FUNCTION(
    void,
    glBufferSubData,
    (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data))
CG_EXT_FUNCTION(void, glDeleteBuffers, (GLsizei n, const GLuint *buffers))
CG_EXT_FUNCTION(GLboolean, glIsBuffer, (GLuint buffer))
CG_EXT_END()

/* GL and GLES 2.0 apis */
CG_EXT_BEGIN(two_point_zero_api, 2, 0, CG_EXT_IN_GLES2, "\0", "\0")
CG_EXT_FUNCTION(void,
                glStencilFuncSeparate,
                (GLenum face, GLenum func, GLint ref, GLuint mask))
CG_EXT_FUNCTION(void, glStencilMaskSeparate, (GLenum face, GLuint mask))
CG_EXT_FUNCTION(void,
                glStencilOpSeparate,
                (GLenum face, GLenum fail, GLenum zfail, GLenum zpass))
CG_EXT_END()
