/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * Authors:
 *  Tomeu Vizoso <tomeu.vizoso@collabora.com>
 *  Robert Bragg <robert@linux.intel.com>
 *  Neil Roberts <neil@linux.intel.com>
 *
 */

#include <cglib-config.h>

#include <string.h>

#include <clib.h>

#include "cg-gles2.h"
#include "cg-gles2-context-private.h"

#include "cg-device-private.h"
#include "cg-display-private.h"
#include "cg-framebuffer-private.h"
#include "cg-framebuffer-gl-private.h"
#include "cg-onscreen-template-private.h"
#include "cg-renderer-private.h"
#include "cg-texture-2d-gl.h"
#include "cg-texture-2d-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-error-private.h"

static void _cg_gles2_context_free(cg_gles2_context_t *gles2_context);

CG_OBJECT_DEFINE(GLES2Context, gles2_context);

static cg_gles2_context_t *current_gles2_context;

static cg_user_data_key_t offscreen_wrapper_key;

/* The application's main function is renamed to this so that we can
 * provide an alternative main function */
#define MAIN_WRAPPER_REPLACEMENT_NAME "_c31"
/* This uniform is used to flip the rendering or not depending on
 * whether we are rendering to an offscreen buffer or not */
#define MAIN_WRAPPER_FLIP_UNIFORM "_cg_flip_vector"
/* These comments are used to delimit the added wrapper snippet so
 * that we can remove it again when the shader source is requested via
 * glGetShaderSource */
#define MAIN_WRAPPER_BEGIN "/*_CG_WRAPPER_BEGIN*/"
#define MAIN_WRAPPER_END "/*_CG_WRAPPER_END*/"

/* This wrapper function around 'main' is appended to every vertex shader
 * so that we can add some extra code to flip the rendering when
 * rendering to an offscreen buffer */
static const char main_wrapper_function[] =
    MAIN_WRAPPER_BEGIN "\n"
    "uniform vec4 " MAIN_WRAPPER_FLIP_UNIFORM ";\n"
    "\n"
    "void\n"
    "main ()\n"
    "{\n"
    "  " MAIN_WRAPPER_REPLACEMENT_NAME " ();\n"
    "  gl_Position *= " MAIN_WRAPPER_FLIP_UNIFORM ";\n"
    "}\n" MAIN_WRAPPER_END;

enum {
    RESTORE_FB_NONE,
    RESTORE_FB_FROM_OFFSCREEN,
    RESTORE_FB_FROM_ONSCREEN,
};

uint32_t
_cg_gles2_context_error_domain(void)
{
    return c_quark_from_static_string("cg-gles2-context-error-quark");
}

static void
shader_data_unref(cg_gles2_context_t *context,
                  cg_gles2_shader_data_t *shader_data)
{
    if (--shader_data->ref_count < 1)
        /* Removing the hash table entry should also destroy the data */
        c_hash_table_remove(context->shader_map,
                            C_INT_TO_POINTER(shader_data->object_id));
}

static void
program_data_unref(cg_gles2_program_data_t *program_data)
{
    if (--program_data->ref_count < 1)
        /* Removing the hash table entry should also destroy the data */
        c_hash_table_remove(program_data->context->program_map,
                            C_INT_TO_POINTER(program_data->object_id));
}

static void
detach_shader(cg_gles2_program_data_t *program_data,
              cg_gles2_shader_data_t *shader_data)
{
    c_llist_t *l;

    for (l = program_data->attached_shaders; l; l = l->next) {
        if (l->data == shader_data) {
            shader_data_unref(program_data->context, shader_data);
            program_data->attached_shaders =
                c_llist_delete_link(program_data->attached_shaders, l);
            break;
        }
    }
}

static bool
is_symbol_character(char ch)
{
    return c_ascii_isalnum(ch) || ch == '_';
}

static void
replace_token(char *string,
              const char *token,
              const char *replacement,
              int length)
{
    char *token_pos;
    char *last_pos = string;
    char *end = string + length;
    int token_length = strlen(token);

    /* NOTE: this assumes token and replacement are the same length */

    while ((token_pos = c_memmem(last_pos, end - last_pos,
                                 token, token_length)))
    {
        /* Make sure this isn't in the middle of some longer token */
        if ((token_pos <= string || !is_symbol_character(token_pos[-1])) &&
            (token_pos + token_length == end ||
             !is_symbol_character(token_pos[token_length])))
            memcpy(token_pos, replacement, token_length);

        last_pos = token_pos + token_length;
    }
}

static void
update_current_flip_state(cg_gles2_context_t *gles2_ctx)
{
    cg_gles2_flip_state_t new_flip_state;

    if (gles2_ctx->current_fbo_handle == 0 &&
        cg_is_offscreen(gles2_ctx->write_buffer))
        new_flip_state = CG_GLES2_FLIP_STATE_FLIPPED;
    else
        new_flip_state = CG_GLES2_FLIP_STATE_NORMAL;

    /* If the flip state has changed then we need to reflush all of the
     * dependent state */
    if (new_flip_state != gles2_ctx->current_flip_state) {
        gles2_ctx->viewport_dirty = true;
        gles2_ctx->scissor_dirty = true;
        gles2_ctx->front_face_dirty = true;
        gles2_ctx->current_flip_state = new_flip_state;
    }
}

static GLuint
get_current_texture_2d_object(cg_gles2_context_t *gles2_ctx)
{
    return c_array_index(gles2_ctx->texture_units,
                         cg_gles2_texture_unit_data_t,
                         gles2_ctx->current_texture_unit).current_texture_2d;
}

static void
set_texture_object_data(cg_gles2_context_t *gles2_ctx,
                        GLenum target,
                        GLint level,
                        GLenum internal_format,
                        GLsizei width,
                        GLsizei height)
{
    GLuint texture_id = get_current_texture_2d_object(gles2_ctx);
    cg_gles2_texture_object_data_t *texture_object;

    /* We want to keep track of all texture objects where the data is
     * created by this context so that we can delete them later */
    texture_object = c_hash_table_lookup(gles2_ctx->texture_object_map,
                                         C_UINT_TO_POINTER(texture_id));
    if (texture_object == NULL) {
        texture_object = c_slice_new0(cg_gles2_texture_object_data_t);
        texture_object->object_id = texture_id;

        c_hash_table_insert(gles2_ctx->texture_object_map,
                            C_UINT_TO_POINTER(texture_id),
                            texture_object);
    }

    switch (target) {
    case GL_TEXTURE_2D:
        texture_object->target = GL_TEXTURE_2D;

        /* We want to keep track of the dimensions of any texture object
         * setting the GL_TEXTURE_2D target */
        if (level == 0) {
            texture_object->width = width;
            texture_object->height = height;
            texture_object->format = internal_format;
        }
        break;

    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        texture_object->target = GL_TEXTURE_CUBE_MAP;
        break;
    }
}

