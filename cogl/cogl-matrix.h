/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

#ifndef __CG_MATRIX_H
#define __CG_MATRIX_H

#include <cogl/cogl-defines.h>

#ifdef CG_HAS_GTYPE_SUPPORT
#include <glib-object.h>
#endif /* CG_HAS_GTYPE_SUPPORT */

#include "cogl-types.h"
#include "cogl-quaternion.h"

CG_BEGIN_DECLS

/**
 * SECTION:cogl-matrix
 * @short_description: Functions for initializing and manipulating 4x4 matrices
 *
 * Matrices are used in Cogl to describe affine model-view transforms, texture
 * transforms, and projective transforms. This exposes a utility API that can
 * be used for direct manipulation of these matrices.
 */

/**
 * cg_matrix_t:
 *
 * A cg_matrix_t holds a 4x4 transform matrix. This is a single precision,
 * column-major matrix which means it is compatible with what OpenGL expects.
 *
 * A cg_matrix_t can represent transforms such as, rotations, scaling,
 * translation, sheering, and linear projections. You can combine these
 * transforms by multiplying multiple matrices in the order you want them
 * applied.
 *
 * The transformation of a vertex (x, y, z, w) by a cg_matrix_t is given by:
 *
 * |[
 *   x_new = xx * x + xy * y + xz * z + xw * w
 *   y_new = yx * x + yy * y + yz * z + yw * w
 *   z_new = zx * x + zy * y + zz * z + zw * w
 *   w_new = wx * x + wy * y + wz * z + ww * w
 * ]|
 *
 * Where w is normally 1
 *
 * <note>You must consider the members of the cg_matrix_t structure read only,
 * and all matrix modifications must be done via the cg_matrix API. This
 * allows Cogl to annotate the matrices internally. Violation of this will give
 * undefined results. If you need to initialize a matrix with a constant other
 * than the identity matrix you can use cg_matrix_init_from_array().</note>
 */
struct _cg_matrix_t {
    /* column 0 */
    float xx;
    float yx;
    float zx;
    float wx;

    /* column 1 */
    float xy;
    float yy;
    float zy;
    float wy;

    /* column 2 */
    float xz;
    float yz;
    float zz;
    float wz;

    /* column 3 */
    float xw;
    float yw;
    float zw;
    float ww;

    /*< private >*/
    unsigned int CG_PRIVATE(type) : 16;
    unsigned int CG_PRIVATE(flags) : 16;
};
CG_STRUCT_SIZE_ASSERT(cg_matrix_t, sizeof(float) * 16 + 4);

/**
 * cg_matrix_init_identity:
 * @matrix: A 4x4 transformation matrix
 *
 * Resets matrix to the identity matrix:
 *
 * |[
 *   .xx=1; .xy=0; .xz=0; .xw=0;
 *   .yx=0; .yy=1; .yz=0; .yw=0;
 *   .zx=0; .zy=0; .zz=1; .zw=0;
 *   .wx=0; .wy=0; .wz=0; .ww=1;
 * ]|
 */
void cg_matrix_init_identity(cg_matrix_t *matrix);

/**
 * cg_matrix_init_translation:
 * @matrix: A 4x4 transformation matrix
 * @tx: x coordinate of the translation vector
 * @ty: y coordinate of the translation vector
 * @tz: z coordinate of the translation vector
 *
 * Resets matrix to the (tx, ty, tz) translation matrix:
 *
 * |[
 *   .xx=1; .xy=0; .xz=0; .xw=tx;
 *   .yx=0; .yy=1; .yz=0; .yw=ty;
 *   .zx=0; .zy=0; .zz=1; .zw=tz;
 *   .wx=0; .wy=0; .wz=0; .ww=1;
 * ]|
 *
 * Since: 2.0
 */
void
cg_matrix_init_translation(cg_matrix_t *matrix, float tx, float ty, float tz);

/**
 * cg_matrix_multiply:
 * @result: The address of a 4x4 matrix to store the result in
 * @a: A 4x4 transformation matrix
 * @b: A 4x4 transformation matrix
 *
 * Multiplies the two supplied matrices together and stores
 * the resulting matrix inside @result.
 *
 * <note>It is possible to multiply the @a matrix in-place, so
 * @result can be equal to @a but can't be equal to @b.</note>
 */
void cg_matrix_multiply(cg_matrix_t *result,
                        const cg_matrix_t *a,
                        const cg_matrix_t *b);

/**
 * cg_matrix_rotate:
 * @matrix: A 4x4 transformation matrix
 * @angle: The angle you want to rotate in degrees
 * @x: X component of your rotation vector
 * @y: Y component of your rotation vector
 * @z: Z component of your rotation vector
 *
 * Multiplies @matrix with a rotation matrix that applies a rotation
 * of @angle degrees around the specified 3D vector.
 */
