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

#ifndef _RUT_VOLUME_PRIVATE_H_
#define _RUT_VOLUME_PRIVATE_H_

#include "rut-volume.h"

struct _rut_volume_t {
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
    rut_vector3_t vertices[8];

    /* As an optimization for internally managed Volumes we allow
     * initializing rut_volume_t variables allocated on the stack
     * so we can avoid hammering the slice allocator. */
    unsigned int is_static : 1;

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
    unsigned int is_empty : 1;

    /* true when we've updated the values we calculate lazily */
    unsigned int is_complete : 1;

    /* true if vertices 4-7 can be ignored. (Only valid if complete is
     * true) */
    unsigned int is_2d : 1;

    /* Set to true initialy but cleared if the paint volume is
     * transfomed by a matrix. */
    unsigned int is_axis_aligned : 1;

    /* Note: There is a precedence to the above bitfields that should be
     * considered whenever we implement code that manipulates
     * Volumes...
     *
     * Firstly if ->is_empty == true then the values for ->is_complete
     * and ->is_2d are undefined, so you should typically check
     * ->is_empty as the first priority.
     *
     * XXX: document other invariables...
     */
};

void _rut_volume_init_static(rut_volume_t *pv);

void _rut_volume_copy_static(const rut_volume_t *src_pv, rut_volume_t *dst_pv);
void _rut_volume_set_from_volume(rut_volume_t *pv, const rut_volume_t *src);

void _rut_volume_complete(rut_volume_t *pv);

void _rut_volume_axis_align(rut_volume_t *volume);

#endif /* _RUT_VOLUME_PRIVATE_H_ */
