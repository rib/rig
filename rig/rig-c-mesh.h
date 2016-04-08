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

#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "rig-c.h"

typedef enum {
    R_ATTRIBUTE_TYPE_BYTE,
    R_ATTRIBUTE_TYPE_UNSIGNED_BYTE,
    R_ATTRIBUTE_TYPE_SHORT,
    R_ATTRIBUTE_TYPE_UNSIGNED_SHORT,
    R_ATTRIBUTE_TYPE_FLOAT
} RAttributeType;

RObject *
r_buffer_new(RModule *module, size_t bytes);

void r_buffer_set_data(RModule *module,
                       RObject *buffer,
                       int offset,
                       void *data,
                       int data_len);

/**
 * SECTION:cg-attribute
 * @short_description: Functions for declaring and drawing vertex
 *    attributes
 *
 * FIXME
 */

/**
 * r_attribute_new: (constructor)
 * @attribute_buffer: The #RObject containing the actual
 *                    attribute data
 * @name: The name of the attribute (used to reference it from GLSL)
 * @stride: The number of bytes to jump to get to the next attribute
 *          value for the next vertex. (Usually
 *          <literal>sizeof (MyVertex)</literal>)
 * @offset: The byte offset from the start of @attribute_buffer for
 *          the first attribute value. (Usually
 *          <literal>offsetof (MyVertex, component0)</literal>
 * @n_components: The number of components (e.g. 4 for an rgba color or
 *              3 for and (x,y,z) position)
 * @type: FIXME
 *
 * Describes the layout for a list of vertex attribute values (For
 * example, a list of texture coordinates or colors).
 *
 * The @name is used to access the attribute inside a GLSL vertex
 * shader and there are some special names you should use if they are
 * applicable:
 *  <itemizedlist>
 *    <listitem>"cg_position_in" (used for vertex positions)</listitem>
 *    <listitem>"cg_color_in" (used for vertex colors)</listitem>
 *    <listitem>"cg_tex_coord0_in", "cg_tex_coord1", ...
 * (used for vertex texture coordinates)</listitem>
 *    <listitem>"cg_normal_in" (used for vertex normals)</listitem>
 *    <listitem>"cg_point_size_in" (used to set the size of points
 *    per-vertex. Note this can only be used if
 *    %CG_FEATURE_ID_POINT_SIZE_ATTRIBUTE is advertised and
 *    cg_pipeline_set_per_vertex_point_size() is called on the pipeline.
 *    </listitem>
 *  </itemizedlist>
 *
 * The attribute values corresponding to different vertices can either
 * be tightly packed or interleaved with other attribute values. For
 * example it's common to define a structure for a single vertex like:
 * |[
 * typedef struct
 * {
 *   float x, y, z; /<!-- -->* position attribute *<!-- -->/
 *   float s, t; /<!-- -->* texture coordinate attribute *<!-- -->/
 * } MyVertex;
 * ]|
 *
 * And then create an array of vertex data something like:
 * |[
 * MyVertex vertices[100] = { .... }
 * ]|
 *
 * In this case, to describe either the position or texture coordinate
 * attribute you have to move <literal>sizeof (MyVertex)</literal> bytes to
 * move from one vertex to the next.  This is called the attribute
 * @stride. If you weren't interleving attributes and you instead had
 * a packed array of float x, y pairs then the attribute stride would
 * be <literal>(2 * sizeof (float))</literal>. So the @stride is the number of
 * bytes to move to find the attribute value of the next vertex.
 *
 * Normally a list of attributes starts at the beginning of an array.
 * So for the <literal>MyVertex</literal> example above the @offset is the
 * offset inside the <literal>MyVertex</literal> structure to the first
 * component of the attribute. For the texture coordinate attribute
 * the offset would be <literal>offsetof (MyVertex, s)</literal> or instead of
 * using the offsetof macro you could use <literal>sizeof (float) *
 * 3</literal>.  If you've divided your @array into blocks of non-interleved
 * attributes then you will need to calculate the @offset as the number of
 * bytes in blocks preceding the attribute you're describing.
 *
 * An attribute often has more than one component. For example a color
 * is often comprised of 4 red, green, blue and alpha @components, and a
 * position may be comprised of 2 x and y @components. You should aim
 * to keep the number of components to a minimum as more components
 * means more data needs to be mapped into the GPU which can be a
 * bottlneck when dealing with a large number of vertices.
 *
 * Finally you need to specify the component data type. Here you
 * should aim to use the smallest type that meets your precision
 * requirements. Again the larger the type then more data needs to be
 * mapped into the GPU which can be a bottlneck when dealing with
 * a large number of vertices.
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          describing the layout for a list of attribute values
 *          stored in @array.
 */