static void
copy_flipped_texture(cg_gles2_context_t *gles2_ctx,
                     int level,
                     int src_x,
                     int src_y,
                     int dst_x,
                     int dst_y,
                     int width,
                     int height)
{
    GLuint tex_id = get_current_texture_2d_object(gles2_ctx);
    cg_gles2_texture_object_data_t *tex_object_data;
    cg_device_t *dev;
    const cg_winsys_vtable_t *winsys;
    cg_texture_2d_t *dst_texture;
    cg_pixel_format_t internal_format;

    tex_object_data = c_hash_table_lookup(gles2_ctx->texture_object_map,
                                          C_UINT_TO_POINTER(tex_id));

    /* We can't do anything if the application hasn't set a level 0
     * image on this texture object */
    if (tex_object_data == NULL || tex_object_data->target != GL_TEXTURE_2D ||
        tex_object_data->width <= 0 || tex_object_data->height <= 0)
        return;

    switch (tex_object_data->format) {
    case GL_RGB:
        internal_format = CG_PIXEL_FORMAT_RGB_888;
        break;

    case GL_RGBA:
        internal_format = CG_PIXEL_FORMAT_RGBA_8888_PRE;
        break;

    case GL_ALPHA:
        internal_format = CG_PIXEL_FORMAT_A_8;
        break;

    default:
        /* We can't handle this format so just give up */
        return;
    }

    dev = gles2_ctx->dev;
    winsys = dev->display->renderer->winsys_vtable;

    /* We need to make sure the rendering on the GLES2 context is
     * complete before the blit will be ready in the GLES2 context */
    dev->glFinish();
    /* We need to force CGlib to rebind the texture because according to
     * the GL spec a shared texture isn't guaranteed to be updated until
     * is rebound */
    _cg_get_texture_unit(dev, 0)->dirty_gl_texture = true;

    /* Temporarily switch back to the CGlib context */
    winsys->restore_context(dev);

    dst_texture = cg_gles2_texture_2d_new_from_handle(gles2_ctx->dev,
                                                      gles2_ctx,
                                                      tex_id,
                                                      tex_object_data->width,
                                                      tex_object_data->height,
                                                      internal_format);

    if (dst_texture) {
        cg_texture_t *src_texture =
            CG_OFFSCREEN(gles2_ctx->read_buffer)->texture;
        cg_pipeline_t *pipeline = cg_pipeline_new(dev);
        const cg_offscreen_flags_t flags =
            CG_OFFSCREEN_DISABLE_AUTO_DEPTH_AND_STENCIL;
        cg_offscreen_t *offscreen = _cg_offscreen_new_with_texture_full(
            CG_TEXTURE(dst_texture), flags, level);
        int src_width = cg_texture_get_width(src_texture);
        int src_height = cg_texture_get_height(src_texture);
        /* The framebuffer size might be different from the texture size
         * if a level > 0 is used */
        int dst_width = cg_framebuffer_get_width(CG_FRAMEBUFFER(offscreen));
        int dst_height = cg_framebuffer_get_height(CG_FRAMEBUFFER(offscreen));
        float x_1, y_1, x_2, y_2, s_1, t_1, s_2, t_2;

        cg_pipeline_set_layer_texture(pipeline, 0, src_texture);
        cg_pipeline_set_blend(pipeline, "RGBA = ADD(SRC_COLOR, 0)", NULL);
        cg_pipeline_set_layer_filters(pipeline,
                                      0, /* layer_num */
                                      CG_PIPELINE_FILTER_NEAREST,
                                      CG_PIPELINE_FILTER_NEAREST);

        x_1 = dst_x * 2.0f / dst_width - 1.0f;
        y_1 = dst_y * 2.0f / dst_height - 1.0f;
        x_2 = x_1 + width * 2.0f / dst_width;
        y_2 = y_1 + height * 2.0f / dst_height;

        s_1 = src_x / (float)src_width;
        t_1 = 1.0f - src_y / (float)src_height;
        s_2 = (src_x + width) / (float)src_width;
        t_2 = 1.0f - (src_y + height) / (float)src_height;

        cg_framebuffer_draw_textured_rectangle(CG_FRAMEBUFFER(offscreen),
                                               pipeline,
                                               x_1,
                                               y_1,
                                               x_2,
                                               y_2,
                                               s_1,
                                               t_1,
                                               s_2,
                                               t_2);

        _cg_framebuffer_flush(CG_FRAMEBUFFER(offscreen));

        /* We need to make sure the rendering is complete before the
         * blit will be ready in the GLES2 context */
        dev->glFinish();

        cg_object_unref(pipeline);
        cg_object_unref(dst_texture);
        cg_object_unref(offscreen);
    }

    winsys->set_gles2_context(gles2_ctx, NULL);

    /* From what I understand of the GL spec, changes to a shared object
     * are not guaranteed to be propagated to another context until that
     * object is rebound in that context so we can just rebind it
     * here */
    gles2_ctx->vtable->glBindTexture(GL_TEXTURE_2D, tex_id);
}

/* We wrap glBindFramebuffer so that when framebuffer 0 is bound
 * we can instead bind the write_framebuffer passed to
 * cg_push_gles2_context().
 */
static void
gl_bind_framebuffer_wrapper(GLenum target, GLuint framebuffer)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    gles2_ctx->current_fbo_handle = framebuffer;

    if (framebuffer == 0 && cg_is_offscreen(gles2_ctx->write_buffer)) {
        cg_gles2_offscreen_t *write = gles2_ctx->gles2_write_buffer;
        framebuffer = write->gl_framebuffer.fbo_handle;
    }

    gles2_ctx->dev->glBindFramebuffer(target, framebuffer);

    update_current_flip_state(gles2_ctx);
}

static int
transient_bind_read_buffer(cg_gles2_context_t *gles2_ctx)
{
    if (gles2_ctx->current_fbo_handle == 0) {
        if (cg_is_offscreen(gles2_ctx->read_buffer)) {
            cg_gles2_offscreen_t *read = gles2_ctx->gles2_read_buffer;
            GLuint read_fbo_handle = read->gl_framebuffer.fbo_handle;

            gles2_ctx->dev->glBindFramebuffer(GL_FRAMEBUFFER,
                                                  read_fbo_handle);

            return RESTORE_FB_FROM_OFFSCREEN;
        } else {
            _cg_framebuffer_gl_bind(gles2_ctx->read_buffer,
                                    0 /* target ignored */);

            return RESTORE_FB_FROM_ONSCREEN;
        }
    } else
        return RESTORE_FB_NONE;
}

static void
restore_write_buffer(cg_gles2_context_t *gles2_ctx,
                     int restore_mode)
{
    switch (restore_mode) {
    case RESTORE_FB_FROM_OFFSCREEN:

        gl_bind_framebuffer_wrapper(GL_FRAMEBUFFER, 0);

        break;
    case RESTORE_FB_FROM_ONSCREEN:

        /* Note: we can't restore the original write buffer using
         * _cg_framebuffer_gl_bind() if it's an offscreen
         * framebuffer because _cg_framebuffer_gl_bind() doesn't
         * know about the fbo handle owned by the gles2 context.
         */
        if (cg_is_offscreen(gles2_ctx->write_buffer))
            gl_bind_framebuffer_wrapper(GL_FRAMEBUFFER, 0);
        else
            _cg_framebuffer_gl_bind(gles2_ctx->write_buffer, GL_FRAMEBUFFER);

        break;
    case RESTORE_FB_NONE:
        break;
    }
}

/* We wrap glReadPixels so when framebuffer 0 is bound then we can
 * read from the read_framebuffer passed to cg_push_gles2_context().
 */
