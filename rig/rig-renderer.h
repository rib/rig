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

typedef enum _RigPass
{
  RIG_PASS_COLOR_UNBLENDED,
  RIG_PASS_COLOR_BLENDED,
  RIG_PASS_SHADOW,
  RIG_PASS_DOF_DEPTH
} RigPass;

typedef struct _RigPaintContext
{
  RutPaintContext _parent;

  RigEngine *engine;
  RutObject *renderer;

  CList *camera_stack;

  RigPass pass;

} RigPaintContext;

typedef struct _RigRenderer RigRenderer;

extern RutType rig_renderer_type;

RigRenderer *
rig_renderer_new (RigEngine *engine);

CArray *
rig_journal_new (void);

void
rig_camera_update_view (RigEngine *engine, RigEntity *camera, CoglBool shadow_pass);

void
rig_paint_camera_entity (RigEntity *view_camera,
                         RigPaintContext *paint_ctx,
                         RutObject *play_camera);

void
rig_renderer_dirty_entity_state (RigEntity *entity);

void
rig_renderer_init (RigEngine *engine);

void
rig_renderer_fini (RigEngine *engine);

#endif /* _RIG_RENDERER_H_ */
