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

#ifndef __CG_PRIMITIVE_H__
#define __CG_PRIMITIVE_H__

/* We forward declare the cg_primitive_t type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _cg_primitive_t cg_primitive_t;

#include <cglib/cg-types.h>
#include <cglib/cg-attribute.h>
#include <cglib/cg-framebuffer.h>

CG_BEGIN_DECLS

/**
 * SECTION:cg-primitive
 * @short_description: Functions for creating, manipulating and drawing
 *    primitives
 *
 * FIXME
 */

/**
 * cg_vertex_p2_t:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 *
 * A convenience vertex definition that can be used with
 * cg_primitive_new_p2().
 *
 * Stability: Unstable
 */
typedef struct {
    float x, y;
} cg_vertex_p2_t;

/**
 * cg_vertex_p3_t:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @z: The z component of a position attribute
 *
 * A convenience vertex definition that can be used with
 * cg_primitive_new_p3().
 *
 * Stability: Unstable
 */
typedef struct {
    float x, y, z;
} cg_vertex_p3_t;

/**
 * cg_vertex_p2c4_t:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @r: The red component of a color attribute
 * @b: The green component of a color attribute
 * @g: The blue component of a color attribute
 * @a: The alpha component of a color attribute
 *
 * A convenience vertex definition that can be used with
 * cg_primitive_new_p2c4().
 *
 * Stability: Unstable
 */
typedef struct {
    float x, y;
    uint8_t r, g, b, a;
} cg_vertex_p2c4_t;

/**
 * cg_vertex_p3c4_t:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @z: The z component of a position attribute
 * @r: The red component of a color attribute
 * @b: The green component of a color attribute
 * @g: The blue component of a color attribute
 * @a: The alpha component of a color attribute
 *
 * A convenience vertex definition that can be used with
 * cg_primitive_new_p3c4().
 *
 * Stability: Unstable
 */
typedef struct {
    float x, y, z;
    uint8_t r, g, b, a;
} cg_vertex_p3c4_t;

/**
 * cg_vertex_p2t2_t:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @s: The s component of a texture coordinate attribute
 * @t: The t component of a texture coordinate attribute
 *
 * A convenience vertex definition that can be used with
 * cg_primitive_new_p2t2().
 *
 * Stability: Unstable
 */
typedef struct {
    float x, y;
    float s, t;
} cg_vertex_p2t2_t;

/**
 * cg_vertex_p3t2_t:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @z: The z component of a position attribute
 * @s: The s component of a texture coordinate attribute
 * @t: The t component of a texture coordinate attribute
 *
 * A convenience vertex definition that can be used with
 * cg_primitive_new_p3t2().
 *
 * Stability: Unstable
 */
typedef struct {
    float x, y, z;
    float s, t;
} cg_vertex_p3t2_t;

/**
 * cg_vertex_p2t2c4_t:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @s: The s component of a texture coordinate attribute
 * @t: The t component of a texture coordinate attribute
 * @r: The red component of a color attribute
 * @b: The green component of a color attribute
 * @g: The blue component of a color attribute
 * @a: The alpha component of a color attribute
 *
 * A convenience vertex definition that can be used with
 * cg_primitive_new_p3t2c4().
 *
 * Stability: Unstable
 */
typedef struct {
    float x, y;
    float s, t;
    uint8_t r, g, b, a;
} cg_vertex_p2t2c4_t;

/**
 * cg_vertex_p3t2c4_t:
 * @x: The x component of a position attribute
 * @y: The y component of a position attribute
 * @z: The z component of a position attribute
 * @s: The s component of a texture coordinate attribute
 * @t: The t component of a texture coordinate attribute
 * @r: The red component of a color attribute
 * @b: The green component of a color attribute
 * @g: The blue component of a color attribute
 * @a: The alpha component of a color attribute
 *
 * A convenience vertex definition that can be used with
 * cg_primitive_new_p3t2c4().
 *
 * Stability: Unstable
 */
typedef struct {
    float x, y, z;
    float s, t;
    uint8_t r, g, b, a;
} cg_vertex_p3t2c4_t;

