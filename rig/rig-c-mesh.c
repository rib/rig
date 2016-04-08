/*
 * Rig C
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2016  Intel Corporation.
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
 */

#include <rig-config.h>

#include <rut.h>

#include "rig-c.h"
#include "rig-c-mesh.h"
#include "rig-engine.h"
#include "rig-code-module.h"
#include "rig-curses-debug.h"
#include "rig-entity.h"

#include "components/rig-mesh.h"

_Static_assert((int)R_VERTICES_MODE_POINTS == (int)CG_VERTICES_MODE_POINTS, "VerticesMode enum error");
_Static_assert((int)R_VERTICES_MODE_LINES == (int)CG_VERTICES_MODE_LINES, "VerticesMode enum error");
_Static_assert((int)R_VERTICES_MODE_LINE_LOOP == (int)CG_VERTICES_MODE_LINE_LOOP, "VerticesMode enum error");
_Static_assert((int)R_VERTICES_MODE_LINE_STRIP == (int)CG_VERTICES_MODE_LINE_STRIP, "VerticesMode enum error");
_Static_assert((int)R_VERTICES_MODE_TRIANGLES == (int)CG_VERTICES_MODE_TRIANGLES, "VerticesMode enum error");
_Static_assert((int)R_VERTICES_MODE_TRIANGLE_STRIP == (int)CG_VERTICES_MODE_TRIANGLE_STRIP, "VerticesMode enum error");
_Static_assert((int)R_VERTICES_MODE_TRIANGLE_FAN == (int)CG_VERTICES_MODE_TRIANGLE_FAN, "VerticesMode enum error");


_Static_assert((int)R_INDICES_TYPE_UINT8 == (int)CG_INDICES_TYPE_UNSIGNED_BYTE, "IndicesType enum error");
_Static_assert((int)R_INDICES_TYPE_UINT16 == (int)CG_INDICES_TYPE_UNSIGNED_SHORT, "IndicesType enum error");
_Static_assert((int)R_INDICES_TYPE_UINT32 == (int)CG_INDICES_TYPE_UNSIGNED_INT, "IndicesType enum error");

RObject *
r_buffer_new(RModule *module, size_t bytes)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rut_property_context_t *prop_ctx = engine->property_ctx;
    rut_buffer_t *buf;

    prop_ctx->logging_disabled++;
    buf = rut_buffer_new(bytes);
    prop_ctx->logging_disabled--;

    rut_object_claim(buf, engine);
    rut_object_unref(buf);

    rig_engine_op_add_buffer(engine, buf);

    return (RObject *)buf;
}

void
r_buffer_set_data(RModule *module,
                  RObject *buffer,
                  int offset,
                  void *data,
                  int data_len)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;

    rig_engine_op_buffer_set_data(engine,
                                  (rut_buffer_t *)buffer,
                                  offset, data, data_len);
}

RObject *
r_attribute_new(RModule *module,
                RObject *buffer,
                const char *name,
                size_t stride,
                size_t offset,
                int n_components,
                RAttributeType type)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rut_property_context_t *prop_ctx = engine->property_ctx;
    rut_attribute_t *attribute;

    prop_ctx->logging_disabled++;
    attribute = rut_attribute_new((rut_buffer_t *)buffer,
                                  name, stride, offset, n_components, type);
    prop_ctx->logging_disabled--;

    rut_object_claim(attribute, engine);
    rut_object_unref(attribute);

    /* XXX: note there are no associated OPs for attributes (they are
     * just serialized as part of a mesh).
     * Consider how applications are meant to unref attributes.
     * Consider how attributes are cleaned up if reaping a UI.
     */
#warning "r_attribute_new(): only return a temporary reference perhaps?"

    return (RObject *)attribute;
}

static RObject *
_r_attribute_new_const(RModule *module,
                       const char *name,
                       int n_components,
                       int n_columns,
                       bool transpose,
                       const float *value)
{
    return (RObject *)rut_attribute_new_const(name,
                                              n_components,
                                              n_columns,
                                              transpose,
                                              value);
}

RObject *
r_attribute_new_const(RModule *module,
                      const char *name,
                      int n_components,
                      int n_columns,
                      bool transpose,
                      const float *value)
{
    return _r_attribute_new_const(module, name, n_components, n_columns,
                                  transpose, value);
}

RObject *
r_attribute_new_const_1f(RModule *module, const char *name, float value)
{
    return _r_attribute_new_const(module,
                                  name,
                                  1, /* n_components */
                                  1, /* 1 column vector */
                                  false, /* no transpose */
                                  &value);
}

RObject *
r_attribute_new_const_2fv(RModule *module,
                          const char *name,
                          const float *value)
{
    return _r_attribute_new_const(module,
                                  name,
                                  2, /* n_components */
                                  1, /* 1 column vector */
                                  false, /* no transpose */
                                  value);
}

RObject *
r_attribute_new_const_3fv(RModule *module,
                          const char *name,
                          const float *value)
{
    return _r_attribute_new_const(module,
                                  name,
                                  3, /* n_components */
                                  1, /* 1 column vector */
                                  false, /* no transpose */
                                  value);
}

