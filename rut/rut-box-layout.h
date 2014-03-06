/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef _RUT_BOX_LAYOUT_H_
#define _RUT_BOX_LAYOUT_H_

#include "rut-type.h"
#include "rut-object.h"
#include "rut-interfaces.h"

extern RutType rut_box_layout_type;
typedef struct _RutBoxLayout RutBoxLayout;

typedef enum
{
  RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT,
  RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT,
  RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM,
  RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP
} RutBoxLayoutPacking;

RutBoxLayout *
rut_box_layout_new (RutContext *ctx,
                    RutBoxLayoutPacking packing);

void
rut_box_layout_add (RutBoxLayout *box,
                    bool expand,
                    RutObject *child);

void
rut_box_layout_remove (RutBoxLayout *box,
                       RutObject *child);

bool
rut_box_layout_get_homogeneous (RutObject *obj);

void
rut_box_layout_set_homogeneous (RutObject *obj,
                                bool homogeneous);

int
rut_box_layout_get_spacing (RutObject *obj);

void
rut_box_layout_set_spacing (RutObject *obj,
                            int spacing);

int
rut_box_layout_get_packing (RutObject *obj);

void
rut_box_layout_set_packing (RutObject *obj,
                            RutBoxLayoutPacking packing);

#endif /* _RUT_BOX_LAYOUT_H_ */
