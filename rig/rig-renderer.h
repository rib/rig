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

#ifndef _RIG_RENDERER_H_
#define _RIG_RENDERER_H_

#include "rig-types.h"
#include "rig-entity.h"
#include "rig-frontend.h"

typedef enum _rig_pass_t {
    RIG_PASS_COLOR_UNBLENDED,
    RIG_PASS_COLOR_BLENDED,
    RIG_PASS_SHADOW,
    RIG_PASS_DOF_DEPTH
} rig_pass_t;

typedef struct _rig_paint_context_t {
    rut_paint_context_t _parent;

    rig_engine_t *engine;
    rut_object_t *renderer;

    c_llist_t *camera_stack;

    rig_pass_t pass;

    bool enable_dof;

} rig_paint_context_t;

typedef struct _rig_renderer_t rig_renderer_t;

extern rut_type_t rig_renderer_type;

rig_renderer_t *rig_renderer_new(rig_frontend_t *frontend);

c_array_t *rig_journal_new(void);

void
paint_camera_entity_pass(rig_paint_context_t *paint_ctx,
                         rig_entity_t *camera_entity);

void rig_renderer_dirty_entity_state(rig_entity_t *entity);

void rig_renderer_init(rig_renderer_t *renderer);

void rig_renderer_fini(rig_renderer_t *renderer);

void rig_renderer_paint_camera(rig_paint_context_t *paint_ctx,
                               rig_entity_t *camera_entity);

#endif /* _RIG_RENDERER_H_ */