/**
 * cg_primitive_new:
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @...: A %NULL terminated list of attributes
 *
 * Combines a set of #cg_attribute_t<!-- -->s with a specific draw @mode
 * and defines a vertex count so a #cg_primitive_t object can be retained and
 * drawn later with no addition information required.
 *
 * The value passed as @n_vertices will simply update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t object
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new(cg_vertices_mode_t mode, int n_vertices, ...);

/**
 * cg_primitive_new_with_attributes:
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to process when drawing
 * @attributes: An array of cg_attribute_t
 * @n_attributes: The number of attributes
 *
 * Combines a set of #cg_attribute_t<!-- -->s with a specific draw @mode
 * and defines a vertex count so a #cg_primitive_t object can be retained and
 * drawn later with no addition information required.
 *
 * The value passed as @n_vertices will simply update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t object
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new_with_attributes(cg_vertices_mode_t mode,
                                                 int n_vertices,
                                                 cg_attribute_t **attributes,
                                                 int n_attributes);

/**
 * cg_primitive_new_p2:
 * @dev: A #cg_device_t
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.

 * @data: (array length=n_vertices): (type CGlib.VertexP2): An array
 *        of #cg_vertex_p2_t vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #cg_attribute_buffer_t storage, describe the position
 * attribute with a #cg_attribute_t and upload your data.
 *
 * For example to draw a convex polygon you can do:
 * |[
 * cg_vertex_p2_t triangle[] =
 * {
 *   { 0,   300 },
 *   { 150, 0,  },
 *   { 300, 300 }
 * };
 * prim = cg_primitive_new_p2 (CG_VERTICES_MODE_TRIANGLE_FAN,
 *                               3, triangle);
 * cg_primitive_draw (prim, fb, pipeline);
 * ]|
 *
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * <note>The primitive API doesn't support drawing with high-level
 * meta texture types such as #cg_texture_2d_sliced_t or
 * #cg_atlas_texture_t so you need to ensure that only low-level
 * textures that can be directly sampled by a GPU such as
 * #cg_texture_2d_t or #cg_texture_3d_t are associated with the layers of
 * any pipeline used while drawing a primitive.</note>
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t
 * with a reference of 1. This can be freed using cg_object_unref().
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new_p2(cg_device_t *dev,
                                    cg_vertices_mode_t mode,
                                    int n_vertices,
                                    const cg_vertex_p2_t *data);

/**
 * cg_primitive_new_p3:
 * @dev: A #cg_device_t
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type CGlib.VertexP3): An array of
 *        #cg_vertex_p3_t vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #cg_attribute_buffer_t storage, describe the position
 * attribute with a #cg_attribute_t and upload your data.
 *
 * For example to draw a convex polygon you can do:
 * |[
 * cg_vertex_p3_t triangle[] =
 * {
 *   { 0,   300, 0 },
 *   { 150, 0,   0 },
 *   { 300, 300, 0 }
 * };
 * prim = cg_primitive_new_p3 (CG_VERTICES_MODE_TRIANGLE_FAN,
 *                               3, triangle);
 * cg_primitive_draw (prim, fb, pipeline);
 * ]|
 *
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * <note>The primitive API doesn't support drawing with high-level
 * meta texture types such as #cg_texture_2d_sliced_t or
 * #cg_atlas_texture_t so you need to ensure that only low-level
 * textures that can be directly sampled by a GPU such as
 * #cg_texture_2d_t or #cg_texture_3d_t are associated with the layers of
 * any pipeline used while drawing a primitive.</note>
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t
 * with a reference of 1. This can be freed using cg_object_unref().
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new_p3(cg_device_t *dev,
                                    cg_vertices_mode_t mode,
                                    int n_vertices,
                                    const cg_vertex_p3_t *data);

/**
 * cg_primitive_new_p2c4:
 * @dev: A #cg_device_t
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type CGlib.VertexP2C4): An array
 *        of #cg_vertex_p2c4_t vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #cg_attribute_buffer_t storage, describe the position
 * and color attributes with #cg_attribute_t<!-- -->s and upload
 * your data.
 *
 * For example to draw a convex polygon with a linear gradient you
 * can do:
 * |[
 * cg_vertex_p2c4_t triangle[] =
 * {
 *   { 0,   300,  0xff, 0x00, 0x00, 0xff },
 *   { 150, 0,    0x00, 0xff, 0x00, 0xff },
 *   { 300, 300,  0xff, 0x00, 0x00, 0xff }
 * };
 * prim = cg_primitive_new_p2c4 (CG_VERTICES_MODE_TRIANGLE_FAN,
 *                                 3, triangle);
 * cg_primitive_draw (prim, fb, pipeline);
 * ]|
 *
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * <note>The primitive API doesn't support drawing with high-level
 * meta texture types such as #cg_texture_2d_sliced_t or
 * #cg_atlas_texture_t so you need to ensure that only low-level
 * textures that can be directly sampled by a GPU such as
 * #cg_texture_2d_t or #cg_texture_3d_t are associated with the layers of
 * any pipeline used while drawing a primitive.</note>
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t
 * with a reference of 1. This can be freed using cg_object_unref().
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new_p2c4(cg_device_t *dev,
                                      cg_vertices_mode_t mode,
                                      int n_vertices,
                                      const cg_vertex_p2c4_t *data);

/**
 * cg_primitive_new_p3c4:
 * @dev: A #cg_device_t
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type CGlib.VertexP3C4): An array
 *        of #cg_vertex_p3c4_t vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #cg_attribute_buffer_t storage, describe the position
 * and color attributes with #cg_attribute_t<!-- -->s and upload
 * your data.
 *
 * For example to draw a convex polygon with a linear gradient you
 * can do:
 * |[
 * cg_vertex_p3c4_t triangle[] =
 * {
 *   { 0,   300, 0,  0xff, 0x00, 0x00, 0xff },
 *   { 150, 0,   0,  0x00, 0xff, 0x00, 0xff },
 *   { 300, 300, 0,  0xff, 0x00, 0x00, 0xff }
 * };
 * prim = cg_primitive_new_p3c4 (CG_VERTICES_MODE_TRIANGLE_FAN,
 *                                 3, triangle);
 * cg_primitive_draw (prim, fb, pipeline);
 * ]|
 *
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * <note>The primitive API doesn't support drawing with high-level
 * meta texture types such as #cg_texture_2d_sliced_t or
 * #cg_atlas_texture_t so you need to ensure that only low-level
 * textures that can be directly sampled by a GPU such as
 * #cg_texture_2d_t or #cg_texture_3d_t are associated with the layers of
 * any pipeline used while drawing a primitive.</note>
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t
 * with a reference of 1. This can be freed using cg_object_unref().
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new_p3c4(cg_device_t *dev,
                                      cg_vertices_mode_t mode,
                                      int n_vertices,
                                      const cg_vertex_p3c4_t *data);

/**
 * cg_primitive_new_p2t2:
 * @dev: A #cg_device_t
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type CGlib.VertexP2T2): An array
 *        of #cg_vertex_p2t2_t vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #cg_attribute_buffer_t storage, describe the position and
 * texture coordinate attributes with #cg_attribute_t<!-- -->s and
 * upload your data.
 *
 * For example to draw a convex polygon with texture mapping you can
 * do:
 * |[
 * cg_vertex_p2t2_t triangle[] =
 * {
 *   { 0,   300,  0.0, 1.0},
 *   { 150, 0,    0.5, 0.0},
 *   { 300, 300,  1.0, 1.0}
 * };
 * prim = cg_primitive_new_p2t2 (CG_VERTICES_MODE_TRIANGLE_FAN,
 *                                 3, triangle);
 * cg_primitive_draw (prim, fb, pipeline);
 * ]|
 *
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * <note>The primitive API doesn't support drawing with high-level
 * meta texture types such as #cg_texture_2d_sliced_t or
 * #cg_atlas_texture_t so you need to ensure that only low-level
 * textures that can be directly sampled by a GPU such as
 * #cg_texture_2d_t or #cg_texture_3d_t are associated with the layers of
 * any pipeline used while drawing a primitive.</note>
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t
 * with a reference of 1. This can be freed using cg_object_unref().
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new_p2t2(cg_device_t *dev,
                                      cg_vertices_mode_t mode,
                                      int n_vertices,
                                      const cg_vertex_p2t2_t *data);

/**
 * cg_primitive_new_p3t2:
 * @dev: A #cg_device_t
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type CGlib.VertexP3T2): An array
 *        of #cg_vertex_p3t2_t vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #cg_attribute_buffer_t storage, describe the position and
 * texture coordinate attributes with #cg_attribute_t<!-- -->s and
 * upload your data.
 *
 * For example to draw a convex polygon with texture mapping you can
 * do:
 * |[
 * cg_vertex_p3t2_t triangle[] =
 * {
 *   { 0,   300, 0,  0.0, 1.0},
 *   { 150, 0,   0,  0.5, 0.0},
 *   { 300, 300, 0,  1.0, 1.0}
 * };
 * prim = cg_primitive_new_p3t2 (CG_VERTICES_MODE_TRIANGLE_FAN,
 *                                 3, triangle);
 * cg_primitive_draw (prim, fb, pipeline);
 * ]|
 *
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * <note>The primitive API doesn't support drawing with high-level
 * meta texture types such as #cg_texture_2d_sliced_t or
 * #cg_atlas_texture_t so you need to ensure that only low-level
 * textures that can be directly sampled by a GPU such as
 * #cg_texture_2d_t or #cg_texture_3d_t are associated with the layers of
 * any pipeline used while drawing a primitive.</note>
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t
 * with a reference of 1. This can be freed using cg_object_unref().
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new_p3t2(cg_device_t *dev,
                                      cg_vertices_mode_t mode,
                                      int n_vertices,
                                      const cg_vertex_p3t2_t *data);

/**
 * cg_primitive_new_p2t2c4:
 * @dev: A #cg_device_t
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type CGlib.VertexP2T2C4): An
 *        array of #cg_vertex_p2t2c4_t vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #cg_attribute_buffer_t storage, describe the position, texture
 * coordinate and color attributes with #cg_attribute_t<!-- -->s and
 * upload your data.
 *
 * For example to draw a convex polygon with texture mapping and a
 * linear gradient you can do:
 * |[
 * cg_vertex_p2t2c4_t triangle[] =
 * {
 *   { 0,   300,  0.0, 1.0,  0xff, 0x00, 0x00, 0xff},
 *   { 150, 0,    0.5, 0.0,  0x00, 0xff, 0x00, 0xff},
 *   { 300, 300,  1.0, 1.0,  0xff, 0x00, 0x00, 0xff}
 * };
 * prim = cg_primitive_new_p2t2c4 (CG_VERTICES_MODE_TRIANGLE_FAN,
 *                                   3, triangle);
 * cg_primitive_draw (prim, fb, pipeline);
 * ]|
 *
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * <note>The primitive API doesn't support drawing with high-level
 * meta texture types such as #cg_texture_2d_sliced_t or
 * #cg_atlas_texture_t so you need to ensure that only low-level
 * textures that can be directly sampled by a GPU such as
 * #cg_texture_2d_t or #cg_texture_3d_t are associated with the layers of
 * any pipeline used while drawing a primitive.</note>
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t
 * with a reference of 1. This can be freed using cg_object_unref().
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new_p2t2c4(cg_device_t *dev,
                                        cg_vertices_mode_t mode,
                                        int n_vertices,
                                        const cg_vertex_p2t2c4_t *data);

/**
 * cg_primitive_new_p3t2c4:
 * @dev: A #cg_device_t
 * @mode: A #cg_vertices_mode_t defining how to draw the vertices
 * @n_vertices: The number of vertices to read from @data and also
 *              the number of vertices to read when later drawing.
 * @data: (array length=n_vertices): (type CGlib.VertexP3T2C4): An
 *        array of #cg_vertex_p3t2c4_t vertices
 *
 * Provides a convenient way to describe a primitive, such as a single
 * triangle strip or a triangle fan, that will internally allocate the
 * necessary #cg_attribute_buffer_t storage, describe the position, texture
 * coordinate and color attributes with #cg_attribute_t<!-- -->s and
 * upload your data.
 *
 * For example to draw a convex polygon with texture mapping and a
 * linear gradient you can do:
 * |[
 * cg_vertex_p3t2c4_t triangle[] =
 * {
 *   { 0,   300, 0,  0.0, 1.0,  0xff, 0x00, 0x00, 0xff},
 *   { 150, 0,   0,  0.5, 0.0,  0x00, 0xff, 0x00, 0xff},
 *   { 300, 300, 0,  1.0, 1.0,  0xff, 0x00, 0x00, 0xff}
 * };
 * prim = cg_primitive_new_p3t2c4 (CG_VERTICES_MODE_TRIANGLE_FAN,
 *                                   3, triangle);
 * cg_primitive_draw (prim, fb, pipeline);
 * ]|
 *
 * The value passed as @n_vertices is initially used to determine how
 * much can be read from @data but it will also be used to update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to read when drawing.
 *
 * <note>The primitive API doesn't support drawing with high-level
 * meta texture types such as #cg_texture_2d_sliced_t or
 * #cg_atlas_texture_t so you need to ensure that only low-level
 * textures that can be directly sampled by a GPU such as
 * #cg_texture_2d_t or #cg_texture_3d_t are associated with the layers of
 * any pipeline used while drawing a primitive.</note>
 *
 * Return value: (transfer full): A newly allocated #cg_primitive_t
 * with a reference of 1. This can be freed using cg_object_unref().
 *
 * Stability: Unstable
 */
