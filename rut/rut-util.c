/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation
 * Copyright (C) 2010 Openismus GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Note: rut_util_distribute_natural_allocation() is taken from gtk
 * and was implemented by:
 *   Tristan Van Berkom <tristan.van.berkom@gmail.com>
 */

#include <config.h>

#include <math.h>
#include <stdlib.h>

#include <clib.h>
#include <cogl/cogl.h>

#include "rut-global.h"
#include "rut-mesh.h"
#include "rut-util.h"

/* Help macros to scale from OpenGL <-1,1> coordinates system to
 * window coordinates ranging [0,window-size]
 */
#define MTX_GL_SCALE_X(x, w, v1, v2)                                           \
    ((((((x) / (w)) + 1.0f) / 2.0f) * (v1)) + (v2))
#define MTX_GL_SCALE_Y(y, w, v1, v2)                                           \
    ((v1) - (((((y) / (w)) + 1.0f) / 2.0f) * (v1)) + (v2))
#define MTX_GL_SCALE_Z(z, w, v1, v2) (MTX_GL_SCALE_X((z), (w), (v1), (v2)))

typedef struct _vertex4_t {
    float x;
    float y;
    float z;
    float w;
} vertex4_t;

void
rut_util_fully_transform_vertices(const cg_matrix_t *modelview,
                                  const cg_matrix_t *projection,
                                  const float *viewport,
                                  const float *vertices3_in,
                                  float *vertices3_out,
                                  int n_vertices)
{
    cg_matrix_t modelview_projection;
    vertex4_t *vertices_tmp;
    int i;

    vertices_tmp = c_alloca(sizeof(vertex4_t) * n_vertices);

    if (n_vertices >= 4) {
        /* XXX: we should find a way to cache this per actor */
        cg_matrix_multiply(&modelview_projection, projection, modelview);
        cg_matrix_project_points(&modelview_projection,
                                 3,
                                 sizeof(float) * 3,
                                 vertices3_in,
                                 sizeof(vertex4_t),
                                 vertices_tmp,
                                 n_vertices);
    } else {
        cg_matrix_transform_points(modelview,
                                   3,
                                   sizeof(float) * 3,
                                   vertices3_in,
                                   sizeof(vertex4_t),
                                   vertices_tmp,
                                   n_vertices);

        cg_matrix_project_points(projection,
                                 3,
                                 sizeof(vertex4_t),
                                 vertices_tmp,
                                 sizeof(vertex4_t),
                                 vertices_tmp,
                                 n_vertices);
    }

    for (i = 0; i < n_vertices; i++) {
        vertex4_t vertex_tmp = vertices_tmp[i];
        float *vertex_out = vertices3_out + i * 3;
        /* Finally translate from OpenGL coords to window coords */
        vertex_out[0] = MTX_GL_SCALE_X(
            vertex_tmp.x, vertex_tmp.w, viewport[2], viewport[0]);
        vertex_out[1] = MTX_GL_SCALE_Y(
            vertex_tmp.y, vertex_tmp.w, viewport[3], viewport[1]);
    }
}

void
rut_util_print_quaternion(const char *prefix,
                          const cg_quaternion_t *quaternion)
{
    float axis[3], angle;

    cg_quaternion_get_rotation_axis(quaternion, axis);
    angle = cg_quaternion_get_rotation_angle(quaternion);

    c_print("%saxis: (%.2f,%.2f,%.2f) angle: %.2f\n",
            prefix,
            axis[0],
            axis[1],
            axis[2],
            angle);
}

