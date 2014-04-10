/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2014  Intel Corporation.
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


#ifndef _RIG_SLAVE_H
#define _RIG_SLAVE_H

#include <rut.h>

#include "rig-frontend.h"

typedef struct _RigSlave
{
  RutObjectBase _base;

  RutShell *shell;
  RutContext *ctx;

  int request_width;
  int request_height;
  int request_scale;

  RigFrontend *frontend;
  RigEngine *engine;

  GHashTable *edit_id_to_play_object_map;
  GHashTable *play_object_to_edit_id_map;

  RigPBUnSerializer *ui_unserializer;

  RigEngineOpMapContext map_op_ctx;
  RigEngineOpApplyContext apply_op_ctx;

  RutClosure *ui_update_closure;
  RutQueue *pending_edits;

  RutClosure *ui_load_closure;
  const Rig__UI *pending_ui_load;
  Rig__LoadResult_Closure pending_ui_load_closure;
  void *pending_ui_load_closure_data;

} RigSlave;


RigSlave *
rig_slave_new (int width, int height, int scale);

void
rig_slave_run (RigSlave *slave);

void
rig_slave_print_mappings (RigSlave *slave);

#endif /* _RIG_SLAVE_H */
