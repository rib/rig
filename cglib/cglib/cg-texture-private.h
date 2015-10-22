/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __CG_TEXTURE_PRIVATE_H
#define __CG_TEXTURE_PRIVATE_H

#include "cg-bitmap-private.h"
#include "cg-object-private.h"
#include "cg-pipeline-private.h"
#include "cg-spans.h"
#include "cg-meta-texture.h"
#include "cg-framebuffer.h"

#ifdef CG_HAS_EGL_SUPPORT
#include "cg-egl-defines.h"
#endif

#ifdef CG_HAS_WEBGL_SUPPORT
#include "cg-webgl.h"
#endif

typedef struct _cg_texture_vtable_t cg_texture_vtable_t;

/* Encodes three possible result of transforming a quad */
typedef enum {
    /* quad doesn't cross the boundaries of a texture */
    CG_TRANSFORM_NO_REPEAT,
    /* quad crosses boundaries, hardware wrap mode can handle */
    CG_TRANSFORM_HARDWARE_REPEAT,
    /* quad crosses boundaries, needs software fallback;
    * for a sliced texture, this might not actually involve
    * repeating, just a quad crossing multiple slices */
    CG_TRANSFORM_SOFTWARE_REPEAT,
} cg_transform_result_t;

/* Flags given to the pre_paint method */
typedef enum {
    /* The texture is going to be used with filters that require
       mipmapping. This gives the texture the opportunity to
       automatically update the mipmap tree */
    CG_TEXTURE_NEEDS_MIPMAP = 1
} cg_texture_pre_paint_flags_t;

struct _cg_texture_vtable_t {
    /* Virtual functions that must be implemented for a texture
       backend */

    bool is_primitive;

    bool (*allocate)(cg_texture_t *tex, cg_error_t **error);

    /* This should update the specified sub region of the texture with a
       sub region of the given bitmap. The bitmap is not converted
       before being set so the caller is expected to have called
       _cg_bitmap_convert_for_upload with a suitable internal_format
       before passing here */
    bool (*set_region)(cg_texture_t *tex,
                       int src_x,
                       int src_y,
                       int dst_x,
                       int dst_y,
                       int dst_width,
                       int dst_height,
                       int level,
                       cg_bitmap_t *bitmap,
                       cg_error_t **error);

    /* This should copy the image data of the texture into @data. The
       requested format will have been first passed through
       ctx->texture_driver->find_best_gl_get_data_format so it should
       always be a format that is valid for GL (ie, no conversion should
       be necessary). */
    bool (*get_data)(cg_texture_t *tex,
                     cg_pixel_format_t format,
                     int rowstride,
                     uint8_t *data);

    void (*foreach_sub_texture_in_region)(cg_texture_t *tex,
                                          float virtual_tx_1,
                                          float virtual_ty_1,
                                          float virtual_tx_2,
                                          float virtual_ty_2,
                                          cg_meta_texture_callback_t callback,
                                          void *user_data);

    bool (*is_sliced)(cg_texture_t *tex);

    bool (*can_hardware_repeat)(cg_texture_t *tex);

    bool (*get_gl_texture)(cg_texture_t *tex,
                           GLuint *out_gl_handle,
                           GLenum *out_gl_target);

    /* OpenGL driver specific virtual function */
    void (*gl_flush_legacy_texobj_filters)(cg_texture_t *tex,
                                           GLenum min_filter,
                                           GLenum mag_filter);

    void (*pre_paint)(cg_texture_t *tex, cg_texture_pre_paint_flags_t flags);

    /* OpenGL driver specific virtual function */
    void (*gl_flush_legacy_texobj_wrap_modes)(cg_texture_t *tex,
                                              GLenum wrap_mode_s,
                                              GLenum wrap_mode_t,
                                              GLenum wrap_mode_p);

    cg_pixel_format_t (*get_format)(cg_texture_t *tex);
    GLenum (*get_gl_format)(cg_texture_t *tex);

