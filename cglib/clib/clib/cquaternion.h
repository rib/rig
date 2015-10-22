/*
 * Copyright (C) 2010 Intel Corporation.
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

#pragma once

#include <clib.h>

C_BEGIN_DECLS

/**
 * SECTION:cquaternion
 * @short_description: API for initializing and manipulating
 * quaternions.
 *
 * Quaternions have become a standard form for representing 3D
 * rotations and have some nice properties when compared with other
 * representation such as (roll,pitch,yaw) Euler angles. They can be
 * used to interpolate between different rotations and they don't
 * suffer from a problem called
 * <ulink url="http://en.wikipedia.org/wiki/Gimbal_lock">"Gimbal lock"</ulink>
 * where two of the axis of rotation may become aligned and you loose a
 * degree of freedom.
 * .
 */

/**
 * c_quaternion_t:
 * @w: based on the angle of rotation it is cos(𝜃/2)
 * @x: based on the angle of rotation and x component of the axis of
 *     rotation it is sin(𝜃/2)*axis.x
 * @y: based on the angle of rotation and y component of the axis of
 *     rotation it is sin(𝜃/2)*axis.y
 * @z: based on the angle of rotation and z component of the axis of
 *     rotation it is sin(𝜃/2)*axis.z
 *
 * A quaternion is comprised of a scalar component and a 3D vector
 * component. The scalar component is normally referred to as w and the
 * vector might either be referred to as v or a (for axis) or expanded
 * with the individual components: (x, y, z) A full quaternion would
 * then be written as <literal>[w (x, y, z)]</literal>.
 *
 * Quaternions can be considered to represent an axis and angle
 * pair although these numbers are buried somewhat under some
 * maths...
 *
 * For the curious you can see here that a given axis (a) and angle (𝜃)
 * pair are represented in a quaternion as follows:
 * |[
 * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
 * ]|
 *
 * Unit Quaternions:
 * When using Quaternions to represent spatial orientations for 3D
 * graphics it's always assumed you have a unit quaternion. The
 * magnitude of a quaternion is defined as:
 * |[
 * sqrt (w² + x² + y² + z²)
 * ]|
 * and a unit quaternion satisfies this equation:
 * |[
 * w² + x² + y² + z² = 1
 * ]|
 *
 * Most of the time we don't have to worry about the maths involved
 * with quaternions but if you are curious to learn more here are some
 * external references:
 *
 * <itemizedlist>
 * <listitem>
 * <ulink url="http://mathworld.wolfram.com/Quaternion.html"/>
 * </listitem>
 * <listitem>
 * <ulink url="http://www.gamedev.net/reference/articles/article1095.asp"/>
 * </listitem>
 * <listitem>
 * <ulink url="http://www.cprogramming.com/tutorial/3d/quaternions.html"/>
 * </listitem>
 * <listitem>
 * <ulink
 * url="http://www.isner.com/tutorials/quatSpells/quaternion_spells_12.htm"/>
 * </listitem>
 * <listitem>
 * 3D Maths Primer for Graphics and Game Development ISBN-10: 1556229119
 * </listitem>
 * <listitem>
 * <ulink url="http://www.cs.caltech.edu/courses/cs171/quatut.pdf"/>
 * </listitem>
 * <listitem>
 * <ulink url="http://www.j3d.org/matrix_faq/matrfaq_latest.html#Q56"/>
 * </listitem>
 * </itemizedlist>
 *
 */
struct _c_quaternion_t {
    float w;

    float x;
    float y;
    float z;
};
_C_STRUCT_SIZE_ASSERT(c_quaternion_t, 16);

/**
 * c_quaternion_init:
 * @quaternion: An uninitialized #c_quaternion_t
 * @angle: The angle you want to rotate around the given axis
 * @x: The x component of your axis vector about which you want to
 * rotate.
 * @y: The y component of your axis vector about which you want to
 * rotate.
 * @z: The z component of your axis vector about which you want to
 * rotate.
 *
 * Initializes a quaternion that rotates @angle degrees around the
 * axis vector (@x, @y, @z). The axis vector does not need to be
 * normalized.
 *
 * Returns: A normalized, unit quaternion representing an orientation
 * rotated @angle degrees around the axis vector (@x, @y, @z)
 *
 */
void c_quaternion_init(
    c_quaternion_t *quaternion, float angle, float x, float y, float z);

/**
 * c_quaternion_init_from_angle_vector:
 * @quaternion: An uninitialized #c_quaternion_t
 * @angle: The angle to rotate around @axis3f
 * @axis3f: your 3 component axis vector about which you want to rotate.
 *
 * Initializes a quaternion that rotates @angle degrees around the
 * given @axis vector. The axis vector does not need to be
 * normalized.
 *
 * Returns: A normalized, unit quaternion representing an orientation
 * rotated @angle degrees around the given @axis vector.
 *
 */
