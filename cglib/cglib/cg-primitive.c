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

#include <cglib-config.h>

#include "cg-util.h"
#include "cg-object-private.h"
#include "cg-primitive.h"
#include "cg-primitive-private.h"
#include "cg-attribute-private.h"
#include "cg-framebuffer-private.h"

#include <stdarg.h>
#include <string.h>

static void _cg_primitive_free(cg_primitive_t *primitive);

CG_OBJECT_DEFINE(Primitive, primitive);

cg_primitive_t *
cg_primitive_new_with_attributes(cg_vertices_mode_t mode,
                                 int n_vertices,
                                 cg_attribute_t **attributes,
                                 int n_attributes)
{
    cg_primitive_t *primitive;
    int i;

    primitive = c_slice_alloc(sizeof(cg_primitive_t) +
                              sizeof(cg_attribute_t *) * (n_attributes - 1));
    primitive->mode = mode;
    primitive->first_vertex = 0;
    primitive->n_vertices = n_vertices;
    primitive->indices = NULL;
    primitive->immutable_ref = 0;

    primitive->n_attributes = n_attributes;
    primitive->n_embedded_attributes = n_attributes;
    primitive->attributes = &primitive->embedded_attribute;
    for (i = 0; i < n_attributes; i++) {
        cg_attribute_t *attribute = attributes[i];
        cg_object_ref(attribute);

        c_return_val_if_fail(cg_is_attribute(attribute), NULL);

        primitive->attributes[i] = attribute;
    }

    return _cg_primitive_object_new(primitive);
}

/* This is just an internal convenience wrapper around
   new_with_attributes that also unrefs the attributes. It is just
   used for the builtin struct constructors */
static cg_primitive_t *
_cg_primitive_new_with_attributes_unref(cg_vertices_mode_t mode,
                                        int n_vertices,
                                        cg_attribute_t **attributes,
                                        int n_attributes)
{
    cg_primitive_t *primitive;
    int i;

    primitive = cg_primitive_new_with_attributes(
        mode, n_vertices, attributes, n_attributes);

    for (i = 0; i < n_attributes; i++)
        cg_object_unref(attributes[i]);

    return primitive;
}

cg_primitive_t *
cg_primitive_new(cg_vertices_mode_t mode, int n_vertices, ...)
{
    va_list ap;
    int n_attributes;
    cg_attribute_t **attributes;
    int i;
    cg_attribute_t *attribute;

    va_start(ap, n_vertices);
    for (n_attributes = 0; va_arg(ap, cg_attribute_t *); n_attributes++)
        ;
    va_end(ap);

    attributes = c_alloca(sizeof(cg_attribute_t *) * n_attributes);

    va_start(ap, n_vertices);
    for (i = 0; (attribute = va_arg(ap, cg_attribute_t *)); i++)
        attributes[i] = attribute;
    va_end(ap);

    return cg_primitive_new_with_attributes(mode, n_vertices, attributes, i);
}

cg_primitive_t *
cg_primitive_new_p2(cg_device_t *dev,
                    cg_vertices_mode_t mode,
                    int n_vertices,
                    const cg_vertex_p2_t *data)
{
    cg_attribute_buffer_t *attribute_buffer =
        cg_attribute_buffer_new(dev, n_vertices * sizeof(cg_vertex_p2_t),
                                data);
    cg_attribute_t *attributes[1];

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(cg_vertex_p2_t),
                                     offsetof(cg_vertex_p2_t, x),
                                     2,
                                     CG_ATTRIBUTE_TYPE_FLOAT);

    cg_object_unref(attribute_buffer);

    return _cg_primitive_new_with_attributes_unref(
        mode, n_vertices, attributes, 1);
}

cg_primitive_t *
cg_primitive_new_p3(cg_device_t *dev,
                    cg_vertices_mode_t mode,
                    int n_vertices,
                    const cg_vertex_p3_t *data)
{
    cg_attribute_buffer_t *attribute_buffer =
        cg_attribute_buffer_new(dev, n_vertices * sizeof(cg_vertex_p3_t),
                                data);
    cg_attribute_t *attributes[1];

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(cg_vertex_p3_t),
                                     offsetof(cg_vertex_p3_t, x),
                                     3,
                                     CG_ATTRIBUTE_TYPE_FLOAT);

    cg_object_unref(attribute_buffer);

    return _cg_primitive_new_with_attributes_unref(
        mode, n_vertices, attributes, 1);
}

