/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009,2010,2012 Intel Corporation.
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
 *   Havoc Pennington <hp@pobox.com> for litl
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef _CG_MATRIX_STACK_H_
#define _CG_MATRIX_STACK_H_

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#include "cg-device.h"

/**
 * SECTION:cg-matrix-stack
 * @short_description: Functions for efficiently tracking many
 *                     related transformations
 *
 * Matrices can be used (for example) to describe the model-view
 * transforms of objects, texture transforms, and projective
 * transforms.
 *
 * The #c_matrix_t api provides a good way to manipulate individual
 * matrices representing a single transformation but if you need to
 * track many-many such transformations for many objects that are
 * organized in a scenegraph for example then using a separate
 * #c_matrix_t for each object may not be the most efficient way.
 *
 * A #cg_matrix_stack_t enables applications to track lots of
 * transformations that are related to each other in some kind of
 * hierarchy.  In a scenegraph for example if you want to know how to
 * transform a particular node then you usually have to walk up
 * through the ancestors and accumulate their transforms before
 * finally applying the transform of the node itself. In this model
 * things are grouped together spatially according to their ancestry
 * and all siblings with the same parent share the same initial
 * transformation. The #cg_matrix_stack_t API is suited to tracking lots
 * of transformations that fit this kind of model.
 *
 * Compared to using the #c_matrix_t api directly to track many
 * related transforms, these can be some advantages to using a
 * #cg_matrix_stack_t:
 * <itemizedlist>
 *   <listitem>Faster equality comparisons of transformations</listitem>
 *   <listitem>Efficient comparisons of the differences between arbitrary
 *   transformations</listitem>
 *   <listitem>Avoid redundant arithmetic related to common transforms
 *   </listitem>
 *   <listitem>Can be more space efficient (not always though)</listitem>
 * </itemizedlist>
 *
 * For reference (to give an idea of when a #cg_matrix_stack_t can
 * provide a space saving) a #c_matrix_t can be expected to take 72
 * bytes whereas a single #cg_matrix_entry_t in a #cg_matrix_stack_t is
 * currently around 32 bytes on a 32bit CPU or 36 bytes on a 64bit
 * CPU. An entry is needed for each individual operation applied to
 * the stack (such as rotate, scale, translate) so if most of your
 * leaf node transformations only need one or two simple operations
 * relative to their parent then a matrix stack will likely take less
 * space than having a #c_matrix_t for each node.
 *
 * Even without any space saving though the ability to perform fast
 * comparisons and avoid redundant arithmetic (especially sine and
 * cosine calculations for rotations) can make using a matrix stack
 * worthwhile.
 */

/**
 * cg_matrix_stack_t:
 *
 * Tracks your current position within a hierarchy and lets you build
 * up a graph of transformations as you traverse through a hierarchy
 * such as a scenegraph.
 *
 * A #cg_matrix_stack_t always maintains a reference to a single
 * transformation at any point in time, representing the
 * transformation at the current position in the hierarchy. You can
 * get a reference to the current transformation by calling
 * cg_matrix_stack_get_entry().
 *
 * When a #cg_matrix_stack_t is first created with
 * cg_matrix_stack_new() then it is conceptually positioned at the
 * root of your hierarchy and the current transformation simply
 * represents an identity transformation.
 *
 * As you traverse your object hierarchy (your scenegraph) then you
 * should call cg_matrix_stack_push() whenever you move down one
 * level and call cg_matrix_stack_pop() whenever you move back up
 * one level towards the root.
 *
 * At any time you can apply a set of operations, such as "rotate",
 * "scale", "translate" on top of the current transformation of a
 * #cg_matrix_stack_t using functions such as
 * cg_matrix_stack_rotate(), cg_matrix_stack_scale() and
 * cg_matrix_stack_translate(). These operations will derive a new
 * current transformation and will never affect a transformation
 * that you have referenced using cg_matrix_stack_get_entry().
 *
 * Internally applying operations to a #cg_matrix_stack_t builds up a
 * graph of #cg_matrix_entry_t structures which each represent a single
 * immutable transform.
 */
