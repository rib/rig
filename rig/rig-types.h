/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
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

#ifndef _RIG_TYPES_H_
#define _RIG_TYPES_H_

/* This header exists to resolve circular header dependecies that
 * occur from having pointers to lots of engine types from the RigEngine
 * struct that also themselves need to refer back to the RigEngine
 * struct */

typedef struct _RigEngine RigEngine;

#endif /* _RIG_TYPES_H_ */
