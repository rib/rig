/*
 * Cogl
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

#include "cogl-object-private.h"
#include "cogl-attribute.h"
#include "cogl-framebuffer.h"
#include "cogl-pipeline-private.h"
#include "cogl-boxed-value.h"

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
    bool normalized;

    bool is_buffered;

    union {
        struct {
            cg_attribute_buffer_t *attribute_buffer;
            size_t stride;
            size_t offset;
            int n_components;
            cg_attribute_type_t type;
        } buffered;
        struct {
            cg_context_t *context;
            cg_boxed_value_t boxed;
        } constant;
    } d;

    int immutable_ref;
};

typedef enum {
    CG_DRAW_SKIP_JOURNAL_FLUSH = 1 << 0,
    CG_DRAW_SKIP_PIPELINE_VALIDATION = 1 << 1,
    CG_DRAW_SKIP_FRAMEBUFFER_FLUSH = 1 << 2,
    /* By default the vertex attribute drawing code will assume that if
       there is a color attribute array enabled then we can't determine
       if the colors will be opaque so we need to enabling
       blending. However when drawing from the journal we know what the
       contents of the color array is so we can override this by passing
       this flag. */
    CG_DRAW_COLOR_ATTRIBUTE_IS_OPAQUE = 1 << 3,
    /* This forcibly disables the debug option to divert all drawing to
     * wireframes */
    CG_DRAW_SKIP_DEBUG_WIREFRAME = 1 << 4
} cg_draw_flags_t;

/* During cg_context_t initialization we register the "cg_color_in"
 * attribute name so it gets a global name_index of 0. We need to know
 * the name_index for "cg_color_in" in
 * _cg_pipeline_flush_gl_state() */
#define CG_ATTRIBUTE_COLOR_NAME_INDEX 0

cg_attribute_name_state_t *
_cg_attribute_register_attribute_name(cg_context_t *context, const char *name);

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