void
cg_matrix_rotate(cg_matrix_t *matrix, float angle, float x, float y, float z);

/**
 * cg_matrix_rotate_quaternion:
 * @matrix: A 4x4 transformation matrix
 * @quaternion: A quaternion describing a rotation
 *
 * Multiplies @matrix with a rotation transformation described by the
 * given #cg_quaternion_t.
 *
 * Since: 2.0
 */
void cg_matrix_rotate_quaternion(cg_matrix_t *matrix,
                                 const cg_quaternion_t *quaternion);

/**
 * cg_matrix_rotate_euler:
 * @matrix: A 4x4 transformation matrix
 * @euler: A euler describing a rotation
 *
 * Multiplies @matrix with a rotation transformation described by the
 * given #cg_euler_t.
 *
 * Since: 2.0
 */
void cg_matrix_rotate_euler(cg_matrix_t *matrix, const cg_euler_t *euler);

/**
 * cg_matrix_translate:
 * @matrix: A 4x4 transformation matrix
 * @x: The X translation you want to apply
 * @y: The Y translation you want to apply
 * @z: The Z translation you want to apply
 *
 * Multiplies @matrix with a transform matrix that translates along
 * the X, Y and Z axis.
 */
void cg_matrix_translate(cg_matrix_t *matrix, float x, float y, float z);

/**
 * cg_matrix_scale:
 * @matrix: A 4x4 transformation matrix
 * @sx: The X scale factor
 * @sy: The Y scale factor
 * @sz: The Z scale factor
 *
 * Multiplies @matrix with a transform matrix that scales along the X,
 * Y and Z axis.
 */
void cg_matrix_scale(cg_matrix_t *matrix, float sx, float sy, float sz);

/**
 * cg_matrix_look_at:
 * @matrix: A 4x4 transformation matrix
 * @eye_position_x: The X coordinate to look from
 * @eye_position_y: The Y coordinate to look from
 * @eye_position_z: The Z coordinate to look from
 * @object_x: The X coordinate of the object to look at
 * @object_y: The Y coordinate of the object to look at
 * @object_z: The Z coordinate of the object to look at
 * @world_up_x: The X component of the world's up direction vector
 * @world_up_y: The Y component of the world's up direction vector
 * @world_up_z: The Z component of the world's up direction vector
 *
 * Applies a view transform @matrix that positions the camera at
 * the coordinate (@eye_position_x, @eye_position_y, @eye_position_z)
 * looking towards an object at the coordinate (@object_x, @object_y,
 * @object_z). The top of the camera is aligned to the given world up
 * vector, which is normally simply (0, 1, 0) to map up to the
 * positive direction of the y axis.
 *
 * Because there is a lot of missleading documentation online for
 * gluLookAt regarding the up vector we want to try and be a bit
 * clearer here.
 *
 * The up vector should simply be relative to your world coordinates
 * and does not need to change as you move the eye and object
 * positions.  Many online sources may claim that the up vector needs
 * to be perpendicular to the vector between the eye and object
 * position (partly because the man page is somewhat missleading) but
 * that is not necessary for this function.
 *
 * <note>You should never look directly along the world-up
 * vector.</note>
 *
 * <note>It is assumed you are using a typical projection matrix where
 * your origin maps to the center of your viewport.</note>
 *
 * <note>Almost always when you use this function it should be the first
 * transform applied to a new modelview transform</note>
 *
 * Since: 1.8
 * Stability: unstable
 */
void cg_matrix_look_at(cg_matrix_t *matrix,
                       float eye_position_x,
                       float eye_position_y,
                       float eye_position_z,
                       float object_x,
                       float object_y,
                       float object_z,
                       float world_up_x,
                       float world_up_y,
                       float world_up_z);

/**
 * cg_matrix_frustum:
 * @matrix: A 4x4 transformation matrix
 * @left: X position of the left clipping plane where it
 *   intersects the near clipping plane
 * @right: X position of the right clipping plane where it
 *   intersects the near clipping plane
 * @bottom: Y position of the bottom clipping plane where it
 *   intersects the near clipping plane
 * @top: Y position of the top clipping plane where it intersects
 *   the near clipping plane
 * @z_near: The distance to the near clipping plane (Must be positive)
 * @z_far: The distance to the far clipping plane (Must be positive)
 *
 * Multiplies @matrix by the given frustum perspective matrix.
 */
void cg_matrix_frustum(cg_matrix_t *matrix,
                       float left,
                       float right,
                       float bottom,
                       float top,
                       float z_near,
                       float z_far);

