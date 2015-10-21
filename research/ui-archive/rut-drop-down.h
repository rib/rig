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

#ifndef _RUT_DROP_DOWN_H_
#define _RUT_DROP_DOWN_H_

#include <cglib/cglib.h>

#include "rut-types.h"
#include "rut-object.h"

extern rut_type_t rut_drop_down_type;

typedef struct _rut_drop_down_t rut_drop_down_t;

typedef struct {
    const char *name;
    int value;
} rut_drop_down_value_t;

rut_drop_down_t *rut_drop_down_new(rut_shell_t *shell);

void rut_drop_down_set_value(rut_object_t *slider, int value);

int rut_drop_down_get_value(rut_object_t *slider);

void rut_drop_down_set_values(rut_drop_down_t *drop,
                              ...) G_GNUC_NULL_TERMINATED;

void rut_drop_down_set_values_array(rut_drop_down_t *drop,
                                    const rut_drop_down_value_t *values,
                                    int n_values);

#endif /* _RUT_DROP_DOWN_H_ */
