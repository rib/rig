/*
 * Rig
 *
 * UI Engine & Editor
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

#ifndef __RIG_CODEGEN_H__
#define __RIG_CODEGEN_H__

//#define NULL ((void *)0)

typedef struct _rut_type_t rut_type_t;

typedef void rut_object_t;

typedef struct _rut_shell_t rut_shell_t;

typedef struct _rut_ui_enum_t rut_ui_enum_t;

typedef struct _rut_property_context_t rut_property_context_t;

typedef void rig_asset_t;
typedef int rig_asset_type_t;

typedef struct _rut_memory_stack_t rut_memory_stack_t;

/* We want to minimize the number of headers we include during the
 * runtime compilation of UI logic and so we just duplicate the
 * GLib and Cogl typedefs that are required in rut-property-bare.h...
 */

typedef struct _c_sllist_t c_sllist_t;
#define c_return_if_fail(X)
#define c_return_val_if_fail(X, Y)

typedef int bool;

typedef struct _cg_color_t {
    float r, g, b, a;
} cg_color_t;

typedef struct _c_quaternion_t {
    float w, x, y, z;
} c_quaternion_t;

#endif /* __RIG_CODEGEN_H__ */
