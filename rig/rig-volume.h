/*
 * Rig
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

#ifndef _RIG_VOLUME_H_
#define _RIG_VOLUME_H_

#include "rig-planes.h"
#include "rig-types.h"

#include <cogl/cogl.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _RigVolume RigVolume;

void
rig_volume_init (RigVolume *volume);

/**
 * rig_volume_new:
 *
 * Creates a new #RigVolume representing a 3D region
 *
 * Return value: the newly allocated #RigVolume. Use
 *   rig_volume_free() to free the resources it uses
 */
RigVolume *
rig_volume_new (void);

/**
 * rig_volume_copy:
 * @volume: a #RigVolume
 *
 * Copies @volume into a new #RigVolume
 *
 * Return value: a newly allocated copy of a #RigVolume
 */
RigVolume *
rig_volume_copy (const RigVolume *volume);

/**
 * rig_volume_free:
 * @volume: a #RigVolume
 *
 * Frees the resources allocated by @volume
 */
void
rig_volume_free (RigVolume *volume);

/**
 * rig_volume_set_origin:
 * @volume: a #RigVolume
 * @origin: a #RigVector3
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
rig_volume_set_origin (RigVolume *volume, const RigVector3 *origin);

/**
 * rig_volume_get_origin:
 * @volume: a #RigVolume
 * @origin: (out): a return location for 3 x,y,z coordinate values.
 *
 * Retrieves the origin of the #RigVolume.
 */
void
rig_volume_get_origin (const RigVolume *volume, RigVector3 *origin);

/**
 * rig_volume_set_width:
 * @volume: a #RigVolume
 * @width: the width of the volume, in object units
 *
 * Sets the width of the volume. The width is measured along the x
 * axis in the object coordinates that @volume is associated with.
 */
void
rig_volume_set_width (RigVolume *volume, float width);

/**
 * rig_volume_get_width:
 * @volume: a #RigVolume
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
rig_volume_get_width (const RigVolume *volume);

/**
 * rig_volume_set_height:
 * @volume: a #RigVolume
 * @height: the height of the volume, in object units
 *
 * Sets the height of the volume. The height is measured along
 * the y axis in the object coordinates that @volume is associated with.
 */
void
rig_volume_set_height (RigVolume *volume,
                       float height);

/**
 * rig_volume_get_height:
 * @volume: a #RigVolume
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
rig_volume_get_height (const RigVolume *volume);

/**
 * rig_volume_set_depth:
 * @volume: a #RigVolume
 * @depth: the depth of the volume, in object units
 *
 * Sets the depth of the volume. The depth is measured along
 * the z axis in the object coordinates that @volume is associated with.
 */
void
rig_volume_set_depth (RigVolume *volume,
                      float depth);

/**
 * rig_volume_get_depth:
 * @volume: a #RigVolume
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
rig_volume_get_depth (const RigVolume *volume);

/**
 * rig_volume_union:
 * @volume: The first #RigVolume and destination for resulting
 *          union
 * @another_volume: A second #RigVolume to union with @volume
 *
 * Updates the geometry of @volume to encompass @volume and @another_volume.
 *
 * <note>There are no guarantees about how precisely the two volumes
 * will be encompassed.</note>
 */
void
rig_volume_union (RigVolume *volume,
                  const RigVolume *another_volume);

void
rig_volume_transform (RigVolume *pv,
                      const CoglMatrix *matrix);
void
rig_volume_project (RigVolume *pv,
                    const CoglMatrix *modelview,
                    const CoglMatrix *projection,
                    const float *viewport);

void
rig_volume_get_bounding_box (RigVolume *pv,
                             RigBox *box);
/**
 * @volume: A #RigVolume
 *
 * Given a volume that has been transformed by an arbitrary modelview
 * and is no longer axis aligned, this derives a replacement that is
 * axis aligned.
 */
void
rig_volume_axis_align (RigVolume *volume);

RigCullResult
rig_volume_cull (RigVolume *pv,
                 const RigPlane *planes);


G_END_DECLS

#endif /* _RIG_VOLUME_H_ */