/* XXX: look for a precedent to see if the stride/offset args should
 * have a different order. */
RObject *r_attribute_new(RModule *module,
                         RObject *attribute_buffer,
                         const char *name,
                         size_t stride,
                         size_t offset,
                         int n_components,
                         RAttributeType type);


/**
 * r_attribute_new_const:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @n_components: The number of vector components for the attribute value
 * @n_columns: The number of column vectors for the attribute value
 * @value: The constant value for the attribute, in column-major order
 *
 * Creates a new attribute whose value remains constant across all the
 * vertices of a primitive without needing to duplicate the value for
 * each vertex.
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant @value.
 */
RObject *r_attribute_new_const(RModule *module,
                                  const char *name,
                                  int n_components,
                                  int n_columns,
                                  bool transpose,
                                  const float *value);

/**
 * r_attribute_new_const_1f:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @value: The constant value for the attribute
 *
 * Creates a new, single component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constant @value is a single precision floating point scalar
 * which should have a corresponding declaration in GLSL code like:
 *
 * [|
 * attribute float name;
 * |]
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant @value.
 */
RObject *
r_attribute_new_const_1f(RModule *module, const char *name, float value);

/**
 * r_attribute_new_const_2f:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @component0: The first component of a 2 component vector
 * @component1: The second component of a 2 component vector
 *
 * Creates a new, 2 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (@component0, @component1) represent a 2 component
 * float vector which should have a corresponding declaration in GLSL
 * code like:
 *
 * [|
 * in vec2 name;
 * |]
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant vector.
 */
RObject *r_attribute_new_const_2f(RModule *module,
                                     const char *name,
                                     float component0,
                                     float component1);

/**
 * r_attribute_new_const_3f:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @component0: The first component of a 3 component vector
 * @component1: The second component of a 3 component vector
 * @component2: The third component of a 3 component vector
 *
 * Creates a new, 3 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (@component0, @component1, @component2) represent a 3
 * component float vector which should have a corresponding
 * declaration in GLSL code like:
 *
 * [|
 * in vec3 name;
 * |]
 *
 * unless the built in name "cg_normal_in" is being used where no
 * explicit GLSL declaration need be made.
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant vector.
 */
RObject *r_attribute_new_const_3f(RModule *module,
                                     const char *name,
                                     float component0,
                                     float component1,
                                     float component2);

/**
 * r_attribute_new_const_4f:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @component0: The first component of a 4 component vector
 * @component1: The second component of a 4 component vector
 * @component2: The third component of a 4 component vector
 * @component3: The fourth component of a 4 component vector
 *
 * Creates a new, 4 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (@component0, @component1, @component2, @constant3)
 * represent a 4 component float vector which should have a
 * corresponding declaration in GLSL code like:
 *
 * [|
 * in vec4 name;
 * |]
 *
 * unless one of the built in names "cg_color_in",
 * "cg_tex_coord0_in or "cg_tex_coord1_in" etc is being used where
 * no explicit GLSL declaration need be made.
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant vector.
 */
RObject *r_attribute_new_const_4f(RModule *module,
                                     const char *name,
                                     float component0,
                                     float component1,
                                     float component2,
                                     float component3);

/**
 * r_attribute_new_const_2fv:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @value: A pointer to a 2 component float vector
 *
 * Creates a new, 2 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (value[0], value[1]) represent a 2 component float
 * vector which should have a corresponding declaration in GLSL code
 * like:
 *
 * [|
 * in vec2 name;
 * |]
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant vector.
 */
RObject *r_attribute_new_const_2fv(RModule *module,
                                      const char *name,
                                      const float *value);

