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

#include <config.h>

#include "rut-renderer.h"

void
rut_renderer_notify_entity_changed (RutObject *object, RutEntity *entity)
{
  RutRendererVTable *renderer =
    rut_object_get_vtable (object, RUT_INTERFACE_ID_RENDERER);

  return renderer->notify_entity_changed (entity);
}

void
rut_renderer_free_priv (RutObject *object, RutEntity *entity)
{
  RutRendererVTable *renderer =
    rut_object_get_vtable (object, RUT_INTERFACE_ID_RENDERER);

  return renderer->free_priv (entity);
}
