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

#include <config.h>

#include "rig-undo-journal.h"
#include "rig-engine.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

typedef struct _UndoRedoOpImpl
{
  void (*apply) (RigUndoJournal *journal, UndoRedo *undo_redo);
  UndoRedo *(*invert) (UndoRedo *src);
  void (*free) (UndoRedo *undo_redo);
} UndoRedoOpImpl;

static bool
rig_undo_journal_insert (RigUndoJournal *journal,
                         UndoRedo *undo_redo);

static UndoRedo *
rig_undo_journal_revert (RigUndoJournal *journal);

static void
undo_redo_apply (RigUndoJournal *journal, UndoRedo *undo_redo);

static UndoRedo *
undo_redo_invert (UndoRedo *undo_redo);

static void
undo_redo_free (UndoRedo *undo_redo);

static void
dump_journal (RigUndoJournal *journal, int indent);

static void
dump_op (UndoRedo *op, int indent)
{
  switch (op->op)
    {
    case UNDO_REDO_SET_PROPERTY_OP:
      {
        char *v0_string =
          rut_boxed_to_string (&op->d.set_property.value0,
                               op->d.set_property.property->spec);
        char *v1_string =
          rut_boxed_to_string (&op->d.set_property.value1,
                               op->d.set_property.property->spec);

        g_print ("%*sproperty (\"%s\") change: %s → %s\n",
                 indent, "",
                 op->d.set_property.property->spec->name,
                 v0_string,
                 v1_string);

        g_free (v0_string);
        g_free (v1_string);
        break;
      }
    case UNDO_REDO_CONST_PROPERTY_CHANGE_OP:
      {
        char *v0_string =
          rut_boxed_to_string (&op->d.set_controller_const.value0,
                               op->d.set_controller_const.property->spec);
        char *v1_string =
          rut_boxed_to_string (&op->d.set_controller_const.value1,
                               op->d.set_controller_const.property->spec);

        g_print ("%*scontroller (\"%s\") const property (\"%s\") change: %s → %s\n",
                 indent, "",
                 op->d.set_controller_const.controller->label,
                 op->d.set_controller_const.property->spec->name,
                 v0_string,
                 v1_string);
        break;
      }
    case UNDO_REDO_SET_CONTROLLED_OP:
      g_print ("%*scontrolled=%s\n",
               indent, "",
               op->d.set_controlled.value ? "yes" : "no");
      break;

    case UNDO_REDO_PATH_ADD_OP:
      g_print ("%*spath add\n", indent, "");
      break;
    case UNDO_REDO_PATH_MODIFY_OP:
      g_print ("%*spath modify\n", indent, "");
      break;
    case UNDO_REDO_PATH_REMOVE_OP:
      g_print ("%*sremove path\n", indent, "");
      break;
    case UNDO_REDO_SET_CONTROL_METHOD_OP:
      g_print ("%*sset control method\n", indent, "");
      break;
    case UNDO_REDO_ADD_ENTITY_OP:
      g_print ("%*sadd entity\n", indent, "");
      break;
    case UNDO_REDO_DELETE_ENTITY_OP:
      g_print ("%*sdelete entity\n", indent, "");
      break;
    case UNDO_REDO_ADD_COMPONENT_OP:
      g_print ("%*sadd component\n", indent, "");
      break;
    case UNDO_REDO_DELETE_COMPONENT_OP:
      g_print ("%*sdelete component\n", indent, "");
      break;
    case UNDO_REDO_SUBJOURNAL_OP:
      g_print ("%*ssub-journal %p\n", indent, "", op->d.subjournal);
      dump_journal (op->d.subjournal, indent + 5);
      break;
    default:
      g_print ("%*sTODO\n", indent, "");
      break;
    }
}

static void
dump_journal (RigUndoJournal *journal, int indent)
{
  UndoRedo *undo_redo;

  g_print ("\n\n%*sJournal %p\n", indent, "", journal);
  indent += 2;

  if (!rut_list_empty (&journal->redo_ops))
    {
      rut_list_for_each (undo_redo, &journal->redo_ops, list_node)
        dump_op (undo_redo, indent);

      g_print ("%*s %25s REDO OPS\n", indent, "", "");
      g_print ("%*s %25s <-----\n", indent, "", "");
      g_print ("%*s %25s UNDO OPS\n", indent, "", "");
    }

  rut_list_for_each_reverse (undo_redo, &journal->undo_ops, list_node)
    dump_op (undo_redo, indent);
}

static UndoRedo *
revert_recent_controller_constant_change (RigUndoJournal *journal,
                                          RigController *controller,
                                          RutProperty *property)
{
  if (!rut_list_empty (&journal->undo_ops))
    {
      UndoRedo *last_op =
        rut_container_of (journal->undo_ops.prev, last_op, list_node);

      if (last_op->op == UNDO_REDO_CONST_PROPERTY_CHANGE_OP &&
          last_op->d.set_controller_const.controller == controller &&
          last_op->d.set_controller_const.property == property &&
          last_op->mergable)
        {
          return rig_undo_journal_revert (journal);
        }
    }

  return NULL;
}

void
rig_undo_journal_set_controller_constant (RigUndoJournal *journal,
                                          bool mergable,
                                          RigController *controller,
                                          const RutBoxed *value,
                                          RutProperty *property)
{
  UndoRedo *undo_redo;
  UndoRedoSetControllerConst *prop_change;
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  g_return_if_fail (prop_data != NULL);

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo = revert_recent_controller_constant_change (journal,
                                                             controller,
                                                             property)))
    {
      prop_change = &undo_redo->d.set_controller_const;
      rut_boxed_destroy (&prop_change->value1);
      rut_boxed_copy (&prop_change->value1, value);
    }
  else
    {
      undo_redo = g_slice_new (UndoRedo);

      undo_redo->op = UNDO_REDO_CONST_PROPERTY_CHANGE_OP;
      undo_redo->mergable = mergable;

      prop_change = &undo_redo->d.set_controller_const;
      prop_change->controller = rut_object_ref (controller);

      rut_boxed_copy (&prop_change->value0, &prop_data->constant_value);
      rut_boxed_copy (&prop_change->value1, value);

      prop_change->object = rut_object_ref (property->object);
      prop_change->property = property;
    }

  rig_undo_journal_insert (journal, undo_redo);
}