cg_primitive_t *
cg_primitive_new_p2c4(cg_device_t *dev,
                      cg_vertices_mode_t mode,
                      int n_vertices,
                      const cg_vertex_p2c4_t *data)
{
    cg_attribute_buffer_t *attribute_buffer = cg_attribute_buffer_new(dev,
                                                                      n_vertices * sizeof(cg_vertex_p2c4_t),
                                                                      data);
    cg_attribute_t *attributes[2];

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(cg_vertex_p2c4_t),
                                     offsetof(cg_vertex_p2c4_t, x),
                                     2,
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[1] = cg_attribute_new(attribute_buffer,
                                     "cg_color_in",
                                     sizeof(cg_vertex_p2c4_t),
                                     offsetof(cg_vertex_p2c4_t, r),
                                     4,
                                     CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

    cg_object_unref(attribute_buffer);

    return _cg_primitive_new_with_attributes_unref(
        mode, n_vertices, attributes, 2);
}

cg_primitive_t *
cg_primitive_new_p3c4(cg_device_t *dev,
                      cg_vertices_mode_t mode,
                      int n_vertices,
                      const cg_vertex_p3c4_t *data)
{
    cg_attribute_buffer_t *attribute_buffer = cg_attribute_buffer_new(dev,
                                                                      n_vertices * sizeof(cg_vertex_p3c4_t),
                                                                      data);
    cg_attribute_t *attributes[2];

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(cg_vertex_p3c4_t),
                                     offsetof(cg_vertex_p3c4_t, x),
                                     3,
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[1] = cg_attribute_new(attribute_buffer,
                                     "cg_color_in",
                                     sizeof(cg_vertex_p3c4_t),
                                     offsetof(cg_vertex_p3c4_t, r),
                                     4,
                                     CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

    cg_object_unref(attribute_buffer);

    return _cg_primitive_new_with_attributes_unref(
        mode, n_vertices, attributes, 2);
}

cg_primitive_t *
cg_primitive_new_p2t2(cg_device_t *dev,
                      cg_vertices_mode_t mode,
                      int n_vertices,
                      const cg_vertex_p2t2_t *data)
{
    cg_attribute_buffer_t *attribute_buffer = cg_attribute_buffer_new(dev,
                                                                      n_vertices * sizeof(cg_vertex_p2t2_t),
                                                                      data);
    cg_attribute_t *attributes[2];

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(cg_vertex_p2t2_t),
                                     offsetof(cg_vertex_p2t2_t, x),
                                     2,
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[1] = cg_attribute_new(attribute_buffer,
                                     "cg_tex_coord0_in",
                                     sizeof(cg_vertex_p2t2_t),
                                     offsetof(cg_vertex_p2t2_t, s),
                                     2,
                                     CG_ATTRIBUTE_TYPE_FLOAT);

    cg_object_unref(attribute_buffer);

    return _cg_primitive_new_with_attributes_unref(
        mode, n_vertices, attributes, 2);
}

cg_primitive_t *
cg_primitive_new_p3t2(cg_device_t *dev,
                      cg_vertices_mode_t mode,
                      int n_vertices,
                      const cg_vertex_p3t2_t *data)
{
    cg_attribute_buffer_t *attribute_buffer = cg_attribute_buffer_new(dev,
                                                                      n_vertices * sizeof(cg_vertex_p3t2_t),
                                                                      data);
    cg_attribute_t *attributes[2];

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(cg_vertex_p3t2_t),
                                     offsetof(cg_vertex_p3t2_t, x),
                                     3,
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[1] = cg_attribute_new(attribute_buffer,
                                     "cg_tex_coord0_in",
                                     sizeof(cg_vertex_p3t2_t),
                                     offsetof(cg_vertex_p3t2_t, s),
                                     2,
                                     CG_ATTRIBUTE_TYPE_FLOAT);

    cg_object_unref(attribute_buffer);

    return _cg_primitive_new_with_attributes_unref(
        mode, n_vertices, attributes, 2);
}

cg_primitive_t *
cg_primitive_new_p2t2c4(cg_device_t *dev,
                        cg_vertices_mode_t mode,
                        int n_vertices,
                        const cg_vertex_p2t2c4_t *data)
{
    cg_attribute_buffer_t *attribute_buffer = cg_attribute_buffer_new(dev,
                                                                      n_vertices * sizeof(cg_vertex_p2t2c4_t),
                                                                      data);
    cg_attribute_t *attributes[3];

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(cg_vertex_p2t2c4_t),
                                     offsetof(cg_vertex_p2t2c4_t, x),
                                     2,
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[1] = cg_attribute_new(attribute_buffer,
                                     "cg_tex_coord0_in",
                                     sizeof(cg_vertex_p2t2c4_t),
                                     offsetof(cg_vertex_p2t2c4_t, s),
                                     2,
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[2] = cg_attribute_new(attribute_buffer,
                                     "cg_color_in",
                                     sizeof(cg_vertex_p2t2c4_t),
                                     offsetof(cg_vertex_p2t2c4_t, r),
                                     4,
                                     CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

    cg_object_unref(attribute_buffer);

    return _cg_primitive_new_with_attributes_unref(
        mode, n_vertices, attributes, 3);
}

cg_primitive_t *
cg_primitive_new_p3t2c4(cg_device_t *dev,
                        cg_vertices_mode_t mode,
                        int n_vertices,
                        const cg_vertex_p3t2c4_t *data)
{
    cg_attribute_buffer_t *attribute_buffer = cg_attribute_buffer_new(dev,
                                                                      n_vertices * sizeof(cg_vertex_p3t2c4_t),
                                                                      data);
    cg_attribute_t *attributes[3];

    attributes[0] = cg_attribute_new(attribute_buffer,
                                     "cg_position_in",
                                     sizeof(cg_vertex_p3t2c4_t),
                                     offsetof(cg_vertex_p3t2c4_t, x),
                                     3,
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[1] = cg_attribute_new(attribute_buffer,
                                     "cg_tex_coord0_in",
                                     sizeof(cg_vertex_p3t2c4_t),
                                     offsetof(cg_vertex_p3t2c4_t, s),
                                     2,
                                     CG_ATTRIBUTE_TYPE_FLOAT);
    attributes[2] = cg_attribute_new(attribute_buffer,
                                     "cg_color_in",
                                     sizeof(cg_vertex_p3t2c4_t),
                                     offsetof(cg_vertex_p3t2c4_t, r),
                                     4,
                                     CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE);

    cg_object_unref(attribute_buffer);

    return _cg_primitive_new_with_attributes_unref(
        mode, n_vertices, attributes, 3);
}

static void
_cg_primitive_free(cg_primitive_t *primitive)
{
    int i;

    for (i = 0; i < primitive->n_attributes; i++)
        cg_object_unref(primitive->attributes[i]);

    if (primitive->attributes != &primitive->embedded_attribute)
        c_slice_free1(sizeof(cg_attribute_t *) * primitive->n_attributes,
                      primitive->attributes);

    if (primitive->indices)
        cg_object_unref(primitive->indices);

    c_slice_free1(sizeof(cg_primitive_t) +
                  sizeof(cg_attribute_t *) *
                  (primitive->n_embedded_attributes - 1),
                  primitive);
}

static void
warn_about_midscene_changes(void)
{
    static bool seen = false;
    if (!seen) {
        c_warning("Mid-scene modification of primitives has "
                  "undefined results\n");
        seen = true;
    }
}

void
cg_primitive_set_attributes(cg_primitive_t *primitive,
                            cg_attribute_t **attributes,
                            int n_attributes)
{
    int i;

    c_return_if_fail(cg_is_primitive(primitive));

    if (C_UNLIKELY(primitive->immutable_ref)) {
        warn_about_midscene_changes();
        return;
    }

    /* NB: we don't unref the previous attributes before refing the new
     * in case we would end up releasing the last reference for an
     * attribute thats actually in the new list too. */
    for (i = 0; i < n_attributes; i++) {
        c_return_if_fail(cg_is_attribute(attributes[i]));
        cg_object_ref(attributes[i]);
    }

    for (i = 0; i < primitive->n_attributes; i++)
        cg_object_unref(primitive->attributes[i]);

    /* First try to use the embedded storage assocated with the
     * primitive, else fallback to slice allocating separate storage for
     * the attribute pointers... */

    if (n_attributes <= primitive->n_embedded_attributes) {
        if (primitive->attributes != &primitive->embedded_attribute)
            c_slice_free1(sizeof(cg_attribute_t *) * primitive->n_attributes,
                          primitive->attributes);
        primitive->attributes = &primitive->embedded_attribute;
    } else {
        if (primitive->attributes != &primitive->embedded_attribute)
            c_slice_free1(sizeof(cg_attribute_t *) * primitive->n_attributes,
                          primitive->attributes);
        primitive->attributes =
            c_slice_alloc(sizeof(cg_attribute_t *) * n_attributes);
    }

    memcpy(primitive->attributes,
           attributes,
           sizeof(cg_attribute_t *) * n_attributes);

    primitive->n_attributes = n_attributes;
}

int
cg_primitive_get_first_vertex(cg_primitive_t *primitive)
{
    c_return_val_if_fail(cg_is_primitive(primitive), 0);

    return primitive->first_vertex;
}

void
cg_primitive_set_first_vertex(cg_primitive_t *primitive, int first_vertex)
{
    c_return_if_fail(cg_is_primitive(primitive));

    if (C_UNLIKELY(primitive->immutable_ref)) {
        warn_about_midscene_changes();
        return;
    }

    primitive->first_vertex = first_vertex;
}

int
cg_primitive_get_n_vertices(cg_primitive_t *primitive)
{
    c_return_val_if_fail(cg_is_primitive(primitive), 0);

    return primitive->n_vertices;
}

void
cg_primitive_set_n_vertices(cg_primitive_t *primitive, int n_vertices)
{
    c_return_if_fail(cg_is_primitive(primitive));

    primitive->n_vertices = n_vertices;
}

cg_vertices_mode_t
cg_primitive_get_mode(cg_primitive_t *primitive)
{
    c_return_val_if_fail(cg_is_primitive(primitive), 0);

    return primitive->mode;
}

void
cg_primitive_set_mode(cg_primitive_t *primitive, cg_vertices_mode_t mode)
{
    c_return_if_fail(cg_is_primitive(primitive));

    if (C_UNLIKELY(primitive->immutable_ref)) {
        warn_about_midscene_changes();
        return;
    }

    primitive->mode = mode;
}

void
cg_primitive_set_indices(cg_primitive_t *primitive,
                         cg_indices_t *indices,
                         int n_indices)
{
    c_return_if_fail(cg_is_primitive(primitive));

    if (C_UNLIKELY(primitive->immutable_ref)) {
        warn_about_midscene_changes();
        return;
    }

    if (indices)
        cg_object_ref(indices);
    if (primitive->indices)
        cg_object_unref(primitive->indices);
    primitive->indices = indices;
    primitive->n_vertices = n_indices;
}

cg_indices_t *
cg_primitive_get_indices(cg_primitive_t *primitive)
{
    return primitive->indices;
}

cg_primitive_t *
cg_primitive_copy(cg_primitive_t *primitive)
{
    cg_primitive_t *copy;

    copy = cg_primitive_new_with_attributes(primitive->mode,
                                            primitive->n_vertices,
                                            primitive->attributes,
                                            primitive->n_attributes);

    cg_primitive_set_indices(copy, primitive->indices, primitive->n_vertices);
    cg_primitive_set_first_vertex(copy, primitive->first_vertex);

    return copy;
}

cg_primitive_t *
_cg_primitive_immutable_ref(cg_primitive_t *primitive)
{
    int i;

    c_return_val_if_fail(cg_is_primitive(primitive), NULL);

    primitive->immutable_ref++;

    for (i = 0; i < primitive->n_attributes; i++)
        _cg_attribute_immutable_ref(primitive->attributes[i]);

    return primitive;
}

void
_cg_primitive_immutable_unref(cg_primitive_t *primitive)
{
    int i;

    c_return_if_fail(cg_is_primitive(primitive));
    c_return_if_fail(primitive->immutable_ref > 0);

    primitive->immutable_ref--;

    for (i = 0; i < primitive->n_attributes; i++)
        _cg_attribute_immutable_unref(primitive->attributes[i]);
}

void
cg_primitive_foreach_attribute(cg_primitive_t *primitive,
                               cg_primitive_attribute_callback_t callback,
                               void *user_data)
{
    int i;

    for (i = 0; i < primitive->n_attributes; i++)
        if (!callback(primitive, primitive->attributes[i], user_data))
            break;
}

void
_cg_primitive_draw(cg_primitive_t *primitive,
                   cg_framebuffer_t *framebuffer,
                   cg_pipeline_t *pipeline,
                   int n_instances,
                   cg_draw_flags_t flags)
{
    if (primitive->indices)
        _cg_framebuffer_draw_indexed_attributes(framebuffer,
                                                pipeline,
                                                primitive->mode,
                                                primitive->first_vertex,
                                                primitive->n_vertices,
                                                primitive->indices,
                                                primitive->attributes,
                                                primitive->n_attributes,
                                                n_instances,
                                                flags);
    else
        _cg_framebuffer_draw_attributes(framebuffer,
                                        pipeline,
                                        primitive->mode,
                                        primitive->first_vertex,
                                        primitive->n_vertices,
                                        primitive->attributes,
                                        primitive->n_attributes,
                                        n_instances,
                                        flags);
}

void
cg_primitive_draw(cg_primitive_t *primitive,
                  cg_framebuffer_t *framebuffer,
                  cg_pipeline_t *pipeline)
{
    _cg_primitive_draw(primitive, framebuffer, pipeline, 1, 0 /* flags */);
}

void
cg_primitive_draw_instances(cg_primitive_t *primitive,
                            cg_framebuffer_t *framebuffer,
                            cg_pipeline_t *pipeline,
                            int n_instances)
{
    _cg_primitive_draw(primitive, framebuffer, pipeline,
                       n_instances, 0 /* flags */);
}