cg_primitive_t *cg_primitive_new_p3t2c4(cg_device_t *dev,
                                        cg_vertices_mode_t mode,
                                        int n_vertices,
                                        const cg_vertex_p3t2c4_t *data);
int cg_primitive_get_first_vertex(cg_primitive_t *primitive);

void cg_primitive_set_first_vertex(cg_primitive_t *primitive, int first_vertex);

/**
 * cg_primitive_get_n_vertices:
 * @primitive: A #cg_primitive_t object
 *
 * Queries the number of vertices to read when drawing the given
 * @primitive. Usually this value is implicitly set when associating
 * vertex data or indices with a #cg_primitive_t.
 *
 * If cg_primitive_set_indices() has been used to associate a
 * sequence of #cg_indices_t with the given @primitive then the
 * number of vertices to read can also be phrased as the number
 * of indices to read.
 *
 * <note>To be clear; it doesn't refer to the number of vertices - in
 * terms of data - associated with the primitive it's just the number
 * of vertices to read and draw.</note>
 *
 * Returns: The number of vertices to read when drawing.
 *
 * Stability: unstable
 */
int cg_primitive_get_n_vertices(cg_primitive_t *primitive);

/**
 * cg_primitive_set_n_vertices:
 * @primitive: A #cg_primitive_t object
 * @n_vertices: The number of vertices to read when drawing.
 *
 * Specifies how many vertices should be read when drawing the given
 * @primitive.
 *
 * Usually this value is set implicitly when associating vertex data
 * or indices with a #cg_primitive_t.
 *
 * <note>To be clear; it doesn't refer to the number of vertices - in
 * terms of data - associated with the primitive it's just the number
 * of vertices to read and draw.</note>
 *
 * Stability: unstable
 */