typedef struct _cg_matrix_stack_t cg_matrix_stack_t;

/**
 * cg_matrix_entry_t:
 *
 * Represents a single immutable transformation that was retrieved
 * from a #cg_matrix_stack_t using cg_matrix_stack_get_entry().
 *
 * Internally a #cg_matrix_entry_t represents a single matrix
 * operation (such as "rotate", "scale", "translate") which is applied
 * to the transform of a single parent entry.
 *
 * Using the #cg_matrix_stack_t api effectively builds up a graph of
 * these immutable #cg_matrix_entry_t structures whereby operations
 * that can be shared between multiple transformations will result
 * in shared #cg_matrix_entry_t nodes in the graph.
 *
 * When a #cg_matrix_stack_t is first created it references one
 * #cg_matrix_entry_t that represents a single "load identity"
 * operation. This serves as the root entry and all operations
 * that are then applied to the stack will extend the graph
 * starting from this root "load identity" entry.
 *
 * Given the typical usage model for a #cg_matrix_stack_t and the way
 * the entries are built up while traversing a scenegraph then in most
 * cases where an application is interested in comparing two
 * transformations for equality then it is enough to simply compare
 * two #cg_matrix_entry_t pointers directly. Technically this can lead
 * to false negatives that could be identified with a deeper
 * comparison but often these false negatives are unlikely and
 * don't matter anyway so this enables extremely cheap comparisons.
 *
 * <note>#cg_matrix_entry_t<!-- -->s are reference counted using
 * cg_matrix_entry_ref() and cg_matrix_entry_unref() not with
 * cg_object_ref() and cg_object_unref().</note>
 */
typedef struct _cg_matrix_entry_t cg_matrix_entry_t;

/**
 * cg_matrix_stack_new:
 * @dev: A #cg_device_t
 *
 * Allocates a new #cg_matrix_stack_t that can be used to build up
 * transformations relating to objects in a scenegraph like hierarchy.
 * (See the description of #cg_matrix_stack_t and #cg_matrix_entry_t for
 * more details of what a matrix stack is best suited for)
 *
 * When a #cg_matrix_stack_t is first allocated it is conceptually
 * positioned at the root of your scenegraph hierarchy. As you
 * traverse your scenegraph then you should call
 * cg_matrix_stack_push() whenever you move down a level and
 * cg_matrix_stack_pop() whenever you move back up a level towards
 * the root.
 *
 * Once you have allocated a #cg_matrix_stack_t you can get a reference
 * to the current transformation for the current position in the
 * hierarchy by calling cg_matrix_stack_get_entry().
 *
 * Once you have allocated a #cg_matrix_stack_t you can apply operations
 * such as rotate, scale and translate to modify the current transform
 * for the current position in the hierarchy by calling
 * cg_matrix_stack_rotate(), cg_matrix_stack_scale() and
 * cg_matrix_stack_translate().
 *
 * Return value: (transfer full): A newly allocated #cg_matrix_stack_t
 */
cg_matrix_stack_t *cg_matrix_stack_new(cg_device_t *dev);

/**
 * cg_matrix_stack_push:
 * @stack: A #cg_matrix_stack_t
 *
 * Saves the current transform and starts a new transform that derives
 * from the current transform.
 *
 * This is usually called while traversing a scenegraph whenever you
 * traverse one level deeper. cg_matrix_stack_pop() can then be
 * called when going back up one layer to restore the previous
 * transform of an ancestor.
 */
void cg_matrix_stack_push(cg_matrix_stack_t *stack);

/**
 * cg_matrix_stack_pop:
 * @stack: A #cg_matrix_stack_t
 *
 * Restores the previous transform that was last saved by calling
 * cg_matrix_stack_push().
 *
 * This is usually called while traversing a scenegraph whenever you
 * return up one level in the graph towards the root node.
 */
void cg_matrix_stack_pop(cg_matrix_stack_t *stack);

/**
 * cg_matrix_stack_load_identity:
 * @stack: A #cg_matrix_stack_t
 *
 * Resets the current matrix to the identity matrix.
 */
