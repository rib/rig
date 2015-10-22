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

/* The functions in this file are part of the core GL, and GLES2 apis */
#include "cg-core-functions.h"

/* The functions in this file are core to GLES2 only but may be
 * extensions for GL */
#include "cg-in-gles2-core-functions.h"

/* These are fixed-function APIs core to GL */
#include "cg-fixed-functions.h"

/* These are GLSL shader APIs core to GL 2.0 and GLES2 */
#include "cg-glsl-functions.h"

/* These are the core GL functions which are only available in big
   GL */
CG_EXT_BEGIN(only_in_big_gl,
             0,
             0,
             0, /* not in GLES */
             "\0",
             "\0")
CG_EXT_FUNCTION(void,
                glGetTexLevelParameteriv,
                (GLenum target, GLint level, GLenum pname, GLint *params))
CG_EXT_FUNCTION(
    void,
    glGetTexImage,
    (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels))
CG_EXT_FUNCTION(void, glClipPlane, (GLenum plane, const double *equation))
CG_EXT_FUNCTION(void, glDepthRange, (double near_val, double far_val))
CG_EXT_FUNCTION(void, glDrawBuffer, (GLenum mode))
CG_EXT_END()

/* GLES doesn't support mapping buffers in core so this has to be a
   separate check */
CG_EXT_BEGIN(map_vbos,
             1,
             5,
             0, /* not in GLES core */
             "ARB\0OES\0",
             "vertex_buffer_object\0mapbuffer\0")
CG_EXT_FUNCTION(void *, glMapBuffer, (GLenum target, GLenum access))
CG_EXT_FUNCTION(GLboolean, glUnmapBuffer, (GLenum target))
CG_EXT_END()

CG_EXT_BEGIN(texture_3d,
             1,
             2,
             0, /* not in GLES */
             "OES\0",
             "texture_3D\0")
CG_EXT_FUNCTION(void,
                glTexImage3D,
                (GLenum target,
                 GLint level,
                 GLint internalFormat,
                 GLsizei width,
                 GLsizei height,
                 GLsizei depth,
                 GLint border,
                 GLenum format,
                 GLenum type,
                 const GLvoid *pixels))
CG_EXT_FUNCTION(void,
                glTexSubImage3D,
                (GLenum target,
                 GLint level,
                 GLint xoffset,
                 GLint yoffset,
                 GLint zoffset,
                 GLsizei width,
                 GLsizei height,
                 GLsizei depth,
                 GLenum format,
                 GLenum type,
                 const GLvoid *pixels))
CG_EXT_END()

CG_EXT_BEGIN(offscreen_blit,
             3,
             0,
             0, /* not in GLES */
             "EXT\0ANGLE\0",
             "framebuffer_blit\0")
CG_EXT_FUNCTION(void,
                glBlitFramebuffer,
                (GLint srcX0,
                 GLint srcY0,
                 GLint srcX1,
                 GLint srcY1,
                 GLint dstX0,
                 GLint dstY0,
                 GLint dstX1,
                 GLint dstY1,
                 GLbitfield mask,
                 GLenum filter))
CG_EXT_END()

CG_EXT_BEGIN(EGL_image,
             255,
             255,
             0, /* not in GLES */
             "OES\0",
             "EGL_image\0")
CG_EXT_FUNCTION(void,
                glEGLImageTargetTexture2D,
                (GLenum target, GLeglImageOES image))
CG_EXT_FUNCTION(void,
                glEGLImageTargetRenderbufferStorage,
                (GLenum target, GLeglImageOES image))
CG_EXT_END()

CG_EXT_BEGIN(framebuffer_discard,
             255,
             255,
             0, /* not in GLES */
             "EXT\0",
             "framebuffer_discard\0")
CG_EXT_FUNCTION(void,
                glDiscardFramebuffer,
                (GLenum target,
                 GLsizei numAttachments,
                 const GLenum *attachments))
CG_EXT_END()

CG_EXT_BEGIN(IMG_multisampled_render_to_texture,
             255,
             255,
             0, /* not in GLES */
             "\0",
             "IMG_multisampled_render_to_texture\0")
CG_EXT_FUNCTION(void,
                glRenderbufferStorageMultisampleIMG,
                (GLenum target,
                 GLsizei samples,
                 GLenum internal_format,
                 GLsizei width,
                 GLsizei height))
CG_EXT_FUNCTION(void,
                glFramebufferTexture2DMultisampleIMG,
                (GLenum target,
                 GLenum attachment,
                 GLenum textarget,
                 GLuint texture,
                 GLint level,
                 GLsizei samples))
CG_EXT_END()

CG_EXT_BEGIN(ARB_sampler_objects,
             3,
             3,
             0, /* not in GLES */
             "ARB:\0",
             "sampler_objects\0")
