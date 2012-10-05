#ifndef _RUT_GEOMETRY_H_
#define _RUT_GEOMETRY_H_

#include <rut-types.h>
#include <rut-mesh.h>

CoglAttribute *
rut_create_circle_fan_p2 (RutContext *ctx,
                          int subdivisions,
                          int *n_verts);

RutMesh *
rut_create_circle_outline_mesh (uint8_t n_vertices);

CoglPrimitive *
rut_create_circle_outline_primitive (RutContext *ctx,
                                     uint8_t n_vertices);

CoglTexture *
rut_create_circle_texture (RutContext *ctx,
                           int radius_texels,
                           int padding_texels);

void
rut_tesselate_circle_with_line_indices (CoglVertexP3C4 *buffer,
                                        uint8_t n_vertices,
                                        uint8_t *indices_data,
                                        int indices_base,
                                        RutAxis axis,
                                        uint8_t r,
                                        uint8_t g,
                                        uint8_t b);

RutMesh *
rut_create_rotation_tool_mesh (uint8_t n_vertices);

CoglPrimitive *
rut_create_rotation_tool_primitive (RutContext *ctx,
                                    uint8_t n_vertices);

CoglPrimitive *
rut_create_create_grid (RutContext *ctx,
                        float width,
                        float height,
                        float x_space,
                        float y_space);

#endif /* _RUT_GEOMETRY_H_ */