/**
 * r_attribute_new_const_3fv:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @value: A pointer to a 3 component float vector
 *
 * Creates a new, 3 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (value[0], value[1], value[2]) represent a 3
 * component float vector which should have a corresponding
 * declaration in GLSL code like:
 *
 * [|
 * in vec3 name;
 * |]
 *
 * unless the built in name "cg_normal_in" is being used where no
 * explicit GLSL declaration need be made.
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant vector.
 */
RObject *r_attribute_new_const_3fv(RModule *module,
                                      const char *name,
                                      const float *value);

/**
 * r_attribute_new_const_4fv:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @value: A pointer to a 4 component float vector
 *
 * Creates a new, 4 component, attribute whose value remains
 * constant across all the vertices of a primitive without needing to
 * duplicate the value for each vertex.
 *
 * The constants (value[0], value[1], value[2], value[3]) represent a
 * 4 component float vector which should have a corresponding
 * declaration in GLSL code like:
 *
 * [|
 * in vec4 name;
 * |]
 *
 * unless one of the built in names "cg_color_in",
 * "cg_tex_coord0_in or "cg_tex_coord1_in" etc is being used where
 * no explicit GLSL declaration need be made.
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant vector.
 */
RObject *r_attribute_new_const_4fv(RModule *module,
                                      const char *name,
                                      const float *value);

/**
 * r_attribute_new_const_2x2fv:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @matrix2x2: A pointer to a 2 by 2 matrix
 * @transpose: Whether the matrix should be transposed on upload or
 *             not
 *
 * Creates a new matrix attribute whose value remains constant
 * across all the vertices of a primitive without needing to duplicate
 * the value for each vertex.
 *
 * @matrix2x2 represent a square 2 by 2 matrix specified in
 * column-major order (each pair of consecutive numbers represents a
 * column) which should have a corresponding declaration in GLSL code
 * like:
 *
 * [|
 * attribute mat2 name;
 * |]
 *
 * If @transpose is %true then all matrix components are rotated
 * around the diagonal of the matrix such that the first column
 * becomes the first row and the second column becomes the second row.
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant matrix.
 */
RObject *r_attribute_new_const_2x2fv(RModule *module,
                                        const char *name,
                                        const float *matrix2x2,
                                        bool transpose);

/**
 * r_attribute_new_const_3x3fv:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @matrix3x3: A pointer to a 3 by 3 matrix
 * @transpose: Whether the matrix should be transposed on upload or
 *             not
 *
 * Creates a new matrix attribute whose value remains constant
 * across all the vertices of a primitive without needing to duplicate
 * the value for each vertex.
 *
 * @matrix3x3 represent a square 3 by 3 matrix specified in
 * column-major order (each triple of consecutive numbers represents a
 * column) which should have a corresponding declaration in GLSL code
 * like:
 *
 * [|
 * attribute mat3 name;
 * |]
 *
 * If @transpose is %true then all matrix components are rotated
 * around the diagonal of the matrix such that the first column
 * becomes the first row and the second column becomes the second row
 * etc.
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant matrix.
 */
RObject *r_attribute_new_const_3x3fv(RModule *module,
                                        const char *name,
                                        const float *matrix3x3,
                                        bool transpose);

/**
 * r_attribute_new_const_4x4fv:
 * @module: an #RModule pointer
 * @name: The name of the attribute (used to reference it from GLSL)
 * @matrix4x4: A pointer to a 4 by 4 matrix
 * @transpose: Whether the matrix should be transposed on upload or
 *             not
 *
 * Creates a new matrix attribute whose value remains constant
 * across all the vertices of a primitive without needing to duplicate
 * the value for each vertex.
 *
 * @matrix4x4 represent a square 4 by 4 matrix specified in
 * column-major order (each 4-tuple of consecutive numbers represents a
 * column) which should have a corresponding declaration in GLSL code
 * like:
 *
 * [|
 * attribute mat4 name;
 * |]
 *
 * If @transpose is %true then all matrix components are rotated
 * around the diagonal of the matrix such that the first column
 * becomes the first row and the second column becomes the second row
 * etc.
 *
 * Return value: (transfer full): A newly allocated #RObject
 *          representing the given constant matrix.
 */