static void
gl_read_pixels_wrapper(GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height,
                       GLenum format,
                       GLenum type,
                       GLvoid *pixels)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    int restore_mode = transient_bind_read_buffer(gles2_ctx);

    gles2_ctx->dev->glReadPixels(x, y, width, height, format, type, pixels);

    restore_write_buffer(gles2_ctx, restore_mode);

    /* If the read buffer is a cg_offscreen_t then the data will be
     * upside down compared to what GL expects so we need to flip it */
    if (gles2_ctx->current_fbo_handle == 0 &&
        cg_is_offscreen(gles2_ctx->read_buffer)) {
        int bpp, bytes_per_row, stride, y;
        uint8_t *bytes = pixels;
        uint8_t *temprow;

        /* Try to determine the bytes per pixel for the given
         * format/type combination. If there's a format which doesn't
         * make sense then we'll just give up because GL will probably
         * have just thrown an error */
        switch (format) {
        case GL_RGB:
            switch (type) {
            case GL_UNSIGNED_BYTE:
                bpp = 3;
                break;

            case GL_UNSIGNED_SHORT_5_6_5:
                bpp = 2;
                break;

            default:
                return;
            }
            break;

        case GL_RGBA:
            switch (type) {
            case GL_UNSIGNED_BYTE:
                bpp = 4;
                break;

            case GL_UNSIGNED_SHORT_4_4_4_4:
            case GL_UNSIGNED_SHORT_5_5_5_1:
                bpp = 2;
                break;

            default:
                return;
            }
            break;

        case GL_ALPHA:
            switch (type) {
            case GL_UNSIGNED_BYTE:
                bpp = 1;
                break;

            default:
                return;
            }
            break;

        default:
            return;
        }

        bytes_per_row = bpp * width;
        stride = ((bytes_per_row + gles2_ctx->pack_alignment - 1) &
                  ~(gles2_ctx->pack_alignment - 1));
        temprow = c_alloca(bytes_per_row);

        /* vertically flip the buffer in-place */
        for (y = 0; y < height / 2; y++) {
            if (y != height - y - 1) /* skip center row */
            {
                memcpy(temprow, bytes + y * stride, bytes_per_row);
                memcpy(bytes + y * stride,
                       bytes + (height - y - 1) * stride,
                       bytes_per_row);
                memcpy(
                    bytes + (height - y - 1) * stride, temprow, bytes_per_row);
            }
        }
    }
}

static void
gl_copy_tex_image_2d_wrapper(GLenum target,
                             GLint level,
                             GLenum internal_format,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLint border)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    /* If we are reading from a cg_offscreen_t buffer then the image will
     * be upside down with respect to what GL expects so we can't use
     * glCopyTexImage2D. Instead we we'll try to use the CGlib API to
     * flip it */
    if (gles2_ctx->current_fbo_handle == 0 &&
        cg_is_offscreen(gles2_ctx->read_buffer)) {
        /* This will only work with the GL_TEXTURE_2D target. FIXME:
         * GLES2 also supports setting cube map textures with
         * glTexImage2D so we need to handle that too */
        if (target != GL_TEXTURE_2D)
            return;

        /* Create an empty texture to hold the data */
        gles2_ctx->vtable->glTexImage2D(target,
                                        level,
                                        internal_format,
                                        width,
                                        height,
                                        border,
                                        internal_format, /* format */
                                        GL_UNSIGNED_BYTE, /* type */
                                        NULL /* data */);

        copy_flipped_texture(gles2_ctx,
                             level,
                             x,
                             y, /* src_x/src_y */
                             0,
                             0, /* dst_x/dst_y */
                             width,
                             height);
    } else {
        int restore_mode = transient_bind_read_buffer(gles2_ctx);

        gles2_ctx->dev->glCopyTexImage2D(
            target, level, internal_format, x, y, width, height, border);

        restore_write_buffer(gles2_ctx, restore_mode);

        set_texture_object_data(
            gles2_ctx, target, level, internal_format, width, height);
    }
}

static void
gl_copy_tex_sub_image_2d_wrapper(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    /* If we are reading from a cg_offscreen_t buffer then the image will
     * be upside down with respect to what GL expects so we can't use
     * glCopyTexSubImage2D. Instead we we'll try to use the CGlib API to
     * flip it */
    if (gles2_ctx->current_fbo_handle == 0 &&
        cg_is_offscreen(gles2_ctx->read_buffer)) {
        /* This will only work with the GL_TEXTURE_2D target. FIXME:
         * GLES2 also supports setting cube map textures with
         * glTexImage2D so we need to handle that too */
        if (target != GL_TEXTURE_2D)
            return;

        copy_flipped_texture(gles2_ctx,
                             level,
                             x,
                             y, /* src_x/src_y */
                             xoffset,
                             yoffset, /* dst_x/dst_y */
                             width,
                             height);
    } else {
        int restore_mode = transient_bind_read_buffer(gles2_ctx);

        gles2_ctx->dev->glCopyTexSubImage2D(
            target, level, xoffset, yoffset, x, y, width, height);

        restore_write_buffer(gles2_ctx, restore_mode);
    }
}

static GLuint
gl_create_shader_wrapper(GLenum type)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    GLuint id;

    id = gles2_ctx->dev->glCreateShader(type);

    if (id != 0) {
        cg_gles2_shader_data_t *data = c_slice_new(cg_gles2_shader_data_t);

        data->object_id = id;
        data->type = type;
        data->ref_count = 1;
        data->deleted = false;

        c_hash_table_insert(gles2_ctx->shader_map, C_INT_TO_POINTER(id), data);
    }

    return id;
}

static void
gl_delete_shader_wrapper(GLuint shader)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    cg_gles2_shader_data_t *shader_data;

    if ((shader_data = c_hash_table_lookup(gles2_ctx->shader_map,
                                           C_INT_TO_POINTER(shader))) &&
        !shader_data->deleted) {
        shader_data->deleted = true;
        shader_data_unref(gles2_ctx, shader_data);
    }

    gles2_ctx->dev->glDeleteShader(shader);
}

static GLuint
gl_create_program_wrapper(void)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    GLuint id;

    id = gles2_ctx->dev->glCreateProgram();

    if (id != 0) {
        cg_gles2_program_data_t *data = c_slice_new(cg_gles2_program_data_t);

        data->object_id = id;
        data->attached_shaders = NULL;
        data->ref_count = 1;
        data->deleted = false;
        data->context = gles2_ctx;
        data->flip_vector_location = 0;
        data->flip_vector_state = CG_GLES2_FLIP_STATE_UNKNOWN;

        c_hash_table_insert(gles2_ctx->program_map, C_INT_TO_POINTER(id), data);
    }

    return id;
}

static void
gl_delete_program_wrapper(GLuint program)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    cg_gles2_program_data_t *program_data;

    if ((program_data = c_hash_table_lookup(gles2_ctx->program_map,
                                            C_INT_TO_POINTER(program))) &&
        !program_data->deleted) {
        program_data->deleted = true;
        program_data_unref(program_data);
    }

    gles2_ctx->dev->glDeleteProgram(program);
}