void cg_primitive_set_n_vertices(cg_primitive_t *primitive, int n_vertices);

cg_vertices_mode_t cg_primitive_get_mode(cg_primitive_t *primitive);

void cg_primitive_set_mode(cg_primitive_t *primitive, cg_vertices_mode_t mode);

/**
 * cg_primitive_set_attributes:
 * @primitive: A #cg_primitive_t object
 * @attributes: an array of #cg_attribute_t pointers
 * @n_attributes: the number of elements in @attributes
 *
 * Replaces all the attributes of the given #cg_primitive_t object.
 *
 * Stability: Unstable
 */
void cg_primitive_set_attributes(cg_primitive_t *primitive,
                                 cg_attribute_t **attributes,
                                 int n_attributes);

/**
 * cg_primitive_set_indices:
 * @primitive: A #cg_primitive_t
 * @indices: A #cg_indices_t array
 * @n_indices: The number of indices to reference when drawing
 *
 * Associates a sequence of #cg_indices_t with the given @primitive.
 *
 * #cg_indices_t provide a way to virtualize your real vertex data by
 * providing a sequence of indices that index into your real vertex
 * data. The GPU will walk though the index values to indirectly
 * lookup the data for each vertex instead of sequentially walking
 * through the data directly. This lets you save memory by indexing
 * shared data multiple times instead of duplicating the data.
 *
 * The value passed as @n_indices will simply update the
 * #cg_primitive_t <structfield>n_vertices</structfield> property as if
 * cg_primitive_set_n_vertices() were called. This property defines
 * the number of vertices to draw or, put another way, how many
 * indices should be read from @indices when drawing.
 *
 * <note>The #cg_primitive_t <structfield>first_vertex</structfield> property
 * also affects drawing with indices by defining the first entry of the
 * indices to start drawing from.</note>
 *
 * Stability: unstable
 */