RObject *r_attribute_new_const_4x4fv(RModule *module,
                                        const char *name,
                                        const float *matrix4x4,
                                        bool transpose);

/**
 * r_attribute_set_normalized:
 * @attribute: A #RObject
 * @normalized: The new value for the normalized property.
 *
 * Sets whether fixed point attribute types are mapped to the range
 * 0→1. For example when this property is true and a
 * %CG_ATTRIBUTE_TYPE_UNSIGNED_BYTE type is used then the value 255
 * will be mapped to 1.0.
 *
 * The default value of this property depends on the name of the
 * attribute. For the builtin properties cg_color_in and
 * cg_normal_in it will default to true and for all other names it
 * will default to false.
 *
 * Stability: unstable
 */
void r_attribute_set_normalized(RObject *attribute, bool normalized);

/**
 * r_attribute_get_normalized:
 * @attribute: A #RObject
 *
 * Return value: the value of the normalized property set with
 * r_attribute_set_normalized().
 *
 * Stability: unstable
 */
bool r_attribute_get_normalized(RObject *attribute);


/**
 * r_attribute_set_instance_stride:
 * @attribute: A #RObject
 * @instance_stride: Number of instances the gpu should process before
 *                   progressing to the next attribute value.
 *
 * Determines whether the gpu should step through the data associated with
 * the given @attribute on a per vertex (default) or per instance basis.
 *
 * By default the @instance_stride is %0 and the gpu progresses through
 * @attribute values on a per vertex basis.
 *
 * An instance may be a primitive comprised of multiple triangles. For example
 * if an application represents a rectangle as %2 triangles (%6 vertices) and
 * wants to use a different transformation matrix for thousands of rectangles
 * then this api enables it to upload a matrix for each rectangle and tell the
 * gpu to only progress to the next matrix after @instance_stride rectangles.
 *
 * The number of vertices that belong to a single instance (%6 in this
 * example) is defined by the number of vertices belonging to the
 * #cg_primitive_t being drawn via cg_primitive_draw(). Multiple instances can
 * be drawn via cg_primitive_draw_instances().
 *
 * The default value of this property is %0 which means that the gpu progress
 * though the @attribute data on a per-vertex basis instead of a per-instance
 * basis.
 */
void r_attribute_set_instance_stride(RObject *attribute, int stride);


/**
 * r_attribute_get_instance_stride:
 * @attribute: A #RObject
 *
 * Returns how many instances the gpu would process before progressing to the
 * next @attribute value, as determined by calling
 * r_attribute_set_instance_stride().
 *
 * The default value of this property is %0 which means that the gpu progress
 * though the @attribute data on a per-vertex basis instead of a per-instance
 * basis.
 *
 * Return value: The number of instances for the gpu to process before it
 *               should progress to the next @attribute value.
 */
int r_attribute_get_instance_stride(RObject *attribute);

/**
 * r_attribute_get_buffer:
 * @attribute: A #RObject
 *
 * Return value: (transfer none): the #RObject that was
 *        set with r_attribute_set_buffer() or r_attribute_new().
 *
 * Stability: unstable
 */
RObject *r_attribute_get_buffer(RObject *attribute);

/**
 * r_attribute_set_buffer:
 * @attribute: A #RObject
 * @attribute_buffer: A #RObject
 *
 * Sets a new #RObject for the attribute.
 *
 * Stability: unstable
 */
void r_attribute_set_buffer(RObject *attribute,
                            RObject *attribute_buffer);

RObject *r_mesh_new(RModule *module);

void r_mesh_set_attributes(RModule *module,
                           RObject *mesh,
                           RObject **attributes,
                           int n_attributes);

typedef enum {
    R_VERTICES_MODE_POINTS,
    R_VERTICES_MODE_LINES,
    R_VERTICES_MODE_LINE_LOOP,
    R_VERTICES_MODE_LINE_STRIP,
    R_VERTICES_MODE_TRIANGLES,
    R_VERTICES_MODE_TRIANGLE_STRIP,
    R_VERTICES_MODE_TRIANGLE_FAN,
} RVerticesMode;

typedef enum {
    R_INDICES_TYPE_UINT8,
    R_INDICES_TYPE_UINT16,
    R_INDICES_TYPE_UINT32
} RIndicesType;
