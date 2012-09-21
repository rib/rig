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

#ifndef __RUT_ARCBALL_H__
#define __RUT_ARCBALL_H__

#include <glib.h>
#include <cogl/cogl.h>

G_BEGIN_DECLS

typedef struct
{
  float center[2], down[2];
  float radius;
  CoglQuaternion q_drag;

} RutArcball;

void
rut_arcball_init (RutArcball *ball,
                  float       center_x,
                  float       center_y,
                  float       radius);

void
rut_arcball_mouse_down (RutArcball *ball,
                        float       x,
                        float       y);
void
rut_arcball_mouse_motion (RutArcball *ball,
                          float       x,
                          float       y);

G_END_DECLS

#endif /* __RUT_ARCBALL_H__ */
