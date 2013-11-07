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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __RUT_INPUT_REGION_H__
#define __RUT_INPUT_REGION_H__

#include <stdbool.h>

#include <cogl/cogl.h>

#include "rut-types.h"
#include "rut-shell.h"

typedef struct _RutInputRegion RutInputRegion;

typedef RutInputEventStatus (*RutInputRegionCallback) (RutInputRegion *region,
                                                       RutInputEvent *event,
                                                       void *user_data);


RutInputRegion *
rut_input_region_new_rectangle (float x0,
                                float y0,
                                float x1,
                                float y1,
                                RutInputRegionCallback callback,
                                void *user_data);

RutInputRegion *
rut_input_region_new_circle (float x,
                             float y,
                             float radius,
                             RutInputRegionCallback callback,
                             void *user_data);

void
rut_input_region_set_transform (RutInputRegion *region,
                                CoglMatrix *matrix);

void
rut_input_region_set_rectangle (RutInputRegion *region,
                                float x0,
                                float y0,
                                float x1,
                                float y1);

void
rut_input_region_set_circle (RutInputRegion *region,
                             float x0,
                             float y0,
                             float radius);

/* XXX: Note: the plan is to remove this api at some point
 *
 * If HUD mode is TRUE then the region isn't transformed by the
 * camera's view transform so the region is in window coordinates.
 */
void
rut_input_region_set_hud_mode (RutInputRegion *region,
                               bool hud_mode);


#endif /* __RUT_INPUT_REGION_H__ */
