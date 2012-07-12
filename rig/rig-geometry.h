#ifndef _RIG_GEOMETRY_H_
#define _RIG_GEOMETRY_H_

CoglAttribute *
rig_create_circle_fan_p2 (RigContext *ctx,
                          int subdivisions,
                          int *n_verts);

CoglPrimitive *
rig_create_circle_outline_primitive (RigContext *ctx,
                                     uint8_t n_vertices);

CoglTexture *
rig_create_circle_texture (RigContext *ctx,
                           int radius_texels,
                           int padding_texels);

void
rig_tesselate_circle_with_line_indices (CoglVertexP3C4 *buffer,
                                        uint8_t n_vertices,
                                        uint8_t *indices_data,
                                        int indices_base,
                                        RigAxis axis,
                                        uint8_t r,
                                        uint8_t g,
                                        uint8_t b);

CoglPrimitive *
rig_create_rotation_tool_primitive (RigContext *ctx,
                                    uint8_t n_vertices);

#endif /* _RIG_GEOMETRY_H_ */