void
rut_util_create_pick_ray(const float viewport[4],
                         const cg_matrix_t *inverse_projection,
                         const cg_matrix_t *camera_transform,
                         float viewport_pos[2],
                         float ray_position[3],      /* out */
                         float ray_direction[3])      /* out */
{
    cg_matrix_t inverse_transform;
    float ndc_x, ndc_y;
    float projected_points[6], unprojected_points[8];

    /* Undo the Viewport transform, putting us in Normalized Device
     * Coords
     *
     * XXX: We are assuming the incomming coordinates are in viewport
     * coordinates not device coordinates so we don't need to apply the
     * viewport offset, we just need to normalized according to the
     * width and height of the viewport.
     */
    ndc_x = viewport_pos[0] * 2.0f / viewport[2] - 1.0f;
    ndc_y =
        ((viewport[3] - 1.0f - viewport_pos[1]) * 2.0f / viewport[3] - 1.0f);

    /* The main drawing code is doing P x C¯¹ (P is the Projection matrix
     * and C is the Camera transform. To inverse that transformation we need
     * to apply C x P¯¹ to the points */
    cg_matrix_multiply(
        &inverse_transform, camera_transform, inverse_projection);

    /* unproject the point at both the near plane and the far plane */
    projected_points[0] = ndc_x;
    projected_points[1] = ndc_y;
    projected_points[2] = 0.0f;
    projected_points[3] = ndc_x;
    projected_points[4] = ndc_y;
    projected_points[5] = 1.0f;
    cg_matrix_project_points(&inverse_transform,
                             3, /* num components for input */
                             sizeof(float) * 3, /* input stride */
                             projected_points,
                             sizeof(float) * 4, /* output stride */
                             unprojected_points,
                             2 /* n_points */);
    unprojected_points[0] /= unprojected_points[3];
    unprojected_points[1] /= unprojected_points[3];
    unprojected_points[2] /= unprojected_points[3];
    unprojected_points[4] /= unprojected_points[7];
    unprojected_points[5] /= unprojected_points[7];
    unprojected_points[6] /= unprojected_points[7];

    ray_position[0] = unprojected_points[0];
    ray_position[1] = unprojected_points[1];
    ray_position[2] = unprojected_points[2];

    ray_direction[0] = unprojected_points[4] - unprojected_points[0];
    ray_direction[1] = unprojected_points[5] - unprojected_points[1];
    ray_direction[2] = unprojected_points[6] - unprojected_points[2];

    cg_vector3_normalize(ray_direction);
}

void
rut_util_transform_normal(const cg_matrix_t *matrix,
                          float *x,
                          float *y,
                          float *z)
{
    float _x = *x, _y = *y, _z = *z;

    *x = matrix->xx * _x + matrix->xy * _y + matrix->xz * _z;
    *y = matrix->yx * _x + matrix->yy * _y + matrix->yz * _z;
    *z = matrix->zx * _x + matrix->zy * _y + matrix->zz * _z;
}

/*
 * From "Fast, Minimum Storage Ray/Triangle Intersection",
 * http://www.cs.virginia.edu/~gfx/Courses/2003/ImageSynthesis/papers/Acceleration/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
 */

#define EPSILON 0.00001