static void
gl_use_program_wrapper(GLuint program)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    cg_gles2_program_data_t *program_data;

    program_data =
        c_hash_table_lookup(gles2_ctx->program_map, C_INT_TO_POINTER(program));

    if (program_data)
        program_data->ref_count++;
    if (gles2_ctx->current_program)
        program_data_unref(gles2_ctx->current_program);

    gles2_ctx->current_program = program_data;

    gles2_ctx->dev->glUseProgram(program);
}

static void
gl_attach_shader_wrapper(GLuint program, GLuint shader)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    cg_gles2_program_data_t *program_data;
    cg_gles2_shader_data_t *shader_data;

    if ((program_data = c_hash_table_lookup(gles2_ctx->program_map,
                                            C_INT_TO_POINTER(program))) &&
        (shader_data = c_hash_table_lookup(gles2_ctx->shader_map,
                                           C_INT_TO_POINTER(shader))) &&
        /* Ignore attempts to attach a shader that is already attached */
        c_llist_find(program_data->attached_shaders, shader_data) == NULL) {
        shader_data->ref_count++;
        program_data->attached_shaders =
            c_llist_prepend(program_data->attached_shaders, shader_data);
    }

    gles2_ctx->dev->glAttachShader(program, shader);
}

static void
gl_detach_shader_wrapper(GLuint program, GLuint shader)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    cg_gles2_program_data_t *program_data;
    cg_gles2_shader_data_t *shader_data;

    if ((program_data = c_hash_table_lookup(gles2_ctx->program_map,
                                            C_INT_TO_POINTER(program))) &&
        (shader_data = c_hash_table_lookup(gles2_ctx->shader_map,
                                           C_INT_TO_POINTER(shader))))
        detach_shader(program_data, shader_data);

    gles2_ctx->dev->glDetachShader(program, shader);
}

static void
gl_shader_source_wrapper(GLuint shader,
                         GLsizei count,
                         const char *const *string,
                         const GLint *length)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    cg_gles2_shader_data_t *shader_data;

    if ((shader_data = c_hash_table_lookup(gles2_ctx->shader_map,
                                           C_INT_TO_POINTER(shader))) &&
        shader_data->type == GL_VERTEX_SHADER) {
        char **string_copy = c_alloca((count + 1) * sizeof(char *));
        int *length_copy = c_alloca((count + 1) * sizeof(int));
        int i;

        /* Replace any occurences of the symbol 'main' with a different
         * symbol so that we can provide our own wrapper main
         * function */

        for (i = 0; i < count; i++) {
            int string_length;

            if (length == NULL || length[i] < 0)
                string_length = strlen(string[i]);
            else
                string_length = length[i];

            string_copy[i] = c_memdup(string[i], string_length);

            replace_token(string_copy[i],
                          "main",
                          MAIN_WRAPPER_REPLACEMENT_NAME,
                          string_length);

            length_copy[i] = string_length;
        }

        string_copy[count] = (char *)main_wrapper_function;
        length_copy[count] = sizeof(main_wrapper_function) - 1;

        gles2_ctx->dev->glShaderSource(
            shader, count + 1, (const char * const *)string_copy, length_copy);

        /* Note: we don't need to free the last entry in string_copy[]
         * because it is our static wrapper string... */
        for (i = 0; i < count; i++)
            c_free(string_copy[i]);
    } else
        gles2_ctx->dev->glShaderSource(shader, count, string, length);
}

static void
gl_get_shader_source_wrapper(GLuint shader,
                             GLsizei buf_size,
                             GLsizei *length_out,
                             GLchar *source)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    cg_gles2_shader_data_t *shader_data;
    GLsizei length;

    gles2_ctx->dev->glGetShaderSource(shader, buf_size, &length, source);

    if ((shader_data = c_hash_table_lookup(gles2_ctx->shader_map,
                                           C_INT_TO_POINTER(shader))) &&
        shader_data->type == GL_VERTEX_SHADER) {
        GLsizei copy_length = MIN(length, buf_size - 1);
        static const char wrapper_marker[] = MAIN_WRAPPER_BEGIN;
        char *wrapper_start;

        /* Strip out the wrapper snippet we added when the source was
         * specified */
        wrapper_start = c_memmem(source, copy_length, wrapper_marker,
                                 sizeof(wrapper_marker) - 1);
        if (wrapper_start) {
            length = wrapper_start - source;
            copy_length = length;
            *wrapper_start = '\0';
        }

        /* Correct the name of the main function back to its original */
        replace_token(
            source, MAIN_WRAPPER_REPLACEMENT_NAME, "main", copy_length);
    }

    if (length_out)
        *length_out = length;
}

static void
gl_link_program_wrapper(GLuint program)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    cg_gles2_program_data_t *program_data;

    gles2_ctx->dev->glLinkProgram(program);

    program_data =
        c_hash_table_lookup(gles2_ctx->program_map, C_INT_TO_POINTER(program));

    if (program_data) {
        GLint status;

        gles2_ctx->dev->glGetProgramiv(program, GL_LINK_STATUS, &status);

        if (status)
            program_data->flip_vector_location =
                gles2_ctx->dev->glGetUniformLocation(
                    program, MAIN_WRAPPER_FLIP_UNIFORM);
    }
}

static void
gl_get_program_iv_wrapper(GLuint program, GLenum pname, GLint *params)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    gles2_ctx->dev->glGetProgramiv(program, pname, params);

    switch (pname) {
    case GL_ATTACHED_SHADERS:
        /* Decrease the number of shaders to try and hide the shader
         * wrapper we added */
        if (*params > 1)
            (*params)--;
        break;
    }
}

static void
flush_viewport_state(cg_gles2_context_t *gles2_ctx)
{
    if (gles2_ctx->viewport_dirty) {
        int y;

        if (gles2_ctx->current_flip_state == CG_GLES2_FLIP_STATE_FLIPPED) {
            /* We need to know the height of the current framebuffer in
             * order to flip the viewport. Fortunately we don't need to
             * track the height of the FBOs created within the GLES2
             * context because we would never be flipping if they are
             * bound so we can just assume CGlib's framebuffer is bound
             * when we are flipping */
            int fb_height = cg_framebuffer_get_height(gles2_ctx->write_buffer);
            y = fb_height - (gles2_ctx->viewport[1] + gles2_ctx->viewport[3]);
        } else
            y = gles2_ctx->viewport[1];

        gles2_ctx->dev->glViewport(gles2_ctx->viewport[0],
                                       y,
                                       gles2_ctx->viewport[2],
                                       gles2_ctx->viewport[3]);

        gles2_ctx->viewport_dirty = false;
    }
}

static void
flush_scissor_state(cg_gles2_context_t *gles2_ctx)
{
    if (gles2_ctx->scissor_dirty) {
        int y;

        if (gles2_ctx->current_flip_state == CG_GLES2_FLIP_STATE_FLIPPED) {
            /* See comment above about the viewport flipping */
            int fb_height = cg_framebuffer_get_height(gles2_ctx->write_buffer);
            y = fb_height - (gles2_ctx->scissor[1] + gles2_ctx->scissor[3]);
        } else
            y = gles2_ctx->scissor[1];

        gles2_ctx->dev->glScissor(gles2_ctx->scissor[0],
                                      y,
                                      gles2_ctx->scissor[2],
                                      gles2_ctx->scissor[3]);

        gles2_ctx->scissor_dirty = false;
    }
}

