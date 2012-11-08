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

#include "rig-types.h"
#include "rut-list.h"
#include "rig-node.h"
#include "rig-path.h"

typedef struct _RigUndoJournal RigUndoJournal;

typedef enum _UndoRedoOp
{
  UNDO_REDO_SUBJOURNAL_OP,
  UNDO_REDO_CONST_PROPERTY_CHANGE_OP,
  UNDO_REDO_PATH_ADD_OP,
  UNDO_REDO_PATH_REMOVE_OP,
  UNDO_REDO_PATH_MODIFY_OP,
  UNDO_REDO_SET_ANIMATED_OP,
  UNDO_REDO_ADD_ENTITY_OP,
  UNDO_REDO_DELETE_ENTITY_OP,
  UNDO_REDO_MOVE_PATH_NODES_OP,
  UNDO_REDO_N_OPS
} UndoRedoOp;

typedef struct _UndoRedoConstPropertyChange
{
  RutObject *object;
  RutProperty *property;
  RutBoxed value0;
  RutBoxed value1;
} UndoRedoConstPropertyChange;

typedef struct _UndoRedoPathAddRemove
{
  RutObject *object;
  RutProperty *property;
  float t;
  RutBoxed value;
} UndoRedoPathAddRemove;

typedef struct _UndoRedoPathModify
{
  RutObject *object;
  RutProperty *property;
  float t;
  RutBoxed value0;
  RutBoxed value1;
} UndoRedoPathModify;

typedef struct _UndoRedoMovedPathNode
{
  RutObject *object;
  RutProperty *property;
  float old_time;
  float new_time;
} UndoRedoMovedPathNode;

typedef struct _UndoRedoMovePathNodes
{
  UndoRedoMovedPathNode *nodes;
  int n_nodes;
} UndoRedoMovePathNodes;

typedef struct _UndoRedoSetAnimated
{
  RutObject *object;
  RutProperty *property;
  CoglBool value;

  /* Initially toggle the animated state to TRUE also creates an
   * initial path node at the current timeline position unless there
   * already is one. We want to be able to undo that in the same
   * action so the values for that initial node are also stored
   * here. */
  CoglBool creates_path_node;
  float path_time;
  RutBoxed node_value;
} UndoRedoSetAnimated;

typedef struct
{
  RutList link;

  RutProperty *property;

  CoglBool animated;
  RigPath *path;
  RutBoxed constant_value;
} UndoRedoPropData;

typedef struct _UndoRedoAddDeleteEntity
{
  RutEntity *parent_entity;
  RutEntity *deleted_entity;
  RutList properties;
} UndoRedoAddDeleteEntity;

typedef struct _UndoRedo
{
  RutList list_node;

  UndoRedoOp op;
  CoglBool mergable;
  union
    {
      UndoRedoConstPropertyChange const_prop_change;
      UndoRedoPathAddRemove path_add_remove;
      UndoRedoPathModify path_modify;
      UndoRedoSetAnimated set_animated;
      UndoRedoAddDeleteEntity add_delete_entity;
      UndoRedoMovePathNodes move_path_nodes;
      RigUndoJournal *subjournal;
    } d;
} UndoRedo;

struct _RigUndoJournal
{
  RigData *data;

  /* List of operations that can be undone. The operations are
   * appended to the end of this list so that they are kept in order
   * from the earliest added operation to the last added operation.
   * The operations are not stored inverted so each operation
   * represents the action that user made. */
  RutList undo_ops;

  /* List of operations that can be redone. As the user presses undo,
   * the operations are added to the tail of this list. Therefore the
   * list is in the order of earliest undone operation to the latest.
   * The operations represent the original action that the user made
   * so it will not need to be inverted before redoing the
   * operation. */
  RutList redo_ops;
};

void
rig_undo_journal_set_property_and_log (RigUndoJournal *journal,
                                       CoglBool mergable,
                                       const RutBoxed *value,
                                       RutProperty *property);

typedef struct
{
  RutProperty *property;
  RigNode *node;
} RigUndoJournalPathNode;

void
rig_undo_journal_move_path_nodes_and_log (RigUndoJournal *journal,
                                          float offset,
                                          const RigUndoJournalPathNode *nodes,
                                          int n_path_nodes);

void
rig_undo_journal_move_and_log (RigUndoJournal *journal,
                               CoglBool mergable,
                               RutEntity *entity,
                               float x,
                               float y,
                               float z);

void
rig_undo_journal_delete_path_node_and_log (RigUndoJournal *journal,
                                           RutProperty *property,
                                           RigNode *node);

void
rig_undo_journal_set_animated_and_log (RigUndoJournal *journal,
                                       RutProperty *property,
                                       CoglBool value);

void
rig_undo_journal_add_entity_and_log (RigUndoJournal *journal,
                                     RutEntity *parent_entity,
                                     RutEntity *entity);

void
rig_undo_journal_delete_entity_and_log (RigUndoJournal *journal,
                                        RutEntity *entity);

/**
 * rig_undo_journal_log_subjournal:
 * @journal: The main journal
 * @subjournal: A subjournal to add to @journal
 *
 * Logs a collection of undo entries as a single meta-entry in
 * @journal. The collection is stored in @subjournal which can be
 * built up using the existing journal logging commands. When an undo
 * is performed on the main journal, all of the entries in the
 * subjournal will be performed as a single action. The main journal
 * will take ownership of @subjournal.
 */
void
rig_undo_journal_log_subjournal (RigUndoJournal *journal,
                                 RigUndoJournal *subjournal);

CoglBool
rig_undo_journal_undo (RigUndoJournal *journal);

CoglBool
rig_undo_journal_redo (RigUndoJournal *journal);

RigUndoJournal *
rig_undo_journal_new (RigData *data);

void
rig_undo_journal_free (RigUndoJournal *journal);

#endif /* _RUT_UNDO_JOURNAL_H_ */
