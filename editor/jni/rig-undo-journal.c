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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rig-undo-journal.h"

typedef struct _UndoRedoOpImpl
{
  void (*apply) (RigUndoJournal *journal, UndoRedo *undo_redo);
  UndoRedo *(*invert) (UndoRedo *src);
  void (*free) (UndoRedo *undo_redo);
} UndoRedoOpImpl;

static CoglBool
rig_undo_journal_insert (RigUndoJournal *journal, UndoRedo *undo_redo);

static void
undo_redo_apply (RigUndoJournal *journal, UndoRedo *undo_redo);

static UndoRedo *
undo_redo_invert (UndoRedo *undo_redo);

static void
undo_redo_free (UndoRedo *undo_redo);

static UndoRedo *
rig_undo_journal_find_recent_property_change (RigUndoJournal *journal,
                                              RigProperty *property)
{
  if (journal->pos &&
      journal->pos == journal->ops.tail)
    {
      UndoRedo *recent = journal->pos->data;
      if (recent->d.prop_change.property == property &&
          recent->mergable)
        return recent;
    }

  return NULL;
}

void
rig_undo_journal_log_move (RigUndoJournal *journal,
                           CoglBool mergable,
                           RigEntity *entity,
                           float prev_x,
                           float prev_y,
                           float prev_z,
                           float x,
                           float y,
                           float z)
{
  RigProperty *position =
    rig_introspectable_lookup_property (entity, "position");
  UndoRedo *undo_redo;
  UndoRedoPropertyChange *prop_change;

  if (mergable)
    {
      undo_redo = rig_undo_journal_find_recent_property_change (journal,
                                                                position);
      if (undo_redo)
        {
          prop_change = &undo_redo->d.prop_change;
          /* NB: when we are merging then the existing operation is an
           * inverse of a normal move operation so the new move
           * location goes into value0... */
          prop_change->value0.d.vec3_val[0] = x;
          prop_change->value0.d.vec3_val[1] = y;
          prop_change->value0.d.vec3_val[2] = z;
        }
    }

  undo_redo = g_slice_new (UndoRedo);

  undo_redo->op = UNDO_REDO_PROPERTY_CHANGE_OP;
  undo_redo->mergable = mergable;

  prop_change = &undo_redo->d.prop_change;
  prop_change->entity = rig_ref_countable_ref (entity);
  prop_change->property = position;

  prop_change->value0.type = RIG_PROPERTY_TYPE_VEC3;
  prop_change->value0.d.vec3_val[0] = prev_x;
  prop_change->value0.d.vec3_val[1] = prev_y;
  prop_change->value0.d.vec3_val[2] = prev_z;

  prop_change->value1.type = RIG_PROPERTY_TYPE_VEC3;
  prop_change->value1.d.vec3_val[0] = x;
  prop_change->value1.d.vec3_val[1] = y;
  prop_change->value1.d.vec3_val[2] = z;

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_copy_property_and_log (RigUndoJournal *journal,
                                        CoglBool mergable,
                                        RigEntity *entity,
                                        RigProperty *target_prop,
                                        RigProperty *source_prop)
{
  UndoRedo *undo_redo;
  UndoRedoPropertyChange *prop_change;

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo =
       rig_undo_journal_find_recent_property_change (journal, target_prop)))
    {
      prop_change = &undo_redo->d.prop_change;
      /* NB: when we are merging then the existing operation is an
       * inverse of a normal move operation so the new move location
       * goes into value0... */
      rig_boxed_destroy (&prop_change->value0);
      rig_property_box (source_prop, &prop_change->value0);
      rig_property_set_boxed (&journal->data->ctx->property_ctx,
                              target_prop,
                              &prop_change->value0);
    }
  else
    {
      undo_redo = g_slice_new (UndoRedo);

      undo_redo->op = UNDO_REDO_PROPERTY_CHANGE_OP;
      undo_redo->mergable = mergable;

      prop_change = &undo_redo->d.prop_change;

      rig_property_box (target_prop, &prop_change->value0);
      rig_property_box (source_prop, &prop_change->value1);

      prop_change = &undo_redo->d.prop_change;
      prop_change->entity = rig_ref_countable_ref (entity);
      prop_change->property = target_prop;

      rig_property_set_boxed (&journal->data->ctx->property_ctx,
                              target_prop,
                              &prop_change->value1);

      rig_undo_journal_insert (journal, undo_redo);
    }
}