static void
flush_front_face_state(cg_gles2_context_t *gles2_ctx)
{
    if (gles2_ctx->front_face_dirty) {
        GLenum front_face;

        if (gles2_ctx->current_flip_state == CG_GLES2_FLIP_STATE_FLIPPED) {
            if (gles2_ctx->front_face == GL_CW)
                front_face = GL_CCW;
            else
                front_face = GL_CW;
        } else
            front_face = gles2_ctx->front_face;

        gles2_ctx->dev->glFrontFace(front_face);

        gles2_ctx->front_face_dirty = false;
    }
}

static void
pre_draw_wrapper(cg_gles2_context_t *gles2_ctx)
{
    /* If there's no current program then we'll just let GL report an
     * error */
    if (gles2_ctx->current_program == NULL)
        return;

    flush_viewport_state(gles2_ctx);
    flush_scissor_state(gles2_ctx);
    flush_front_face_state(gles2_ctx);

    /* We want to flip rendering when the application is rendering to a
     * CGlib offscreen buffer in order to maintain the flipped texture
     * coordinate origin */
    if (gles2_ctx->current_flip_state !=
        gles2_ctx->current_program->flip_vector_state) {
        GLuint location = gles2_ctx->current_program->flip_vector_location;
        float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        if (gles2_ctx->current_flip_state == CG_GLES2_FLIP_STATE_FLIPPED)
            value[1] = -1.0f;

        gles2_ctx->dev->glUniform4fv(location, 1, value);

        gles2_ctx->current_program->flip_vector_state =
            gles2_ctx->current_flip_state;
    }
}

static void
gl_clear_wrapper(GLbitfield mask)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    /* Clearing is affected by the scissor state so we need to ensure
     * that's flushed */
    flush_scissor_state(gles2_ctx);

    gles2_ctx->dev->glClear(mask);
}

static void
gl_draw_elements_wrapper(GLenum mode,
                         GLsizei count,
                         GLenum type,
                         const GLvoid *indices)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    pre_draw_wrapper(gles2_ctx);

    gles2_ctx->dev->glDrawElements(mode, count, type, indices);
}

static void
gl_draw_arrays_wrapper(GLenum mode, GLint first, GLsizei count)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    pre_draw_wrapper(gles2_ctx);

    gles2_ctx->dev->glDrawArrays(mode, first, count);
}

static void
gl_get_program_info_log_wrapper(GLuint program,
                                GLsizei buf_size,
                                GLsizei *length_out,
                                GLchar *info_log)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    GLsizei length;

    gles2_ctx->dev->glGetProgramInfoLog(
        program, buf_size, &length, info_log);

    replace_token(
        info_log, MAIN_WRAPPER_REPLACEMENT_NAME, "main", MIN(length, buf_size));

    if (length_out)
        *length_out = length;
}

static void
gl_get_shader_info_log_wrapper(GLuint shader,
                               GLsizei buf_size,
                               GLsizei *length_out,
                               GLchar *info_log)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    GLsizei length;

    gles2_ctx->dev->glGetShaderInfoLog(shader, buf_size, &length, info_log);

    replace_token(
        info_log, MAIN_WRAPPER_REPLACEMENT_NAME, "main", MIN(length, buf_size));

    if (length_out)
        *length_out = length;
}

static void
gl_front_face_wrapper(GLenum mode)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    /* If the mode doesn't make any sense then we'll just let the
     * context deal with it directly so that it will throw an error */
    if (mode != GL_CW && mode != GL_CCW)
        gles2_ctx->dev->glFrontFace(mode);
    else {
        gles2_ctx->front_face = mode;
        gles2_ctx->front_face_dirty = true;
    }
}

static void
gl_viewport_wrapper(GLint x, GLint y, GLsizei width, GLsizei height)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    /* If the viewport is invalid then we'll just let the context deal
     * with it directly so that it will throw an error */
    if (width < 0 || height < 0)
        gles2_ctx->dev->glViewport(x, y, width, height);
    else {
        gles2_ctx->viewport[0] = x;
        gles2_ctx->viewport[1] = y;
        gles2_ctx->viewport[2] = width;
        gles2_ctx->viewport[3] = height;
        gles2_ctx->viewport_dirty = true;
    }
}

static void
gl_scissor_wrapper(GLint x, GLint y, GLsizei width, GLsizei height)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    /* If the scissor is invalid then we'll just let the context deal
     * with it directly so that it will throw an error */
    if (width < 0 || height < 0)
        gles2_ctx->dev->glScissor(x, y, width, height);
    else {
        gles2_ctx->scissor[0] = x;
        gles2_ctx->scissor[1] = y;
        gles2_ctx->scissor[2] = width;
        gles2_ctx->scissor[3] = height;
        gles2_ctx->scissor_dirty = true;
    }
}

static void
gl_get_boolean_v_wrapper(GLenum pname, GLboolean *params)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    switch (pname) {
    case GL_VIEWPORT: {
        int i;

        for (i = 0; i < 4; i++)
            params[i] = !!gles2_ctx->viewport[i];
    } break;

    case GL_SCISSOR_BOX: {
        int i;

        for (i = 0; i < 4; i++)
            params[i] = !!gles2_ctx->scissor[i];
    } break;

    default:
        gles2_ctx->dev->glGetBooleanv(pname, params);
    }
}

static void
gl_get_integer_v_wrapper(GLenum pname, GLint *params)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    switch (pname) {
    case GL_VIEWPORT: {
        int i;

        for (i = 0; i < 4; i++)
            params[i] = gles2_ctx->viewport[i];
    } break;

    case GL_SCISSOR_BOX: {
        int i;

        for (i = 0; i < 4; i++)
            params[i] = gles2_ctx->scissor[i];
    } break;

    case GL_FRONT_FACE:
        params[0] = gles2_ctx->front_face;
        break;

    default:
        gles2_ctx->dev->glGetIntegerv(pname, params);
    }
}

static void
gl_get_float_v_wrapper(GLenum pname, GLfloat *params)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    switch (pname) {
    case GL_VIEWPORT: {
        int i;

        for (i = 0; i < 4; i++)
            params[i] = gles2_ctx->viewport[i];
    } break;

    case GL_SCISSOR_BOX: {
        int i;

        for (i = 0; i < 4; i++)
            params[i] = gles2_ctx->scissor[i];
    } break;

    case GL_FRONT_FACE:
        params[0] = gles2_ctx->front_face;
        break;

    default:
        gles2_ctx->dev->glGetFloatv(pname, params);
    }
}

static void
gl_pixel_store_i_wrapper(GLenum pname, GLint param)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    gles2_ctx->dev->glPixelStorei(pname, param);

    if (pname == GL_PACK_ALIGNMENT &&
        (param == 1 || param == 2 || param == 4 || param == 8))
        gles2_ctx->pack_alignment = param;
}

static void
gl_active_texture_wrapper(GLenum texture)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    int texture_unit;

    gles2_ctx->dev->glActiveTexture(texture);

    texture_unit = texture - GL_TEXTURE0;

    /* If the application is binding some odd looking texture unit
     * numbers then we'll just ignore it and hope that GL has generated
     * an error */
    if (texture_unit >= 0 && texture_unit < 512) {
        gles2_ctx->current_texture_unit = texture_unit;
        c_array_set_size(gles2_ctx->texture_units,
                         MAX(texture_unit, gles2_ctx->texture_units->len));
    }
}

