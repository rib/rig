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

static void
dump_op (UndoRedo *op,
         GString *buf)
{
  switch (op->op)
    {
    case UNDO_REDO_PROPERTY_CHANGE_OP:
      if (op->d.prop_change.value0.type == RUT_PROPERTY_TYPE_VEC3)
        {
          const float *v = op->d.prop_change.value0.d.vec3_val;
          int i;

          g_string_append_c (buf, '(');

          for (i = 0; i < 3; i++)
            {
              if (i > 0)
                g_string_append_c (buf, ',');
              g_string_append_printf (buf, "%.1f", v[i]);
            }

          g_string_append (buf, ")→(");

          v = op->d.prop_change.value1.d.vec3_val;

          for (i = 0; i < 3; i++)
            {
              if (i > 0)
                g_string_append_c (buf, ',');
              g_string_append_printf (buf, "%.1f", v[i]);
            }

          g_string_append_c (buf, ')');
        }
      else
        g_string_append (buf, "-");
      break;

    default:
      g_string_append (buf, "-");
      break;
    }
}

static void
dump_journal (RigUndoJournal *journal)
{
  GString *buf_a = g_string_new (NULL), *buf_b = g_string_new (NULL);
  UndoRedo *undo, *redo;

  undo = rut_container_of (journal->undo_ops.next, undo, list_node);
  redo = rut_container_of (journal->redo_ops.next, redo, list_node);

  printf ("%-50s%-50s\n", "Undo", "Redo");

  while (&undo->list_node != &journal->undo_ops ||
         &redo->list_node != &journal->redo_ops)
    {
      g_string_set_size (buf_a, 0);
      g_string_set_size (buf_b, 0);

      if (&undo->list_node != &journal->undo_ops)
        {
          dump_op (undo, buf_a);
          undo = rut_container_of (undo->list_node.next, undo, list_node);
        }

      if (&redo->list_node != &journal->redo_ops)
        {
          dump_op (redo, buf_b);
          redo = rut_container_of (redo->list_node.next, redo, list_node);
        }

      printf ("%-50s%-50s\n", buf_a->str, buf_b->str);
    }

  g_string_free (buf_a, TRUE);
  g_string_free (buf_b, TRUE);
}

static UndoRedo *
rig_undo_journal_find_recent_property_change (RigUndoJournal *journal,
                                              RutProperty *property)
{
  if (!rut_list_empty (&journal->undo_ops))
    {
      UndoRedo *last_op =
        rut_container_of (journal->undo_ops.prev, last_op, list_node);

      if (last_op->op == UNDO_REDO_PROPERTY_CHANGE_OP &&
          last_op->d.prop_change.property == property &&
          last_op->mergable)
        return last_op;
    }

  return NULL;
}