static UndoRedo *
revert_recent_controller_path_change (RigUndoJournal *journal,
                                      RigController *controller,
                                      float t,
                                      RutProperty *property)
{
  if (!rut_list_empty (&journal->undo_ops))
    {
      UndoRedo *last_op =
        rut_container_of (journal->undo_ops.prev, last_op, list_node);

      if (last_op->op == UNDO_REDO_PATH_ADD_OP &&
          last_op->d.path_add_remove.controller == controller &&
          last_op->d.path_add_remove.property == property &&
          last_op->d.path_add_remove.t == t &&
          last_op->mergable)
        {
          return rig_undo_journal_revert (journal);
        }

      if (last_op->op == UNDO_REDO_PATH_MODIFY_OP &&
          last_op->d.path_add_remove.controller == controller &&
          last_op->d.path_modify.property == property &&
          last_op->d.path_modify.t == t &&
          last_op->mergable)
        {
          return rig_undo_journal_revert (journal);
        }
    }

  return NULL;
}

void
rig_undo_journal_set_controller_path_node_value (RigUndoJournal *journal,
                                                 CoglBool mergable,
                                                 RigController *controller,
                                                 float t,
                                                 const RutBoxed *value,
                                                 RutProperty *property)
{
  UndoRedo *undo_redo;
  RigPath *path = rig_controller_get_path_for_property (controller, property);

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo = revert_recent_controller_path_change (journal,
                                                         controller,
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
    }
  else
    {
      RutBoxed old_value;
      float normalized_t = t / rig_controller_get_length (controller);

      undo_redo = g_slice_new (UndoRedo);

      if (rig_path_get_boxed (path, normalized_t, &old_value))
        {
          UndoRedoPathModify *modify = &undo_redo->d.path_modify;

          undo_redo->op = UNDO_REDO_PATH_MODIFY_OP;
          modify->controller = rut_object_ref (controller);
          modify->object = rut_object_ref (property->object);
          modify->property = property;
          modify->t = t;
          modify->value0 = old_value;
          rut_boxed_copy (&modify->value1, value);
        }
      else
        {
          UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;

          undo_redo->op = UNDO_REDO_PATH_ADD_OP;
          add_remove->controller = rut_object_ref (controller);
          add_remove->object = rut_object_ref (property->object);
          add_remove->property = property;
          add_remove->t = t;
          rut_boxed_copy (&add_remove->value, value);
          add_remove->have_value = true;
        }

      undo_redo->mergable = mergable;
    }

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_remove_controller_path_node (RigUndoJournal *journal,
                                              RigController *controller,
                                              RutProperty *property,
                                              float t)
{
  UndoRedoPathAddRemove *add_remove;
  UndoRedo *undo_redo = g_slice_new (UndoRedo);

  add_remove = &undo_redo->d.path_add_remove;

  undo_redo->op = UNDO_REDO_PATH_REMOVE_OP;
  undo_redo->mergable = FALSE;
  add_remove->controller = rut_object_ref (controller);
  add_remove->object = rut_object_ref (property->object);
  add_remove->property = property;
  add_remove->t = t;
  add_remove->have_value = false;

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_set_controlled (RigUndoJournal *journal,
                                 RigController *controller,
                                 RutProperty *property,
                                 bool value)
{
  UndoRedo *undo_redo;
  UndoRedoSetControlled *set_controlled;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_SET_CONTROLLED_OP;
  undo_redo->mergable = FALSE;

  set_controlled = &undo_redo->d.set_controlled;

  set_controlled->controller = rut_object_ref (controller);
  set_controlled->object = rut_object_ref (property->object);
  set_controlled->property = property;
  set_controlled->value = value;

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_set_control_method (RigUndoJournal *journal,
                                     RigController *controller,
                                     RutProperty *property,
                                     RigControllerMethod method)
{
  UndoRedo *undo_redo;
  UndoRedoSetControlMethod *set_control_method;
  RigControllerPropData *prop_data =
    rig_controller_find_prop_data_for_property (controller, property);

  g_return_if_fail (prop_data != NULL);

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_SET_CONTROL_METHOD_OP;
  undo_redo->mergable = FALSE;

  set_control_method = &undo_redo->d.set_control_method;

  set_control_method->controller = rut_object_ref (controller);
  set_control_method->object = rut_object_ref (property->object);
  set_control_method->property = property;
  set_control_method->prev_method = prop_data->method;
  set_control_method->method = method;

  rig_undo_journal_insert (journal, undo_redo);
}

static UndoRedo *
revert_recent_property_change (RigUndoJournal *journal,
                               RutProperty *property)
{
  if (!rut_list_empty (&journal->undo_ops))
    {
      UndoRedo *last_op =
        rut_container_of (journal->undo_ops.prev, last_op, list_node);

      if (last_op->op == UNDO_REDO_SET_PROPERTY_OP &&
          last_op->d.set_property.property == property &&
          last_op->mergable)
        {
          return rig_undo_journal_revert (journal);
        }
    }

  return NULL;
}

void
rig_undo_journal_set_property (RigUndoJournal *journal,
                               bool mergable,
                               const RutBoxed *value,
                               RutProperty *property)
{
  UndoRedo *undo_redo;
  UndoRedoSetProperty *set_property;

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo = revert_recent_property_change (journal, property)))
    {
      set_property = &undo_redo->d.set_property;
      rut_boxed_destroy (&set_property->value1);
      rut_boxed_copy (&set_property->value1, value);
    }
  else
    {
      undo_redo = g_slice_new (UndoRedo);

      undo_redo->op = UNDO_REDO_SET_PROPERTY_OP;
      undo_redo->mergable = mergable;

      set_property = &undo_redo->d.set_property;

      rut_property_box (property, &set_property->value0);
      rut_boxed_copy (&set_property->value1, value);

      set_property->object = rut_object_ref (property->object);
      set_property->property = property;
    }

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_add_entity (RigUndoJournal *journal,
                             RutEntity *parent_entity,
                             RutEntity *entity)
{
  UndoRedo *undo_redo;
  UndoRedoAddDeleteEntity *add_entity;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_ADD_ENTITY_OP;
  undo_redo->mergable = FALSE;

  add_entity = &undo_redo->d.add_delete_entity;

  add_entity->parent_entity = rut_object_ref (parent_entity);
  add_entity->deleted_entity = rut_object_ref (entity);

  /* We assume there aren't currently any controller references to
   * this entity. */
  rut_list_init (&add_entity->controller_properties);
  add_entity->saved_controller_properties = true;

  rig_undo_journal_insert (journal, undo_redo);
}

static void
delete_entity_component_cb (RutComponent *component,
                            void *user_data)
{
  RigUndoJournal *journal = user_data;
  rig_undo_journal_delete_component (journal, component);
}

void
rig_undo_journal_delete_entity (RigUndoJournal *journal,
                                RutEntity *entity)
{
  RigUndoJournal *sub_journal = rig_undo_journal_new (journal->engine);
  UndoRedo *undo_redo;
  UndoRedoAddDeleteEntity *delete_entity;
  RutEntity *parent = rut_graphable_get_parent (entity);

  rut_entity_foreach_component_safe (entity,
                                     delete_entity_component_cb,
                                     sub_journal);

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_DELETE_ENTITY_OP;
  undo_redo->mergable = FALSE;

  delete_entity = &undo_redo->d.add_delete_entity;

  delete_entity->parent_entity = rut_object_ref (parent);
  delete_entity->deleted_entity = rut_object_ref (entity);

  delete_entity->saved_controller_properties = false;

  rig_undo_journal_insert (sub_journal, undo_redo);
  rig_undo_journal_log_subjournal (journal, sub_journal);
}

void
rig_undo_journal_add_component (RigUndoJournal *journal,
                                RutEntity *entity,
                                RutObject *component)
{
  UndoRedo *undo_redo;
  UndoRedoAddDeleteComponent *add_component;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_ADD_COMPONENT_OP;
  undo_redo->mergable = FALSE;

  add_component = &undo_redo->d.add_delete_component;

  add_component->parent_entity = rut_object_ref (entity);
  add_component->deleted_component = rut_object_ref (component);

  /* We assume there are no controller references to the entity
   * currently */
  rut_list_init (&add_component->controller_properties);
  add_component->saved_controller_properties = true;

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_delete_component (RigUndoJournal *journal,
                                   RutObject *component)
{
  UndoRedo *undo_redo;
  UndoRedoAddDeleteComponent *delete_component;
  RutComponentableProps *componentable =
    rut_object_get_properties (component, RUT_TRAIT_ID_COMPONENTABLE);
  RutEntity *entity = componentable->entity;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_DELETE_COMPONENT_OP;
  undo_redo->mergable = FALSE;

  delete_component = &undo_redo->d.add_delete_component;

  delete_component->parent_entity = rut_object_ref (entity);
  delete_component->deleted_component = rut_object_ref (component);

  delete_component->saved_controller_properties = false;

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_log_add_controller (RigUndoJournal *journal,
                                     RigController *controller)
{
  UndoRedo *undo_redo = g_slice_new0 (UndoRedo);
  UndoRedoAddRemoveController *add_controller =
    &undo_redo->d.add_remove_controller;

  undo_redo->op = UNDO_REDO_ADD_CONTROLLER_OP;
  undo_redo->mergable = false;

  add_controller = &undo_redo->d.add_remove_controller;

  add_controller->controller = rut_object_ref (controller);

  g_warn_if_fail (rig_controller_get_active (controller) == false);
  add_controller->active_state = false;

  /* We assume there are no controller references to this controller
   * currently */
  rut_list_init (&add_controller->controller_properties);
  add_controller->saved_controller_properties = true;

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_log_remove_controller (RigUndoJournal *journal,
                                        RigController *controller)
{
  UndoRedo *undo_redo = g_slice_new0 (UndoRedo);
  UndoRedoAddRemoveController *remove_controller =
    &undo_redo->d.add_remove_controller;

  undo_redo->op = UNDO_REDO_REMOVE_CONTROLLER_OP;
  undo_redo->mergable = false;

  remove_controller->controller = rut_object_ref (controller);

  remove_controller->saved_controller_properties = false;

  rig_undo_journal_insert (journal, undo_redo);
}

void
rig_undo_journal_log_subjournal (RigUndoJournal *journal,
                                 RigUndoJournal *subjournal)
{
  UndoRedo *undo_redo;

  /* It indicates a programming error to be logging a subjournal with
   * ::apply_on_insert enabled into a journal with ::apply_on_insert
   * disabled. */

  g_return_if_fail (journal->apply_on_insert == true ||
                    subjournal->apply_on_insert == false);

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_SUBJOURNAL_OP;
  undo_redo->mergable = FALSE;

  undo_redo->d.subjournal = subjournal;

  rig_undo_journal_insert (journal, undo_redo);
}

static void
undo_redo_subjournal_apply (RigUndoJournal *journal,
                            UndoRedo *undo_redo)
{
  RigUndoJournal *subjournal = undo_redo->d.subjournal;

  rut_list_for_each (undo_redo, &subjournal->undo_ops, list_node)
    undo_redo_apply (journal, undo_redo);
}

static UndoRedo *
undo_redo_subjournal_invert (UndoRedo *undo_redo_src)
{
  RigUndoJournal *subjournal_src = undo_redo_src->d.subjournal;
  RigUndoJournal *subjournal_dst;
  UndoRedo *inverse, *sub_undo_redo;

  subjournal_dst = rig_undo_journal_new (subjournal_src->engine);

  rut_list_for_each (sub_undo_redo, &subjournal_src->undo_ops, list_node)
    {
      UndoRedo *subinverse = undo_redo_invert (sub_undo_redo);
      /* Insert at the beginning so that the list will end up in the
       * reverse order */
      rut_list_insert (&subjournal_dst->undo_ops, &subinverse->list_node);
    }

  inverse = g_slice_dup (UndoRedo, undo_redo_src);
  inverse->d.subjournal= subjournal_dst;

  return inverse;
}

static void
undo_redo_subjournal_free (UndoRedo *undo_redo)
{
  rig_undo_journal_free (undo_redo->d.subjournal);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_set_property_apply (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  UndoRedoSetProperty *set_property = &undo_redo->d.set_property;
  RigEngine *engine = journal->engine;

  rig_frontend_op_set_property (engine->frontend,
                                set_property->property,
                                &set_property->value1);
}

static UndoRedo *
undo_redo_set_property_invert (UndoRedo *undo_redo_src)
{
  UndoRedoSetProperty *src = &undo_redo_src->d.set_property;
  UndoRedo *undo_redo_inverse = g_slice_new (UndoRedo);
  UndoRedoSetProperty *inverse =
    &undo_redo_inverse->d.set_property;

  undo_redo_inverse->op = undo_redo_src->op;
  undo_redo_inverse->mergable = FALSE;

  inverse->object = rut_object_ref (src->object);
  inverse->property = src->property;
  inverse->value0 = src->value1;
  inverse->value1 = src->value0;

  return undo_redo_inverse;
}

static void
undo_redo_set_property_free (UndoRedo *undo_redo)
{
  UndoRedoSetProperty *set_property =
    &undo_redo->d.set_property;
  rut_object_unref (set_property->object);
  rut_boxed_destroy (&set_property->value0);
  rut_boxed_destroy (&set_property->value1);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_set_controller_const_apply (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  UndoRedoSetControllerConst *set_controller_const = &undo_redo->d.set_controller_const;

  rig_controller_set_property_constant (set_controller_const->controller,
                                        set_controller_const->property,
                                        &set_controller_const->value1);

  rig_reload_inspector_property (journal->engine,
                                 set_controller_const->property);
}

static UndoRedo *
undo_redo_set_controller_const_invert (UndoRedo *undo_redo_src)
{
  UndoRedoSetControllerConst *src = &undo_redo_src->d.set_controller_const;
  UndoRedo *undo_redo_inverse = g_slice_new (UndoRedo);
  UndoRedoSetControllerConst *inverse =
    &undo_redo_inverse->d.set_controller_const;

  undo_redo_inverse->op = undo_redo_src->op;
  undo_redo_inverse->mergable = FALSE;

  inverse->controller = rut_object_ref (src->controller);
  inverse->object = rut_object_ref (src->object);
  inverse->property = src->property;
  inverse->value0 = src->value1;
  inverse->value1 = src->value0;

  return undo_redo_inverse;
}

static void
undo_redo_set_controller_const_free (UndoRedo *undo_redo)
{
  UndoRedoSetControllerConst *set_controller_const =
    &undo_redo->d.set_controller_const;
  rut_object_unref (set_controller_const->object);
  rut_object_unref (set_controller_const->controller);
  rut_boxed_destroy (&set_controller_const->value0);
  rut_boxed_destroy (&set_controller_const->value1);
  g_slice_free (UndoRedo, undo_redo);
}


static void
undo_redo_path_add_apply (RigUndoJournal *journal,
                          UndoRedo *undo_redo)
{
  UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;
  RigEngine *engine = journal->engine;

  g_return_if_fail (add_remove->have_value == true);

  rig_controller_insert_path_value (add_remove->controller,
                                    add_remove->property,
                                    add_remove->t,
                                    &add_remove->value);

  rig_reload_inspector_property (engine, add_remove->property);
}

static UndoRedo *
undo_redo_path_add_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  inverse->op = UNDO_REDO_PATH_REMOVE_OP;
  inverse->d.path_add_remove.have_value =
    undo_redo_src->d.path_add_remove.have_value;
  if (inverse->d.path_add_remove.have_value)
    {
      rut_boxed_copy (&inverse->d.path_add_remove.value,
                      &undo_redo_src->d.path_add_remove.value);
    }
  rut_object_ref (inverse->d.path_add_remove.object);
  rut_object_ref (inverse->d.path_add_remove.controller);

  return inverse;
}

static void
undo_redo_path_remove_apply (RigUndoJournal *journal,
                             UndoRedo *undo_redo)
{
  UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;
  RigEngine *engine = journal->engine;

  if (!add_remove->have_value)
    {
      rig_controller_box_path_value (add_remove->controller,
                                     add_remove->property,
                                     add_remove->t,
                                     &add_remove->value);

      add_remove->have_value = true;
    }

  rig_controller_remove_path_value (add_remove->controller,
                                    add_remove->property,
                                    add_remove->t);

  rig_reload_inspector_property (engine, add_remove->property);
}

static UndoRedo *
undo_redo_path_remove_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  inverse->op = UNDO_REDO_PATH_ADD_OP;
  inverse->d.path_add_remove.have_value =
    undo_redo_src->d.path_add_remove.have_value;
  if (inverse->d.path_add_remove.have_value)
    {
      rut_boxed_copy (&inverse->d.path_add_remove.value,
                      &undo_redo_src->d.path_add_remove.value);
    }
  rut_object_ref (inverse->d.path_add_remove.object);
  rut_object_ref (inverse->d.path_add_remove.controller);

  return inverse;
}

static void
undo_redo_path_add_remove_free (UndoRedo *undo_redo)
{
  UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;
  if (add_remove->have_value)
    rut_boxed_destroy (&add_remove->value);
  rut_object_unref (add_remove->object);
  rut_object_unref (add_remove->controller);
  g_slice_free (UndoRedo, undo_redo);
}


static void
undo_redo_path_modify_apply (RigUndoJournal *journal,
                             UndoRedo *undo_redo)
{
  UndoRedoPathModify *modify = &undo_redo->d.path_modify;
  RigEngine *engine = journal->engine;

  rig_controller_insert_path_value (modify->controller,
                                    modify->property,
                                    modify->t,
                                    &modify->value1);

  rig_reload_inspector_property (engine, modify->property);
}

static UndoRedo *
undo_redo_path_modify_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  rut_boxed_copy (&inverse->d.path_modify.value0,
                  &undo_redo_src->d.path_modify.value1);
  rut_boxed_copy (&inverse->d.path_modify.value1,
                  &undo_redo_src->d.path_modify.value0);
  rut_object_ref (inverse->d.path_modify.object);
  rut_object_ref (inverse->d.path_modify.controller);

  return inverse;
}

static void
undo_redo_path_modify_free (UndoRedo *undo_redo)
{
  UndoRedoPathModify *modify = &undo_redo->d.path_modify;
  rut_boxed_destroy (&modify->value0);
  rut_boxed_destroy (&modify->value1);
  rut_object_unref (modify->object);
  rut_object_unref (modify->controller);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_set_controlled_apply (RigUndoJournal *journal,
                                UndoRedo *undo_redo)
{
  UndoRedoSetControlled *set_controlled = &undo_redo->d.set_controlled;
  RigEngine *engine = journal->engine;

  if (set_controlled->value)
    rig_controller_add_property (set_controlled->controller,
                                 set_controlled->property);
  else
    rig_controller_remove_property (set_controlled->controller,
                                    set_controlled->property);

  rig_reload_inspector_property (engine, set_controlled->property);
}

static UndoRedo *
undo_redo_set_controlled_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  inverse->d.set_controlled.value = !inverse->d.set_controlled.value;

  rut_object_ref (inverse->d.set_controlled.object);
  rut_object_ref (inverse->d.set_controlled.controller);

  return inverse;
}

static void
undo_redo_set_controlled_free (UndoRedo *undo_redo)
{
  UndoRedoSetControlled *set_controlled = &undo_redo->d.set_controlled;
  rut_object_unref (set_controlled->object);
  rut_object_unref (set_controlled->controller);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_set_control_method_apply (RigUndoJournal *journal,
                                    UndoRedo *undo_redo)
{
  UndoRedoSetControlMethod *set_control_method =
    &undo_redo->d.set_control_method;
  RigEngine *engine = journal->engine;

  rig_controller_set_property_method (set_control_method->controller,
                                      set_control_method->property,
                                      set_control_method->method);

  rig_reload_inspector_property (engine, set_control_method->property);
}

static UndoRedo *
undo_redo_set_control_method_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);
  RigControllerMethod tmp;

  tmp = inverse->d.set_control_method.method;
  inverse->d.set_control_method.method =
    inverse->d.set_control_method.prev_method;
  inverse->d.set_control_method.prev_method = tmp;

  rut_object_ref (inverse->d.set_control_method.object);
  rut_object_ref (inverse->d.set_control_method.controller);

  return inverse;
}

static void
undo_redo_set_control_method_free (UndoRedo *undo_redo)
{
  UndoRedoSetControlMethod *set_control_method = &undo_redo->d.set_control_method;
  rut_object_unref (set_control_method->object);
  rut_object_unref (set_control_method->controller);
  g_slice_free (UndoRedo, undo_redo);
}

static void
copy_controller_property_list (RutList *src, RutList *dst)
{
  UndoRedoPropData *prop_data;

  rut_list_for_each (prop_data, src, link)
    {
      UndoRedoPropData *prop_data_copy = g_slice_new (UndoRedoPropData);

      prop_data_copy->property = prop_data->property;
      prop_data_copy->method = prop_data->method;
      prop_data_copy->path =
        prop_data->path ? rut_object_ref (prop_data->path) : NULL;
      rut_boxed_copy (&prop_data_copy->constant_value,
                      &prop_data->constant_value);

      rut_list_insert (dst->prev, &prop_data_copy->link);
    }
}

typedef struct _UndoRedoControllerState
{
  RutList link;

  RigController *controller;
  RutList properties;
} UndoRedoControllerState;

static void
copy_controller_references (RutList *src_controller_properties,
                            RutList *dst_controller_properties)
{
  UndoRedoControllerState *src_controller_state;

  rut_list_init (dst_controller_properties);

  rut_list_for_each (src_controller_state, src_controller_properties, link)
    {
      UndoRedoControllerState *dst_controller_state =
        g_slice_new (UndoRedoControllerState);

      dst_controller_state->controller =
        rut_object_ref (src_controller_state->controller);

      rut_list_init (&dst_controller_state->properties);

      rut_list_insert (dst_controller_properties->prev,
                       &dst_controller_state->link);

      copy_controller_property_list (&src_controller_state->properties,
                                     dst_controller_state->properties.prev);
    }
}

static UndoRedo *
copy_add_delete_entity (UndoRedo *undo_redo)
{
  UndoRedo *copy = g_slice_dup (UndoRedo, undo_redo);
  UndoRedoAddDeleteEntity *add_delete_entity = &copy->d.add_delete_entity;

  rut_object_ref (add_delete_entity->parent_entity);
  rut_object_ref (add_delete_entity->deleted_entity);

  copy_controller_references (&undo_redo->d.add_delete_entity.controller_properties,
                              &add_delete_entity->controller_properties);

  return copy;
}

typedef struct
{
  RutObject *object;
  RutList *properties;
} CopyControllerPropertiesData;

static void
copy_controller_property_cb (RigControllerPropData *prop_data,
                             void *user_data)
{
  CopyControllerPropertiesData *data = user_data;

  if (prop_data->property->object == data->object)
    {
      UndoRedoPropData *undo_prop_data = g_slice_new (UndoRedoPropData);

      undo_prop_data->method = prop_data->method;
      rut_boxed_copy (&undo_prop_data->constant_value,
                      &prop_data->constant_value);
      /* As the property's owner is being deleted we can safely just
       * take ownership of the path without worrying about it later
       * being modified.
       */
      undo_prop_data->path =
        prop_data->path ? rut_object_ref (prop_data->path) : NULL;
      undo_prop_data->property = prop_data->property;

      rut_list_insert (data->properties->prev,
                       &undo_prop_data->link);
    }
}

static void
save_controller_properties (RigEngine *engine,
                            RutObject *object,
                            RutList *controller_properties)
{
  CopyControllerPropertiesData copy_properties_data;
  GList *l;

  rut_list_init (controller_properties);

  for (l = engine->controllers; l; l = l->next)
    {
      RigController *controller = l->data;
      UndoRedoControllerState *controller_state =
        g_slice_new (UndoRedoControllerState);

      /* Grab a copy of the controller data for all the properties of the
       * entity */
      rut_list_init (&controller_state->properties);

      copy_properties_data.object = object;
      copy_properties_data.properties = &controller_state->properties;

      rig_controller_foreach_property (controller,
                                       copy_controller_property_cb,
                                       &copy_properties_data);

      if (rut_list_empty (&controller_state->properties))
        {
          g_slice_free (UndoRedoControllerState, controller_state);
          continue;
        }

      controller_state->controller = rut_object_ref (controller);
      rut_list_insert (controller_properties,
                       &controller_state->link);
    }
}

static void
undo_redo_delete_entity_apply (RigUndoJournal *journal,
                               UndoRedo *undo_redo)
{
  UndoRedoAddDeleteEntity *delete_entity = &undo_redo->d.add_delete_entity;
  UndoRedoControllerState *controller_state;

  if (!delete_entity->saved_controller_properties)
    {
      save_controller_properties (journal->engine,
                                  delete_entity->deleted_entity,
                                  &delete_entity->controller_properties);
      delete_entity->saved_controller_properties = true;
    }

  rut_graphable_remove_child (delete_entity->deleted_entity);

  rut_list_for_each (controller_state,
                     &delete_entity->controller_properties, link)
    {
      UndoRedoPropData *prop_data;

      rut_list_for_each (prop_data, &controller_state->properties, link)
        rig_controller_remove_property (controller_state->controller,
                                        prop_data->property);
    }
}

static UndoRedo *
undo_redo_delete_entity_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = copy_add_delete_entity (undo_redo_src);

  inverse->op = UNDO_REDO_ADD_ENTITY_OP;

  return inverse;
}

static void
add_controller_properties (RigController *controller, RutList *properties)
{
  UndoRedoPropData *undo_prop_data;

  rut_list_for_each (undo_prop_data, properties, link)
    {
      rig_controller_add_property (controller, undo_prop_data->property);

      if (undo_prop_data->path)
        {
          RigPath *path;
          path = rig_path_copy (undo_prop_data->path);
          rig_controller_set_property_path (controller,
                                            undo_prop_data->property, path);
          rut_object_unref (path);
        }

      rig_controller_set_property_constant (controller, undo_prop_data->property,
                                            &undo_prop_data->constant_value);

      rig_controller_set_property_method (controller,
                                          undo_prop_data->property,
                                          undo_prop_data->method);
    }
}

static void
undo_redo_add_entity_apply (RigUndoJournal *journal,
                            UndoRedo *undo_redo)
{
  UndoRedoAddDeleteEntity *add_entity = &undo_redo->d.add_delete_entity;
  UndoRedoControllerState *controller_state;

  rut_graphable_add_child (add_entity->parent_entity,
                           add_entity->deleted_entity);
  rut_list_for_each (controller_state,
                     &add_entity->controller_properties, link)
    {
      add_controller_properties (controller_state->controller,
                                 &controller_state->properties);
    }

  rut_shell_queue_redraw (journal->engine->ctx->shell);
}

static UndoRedo *
undo_redo_add_entity_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = copy_add_delete_entity (undo_redo_src);

  inverse->op = UNDO_REDO_DELETE_ENTITY_OP;

  return inverse;
}

static void
free_controller_properties (RutList *controller_properties)
{
  UndoRedoControllerState *controller_state, *tmp;

  rut_list_for_each_safe (controller_state, tmp, controller_properties, link)
    {
      UndoRedoPropData *prop_data, *tmp1;
      rut_list_for_each_safe (prop_data, tmp1,
                              &controller_state->properties, link)
        {
          if (prop_data->path)
            rut_object_unref (prop_data->path);

          rut_boxed_destroy (&prop_data->constant_value);

          g_slice_free (UndoRedoPropData, prop_data);
        }

      g_slice_free (UndoRedoControllerState, controller_state);
    }
}

static void
undo_redo_add_delete_entity_free (UndoRedo *undo_redo)
{
  UndoRedoAddDeleteEntity *add_delete_entity = &undo_redo->d.add_delete_entity;

  rut_object_unref (add_delete_entity->parent_entity);
  rut_object_unref (add_delete_entity->deleted_entity);

  free_controller_properties (&add_delete_entity->controller_properties);

  g_slice_free (UndoRedo, undo_redo);
}

static UndoRedo *
copy_add_delete_component (UndoRedo *undo_redo)
{
  UndoRedo *copy = g_slice_dup (UndoRedo, undo_redo);
  UndoRedoAddDeleteComponent *add_delete_component = &copy->d.add_delete_component;

  rut_object_ref (add_delete_component->parent_entity);
  rut_object_ref (add_delete_component->deleted_component);

  copy_controller_references (&undo_redo->d.add_delete_component.controller_properties,
                              &add_delete_component->controller_properties);

  return copy;
}

static void
undo_redo_delete_component_apply (RigUndoJournal *journal,
                                  UndoRedo *undo_redo)
{
  UndoRedoAddDeleteComponent *delete_component = &undo_redo->d.add_delete_component;
  UndoRedoControllerState *controller_state;

  if (!delete_component->saved_controller_properties)
    {
      save_controller_properties (journal->engine,
                                  delete_component->deleted_component,
                                  &delete_component->controller_properties);
      delete_component->saved_controller_properties = true;
    }

  rut_list_for_each (controller_state,
                     &delete_component->controller_properties, link)
    {
      UndoRedoPropData *prop_data;

      rut_list_for_each (prop_data, &controller_state->properties, link)
        rig_controller_remove_property (controller_state->controller,
                                        prop_data->property);
    }

  rut_entity_remove_component (delete_component->parent_entity,
                               delete_component->deleted_component);

  _rig_engine_update_inspector (journal->engine);
}

static UndoRedo *
undo_redo_delete_component_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = copy_add_delete_component (undo_redo_src);

  inverse->op = UNDO_REDO_ADD_COMPONENT_OP;

  return inverse;
}

