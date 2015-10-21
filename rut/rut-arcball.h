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

#ifndef __RUT_ARCBALL_H__
#define __RUT_ARCBALL_H__

#include <clib.h>
#include <cglib/cglib.h>

C_BEGIN_DECLS

typedef struct {
    float center[2], down[2];
    float radius;
    c_quaternion_t q_drag;

} rut_arcball_t;

void rut_arcball_init(rut_arcball_t *ball,
                      float center_x,
                      float center_y,
                      float radius);

void rut_arcball_mouse_down(rut_arcball_t *ball, float x, float y);
void rut_arcball_mouse_motion(rut_arcball_t *ball, float x, float y);

C_END_DECLS

#endif /* __RUT_ARCBALL_H__ */
