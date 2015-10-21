/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2011, 2013  Intel Corporation.
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

#ifndef _RUT_UTIL_H_
#define _RUT_UTIL_H_

#include <stdbool.h>

#include <cglib/cglib.h>

#include "rut-mesh.h"

/* This is a replacement for the nearbyint function which always
 * rounds to the nearest integer. nearbyint is apparently a C99
 * function so it might not always be available but also it seems in
 * glibc it is defined as a function call so this macro could end up
 * faster anyway. We can't just add 0.5f because it will break for
 * negative numbers. */
#define RUT_UTIL_NEARBYINT(x) ((int)((x) < 0.0f ? (x) - 0.5f : (x) + 0.5f))

#ifdef RUT_HAS_GLIB_SUPPORT
#define _RUT_RETURN_IF_FAIL(EXPR) c_return_if_fail(EXPR)
#define _RUT_RETURN_VAL_IF_FAIL(EXPR, VAL) c_return_val_if_fail(EXPR, VAL)
#else
#define _RUT_RETURN_IF_FAIL(EXPR)                                              \
    do {                                                                       \
        if (!(EXPR)) {                                                         \
            fprintf(stderr,                                                    \
                    "file %s: line %d: assertion `%s' failed",                 \
                    __FILE__,                                                  \
                    __LINE__,                                                  \
                    #EXPR);                                                    \
            return;                                                            \
        };                                                                     \
    } while (0)
#define _RUT_RETURN_VAL_IF_FAIL(EXPR, VAL)                                     \
    do {                                                                       \
        if (!(EXPR)) {                                                         \
            fprintf(stderr,                                                    \
                    "file %s: line %d: assertion `%s' failed",                 \
                    __FILE__,                                                  \
                    __LINE__,                                                  \
                    #EXPR);                                                    \
            return (VAL);                                                      \
        };                                                                     \
    } while (0)
#endif /* RUT_HAS_GLIB_SUPPORT */

void rut_util_fully_transform_vertices(const c_matrix_t *modelview,
                                       const c_matrix_t *projection,
                                       const float *viewport,
                                       const float *vertices3_in,
                                       float *vertices3_out,
                                       int n_vertices);

void rut_util_create_pick_ray(const float viewport[4],
                              const c_matrix_t *inverse_projection,
                              const c_matrix_t *camera_transform,
                              float screen_pos[2],
                              float ray_position[3], /* out */
                              float ray_direction[3]); /* out */

void rut_util_print_quaternion(const char *prefix,
                               const c_quaternion_t *quaternion);

void rut_util_transform_normal(const c_matrix_t *matrix,
                               float *x,
                               float *y,
                               float *z);

bool rut_util_intersect_triangle(float v0[3],
                                 float v1[3],
                                 float v2[3],
                                 float ray_origin[3],
                                 float ray_direction[3],
                                 float *u,
                                 float *v,
                                 float *t);
bool rut_util_intersect_model(const void *vertices,
                              int n_points,
                              size_t stride,
                              float ray_origin[3],
                              float ray_direction[3],
                              int *index,
                              float *t_out);

/* Split Bob Jenkins' One-at-a-Time hash
 *
 * This uses the One-at-a-Time hash algorithm designed by Bob Jenkins
 * but the mixing step is split out so the function can be used in a
 * more incremental fashion.
 */
static inline unsigned int
rut_util_one_at_a_time_hash(unsigned int hash, const void *key, size_t bytes)
{
    const unsigned char *p = (const unsigned char *)key;
    int i;

    for (i = 0; i < bytes; i++) {
        hash += p[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    return hash;
}

unsigned int rut_util_one_at_a_time_mix(unsigned int hash);

void rut_util_draw_jittered_primitive3f(cg_framebuffer_t *fb,
                                        cg_primitive_t *prim,
                                        float red,
                                        float green,
                                        float blue);

bool rut_util_find_tag(const c_llist_t *tags, const char *tag);

bool rut_util_intersect_mesh(rut_mesh_t *mesh,
                             float ray_origin[3],
                             float ray_direction[3],
                             int *index,
                             float *t_out);

bool rut_util_is_boolean_env_set(const char *variable);

void rut_util_matrix_scaled_frustum(c_matrix_t *matrix,
                                    float left,
                                    float right,
                                    float bottom,
                                    float top,
                                    float z_near,
                                    float z_far,
                                    float scale);

void rut_util_matrix_scaled_perspective(c_matrix_t *matrix,
                                        float fov_y,
                                        float aspect,
                                        float z_near,
                                        float z_far,
                                        float scale);

/* We've made a notable change to the original algorithm referenced
 * above to make sure we have reliable results for screen aligned
 * rectangles even though there may be some numerical in-precision in
 * how the vertices of the polygon were calculated.
 *
 * We've avoided introducing an epsilon factor to the comparisons
 * since we feel there's a risk of changing some semantics in ways that
 * might not be desirable. One of those is that if you transform two
 * polygons which share an edge and test a point close to that edge
 * then this algorithm will currently give a positive result for only
 * one polygon.
 *
 * Another concern is the way this algorithm resolves the corner case
 * where the horizontal ray being cast to count edge crossings may
 * cross directly through a vertex. The solution is based on the "idea
 * of Simulation of Simplicity" and "pretends to shift the ray
 * infinitesimally down so that it either clearly intersects, or
 * clearly doesn't touch". I'm not familiar with the idea myself so I
 * expect a misplaced epsilon is likely to break that aspect of the
 * algorithm.
 *
 * The simple solution we've gone for is to pixel align the polygon
 * vertices which should eradicate most noise due to in-precision.
 */
static inline int
rut_util_point_in_screen_poly(
    float point_x, float point_y, void *vertices, size_t stride, int n_vertices)
{
    int i, j, c = 0;

    for (i = 0, j = n_vertices - 1; i < n_vertices; j = i++) {
        float vert_xi = *(float *)((uint8_t *)vertices + i * stride);
        float vert_xj = *(float *)((uint8_t *)vertices + j * stride);
        float vert_yi =
            *(float *)((uint8_t *)vertices + i * stride + sizeof(float));
        float vert_yj =
            *(float *)((uint8_t *)vertices + j * stride + sizeof(float));

        vert_xi = RUT_UTIL_NEARBYINT(vert_xi);
        vert_xj = RUT_UTIL_NEARBYINT(vert_xj);
        vert_yi = RUT_UTIL_NEARBYINT(vert_yi);
        vert_yj = RUT_UTIL_NEARBYINT(vert_yj);

        if (((vert_yi > point_y) != (vert_yj > point_y)) &&
            (point_x <
             (vert_xj - vert_xi) * (point_y - vert_yi) / (vert_yj - vert_yi) +
             vert_xi))
            c = !c;
    }

    return c;
}

void rut_util_fully_transform_points(const c_matrix_t *modelview,
                                     const c_matrix_t *projection,
                                     const float *viewport,
                                     float *verts,
                                     int n_verts);

#define RUT_UINT32_RED_AS_FLOAT(COLOR) (((COLOR & 0xff000000) >> 24) / 255.0)
#define RUT_UINT32_GREEN_AS_FLOAT(COLOR) (((COLOR & 0xff0000) >> 16) / 255.0)
#define RUT_UINT32_BLUE_AS_FLOAT(COLOR) (((COLOR & 0xff00) >> 8) / 255.0)
#define RUT_UINT32_ALPHA_AS_FLOAT(COLOR) ((COLOR & 0xff) / 255.0)

#endif /* _RUT_UTIL_H_ */