void cg_matrix_stack_load_identity(cg_matrix_stack_t *stack);

/**
 * cg_matrix_stack_scale:
 * @stack: A #cg_matrix_stack_t
 * @x: Amount to scale along the x-axis
 * @y: Amount to scale along the y-axis
 * @z: Amount to scale along the z-axis
 *
 * Multiplies the current matrix by one that scales the x, y and z
 * axes by the given values.
 */
void cg_matrix_stack_scale(cg_matrix_stack_t *stack, float x, float y, float z);

/**
 * cg_matrix_stack_translate:
 * @stack: A #cg_matrix_stack_t
 * @x: Distance to translate along the x-axis
 * @y: Distance to translate along the y-axis
 * @z: Distance to translate along the z-axis
 *
 * Multiplies the current matrix by one that translates along all
 * three axes according to the given values.
 */
void
cg_matrix_stack_translate(cg_matrix_stack_t *stack, float x, float y, float z);

/**
 * cg_matrix_stack_rotate:
 * @stack: A #cg_matrix_stack_t
 * @angle: Angle in degrees to rotate.
 * @x: X-component of vertex to rotate around.
 * @y: Y-component of vertex to rotate around.
 * @z: Z-component of vertex to rotate around.
 *
 * Multiplies the current matrix by one that rotates the around the
 * axis-vector specified by @x, @y and @z. The rotation follows the
 * right-hand thumb rule so for example rotating by 10 degrees about
 * the axis-vector (0, 0, 1) causes a small counter-clockwise
 * rotation.
 */
void cg_matrix_stack_rotate(
    cg_matrix_stack_t *stack, float angle, float x, float y, float z);

/**
 * cg_matrix_stack_rotate_quaternion:
 * @stack: A #cg_matrix_stack_t
 * @quaternion: A #c_quaternion_t
 *
 * Multiplies the current matrix by one that rotates according to the
 * rotation described by @quaternion.
 */
void cg_matrix_stack_rotate_quaternion(cg_matrix_stack_t *stack,
                                       const c_quaternion_t *quaternion);

/**
 * cg_matrix_stack_rotate_euler:
 * @stack: A #cg_matrix_stack_t
 * @euler: A #c_euler_t
 *
 * Multiplies the current matrix by one that rotates according to the
 * rotation described by @euler.
 */
void cg_matrix_stack_rotate_euler(cg_matrix_stack_t *stack,
                                  const c_euler_t *euler);

/**
 * cg_matrix_stack_multiply:
 * @stack: A #cg_matrix_stack_t
 * @matrix: the matrix to multiply with the current model-view
 *
 * Multiplies the current matrix by the given matrix.
 */
void cg_matrix_stack_multiply(cg_matrix_stack_t *stack,
                              const c_matrix_t *matrix);

/**
 * cg_matrix_stack_frustum:
 * @stack: A #cg_matrix_stack_t
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
 * Replaces the current matrix with a perspective matrix for a given
 * viewing frustum defined by 4 side clip planes that all cross
 * through the origin and 2 near and far clip planes.
 */
void cg_matrix_stack_frustum(cg_matrix_stack_t *stack,
                             float left,
                             float right,
                             float bottom,
                             float top,
                             float z_near,
                             float z_far);

/**
 * cg_matrix_stack_perspective:
 * @stack: A #cg_matrix_stack_t
 * @fov_y: Vertical field of view angle in degrees.
 * @aspect: The (width over height) aspect ratio for display
 * @z_near: The distance to the near clipping plane (Must be positive,
 *   and must not be 0)
 * @z_far: The distance to the far clipping plane (Must be positive)
 *
 * Replaces the current matrix with a perspective matrix based on the
 * provided values.
 *
 * <note>You should be careful not to have too great a @z_far / @z_near
 * ratio since that will reduce the effectiveness of depth testing
 * since there wont be enough precision to identify the depth of
 * objects near to each other.</note>
 */
void cg_matrix_stack_perspective(cg_matrix_stack_t *stack,
                                 float fov_y,
                                 float aspect,
                                 float z_near,
                                 float z_far);