static void
undo_redo_prop_change_apply (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  UndoRedoPropertyChange *prop_change = &undo_redo->d.prop_change;

  g_print ("Property change APPLY\n");

  rig_property_set_boxed (&journal->data->ctx->property_ctx,
                          prop_change->property, &prop_change->value1);
}

static UndoRedo *
undo_redo_prop_change_invert (UndoRedo *undo_redo_src)
{
  UndoRedoPropertyChange *src = &undo_redo_src->d.prop_change;
  UndoRedo *undo_redo_inverse = g_slice_new (UndoRedo);
  UndoRedoPropertyChange *inverse = &undo_redo_inverse->d.prop_change;

  undo_redo_inverse->op = undo_redo_src->op;
  undo_redo_inverse->mergable = FALSE;

  inverse->entity = rig_ref_countable_ref (src->entity);
  inverse->property = src->property;
  inverse->value0 = src->value1;
  inverse->value1 = src->value0;

  return undo_redo_inverse;
}

static void
undo_redo_prop_change_free (UndoRedo *undo_redo)
{
  UndoRedoPropertyChange *prop_change = &undo_redo->d.prop_change;
  rig_ref_countable_unref (prop_change->entity);
  g_slice_free (UndoRedo, undo_redo);
}

static UndoRedoOpImpl undo_redo_ops[] =
  {
      {
        undo_redo_prop_change_apply,
        undo_redo_prop_change_invert,
        undo_redo_prop_change_free
      }
  };

static void
undo_redo_apply (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  g_return_if_fail (undo_redo->op < UNDO_REDO_N_OPS);

  undo_redo_ops[undo_redo->op].apply (journal, undo_redo);
}

static UndoRedo *
undo_redo_invert (UndoRedo *undo_redo)
{
  g_return_val_if_fail (undo_redo->op < UNDO_REDO_N_OPS, NULL);

  return undo_redo_ops[undo_redo->op].invert (undo_redo);
}

static void
undo_redo_free (UndoRedo *undo_redo)
{
  g_return_if_fail (undo_redo->op < UNDO_REDO_N_OPS);

  undo_redo_ops[undo_redo->op].free (undo_redo);
}

static void
rig_undo_journal_flush_redos (RigUndoJournal *journal)
{
  UndoRedo *redo;
  while ((redo = g_queue_pop_head (&journal->redo_ops)))
    g_queue_push_tail (&journal->ops, redo);
  journal->pos = journal->ops.tail;
}

static CoglBool
rig_undo_journal_insert (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  UndoRedo *inverse = undo_redo_invert (undo_redo);

  g_return_val_if_fail (inverse != NULL, FALSE);

  rig_undo_journal_flush_redos (journal);

  /* Purely for testing purposes we now redundantly apply
   * the inverse of the operation followed by the operation
   * itself which should leave us where we started and
   * if not we should hopefully notice quickly!
   */
  undo_redo_apply (journal, inverse);
  undo_redo_apply (journal, undo_redo);

  undo_redo_free (undo_redo);

  g_queue_push_tail (&journal->ops, inverse);
  journal->pos = journal->ops.tail;

  return TRUE;
}

CoglBool
rig_undo_journal_undo (RigUndoJournal *journal)
{
  g_print ("UNDO\n");
  if (journal->pos)
    {
      UndoRedo *redo = undo_redo_invert (journal->pos->data);
      if (!redo)
        {
          g_warning ("Not allowing undo of operation that can't be inverted");
          return FALSE;
        }
      g_queue_push_tail (&journal->redo_ops, redo);

      undo_redo_apply (journal, journal->pos->data);
      journal->pos = journal->pos->prev;

      rig_shell_queue_redraw (journal->data->shell);
      return TRUE;
    }
  else
    return FALSE;
}

CoglBool
rig_undo_journal_redo (RigUndoJournal *journal)
{
  UndoRedo *redo = g_queue_pop_tail (&journal->redo_ops);

  if (!redo)
    return FALSE;

  g_print ("REDO\n");

  undo_redo_apply (journal, redo);

  if (journal->pos)
    journal->pos = journal->pos->next;
  else
    journal->pos = journal->ops.head;

  rig_shell_queue_redraw (journal->data->shell);

  return TRUE;
}

RigUndoJournal *
rig_undo_journal_new (RigData *data)
{
  RigUndoJournal *journal = g_new0 (RigUndoJournal, 1);

  g_queue_init (&journal->ops);
  journal->data = data;
  journal->pos = NULL;
  g_queue_init (&journal->redo_ops);

  return journal;
}


