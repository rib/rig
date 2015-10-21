/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2010,2011  Intel Corporation.
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

#ifndef _RUT_VOLUME_H_
#define _RUT_VOLUME_H_

#include "rut-planes.h"
#include "rut-types.h"

#include <cglib/cglib.h>
#include <clib.h>

C_BEGIN_DECLS

typedef struct _rut_volume_t rut_volume_t;

void rut_volume_init(rut_volume_t *volume);

/**
 * rut_volume_new:
 *
 * Creates a new #rut_volume_t representing a 3D region
 *
 * Return value: the newly allocated #rut_volume_t. Use
 *   rut_volume_free() to free the resources it uses
 */
rut_volume_t *rut_volume_new(void);

/**
 * rut_volume_copy:
 * @volume: a #rut_volume_t
 *
 * Copies @volume into a new #rut_volume_t
 *
 * Return value: a newly allocated copy of a #rut_volume_t
 */
rut_volume_t *rut_volume_copy(const rut_volume_t *volume);

/**
 * rut_volume_free:
 * @volume: a #rut_volume_t
 *
 * Frees the resources allocated by @volume
 */
void rut_volume_free(rut_volume_t *volume);

/**
 * rut_volume_set_origin:
 * @volume: a #rut_volume_t
 * @origin: a #rut_vector3_t
 *
 * Sets the origin of the volume.
 *
 * The origin is defined as the X, Y and Z coordinates of the top-left
 * corner of an object's volume, in object local coordinates.
 *
 * The default is origin is: (0, 0, 0)
 *
 * Since: 1.6
 */
void rut_volume_set_origin(rut_volume_t *volume, const rut_vector3_t *origin);

/**
 * rut_volume_get_origin:
 * @volume: a #rut_volume_t
 * @origin: (out): a return location for 3 x,y,z coordinate values.
 *
 * Retrieves the origin of the #rut_volume_t.
 */
void rut_volume_get_origin(const rut_volume_t *volume, rut_vector3_t *origin);

/**
 * rut_volume_set_width:
 * @volume: a #rut_volume_t
 * @width: the width of the volume, in object units
 *
 * Sets the width of the volume. The width is measured along the x
 * axis in the object coordinates that @volume is associated with.
 */
void rut_volume_set_width(rut_volume_t *volume, float width);

/**
 * rut_volume_get_width:
 * @volume: a #rut_volume_t
 *
 * Retrieves the width of the volume's, axis aligned, bounding box.
 *
 * In other words; it fits an axis aligned box around the given
 * @volume in the same coordinate space that the volume is currently
 * in. It returns the size of that bounding box as measured along the
 * x-axis.
 *
 * <note>There are no accuracy guarantees for the reported width,
 * except that it must always be >= to the true width. This is
 * because objects may report simple, loose fitting volumes
 * for efficiency</note>

 * Return value: the width, in units of @volume's local coordinate system.
 */
float rut_volume_get_width(const rut_volume_t *volume);

/**
 * rut_volume_set_height:
 * @volume: a #rut_volume_t
 * @height: the height of the volume, in object units
 *
 * Sets the height of the volume. The height is measured along
 * the y axis in the object coordinates that @volume is associated with.
 */
void rut_volume_set_height(rut_volume_t *volume, float height);

/**
 * rut_volume_get_height:
 * @volume: a #rut_volume_t
 *
 * Retrieves the height of the volume's, axis aligned, bounding box.
 *
 * In other words; it fits an axis aligned box around the given
 * @volume in the same coordinate space that the volume is currently
 * in. It returns the size of that bounding box as measured along the
 * x-axis.
 *
 * <note>There are no accuracy guarantees for the reported height,
 * except that it must always be >= to the true height. This is
 * because objects may report simple, loose fitting volumes
 * for efficiency</note>
 *
 * Return value: the height, in units of @volume's local coordinate system.
 */
float rut_volume_get_height(const rut_volume_t *volume);

/**
 * rut_volume_set_depth:
 * @volume: a #rut_volume_t
 * @depth: the depth of the volume, in object units
 *
 * Sets the depth of the volume. The depth is measured along
 * the z axis in the object coordinates that @volume is associated with.
 */
void rut_volume_set_depth(rut_volume_t *volume, float depth);

/**
 * rut_volume_get_depth:
 * @volume: a #rut_volume_t
 *
 * Retrieves the depth of the volume's, axis aligned, bounding box.
 *
 * In other words; it fits an axis aligned box around the given
 * @volume in the same coordinate space that the volume is currently
 * in. It returns the size of that bounding box as measured along the
 * x-axis.
 *
 * <note>There are no accuracy guarantees for the reported depth,
 * except that it must always be >= to the true depth. This is
 * because objects may report simple, loose fitting volumes
 * for efficiency</note>
 *
 * Return value: the depth, in units of @volume's local coordinate system.
 */
float rut_volume_get_depth(const rut_volume_t *volume);

/**
 * rut_volume_union:
 * @volume: The first #rut_volume_t and destination for resulting
 *          union
 * @another_volume: A second #rut_volume_t to union with @volume
 *
 * Updates the geometry of @volume to encompass @volume and @another_volume.
 *
 * <note>There are no guarantees about how precisely the two volumes
 * will be encompassed.</note>
 */
void rut_volume_union(rut_volume_t *volume, const rut_volume_t *another_volume);

void rut_volume_transform(rut_volume_t *pv, const c_matrix_t *matrix);
void rut_volume_project(rut_volume_t *pv,
                        const c_matrix_t *modelview,
                        const c_matrix_t *projection,
                        const float *viewport);

void rut_volume_get_bounding_box(rut_volume_t *pv, rut_box_t *box);
/**
 * @volume: A #rut_volume_t
 *
 * Given a volume that has been transformed by an arbitrary modelview
 * and is no longer axis aligned, this derives a replacement that is
 * axis aligned.
 */
void rut_volume_axis_align(rut_volume_t *volume);

rut_cull_result_t rut_volume_cull(rut_volume_t *pv, const rut_plane_t *planes);

C_END_DECLS

#endif /* _RUT_VOLUME_H_ */