    cg_texture_type_t (*get_type)(cg_texture_t *tex);

    bool (*is_foreign)(cg_texture_t *tex);

    /* Only needs to be implemented if is_primitive == true */
    void (*set_auto_mipmap)(cg_texture_t *texture, bool value);
};

typedef enum _cg_texture_soure_type_t {
    CG_TEXTURE_SOURCE_TYPE_SIZED = 1,
    CG_TEXTURE_SOURCE_TYPE_BITMAP,
    CG_TEXTURE_SOURCE_TYPE_EGL_IMAGE,
    CG_TEXTURE_SOURCE_TYPE_WEBGL_IMAGE,
    CG_TEXTURE_SOURCE_TYPE_GL_FOREIGN
} cg_texture_source_type_t;

typedef struct _cg_texture_loader_t {
    cg_texture_source_type_t src_type;
    union {
        struct {
            int width;
            int height;
            int depth; /* for 3d textures */
        } sized;
        struct {
            cg_bitmap_t *bitmap;
            int height; /* for 3d textures */
            int depth; /* for 3d textures */
            bool can_convert_in_place;
        } bitmap;
#if defined(CG_HAS_EGL_SUPPORT) && defined(EGL_KHR_image_base)
        struct {
            EGLImageKHR image;
            int width;
            int height;
            cg_pixel_format_t format;
        } egl_image;
#endif
#if defined(CG_HAS_WEBGL_SUPPORT)
        struct {
            cg_webgl_image_t *image;
            cg_pixel_format_t format;
        } webgl_image;
#endif
        struct {
            int width;
            int height;
            unsigned int gl_handle;
            cg_pixel_format_t format;
        } gl_foreign;
    } src;
} cg_texture_loader_t;

struct _cg_texture_t {
    cg_object_t _parent;
    cg_device_t *dev;
    cg_texture_loader_t *loader;
    c_llist_t *framebuffers;
    int max_level;
    int width;
    int height;
    bool allocated;

    /*
     * Internal format
     */
    cg_texture_components_t components;
    unsigned int premultiplied : 1;

    const cg_texture_vtable_t *vtable;
};

typedef enum _cg_texture_change_flags_t {
    /* Whenever the internals of a texture are changed such that the
     * underlying GL textures that represent the cg_texture_t change then
     * we notify cg-pipeline.c via
     * _cg_pipeline_texture_pre_change_notify
     */
    CG_TEXTURE_CHANGE_GL_TEXTURES
} cg_texture_change_flags_t;

typedef struct _cg_texture_pixel_t cg_texture_pixel_t;

/* This is used by the texture backends to store the first pixel of
   each GL texture. This is only used when glGenerateMipmap is not
   available so that we can temporarily set GL_GENERATE_MIPMAP and
   reupload a pixel */
struct _cg_texture_pixel_t {
    /* We need to store the format of the pixel because we store the
       data in the source format which might end up being different for
       each slice if a subregion is updated with a different format */
    GLenum gl_format;
    GLenum gl_type;
    uint8_t data[4];
};

void _cg_texture_init(cg_texture_t *texture,
                      cg_device_t *dev,
                      int width,
                      int height,
                      cg_pixel_format_t src_format,
                      cg_texture_loader_t *loader,
                      const cg_texture_vtable_t *vtable);

void _cg_texture_free(cg_texture_t *texture);

/* This is used to register a type to the list of handle types that
   will be considered a texture in cg_is_texture() */
void _cg_texture_register_texture_type(const cg_object_class_t *klass);