/**
 * cg_matrix_perspective:
 * @matrix: A 4x4 transformation matrix
 * @fov_y: Vertical field of view angle in degrees.
 * @aspect: The (width over height) aspect ratio for display
 * @z_near: The distance to the near clipping plane (Must be positive,
 *   and must not be 0)
 * @z_far: The distance to the far clipping plane (Must be positive)
 *
 * Multiplies @matrix by the described perspective matrix
 *
 * <note>You should be careful not to have to great a @z_far / @z_near
 * ratio since that will reduce the effectiveness of depth testing
 * since there wont be enough precision to identify the depth of
 * objects near to each other.</note>
 */
void cg_matrix_perspective(
    cg_matrix_t *matrix, float fov_y, float aspect, float z_near, float z_far);

/**
 * cg_matrix_orthographic:
 * @matrix: A 4x4 transformation matrix
 * @x_1: The x coordinate for the first vertical clipping plane
 * @y_1: The y coordinate for the first horizontal clipping plane
 * @x_2: The x coordinate for the second vertical clipping plane
 * @y_2: The y coordinate for the second horizontal clipping plane
 * @near: The <emphasis>distance</emphasis> to the near clipping
 *   plane (will be <emphasis>negative</emphasis> if the plane is
 *   behind the viewer)
 * @far: The <emphasis>distance</emphasis> to the far clipping
 *   plane (will be <emphasis>negative</emphasis> if the plane is
 *   behind the viewer)
 *
 * Multiplies @matrix by a parallel projection matrix.
 *
 * Since: 1.10
 * Stability: unstable
 */
void cg_matrix_orthographic(cg_matrix_t *matrix,
                            float x_1,
                            float y_1,
                            float x_2,
                            float y_2,
                            float near,
                            float far);

/**
 * cg_matrix_view_2d_in_frustum:
 * @matrix: A 4x4 transformation matrix
 * @left: coord of left vertical clipping plane
 * @right: coord of right vertical clipping plane
 * @bottom: coord of bottom horizontal clipping plane
 * @top: coord of top horizontal clipping plane
 * @z_near: The distance to the near clip plane. Never pass 0 and always pass
 *   a positive number.
 * @z_2d: The distance to the 2D plane. (Should always be positive and
 *   be between @z_near and the z_far value that was passed to
 *   cg_matrix_frustum())
 * @width_2d: The width of the 2D coordinate system
 * @height_2d: The height of the 2D coordinate system
 *
 * Multiplies @matrix by a view transform that maps the 2D coordinates
 * (0,0) top left and (@width_2d,@height_2d) bottom right the full viewport
 * size. Geometry at a depth of 0 will now lie on this 2D plane.
 *
 * Note: this doesn't multiply the matrix by any projection matrix,
 * but it assumes you have a perspective projection as defined by
 * passing the corresponding arguments to cg_matrix_frustum().

 * Toolkits such as Clutter that mix 2D and 3D drawing can use this to
 * create a 2D coordinate system within a 3D perspective projected
 * view frustum.
 *
 * Since: 1.8
 * Stability: unstable
 */
void cg_matrix_view_2d_in_frustum(cg_matrix_t *matrix,
                                  float left,
                                  float right,
                                  float bottom,
                                  float top,
                                  float z_near,
                                  float z_2d,
                                  float width_2d,
                                  float height_2d);

/**
 * cg_matrix_view_2d_in_perspective:
 * @fov_y: A field of view angle for the Y axis
 * @aspect: The ratio of width to height determining the field of view angle
 *   for the x axis.
 * @z_near: The distance to the near clip plane. Never pass 0 and always pass
 *   a positive number.
 * @z_2d: The distance to the 2D plane. (Should always be positive and
 *   be between @z_near and the z_far value that was passed to
 *   cg_matrix_frustum())
 * @width_2d: The width of the 2D coordinate system
 * @height_2d: The height of the 2D coordinate system
 *
 * Multiplies @matrix by a view transform that maps the 2D coordinates
 * (0,0) top left and (@width_2d,@height_2d) bottom right the full viewport
 * size. Geometry at a depth of 0 will now lie on this 2D plane.
 *
 * Note: this doesn't multiply the matrix by any projection matrix,
 * but it assumes you have a perspective projection as defined by
 * passing the corresponding arguments to cg_matrix_perspective().
 *
 * Toolkits such as Clutter that mix 2D and 3D drawing can use this to
 * create a 2D coordinate system within a 3D perspective projected
 * view frustum.
 *
 * Since: 1.8
 * Stability: unstable
 */
