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
    case UNDO_REDO_CONST_PROPERTY_CHANGE_OP:
      if (op->d.const_prop_change.value0.type == RUT_PROPERTY_TYPE_VEC3)
        {
          const float *v = op->d.const_prop_change.value0.d.vec3_val;
          int i;

          g_string_append_c (buf, '(');

          for (i = 0; i < 3; i++)
            {
              if (i > 0)
                g_string_append_c (buf, ',');
              g_string_append_printf (buf, "%.1f", v[i]);
            }

          g_string_append (buf, ")→(");

          v = op->d.const_prop_change.value1.d.vec3_val;

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

    case UNDO_REDO_SET_ANIMATED_OP:
      g_string_append_printf (buf,
                              "animated=%s",
                              op->d.set_animated.value ? "yes" : "no");
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

      if (last_op->op == UNDO_REDO_CONST_PROPERTY_CHANGE_OP &&
          last_op->d.const_prop_change.property == property &&
          last_op->mergable)
        return last_op;
    }

  return NULL;
}

static void
rig_undo_journal_set_constant_property_and_log (RigUndoJournal *journal,
                                                CoglBool mergable,
                                                RutEntity *entity,
                                                const RutBoxed *value,
                                                RutProperty *property)
{
  RigData *data = journal->data;
  UndoRedo *undo_redo;
  UndoRedoConstPropertyChange *prop_change;
  RigTransitionPropData *prop_data =
    rig_transition_get_prop_data_for_property (data->selected_transition,
                                               property);

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo =
       rig_undo_journal_find_recent_property_change (journal, property)))
    {
      prop_change = &undo_redo->d.const_prop_change;
      rut_boxed_destroy (&prop_change->value1);
      rut_boxed_copy (&prop_change->value1, value);
      rut_property_set_boxed (&journal->data->ctx->property_ctx,
                              property,
                              value);
      rut_boxed_destroy (&prop_data->constant_value);
      rut_boxed_copy (&prop_data->constant_value, value);
    }
  else
    {
      undo_redo = g_slice_new (UndoRedo);

      undo_redo->op = UNDO_REDO_CONST_PROPERTY_CHANGE_OP;
      undo_redo->mergable = mergable;

      prop_change = &undo_redo->d.const_prop_change;

      rut_boxed_copy (&prop_change->value0, &prop_data->constant_value);
      rut_boxed_copy (&prop_change->value1, value);

      prop_change->entity = rut_refable_ref (entity);
      prop_change->property = property;

      rut_property_set_boxed (&journal->data->ctx->property_ctx,
                              property,
                              value);

      rut_boxed_destroy (&prop_data->constant_value);
      rut_boxed_copy (&prop_data->constant_value, value);

      rig_undo_journal_insert (journal, undo_redo);
    }
}

static UndoRedo *
rig_undo_journal_find_recent_timeline_property_change (RigUndoJournal *journal,
                                                       float t,
                                                       RutProperty *property)
{
  if (!rut_list_empty (&journal->undo_ops))
    {
      UndoRedo *last_op =
        rut_container_of (journal->undo_ops.prev, last_op, list_node);

      if (last_op->op == UNDO_REDO_PATH_ADD_OP &&
          last_op->d.path_add_remove.property == property &&
          last_op->d.path_add_remove.t == t &&
          last_op->mergable)
        return last_op;

      if (last_op->op == UNDO_REDO_PATH_MODIFY_OP &&
          last_op->d.path_modify.property == property &&
          last_op->d.path_modify.t == t &&
          last_op->mergable)
        return last_op;
    }

  return NULL;
}

static void
rig_undo_journal_set_timeline_property_and_log (RigUndoJournal *journal,
                                                CoglBool mergable,
                                                RutEntity *entity,
                                                const RutBoxed *value,
                                                RutProperty *property)
{
  RigData *data = journal->data;
  UndoRedo *undo_redo;
  float t = data->selected_transition->progress;
  RigPath *path =
    rig_transition_get_path_for_property (data->selected_transition,
                                          property);

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo =
       rig_undo_journal_find_recent_timeline_property_change (journal,
                                                              t,
                                                              property)))
    {
      if (undo_redo->op == UNDO_REDO_PATH_ADD_OP)
        {
          UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;

          rut_boxed_destroy (&add_remove->value);
          rut_boxed_copy (&add_remove->value, value);
        }
      else
        {
          UndoRedoPathModify *modify = &undo_redo->d.path_modify;

          rut_boxed_destroy (&modify->value1);
          rut_boxed_copy (&modify->value1, value);
        }

      rig_path_insert_boxed (path, t, value);
      rut_property_set_boxed (&journal->data->ctx->property_ctx,
                              property,
                              value);
    }
  else
    {
      RutBoxed old_value;

      undo_redo = g_slice_new (UndoRedo);

      if (rig_path_get_boxed (path, t, &old_value))
        {
          UndoRedoPathModify *modify = &undo_redo->d.path_modify;

          undo_redo->op = UNDO_REDO_PATH_MODIFY_OP;
          modify->entity = rut_refable_ref (entity);
          modify->property = property;
          modify->t = t;
          modify->value0 = old_value;
          rut_boxed_copy (&modify->value1, value);
        }
      else
        {
          UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;

          undo_redo->op = UNDO_REDO_PATH_ADD_OP;
          add_remove->entity = rut_refable_ref (entity);
          add_remove->property = property;
          add_remove->t = t;
          rut_boxed_copy (&add_remove->value, value);
        }

      undo_redo->mergable = mergable;

      rig_path_insert_boxed (path, t, value);
      rut_property_set_boxed (&journal->data->ctx->property_ctx,
                              property,
                              value);

      rig_undo_journal_insert (journal, undo_redo);
    }
}

