/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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

#ifndef _RUT_ROTATION_INSPECTOR_H_
#define _RUT_ROTATION_INSPECTOR_H_

#include <cglib/cglib.h>

#include "rut-object.h"
#include "rut-shell.h"

extern rut_type_t rut_rotation_inspector_type;
typedef struct _rut_rotation_inspector_t rut_rotation_inspector_t;

rut_rotation_inspector_t *rut_rotation_inspector_new(rut_shell_t *shell);

void rut_rotation_inspector_set_value(rut_object_t *slider,
                                      const c_quaternion_t *value);

void rut_rotation_inspector_set_step(rut_rotation_inspector_t *slider,
                                     float step);

void rut_rotation_inspector_set_decimal_places(rut_rotation_inspector_t *slider,
                                               int decimal_places);

int rut_rotation_inspector_get_decimal_places(rut_rotation_inspector_t *slider);

#endif /* _RUT_ROTATION_INSPECTOR_H_ */