void cg_matrix_view_2d_in_perspective(cg_matrix_t *matrix,
                                      float fov_y,
                                      float aspect,
                                      float z_near,
                                      float z_2d,
                                      float width_2d,
                                      float height_2d);

/**
 * cg_matrix_init_from_array:
 * @matrix: A 4x4 transformation matrix
 * @array: A linear array of 16 floats (column-major order)
 *
 * Initializes @matrix with the contents of @array
 */
void cg_matrix_init_from_array(cg_matrix_t *matrix, const float *array);

/**
 * cg_matrix_get_array:
 * @matrix: A 4x4 transformation matrix
 *
 * Casts @matrix to a float array which can be directly passed to OpenGL.
 *
 * Return value: a pointer to the float array
 */
const float *cg_matrix_get_array(const cg_matrix_t *matrix);

/**
 * cg_matrix_init_from_quaternion:
 * @matrix: A 4x4 transformation matrix
 * @quaternion: A #cg_quaternion_t
 *
 * Initializes @matrix from a #cg_quaternion_t rotation.
 */
void cg_matrix_init_from_quaternion(cg_matrix_t *matrix,
                                    const cg_quaternion_t *quaternion);

/**
 * cg_matrix_init_from_euler:
 * @matrix: A 4x4 transformation matrix
 * @euler: A #cg_euler_t
 *
 * Initializes @matrix from a #cg_euler_t rotation.
 */
void cg_matrix_init_from_euler(cg_matrix_t *matrix, const cg_euler_t *euler);

/**
 * cg_matrix_equal:
 * @v1: A 4x4 transformation matrix
 * @v2: A 4x4 transformation matrix
 *
 * Compares two matrices to see if they represent the same
 * transformation. Although internally the matrices may have different
 * annotations associated with them and may potentially have a cached
 * inverse matrix these are not considered in the comparison.
 *
 * Since: 1.4
 */
bool cg_matrix_equal(const void *v1, const void *v2);

/**
 * cg_matrix_copy:
 * @matrix: A 4x4 transformation matrix you want to copy
 *
 * Allocates a new #cg_matrix_t on the heap and initializes it with
 * the same values as @matrix.
 *
 * Return value: (transfer full): A newly allocated #cg_matrix_t which
 * should be freed using cg_matrix_free()
 *
 * Since: 1.6
 */
cg_matrix_t *cg_matrix_copy(const cg_matrix_t *matrix);

/**
 * cg_matrix_free:
 * @matrix: A 4x4 transformation matrix you want to free
 *
 * Frees a #cg_matrix_t that was previously allocated via a call to
 * cg_matrix_copy().
 *
 * Since: 1.6
 */
void cg_matrix_free(cg_matrix_t *matrix);

/**
 * cg_matrix_get_inverse:
 * @matrix: A 4x4 transformation matrix
 * @inverse: (out): The destination for a 4x4 inverse transformation matrix
 *
 * Gets the inverse transform of a given matrix and uses it to initialize
 * a new #cg_matrix_t.
 *
 * <note>Although the first parameter is annotated as const to indicate
 * that the transform it represents isn't modified this function may
 * technically save a copy of the inverse transform within the given
 * #cg_matrix_t so that subsequent requests for the inverse transform may
 * avoid costly inversion calculations.</note>
 *
 * Return value: %true if the inverse was successfully calculated or %false
 *   for degenerate transformations that can't be inverted (in this case the
 *   @inverse matrix will simply be initialized with the identity matrix)
 *
 * Since: 1.2
 */
bool cg_matrix_get_inverse(const cg_matrix_t *matrix, cg_matrix_t *inverse);

/* FIXME: to be consistent with cg_matrix_{transform,project}_points
 * this could be renamed to cg_matrix_project_point for Cogl 2.0...
 */

/**
 * cg_matrix_transform_point:
 * @matrix: A 4x4 transformation matrix
 * @x: (inout): The X component of your points position
 * @y: (inout): The Y component of your points position
 * @z: (inout): The Z component of your points position
 * @w: (inout): The W component of your points position
 *
 * Transforms a point whos position is given and returned as four float
 * components.
 */
void cg_matrix_transform_point(
    const cg_matrix_t *matrix, float *x, float *y, float *z, float *w);