void cg_primitive_set_indices(cg_primitive_t *primitive,
                              cg_indices_t *indices,
                              int n_indices);

/**
 * cg_primitive_get_indices:
 * @primitive: A #cg_primitive_t
 *
 * Return value: (transfer none): the indices that were set with
 * cg_primitive_set_indices() or %NULL if no indices were set.
 *
 * Stability: unstable
 */
cg_indices_t *cg_primitive_get_indices(cg_primitive_t *primitive);

/**
 * cg_primitive_copy:
 * @primitive: A primitive copy
 *
 * Makes a copy of an existing #cg_primitive_t. Note that the primitive
 * is a shallow copy which means it will use the same attributes and
 * attribute buffers as the original primitive.
 *
 * Return value: (transfer full): the new primitive
 * Stability: unstable
 */
cg_primitive_t *cg_primitive_copy(cg_primitive_t *primitive);

/**
 * cg_is_primitive:
 * @object: A #cg_object_t
 *
 * Gets whether the given object references a #cg_primitive_t.
 *
 * Returns: %true if the @object references a #cg_primitive_t,
 *   %false otherwise
 *
 * Stability: Unstable
 */
bool cg_is_primitive(void *object);

/**
 * cg_primitive_attribute_callback_t:
 * @primitive: The #cg_primitive_t whose attributes are being iterated
 * @attribute: The #cg_attribute_t
 * @user_data: The private data passed to cg_primitive_foreach_attribute()
 *
 * The callback prototype used with cg_primitive_foreach_attribute()
 * for iterating all the attributes of a #cg_primitive_t.
 *
 * The function should return true to continue iteration or false to
 * stop.
 *
 * Stability: Unstable
 */
