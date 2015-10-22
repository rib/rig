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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_INDICES_H__
#define __CG_INDICES_H__

/* We forward declare the cg_indices_t type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _cg_indices_t cg_indices_t;

#include <cglib/cg-index-buffer.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-indices
 * @short_description: Describe vertex indices stored in a #cg_index_buffer_t.
 *
 * Indices allow you to avoid duplicating vertices in your vertex data
 * by virtualizing your data and instead providing a sequence of index
 * values that tell the GPU which data should be used for each vertex.
 *
 * If the GPU is given a sequence of indices it doesn't simply walk
 * through each vertex of your data in order it will instead walk
 * through the indices which can provide random access to the
 * underlying data.
 *
 * Since it's very common to have duplicate vertices when describing a
 * shape as a list of triangles it can often be a significant space
 * saving to describe geometry using indices. Reducing the size of
 * your models can make it cheaper to map them into the GPU by
 * reducing the demand on memory bandwidth and may help to make better
 * use of your GPUs internal vertex caching.
 *
 * For example, to describe a quadrilateral as 2 triangles for the GPU
 * you could either provide data with 6 vertices or instead with
 * indices you can provide vertex data for just 4 vertices and an
 * index buffer that specfies the 6 vertices by indexing the shared
 * vertices multiple times.
 *
 * |[
 *   CGlibVertex2f quad_vertices[] = {
 *     {x0, y0}, //0 = top left
 *     {x1, y1}, //1 = bottom left
 *     {x2, y2}, //2 = bottom right
 *     {x3, y3}, //3 = top right
 *   };
 *   //tell the gpu how to interpret the quad as 2 triangles...
 *   unsigned char indices[] = {0, 1, 2, 0, 2, 3};
 * ]|
 *
 * Even in the above illustration we see a saving of 10bytes for one
 * quad compared to having data for 6 vertices and no indices but if
 * you need to draw 100s or 1000s of quads then its really quite
 * significant.
 *
 * Something else to consider is that often indices can be defined
 * once and remain static while the vertex data may change for
 * animations perhaps. That means you may be able to ignore the
 * negligable cost of mapping your indices into the GPU if they don't
 * ever change.
 *
 * The above illustration is actually a good example of static indices
 * because it's really common that developers have quad mesh data that
 * they need to display and we know exactly what that indices array
 * needs to look like depending on the number of quads that need to be
 * drawn. It doesn't matter how the quads might be animated and
 * changed the indices will remain the same. CGlib even has a utility
 * (cg_get_rectangle_indices()) to get access to re-useable indices
 * for drawing quads as above.
 */

cg_indices_t *cg_indices_new(cg_device_t *dev,
                             cg_indices_type_t type,
                             const void *indices_data,
                             int n_indices);

cg_indices_t *cg_indices_new_for_buffer(cg_indices_type_t type,
                                        cg_index_buffer_t *buffer,
                                        size_t offset);

cg_index_buffer_t *cg_indices_get_buffer(cg_indices_t *indices);

cg_indices_type_t cg_indices_get_type(cg_indices_t *indices);

size_t cg_indices_get_offset(cg_indices_t *indices);

void cg_indices_set_offset(cg_indices_t *indices, size_t offset);

cg_indices_t *cg_get_rectangle_indices(cg_device_t *dev, int n_rectangles);

/**
 * cg_is_indices:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given object references a #cg_indices_t.
 *
 * Return value: %true if the object references a #cg_indices_t
 *   and %false otherwise.
 * Stability: unstable
 */
bool cg_is_indices(void *object);

CG_END_DECLS

#endif /* __CG_INDICES_H__ */