RObject *
r_attribute_new_const_4fv(RModule *module,
                           const char *name,
                           const float *value)
{
    return _r_attribute_new_const(module,
                                   name,
                                   4, /* n_components */
                                   1, /* 1 column vector */
                                   false, /* no transpose */
                                   value);
}

RObject *
r_attribute_new_const_2f(RModule *module,
                          const char *name,
                          float component0,
                          float component1)
{
    float vec2[2] = { component0, component1 };
    return _r_attribute_new_const(module,
                                  name,
                                  2, /* n_components */
                                  1, /* 1 column vector */
                                  false, /* no transpose */
                                  vec2);
}

RObject *
r_attribute_new_const_3f(RModule *module,
                          const char *name,
                          float component0,
                          float component1,
                          float component2)
{
    float vec3[3] = { component0, component1, component2 };
    return _r_attribute_new_const(module,
                                  name,
                                  3, /* n_components */
                                  1, /* 1 column vector */
                                  false, /* no transpose */
                                  vec3);
}

RObject *
r_attribute_new_const_4f(RModule *module,
                         const char *name,
                         float component0,
                         float component1,
                         float component2,
                         float component3)
{
    float vec4[4] = { component0, component1, component2, component3 };
    return _r_attribute_new_const(module,
                                  name,
                                  4, /* n_components */
                                  1, /* 1 column vector */
                                  false, /* no transpose */
                                  vec4);
}

RObject *
r_attribute_new_const_2x2fv(RModule *module,
                            const char *name,
                            const float *matrix2x2,
                            bool transpose)
{
    return _r_attribute_new_const(module,
                                  name,
                                  2, /* n_components */
                                  2, /* 2 column vector */
                                  false, /* no transpose */
                                  matrix2x2);
}

RObject *
r_attribute_new_const_3x3fv(RModule *module,
                            const char *name,
                            const float *matrix3x3,
                            bool transpose)
{
    return _r_attribute_new_const(module,
                                  name,
                                  3, /* n_components */
                                  3, /* 3 column vector */
                                  false, /* no transpose */
                                  matrix3x3);
}

RObject *
r_attribute_new_const_4x4fv(RModule *module,
                            const char *name,
                            const float *matrix4x4,
                            bool transpose)
{
    return _r_attribute_new_const(module,
                                  name,
                                  4, /* n_components */
                                  4, /* 4 column vector */
                                  false, /* no transpose */
                                  matrix4x4);
}

bool
r_attribute_get_normalized(RObject *self)
{
    rut_attribute_t *attribute = (void *)self;

    return attribute->normalized;
}

void
r_attribute_set_normalized(RObject *self, bool normalized)
{
    rut_attribute_t *attribute = (void *)self;

    attribute->normalized = normalized;
}

void
r_attribute_set_instance_stride(RObject *self, int stride)
{
    rut_attribute_t *attribute = (void *)self;

    attribute->instance_stride = stride;
}

int
r_attribute_get_instance_stride(RObject *self)
{
    rut_attribute_t *attribute = (void *)self;

    return attribute->instance_stride;
}

RObject *
r_attribute_get_buffer(RObject *self)
{
    rut_attribute_t *attribute = (void *)self;

    c_return_val_if_fail(attribute->is_buffered, NULL);

#warning "should r_attribute_get_buffer() return a reference?"
    return (RObject *)attribute->buffered.buffer;
}

void
r_attribute_set_buffer(RObject *self,
                       RObject *attribute_buffer)
{
    rut_attribute_t *attribute = (void *)self;

    c_return_if_fail(attribute->is_buffered);

    rut_object_ref(attribute_buffer);

    rut_object_unref(attribute->buffered.buffer);
    attribute->buffered.buffer = (rut_buffer_t *)attribute_buffer;
}

void
r_attribute_unref(RObject *self)
{
    rut_attribute_t *attribute = (void *)self;

    rut_object_unref(attribute);

#warning "r_attribute_unref(): remove in favour of returning temp refs from _new()?"
}

int
r_attribute_get_n_components(RObject *self)
{
    rut_attribute_t *attribute = (void *)self;

    if (attribute->is_buffered)
        return attribute->buffered.n_components;
    else
        return attribute->constant.n_components;
}

RObject *
r_mesh_new(RModule *module)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;
    rut_property_context_t *prop_ctx = engine->property_ctx;
    rut_object_t *component;

    prop_ctx->logging_disabled++;
    component = rig_mesh_new(code_module->engine);
    prop_ctx->logging_disabled--;

    rut_object_claim(component, engine);
    rut_object_unref(component);

    rig_engine_op_register_component(engine, component);

    return (RObject *)component;
}

void
r_mesh_set_attributes(RModule *module,
                      RObject *mesh,
                      RObject **attributes,
                      int n_attributes)
{
    rig_code_module_props_t *code_module = (void *)module;
    rig_engine_t *engine = code_module->engine;

    rig_engine_op_mesh_set_attributes(engine,
                                      (rig_mesh_t *)mesh,
                                      (rut_attribute_t **)attributes,
                                      n_attributes);
}
