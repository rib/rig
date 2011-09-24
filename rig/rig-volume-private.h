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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_VOLUME_PRIVATE_H_
#define _RIG_VOLUME_PRIVATE_H_

#include "rig-volume.h"

struct _RigVolume
{
  /* cuboid for the volume:
   *
   *       4━━━━━━━┓5
   *    ┏━━━━━━━━┓╱┃
   *    ┃0 ┊7   1┃ ┃
   *    ┃   ┄┄┄┄┄┃┄┃6
   *    ┃3      2┃╱
   *    ┗━━━━━━━━┛
   *
   *   0: top, left (origin)  : always valid
   *   1: top, right          : always valid
   *   2: bottom, right       :  updated lazily
   *   3: bottom, left        : always valid
   *
   *   4: top, left, back     : always valid
   *   5: top, right, back    :  updated lazily
   *   6: bottom, right, back :  updated lazily
   *   7: bottom, left, back  :  updated lazily
   *
   * Elements 0, 1, 3 and 4 are filled in by the Volume setters
   *
   * Note: the reason for this ordering is that we can simply ignore
   * elements 4, 5, 6 and 7 when dealing with 2D objects.
   */
  RigVector3 vertices[8];

  /* As an optimization for internally managed Volumes we allow
   * initializing RigVolume variables allocated on the stack
   * so we can avoid hammering the slice allocator. */
  unsigned int is_static:1;

  /* A newly initialized Volume is considered empty as it is
   * degenerate on all three axis.
   *
   * We consider this carefully when we union an empty volume with
   * another so that the union simply results in a copy of the other
   * volume instead of also bounding the origin of the empty volume.
   *
   * For example this is a convenient property when calculating the
   * volume of a container as the union of the volume of its children
   * where the initial volume passed to the containers
   * ->get_paint_volume method will be empty. */
  unsigned int is_empty:1;

  /* TRUE when we've updated the values we calculate lazily */
  unsigned int is_complete:1;

  /* TRUE if vertices 4-7 can be ignored. (Only valid if complete is
   * TRUE) */
  unsigned int is_2d:1;

  /* Set to TRUE initialy but cleared if the paint volume is
   * transfomed by a matrix. */
  unsigned int is_axis_aligned:1;


  /* Note: There is a precedence to the above bitfields that should be
   * considered whenever we implement code that manipulates
   * Volumes...
   *
   * Firstly if ->is_empty == TRUE then the values for ->is_complete
   * and ->is_2d are undefined, so you should typically check
   * ->is_empty as the first priority.
   *
   * XXX: document other invariables...
   */
};

void
_rig_volume_init_static (RigVolume *pv);

void
_rig_volume_copy_static (const RigVolume *src_pv,
                         RigVolume *dst_pv);
void
_rig_volume_set_from_volume (RigVolume *pv,
                             const RigVolume *src);

void
_rig_volume_complete (RigVolume *pv);

void
_rig_volume_axis_align (RigVolume *volume);

#endif /* _RIG_VOLUME_PRIVATE_H_ */