/**
 * cg_matrix_stack_orthographic:
 * @stack: A #cg_matrix_stack_t
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
 * Replaces the current matrix with an orthographic projection matrix.
 */
void cg_matrix_stack_orthographic(cg_matrix_stack_t *stack,
                                  float x_1,
                                  float y_1,
                                  float x_2,
                                  float y_2,
                                  float near,
                                  float far);

/**
 * cg_matrix_stack_get_inverse:
 * @stack: A #cg_matrix_stack_t
 * @inverse: (out): The destination for a 4x4 inverse transformation matrix
 *
 * Gets the inverse transform of the current matrix and uses it to
 * initialize a new #c_matrix_t.
 *
 * Return value: %true if the inverse was successfully calculated or %false
 *   for degenerate transformations that can't be inverted (in this case the
 *   @inverse matrix will simply be initialized with the identity matrix)
 */
bool cg_matrix_stack_get_inverse(cg_matrix_stack_t *stack,
                                 c_matrix_t *inverse);

/**
 * cg_matrix_stack_get_entry:
 * @stack: A #cg_matrix_stack_t
 *
 * Gets a reference to the current transform represented by a
 * #cg_matrix_entry_t pointer.
 *
 * <note>The transform represented by a #cg_matrix_entry_t is
 * immutable.</note>
 *
 * <note>#cg_matrix_entry_t<!-- -->s are reference counted using
 * cg_matrix_entry_ref() and cg_matrix_entry_unref() and you
 * should call cg_matrix_entry_unref() when you are finished with
 * and entry you get via cg_matrix_stack_get_entry().</note>
 *
 * Return value: (transfer none): A pointer to the #cg_matrix_entry_t
 *               representing the current matrix stack transform.
 */
cg_matrix_entry_t *cg_matrix_stack_get_entry(cg_matrix_stack_t *stack);

/**
 * cg_matrix_stack_get:
 * @stack: A #cg_matrix_stack_t
 * @matrix: (out): The potential destination for the current matrix
 *
 * Resolves the current @stack transform into a #c_matrix_t by
 * combining the operations that have been applied to build up the
 * current transform.
 *
 * There are two possible ways that this function may return its
 * result depending on whether the stack is able to directly point
 * to an internal #c_matrix_t or whether the result needs to be
 * composed of multiple operations.
 *
 * If an internal matrix contains the required result then this
 * function will directly return a pointer to that matrix, otherwise
 * if the function returns %NULL then @matrix will be initialized
 * to match the current transform of @stack.
 *
 * <note>@matrix will be left untouched if a direct pointer is
 * returned.</note>
 *
 * Return value: A direct pointer to the current transform or %NULL
 *               and in that case @matrix will be initialized with
 *               the value of the current transform.
 */
c_matrix_t *cg_matrix_stack_get(cg_matrix_stack_t *stack, c_matrix_t *matrix);

/**
 * cg_matrix_entry_get:
 * @entry: A #cg_matrix_entry_t
 * @matrix: (out): The potential destination for the transform as
 *                 a matrix
 *
 * Resolves the current @entry transform into a #c_matrix_t by
 * combining the sequence of operations that have been applied to
 * build up the current transform.
 *
 * There are two possible ways that this function may return its
 * result depending on whether it's possible to directly point
 * to an internal #c_matrix_t or whether the result needs to be
 * composed of multiple operations.
 *
 * If an internal matrix contains the required result then this
 * function will directly return a pointer to that matrix, otherwise
 * if the function returns %NULL then @matrix will be initialized
 * to match the transform of @entry.
 *
 * <note>@matrix will be left untouched if a direct pointer is
 * returned.</note>
 *
 * Return value: A direct pointer to a #c_matrix_t transform or %NULL
 *               and in that case @matrix will be initialized with
 *               the effective transform represented by @entry.
 */
c_matrix_t *cg_matrix_entry_get(cg_matrix_entry_t *entry, c_matrix_t *matrix);

