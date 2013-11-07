/*
 * Rut
 *
 * Copyright (C) 2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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
