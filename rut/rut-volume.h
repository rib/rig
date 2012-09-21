/*
 * Rut
 *
 * A rig of UI prototyping utilities
 *
 * Copyright (C) 2010,2011  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RUT_VOLUME_H_
#define _RUT_VOLUME_H_

#include "rut-planes.h"
#include "rut-types.h"

#include <cogl/cogl.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _RutVolume RutVolume;

void
rut_volume_init (RutVolume *volume);

/**
 * rut_volume_new:
 *
 * Creates a new #RutVolume representing a 3D region
 *
 * Return value: the newly allocated #RutVolume. Use
 *   rut_volume_free() to free the resources it uses
 */
RutVolume *
rut_volume_new (void);

/**
 * rut_volume_copy:
 * @volume: a #RutVolume
 *
 * Copies @volume into a new #RutVolume
 *
 * Return value: a newly allocated copy of a #RutVolume
 */
RutVolume *
rut_volume_copy (const RutVolume *volume);

/**
 * rut_volume_free:
 * @volume: a #RutVolume
 *
 * Frees the resources allocated by @volume
 */
void
rut_volume_free (RutVolume *volume);

/**
 * rut_volume_set_origin:
 * @volume: a #RutVolume
 * @origin: a #RutVector3
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
void
rut_volume_set_origin (RutVolume *volume, const RutVector3 *origin);

/**
 * rut_volume_get_origin:
 * @volume: a #RutVolume
 * @origin: (out): a return location for 3 x,y,z coordinate values.
 *
 * Retrieves the origin of the #RutVolume.
 */
void
rut_volume_get_origin (const RutVolume *volume, RutVector3 *origin);

/**
 * rut_volume_set_width:
 * @volume: a #RutVolume
 * @width: the width of the volume, in object units
 *
 * Sets the width of the volume. The width is measured along the x
 * axis in the object coordinates that @volume is associated with.
 */
void
rut_volume_set_width (RutVolume *volume, float width);

/**
 * rut_volume_get_width:
 * @volume: a #RutVolume
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
float
rut_volume_get_width (const RutVolume *volume);

/**
 * rut_volume_set_height:
 * @volume: a #RutVolume
 * @height: the height of the volume, in object units
 *
 * Sets the height of the volume. The height is measured along
 * the y axis in the object coordinates that @volume is associated with.
 */
void
rut_volume_set_height (RutVolume *volume,
                       float height);

/**
 * rut_volume_get_height:
 * @volume: a #RutVolume
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
float
rut_volume_get_height (const RutVolume *volume);

/**
 * rut_volume_set_depth:
 * @volume: a #RutVolume
 * @depth: the depth of the volume, in object units
 *
 * Sets the depth of the volume. The depth is measured along
 * the z axis in the object coordinates that @volume is associated with.
 */
void
rut_volume_set_depth (RutVolume *volume,
                      float depth);

/**
 * rut_volume_get_depth:
 * @volume: a #RutVolume
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
float
rut_volume_get_depth (const RutVolume *volume);

/**
 * rut_volume_union:
 * @volume: The first #RutVolume and destination for resulting
 *          union
 * @another_volume: A second #RutVolume to union with @volume
 *
 * Updates the geometry of @volume to encompass @volume and @another_volume.
 *
 * <note>There are no guarantees about how precisely the two volumes
 * will be encompassed.</note>
 */
void
rut_volume_union (RutVolume *volume,
                  const RutVolume *another_volume);

void
rut_volume_transform (RutVolume *pv,
                      const CoglMatrix *matrix);
void
rut_volume_project (RutVolume *pv,
                    const CoglMatrix *modelview,
                    const CoglMatrix *projection,
                    const float *viewport);

void
rut_volume_get_bounding_box (RutVolume *pv,
                             RutBox *box);
/**
 * @volume: A #RutVolume
 *
 * Given a volume that has been transformed by an arbitrary modelview
 * and is no longer axis aligned, this derives a replacement that is
 * axis aligned.
 */
void
rut_volume_axis_align (RutVolume *volume);

RutCullResult
rut_volume_cull (RutVolume *pv,
                 const RutPlane *planes);


G_END_DECLS

#endif /* _RUT_VOLUME_H_ */
