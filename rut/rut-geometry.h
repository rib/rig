/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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

#ifndef _RUT_GEOMETRY_H_
#define _RUT_GEOMETRY_H_

#include <rut-types.h>
#include <rut-mesh.h>

cg_attribute_t *
rut_create_circle_fan_p2(rut_shell_t *shell, int subdivisions, int *n_verts);

cg_primitive_t *rut_create_circle_fan_primitive(rut_shell_t *shell,
                                                int subdivisions);

rut_mesh_t *rut_create_circle_outline_mesh(uint8_t n_vertices);

cg_primitive_t *rut_create_circle_outline_primitive(rut_shell_t *shell,
                                                    uint8_t n_vertices);

cg_texture_t *rut_create_circle_texture(rut_shell_t *shell,
                                        int radius_texels,
                                        int padding_texels);

void rut_tesselate_circle_with_line_indices(cg_vertex_p3c4_t *buffer,
                                            uint8_t n_vertices,
                                            uint8_t *indices_data,
                                            int indices_base,
                                            rut_axis_t axis,
                                            uint8_t r,
                                            uint8_t g,
                                            uint8_t b);

rut_mesh_t *rut_create_rotation_tool_mesh(uint8_t n_vertices);

cg_primitive_t *rut_create_rotation_tool_primitive(rut_shell_t *shell,
                                                   uint8_t n_vertices);

cg_primitive_t *rut_create_create_grid(rut_shell_t *shell,
                                       float width,
                                       float height,
                                       float x_space,
                                       float y_space);

#endif /* _RUT_GEOMETRY_H_ */
