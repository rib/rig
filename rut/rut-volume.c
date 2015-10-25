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

#include <string.h>

#include <math.h>
#include <cglib/cglib.h>

#include "rut-volume-private.h"
#include "rut-util.h"

rut_volume_t *
rut_volume_new(void)
{
    rut_volume_t *volume = c_slice_new(rut_volume_t);

    memset(volume->vertices, 0, 8 * sizeof(rut_vector3_t));

    volume->is_static = false;
    volume->is_empty = true;
    volume->is_axis_aligned = true;
    volume->is_complete = true;
    volume->is_2d = true;

    return volume;
}

/* Since volumes are used so heavily in a typical paint
 * traversal of a Rut scene graph and since volumes often
 * have a very short life cycle that maps well to stack allocation we
 * allow initializing a static rut_volume_t variable to avoid
 * hammering the slice allocator.
 *
 * We were seeing slice allocation take about 1% cumulative CPU time
 * for some very simple rig tests which although it isn't a *lot*
 * this is an easy way to basically drop that to 0%.
 *
 * The PaintVolume will be internally marked as static and
 * rut_volume_free should still be used to "free" static
 * volumes. This allows us to potentially store dynamically allocated
 * data inside volumes in the future since we would be able to
 * free it during _paint_volume_free().
 */
void
rut_volume_init(rut_volume_t *volume)
{
    memset(volume->vertices, 0, 8 * sizeof(rut_vector3_t));

    volume->is_static = true;
    volume->is_empty = true;
    volume->is_axis_aligned = true;
    volume->is_complete = true;
    volume->is_2d = true;
}

void
_rut_volume_copy_static(const rut_volume_t *src_volume,
                        rut_volume_t *dst_volume)
{

    c_return_if_fail(src_volume != NULL && dst_volume != NULL);

    memcpy(dst_volume, src_volume, sizeof(rut_volume_t));
    dst_volume->is_static = true;
}

rut_volume_t *
rut_volume_copy(const rut_volume_t *volume)
{
    rut_volume_t *copy;

    c_return_val_if_fail(volume != NULL, NULL);

    copy = c_slice_dup(rut_volume_t, volume);
    copy->is_static = false;

    return copy;
}

void
_rut_volume_set_from_volume(rut_volume_t *volume, const rut_volume_t *src)
{
    bool is_static = volume->is_static;
    memcpy(volume, src, sizeof(rut_volume_t));
    volume->is_static = is_static;
}

void
rut_volume_free(rut_volume_t *volume)
{
    c_return_if_fail(volume != NULL);

    if (C_LIKELY(volume->is_static))
        return;

    c_slice_free(rut_volume_t, volume);
}

void
rut_volume_set_origin(rut_volume_t *volume, const rut_vector3_t *origin)
{
    static const int key_vertices[4] = { 0, 1, 3, 4 };
    float dx, dy, dz;
    int i;

    c_return_if_fail(volume != NULL);

    dx = origin->x - volume->vertices[0].x;
    dy = origin->y - volume->vertices[0].y;
    dz = origin->z - volume->vertices[0].z;

    /* If we change the origin then all the key vertices of the paint
     * volume need to be shifted too... */
    for (i = 0; i < 4; i++) {
        volume->vertices[key_vertices[i]].x += dx;
        volume->vertices[key_vertices[i]].y += dy;
        volume->vertices[key_vertices[i]].z += dz;
    }

    volume->is_complete = false;
}

void
rut_volume_get_origin(const rut_volume_t *volume, rut_vector3_t *origin)
{
    c_return_if_fail(volume != NULL);
    c_return_if_fail(origin != NULL);

    *origin = volume->vertices[0];
}

static void
_rut_volume_update_is_empty(rut_volume_t *volume)
{
    if (volume->vertices[0].x == volume->vertices[1].x &&
        volume->vertices[0].y == volume->vertices[3].y &&
        volume->vertices[0].z == volume->vertices[4].z)
        volume->is_empty = true;
    else
        volume->is_empty = false;
}

