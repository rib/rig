/*
 * Rig
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RIG_TRANSITION_H_
#define _RIG_TRANSITION_H_

#include "rig.h"
#include "rig-path.h"

extern RigType rig_transition_type;

typedef struct _RigTransition RigTransition;

enum {
  RIG_TRANSITION_PROP_PROGRESS,
  RIG_TRANSITION_N_PROPS
};

struct _RigTransition
{
  RigObjectProps _parent;

  uint32_t id;

  float progress;

  GList *paths;

  RigContext *context;

  RigProperty props[RIG_TRANSITION_N_PROPS];
  RigSimpleIntrospectableProps introspectable;
};

void
rig_transition_set_progress (RigTransition *transition,
                             float progress);

RigPath *
rig_transition_get_path (RigTransition *transition,
                         RigObject *object,
                         const char *property_name);

RigTransition *
rig_transition_new (RigContext *context,
                    uint32_t id);

void
rig_transition_add_path (RigTransition *transition,
                         RigPath *path);

void
rig_transition_free (RigTransition *transition);

#endif /* _RIG_TRANSITION_H_ */
