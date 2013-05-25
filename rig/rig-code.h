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

#ifndef __RIG_CODE_H__
#define __RIG_CODE_H__

typedef struct _RigCodeNode RigCodeNode;

#include "rig-engine.h"

G_BEGIN_DECLS

void
_rig_code_init (RigEngine *engine);

void
_rig_code_fini (RigEngine *engine);

RigCodeNode *
rig_code_node_new (RigEngine *engine,
                   const char *pre,
                   const char *post);

void
rig_code_node_set_pre (RigCodeNode *node,
                       const char *pre);

void
rig_code_node_set_post (RigCodeNode *node,
                        const char *post);

typedef void (*RigCodeNodeLinkCallback) (RigCodeNode *node, void *user_data);

RutClosure *
rig_code_node_add_link_callback (RigCodeNode *node,
                                 RigCodeNodeLinkCallback callback,
                                 void *user_data,
                                 RutClosureDestroyCallback destroy);

void
rig_code_node_add_child (RigCodeNode *node,
                         RigCodeNode *child);

void
rig_code_node_remove_child (RigCodeNode *child);

/* A callback for whenever a particular node has been relinked and
 * reloaded so we can e.g. re-query symbol addresses.
 */
void
rig_code_node_add_reload_callback (RigCodeNode *node);

void *
rig_code_resolve_symbol (RigEngine *engine, const char *name);

void
rig_code_update_dso (RigEngine *engine, uint8_t *data, int len);

G_END_DECLS

#endif /* __RIG_CODE_H__ */
