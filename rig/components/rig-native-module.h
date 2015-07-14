/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2015  Intel Corporation.
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
 *
 */

#ifndef __RIG_NATIVE_MODULE_H__
#define __RIG_NATIVE_MODULE_H__

#include <rut.h>

#include "rig-engine.h"

typedef struct _rig_native_module_t rig_native_module_t;
extern rut_type_t rig_native_module_type;

rig_native_module_t *rig_native_module_new(rig_engine_t *engine);
const char *rig_native_module_get_name(rut_object_t *object);
void rig_native_module_set_name(rut_object_t *object, const char *name);

/* Instead of dynamically loading a shared object, a resolver can be
 * used to link with internal symbols. This is a convenience for
 * writting test applications */
typedef void *(*rig_native_module_resolver_func_t)(const char *symbol, void *user_data);
void rig_native_module_set_resolver(rig_native_module_t *module,
                                    rig_native_module_resolver_func_t resolver,
                                    void *user_data);

void rig_native_module_load(rig_native_module_t *module);
void rig_native_module_update(rig_native_module_t *module);
void rig_native_module_handle_update(rig_native_module_t *module,
                                     rut_input_event_t *event);

#endif /* __RIG_NATIVE_MODULE_H__ */