void c_quaternion_init_from_angle_vector(c_quaternion_t *quaternion,
                                          float angle,
                                          const float *axis3f);

/**
 * c_quaternion_init_identity:
 * @quaternion: An uninitialized #c_quaternion_t
 *
 * Initializes the quaternion with the canonical quaternion identity
 * [1 (0, 0, 0)] which represents no rotation. Multiplying a
 * quaternion with this identity leaves the quaternion unchanged.
 *
 * You might also want to consider using
 * cg_get_static_identity_quaternion().
 *
 */
void c_quaternion_init_identity(c_quaternion_t *quaternion);

/**
 * c_quaternion_init_from_array:
 * @quaternion: A #c_quaternion_t
 * @array: An array of 4 floats w,(x,y,z)
 *
 * Initializes a [w (x, y,z)] quaternion directly from an array of 4
 * floats: [w,x,y,z].
 *
 */
void c_quaternion_init_from_array(c_quaternion_t *quaternion,
                                   const float *array);

/**
 * c_quaternion_init_from_x_rotation:
 * @quaternion: An uninitialized #c_quaternion_t
 * @angle: The angle to rotate around the x axis
 *
 * XXX: check which direction this rotates
 *
 */
void c_quaternion_init_from_x_rotation(c_quaternion_t *quaternion,
                                        float angle);

/**
 * c_quaternion_init_from_y_rotation:
 * @quaternion: An uninitialized #c_quaternion_t
 * @angle: The angle to rotate around the y axis
 *
 *
 */
void c_quaternion_init_from_y_rotation(c_quaternion_t *quaternion,
                                        float angle);

/**
 * c_quaternion_init_from_z_rotation:
 * @quaternion: An uninitialized #c_quaternion_t
 * @angle: The angle to rotate around the z axis
 *
 *
 */
void c_quaternion_init_from_z_rotation(c_quaternion_t *quaternion,
                                        float angle);

/**
 * c_quaternion_init_from_euler:
 * @quaternion: A #c_quaternion_t
 * @euler: A #c_euler_t with which to initialize the quaternion
 *
 */
void c_quaternion_init_from_euler(c_quaternion_t *quaternion,
                                   const c_euler_t *euler);

/**
 * c_quaternion_init_from_matrix:
 * @quaternion: A Quaternion
 * @matrix: A rotation matrix with which to initialize the quaternion
 *
 * Initializes a quaternion from a rotation matrix.
 *
 * Stability: unstable
 */
void c_quaternion_init_from_matrix(c_quaternion_t *quaternion,
                                    const c_matrix_t *matrix);

/**
 * c_quaternion_equal:
 * @v1: A #c_quaternion_t
 * @v2: A #c_quaternion_t
 *
 * Compares that all the components of quaternions @a and @b are
 * equal.
 *
 * An epsilon value is not used to compare the float components, but
 * the == operator is at least used so that 0 and -0 are considered
 * equal.
 *
 * Returns: %true if the quaternions are equal else %false.
 *
 */
bool c_quaternion_equal(const void *v1, const void *v2);

/**
 * c_quaternion_copy:
 * @src: A #c_quaternion_t
 *
 * Allocates a new #c_quaternion_t on the stack and initializes it with
 * the same values as @src.
 *
 * Returns: A newly allocated #c_quaternion_t which should be freed
 * using c_quaternion_free()
 *
 */
c_quaternion_t *c_quaternion_copy(const c_quaternion_t *src);

/**
 * c_quaternion_free:
 * @quaternion: A #c_quaternion_t
 *
 * Frees a #c_quaternion_t that was previously allocated via
 * c_quaternion_copy().
 *
 */
void c_quaternion_free(c_quaternion_t *quaternion);

/**
 * c_quaternion_get_rotation_angle:
 * @quaternion: A #c_quaternion_t
 *
 *
 */
float c_quaternion_get_rotation_angle(const c_quaternion_t *quaternion);

/**
 * c_quaternion_get_rotation_axis:
 * @quaternion: A #c_quaternion_t
 * @vector3: (out): an allocated 3-float array
 *
 */
void c_quaternion_get_rotation_axis(const c_quaternion_t *quaternion,
                                     float *vector3);

/**
 * c_quaternion_normalize:
 * @quaternion: A #c_quaternion_t
 *
 *
 */
void c_quaternion_normalize(c_quaternion_t *quaternion);

/**
 * c_quaternion_dot_product:
 * @a: A #c_quaternion_t
 * @b: A #c_quaternion_t
 *
 */
float c_quaternion_dot_product(const c_quaternion_t *a,
                                const c_quaternion_t *b);

/**
 * c_quaternion_invert:
 * @quaternion: A #c_quaternion_t
 *
 *
 */
