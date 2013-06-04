/*
 * Rig
 *
 * Copyright (C) 2013 Intel Corporation.
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

#include <config.h>

#ifndef _RUT_COMPOSITE_SIZABLE_
#define _RUT_COMPOSITE_SIZABLE_

void
rut_composite_sizable_get_preferred_width (void *sizable,
                                           float for_height,
                                           float *min_width_p,
                                           float *natural_width_p);

void
rut_composite_sizable_get_preferred_height (void *sizable,
                                            float for_width,
                                            float *min_height_p,
                                            float *natural_height_p);

RutClosure *
rut_composite_sizable_add_preferred_size_callback (
                                             void *object,
                                             RutSizablePreferredSizeCallback cb,
                                             void *user_data,
                                             RutClosureDestroyCallback destroy);

void
rut_composite_sizable_set_size (void *object, float width, float height);

void
rut_composite_sizable_get_size (void *object, float *width, float *height);

#endif /* _RUT_COMPOSITE_SIZABLE_ */