static void
undo_redo_add_component_apply (RigUndoJournal *journal,
                               UndoRedo *undo_redo)
{
  UndoRedoAddDeleteComponent *add_component = &undo_redo->d.add_delete_component;
  UndoRedoControllerState *controller_state;

  rut_entity_add_component (add_component->parent_entity,
                            add_component->deleted_component);

  rut_list_for_each (controller_state,
                     &add_component->controller_properties, link)
    {
      add_controller_properties (controller_state->controller,
                                 &controller_state->properties);
    }

  _rig_engine_update_inspector (journal->engine);
}

static UndoRedo *
undo_redo_add_component_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = copy_add_delete_component (undo_redo_src);

  inverse->op = UNDO_REDO_DELETE_COMPONENT_OP;

  return inverse;
}

static void
undo_redo_add_delete_component_free (UndoRedo *undo_redo)
{
  UndoRedoAddDeleteComponent *add_delete_component =
    &undo_redo->d.add_delete_component;

  rut_object_unref (add_delete_component->parent_entity);
  rut_object_unref (add_delete_component->deleted_component);

  free_controller_properties (&add_delete_component->controller_properties);

  g_slice_free (UndoRedo, undo_redo);
}

static UndoRedo *
copy_add_remove_controller (UndoRedo *undo_redo)
{
  UndoRedo *copy = g_slice_dup (UndoRedo, undo_redo);
  UndoRedoAddRemoveController *add_remove_controller =
    &copy->d.add_remove_controller;

  rut_object_ref (add_remove_controller->controller);

  copy_controller_references (&undo_redo->d.add_remove_controller.controller_properties,
                              &add_remove_controller->controller_properties);

  return copy;
}