static void
gl_delete_textures_wrapper(GLsizei n, const GLuint *textures)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;
    int texture_index;
    int texture_unit;

    gles2_ctx->dev->glDeleteTextures(n, textures);

    for (texture_index = 0; texture_index < n; texture_index++) {
        /* Reset any texture units that have any of these textures bound */
        for (texture_unit = 0; texture_unit < gles2_ctx->texture_units->len;
             texture_unit++) {
            cg_gles2_texture_unit_data_t *unit =
                &c_array_index(gles2_ctx->texture_units,
                               cg_gles2_texture_unit_data_t,
                               texture_unit);

            if (unit->current_texture_2d == textures[texture_index])
                unit->current_texture_2d = 0;
        }

        /* Remove the binding. We can do this immediately because unlike
         * shader objects the deletion isn't delayed until the object is
         * unbound */
        c_hash_table_remove(gles2_ctx->texture_object_map,
                            C_UINT_TO_POINTER(textures[texture_index]));
    }
}

static void
gl_bind_texture_wrapper(GLenum target, GLuint texture)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    gles2_ctx->dev->glBindTexture(target, texture);

    if (target == GL_TEXTURE_2D) {
        cg_gles2_texture_unit_data_t *unit =
            &c_array_index(gles2_ctx->texture_units,
                           cg_gles2_texture_unit_data_t,
                           gles2_ctx->current_texture_unit);
        unit->current_texture_2d = texture;
    }
}

static void
gl_tex_image_2d_wrapper(GLenum target,
                        GLint level,
                        GLint internal_format,
                        GLsizei width,
                        GLsizei height,
                        GLint border,
                        GLenum format,
                        GLenum type,
                        const GLvoid *pixels)
{
    cg_gles2_context_t *gles2_ctx = current_gles2_context;

    gles2_ctx->dev->glTexImage2D(target,
                                     level,
                                     internal_format,
                                     width,
                                     height,
                                     border,
                                     format,
                                     type,
                                     pixels);

    set_texture_object_data(
        gles2_ctx, target, level, internal_format, width, height);
}

static void
_cg_gles2_offscreen_free(cg_gles2_offscreen_t *gles2_offscreen)
{
    c_list_remove(&gles2_offscreen->link);
    c_slice_free(cg_gles2_offscreen_t, gles2_offscreen);
}

static void
force_delete_program_object(cg_gles2_context_t *context,
                            cg_gles2_program_data_t *program_data)
{
    if (!program_data->deleted) {
        context->dev->glDeleteProgram(program_data->object_id);
        program_data->deleted = true;
        program_data_unref(program_data);
    }
}

static void
force_delete_shader_object(cg_gles2_context_t *context,
                           cg_gles2_shader_data_t *shader_data)
{
    if (!shader_data->deleted) {
        context->dev->glDeleteShader(shader_data->object_id);
        shader_data->deleted = true;
        shader_data_unref(context, shader_data);
    }
}

static void
force_delete_texture_object(cg_gles2_context_t *context,
                            cg_gles2_texture_object_data_t *texture_data)
{
    context->dev->glDeleteTextures(1, &texture_data->object_id);
}

static void
_cg_gles2_context_free(cg_gles2_context_t *gles2_context)
{
    cg_device_t *dev = gles2_context->dev;
    const cg_winsys_vtable_t *winsys;
    c_llist_t *objects, *l;

    if (gles2_context->current_program)
        program_data_unref(gles2_context->current_program);

    /* Try to forcibly delete any shaders, programs and textures so that
     * they won't get leaked. Because all GLES2 contexts are in the same
     * share list as CGlib's context these won't get deleted by default.
     * FIXME: we should do this for all of the other resources too, like
     * textures */
    objects = c_hash_table_get_values(gles2_context->program_map);
    for (l = objects; l; l = l->next)
        force_delete_program_object(gles2_context, l->data);
    c_llist_free(objects);
    objects = c_hash_table_get_values(gles2_context->shader_map);
    for (l = objects; l; l = l->next)
        force_delete_shader_object(gles2_context, l->data);
    c_llist_free(objects);
    objects = c_hash_table_get_values(gles2_context->texture_object_map);
    for (l = objects; l; l = l->next)
        force_delete_texture_object(gles2_context, l->data);
    c_llist_free(objects);

    /* All of the program and shader objects should now be destroyed */
    if (c_hash_table_size(gles2_context->program_map) > 0)
        c_warning("Program objects have been leaked from a cg_gles2_context_t");
    if (c_hash_table_size(gles2_context->shader_map) > 0)
        c_warning("Shader objects have been leaked from a cg_gles2_context_t");

    c_hash_table_destroy(gles2_context->program_map);
    c_hash_table_destroy(gles2_context->shader_map);

    c_hash_table_destroy(gles2_context->texture_object_map);
    c_array_free(gles2_context->texture_units, true);

    winsys = dev->display->renderer->winsys_vtable;
    winsys->destroy_gles2_context(gles2_context);

    while (!c_list_empty(&gles2_context->foreign_offscreens)) {
        cg_gles2_offscreen_t *gles2_offscreen = c_container_of(
            gles2_context->foreign_offscreens.next, cg_gles2_offscreen_t, link);

        /* Note: this will also indirectly free the gles2_offscreen by
         * calling the destroy notify for the _user_data */
        cg_object_set_user_data(CG_OBJECT(gles2_offscreen->original_offscreen),
                                &offscreen_wrapper_key,
                                NULL,
                                NULL);
    }

    c_free(gles2_context->vtable);

    c_free(gles2_context);
}

static void
free_shader_data(cg_gles2_shader_data_t *data)
{
    c_slice_free(cg_gles2_shader_data_t, data);
}

static void
free_program_data(cg_gles2_program_data_t *data)
{
    while (data->attached_shaders)
        detach_shader(data, data->attached_shaders->data);

    c_slice_free(cg_gles2_program_data_t, data);
}

static void
free_texture_object_data(cg_gles2_texture_object_data_t *data)
{
    c_slice_free(cg_gles2_texture_object_data_t, data);
}

cg_gles2_context_t *
cg_gles2_context_new(cg_device_t *dev, cg_error_t **error)
{
    cg_gles2_context_t *gles2_ctx;
    const cg_winsys_vtable_t *winsys;

    if (!cg_has_feature(dev, CG_FEATURE_ID_GLES2_CONTEXT)) {
        _cg_set_error(error,
                      CG_GLES2_CONTEXT_ERROR,
                      CG_GLES2_CONTEXT_ERROR_UNSUPPORTED,
                      "Backend doesn't support creating GLES2 contexts");

        return NULL;
    }

    gles2_ctx = c_malloc0(sizeof(cg_gles2_context_t));

    gles2_ctx->dev = dev;

    c_list_init(&gles2_ctx->foreign_offscreens);

    winsys = dev->display->renderer->winsys_vtable;
    gles2_ctx->winsys = winsys->device_create_gles2_context(dev, error);
    if (gles2_ctx->winsys == NULL) {
        c_free(gles2_ctx);
        return NULL;
    }

    gles2_ctx->current_flip_state = CG_GLES2_FLIP_STATE_UNKNOWN;
    gles2_ctx->viewport_dirty = true;
    gles2_ctx->scissor_dirty = true;
    gles2_ctx->front_face_dirty = true;
    gles2_ctx->front_face = GL_CCW;
    gles2_ctx->pack_alignment = 4;

    gles2_ctx->vtable = c_malloc0(sizeof(cg_gles2_vtable_t));
#define CG_EXT_BEGIN(name,                                                     \
                     min_gl_major,                                             \
                     min_gl_minor,                                             \
                     gles_availability,                                        \
                     extension_suffixes,                                       \
                     extension_names)