void
rut_volume_set_width(rut_volume_t *volume, float width)
{
    float right_xpos;

    c_return_if_fail(volume != NULL);
    c_return_if_fail(width >= 0.0f);

    /* If the volume is currently empty then only the origin is
     * currently valid */
    if (volume->is_empty)
        volume->vertices[1] = volume->vertices[3] = volume->vertices[4] =
                                                        volume->vertices[0];

    if (!volume->is_axis_aligned)
        rut_volume_axis_align(volume);

    right_xpos = volume->vertices[0].x + width;

    /* Move the right vertices of the paint box relative to the
     * origin... */
    volume->vertices[1].x = right_xpos;
    /* volume->vertices[2].x = right_xpos; NB: updated lazily */
    /* volume->vertices[5].x = right_xpos; NB: updated lazily */
    /* volume->vertices[6].x = right_xpos; NB: updated lazily */

    volume->is_complete = false;

    _rut_volume_update_is_empty(volume);
}

float
rut_volume_get_width(const rut_volume_t *volume)
{
    c_return_val_if_fail(volume != NULL, 0.0);

    if (volume->is_empty)
        return 0;
    else if (!volume->is_axis_aligned) {
        rut_volume_t tmp;
        float width;
        _rut_volume_copy_static(volume, &tmp);
        rut_volume_axis_align(&tmp);
        width = tmp.vertices[1].x - tmp.vertices[0].x;
        rut_volume_free(&tmp);
        return width;
    } else
        return volume->vertices[1].x - volume->vertices[0].x;
}

void
rut_volume_set_height(rut_volume_t *volume, float height)
{
    float height_ypos;

    c_return_if_fail(volume != NULL);
    c_return_if_fail(height >= 0.0f);

    /* If the volume is currently empty then only the origin is
     * currently valid */
    if (volume->is_empty)
        volume->vertices[1] = volume->vertices[3] = volume->vertices[4] =
                                                        volume->vertices[0];

    if (!volume->is_axis_aligned)
        rut_volume_axis_align(volume);

    height_ypos = volume->vertices[0].y + height;

    /* Move the bottom vertices of the paint box relative to the
     * origin... */
    /* volume->vertices[2].y = height_ypos; NB: updated lazily */
    volume->vertices[3].y = height_ypos;
    /* volume->vertices[6].y = height_ypos; NB: updated lazily */
    /* volume->vertices[7].y = height_ypos; NB: updated lazily */
    volume->is_complete = false;

    _rut_volume_update_is_empty(volume);
}

float
rut_volume_get_height(const rut_volume_t *volume)
{
    c_return_val_if_fail(volume != NULL, 0.0);

    if (volume->is_empty)
        return 0;
    else if (!volume->is_axis_aligned) {
        rut_volume_t tmp;
        float height;
        _rut_volume_copy_static(volume, &tmp);
        rut_volume_axis_align(&tmp);
        height = tmp.vertices[3].y - tmp.vertices[0].y;
        rut_volume_free(&tmp);
        return height;
    } else
        return volume->vertices[3].y - volume->vertices[0].y;
}

void
rut_volume_set_depth(rut_volume_t *volume, float depth)
{
    float depth_zpos;

    c_return_if_fail(volume != NULL);
    c_return_if_fail(depth >= 0.0f);

    /* If the volume is currently empty then only the origin is
     * currently valid */
    if (volume->is_empty)
        volume->vertices[1] = volume->vertices[3] = volume->vertices[4] =
                                                        volume->vertices[0];

    if (!volume->is_axis_aligned)
        rut_volume_axis_align(volume);

    depth_zpos = volume->vertices[0].z + depth;

    /* Move the back vertices of the paint box relative to the
     * origin... */
    volume->vertices[4].z = depth_zpos;
    /* volume->vertices[5].z = depth_zpos; NB: updated lazily */
    /* volume->vertices[6].z = depth_zpos; NB: updated lazily */
    /* volume->vertices[7].z = depth_zpos; NB: updated lazily */

    volume->is_complete = false;
    volume->is_2d = depth ? false : true;
    _rut_volume_update_is_empty(volume);
}