static void
undo_redo_add_controller_apply (RigUndoJournal *journal,
                                UndoRedo *undo_redo)
{
  UndoRedoAddRemoveController *add_controller =
    &undo_redo->d.add_remove_controller;
  UndoRedoControllerState *controller_state;
  RigEngine *engine = journal->engine;

  engine->controllers =
    g_list_prepend (engine->controllers, add_controller->controller);
  rut_object_ref (add_controller->controller);

  rut_list_for_each (controller_state,
                     &add_controller->controller_properties, link)
    {
      add_controller_properties (controller_state->controller,
                                 &controller_state->properties);
    }

#warning "xxx: It's possible that redoing a controller-add for an active controller might result in conflicting bindings if the side effects of other controller timelines has resulted in enabling another controller that conflicts with this one"
  /* XXX: not sure a.t.m how to make this a reliable operation,
   * considering that other active controllers could lead to a conflict.
   *
   * We could potentially save the active/running/offset etc state of
   * all controllers so we can revert to this state before applying
   * the undo-redo operation. Assuming all controllers are
   * deterministic this would reset everything to how it was when
   * first applied.
   *
   * This will be fragile though and since we want to support
   * non-deterministic controllers we need a better solution.
   *
   * A solution that would work, but be rather expensive is to take a
   * snapshot of everything whenever anything is changed by the user
   * so that whenever we need to undo something we can revert
   * everything to the exact state is was in at the time it was
   * changed. It's expected that this would become prohibitively
   * expensive as UIs become more complex though.
   *
   * As a speed optimization we could have another mirror of the scene
   * graph, a bit like we do for the simulator process except this
   * mirror is only updated at each edit operation. In addition we can
   * then snoop on the stream of property changes sent by the
   * simulator and using this mirrored graph as a reference point we
   * can keep a log of all property changes that happen as a
   * side-effect between user edits. To reduce the size of logged
   * side-effect property changes we could make sure to disable
   * timelines when in edit mode.
   */
  rig_controller_set_active (add_controller->controller,
                             add_controller->active_state);

  rig_controller_view_update_controller_list (engine->controller_view);

  rig_controller_view_set_controller (engine->controller_view,
                                      add_controller->controller);
}

