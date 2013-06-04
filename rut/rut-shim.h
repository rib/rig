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

typedef struct _RutShim RutShim;
extern RutType rut_shim_type;

RutShim *
rut_shim_new (RutContext *ctx,
              float width,
              float height);

void
rut_shim_set_width (RutShim *shim, float width);

void
rut_shim_set_height (RutShim *shim, float height);

void
rut_shim_set_size (RutObject *self,
                    float width,
                    float height);

void
rut_shim_get_size (RutObject *self,
                    float *width,
                    float *height);

void
rut_shim_set_child (RutShim *shim, RutObject *child);

#endif /* __RUT_FIXED_H__ */