float
rut_volume_get_depth(const rut_volume_t *volume)
{
    c_return_val_if_fail(volume != NULL, 0.0);

    if (volume->is_empty)
        return 0;
    else if (!volume->is_axis_aligned) {
        rut_volume_t tmp;
        float depth;
        _rut_volume_copy_static(volume, &tmp);
        rut_volume_axis_align(&tmp);
        depth = tmp.vertices[4].z - tmp.vertices[0].z;
        rut_volume_free(&tmp);
        return depth;
    } else
        return volume->vertices[4].z - volume->vertices[0].z;
}

void
rut_volume_union(rut_volume_t *volume, const rut_volume_t *another_volume)
{
    rut_volume_t aligned_volume;

    c_return_if_fail(volume != NULL);
    c_return_if_fail(another_volume != NULL);

    /* NB: we only have to update vertices 0, 1, 3 and 4
    * (See the rut_volume_t typedef for more details) */

    /* We special case empty volumes because otherwise we'd end up
     * calculating a bounding box that would enclose the origin of
     * the empty volume which isn't desired.
     */
    if (another_volume->is_empty)
        return;

    if (volume->is_empty) {
        _rut_volume_set_from_volume(volume, another_volume);
        goto done;
    }

    if (!volume->is_axis_aligned)
        rut_volume_axis_align(volume);

    if (!another_volume->is_axis_aligned) {
        _rut_volume_copy_static(another_volume, &aligned_volume);
        rut_volume_axis_align(&aligned_volume);
        another_volume = &aligned_volume;
    }

    /* grow left*/
    /* left vertices 0, 3, 4, 7 */
    if (another_volume->vertices[0].x < volume->vertices[0].x) {
        int min_x = another_volume->vertices[0].x;
        volume->vertices[0].x = min_x;
        volume->vertices[3].x = min_x;
        volume->vertices[4].x = min_x;
        /* volume->vertices[7].x = min_x; */
    }

    /* grow right */
    /* right vertices 1, 2, 5, 6 */
    if (another_volume->vertices[1].x > volume->vertices[1].x) {
        int max_x = another_volume->vertices[1].x;
        volume->vertices[1].x = max_x;
        /* volume->vertices[2].x = max_x; */
        /* volume->vertices[5].x = max_x; */
        /* volume->vertices[6].x = max_x; */
    }

    /* grow up */
    /* top vertices 0, 1, 4, 5 */
    if (another_volume->vertices[0].y < volume->vertices[0].y) {
        int min_y = another_volume->vertices[0].y;
        volume->vertices[0].y = min_y;
        volume->vertices[1].y = min_y;
        volume->vertices[4].y = min_y;
        /* volume->vertices[5].y = min_y; */
    }

    /* grow down */
    /* bottom vertices 2, 3, 6, 7 */
    if (another_volume->vertices[3].y > volume->vertices[3].y) {
        int may_y = another_volume->vertices[3].y;
        /* volume->vertices[2].y = may_y; */
        volume->vertices[3].y = may_y;
        /* volume->vertices[6].y = may_y; */
        /* volume->vertices[7].y = may_y; */
    }

    /* grow forward */
    /* front vertices 0, 1, 2, 3 */
    if (another_volume->vertices[0].z < volume->vertices[0].z) {
        int min_z = another_volume->vertices[0].z;
        volume->vertices[0].z = min_z;
        volume->vertices[1].z = min_z;
        /* volume->vertices[2].z = min_z; */
        volume->vertices[3].z = min_z;
    }

    /* grow backward */
    /* back vertices 4, 5, 6, 7 */
    if (another_volume->vertices[4].z > volume->vertices[4].z) {
        int maz_z = another_volume->vertices[4].z;
        volume->vertices[4].z = maz_z;
        /* volume->vertices[5].z = maz_z; */
        /* volume->vertices[6].z = maz_z; */
        /* volume->vertices[7].z = maz_z; */
    }

    if (volume->vertices[4].z == volume->vertices[0].z)
        volume->is_2d = true;
    else
        volume->is_2d = false;

done:
    volume->is_empty = false;
    volume->is_complete = false;
}

/* The paint_volume setters only update vertices 0, 1, 3 and
 * 4 since the others can be drived from them.
 *
 * This will set volume->completed = true;
 */