static UndoRedo *
undo_redo_add_controller_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = copy_add_remove_controller (undo_redo_src);

  inverse->op = UNDO_REDO_REMOVE_CONTROLLER_OP;

  return inverse;
}

static void
undo_redo_remove_controller_apply (RigUndoJournal *journal,
                                   UndoRedo *undo_redo)
{
  UndoRedoAddRemoveController *remove_controller =
    &undo_redo->d.add_remove_controller;
  UndoRedoControllerState *controller_state;
  RigEngine *engine = journal->engine;

  if (!remove_controller->saved_controller_properties)
    {
      save_controller_properties (journal->engine,
                                  remove_controller->controller,
                                  &remove_controller->controller_properties);
      remove_controller->saved_controller_properties = true;
    }

  remove_controller->active_state =
    rig_controller_get_active (remove_controller->controller);
  rig_controller_set_active (remove_controller->controller, false);

  rut_list_for_each (controller_state,
                     &remove_controller->controller_properties, link)
    {
      UndoRedoPropData *prop_data;

      rut_list_for_each (prop_data, &controller_state->properties, link)
        rig_controller_remove_property (controller_state->controller,
                                        prop_data->property);
    }

  engine->controllers =
    g_list_remove (engine->controllers, remove_controller->controller);
  rut_object_unref (remove_controller->controller);

  rig_controller_view_update_controller_list (engine->controller_view);

  if (rig_controller_view_get_controller (engine->controller_view) ==
      remove_controller->controller)
    {
      rig_controller_view_set_controller (engine->controller_view,
                                          engine->controllers->data);
    }
}