CG_EXT_FUNCTION(void, glGenSamplers, (GLsizei count, GLuint *samplers))
CG_EXT_FUNCTION(void, glDeleteSamplers, (GLsizei count, const GLuint *samplers))
CG_EXT_FUNCTION(void, glBindSampler, (GLuint unit, GLuint sampler))
CG_EXT_FUNCTION(void,
                glSamplerParameteri,
                (GLuint sampler, GLenum pname, GLint param))
CG_EXT_END()

/* These only list functions that come from the old GLSL extensions.
 * Functions that are common to the extensions and GLSL 2.0 should
 * instead be listed in cg-glsl-functions.h */
CG_EXT_BEGIN(shader_objects,
             255,
             255,
             0, /* not in GLES */
             "ARB\0",
             "shader_objects\0")
CG_EXT_FUNCTION(GLuint, glCreateProgramObject, (void))
CG_EXT_FUNCTION(GLuint, glCreateShaderObject, (GLenum shaderType))
CG_EXT_FUNCTION(void, glDeleteObject, (GLuint obj))
CG_EXT_FUNCTION(void, glAttachObject, (GLuint container, GLuint obj))
CG_EXT_FUNCTION(void, glUseProgramObject, (GLuint programObj))
CG_EXT_FUNCTION(void,
                glGetInfoLog,
                (GLuint obj, GLsizei maxLength, GLsizei *length, char *infoLog))
CG_EXT_FUNCTION(void,
                glGetObjectParameteriv,
                (GLuint obj, GLenum pname, GLint *params))
CG_EXT_FUNCTION(void, glDetachObject, (GLuint container, GLuint obj))
CG_EXT_FUNCTION(
    void,
    glGetAttachedObjects,
    (GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders))
CG_EXT_END()

CG_EXT_BEGIN(only_gl3,
             3,
             0,
             0, /* not in GLES */
             "\0",
             "\0")
CG_EXT_FUNCTION(const GLubyte *, glGetStringi, (GLenum name, GLuint index))
CG_EXT_END()

CG_EXT_BEGIN(vertex_array_object,
             3,
             0,
             0, /* not in GLES */
             "ARB\0OES\0",
             "vertex_array_object\0")
CG_EXT_FUNCTION(void, glBindVertexArray, (GLuint array))
CG_EXT_FUNCTION(void, glDeleteVertexArrays, (GLsizei n, const GLuint *arrays))
CG_EXT_FUNCTION(void, glGenVertexArrays, (GLsizei n, GLuint *arrays))
CG_EXT_END()

CG_EXT_BEGIN(map_region,
             3,
             0,
             0, /* not in GLES */
             "ARB:\0",
             "map_buffer_range\0")
CG_EXT_FUNCTION(
    GLvoid *,
    glMapBufferRange,
    (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access))
CG_EXT_END()

#ifdef GL_ARB_sync
CG_EXT_BEGIN(sync,
             3,
             2,
             0, /* not in GLES */
             "ARB:\0",
             "sync\0")
CG_EXT_FUNCTION(GLsync, glFenceSync, (GLenum condition, GLbitfield flags))
CG_EXT_FUNCTION(GLenum,
                glClientWaitSync,
                (GLsync sync, GLbitfield flags, GLuint64 timeout))
CG_EXT_FUNCTION(void, glDeleteSync, (GLsync sync))
CG_EXT_END()
#endif

/* Note the check for multitexturing is split into two parts because
 * GLES2 has glActiveTexture() but not glClientActiveTexture()
 */
CG_EXT_BEGIN(multitexture_part1,
             1,
             3,
             0, /* not in GLES2 */
             "ARB\0",
             "multitexture\0")
CG_EXT_FUNCTION(void, glClientActiveTexture, (GLenum texture))
CG_EXT_END()

CG_EXT_BEGIN(
    draw_buffers, 2, 0, CG_EXT_IN_GLES3, "ARB\0EXT\0", "draw_buffers\0")
CG_EXT_FUNCTION(void, glDrawBuffers, (GLsizei n, const GLenum *bufs))
CG_EXT_END()


CG_EXT_BEGIN(instanced_arrays, 3, 1, CG_EXT_IN_GLES3, "ANGLE\0ARB\0EXT\0", "instanced_arrays\0")
CG_EXT_FUNCTION(void, glVertexAttribDivisor, (GLuint index, GLuint divisor))
CG_EXT_FUNCTION(void, glDrawArraysInstanced,
                (GLenum mode, GLint first, GLsizei count, GLsizei primcount))
CG_EXT_FUNCTION(void, glDrawElementsInstanced,
                (GLenum mode,
                 GLsizei count,
                 GLenum type,
                 const GLvoid *indices,
                 GLsizei primcount))
CG_EXT_END()