#define CG_EXT_FUNCTION(ret, name, args)                                       \
    gles2_ctx->vtable->name = (void *)dev->name;

#define CG_EXT_END()

#include "gl-prototypes/cg-gles2-functions.h"

#undef CG_EXT_BEGIN
#undef CG_EXT_FUNCTION
#undef CG_EXT_END

    gles2_ctx->vtable->glBindFramebuffer = (void *)gl_bind_framebuffer_wrapper;
    gles2_ctx->vtable->glReadPixels = (void *)gl_read_pixels_wrapper;
    gles2_ctx->vtable->glCopyTexImage2D = (void *)gl_copy_tex_image_2d_wrapper;
    gles2_ctx->vtable->glCopyTexSubImage2D =
        (void *)gl_copy_tex_sub_image_2d_wrapper;

    gles2_ctx->vtable->glCreateShader = gl_create_shader_wrapper;
    gles2_ctx->vtable->glDeleteShader = gl_delete_shader_wrapper;
    gles2_ctx->vtable->glCreateProgram = gl_create_program_wrapper;
    gles2_ctx->vtable->glDeleteProgram = gl_delete_program_wrapper;
    gles2_ctx->vtable->glUseProgram = gl_use_program_wrapper;
    gles2_ctx->vtable->glAttachShader = gl_attach_shader_wrapper;
    gles2_ctx->vtable->glDetachShader = gl_detach_shader_wrapper;
    gles2_ctx->vtable->glShaderSource = gl_shader_source_wrapper;
    gles2_ctx->vtable->glGetShaderSource = gl_get_shader_source_wrapper;
    gles2_ctx->vtable->glLinkProgram = gl_link_program_wrapper;
    gles2_ctx->vtable->glGetProgramiv = gl_get_program_iv_wrapper;
    gles2_ctx->vtable->glGetProgramInfoLog = gl_get_program_info_log_wrapper;
    gles2_ctx->vtable->glGetShaderInfoLog = gl_get_shader_info_log_wrapper;
    gles2_ctx->vtable->glClear = gl_clear_wrapper;
    gles2_ctx->vtable->glDrawElements = gl_draw_elements_wrapper;
    gles2_ctx->vtable->glDrawArrays = gl_draw_arrays_wrapper;
    gles2_ctx->vtable->glFrontFace = gl_front_face_wrapper;
    gles2_ctx->vtable->glViewport = gl_viewport_wrapper;
    gles2_ctx->vtable->glScissor = gl_scissor_wrapper;
    gles2_ctx->vtable->glGetBooleanv = gl_get_boolean_v_wrapper;
    gles2_ctx->vtable->glGetIntegerv = gl_get_integer_v_wrapper;
    gles2_ctx->vtable->glGetFloatv = gl_get_float_v_wrapper;
    gles2_ctx->vtable->glPixelStorei = gl_pixel_store_i_wrapper;
    gles2_ctx->vtable->glActiveTexture = gl_active_texture_wrapper;
    gles2_ctx->vtable->glDeleteTextures = gl_delete_textures_wrapper;
    gles2_ctx->vtable->glBindTexture = gl_bind_texture_wrapper;
    gles2_ctx->vtable->glTexImage2D = gl_tex_image_2d_wrapper;

    gles2_ctx->shader_map =
        c_hash_table_new_full(c_direct_hash,
                              c_direct_equal,
                              NULL, /* key_destroy */
                              (c_destroy_func_t)free_shader_data);
    gles2_ctx->program_map =
        c_hash_table_new_full(c_direct_hash,
                              c_direct_equal,
                              NULL, /* key_destroy */
                              (c_destroy_func_t)free_program_data);

    gles2_ctx->texture_object_map =
        c_hash_table_new_full(c_direct_hash,
                              c_direct_equal,
                              NULL, /* key_destroy */
                              (c_destroy_func_t)free_texture_object_data);

    gles2_ctx->texture_units =
        c_array_new(false, /* not zero terminated */
                    true, /* clear */
                    sizeof(cg_gles2_texture_unit_data_t));
    gles2_ctx->current_texture_unit = 0;
    c_array_set_size(gles2_ctx->texture_units, 1);

    return _cg_gles2_context_object_new(gles2_ctx);
}

const cg_gles2_vtable_t *
cg_gles2_context_get_vtable(cg_gles2_context_t *gles2_ctx)
{
    return gles2_ctx->vtable;
}

/* When drawing to a cg_framebuffer_t from a separate context we have
 * to be able to allocate ancillary buffers for that context...
 */
static cg_gles2_offscreen_t *
_cg_gles2_offscreen_allocate(cg_offscreen_t *offscreen,
                             cg_gles2_context_t *gles2_context,
                             cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(offscreen);
    const cg_winsys_vtable_t *winsys;
    cg_error_t *internal_error = NULL;
    cg_gles2_offscreen_t *gles2_offscreen;
    int level_width;
    int level_height;

    if (!framebuffer->allocated &&
        !cg_framebuffer_allocate(framebuffer, error)) {
        return NULL;
    }

    c_list_for_each(gles2_offscreen, &gles2_context->foreign_offscreens, link)
    {
        if (gles2_offscreen->original_offscreen == offscreen)
            return gles2_offscreen;
    }

    winsys = _cg_framebuffer_get_winsys(framebuffer);
    winsys->save_device(framebuffer->dev);
    if (!winsys->set_gles2_context(gles2_context, &internal_error)) {
        winsys->restore_context(framebuffer->dev);

        cg_error_free(internal_error);
        _cg_set_error(error,
                      CG_FRAMEBUFFER_ERROR,
                      CG_FRAMEBUFFER_ERROR_ALLOCATE,
                      "Failed to bind gles2 context to create framebuffer");
        return NULL;
    }

    gles2_offscreen = c_slice_new0(cg_gles2_offscreen_t);

    _cg_texture_get_level_size(offscreen->texture,
                               offscreen->texture_level,
                               &level_width,
                               &level_height,
                               NULL);

    if (!_cg_framebuffer_try_creating_gl_fbo(
            gles2_context->dev,
            level_width,
            level_height,
            offscreen->texture,
            offscreen->texture_level,
            offscreen->depth_texture,
            offscreen->depth_texture_level,
            &CG_FRAMEBUFFER(offscreen)->config,
            offscreen->allocation_flags,
            &gles2_offscreen->gl_framebuffer)) {
        winsys->restore_context(framebuffer->dev);

        c_slice_free(cg_gles2_offscreen_t, gles2_offscreen);

        _cg_set_error(error,
                      CG_FRAMEBUFFER_ERROR,
                      CG_FRAMEBUFFER_ERROR_ALLOCATE,
                      "Failed to create an OpenGL framebuffer object");
        return NULL;
    }

    winsys->restore_context(framebuffer->dev);

    gles2_offscreen->original_offscreen = offscreen;

    c_list_insert(&gles2_context->foreign_offscreens, &gles2_offscreen->link);

    /* So we avoid building up an ever growing collection of ancillary
     * buffers for wrapped framebuffers, we make sure that the wrappers
     * get freed when the original offscreen framebuffer is freed. */
    cg_object_set_user_data(
        CG_OBJECT(framebuffer),
        &offscreen_wrapper_key,
        gles2_offscreen,
        (cg_user_data_destroy_callback_t)_cg_gles2_offscreen_free);

    return gles2_offscreen;
}