/**
 * cg_matrix_transform_points:
 * @matrix: A transformation matrix
 * @n_components: The number of position components for each input point.
 *                (either 2 or 3)
 * @stride_in: The stride in bytes between input points.
 * @points_in: A pointer to the first component of the first input point.
 * @stride_out: The stride in bytes between output points.
 * @points_out: A pointer to the first component of the first output point.
 * @n_points: The number of points to transform.
 *
 * Transforms an array of input points and writes the result to
 * another array of output points. The input points can either have 2
 * or 3 components each. The output points always have 3 components.
 * The output array can simply point to the input array to do the
 * transform in-place.
 *
 * If you need to transform 4 component points see
 * cg_matrix_project_points().
 *
 * Here's an example with differing input/output strides:
 * |[
 * typedef struct {
 *   float x,y;
 *   uint8_t r,g,b,a;
 *   float s,t,p;
 * } MyInVertex;
 * typedef struct {
 *   uint8_t r,g,b,a;
 *   float x,y,z;
 * } MyOutVertex;
 * MyInVertex vertices[N_VERTICES];
 * MyOutVertex results[N_VERTICES];
 * cg_matrix_t matrix;
 *
 * my_load_vertices (vertices);
 * my_get_matrix (&matrix);
 *
 * cg_matrix_transform_points (&matrix,
 *                               2,
 *                               sizeof (MyInVertex),
 *                               &vertices[0].x,
 *                               sizeof (MyOutVertex),
 *                               &results[0].x,
 *                               N_VERTICES);
 * ]|
 *
 * Stability: unstable
 */
void cg_matrix_transform_points(const cg_matrix_t *matrix,
                                int n_components,
                                size_t stride_in,
                                const void *points_in,
                                size_t stride_out,
                                void *points_out,
                                int n_points);

/**
 * cg_matrix_project_points:
 * @matrix: A projection matrix
 * @n_components: The number of position components for each input point.
 *                (either 2, 3 or 4)
 * @stride_in: The stride in bytes between input points.
 * @points_in: A pointer to the first component of the first input point.
 * @stride_out: The stride in bytes between output points.
 * @points_out: A pointer to the first component of the first output point.
 * @n_points: The number of points to transform.
 *
 * Projects an array of input points and writes the result to another
 * array of output points. The input points can either have 2, 3 or 4
 * components each. The output points always have 4 components (known
 * as homogenous coordinates). The output array can simply point to
 * the input array to do the transform in-place.
 *
 * Here's an example with differing input/output strides:
 * |[
 * typedef struct {
 *   float x,y;
 *   uint8_t r,g,b,a;
 *   float s,t,p;
 * } MyInVertex;
 * typedef struct {
 *   uint8_t r,g,b,a;
 *   float x,y,z;
 * } MyOutVertex;
 * MyInVertex vertices[N_VERTICES];
 * MyOutVertex results[N_VERTICES];
 * cg_matrix_t matrix;
 *
 * my_load_vertices (vertices);
 * my_get_matrix (&matrix);
 *
 * cg_matrix_project_points (&matrix,
 *                             2,
 *                             sizeof (MyInVertex),
 *                             &vertices[0].x,
 *                             sizeof (MyOutVertex),
 *                             &results[0].x,
 *                             N_VERTICES);
 * ]|
 *
 * Stability: unstable
 */
void cg_matrix_project_points(const cg_matrix_t *matrix,
                              int n_components,
                              size_t stride_in,
                              const void *points_in,
                              size_t stride_out,
                              void *points_out,
                              int n_points);

/**
 * cg_matrix_is_identity:
 * @matrix: A #cg_matrix_t
 *
 * Determines if the given matrix is an identity matrix.
 *
 * Returns: %true if @matrix is an identity matrix else %false
 * Since: 1.8
 */
bool cg_matrix_is_identity(const cg_matrix_t *matrix);

/**
 * cg_matrix_transpose:
 * @matrix: A #cg_matrix_t
 *
 * Replaces @matrix with its transpose. Ie, every element (i,j) in the
 * new matrix is taken from element (j,i) in the old matrix.
 *
 * Since: 1.10
 */
void cg_matrix_transpose(cg_matrix_t *matrix);

/**
 * cg_debug_matrix_print:
 * @matrix: A #cg_matrix_t
 *
 * Prints the contents of a #cg_matrix_t to stdout.
 *
 * Since: 2.0
 */
void cg_debug_matrix_print(const cg_matrix_t *matrix);

#ifdef CG_HAS_GTYPE_SUPPORT

#define CG_GTYPE_TYPE_MATRIX (cg_gtype_matrix_get_type())

/**
 * cg_gtype_matrix_get_type:
 *
 * Returns: the GType for the registered "cg_matrix_t" boxed type. This
 * can be used for example to define GObject properties that accept a
 * #cg_matrix_t value.
 */
GType cg_gtype_matrix_get_type(void);

#endif /* CG_HAS_GTYPE_SUPPORT */

CG_END_DECLS

#endif /* __CG_MATRIX_H */
