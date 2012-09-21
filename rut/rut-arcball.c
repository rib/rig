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

/* http://research.cs.wisc.edu/graphics/Courses/559-f2001/Examples/Gl3D/arcball-gems.pdf */

#include <math.h>

#include "rut-arcball.h"

void
rut_arcball_init (RutArcball *ball,
                  float       center_x,
                  float       center_y,
                  float       radius)
{
  ball->center[0] = center_x;
  ball->center[1] = center_y;
  ball->radius = radius;

  cogl_quaternion_init_identity (&ball->q_drag);
}

void
rut_arcball_mouse_down (RutArcball *ball,
                        float       x,
                        float       y)
{
  ball->down[0] = x;
  ball->down[1] = y;
}

static void
rut_arcball_mouse_to_sphere (RutArcball *ball,
                             float x,
                             float y,
                             float result[3])
{
  float mag_squared;

  result[0] = (x - ball->center[0]) / ball->radius;
  result[1] = (y - ball->center[1]) / ball->radius;
  result[2] = 0.0f;

  mag_squared = result[0] * result[0] + result[1] * result[1];

  if (mag_squared > 1.0f)
    {
      /* normalize, but using mag_squared already computed and knowing the
       * z component is 0 */
      float one_over_mag = 1.0 / sqrtf (mag_squared);

      result[0] *= one_over_mag;
      result[1] *= one_over_mag;
    }
  else
    {
      result[2] = sqrtf (1.0f - mag_squared);
    }
}

void
rut_arcball_mouse_motion (RutArcball *ball,
                          float       x,
                          float       y)
{
  float v0[3], v1[3], *cross, drag_quat[4];

  rut_arcball_mouse_to_sphere (ball, ball->down[0], ball->down[1], v0);
  rut_arcball_mouse_to_sphere (ball, x, y, v1);

  drag_quat[0] = cogl_vector3_dot_product (v0, v1);
  cross = drag_quat + 1;
  cogl_vector3_cross_product (cross, v0, v1);

  cogl_quaternion_init_from_array (&ball->q_drag, drag_quat);
}