bool
cg_push_gles2_context(cg_device_t *dev,
                      cg_gles2_context_t *gles2_ctx,
                      cg_framebuffer_t *read_buffer,
                      cg_framebuffer_t *write_buffer,
                      cg_error_t **error)
{
    const cg_winsys_vtable_t *winsys = dev->display->renderer->winsys_vtable;
    cg_error_t *internal_error = NULL;

    c_return_val_if_fail(gles2_ctx != NULL, false);

    /* The read/write buffers are properties of the gles2 context and we
     * don't currently track the read/write buffers as part of the stack
     * entries so we explicitly don't allow the same context to be
     * pushed multiple times. */
    if (c_queue_find(&dev->gles2_context_stack, gles2_ctx)) {
        c_critical("Pushing the same GLES2 context multiple times isn't "
                   "supported");
        return false;
    }

    if (dev->gles2_context_stack.length == 0) {
        _cg_framebuffer_flush(read_buffer);
        if (write_buffer != read_buffer)
            _cg_framebuffer_flush(write_buffer);
        winsys->save_device(dev);
    } else
        gles2_ctx->vtable->glFlush();

    if (gles2_ctx->read_buffer != read_buffer) {
        if (cg_is_offscreen(read_buffer)) {
            gles2_ctx->gles2_read_buffer = _cg_gles2_offscreen_allocate(
                CG_OFFSCREEN(read_buffer), gles2_ctx, error);
            /* XXX: what consistency guarantees should this api have?
             *
             * It should be safe to return at this point but we provide
             * no guarantee to the caller whether their given buffers
             * may be referenced and old buffers unreferenced even
             * if the _push fails. */
            if (!gles2_ctx->gles2_read_buffer)
                return false;
        } else
            gles2_ctx->gles2_read_buffer = NULL;
        if (gles2_ctx->read_buffer)
            cg_object_unref(gles2_ctx->read_buffer);
        gles2_ctx->read_buffer = cg_object_ref(read_buffer);
    }

    if (gles2_ctx->write_buffer != write_buffer) {
        if (cg_is_offscreen(write_buffer)) {
            gles2_ctx->gles2_write_buffer = _cg_gles2_offscreen_allocate(
                CG_OFFSCREEN(write_buffer), gles2_ctx, error);
            /* XXX: what consistency guarantees should this api have?
             *
             * It should be safe to return at this point but we provide
             * no guarantee to the caller whether their given buffers
             * may be referenced and old buffers unreferenced even
             * if the _push fails. */
            if (!gles2_ctx->gles2_write_buffer)
                return false;
        } else
            gles2_ctx->gles2_write_buffer = NULL;
        if (gles2_ctx->write_buffer)
            cg_object_unref(gles2_ctx->write_buffer);
        gles2_ctx->write_buffer = cg_object_ref(write_buffer);

        update_current_flip_state(gles2_ctx);
    }

    if (!winsys->set_gles2_context(gles2_ctx, &internal_error)) {
        winsys->restore_context(dev);

        cg_error_free(internal_error);
        _cg_set_error(error,
                      CG_GLES2_CONTEXT_ERROR,
                      CG_GLES2_CONTEXT_ERROR_DRIVER,
                      "Driver failed to make GLES2 context current");
        return false;
    }

    c_queue_push_tail(&dev->gles2_context_stack, gles2_ctx);

    /* The last time this context was pushed may have been with a
     * different offscreen draw framebuffer and so if GL framebuffer 0
     * is bound for this GLES2 context we may need to bind a new,
     * corresponding, window system framebuffer... */
    if (gles2_ctx->current_fbo_handle == 0) {
        if (cg_is_offscreen(gles2_ctx->write_buffer)) {
            cg_gles2_offscreen_t *write = gles2_ctx->gles2_write_buffer;
            GLuint handle = write->gl_framebuffer.fbo_handle;
            gles2_ctx->dev->glBindFramebuffer(GL_FRAMEBUFFER, handle);
        }
    }

    current_gles2_context = gles2_ctx;

    /* If this is the first time this gles2 context has been used then
     * we'll force the viewport and scissor to the right size. GL has
     * the semantics that the viewport and scissor default to the size
     * of the first surface the context is used with. If the first
     * cg_framebuffer_t that this context is used with is an offscreen,
     * then the surface from GL's point of view will be the 1x1 dummy
     * surface so the viewport will be wrong. Therefore we just override
     * the default viewport and scissor here */
    if (!gles2_ctx->has_been_bound) {
        int fb_width = cg_framebuffer_get_width(write_buffer);
        int fb_height = cg_framebuffer_get_height(write_buffer);

        gles2_ctx->vtable->glViewport(0,
                                      0, /* x/y */
                                      fb_width,
                                      fb_height);
        gles2_ctx->vtable->glScissor(0,
                                     0, /* x/y */
                                     fb_width,
                                     fb_height);
        gles2_ctx->has_been_bound = true;
    }

    return true;
}

cg_gles2_vtable_t *
cg_gles2_get_current_vtable(void)
{
    return current_gles2_context ? current_gles2_context->vtable : NULL;
}

void
cg_pop_gles2_context(cg_device_t *dev)
{
    cg_gles2_context_t *gles2_ctx;
    const cg_winsys_vtable_t *winsys = dev->display->renderer->winsys_vtable;

    c_return_if_fail(dev->gles2_context_stack.length > 0);

    c_queue_pop_tail(&dev->gles2_context_stack);

    gles2_ctx = c_queue_peek_tail(&dev->gles2_context_stack);

    if (gles2_ctx) {
        winsys->set_gles2_context(gles2_ctx, NULL);
        current_gles2_context = gles2_ctx;
    } else {
        winsys->restore_context(dev);
        current_gles2_context = NULL;
    }
}

cg_texture_2d_t *
cg_gles2_texture_2d_new_from_handle(cg_device_t *dev,
                                    cg_gles2_context_t *gles2_ctx,
                                    unsigned int handle,
                                    int width,
                                    int height,
                                    cg_pixel_format_t format)
{
    return cg_texture_2d_gl_new_from_foreign(dev, handle, width, height,
                                             format);
}

bool
cg_gles2_texture_get_handle(cg_texture_t *texture,
                            unsigned int *handle,
                            unsigned int *target)
{
    return cg_texture_get_gl_texture(texture, handle, target);
}
