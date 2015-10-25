/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */

#include <rut-config.h>

#include <clib.h>
#include <math.h>

#include <cglib/cglib.h>

#include "rut-shell.h"
#include "rut-geometry.h"
#include "rut-interfaces.h"

cg_attribute_t *
rut_create_circle_fan_p2(rut_shell_t *shell, int subdivisions,
                         int *n_verts_ret)
{
    int n_verts = subdivisions + 2;
    struct CircleVert {
        float x, y;
    } *verts;
    size_t buffer_size = sizeof(struct CircleVert) * n_verts;
    int i;
    cg_attribute_buffer_t *attribute_buffer;
    cg_attribute_t *attribute;
    float angle_division = 2.0f * (float)C_PI * (1.0f / (float)subdivisions);

    verts = alloca(buffer_size);

    verts[0].x = 0;
    verts[0].y = 0;
    for (i = 0; i < subdivisions; i++) {
        float angle = angle_division * i;
        verts[i + 1].x = sinf(angle);
        verts[i + 1].y = cosf(angle);
    }
    verts[n_verts - 1].x = sinf(0);
    verts[n_verts - 1].y = cosf(0);

    *n_verts_ret = n_verts;

    attribute_buffer =
        cg_attribute_buffer_new(shell->cg_device, buffer_size, verts);

    attribute = cg_attribute_new(attribute_buffer,
                                 "cg_position_in",
                                 sizeof(struct CircleVert),
                                 offsetof(struct CircleVert, x),
                                 2,
                                 CG_ATTRIBUTE_TYPE_FLOAT);
    return attribute;
}

cg_primitive_t *
rut_create_circle_fan_primitive(rut_shell_t *shell,
                                int subdivisions)
{
    int n_verts;
    cg_attribute_t *attribute =
        rut_create_circle_fan_p2(shell, subdivisions, &n_verts);

    return cg_primitive_new_with_attributes(
        CG_VERTICES_MODE_TRIANGLE_FAN, n_verts, &attribute, 1);
}

rut_mesh_t *
rut_create_circle_outline_mesh(uint8_t n_vertices)
{
    rut_buffer_t *buffer =
        rut_buffer_new(n_vertices * sizeof(cg_vertex_p3c4_t));
    rut_mesh_t *mesh;

    rut_tesselate_circle_with_line_indices((cg_vertex_p3c4_t *)buffer->data,
                                           n_vertices,
                                           NULL, /* no indices required */
                                           0,
                                           RUT_AXIS_Z,
                                           255,
                                           255,
                                           255);

    mesh = rut_mesh_new_from_buffer_p3c4(
        CG_VERTICES_MODE_LINE_LOOP, n_vertices, buffer);

    rut_object_unref(buffer);

    return mesh;
}

cg_primitive_t *
rut_create_circle_outline_primitive(rut_shell_t *shell,
                                    uint8_t n_vertices)
{
    rut_mesh_t *mesh = rut_create_circle_outline_mesh(n_vertices);
    cg_primitive_t *primitive = rut_mesh_create_primitive(shell, mesh);
    rut_object_unref(mesh);
    return primitive;
}

cg_texture_t *
rut_create_circle_texture(rut_shell_t *shell,
                          int radius_texels,
                          int padding_texels)
{
    cg_primitive_t *circle;
    cg_texture_2d_t *tex2d;
    cg_offscreen_t *offscreen;
    cg_framebuffer_t *fb;
    cg_pipeline_t *white_pipeline;
    int half_size = radius_texels + padding_texels;
    int size = half_size * 2;

    tex2d = cg_texture_2d_new_with_size(shell->cg_device, size, size);
    offscreen = cg_offscreen_new_with_texture(tex2d);
    fb = offscreen;

    circle = rut_create_circle_fan_primitive(shell, 360);

    cg_framebuffer_clear4f(fb, CG_BUFFER_BIT_COLOR, 0, 0, 0, 0);

    cg_framebuffer_identity_matrix(fb);
    cg_framebuffer_orthographic(fb, 0, 0, size, size, -1, 100);

    cg_framebuffer_translate(fb, half_size, half_size, 0);
    cg_framebuffer_scale(fb, radius_texels, radius_texels, 1);

    white_pipeline = cg_pipeline_new(shell->cg_device);
    cg_pipeline_set_color4f(white_pipeline, 1, 1, 1, 1);

    cg_primitive_draw(circle, fb, white_pipeline);

    cg_object_unref(white_pipeline);
    cg_object_unref(circle);
    cg_object_unref(offscreen);

    return tex2d;
}

/*
 * tesselate_circle:
 * @axis: the axis around which the circle is centered
 */