void c_quaternion_invert(c_quaternion_t *quaternion);

/**
 * c_quaternion_multiply:
 * @result: The destination #c_quaternion_t
 * @left: The second #c_quaternion_t rotation to apply
 * @right: The first #c_quaternion_t rotation to apply
 *
 * This combines the rotations of two quaternions into @result. The
 * operation is not commutative so the order is important; AxB
 * != BxA. Clib follows the standard convention for quaternions here
 * so the rotations are applied @right to @left. This is similar to the
 * combining of matrices.
 *
 * <note>It is possible to multiply the @a quaternion in-place, so
 * @result can be equal to @a but can't be equal to @b.</note>
 *
 */
void c_quaternion_multiply(c_quaternion_t *result,
                            const c_quaternion_t *left,
                            const c_quaternion_t *right);

/**
 * c_quaternion_pow:
 * @quaternion: A #c_quaternion_t
 * @exponent: the exponent
 *
 *
 */
void c_quaternion_pow(c_quaternion_t *quaternion, float exponent);

/**
 * c_quaternion_slerp:
 * @result: The destination #c_quaternion_t
 * @a: The first #c_quaternion_t
 * @b: The second #c_quaternion_t
 * @t: The factor in the range [0,1] used to interpolate between
 * quaternion @a and @b.
 *
 * Performs a spherical linear interpolation between two quaternions.
 *
 * Noteable properties:
 * <itemizedlist>
 * <listitem>
 * commutative: No
 * </listitem>
 * <listitem>
 * constant velocity: Yes
 * </listitem>
 * <listitem>
 * torque minimal (travels along the surface of the 4-sphere): Yes
 * </listitem>
 * <listitem>
 * more expensive than c_quaternion_nlerp()
 * </listitem>
 * </itemizedlist>
 */
void c_quaternion_slerp(c_quaternion_t *result,
                         const c_quaternion_t *a,
                         const c_quaternion_t *b,
                         float t);

/**
 * c_quaternion_nlerp:
 * @result: The destination #c_quaternion_t
 * @a: The first #c_quaternion_t
 * @b: The second #c_quaternion_t
 * @t: The factor in the range [0,1] used to interpolate between
 * quaterion @a and @b.
 *
 * Performs a normalized linear interpolation between two quaternions.
 * That is it does a linear interpolation of the quaternion components
 * and then normalizes the result. This will follow the shortest arc
 * between the two orientations (just like the slerp() function) but
 * will not progress at a constant speed. Unlike slerp() nlerp is
 * commutative which is useful if you are blending animations
 * together. (I.e. nlerp (tmp, a, b) followed by nlerp (result, tmp,
 * d) is the same as nlerp (tmp, a, d) followed by nlerp (result, tmp,
 * b)). Finally nlerp is cheaper than slerp so it can be a good choice
 * if you don't need the constant speed property of the slerp() function.
 *
 * Notable properties:
 * <itemizedlist>
 * <listitem>
 * commutative: Yes
 * </listitem>
 * <listitem>
 * constant velocity: No
 * </listitem>
 * <listitem>
 * torque minimal (travels along the surface of the 4-sphere): Yes
 * </listitem>
 * <listitem>
 * faster than c_quaternion_slerp()
 * </listitem>
 * </itemizedlist>
 */
void c_quaternion_nlerp(c_quaternion_t *result,
                         const c_quaternion_t *a,
                         const c_quaternion_t *b,
                         float t);
/**
 * c_quaternion_squad:
 * @result: The destination #c_quaternion_t
 * @prev: A #c_quaternion_t used before @a
 * @a: The first #c_quaternion_t
 * @b: The second #c_quaternion_t
 * @next: A #c_quaternion_t that will be used after @b
 * @t: The factor in the range [0,1] used to interpolate between
 * quaternion @a and @b.
 *
 *
 */
void c_quaternion_squad(c_quaternion_t *result,
                         const c_quaternion_t *prev,
                         const c_quaternion_t *a,
                         const c_quaternion_t *b,
                         const c_quaternion_t *next,
                         float t);

/**
 * cg_get_static_identity_quaternion:
 *
 * Returns a pointer to a singleton quaternion constant describing the
 * canonical identity [1 (0, 0, 0)] which represents no rotation.
 *
 * If you multiply a quaternion with the identity quaternion you will
 * get back the same value as the original quaternion.
 *
 * Returns: A pointer to an identity quaternion
 *
 */
const c_quaternion_t *cg_get_static_identity_quaternion(void);

/**
 * cg_get_static_zero_quaternion:
 *
 * Returns: a pointer to a singleton quaternion constant describing a
 *          rotation of 180 degrees around a degenerate axis:
 *          [0 (0, 0, 0)]
 *
 */
const c_quaternion_t *cg_get_static_zero_quaternion(void);

C_END_DECLS
