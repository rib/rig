/*
 * Rig
 *
 * UI Engine & Editor
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

#ifndef _RIG_SPLIT_VIEW_H_
#define _RIG_SPLIT_VIEW_H_

#include <cglib/cglib.h>

#include "rut.h"

#include "rig-engine.h"

extern rut_type_t rig_split_view_type;
typedef struct _rig_split_view_t rig_split_view_t;

typedef enum _rig_split_view_split_t {
    RIG_SPLIT_VIEW_SPLIT_VERTICAL,
    RIG_SPLIT_VIEW_SPLIT_HORIZONTAL
} rig_split_view_split_t;

rig_split_view_t *rig_split_view_new(rig_engine_t *engine,
                                     rig_split_view_split_t split,
                                     float width,
                                     float height);

void
rig_split_view_set_size(rut_object_t *split_view, float width, float height);
void rig_split_view_set_width(rut_object_t *split_view, float width);

void rig_split_view_set_height(rut_object_t *split_view, float height);

void
rig_split_view_get_size(rut_object_t *split_view, float *width, float *height);

void rig_split_view_set_split_fraction(rig_split_view_t *split_view,
                                       float fraction);

void rig_split_view_set_child0(rig_split_view_t *split_view,
                               rut_object_t *child0);

void rig_split_view_set_child1(rig_split_view_t *split_view,
                               rut_object_t *child1);

#endif /* _RIG_SPLIT_VIEW_H_ */