void
_rut_volume_complete(rut_volume_t *volume)
{
    float dx_l2r, dy_l2r, dz_l2r;
    float dx_t2b, dy_t2b, dz_t2b;

    if (volume->is_empty)
        return;

    /* Find the vector that takes us from any vertex on the left face to
     * the corresponding vertex on the right face. */
    dx_l2r = volume->vertices[1].x - volume->vertices[0].x;
    dy_l2r = volume->vertices[1].y - volume->vertices[0].y;
    dz_l2r = volume->vertices[1].z - volume->vertices[0].z;

    /* Find the vector that takes us from any vertex on the top face to
     * the corresponding vertex on the bottom face. */
    dx_t2b = volume->vertices[3].x - volume->vertices[0].x;
    dy_t2b = volume->vertices[3].y - volume->vertices[0].y;
    dz_t2b = volume->vertices[3].z - volume->vertices[0].z;

    /* front-bottom-right */
    volume->vertices[2].x = volume->vertices[3].x + dx_l2r;
    volume->vertices[2].y = volume->vertices[3].y + dy_l2r;
    volume->vertices[2].z = volume->vertices[3].z + dz_l2r;

    if (C_UNLIKELY(!volume->is_2d)) {
        /* back-top-right */
        volume->vertices[5].x = volume->vertices[4].x + dx_l2r;
        volume->vertices[5].y = volume->vertices[4].y + dy_l2r;
        volume->vertices[5].z = volume->vertices[4].z + dz_l2r;

        /* back-bottom-right */
        volume->vertices[6].x = volume->vertices[5].x + dx_t2b;
        volume->vertices[6].y = volume->vertices[5].y + dy_t2b;
        volume->vertices[6].z = volume->vertices[5].z + dz_t2b;

        /* back-bottom-left */
        volume->vertices[7].x = volume->vertices[4].x + dx_t2b;
        volume->vertices[7].y = volume->vertices[4].y + dy_t2b;
        volume->vertices[7].z = volume->vertices[4].z + dz_t2b;
    }

    volume->is_complete = true;
}

/*
 * _rut_volume_get_box:
 * @volume: a #rut_volume_t
 * @box: a pixel aligned #RutGeometry
 *
 * Transforms a 3D volume into a 2D bounding box in the
 * same coordinate space as the 3D volume.
 *
 * To get a "paint box" you should first project the volume into window
 * coordinates before getting the 2D bounding box.
 *
 * <note>The coordinates of the returned box are not clamped to
 * integer pixel values, if you need them to be clamped you can use
 * rut_box_clamp_to_pixel()</note>
 */
void
_rut_volume_get_bounding_box(rut_volume_t *volume, rut_box_t *box)
{
    float x_min, y_min, x_max, y_max;
    rut_vector3_t *vertices;
    int count;
    int i;

    c_return_if_fail(volume != NULL);
    c_return_if_fail(box != NULL);

    if (volume->is_empty) {
        box->x1 = box->x2 = volume->vertices[0].x;
        box->y1 = box->y2 = volume->vertices[0].y;
        return;
    }

    /* Updates the vertices we calculate lazily
     * (See rut_volume_t typedef for more details) */
    _rut_volume_complete(volume);

    vertices = volume->vertices;

    x_min = x_max = vertices[0].x;
    y_min = y_max = vertices[0].y;

    /* Assuming that most objects are 2D we only have to look at the front 4
     * vertices of the volume... */
    if (C_LIKELY(volume->is_2d))
        count = 4;
    else
        count = 8;

    for (i = 1; i < count; i++) {
        if (vertices[i].x < x_min)
            x_min = vertices[i].x;
        else if (vertices[i].x > x_max)
            x_max = vertices[i].x;

        if (vertices[i].y < y_min)
            y_min = vertices[i].y;
        else if (vertices[i].y > y_max)
            y_max = vertices[i].y;
    }

    box->x1 = x_min;
    box->y1 = y_min;
    box->x2 = x_max;
    box->y2 = y_max;
}

