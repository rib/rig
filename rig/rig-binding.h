/*
 * Rig
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
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
                 int id);

void
rig_binding_add_dependency (RigBinding *binding,
                            RutProperty *property,
                            const char *name);

void
rig_binding_remove_dependency (RigBinding *binding,
                               RutProperty *property);

void
rig_binding_set_expression (RigBinding *binding,
                            const char *expression);

void
rig_binding_set_dependency_name (RigBinding *binding,
                                 RutProperty *property,
                                 const char *name);

#endif /* _RIG_BINDING_H_ */
