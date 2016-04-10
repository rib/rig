/*
 * Rig
 *
 * UI Engine & Editor
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

#pragma once

#include <clib.h>

#include <rut.h>

typedef struct {
    double progress;
} rig_code_module_update_t;

#include "rig-engine.h"

extern int rig_code_module_trait_id;

typedef struct _rig_code_module_props {
    /* To allow code modules to be tracked in list without needing
     * to traverse the scenegraph to iterate through them... */
    c_list_t system_link;

    rig_engine_t *engine;

    rut_object_t *object; /* back pointer, to access + use
                             the vtable interface */
} rig_code_module_props_t;

typedef struct _rig_code_module_vtable {
    void (*load)(rut_object_t *object);
    void (*update)(rut_object_t *object, rig_code_module_update_t *state);
    void (*input)(rut_object_t *object, rut_input_event_t *event);
} rig_code_module_vtable_t;