void
_rut_volume_project(rut_volume_t *volume,
                    const c_matrix_t *modelview,
                    const c_matrix_t *projection,
                    const float *viewport)
{
    int transform_count;

    if (volume->is_empty) {
        /* Just transform the origin... */
        rut_util_fully_transform_vertices(modelview,
                                          projection,
                                          viewport,
                                          (float *)volume->vertices,
                                          (float *)volume->vertices,
                                          1);
        return;
    }

    /* All the vertices must be up to date, since after the projection
     * it wont be trivial to derive the other vertices. */
    _rut_volume_complete(volume);

    /* Most actors are 2D so we only have to transform the front 4
     * vertices of the volume... */
    if (C_LIKELY(volume->is_2d))
        transform_count = 4;
    else
        transform_count = 8;

    rut_util_fully_transform_vertices(modelview,
                                      projection,
                                      viewport,
                                      (float *)volume->vertices,
                                      (float *)volume->vertices,
                                      transform_count);

    volume->is_axis_aligned = false;
}

void
_rut_volume_transform(rut_volume_t *volume, const c_matrix_t *matrix)
{
    int transform_count;

    if (volume->is_empty) {
        float w = 1;
        /* Just transform the origin */
        c_matrix_transform_point(matrix,
                                  &volume->vertices[0].x,
                                  &volume->vertices[0].y,
                                  &volume->vertices[0].z,
                                  &w);
        return;
    }

    /* All the vertices must be up to date, since after the transform
     * it wont be trivial to derive the other vertices. */
    _rut_volume_complete(volume);

    /* Most actors are 2D so we only have to transform the front 4
     * vertices of the volume... */
    if (C_LIKELY(volume->is_2d))
        transform_count = 4;
    else
        transform_count = 8;

    c_matrix_transform_points(matrix,
                               3,
                               sizeof(rut_vector3_t),
                               volume->vertices,
                               sizeof(rut_vector3_t),
                               volume->vertices,
                               transform_count);

    volume->is_axis_aligned = false;
}

/* Given a volume that has been transformed by an arbitrary
 * modelview and is no longer axis aligned, this derives a replacement
 * that is axis aligned. */
void
rut_volume_axis_align(rut_volume_t *volume)
{
    int count;
    int i;
    rut_vector3_t origin;
    float max_x;
    float max_y;
    float max_z;

    c_return_if_fail(volume != NULL);

    if (volume->is_empty)
        return;

    if (C_LIKELY(volume->is_axis_aligned))
        return;

    if (C_LIKELY(volume->vertices[0].x == volume->vertices[1].x &&
                 volume->vertices[0].y == volume->vertices[3].y &&
                 volume->vertices[0].z == volume->vertices[4].y)) {
        volume->is_axis_aligned = true;
        return;
    }

    if (!volume->is_complete)
        _rut_volume_complete(volume);

    origin = volume->vertices[0];
    max_x = volume->vertices[0].x;
    max_y = volume->vertices[0].y;
    max_z = volume->vertices[0].z;

    count = volume->is_2d ? 4 : 8;
    for (i = 1; i < count; i++) {
        if (volume->vertices[i].x < origin.x)
            origin.x = volume->vertices[i].x;
        else if (volume->vertices[i].x > max_x)
            max_x = volume->vertices[i].x;

        if (volume->vertices[i].y < origin.y)
            origin.y = volume->vertices[i].y;
        else if (volume->vertices[i].y > max_y)
            max_y = volume->vertices[i].y;

        if (volume->vertices[i].z < origin.z)
            origin.z = volume->vertices[i].z;
        else if (volume->vertices[i].z > max_z)
            max_z = volume->vertices[i].z;
    }

    volume->vertices[0] = origin;

    volume->vertices[1].x = max_x;
    volume->vertices[1].y = origin.y;
    volume->vertices[1].z = origin.z;

    volume->vertices[3].x = origin.x;
    volume->vertices[3].y = max_y;
    volume->vertices[3].z = origin.z;

    volume->vertices[4].x = origin.x;
    volume->vertices[4].y = origin.y;
    volume->vertices[4].z = max_z;

    volume->is_complete = false;
    volume->is_axis_aligned = true;

    if (volume->vertices[4].z == volume->vertices[0].z)
        volume->is_2d = true;
    else
        volume->is_2d = false;
}