void
rig_undo_journal_set_property_and_log (RigUndoJournal *journal,
                                       CoglBool mergable,
                                       RutEntity *entity,
                                       const RutBoxed *value,
                                       RutProperty *property)
{
  if (property->animated)
    rig_undo_journal_set_timeline_property_and_log (journal,
                                                    mergable,
                                                    entity,
                                                    value,
                                                    property);
  else
    rig_undo_journal_set_constant_property_and_log (journal,
                                                    mergable,
                                                    entity,
                                                    value,
                                                    property);
}

void
rig_undo_journal_move_and_log (RigUndoJournal *journal,
                               CoglBool mergable,
                               RutEntity *entity,
                               float x,
                               float y,
                               float z)
{
  RutProperty *position =
    rut_introspectable_lookup_property (entity, "position");
  RutBoxed value;

  value.type = RUT_PROPERTY_TYPE_VEC3;
  value.d.vec3_val[0] = x;
  value.d.vec3_val[1] = y;
  value.d.vec3_val[2] = z;

  rig_undo_journal_set_property_and_log (journal,
                                         mergable,
                                         entity,
                                         &value,
                                         position);
}

void
rig_undo_journal_log_set_animated (RigUndoJournal *journal,
                                   RutEntity *entity,
                                   RutProperty *property,
                                   CoglBool value)
{
  UndoRedo *undo_redo;
  UndoRedoSetAnimated *set_animated;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_SET_ANIMATED_OP;
  undo_redo->mergable = FALSE;

  set_animated = &undo_redo->d.set_animated;

  set_animated->entity = rut_refable_ref (entity);
  set_animated->property = property;
  set_animated->value = value;

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_delete_entity_and_log (RigUndoJournal *journal,
                                        RutEntity *entity)
{
  UndoRedo *undo_redo;
  UndoRedoDeleteEntity *delete_entity;
  RutEntity *parent = rut_graphable_get_parent (entity);

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_DELETE_ENTITY_OP;
  undo_redo->mergable = FALSE;

  delete_entity = &undo_redo->d.delete_entity;

  delete_entity->parent_entity = rut_refable_ref (parent);
  delete_entity->deleted_entity = rut_refable_ref (entity);
  delete_entity->inverted = FALSE;

  rut_graphable_remove_child (entity);
  rut_shell_queue_redraw (journal->data->ctx->shell);

  rig_undo_journal_insert (journal, undo_redo);
}

static void
undo_redo_const_prop_change_apply (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  UndoRedoConstPropertyChange *prop_change = &undo_redo->d.const_prop_change;

  g_print ("Property change APPLY\n");

  rut_property_set_boxed (&journal->data->ctx->property_ctx,
                          prop_change->property, &prop_change->value1);
}

static UndoRedo *
undo_redo_const_prop_change_invert (UndoRedo *undo_redo_src)
{
  UndoRedoConstPropertyChange *src = &undo_redo_src->d.const_prop_change;
  UndoRedo *undo_redo_inverse = g_slice_new (UndoRedo);
  UndoRedoConstPropertyChange *inverse =
    &undo_redo_inverse->d.const_prop_change;

  undo_redo_inverse->op = undo_redo_src->op;
  undo_redo_inverse->mergable = FALSE;

  inverse->entity = rut_refable_ref (src->entity);
  inverse->property = src->property;
  inverse->value0 = src->value1;
  inverse->value1 = src->value0;

  return undo_redo_inverse;
}

static void
undo_redo_const_prop_change_free (UndoRedo *undo_redo)
{
  UndoRedoConstPropertyChange *prop_change = &undo_redo->d.const_prop_change;
  rut_refable_unref (prop_change->entity);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_path_add_apply (RigUndoJournal *journal,
                          UndoRedo *undo_redo)
{
  UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;
  RigData *data = journal->data;
  RigPath *path;

  g_print ("Path add APPLY\n");

  path = rig_transition_get_path_for_property (data->selected_transition,
                                               add_remove->property);
  rig_path_insert_boxed (path, add_remove->t, &add_remove->value);

  rig_transition_update_property (data->selected_transition,
                                  add_remove->property);
}

static UndoRedo *
undo_redo_path_add_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  inverse->op = UNDO_REDO_PATH_REMOVE_OP;
  rut_boxed_copy (&inverse->d.path_add_remove.value,
                  &undo_redo_src->d.path_add_remove.value);
  rut_refable_ref (inverse->d.path_add_remove.entity);

  return inverse;
}

static void
undo_redo_path_remove_apply (RigUndoJournal *journal,
                             UndoRedo *undo_redo)
{
  UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;
  RigData *data = journal->data;
  RigPath *path;

  g_print ("Path remove APPLY\n");

  path = rig_transition_get_path_for_property (data->selected_transition,
                                               add_remove->property);
  rig_path_remove (path, add_remove->t);

  rig_transition_update_property (data->selected_transition,
                                  add_remove->property);
}

static UndoRedo *
undo_redo_path_remove_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  inverse->op = UNDO_REDO_PATH_ADD_OP;
  rut_boxed_copy (&inverse->d.path_add_remove.value,
                  &undo_redo_src->d.path_add_remove.value);
  rut_refable_ref (inverse->d.path_add_remove.entity);

  return inverse;
}

