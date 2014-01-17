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

typedef struct _RigUndoJournal RigUndoJournal;

#include "rig-types.h"
#include "rut-list.h"
#include "rig-node.h"
#include "rig-path.h"
#include "rig-controller.h"
#include "rig-pb.h"

typedef enum _UndoRedoOp
{
  UNDO_REDO_SUBJOURNAL_OP,
  UNDO_REDO_SET_PROPERTY_OP,
  UNDO_REDO_SET_CONTROLLED_OP,
  UNDO_REDO_SET_CONTROL_METHOD_OP,
  UNDO_REDO_CONST_PROPERTY_CHANGE_OP,
  UNDO_REDO_PATH_ADD_OP,
  UNDO_REDO_PATH_REMOVE_OP,
  UNDO_REDO_PATH_MODIFY_OP,
  UNDO_REDO_ADD_ENTITY_OP,
  UNDO_REDO_DELETE_ENTITY_OP,
  UNDO_REDO_ADD_COMPONENT_OP,
  UNDO_REDO_DELETE_COMPONENT_OP,
  UNDO_REDO_ADD_CONTROLLER_OP,
  UNDO_REDO_REMOVE_CONTROLLER_OP,
  UNDO_REDO_N_OPS
} UndoRedoOp;

typedef struct _UndoRedoSetProperty
{
  RutObject *object;
  RutProperty *property;
  RutBoxed value0;
  RutBoxed value1;
} UndoRedoSetProperty;

typedef struct _UndoRedoSetControllerConst
{
  RigController *controller;
  RutObject *object;
  RutProperty *property;
  RutBoxed value0;
  RutBoxed value1;
} UndoRedoSetControllerConst;

typedef struct _UndoRedoPathAddRemove
{
  RigController *controller;
  RutObject *object;
  RutProperty *property;
  float t;

#warning "XXX: figure out how UndoRedoPathAddRemove with async edits via the simulator"
  /* When we initially log to remove a node then we won't save
   * a value until we actually apply the operation and so we
   * need to track when this boxed value is valid... */
  bool have_value;
  RutBoxed value;
} UndoRedoPathAddRemove;

typedef struct _UndoRedoPathModify
{
  RigController *controller;
  RutObject *object;
  RutProperty *property;
  float t;
  RutBoxed value0;
  RutBoxed value1;
} UndoRedoPathModify;

typedef struct _UndoRedoSetControlled
{
  RigController *controller;
  RutObject *object;
  RutProperty *property;
  CoglBool value;
} UndoRedoSetControlled;

typedef struct _UndoRedoSetControlMethod
{
  RigController *controller;
  RutObject *object;
  RutProperty *property;
  RigControllerMethod prev_method;
  RigControllerMethod method;
} UndoRedoSetControlMethod;

typedef struct
{
  RutList link;

  RutProperty *property;

  RigControllerMethod method;
  RigPath *path;
  RutBoxed constant_value;
} UndoRedoPropData;

typedef struct _UndoRedoAddDeleteEntity
{
  RutEntity *parent_entity;
  RutEntity *deleted_entity;
  bool saved_controller_properties;
  RutList controller_properties;
} UndoRedoAddDeleteEntity;

typedef struct _UndoRedoAddDeleteComponent
{
  RutEntity *parent_entity;
  RutObject *deleted_component;
  bool saved_controller_properties;
  RutList controller_properties;
} UndoRedoAddDeleteComponent;

typedef struct _UndoRedoAddRemoveController
{
  RigController *controller;
  bool active_state;
  bool saved_controller_properties;
  RutList controller_properties;
} UndoRedoAddRemoveController;

typedef struct _UndoRedo
{
  RutList list_node;

  UndoRedoOp op;
  CoglBool mergable;
  union
    {
      UndoRedoSetProperty set_property;
      UndoRedoSetControllerConst set_controller_const;
      UndoRedoPathAddRemove path_add_remove;
      UndoRedoPathModify path_modify;
      UndoRedoSetControlled set_controlled;
      UndoRedoSetControlMethod set_control_method;
      UndoRedoAddDeleteEntity add_delete_entity;
      UndoRedoAddDeleteComponent add_delete_component;
      UndoRedoAddRemoveController add_remove_controller;
      RigUndoJournal *subjournal;
    } d;
} UndoRedo;

struct _RigUndoJournal
{
  RigEngine *engine;

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

  /* Detect recursion on insertion which indicates a bug */
  bool inserting;

  /* Whether or not operations should be applied as they are inserted
   * into the journal. By default this is false, so we can create
   * subjournals, log operations and then apply them all together when
   * inserting the subjournal into the master journal. Normally only
   * the top-level, master journal would set this to true.
   */
  bool apply_on_insert;
};

void
rig_undo_journal_log_add_controller (RigUndoJournal *journal,
                                     RigController *controller);

void
rig_undo_journal_log_remove_controller (RigUndoJournal *journal,
                                        RigController *controller);

void
rig_undo_journal_set_controlled (RigUndoJournal *journal,
                                 RigController *controller,
                                 RutProperty *property,
                                 bool value);

void
rig_undo_journal_set_control_method (RigUndoJournal *journal,
                                     RigController *controller,
                                     RutProperty *property,
                                     RigControllerMethod method);

void
rig_undo_journal_set_controller_constant (RigUndoJournal *journal,
                                          bool mergable,
                                          RigController *controller,
                                          const RutBoxed *value,
                                          RutProperty *property);

void
rig_undo_journal_set_controller_path_node_value (RigUndoJournal *journal,
                                                 CoglBool mergable,
                                                 RigController *controller,
                                                 float t,
                                                 const RutBoxed *value,
                                                 RutProperty *property);

void
rig_undo_journal_remove_controller_path_node (RigUndoJournal *journal,
                                              RigController *controller,
                                              RutProperty *property,
                                              float t);

#if 0
typedef struct
{
  RigControllerPropData *prop_data;
  RigNode *node;
} RigUndoJournalPathNode;

void
rig_undo_journal_move_controller_path_nodes (RigUndoJournal *journal,
                                             RigController *controller,
                                             float offset,
                                             const RigUndoJournalPathNode *nodes,
                                             int n_path_nodes);
#endif

void
rig_undo_journal_set_property (RigUndoJournal *journal,
                               bool mergable,
                               const RutBoxed *value,
                               RutProperty *property);

void
rig_undo_journal_add_entity (RigUndoJournal *journal,
                             RutEntity *parent_entity,
                             RutEntity *entity);

void
rig_undo_journal_delete_entity (RigUndoJournal *journal,
                                RutEntity *entity);

void
rig_undo_journal_add_component (RigUndoJournal *journal,
                                RutEntity *entity,
                                RutObject *component);

void
rig_undo_journal_delete_component (RigUndoJournal *journal,
                                   RutObject *component);

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
rig_undo_journal_new (RigEngine *engine);

void
rig_undo_journal_set_apply_on_insert (RigUndoJournal *journal,
                                      bool apply_on_insert);

bool
rig_undo_journal_is_empty (RigUndoJournal *journal);

void
rig_undo_journal_free (RigUndoJournal *journal);

#endif /* _RUT_UNDO_JOURNAL_H_ */