static void
cross(float dest[3], float v1[3], float v2[3])
{
    dest[0] = v1[1] * v2[2] - v1[2] * v2[1];
    dest[1] = v1[2] * v2[0] - v1[0] * v2[2];
    dest[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

static float
dot(float v1[3], float v2[3])
{
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

static void
sub(float dest[3], float v1[3], float v2[3])
{
    dest[0] = v1[0] - v2[0];
    dest[1] = v1[1] - v2[1];
    dest[2] = v1[2] - v2[2];
}

bool
rut_util_intersect_triangle(float v0[3],
                            float v1[3],
                            float v2[3],
                            float ray_origin[3],
                            float ray_direction[3],
                            float *u,
                            float *v,
                            float *t)
{
    float edge1[3], edge2[3], tvec[3], pvec[3], qvec[3];
    float det, inv_det;

    /* find vectors for the two edges sharing v0 */
    sub(edge1, v1, v0);
    sub(edge2, v2, v0);

    /* begin calculating determinant, also used to calculate u */
    cross(pvec, ray_direction, edge2);

    /* if determinant is near zero, ray lies in the triangle's plane */
    det = dot(edge1, pvec);

    if (det > -EPSILON && det < EPSILON)
        return false;

    inv_det = 1.0f / det;

    /* calculate the distance from v0, to ray_origin */
    sub(tvec, ray_origin, v0);

    /* calculate U and test bounds */
    *u = dot(tvec, pvec) * inv_det;
    if (*u < 0.f || *u > 1.f)
        return false;

    /* prepare to test V */
    cross(qvec, tvec, edge1);

    /* calculate V and test bounds */
    *v = dot(ray_direction, qvec) * inv_det;
    if (*v < 0.f || *u + *v > 1.f)
        return false;

    /* calculate t, ray intersects triangle */
    *t = dot(edge2, qvec) * inv_det;

    return true;
}

typedef struct _intersect_state_t {
    float *ray_origin;
    float *ray_direction;
    float min_t;
    int index;
    int hit_index;
    bool found;
} intersect_state_t;

static bool
intersect_triangle_cb(void **attributes_v0,
                      void **attributes_v1,
                      void **attributes_v2,
                      int index_v0,
                      int index_v1,
                      int index_v2,
                      void *user_data)
{
    intersect_state_t *state = user_data;
    float *pos_v0 = attributes_v0[0];
    float *pos_v1 = attributes_v1[0];
    float *pos_v2 = attributes_v2[0];
    float u, v, t;
    bool hit;

    hit = rut_util_intersect_triangle(pos_v0,
                                      pos_v1,
                                      pos_v2,
                                      state->ray_origin,
                                      state->ray_direction,
                                      &u,
                                      &v,
                                      &t);

    /* found a closer triangle. t > 0 means that we don't want results
     * behind the ray origin */
    if (hit && t > 0 && t < state->min_t) {
        state->min_t = t;
        state->found = true;
        state->hit_index = state->index;
    }

    state->index++;

    return true;
}

bool
rut_util_intersect_mesh(rut_mesh_t *mesh,
                        float ray_origin[3],
                        float ray_direction[3],
                        int *index,
                        float *t_out)
{
    intersect_state_t state;

    state.ray_origin = ray_origin;
    state.ray_direction = ray_direction;
    state.min_t = FLT_MAX;
    state.found = false;
    state.index = 0;

    rut_mesh_foreach_triangle(
        mesh, intersect_triangle_cb, &state, "cg_position_in", NULL);

    if (state.found) {
        if (t_out)
            *t_out = state.min_t;

        if (index)
            *index = state.hit_index;

        return true;
    }

    return false;
}

unsigned int
rut_util_one_at_a_time_mix(unsigned int hash)
{
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

static const float jitter_offsets[32] = {
    0.375f, 0.4375f, 0.625f, 0.0625f, 0.875f, 0.1875f, 0.125f, 0.0625f,
    0.375f, 0.6875f, 0.875f, 0.4375f, 0.625f, 0.5625f, 0.375f, 0.9375f,
    0.625f, 0.3125f, 0.125f, 0.5625f, 0.125f, 0.8125f, 0.375f, 0.1875f,
    0.875f, 0.9375f, 0.875f, 0.6875f, 0.125f, 0.3125f, 0.625f, 0.8125f
};

/* XXX: This assumes that the primitive is being drawn in pixel coordinates,
 * since we jitter the modelview not the projection.
 */
void
rut_util_draw_jittered_primitive3f(cg_framebuffer_t *fb,
                                   cg_primitive_t *prim,
                                   float red,
                                   float green,
                                   float blue)
{
    cg_device_t *dev = cg_framebuffer_get_context(fb);
    cg_pipeline_t *pipeline = cg_pipeline_new(dev);
    float viewport[4];
    cg_matrix_t projection;
    float pixel_dx, pixel_dy;
    int i;

    cg_pipeline_set_color4f(
        pipeline, red / 16.0f, green / 16.0f, blue / 16.0f, 1.0f / 16.0f);

    cg_framebuffer_get_viewport4fv(fb, viewport);
    cg_framebuffer_get_projection_matrix(fb, &projection);

    pixel_dx = 2.0 / viewport[2];
    pixel_dy = 2.0 / viewport[3];

    for (i = 0; i < 16; i++) {
        cg_matrix_t tmp = projection;
        cg_matrix_t jitter;
        cg_matrix_t jittered_projection;

        const float *offset = jitter_offsets + 2 * i;

        cg_matrix_init_identity(&jitter);
        cg_matrix_translate(
            &jitter, offset[0] * pixel_dx, offset[1] * pixel_dy, 0);
        cg_matrix_multiply(&jittered_projection, &jitter, &tmp);
        cg_framebuffer_set_projection_matrix(fb, &jittered_projection);
        cg_primitive_draw(prim, fb, pipeline);
    }

    cg_framebuffer_set_projection_matrix(fb, &projection);

    cg_object_unref(pipeline);
}

bool
rut_util_find_tag(const c_list_t *tags, const char *tag)
{
    const c_list_t *l;

    for (l = tags; l; l = l->next) {
        if (strcmp(tag, l->data) == 0)
            return true;
    }
    return false;
}

/* Pulled from gtksizerequest.c from Gtk+ */
static int
compare_gap(const void *p1, const void *p2, void *data)
{
    rut_preferred_size_t *sizes = data;
    const unsigned int *c1 = p1;
    const unsigned int *c2 = p2;

    const int d1 = MAX(sizes[*c1].natural_size - sizes[*c1].minimum_size, 0);
    const int d2 = MAX(sizes[*c2].natural_size - sizes[*c2].minimum_size, 0);

    int delta = (d2 - d1);

    if (0 == delta)
        delta = (*c2 - *c1);

    return delta;
}

int
rut_util_distribute_natural_allocation(int extra_space,
                                       unsigned int n_requested_sizes,
                                       rut_preferred_size_t *sizes)
{
    unsigned int *spreading;
    int i;

    c_return_val_if_fail(extra_space >= 0, 0);

    spreading = c_newa(unsigned int, n_requested_sizes);

    for (i = 0; i < n_requested_sizes; i++)
        spreading[i] = i;

    /* Distribute the container's extra space c_gap. We want to assign
     * this space such that the sum of extra space assigned to children
     * (c^i_gap) is equal to c_cap. The case that there's not enough
     * space for all children to take their natural size needs some
     * attention. The goals we want to achieve are:
     *
     *   a) Maximize number of children taking their natural size.
     *   b) The allocated size of children should be a continuous
     *   function of c_gap.  That is, increasing the container size by
     *   one pixel should never make drastic changes in the distribution.
     *   c) If child i takes its natural size and child j doesn't,
     *   child j should have received at least as much gap as child i.
     *
     * The following code distributes the additional space by following
     * these rules.
     */

    /* Sort descending by gap and position. */
    c_qsort_with_data(
        spreading, n_requested_sizes, sizeof(unsigned int), compare_gap, sizes);

    /* Distribute available space.
     * This master piece of a loop was conceived by Behdad Esfahbod.
     */
    for (i = n_requested_sizes - 1; extra_space > 0 && i >= 0; --i) {
        /* Divide remaining space by number of remaining children.
         * Sort order and reducing remaining space by assigned space
         * ensures that space is distributed equally.
         */
        int glue = (extra_space + i) / (i + 1);
        int gap = sizes[(spreading[i])].natural_size -
                  sizes[(spreading[i])].minimum_size;

        int extra = MIN(glue, gap);

        sizes[spreading[i]].minimum_size += extra;

        extra_space -= extra;
    }

    return extra_space;
}

bool
rut_util_is_boolean_env_set(const char *variable)
{
    char *val = getenv(variable);
    bool ret;

    if (!val)
        return false;

    if (c_ascii_strcasecmp(val, "1") == 0 ||
        c_ascii_strcasecmp(val, "on") == 0 ||
        c_ascii_strcasecmp(val, "true") == 0)
        ret = true;
    else if (c_ascii_strcasecmp(val, "0") == 0 ||
             c_ascii_strcasecmp(val, "off") == 0 ||
             c_ascii_strcasecmp(val, "false") == 0)
        ret = false;
    else {
        c_critical("Spurious boolean environment variable value (%s=%s)",
                   variable,
                   val);
        ret = true;
    }

    return ret;
}

void
rut_util_matrix_scaled_perspective(cg_matrix_t *matrix,
                                   float fov_y,
                                   float aspect,
                                   float z_near,
                                   float z_far,
                                   float scale)
{
    float ymax = z_near * tanf(fov_y * C_PI / 360.0);
    float inverse_scale = 1.0 / scale;

    cg_matrix_frustum(matrix,
                      -ymax * aspect * inverse_scale, /* left */
                      ymax * aspect * inverse_scale, /* right */
                      -ymax * inverse_scale, /* bottom */
                      ymax * inverse_scale, /* top */
                      z_near,
                      z_far);
}

/* XXX: The vertices must be 4 components: [x, y, z, w] */
void
rut_util_fully_transform_points(const cg_matrix_t *modelview,
                                const cg_matrix_t *projection,
                                const float *viewport,
                                float *verts,
                                int n_verts)
{
    int i;

    cg_matrix_transform_points(modelview,
                               2, /* n_components */
                               sizeof(float) * 4, /* stride_in */
                               verts, /* points_in */
                               /* strideout */
                               sizeof(float) * 4,
                               verts, /* points_out */
                               4 /* n_points */);

    cg_matrix_project_points(projection,
                             3, /* n_components */
                             sizeof(float) * 4, /* stride_in */
                             verts, /* points_in */
                             /* strideout */
                             sizeof(float) * 4,
                             verts, /* points_out */
                             4 /* n_points */);

/* Scale from OpenGL normalized device coordinates (ranging from -1 to 1)
 * to Cogl window/framebuffer coordinates (ranging from 0 to buffer-size) with
 * (0,0) being top left. */
#define VIEWPORT_TRANSFORM_X(x, vp_origin_x, vp_width)                         \
    ((((x) + 1.0) * ((vp_width) / 2.0)) + (vp_origin_x))
/* Note: for Y we first flip all coordinates around the X axis while in
 * normalized device coodinates */
#define VIEWPORT_TRANSFORM_Y(y, vp_origin_y, vp_height)                        \
    ((((-(y)) + 1.0) * ((vp_height) / 2.0)) + (vp_origin_y))

    /* Scale from normalized device coordinates (in range [-1,1]) to
     * window coordinates ranging [0,window-size] ... */
    for (i = 0; i < n_verts; i++) {
        float w = verts[4 * i + 3];

        /* Perform perspective division */
        verts[4 * i] /= w;
        verts[4 * i + 1] /= w;

        /* Apply viewport transform */
        verts[4 * i] =
            VIEWPORT_TRANSFORM_X(verts[4 * i], viewport[0], viewport[2]);
        verts[4 * i + 1] =
            VIEWPORT_TRANSFORM_Y(verts[4 * i + 1], viewport[1], viewport[3]);
    }

#undef VIEWPORT_TRANSFORM_X
#undef VIEWPORT_TRANSFORM_Y
}