static void
undo_redo_path_add_remove_free (UndoRedo *undo_redo)
{
  UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;
  rut_boxed_destroy (&add_remove->value);
  rut_refable_unref (add_remove->entity);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_path_modify_apply (RigUndoJournal *journal,
                             UndoRedo *undo_redo)
{
  UndoRedoPathModify *modify = &undo_redo->d.path_modify;
  RigData *data = journal->data;
  RigPath *path;

  g_print ("Path modify APPLY\n");

  path = rig_transition_get_path_for_property (data->selected_transition,
                                               modify->property);
  rig_path_insert_boxed (path, modify->t, &modify->value1);

  rig_transition_update_property (data->selected_transition,
                                  modify->property);
}

static UndoRedo *
undo_redo_path_modify_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  rut_boxed_copy (&inverse->d.path_modify.value0,
                  &undo_redo_src->d.path_modify.value1);
  rut_boxed_copy (&inverse->d.path_modify.value1,
                  &undo_redo_src->d.path_modify.value0);
  rut_refable_ref (inverse->d.path_modify.entity);

  return inverse;
}

static void
undo_redo_path_modify_free (UndoRedo *undo_redo)
{
  UndoRedoPathModify *modify = &undo_redo->d.path_modify;
  rut_boxed_destroy (&modify->value0);
  rut_boxed_destroy (&modify->value1);
  rut_refable_unref (modify->entity);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_set_animated_apply (RigUndoJournal *journal,
                              UndoRedo *undo_redo)
{
  UndoRedoSetAnimated *set_animated = &undo_redo->d.set_animated;
  RigData *data = journal->data;

  g_print ("Set animated APPLY\n");

  rut_property_set_animated (&data->ctx->property_ctx,
                             set_animated->property,
                             set_animated->value);
}

static UndoRedo *
undo_redo_set_animated_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  inverse->d.set_animated.value = !inverse->d.set_animated.value;

  rut_refable_ref (inverse->d.set_animated.entity);

  return inverse;
}

static void
undo_redo_set_animated_free (UndoRedo *undo_redo)
{
  UndoRedoSetAnimated *set_animated = &undo_redo->d.set_animated;
  rut_refable_unref (set_animated->entity);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_delete_entity_apply (RigUndoJournal *journal,
                               UndoRedo *undo_redo)
{
  UndoRedoDeleteEntity *delete_entity = &undo_redo->d.delete_entity;

  g_print ("Delete entity APPLY\n");

  if (!delete_entity->inverted)
    rut_graphable_remove_child (delete_entity->deleted_entity);
  else
    rut_graphable_add_child (delete_entity->parent_entity,
                             delete_entity->deleted_entity);
}

static UndoRedo *
undo_redo_delete_entity_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);
  UndoRedoDeleteEntity *inverse_delete_entity = &inverse->d.delete_entity;

  rut_refable_ref (inverse_delete_entity->parent_entity);
  rut_refable_ref (inverse_delete_entity->deleted_entity);
  inverse_delete_entity->inverted = !inverse_delete_entity->inverted;

  return inverse;
}

static void
undo_redo_delete_entity_free (UndoRedo *undo_redo)
{
  UndoRedoDeleteEntity *delete_entity = &undo_redo->d.delete_entity;

  rut_refable_unref (delete_entity->parent_entity);
  rut_refable_unref (delete_entity->deleted_entity);
  g_slice_free (UndoRedo, undo_redo);
}

static UndoRedoOpImpl undo_redo_ops[] =
  {
    {
      undo_redo_const_prop_change_apply,
      undo_redo_const_prop_change_invert,
      undo_redo_const_prop_change_free
    },
    {
      undo_redo_path_add_apply,
      undo_redo_path_add_invert,
      undo_redo_path_add_remove_free
    },
    {
      undo_redo_path_remove_apply,
      undo_redo_path_remove_invert,
      undo_redo_path_add_remove_free
    },
    {
      undo_redo_path_modify_apply,
      undo_redo_path_modify_invert,
      undo_redo_path_modify_free
    },
    {
      undo_redo_set_animated_apply,
      undo_redo_set_animated_invert,
      undo_redo_set_animated_free
    },
    {
      undo_redo_delete_entity_apply,
      undo_redo_delete_entity_invert,
      undo_redo_delete_entity_free
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
