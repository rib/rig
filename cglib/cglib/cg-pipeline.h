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

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_PIPELINE_H__
#define __CG_PIPELINE_H__

/* We forward declare the cg_pipeline_t type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _cg_pipeline_t cg_pipeline_t;

#include <cglib/cg-types.h>
#include <cglib/cg-device.h>
#include <cglib/cg-snippet.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-pipeline
 * @short_description: Functions for creating and manipulating the GPU
 *                     pipeline
 *
 * CGlib allows creating and manipulating objects representing the full
 * configuration of the GPU pipeline. In simplified terms the GPU
 * pipeline takes primitive geometry as the input, it first performs
 * vertex processing, allowing you to deform your geometry, then
 * rasterizes that (turning it from pure geometry into fragments) then
 * performs fragment processing including depth testing and texture
 * mapping. Finally it blends the result with the framebuffer.
 */

#define CG_PIPELINE(OBJECT) ((cg_pipeline_t *)OBJECT)

/**
 * cg_pipeline_new:
 * @dev: A #cg_device_t
 *
 * Allocates and initializes a default simple pipeline that will color
 * a primitive white.
 *
 * Return value: a pointer to a new #cg_pipeline_t
 *
 * Stability: Unstable
 */
cg_pipeline_t *cg_pipeline_new(cg_device_t *dev);

/**
 * cg_pipeline_copy:
 * @source: a #cg_pipeline_t object to copy
 *
 * Creates a new pipeline with the configuration copied from the
 * source pipeline.
 *
 * We would strongly advise developers to always aim to use
 * cg_pipeline_copy() instead of cg_pipeline_new() whenever there will
 * be any similarity between two pipelines. Copying a pipeline helps CGlib
 * keep track of a pipelines ancestry which we may use to help minimize GPU
 * state changes.
 *
 * Returns: a pointer to the newly allocated #cg_pipeline_t
 *
 * Stability: Unstable
 */
cg_pipeline_t *cg_pipeline_copy(cg_pipeline_t *source);

/**
 * cg_is_pipeline:
 * @object: A #cg_object_t
 *
 * Gets whether the given @object references an existing pipeline object.
 *
 * Return value: %true if the @object references a #cg_pipeline_t,
 *   %false otherwise
 *
 * Stability: Unstable
 */
bool cg_is_pipeline(void *object);

/**
 * cg_pipeline_layer_callback_t:
 * @pipeline: The #cg_pipeline_t whos layers are being iterated
 * @layer_index: The current layer index
 * @user_data: The private data passed to cg_pipeline_foreach_layer()
 *
 * The callback prototype used with cg_pipeline_foreach_layer() for
 * iterating all the layers of a @pipeline.
 *
 * Stability: Unstable
 */
typedef bool (*cg_pipeline_layer_callback_t)(cg_pipeline_t *pipeline,
                                             int layer_index,
                                             void *user_data);

/**
 * cg_pipeline_foreach_layer:
 * @pipeline: A #cg_pipeline_t object
 * @callback: (scope call): A #cg_pipeline_layer_callback_t to be
 *            called for each layer index
 * @user_data: (closure): Private data that will be passed to the
 *             callback
 *
 * Iterates all the layer indices of the given @pipeline.
 *
 * Stability: Unstable
 */
void cg_pipeline_foreach_layer(cg_pipeline_t *pipeline,
                               cg_pipeline_layer_callback_t callback,
                               void *user_data);

/**
 * cg_pipeline_get_uniform_location:
 * @pipeline: A #cg_pipeline_t object
 * @uniform_name: The name of a uniform
 *
 * This is used to get an integer representing the uniform with the
 * name @uniform_name. The integer can be passed to functions such as
 * cg_pipeline_set_uniform_1f() to set the value of a uniform.
 *
 * This function will always return a valid integer. Ie, unlike
 * OpenGL, it does not return -1 if the uniform is not available in
 * this pipeline so it can not be used to test whether uniforms are
 * present. It is not necessary to set the program on the pipeline
 * before calling this function.
 *
 * Return value: A integer representing the location of the given uniform.
 *
 * Stability: Unstable
 */
int cg_pipeline_get_uniform_location(cg_pipeline_t *pipeline,
                                     const char *uniform_name);

CG_END_DECLS

#endif /* __CG_PIPELINE_H__ */
