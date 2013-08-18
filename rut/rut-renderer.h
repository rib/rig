/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#ifndef _RUT_RENDERER_H_
#define _RUT_RENDERER_H_

#include "rut-object.h"
#include "rut-entity.h"

/*
 * Renderer interface
 *
 * An interface for something to act as the renderer of a scenegraph
 * of entities.
 */
typedef struct _RutRendererVTable
{
  void (*notify_entity_changed) (RutEntity *entity);
  void (*free_priv) (RutEntity *entity);
} RutRendererVTable;

void
rut_renderer_notify_entity_changed (RutObject *renderer, RutEntity *entity);

void
rut_renderer_free_priv (RutObject *renderer, RutEntity *entity);

#endif /* _RUT_RENDERER_H_ */