static UndoRedo *
undo_redo_remove_controller_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = copy_add_remove_controller (undo_redo_src);

  inverse->op = UNDO_REDO_ADD_CONTROLLER_OP;

  return inverse;
}

static void
undo_redo_add_remove_controller_free (UndoRedo *undo_redo)
{
  UndoRedoAddRemoveController *add_remove_controller =
    &undo_redo->d.add_remove_controller;

  rut_object_unref (add_remove_controller->controller);

  free_controller_properties (&add_remove_controller->controller_properties);

  g_slice_free (UndoRedo, undo_redo);
}

static UndoRedoOpImpl undo_redo_ops[] =
  {
    {
      undo_redo_subjournal_apply,
      undo_redo_subjournal_invert,
      undo_redo_subjournal_free,
    },
    {
      undo_redo_set_property_apply,
      undo_redo_set_property_invert,
      undo_redo_set_property_free,
    },
    {
      undo_redo_set_controlled_apply,
      undo_redo_set_controlled_invert,
      undo_redo_set_controlled_free,
    },
    {
      undo_redo_set_control_method_apply,
      undo_redo_set_control_method_invert,
      undo_redo_set_control_method_free,
    },
    {
      undo_redo_set_controller_const_apply,
      undo_redo_set_controller_const_invert,
      undo_redo_set_controller_const_free,
    },
    {
      undo_redo_path_add_apply,
      undo_redo_path_add_invert,
      undo_redo_path_add_remove_free,
    },
    {
      undo_redo_path_remove_apply,
      undo_redo_path_remove_invert,
      undo_redo_path_add_remove_free,
    },
    {
      undo_redo_path_modify_apply,
      undo_redo_path_modify_invert,
      undo_redo_path_modify_free,
    },
    {
      undo_redo_add_entity_apply,
      undo_redo_add_entity_invert,
      undo_redo_add_delete_entity_free,
    },
    {
      undo_redo_delete_entity_apply,
      undo_redo_delete_entity_invert,
      undo_redo_add_delete_entity_free,
    },
    {
      undo_redo_add_component_apply,
      undo_redo_add_component_invert,
      undo_redo_add_delete_component_free,
    },
    {
      undo_redo_delete_component_apply,
      undo_redo_delete_component_invert,
      undo_redo_add_delete_component_free,
    },
    {
      undo_redo_add_controller_apply,
      undo_redo_add_controller_invert,
      undo_redo_add_remove_controller_free,
    },
    {
      undo_redo_remove_controller_apply,
      undo_redo_remove_controller_invert,
      undo_redo_add_remove_controller_free,
    },

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

static bool
rig_undo_journal_insert (RigUndoJournal *journal,
                         UndoRedo *undo_redo)
{
  bool apply;

  g_return_val_if_fail (undo_redo != NULL, FALSE);
  g_return_val_if_fail (journal->inserting == FALSE, FALSE);

  rig_undo_journal_flush_redos (journal);

  journal->inserting = TRUE;

  apply = journal->apply_on_insert;

  /* If we are inserting a journal where the operations have already
   * been applied then we don't want to re-apply them if this journal
   * normally also applys operations when inserting them... */
  if (undo_redo->op == UNDO_REDO_SUBJOURNAL_OP &&
      undo_redo->d.subjournal->apply_on_insert)
    {
      apply = false;
    }

  if (apply)
    {
      UndoRedo *inverse;

      /* Purely for testing purposes we now redundantly apply the
       * operation followed by the inverse of the operation so we are
       * alway verifying our ability to invert operations correctly...
       */
      undo_redo_apply (journal, undo_redo);

      /* XXX: For now we have stopped exercising the inversion code
       * for each undo-redo entry because it raises simulator
       * synchronization difficulties (for operations that can't
       * be inverted until they have been applied at least once we
       * would need to wait until the simulator has responded to
       * each operation before moving on)
       */
#warning "TODO: improve how we synchronize making scene edits with the simulator"
#if 0
      /* XXX: Some operations can't be inverted until they have been
       * applied once. For example the UndoRedoPathAddRemove operation
       * will save the value of a path node when it is removed so the
       * node can be re-added later, but until we have saved that
       * value we can't invert the operation */
      inverse = undo_redo_invert (undo_redo);
      g_warn_if_fail (inverse != NULL);

      if (inverse)
        {
          undo_redo_apply (journal, inverse);
          undo_redo_apply (journal, undo_redo);
          undo_redo_free (inverse);
        }
#endif
    }

  rut_list_insert (journal->undo_ops.prev, &undo_redo->list_node);

  dump_journal (journal, 0);

  journal->inserting = FALSE;

  rig_engine_sync_slaves (journal->engine);

  return TRUE;
}

/* This api can be used to undo the last operation without freeing
 * the UndoRedo struct so that it can be modified and re-inserted.
 *
 * We use this to handle modifying mergable operations so we avoid
 * having to special case applying the changes of a modification.
 */
static UndoRedo *
rig_undo_journal_revert (RigUndoJournal *journal)
{
  UndoRedo *op = rut_container_of (journal->undo_ops.prev, op, list_node);

  if (journal->apply_on_insert)
    {
      /* XXX: we should probably be making sure to sync with the
       * simulator here. Some operations can't be inverted until
       * they have been applied first.
       */

      UndoRedo *inverse = undo_redo_invert (op);
      RigPBSerializer *serializer;

      if (!inverse)
        {
          g_warning ("Not allowing undo of operation that can't be inverted");
          return NULL;
        }

      undo_redo_apply (journal, inverse);
      undo_redo_free (inverse);
    }

  rut_list_remove (&op->list_node);

  return op;
}

CoglBool
rig_undo_journal_undo (RigUndoJournal *journal)
{
  if (!rut_list_empty (&journal->undo_ops))
    {
      UndoRedo *op = rig_undo_journal_revert (journal);
      if (!op)
        return false;

      rut_list_insert (journal->redo_ops.prev, &op->list_node);

      rut_shell_queue_redraw (journal->engine->shell);

      dump_journal (journal, 0);

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

  undo_redo_apply (journal, op);
  rut_list_remove (&op->list_node);
  rut_list_insert (journal->undo_ops.prev, &op->list_node);

  rut_shell_queue_redraw (journal->engine->shell);

  dump_journal (journal, 0);

  return TRUE;
}

RigUndoJournal *
rig_undo_journal_new (RigEngine *engine)
{
  RigUndoJournal *journal = g_slice_new0 (RigUndoJournal);

  journal->engine = engine;
  rut_list_init (&journal->undo_ops);
  rut_list_init (&journal->redo_ops);

  return journal;
}

void
rig_undo_journal_set_apply_on_insert (RigUndoJournal *journal,
                                      bool apply_on_insert)
{
  journal->apply_on_insert = apply_on_insert;
}

bool
rig_undo_journal_is_empty (RigUndoJournal *journal)
{
  return (rut_list_length (&journal->undo_ops) == 0 &&
          rut_list_length (&journal->redo_ops) == 0);
}

void
rig_undo_journal_free (RigUndoJournal *journal)
{
  UndoRedo *node, *tmp;

  rut_list_for_each_safe (node, tmp, &journal->undo_ops, list_node)
    undo_redo_free (node);
  rut_list_for_each_safe (node, tmp, &journal->redo_ops, list_node)
    undo_redo_free (node);

  g_slice_free (RigUndoJournal, journal);
}
