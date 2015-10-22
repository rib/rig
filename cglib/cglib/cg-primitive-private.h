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

#ifndef __CG_PRIMITIVE_PRIVATE_H
#define __CG_PRIMITIVE_PRIVATE_H

#include "cg-object-private.h"
#include "cg-attribute-buffer-private.h"
#include "cg-attribute-private.h"
#include "cg-framebuffer.h"

struct _cg_primitive_t {
    cg_object_t _parent;

    cg_indices_t *indices;
    cg_vertices_mode_t mode;
    int first_vertex;
    int n_vertices;

    int immutable_ref;

    cg_attribute_t **attributes;
    int n_attributes;

    int n_embedded_attributes;
    cg_attribute_t *embedded_attribute;
};

cg_primitive_t *_cg_primitive_immutable_ref(cg_primitive_t *primitive);

void _cg_primitive_immutable_unref(cg_primitive_t *primitive);

void _cg_primitive_draw(cg_primitive_t *primitive,
                        cg_framebuffer_t *framebuffer,
                        cg_pipeline_t *pipeline,
                        int n_instances,
                        cg_draw_flags_t flags);

#endif /* __CG_PRIMITIVE_PRIVATE_H */
