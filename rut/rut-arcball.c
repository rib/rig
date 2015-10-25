/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* http://research.cs.wisc.edu/graphics/Courses/559-f2001/Examples/Gl3D/arcball-gems.pdf
 */

#include <rut-config.h>

#include <math.h>

#include "rut-arcball.h"

void
rut_arcball_init(rut_arcball_t *ball,
                 float center_x,
                 float center_y,
                 float radius)
{
    ball->center[0] = center_x;
    ball->center[1] = center_y;
    ball->radius = radius;

    c_quaternion_init_identity(&ball->q_drag);
}

void
rut_arcball_mouse_down(rut_arcball_t *ball, float x, float y)
{
    ball->down[0] = x;
    ball->down[1] = y;
}

static void
rut_arcball_mouse_to_sphere(rut_arcball_t *ball,
                            float x,
                            float y,
                            float result[3])
{
    float mag_squared;

    result[0] = (x - ball->center[0]) / ball->radius;
    result[1] = (y - ball->center[1]) / ball->radius;
    result[2] = 0.0f;

    mag_squared = result[0] * result[0] + result[1] * result[1];

    if (mag_squared > 1.0f) {
        /* normalize, but using mag_squared already computed and knowing the
         * z component is 0 */
        float one_over_mag = 1.0 / sqrtf(mag_squared);

        result[0] *= one_over_mag;
        result[1] *= one_over_mag;
    } else {
        result[2] = sqrtf(1.0f - mag_squared);
    }
}

void
rut_arcball_mouse_motion(rut_arcball_t *ball, float x, float y)
{
    float v0[3], v1[3], *cross, drag_quat[4];

    rut_arcball_mouse_to_sphere(ball, ball->down[0], ball->down[1], v0);
    rut_arcball_mouse_to_sphere(ball, x, y, v1);

    drag_quat[0] = c_vector3_dot_product(v0, v1);
    cross = drag_quat + 1;
    c_vector3_cross_product(cross, v0, v1);

    c_quaternion_init_from_array(&ball->q_drag, drag_quat);
}
