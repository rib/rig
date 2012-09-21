/*
 * Rut
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _RUT_UNDO_JOURNAL_H_
#define _RUT_UNDO_JOURNAL_H_

#include <glib.h>

#include <rut.h>

#include "rig-data.h"

typedef enum _UndoRedoOp
{
  UNDO_REDO_PROPERTY_CHANGE_OP,
  UNDO_REDO_N_OPS
} UndoRedoOp;

typedef struct _UndoRedoPropertyChange
{
  RutEntity *entity;
  RutProperty *property;
  RutBoxed value0;
  RutBoxed value1;
} UndoRedoPropertyChange;

typedef struct _UndoRedo
{
  UndoRedoOp op;
  CoglBool mergable;
  union
    {
      UndoRedoPropertyChange prop_change;
    } d;
} UndoRedo;

struct _RigUndoJournal
{
  RigData *data;
  GQueue ops;
  GList *pos;
  GQueue redo_ops;
};

void
rig_undo_journal_copy_property_and_log (RigUndoJournal *journal,
                                        CoglBool mergable,
                                        RutEntity *entity,
                                        RutProperty *target_prop,
                                        RutProperty *source_prop);

void
rig_undo_journal_log_move (RigUndoJournal *journal,
                           CoglBool mergable,
                           RutEntity *entity,
                           float prev_x,
                           float prev_y,
                           float prev_z,
                           float x,
                           float y,
                           float z);

CoglBool
rig_undo_journal_undo (RigUndoJournal *journal);

CoglBool
rig_undo_journal_redo (RigUndoJournal *journal);

RigUndoJournal *
rig_undo_journal_new (RigData *data);

#endif /* _RUT_UNDO_JOURNAL_H_ */
