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

#include <rut-config.h>

#include <cglib/cglib.h>
#include <clib.h>
#include <string.h>

#include "rut-planes.h"

typedef struct _vector4_t {
    float x, y, z, w;
} vector4_t;

void
rut_get_eye_planes_for_screen_poly(float *polygon,
                                   int n_vertices,
                                   float *viewport,
                                   const c_matrix_t *projection,
                                   const c_matrix_t *inverse_project,
                                   rut_plane_t *planes)
{
#if 0
    float Wc;
    vector4_t *tmp_poly;
    rut_plane_t *plane;
    int i;
    float b[3];
    float c[3];
    int count;

    tmp_poly = c_alloca (sizeof (vector4_t) * n_vertices * 2);

#define DEPTH -50

    /* Determine W in clip-space (Wc) for a point (0, 0, DEPTH, 1)
     *
     * Note: the depth could be anything except 0.
     *
     * We will transform the polygon into clip coordinates using this
     * depth and then into eye coordinates. Our clip planes will be
     * defined by triangles that extend between points of the polygon at
     * DEPTH and corresponding points of the same polygon at DEPTH * 2.
     *
     * NB: Wc defines the position of the clip planes in clip
     * coordinates. Given a screen aligned cross section through the
     * frustum; coordinates range from [-Wc,Wc] left to right on the
     * x-axis and [Wc,-Wc] top to bottom on the y-axis.
     */
    Wc = DEPTH * projection->wz + projection->ww;

#define CLIP_X(X) ((((float)X - viewport[0]) * (2.0 / viewport[2])) - 1) * Wc
#define CLIP_Y(Y) ((((float)Y - viewport[1]) * (2.0 / viewport[3])) - 1) * -Wc

    for (i = 0; i < n_vertices; i++)
    {
        tmp_poly[i].x = CLIP_X (polygon[i * 2]);
        tmp_poly[i].y = CLIP_Y (polygon[i * 2 + 1]);
        tmp_poly[i].z = DEPTH;
        tmp_poly[i].w = Wc;
    }

    Wc = DEPTH * 2 * projection->wz + projection->ww;

    /* FIXME: technically we don't need to project all of the points
     * twice, it would be enough project every other point since
     * we can share points in this set to define the plane vectors. */
    for (i = 0; i < n_vertices; i++)
    {
        tmp_poly[n_vertices + i].x = CLIP_X (polygon[i * 2]);
        tmp_poly[n_vertices + i].y = CLIP_Y (polygon[i * 2 + 1]);
        tmp_poly[n_vertices + i].z = DEPTH * 2;
        tmp_poly[n_vertices + i].w = Wc;
    }

#undef CLIP_X
#undef CLIP_Y

    c_matrix_project_points (inverse_project,
                              4,
                              sizeof (vector4_t),
                              tmp_poly,
                              sizeof (vector4_t),
                              tmp_poly,
                              n_vertices * 2);

    /* XXX: It's quite ugly that we end up with these casts between
     * vector4_t types and CoglVector3s, it might be better if the
     * c_vector APIs just took pointers to floats.
     */

    count = n_vertices - 1;
    for (i = 0; i < count; i++)
    {
        plane = &planes[i];
        plane->v0 = *(CoglVector3 *)&tmp_poly[i];
        b = *(CoglVector3 *)&tmp_poly[n_vertices + i];
        c = *(CoglVector3 *)&tmp_poly[n_vertices + i + 1];
        c_vector3_subtract (&b, &b, &plane->v0);
        c_vector3_subtract (&c, &c, &plane->v0);
        c_vector3_cross_product (&plane->n, &b, &c);
        c_vector3_normalize (&plane->n);
    }

    plane = &planes[n_vertices - 1];
    plane->v0 = *(CoglVector3 *)&tmp_poly[0];
    b = *(CoglVector3 *)&tmp_poly[2 * n_vertices - 1];
    c = *(CoglVector3 *)&tmp_poly[n_vertices];
    c_vector3_subtract (&b, &b, &plane->v0);
    c_vector3_subtract (&c, &c, &plane->v0);
    c_vector3_cross_product (&plane->n, &b, &c);
    c_vector3_normalize (&plane->n);
#endif
    float Wc;
    vector4_t *tmp_poly;
    rut_plane_t *plane;
    int i;
    float b[3];
    float c[3];
    int count;

    tmp_poly = c_alloca(sizeof(vector4_t) * n_vertices * 2);

#define DEPTH -50

    /* Determine W in clip-space (Wc) for a point (0, 0, DEPTH, 1)
     *
     * Note: the depth could be anything except 0.
     *
     * We will transform the polygon into clip coordinates using this
     * depth and then into eye coordinates. Our clip planes will be
     * defined by triangles that extend between points of the polygon at
     * DEPTH and corresponding points of the same polygon at DEPTH * 2.
     *
     * NB: Wc defines the position of the clip planes in clip
     * coordinates. Given a screen aligned cross section through the
     * frustum; coordinates range from [-Wc,Wc] left to right on the
     * x-axis and [Wc,-Wc] top to bottom on the y-axis.
     */
    Wc = DEPTH * projection->wz + projection->ww;

#define CLIP_X(X) ((((float)X - viewport[0]) * (2.0 / viewport[2])) - 1) * Wc
#define CLIP_Y(Y) ((((float)Y - viewport[1]) * (2.0 / viewport[3])) - 1) * -Wc

    for (i = 0; i < n_vertices; i++) {
        tmp_poly[i].x = CLIP_X(polygon[i * 2]);
        tmp_poly[i].y = CLIP_Y(polygon[i * 2 + 1]);
        tmp_poly[i].z = DEPTH;
        tmp_poly[i].w = Wc;
    }

    Wc = DEPTH * 2 * projection->wz + projection->ww;

    /* FIXME: technically we don't need to project all of the points
     * twice, it would be enough project every other point since
     * we can share points in this set to define the plane vectors. */
    for (i = 0; i < n_vertices; i++) {
        tmp_poly[n_vertices + i].x = CLIP_X(polygon[i * 2]);
        tmp_poly[n_vertices + i].y = CLIP_Y(polygon[i * 2 + 1]);
        tmp_poly[n_vertices + i].z = DEPTH * 2;
        tmp_poly[n_vertices + i].w = Wc;
    }

#undef CLIP_X
#undef CLIP_Y

    c_matrix_project_points(inverse_project,
                             4,
                             sizeof(vector4_t),
                             tmp_poly,
                             sizeof(vector4_t),
                             tmp_poly,
                             n_vertices * 2);

    /* XXX: It's quite ugly that we end up with these casts between
     * vector4_t types and CoglVector3s, it might be better if the
     * c_vector APIs just took pointers to floats.
     */

    count = n_vertices - 1;
    for (i = 0; i < count; i++) {
        plane = &planes[i];
        memcpy(plane->v0, tmp_poly + i, sizeof(float) * 3);
        memcpy(b, tmp_poly + n_vertices + i, sizeof(float) * 3);
        memcpy(c, tmp_poly + n_vertices + i + 1, sizeof(float) * 3);
        c_vector3_subtract(b, b, plane->v0);
        c_vector3_subtract(c, c, plane->v0);
        c_vector3_cross_product(plane->n, b, c);
        c_vector3_normalize(plane->n);
    }

    plane = &planes[n_vertices - 1];
    memcpy(plane->v0, tmp_poly + 0, sizeof(float) * 3);
    memcpy(b, tmp_poly + (2 * n_vertices - 1), sizeof(float) * 3);
    memcpy(c, tmp_poly + n_vertices, sizeof(float) * 3);
    c_vector3_subtract(b, b, plane->v0);
    c_vector3_subtract(c, c, plane->v0);
    c_vector3_cross_product(plane->n, b, c);
    c_vector3_normalize(plane->n);
}
