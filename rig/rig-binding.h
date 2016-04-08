/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013 Intel Corporation.
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

#ifndef _RIG_BINDING_H_
#define _RIG_BINDING_H_

#include <rut.h>

extern rut_type_t rig_binding_type;

typedef struct _rig_binding_t rig_binding_t;

rig_binding_t *
rig_binding_new(rig_engine_t *engine, rig_property_t *property, int binding_id);

rig_binding_t *
rig_binding_new_simple_copy(rig_engine_t *engine,
                            rig_property_t *dst_prop,
                            rig_property_t *src_prop);

int rig_binding_get_id(rig_binding_t *binding);

void rig_binding_add_dependency(rig_binding_t *binding,
                                rig_property_t *property,
                                const char *name);

void rig_binding_remove_dependency(rig_binding_t *binding,
                                   rig_property_t *property);

char *rig_binding_get_expression(rig_binding_t *binding);

void rig_binding_set_expression(rig_binding_t *binding, const char *expression);

void rig_binding_set_dependency_name(rig_binding_t *binding,
                                     rig_property_t *property,
                                     const char *name);

void rig_binding_activate(rig_binding_t *binding);

void rig_binding_deactivate(rig_binding_t *binding);

int rig_binding_get_n_dependencies(rig_binding_t *binding);

void rig_binding_foreach_dependency(rig_binding_t *binding,
                                    void (*callback)(rig_binding_t *binding,
                                                     rig_property_t *dependency,
                                                     void *user_data),
                                    void *user_data);

#endif /* _RIG_BINDING_H_ */