/**
 * cg_matrix_stack_set:
 * @stack: A #cg_matrix_stack_t
 * @matrix: A #c_matrix_t replace the current matrix value with
 *
 * Replaces the current @stack matrix value with the value of @matrix.
 * This effectively discards any other operations that were applied
 * since the last time cg_matrix_stack_push() was called or since
 * the stack was initialized.
 */
void cg_matrix_stack_set(cg_matrix_stack_t *stack, const c_matrix_t *matrix);

/**
 * cg_is_matrix_stack:
 * @object: a #cg_object_t
 *
 * Determines if the given #cg_object_t refers to a #cg_matrix_stack_t.
 *
 * Return value: %true if @object is a #cg_matrix_stack_t, otherwise
 *               %false.
 */
bool cg_is_matrix_stack(void *object);

/**
 * cg_matrix_entry_calculate_translation:
 * @entry0: The first reference transform
 * @entry1: A second reference transform
 * @x: (out): The destination for the x-component of the translation
 * @y: (out): The destination for the y-component of the translation
 * @z: (out): The destination for the z-component of the translation
 *
 * Determines if the only difference between two transforms is a
 * translation and if so returns what the @x, @y, and @z components of
 * the translation are.
 *
 * If the difference between the two translations involves anything
 * other than a translation then the function returns %false.
 *
 * Return value: %true if the only difference between the transform of
 *                @entry0 and the transform of @entry1 is a translation,
 *                otherwise %false.
 */
bool cg_matrix_entry_calculate_translation(cg_matrix_entry_t *entry0,
                                           cg_matrix_entry_t *entry1,
                                           float *x,
                                           float *y,
                                           float *z);

/**
 * cg_matrix_entry_is_identity:
 * @entry: A #cg_matrix_entry_t
 *
 * Determines whether @entry is known to represent an identity
 * transform.
 *
 * If this returns %true then the entry is definitely the identity
 * matrix. If it returns %false it may or may not be the identity
 * matrix but no expensive comparison is performed to verify it.
 *
 * Return value: %true if @entry is definitely an identity transform,
 *               otherwise %false.
 */
bool cg_matrix_entry_is_identity(cg_matrix_entry_t *entry);

/**
 * cg_matrix_entry_equal:
 * @entry0: The first #cg_matrix_entry_t to compare
 * @entry1: A second #cg_matrix_entry_t to compare
 *
 * Compares two arbitrary #cg_matrix_entry_t transforms for equality
 * returning %true if they are equal or %false otherwise.
 *
 * <note>In many cases it is unnecessary to use this api and instead
 * direct pointer comparisons of entries are good enough and much
 * cheaper too.</note>
 *
 * Return value: %true if @entry0 represents the same transform as
 *               @entry1, otherwise %false.
 */
bool cg_matrix_entry_equal(cg_matrix_entry_t *entry0,
                           cg_matrix_entry_t *entry1);

/**
 * cg_debug_matrix_entry_print:
 * @entry: A #cg_matrix_entry_t
 *
 * Allows visualizing the operations that build up the given @entry
 * for debugging purposes by printing to stdout.
 */
void cg_debug_matrix_entry_print(cg_matrix_entry_t *entry);

/**
 * cg_matrix_entry_ref:
 * @entry: A #cg_matrix_entry_t
 *
 * Takes a reference on the given @entry to ensure the @entry stays
 * alive and remains valid. When you are finished with the @entry then
 * you should call cg_matrix_entry_unref().
 *
 * It is an error to pass an @entry pointer to cg_object_ref() and
 * cg_object_unref()
 */
cg_matrix_entry_t *cg_matrix_entry_ref(cg_matrix_entry_t *entry);

/**
 * cg_matrix_entry_unref:
 * @entry: A #cg_matrix_entry_t
 *
 * Releases a reference on @entry either taken by calling
 * cg_matrix_entry_unref() or to release the reference given when
 * calling cg_matrix_stack_get_entry().
 */
void cg_matrix_entry_unref(cg_matrix_entry_t *entry);

#endif /* _CG_MATRIX_STACK_H_ */
