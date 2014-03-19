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

extern RutType rig_binding_type;

typedef struct _RigBinding RigBinding;

RigBinding *
rig_binding_new (RigEngine *engine,
                 RutProperty *property,
                 int binding_id);

int
rig_binding_get_id (RigBinding *binding);

void
rig_binding_add_dependency (RigBinding *binding,
                            RutProperty *property,
                            const char *name);

void
rig_binding_remove_dependency (RigBinding *binding,
                               RutProperty *property);

char *
rig_binding_get_expression (RigBinding *binding);

void
rig_binding_set_expression (RigBinding *binding,
                            const char *expression);

void
rig_binding_set_dependency_name (RigBinding *binding,
                                 RutProperty *property,
                                 const char *name);

void
rig_binding_activate (RigBinding *binding);

void
rig_binding_deactivate (RigBinding *binding);

int
rig_binding_get_n_dependencies (RigBinding *binding);

void
rig_binding_foreach_dependency (RigBinding *binding,
                                void (*callback) (RigBinding *binding,
                                                  RutProperty *dependency,
                                                  void *user_data),
                                void *user_data);

#endif /* _RIG_BINDING_H_ */
