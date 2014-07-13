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

#ifndef __RIG_CODE_H__
#define __RIG_CODE_H__

typedef struct _rig_code_node_t rig_code_node_t;

#include "rig-engine.h"

C_BEGIN_DECLS

void _rig_code_init(rig_engine_t *engine);

void _rig_code_fini(rig_engine_t *engine);

rig_code_node_t *
rig_code_node_new(rig_engine_t *engine, const char *pre, const char *post);

void rig_code_node_set_pre(rig_code_node_t *node, const char *pre);

void rig_code_node_set_post(rig_code_node_t *node, const char *post);

typedef void (*rig_code_node_link_callback_t)(rig_code_node_t *node,
                                              void *user_data);

rut_closure_t *
rig_code_node_add_link_callback(rig_code_node_t *node,
                                rig_code_node_link_callback_t callback,
                                void *user_data,
                                rut_closure_destroy_callback_t destroy);

void rig_code_node_add_child(rig_code_node_t *node, rig_code_node_t *child);

void rig_code_node_remove_child(rig_code_node_t *child);

/* A callback for whenever a particular node has been relinked and
 * reloaded so we can e.g. re-query symbol addresses.
 */
void rig_code_node_add_reload_callback(rig_code_node_t *node);

void *rig_code_resolve_symbol(rig_engine_t *engine, const char *name);

void rig_code_update_dso(rig_engine_t *engine, uint8_t *data, int len);

C_END_DECLS

#endif /* __RIG_CODE_H__ */