rut_cull_result_t
rut_volume_cull(rut_volume_t *volume,
                const rut_plane_t *planes)
{
    int vertex_count;
    rut_vector3_t *vertices = volume->vertices;
    bool partial = false;
    int i;
    int j;

    if (volume->is_empty)
        return RUT_CULL_RESULT_OUT;

    /* We expect the volume to already be transformed into eye coordinates
     */
    c_return_val_if_fail(volume->is_complete == true, RUT_CULL_RESULT_IN);

    /* Most actors are 2D so we only have to transform the front 4
     * vertices of the volume... */
    if (C_LIKELY(volume->is_2d))
        vertex_count = 4;
    else
        vertex_count = 8;

    for (i = 0; i < 4; i++) {
        int out = 0;
        for (j = 0; j < vertex_count; j++) {
            rut_vector3_t p;
            float distance;

            /* XXX: for perspective projections this can be optimized
             * out because all the planes should pass through the origin
             * so (0,0,0) is a valid v0. */
            p.x = vertices[j].x - planes[i].v0[0];
            p.y = vertices[j].y - planes[i].v0[1];
            p.z = vertices[j].z - planes[i].v0[2];

            distance = planes[i].n[0] * p.x + planes[i].n[1] * p.y +
                       planes[i].n[2] * p.z;

            if (distance < 0)
                out++;
        }

        if (out == vertex_count)
            return RUT_CULL_RESULT_OUT;
        else if (out != 0)
            partial = true;
    }

    if (partial)
        return RUT_CULL_RESULT_PARTIAL;
    else
        return RUT_CULL_RESULT_IN;
}

void
_rut_volume_get_stable_bounding_int_rectangle(rut_volume_t *volume,
                                              float *viewport,
                                              const c_matrix_t *projection,
                                              const c_matrix_t *modelview,
                                              rut_box_t *box)
{
    rut_volume_t projected_volume;
    float width;
    float height;

    _rut_volume_copy_static(volume, &projected_volume);

    _rut_volume_project(&projected_volume, modelview, projection, viewport);

    _rut_volume_get_bounding_box(&projected_volume, box);

    /* The aim here is that for a given rectangle defined with floating point
     * coordinates we want to determine a stable quantized size in pixels
     * that doesn't vary due to the original box's sub-pixel position.
     *
     * The reason this is important is because effects will use this
     * API to determine the size of offscreen framebuffers and so for
     * a fixed-size object that may be animated accross the screen we
     * want to make sure that the stage paint-box has an equally stable
     * size so that effects aren't made to continuously re-allocate
     * a corresponding fbo.
     *
     * The other thing we consider is that the calculation of this box is
     * subject to floating point precision issues that might be slightly
     * different to the precision issues involved with actually painting the
     * actor, which might result in painting slightly leaking outside the
     * user's calculated paint-volume. For this we simply aim to pad out the
     * paint-volume by at least half a pixel all the way around.
     */
    width = box->x2 - box->x1;
    height = box->y2 - box->y1;
    width = RUT_UTIL_NEARBYINT(width);
    height = RUT_UTIL_NEARBYINT(height);
    /* XXX: NB the width/height may now be up to 0.5px too small so we
     * must also pad by 0.25px all around to account for this. In total we
     * must padd by at least 0.75px around all sides. */

    /* XXX: The furthest that we can overshoot the bottom right corner by
     * here is 1.75px in total if you consider that the 0.75 padding could
     * just cross an integer boundary and so ceil will effectively add 1.
     */
    box->x2 = ceilf(box->x2 + 0.75);
    box->y2 = ceilf(box->y2 + 0.75);

    /* Now we redefine the top-left relative to the bottom right based on the
     * rounded width/height determined above + a constant so that the overall
     * size of the box will be stable and not dependant on the box's
     * position.
     *
     * Adding 3px to the width/height will ensure we cover the maximum of
     * 1.75px padding on the bottom/right and still ensure we have > 0.75px
     * padding on the top/left.
     */
    box->x1 = box->x2 - width - 3;
    box->y1 = box->y2 - height - 3;

    rut_volume_free(&projected_volume);
}
