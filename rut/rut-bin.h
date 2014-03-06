/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef _RUT_BIN_H_
#define _RUT_BIN_H_

#include "rut-object.h"
#include "rut-context.h"

extern RutType rut_bin_type;

typedef struct _RutBin RutBin;

/* How to position the child along a particular axis when there is
 * more space available then its needs */
typedef enum
{
  RUT_BIN_POSITION_BEGIN,
  RUT_BIN_POSITION_CENTER,
  RUT_BIN_POSITION_END,
  RUT_BIN_POSITION_EXPAND
} RutBinPosition;

RutBin *
rut_bin_new (RutContext *ctx);

void
rut_bin_set_child (RutBin *bin,
                   RutObject *child);

RutObject *
rut_bin_get_child (RutBin *bin);

void
rut_bin_set_x_position (RutBin *bin,
                        RutBinPosition position);

void
rut_bin_set_y_position (RutBin *bin,
                        RutBinPosition position);

void
rut_bin_set_top_padding (RutObject *bin,
                         float top_padding);

void
rut_bin_set_bottom_padding (RutObject *bin,
                            float bottom_padding);

void
rut_bin_set_left_padding (RutObject *bin,
                          float left_padding);

void
rut_bin_set_right_padding (RutObject *bin,
                           float right_padding);

#endif /* _RUT_BIN_H_ */
