/*
 * Rig
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#include <glib.h>

#ifndef __RIG_MEMORY_STACK__
#define __RIG_MEMORY_STACK__

G_BEGIN_DECLS

typedef struct _RigMemoryStack RigMemoryStack;

RigMemoryStack *
rig_memory_stack_new (size_t initial_size_bytes);

void *
rig_memory_stack_alloc (RigMemoryStack *stack, size_t bytes);

void
rig_memory_stack_rewind (RigMemoryStack *stack);

void
rig_memory_stack_free (RigMemoryStack *stack);

G_END_DECLS

#endif /* __RIG_MEMORY_STACK__ */