#define CG_TEXTURE_DEFINE(TypeName, type_name)                                 \
    CG_OBJECT_DEFINE_WITH_CODE(                                                \
        TypeName,                                                              \
        type_name,                                                             \
        _cg_texture_register_texture_type(&_cg_##type_name##_class))

#define CG_TEXTURE_INTERNAL_DEFINE(TypeName, type_name)                        \
    CG_OBJECT_INTERNAL_DEFINE_WITH_CODE(                                       \
        TypeName,                                                              \
        type_name,                                                             \
        _cg_texture_register_texture_type(&_cg_##type_name##_class))

bool _cg_texture_can_hardware_repeat(cg_texture_t *texture);

void _cg_texture_pre_paint(cg_texture_t *texture,
                           cg_texture_pre_paint_flags_t flags);

/*
 * This determines a cg_pixel_format_t according to @components and
 * @premultiplied (i.e. the user required components and whether the
 * texture should be considered premultiplied)
 *
 * A reference/source format can be given (or CG_PIXEL_FORMAT_ANY)
 * and wherever possible this function tries to simply return the
 * given source format if its compatible with the required components.
 *
 * Texture backends can call this when allocating a texture to know
 * how to convert a source image in preparation for uploading.
 */
cg_pixel_format_t _cg_texture_derive_format(cg_device_t *dev,
                                            cg_pixel_format_t src_format,
                                            cg_texture_components_t components,
                                            bool premultiplied);

/* This is a thin wrapper around _cg_texture_derive_format
 * that simply passes texture->context, texture->components and
 * texture->premultiplied in as arguments */
cg_pixel_format_t
_cg_texture_determine_internal_format(cg_texture_t *texture,
                                      cg_pixel_format_t src_format);

/* This is called by texture backends when they have successfully
 * allocated a texture.
 *
 * Most texture backends currently track the internal layout of
 * textures using a cg_pixel_format_t which will be finalized when a
 * texture is allocated. At this point we need to update
 * texture::components and texture::premultiplied according to the
 * determined layout.
 *
 * XXX: Going forward we should probably aim to stop using
 * cg_pixel_format_t at all for tracking the internal layout of
 * textures.
 */
void _cg_texture_set_internal_format(cg_texture_t *texture,
                                     cg_pixel_format_t internal_format);

bool _cg_texture_is_foreign(cg_texture_t *texture);

void _cg_texture_associate_framebuffer(cg_texture_t *texture,
                                       cg_framebuffer_t *framebuffer);

const c_llist_t *_cg_texture_get_associated_framebuffers(cg_texture_t *texture);

void _cg_texture_flush_batched_rendering(cg_texture_t *texture);

void _cg_texture_spans_foreach_in_region(cg_span_t *x_spans,
                                         int n_x_spans,
                                         cg_span_t *y_spans,
                                         int n_y_spans,
                                         cg_texture_t **textures,
                                         float *virtual_coords,
                                         float x_normalize_factor,
                                         float y_normalize_factor,
                                         cg_pipeline_wrap_mode_t wrap_x,
                                         cg_pipeline_wrap_mode_t wrap_y,
                                         cg_meta_texture_callback_t callback,
                                         void *user_data);

/*
 * _cg_texture_get_type:
 * @texture: a #cg_texture_t pointer
 *
 * Retrieves the texture type of the underlying hardware texture that
 * this #cg_texture_t will use.
 *
 * Return value: The type of the hardware texture.
 */
cg_texture_type_t _cg_texture_get_type(cg_texture_t *texture);

bool _cg_texture_needs_premult_conversion(cg_pixel_format_t src_format,
                                          cg_pixel_format_t dst_format);

int _cg_texture_get_n_levels(cg_texture_t *texture);

void _cg_texture_get_level_size(
    cg_texture_t *texture, int level, int *width, int *height, int *depth);

void _cg_texture_set_allocated(cg_texture_t *texture,
                               cg_pixel_format_t internal_format,
                               int width,
                               int height);

cg_pixel_format_t _cg_texture_get_format(cg_texture_t *texture);

cg_texture_loader_t *_cg_texture_create_loader(cg_device_t *dev);

void _cg_texture_copy_internal_format(cg_texture_t *src, cg_texture_t *dest);

#endif /* __CG_TEXTURE_PRIVATE_H */
