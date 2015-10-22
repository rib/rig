/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __CG_BUFFER_PRIVATE_H__
#define __CG_BUFFER_PRIVATE_H__

#include <clib.h>

#include "cg-object-private.h"
#include "cg-buffer.h"
#include "cg-device.h"
#include "cg-gl-header.h"

CG_BEGIN_DECLS

typedef struct _cg_buffer_vtable_t cg_buffer_vtable_t;

struct _cg_buffer_vtable_t {
    void *(*map_range)(cg_buffer_t *buffer,
                       size_t offset,
                       size_t size,
                       cg_buffer_access_t access,
                       cg_buffer_map_hint_t hints,
                       cg_error_t **error);

    void (*unmap)(cg_buffer_t *buffer);

    bool (*set_data)(cg_buffer_t *buffer,
                     unsigned int offset,
                     const void *data,
                     unsigned int size,
                     cg_error_t **error);
};

typedef enum _cg_buffer_flags_t {
    CG_BUFFER_FLAG_NONE = 0,
    CG_BUFFER_FLAG_BUFFER_OBJECT = 1UL << 0, /* real openGL buffer object */
    CG_BUFFER_FLAG_MAPPED = 1UL << 1,
    CG_BUFFER_FLAG_MAPPED_FALLBACK = 1UL << 2
} cg_buffer_flags_t;

typedef enum {
    CG_BUFFER_USAGE_HINT_TEXTURE,
    CG_BUFFER_USAGE_HINT_ATTRIBUTE_BUFFER,
    CG_BUFFER_USAGE_HINT_INDEX_BUFFER
} cg_buffer_usage_hint_t;

typedef enum {
    CG_BUFFER_BIND_TARGET_PIXEL_PACK,
    CG_BUFFER_BIND_TARGET_PIXEL_UNPACK,
    CG_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER,
    CG_BUFFER_BIND_TARGET_INDEX_BUFFER,
    CG_BUFFER_BIND_TARGET_COUNT
} cg_buffer_bind_target_t;

struct _cg_buffer_t {
    cg_object_t _parent;

    cg_device_t *dev;

    cg_buffer_vtable_t vtable;

    cg_buffer_bind_target_t last_target;

    cg_buffer_flags_t flags;

    GLuint gl_handle; /* OpenGL handle */
    unsigned int size; /* size of the buffer, in bytes */
    cg_buffer_usage_hint_t usage_hint;
    cg_buffer_update_hint_t update_hint;

    /* points to the mapped memory when the cg_buffer_t is a VBO, PBO,
     * ... or points to allocated memory in the fallback paths */
    uint8_t *data;

    int immutable_ref;

    unsigned int store_created : 1;
};

/* This is used to register a type to the list of handle types that
   will be considered a texture in cg_is_texture() */
void _cg_buffer_register_buffer_type(const cg_object_class_t *klass);

#define CG_BUFFER_DEFINE(TypeName, type_name)                                  \
    CG_OBJECT_DEFINE_WITH_CODE(                                                \
        TypeName,                                                              \
        type_name,                                                             \
        _cg_buffer_register_buffer_type(&_cg_##type_name##_class))

void _cg_buffer_initialize(cg_buffer_t *buffer,
                           cg_device_t *dev,
                           size_t size,
                           cg_buffer_bind_target_t default_target,
                           cg_buffer_usage_hint_t usage_hint,
                           cg_buffer_update_hint_t update_hint);

void _cg_buffer_fini(cg_buffer_t *buffer);

cg_buffer_usage_hint_t _cg_buffer_get_usage_hint(cg_buffer_t *buffer);

GLenum _cg_buffer_access_to_gl_enum(cg_buffer_access_t access);

cg_buffer_t *_cg_buffer_immutable_ref(cg_buffer_t *buffer);

void _cg_buffer_immutable_unref(cg_buffer_t *buffer);

/* This is a wrapper around cg_buffer_map_range for internal use
   when we want to map the buffer for write only to replace the entire
   contents. If the map fails then it will fallback to writing to a
   temporary buffer. When _cg_buffer_unmap_for_fill_or_fallback is
   called the temporary buffer will be copied into the array. Note
   that these calls share a global array so they can not be nested. */
void *_cg_buffer_map_range_for_fill_or_fallback(cg_buffer_t *buffer,
                                                size_t offset,
                                                size_t size);
void *_cg_buffer_map_for_fill_or_fallback(cg_buffer_t *buffer);

void _cg_buffer_unmap_for_fill_or_fallback(cg_buffer_t *buffer);

CG_END_DECLS

#endif /* __CG_BUFFER_PRIVATE_H__ */
