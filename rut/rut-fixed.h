/*
 * Rut.
 *
 * Copyright (C) 2013  Intel Corporation.
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
 *
 */

#ifndef __RUT_FIXED_H__
#define __RUT_FIXED_H__

#include <cogl/cogl.h>

#include <rut-context.h>

typedef struct _RutFixed RutFixed;
extern RutType rut_fixed_type;

RutFixed *
rut_fixed_new (RutContext *ctx,
               float width,
               float height);

void
rut_fixed_set_width (RutFixed *fixed, float width);

void
rut_fixed_set_height (RutFixed *fixed, float height);

void
rut_fixed_set_size (RutObject *self,
                    float width,
                    float height);

void
rut_fixed_get_size (RutObject *self,
                    float *width,
                    float *height);

void
rut_fixed_add_child (RutFixed *fixed, RutObject *child);

void
rut_fixed_remove_child (RutFixed *fixed, RutObject *child);

#endif /* __RUT_FIXED_H__ */