typedef bool (*cg_primitive_attribute_callback_t)(cg_primitive_t *primitive,
                                                  cg_attribute_t *attribute,
                                                  void *user_data);

/**
 * cg_primitive_foreach_attribute:
 * @primitive: A #cg_primitive_t object
 * @callback: (scope call): A #cg_primitive_attribute_callback_t to be
 *            called for each attribute
 * @user_data: (closure): Private data that will be passed to the
 *             callback
 *
 * Iterates all the attributes of the given #cg_primitive_t.
 *
 * Stability: Unstable
 */
void cg_primitive_foreach_attribute(cg_primitive_t *primitive,
                                    cg_primitive_attribute_callback_t callback,
                                    void *user_data);

/**
 * cg_primitive_draw:
 * @primitive: A #cg_primitive_t geometry object
 * @framebuffer: A destination #cg_framebuffer_t
 * @pipeline: A #cg_pipeline_t state object
 *
 * Draws the given @primitive geometry to the specified destination
 * @framebuffer using the graphics processing state described by @pipeline.
 *
 * This drawing api doesn't support high-level meta texture types such
 * as #cg_texture_2d_sliced_t so it is the user's responsibility to
 * ensure that only low-level textures that can be directly sampled by
 * a GPU such as #cg_texture_2d_t or #cg_texture_3d_t are associated with
 * layers of the given @pipeline.
 *
 * Stability: unstable
 */
void cg_primitive_draw(cg_primitive_t *primitive,
                       cg_framebuffer_t *framebuffer,
                       cg_pipeline_t *pipeline);

/**
 * cg_primitive_draw_instances:
 * @primitive: A #cg_primitive_t geometry object
 * @framebuffer: A destination #cg_framebuffer_t
 * @pipeline: A #cg_pipeline_t state object
 * @n_instances: The number of instances of @primitive to draw
 *
 * Draws the given @primitive geometry to the specified destination
 * @framebuffer @n_instances times using the graphics processing state
 * described by @pipeline.
 *
 * This drawing api doesn't support high-level meta texture types such
 * as #cg_texture_2d_sliced_t so it is the user's responsibility to
 * ensure that only low-level textures that can be directly sampled by
 * a GPU such as #cg_texture_2d_t or #cg_texture_3d_t are associated with
 * layers of the given @pipeline.
 */
void cg_primitive_draw_instances(cg_primitive_t *primitive,
                                 cg_framebuffer_t *framebuffer,
                                 cg_pipeline_t *pipeline,
                                 int n_instances);

CG_END_DECLS

#endif /* __CG_PRIMITIVE_H__ */