void
rut_tesselate_circle_with_line_indices(cg_vertex_p3c4_t *buffer,
                                       uint8_t n_vertices,
                                       uint8_t *indices_data,
                                       int indices_base,
                                       rut_axis_t axis,
                                       uint8_t r,
                                       uint8_t g,
                                       uint8_t b)
{
    float angle, increment;
    cg_vertex_p3c4_t *vertex;
    uint8_t i;

    increment = 2 * C_PI / n_vertices;

    for (i = 0, angle = 0.f; i < n_vertices; i++, angle += increment) {
        vertex = &buffer[i];

        switch (axis) {
        case RUT_AXIS_X:
            vertex->x = 0.f;
            vertex->y = sinf(angle);
            vertex->z = cosf(angle);
            break;
        case RUT_AXIS_Y:
            vertex->x = sinf(angle);
            vertex->y = 0.f;
            vertex->z = cosf(angle);
            break;
        case RUT_AXIS_Z:
            vertex->x = cosf(angle);
            vertex->y = sinf(angle);
            vertex->z = 0.f;
            break;
        }

        vertex->r = r;
        vertex->g = g;
        vertex->b = b;
        vertex->a = 255;
    }

    if (indices_data) {
        for (i = indices_base; i < indices_base + n_vertices - 1; i++) {
            indices_data[i * 2] = i;
            indices_data[i * 2 + 1] = i + 1;
        }
        indices_data[i * 2] = i;
        indices_data[i * 2 + 1] = indices_base;
    }
}

rut_mesh_t *
rut_create_rotation_tool_mesh(uint8_t n_vertices)
{
    rut_mesh_t *mesh;
    rut_buffer_t *buffer;
    rut_buffer_t *indices_buffer;
    uint8_t *indices_data;
    int vertex_buffer_size;

    c_assert(n_vertices < 255 / 3);

    vertex_buffer_size = n_vertices * sizeof(cg_vertex_p3c4_t) * 3;
    buffer = rut_buffer_new(vertex_buffer_size);

    indices_buffer = rut_buffer_new(n_vertices * 2 * 3);
    indices_data = indices_buffer->data;

    rut_tesselate_circle_with_line_indices((cg_vertex_p3c4_t *)buffer->data,
                                           n_vertices,
                                           indices_data,
                                           0,
                                           RUT_AXIS_X,
                                           255,
                                           0,
                                           0);

    rut_tesselate_circle_with_line_indices((cg_vertex_p3c4_t *)buffer->data +
                                           n_vertices,
                                           n_vertices,
                                           indices_data,
                                           n_vertices,
                                           RUT_AXIS_Y,
                                           0,
                                           255,
                                           0);

    rut_tesselate_circle_with_line_indices((cg_vertex_p3c4_t *)buffer->data +
                                           2 * n_vertices,
                                           n_vertices,
                                           indices_data,
                                           n_vertices * 2,
                                           RUT_AXIS_Z,
                                           0,
                                           0,
                                           255);

    mesh = rut_mesh_new_from_buffer_p3c4(
        CG_VERTICES_MODE_LINES, n_vertices * 3, buffer);

    rut_object_unref(buffer);

    rut_mesh_set_indices(mesh,
                         CG_INDICES_TYPE_UNSIGNED_BYTE,
                         indices_buffer,
                         n_vertices * 2 * 3);

    rut_object_unref(indices_buffer);

    return mesh;
}

cg_primitive_t *
rut_create_rotation_tool_primitive(rut_shell_t *shell,
                                   uint8_t n_vertices)
{
    rut_mesh_t *mesh = rut_create_rotation_tool_mesh(n_vertices);
    cg_primitive_t *primitive = rut_mesh_create_primitive(shell, mesh);
    rut_object_unref(mesh);

    return primitive;
}

cg_primitive_t *
rut_create_create_grid(rut_shell_t *shell, float width, float height,
                       float x_space, float y_space)
{
    c_array_t *lines = c_array_new(false, false, sizeof(cg_vertex_p2_t));
    float x, y;
    int n_lines = 0;

    for (x = 0; x < width; x += x_space) {
        cg_vertex_p2_t p[2] = { { .x = x, .y = 0 }, { .x = x, .y = height } };
        c_array_append_vals(lines, p, 2);
        n_lines++;
    }

    for (y = 0; y < height; y += y_space) {
        cg_vertex_p2_t p[2] = { { .x = 0, .y = y }, { .x = width, .y = y } };
        c_array_append_vals(lines, p, 2);
        n_lines++;
    }

    return cg_primitive_new_p2(shell->cg_device,
                               CG_VERTICES_MODE_LINES,
                               n_lines * 2,
                               (cg_vertex_p2_t *)lines->data);
}
