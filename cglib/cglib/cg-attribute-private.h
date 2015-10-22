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
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __CG_ATTRIBUTE_PRIVATE_H
#define __CG_ATTRIBUTE_PRIVATE_H

#include "cg-object-private.h"
#include "cg-attribute.h"
#include "cg-framebuffer.h"
#include "cg-pipeline-private.h"
#include "cg-boxed-value.h"

typedef enum {
    CG_ATTRIBUTE_NAME_ID_POSITION_ARRAY,
    CG_ATTRIBUTE_NAME_ID_COLOR_ARRAY,
    CG_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY,
    CG_ATTRIBUTE_NAME_ID_NORMAL_ARRAY,
    CG_ATTRIBUTE_NAME_ID_POINT_SIZE_ARRAY,
    CG_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY
} cg_attribute_name_id_t;

typedef struct _cg_attribute_name_state_t {
    char *name;
    cg_attribute_name_id_t name_id;
    int name_index;
    bool normalized_default;
    int layer_number;
} cg_attribute_name_state_t;

struct _cg_attribute_t {
    cg_object_t _parent;

    const cg_attribute_name_state_t *name_state;

    union {
        struct {
            cg_attribute_buffer_t *attribute_buffer;
            size_t stride;
            size_t offset;
            int n_components;
            cg_attribute_type_t type;
        } buffered;
        struct {
            cg_device_t *dev;
            cg_boxed_value_t boxed;
        } constant;
    } d;

    int immutable_ref;
    int instance_stride;

    unsigned int normalized : 1;
    unsigned int is_buffered : 1;
};

typedef enum {
    CG_DRAW_SKIP_FRAMEBUFFER_FLUSH = 1 << 1,
    /* This forcibly disables the debug option to divert all drawing to
     * wireframes */
    CG_DRAW_SKIP_DEBUG_WIREFRAME = 1 << 2
} cg_draw_flags_t;

/* During cg_device_t initialization we register the "cg_color_in"
 * attribute name so it gets a global name_index of 0. We need to know
 * the name_index for "cg_color_in" in
 * _cg_pipeline_flush_gl_state() */
#define CG_ATTRIBUTE_COLOR_NAME_INDEX 0

cg_attribute_name_state_t *
_cg_attribute_register_attribute_name(cg_device_t *dev, const char *name);

cg_attribute_t *_cg_attribute_immutable_ref(cg_attribute_t *attribute);

void _cg_attribute_immutable_unref(cg_attribute_t *attribute);

typedef struct {
    int unit;
    cg_pipeline_flush_options_t options;
    uint32_t fallback_layers;
} cg_flush_layer_state_t;

void _cg_flush_attributes_state(cg_framebuffer_t *framebuffer,
                                cg_pipeline_t *pipeline,
                                cg_draw_flags_t flags,
                                cg_attribute_t **attributes,
                                int n_attributes);

int _cg_attribute_get_n_components(cg_attribute_t *attribute);

#endif /* __CG_ATTRIBUTE_PRIVATE_H */