void
rig_undo_journal_log_move (RigUndoJournal *journal,
                           CoglBool mergable,
                           RutEntity *entity,
                           float prev_x,
                           float prev_y,
                           float prev_z,
                           float x,
                           float y,
                           float z)
{
  RutProperty *position =
    rut_introspectable_lookup_property (entity, "position");
  UndoRedo *undo_redo;
  UndoRedoPropertyChange *prop_change;

  if (mergable)
    {
      undo_redo = rig_undo_journal_find_recent_property_change (journal,
                                                                position);
      if (undo_redo)
        {
          prop_change = &undo_redo->d.prop_change;
          prop_change->value1.d.vec3_val[0] = x;
          prop_change->value1.d.vec3_val[1] = y;
          prop_change->value1.d.vec3_val[2] = z;
        }
    }

  undo_redo = g_slice_new (UndoRedo);

  undo_redo->op = UNDO_REDO_PROPERTY_CHANGE_OP;
  undo_redo->mergable = mergable;

  prop_change = &undo_redo->d.prop_change;
  prop_change->entity = rut_refable_ref (entity);
  prop_change->property = position;

  prop_change->value0.type = RUT_PROPERTY_TYPE_VEC3;
  prop_change->value0.d.vec3_val[0] = prev_x;
  prop_change->value0.d.vec3_val[1] = prev_y;
  prop_change->value0.d.vec3_val[2] = prev_z;

  prop_change->value1.type = RUT_PROPERTY_TYPE_VEC3;
  prop_change->value1.d.vec3_val[0] = x;
  prop_change->value1.d.vec3_val[1] = y;
  prop_change->value1.d.vec3_val[2] = z;

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_copy_property_and_log (RigUndoJournal *journal,
                                        CoglBool mergable,
                                        RutEntity *entity,
                                        RutProperty *target_prop,
                                        RutProperty *source_prop)
{
  UndoRedo *undo_redo;
  UndoRedoPropertyChange *prop_change;

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo =
       rig_undo_journal_find_recent_property_change (journal, target_prop)))
    {
      prop_change = &undo_redo->d.prop_change;
      rut_boxed_destroy (&prop_change->value1);
      rut_property_box (source_prop, &prop_change->value1);
      rut_property_set_boxed (&journal->data->ctx->property_ctx,
                              target_prop,
                              &prop_change->value1);
    }
  else
    {
      undo_redo = g_slice_new (UndoRedo);

      undo_redo->op = UNDO_REDO_PROPERTY_CHANGE_OP;
      undo_redo->mergable = mergable;

      prop_change = &undo_redo->d.prop_change;

      rut_property_box (target_prop, &prop_change->value0);
      rut_property_box (source_prop, &prop_change->value1);

      prop_change = &undo_redo->d.prop_change;
      prop_change->entity = rut_refable_ref (entity);
      prop_change->property = target_prop;

      rut_property_set_boxed (&journal->data->ctx->property_ctx,
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

  rut_property_set_boxed (&journal->data->ctx->property_ctx,
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

  inverse->entity = rut_refable_ref (src->entity);
  inverse->property = src->property;
  inverse->value0 = src->value1;
  inverse->value1 = src->value0;

  return undo_redo_inverse;
}

static void
undo_redo_prop_change_free (UndoRedo *undo_redo)
{
  UndoRedoPropertyChange *prop_change = &undo_redo->d.prop_change;
  rut_refable_unref (prop_change->entity);
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
  RutList reversed_operations;
  UndoRedo *l, *tmp;

  /* Build a list of inverted operations out of the redo list. These
   * will be added two the end of the undo list so that the previously
   * undone actions themselves become undoable actions */
  rut_list_init (&reversed_operations);

  rut_list_for_each (l, &journal->redo_ops, list_node)
    {
      UndoRedo *inverted = undo_redo_invert (l);

      /* Add the inverted node to the end so it will keep the same
       * order */
      if (inverted)
        rut_list_insert (reversed_operations.prev, &inverted->list_node);
    }

  /* Add all of the redo operations again in reverse order so that if
   * the user undoes past all of the redoes to put them back into the
   * state they were before the undoes, they will be able to continue
   * undoing to undo those actions again */
  rut_list_for_each_reverse_safe (l, tmp, &journal->redo_ops, list_node)
    rut_list_insert (journal->undo_ops.prev, &l->list_node);
  rut_list_init (&journal->redo_ops);

  rut_list_insert_list (journal->undo_ops.prev, &reversed_operations);
}

static CoglBool
rig_undo_journal_insert (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  UndoRedo *inverse;

  g_return_val_if_fail (undo_redo != NULL, FALSE);

  rig_undo_journal_flush_redos (journal);

  /* Purely for testing purposes we now redundantly apply
   * the inverse of the operation followed by the operation
   * itself which should leave us where we started and
   * if not we should hopefully notice quickly!
   */
  inverse = undo_redo_invert (undo_redo);
  undo_redo_apply (journal, inverse);
  undo_redo_apply (journal, undo_redo);
  undo_redo_free (inverse);

  rut_list_insert (journal->undo_ops.prev, &undo_redo->list_node);

  dump_journal (journal);

  return TRUE;
}

CoglBool
rig_undo_journal_undo (RigUndoJournal *journal)
{
  g_print ("UNDO\n");
  if (!rut_list_empty (&journal->undo_ops))
    {
      UndoRedo *op = rut_container_of (journal->undo_ops.prev, op, list_node);
      UndoRedo *inverse = undo_redo_invert (op);

      if (!inverse)
        {
          g_warning ("Not allowing undo of operation that can't be inverted");
          return FALSE;
        }

      rut_list_remove (&op->list_node);
      rut_list_insert (journal->redo_ops.prev, &op->list_node);

      undo_redo_apply (journal, inverse);
      undo_redo_free (inverse);

      rut_shell_queue_redraw (journal->data->shell);

      dump_journal (journal);

      return TRUE;
    }
  else
    return FALSE;
}

CoglBool
rig_undo_journal_redo (RigUndoJournal *journal)
{
  UndoRedo *op;

  if (rut_list_empty (&journal->redo_ops))
    return FALSE;

  op = rut_container_of (journal->redo_ops.prev, op, list_node);

  g_print ("REDO\n");

  undo_redo_apply (journal, op);
  rut_list_remove (&op->list_node);
  rut_list_insert (journal->undo_ops.prev, &op->list_node);

  rut_shell_queue_redraw (journal->data->shell);

  dump_journal (journal);

  return TRUE;
}

RigUndoJournal *
rig_undo_journal_new (RigData *data)
{
  RigUndoJournal *journal = g_new0 (RigUndoJournal, 1);

  journal->data = data;
  rut_list_init (&journal->undo_ops);
  rut_list_init (&journal->redo_ops);

  return journal;
}


