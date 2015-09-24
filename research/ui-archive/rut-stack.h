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

#ifndef _RUT_STACK_H_
#define _RUT_STACK_H_

#include "rut-object.h"

extern rut_type_t rut_stack_type;
typedef struct _rut_stack_t rut_stack_t;

rut_stack_t *rut_stack_new(rut_shell_t *shell, float width, float height);

void rut_stack_add(rut_stack_t *stack, rut_object_t *child);

void rut_stack_set_size(rut_object_t *stack, float width, float height);

void rut_stack_set_width(rut_object_t *stack, float width);

void rut_stack_set_height(rut_object_t *stack, float height);

void rut_stack_get_size(rut_object_t *stack, float *width, float *height);

#endif /* _RUT_STACK_H_ */
