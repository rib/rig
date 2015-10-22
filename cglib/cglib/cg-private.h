/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2013 Intel Corporation.
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

#ifndef __CG_PRIVATE_H__
#define __CG_PRIVATE_H__

#include <cglib/cg-pipeline.h>

#include "cg-device.h"
#include "cg-flags.h"

CG_BEGIN_DECLS

typedef enum {
    CG_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE,
    CG_PRIVATE_FEATURE_MESA_PACK_INVERT,
    CG_PRIVATE_FEATURE_OFFSCREEN_BLIT,
    CG_PRIVATE_FEATURE_PBOS,
    CG_PRIVATE_FEATURE_VBOS,
    CG_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL,
    CG_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL,
    CG_PRIVATE_FEATURE_TEXTURE_FORMAT_BGRA8888,
    CG_PRIVATE_FEATURE_UNPACK_SUBIMAGE,
    CG_PRIVATE_FEATURE_SAMPLER_OBJECTS,
    CG_PRIVATE_FEATURE_READ_PIXELS_ANY_FORMAT,
    CG_PRIVATE_FEATURE_FORMAT_CONVERSION,
    CG_PRIVATE_FEATURE_QUADS,
    CG_PRIVATE_FEATURE_BLEND_CONSTANT,
    CG_PRIVATE_FEATURE_QUERY_FRAMEBUFFER_BITS,
    CG_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM,
    CG_PRIVATE_FEATURE_QUERY_TEXTURE_PARAMETERS,
    CG_PRIVATE_FEATURE_ALPHA_TEXTURES,
    CG_PRIVATE_FEATURE_TEXTURE_SWIZZLE,
    CG_PRIVATE_FEATURE_TEXTURE_MAX_LEVEL,
    CG_PRIVATE_FEATURE_OES_EGL_SYNC,
    /* If this is set then the winsys is responsible for queueing dirty
     * events. Otherwise a dirty event will be queued when the onscreen
     * is first allocated or when it is shown or resized */
    CG_PRIVATE_FEATURE_DIRTY_EVENTS,
    CG_PRIVATE_FEATURE_ENABLE_PROGRAM_POINT_SIZE,
    /* These features let us avoid conditioning code based on the exact
     * driver being used and instead check for broad opengl feature
     * sets that can be shared by several GL apis */
    CG_PRIVATE_FEATURE_ANY_GL,
    CG_PRIVATE_FEATURE_GL_PROGRAMMABLE,
    CG_PRIVATE_FEATURE_GL_EMBEDDED,
    CG_PRIVATE_FEATURE_GL_WEB,
    CG_N_PRIVATE_FEATURES
} cg_private_feature_t;

/* Sometimes when evaluating pipelines, either during comparisons or
 * if calculating a hash value we need to tweak the evaluation
 * semantics */
typedef enum _cg_pipeline_eval_flags_t {
    CG_PIPELINE_EVAL_FLAG_NONE = 0
} cg_pipeline_eval_flags_t;

void _cg_transform_point(const c_matrix_t *matrix_mv,
                         const c_matrix_t *matrix_p,
                         const float *viewport,
                         float *x,
                         float *y);

bool _cg_check_extension(const char *name, char *const *ext);

void _cg_flush(cg_device_t *dev);

void _cg_clear(const cg_color_t *color, unsigned long buffers);

void _cg_init(void);

#define _cg_has_private_feature(ctx, feature)                                  \
    CG_FLAGS_GET((ctx)->private_features, (feature))

/*
 * _cg_pixel_format_get_bytes_per_pixel:
 * @format: a #cg_pixel_format_t
 *
 * Queries how many bytes a pixel of the given @format takes.
 *
 * Return value: The number of bytes taken for a pixel of the given
 *               @format.
 */
int _cg_pixel_format_get_bytes_per_pixel(cg_pixel_format_t format);

/*
 * _cg_pixel_format_has_aligned_components:
 * @format: a #cg_pixel_format_t
 *
 * Queries whether the ordering of the components for the given
 * @format depend on the endianness of the host CPU or if the
 * components can be accessed using bit shifting and bitmasking by
 * loading a whole pixel into a word.
 *
 * XXX: If we ever consider making something like this public we
 * should really try to think of a better name and come up with
 * much clearer documentation since it really depends on what
 * point of view you consider this from whether a format like
 * CG_PIXEL_FORMAT_RGBA_8888 is endian dependent. E.g. If you
 * read an RGBA_8888 pixel into a uint32
 * it's endian dependent how you mask out the different channels.
 * But If you already have separate color components and you want
 * to write them to an RGBA_8888 pixel then the bytes can be
 * written sequentially regardless of the endianness.
 *
 * Return value: %true if you need to consider the host CPU
 *               endianness when dealing with the given @format
 *               else %false.
 */
bool _cg_pixel_format_is_endian_dependant(cg_pixel_format_t format);

bool
_cg_pixel_format_is_premultiplied(cg_pixel_format_t format);

bool
_cg_pixel_format_has_alpha(cg_pixel_format_t format);

/*
 * _cg_pixel_format_can_be_premultiplied:
 * @format: a #cg_pixel_format_t
 *
 * Returns true if the pixel format can be premultiplied. This is
 * currently true for all formats that have an alpha channel except
 * %CG_PIXEL_FORMAT_A_8 (because that doesn't have any other
 * components to multiply by the alpha).
 */
bool
_cg_pixel_format_can_be_premultiplied(cg_pixel_format_t format);

/* If the format is premultiplied, return the closest non-premultiplied format */
cg_pixel_format_t
_cg_pixel_format_premult_stem(cg_pixel_format_t format);

cg_pixel_format_t
_cg_pixel_format_premultiply(cg_pixel_format_t format);

cg_texture_components_t
_cg_pixel_format_get_components(cg_pixel_format_t format);

cg_pixel_format_t
_cg_pixel_format_toggle_premult_status(cg_pixel_format_t format);

cg_pixel_format_t
_cg_pixel_format_flip_rgb_order(cg_pixel_format_t format);

cg_pixel_format_t
_cg_pixel_format_flip_alpha_position(cg_pixel_format_t format);

CG_END_DECLS

#endif /* __CG_PRIVATE_H__ */
