#ifndef _RIG_GEOMETRY_H_
#define _RIG_GEOMETRY_H_

CoglAttribute *
rig_create_circle (CoglContext *ctx,
                   int subdivisions,
                   int *n_verts);

CoglTexture *
rig_create_circle_texture (RigContext *ctx,
                           int radius_texels,
                           int padding_texels);

#endif /* _RIG_GEOMETRY_H_ */
