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
#include "rig-engine.h"

typedef struct _UndoRedoOpImpl
{
  void (*apply) (RigUndoJournal *journal, UndoRedo *undo_redo);
  UndoRedo *(*invert) (UndoRedo *src);
  void (*free) (UndoRedo *undo_redo);
} UndoRedoOpImpl;

static bool
rig_undo_journal_insert (RigUndoJournal *journal,
                         UndoRedo *undo_redo,
                         bool apply);

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
    case UNDO_REDO_CONST_PROPERTY_CHANGE_OP:
      if (op->d.const_prop_change.value0.type == RUT_PROPERTY_TYPE_VEC3)
        {
          const float *v = op->d.const_prop_change.value0.d.vec3_val;
          int i;

          g_print ("%*sproperty change: vec3 (", indent, "");

          for (i = 0; i < 3; i++)
            {
              if (i > 0)
                g_print (",");
              g_print ("%.1f", v[i]);
            }

          g_print (")→(");

          v = op->d.const_prop_change.value1.d.vec3_val;

          for (i = 0; i < 3; i++)
            {
              if (i > 0)
                g_print (",");
              g_print ("%.1f", v[i]);
            }

          g_print (")\n");
        }
      else
        g_print ("%*sproperty change: TODO\n", indent, "");
      break;

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
    case UNDO_REDO_MOVE_PATH_NODES_OP:
      g_print ("%*sremove path nodes\n", indent, "");
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
rig_undo_journal_find_recent_controller_constant_change (RigUndoJournal *journal,
                                                         RigController *controller,
                                                         RutProperty *property)
{
  if (!rut_list_empty (&journal->undo_ops))
    {
      UndoRedo *last_op =
        rut_container_of (journal->undo_ops.prev, last_op, list_node);

      if (last_op->op == UNDO_REDO_CONST_PROPERTY_CHANGE_OP &&
          last_op->d.const_prop_change.controller == controller &&
          last_op->d.const_prop_change.property == property &&
          last_op->mergable)
        return last_op;
    }

  return NULL;
}

static void
rig_undo_journal_set_controller_constant_and_log (RigUndoJournal *journal,
                                                  CoglBool mergable,
                                                  RigController *controller,
                                                  const RutBoxed *value,
                                                  RutProperty *property)
{
  UndoRedo *undo_redo;
  UndoRedoConstPropertyChange *prop_change;
  RigControllerPropData *prop_data =
    rig_controller_get_prop_data_for_property (controller, property);

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo =
       rig_undo_journal_find_recent_controller_constant_change (journal,
                                                                controller,
                                                                property)))
    {
      prop_change = &undo_redo->d.const_prop_change;
      rut_boxed_destroy (&prop_change->value1);
      rut_boxed_copy (&prop_change->value1, value);
      rut_property_set_boxed (&journal->engine->ctx->property_ctx,
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
      prop_change->controller = rut_refable_ref (controller);

      rut_boxed_copy (&prop_change->value0, &prop_data->constant_value);
      rut_boxed_copy (&prop_change->value1, value);

      prop_change->object = rut_refable_ref (property->object);
      prop_change->property = property;

      rut_property_set_boxed (&journal->engine->ctx->property_ctx,
                              property,
                              value);

      rut_boxed_destroy (&prop_data->constant_value);
      rut_boxed_copy (&prop_data->constant_value, value);

      rig_undo_journal_insert (journal, undo_redo, FALSE);
    }
}

static UndoRedo *
rig_undo_journal_find_recent_controller_path_change (RigUndoJournal *journal,
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
        return last_op;

      if (last_op->op == UNDO_REDO_PATH_MODIFY_OP &&
          last_op->d.path_add_remove.controller == controller &&
          last_op->d.path_modify.property == property &&
          last_op->d.path_modify.t == t &&
          last_op->mergable)
        return last_op;
    }

  return NULL;
}

static void
rig_undo_journal_set_controller_path_property_and_log (RigUndoJournal *journal,
                                                       CoglBool mergable,
                                                       RigController *controller,
                                                       const RutBoxed *value,
                                                       RutProperty *property)
{
  UndoRedo *undo_redo;
  float t = controller->progress;
  RigPath *path = rig_controller_get_path_for_property (controller, property);

  /* If we have a mergable entry then we can just update the final value */
  if (mergable &&
      (undo_redo =
       rig_undo_journal_find_recent_controller_path_change (journal,
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

      rig_path_insert_boxed (path, t, value);
      rut_property_set_boxed (&journal->engine->ctx->property_ctx,
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
          modify->controller = rut_refable_ref (controller);
          modify->object = rut_refable_ref (property->object);
          modify->property = property;
          modify->t = t;
          modify->value0 = old_value;
          rut_boxed_copy (&modify->value1, value);
        }
      else
        {
          UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;

          undo_redo->op = UNDO_REDO_PATH_ADD_OP;
          add_remove->controller = rut_refable_ref (controller);
          add_remove->object = rut_refable_ref (property->object);
          add_remove->property = property;
          add_remove->t = t;
          rut_boxed_copy (&add_remove->value, value);
        }

      undo_redo->mergable = mergable;

      rig_path_insert_boxed (path, t, value);
      rut_property_set_boxed (&journal->engine->ctx->property_ctx,
                              property,
                              value);

      rig_undo_journal_insert (journal, undo_redo, FALSE);
    }
}

void
rig_undo_journal_set_property_and_log (RigUndoJournal *journal,
                                       CoglBool mergable,
                                       RigController *controller,
                                       const RutBoxed *value,
                                       RutProperty *property)
{
  RigControllerPropData *prop_data;

  prop_data =
    rig_controller_get_prop_data_for_property (controller, property);

  if (prop_data && prop_data->method == RIG_CONTROLLER_METHOD_PATH)
    rig_undo_journal_set_controller_path_property_and_log (journal,
                                                           mergable,
                                                           controller,
                                                           value,
                                                           property);
  else
    rig_undo_journal_set_controller_constant_and_log (journal,
                                                      mergable,
                                                      controller,
                                                      value,
                                                      property);
}

void
rig_undo_journal_move_path_nodes_and_log (RigUndoJournal *journal,
                                          RigController *controller,
                                          float offset,
                                          const RigUndoJournalPathNode *nodes,
                                          int n_nodes)
{
  UndoRedo *undo_redo;
  UndoRedoMovePathNodes *move_path_nodes;
  int i;

  undo_redo = g_slice_new (UndoRedo);

  undo_redo->op = UNDO_REDO_MOVE_PATH_NODES_OP;
  undo_redo->mergable = FALSE;

  move_path_nodes = &undo_redo->d.move_path_nodes;
  move_path_nodes->controller = rut_refable_ref (controller);
  move_path_nodes->nodes = g_malloc (sizeof (UndoRedoMovedPathNode) * n_nodes);
  move_path_nodes->n_nodes = n_nodes;

  for (i = 0; i < n_nodes; i++)
    {
      const RigUndoJournalPathNode *node = nodes + i;
      UndoRedoMovedPathNode *moved_node = move_path_nodes->nodes + i;
      RigControllerPropData *prop_data =
        rig_controller_get_prop_data_for_property (controller, node->property);

      moved_node->object = rut_refable_ref (node->property->object);
      moved_node->property = node->property;
      moved_node->old_time = node->node->t;
      moved_node->new_time = node->node->t + offset;

      rig_path_move_node (prop_data->path, node->node, moved_node->new_time);
    }

  rig_undo_journal_insert (journal, undo_redo, FALSE);
}

void
rig_undo_journal_move_and_log (RigUndoJournal *journal,
                               CoglBool mergable,
                               RigController *controller,
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
                                         controller,
                                         &value,
                                         position);
}

void
rig_undo_journal_delete_path_node_and_log (RigUndoJournal *journal,
                                           RigController *controller,
                                           RutProperty *property,
                                           RigNode *node)
{
  RutBoxed old_value;
  RigPath *path =
    rig_controller_get_path_for_property (controller, property);

  if (rig_node_box (path->type, node, &old_value))
    {
      UndoRedoPathAddRemove *add_remove;
      UndoRedo *undo_redo = g_slice_new (UndoRedo);
      add_remove = &undo_redo->d.path_add_remove;

      undo_redo->op = UNDO_REDO_PATH_REMOVE_OP;
      undo_redo->mergable = FALSE;
      add_remove->controller = rut_refable_ref (controller);
      add_remove->object = rut_refable_ref (property->object);
      add_remove->property = property;
      add_remove->t = node->t;
      add_remove->value = old_value;

      rig_path_remove_node (path, node);

      rig_undo_journal_insert (journal, undo_redo, FALSE);
    }
}

void
rig_undo_journal_set_controlled_and_log (RigUndoJournal *journal,
                                         RigController *controller,
                                         RutProperty *property,
                                         CoglBool value)
{
  UndoRedo *undo_redo;
  UndoRedoSetControlled *set_controlled;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_SET_CONTROLLED_OP;
  undo_redo->mergable = FALSE;

  set_controlled = &undo_redo->d.set_controlled;

  set_controlled->controller = rut_refable_ref (controller);
  set_controlled->object = rut_refable_ref (property->object);
  set_controlled->property = property;
  set_controlled->value = value;

  if (value)
    rig_controller_get_prop_data_for_property (controller, property);
  else
    rig_controller_remove_property (controller, property);

  rig_undo_journal_insert (journal, undo_redo, FALSE);
}

void
rig_undo_journal_set_control_method_and_log (RigUndoJournal *journal,
                                             RigController *controller,
                                             RutProperty *property,
                                             RigControllerMethod method)
{
  UndoRedo *undo_redo;
  UndoRedoSetControlMethod *set_control_method;
  RigControllerPropData *prop_data =
    rig_controller_get_prop_data_for_property (controller, property);

  g_return_if_fail (prop_data != NULL);

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_SET_CONTROL_METHOD_OP;
  undo_redo->mergable = FALSE;

  set_control_method = &undo_redo->d.set_control_method;

  set_control_method->controller = rut_refable_ref (controller);
  set_control_method->object = rut_refable_ref (property->object);
  set_control_method->property = property;
  set_control_method->prev_method = prop_data->method;
  set_control_method->method = method;

  rig_controller_set_property_method (controller, property, method);

  rig_undo_journal_insert (journal, undo_redo, FALSE);
}

void
rig_undo_journal_add_entity_and_log (RigUndoJournal *journal,
                                     RutEntity *parent_entity,
                                     RutEntity *entity)
{
  RigEngine *engine = journal->engine;
  UndoRedo *undo_redo;
  UndoRedoAddDeleteEntity *add_entity;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_ADD_ENTITY_OP;
  undo_redo->mergable = FALSE;

  add_entity = &undo_redo->d.add_delete_entity;

  add_entity->parent_entity = rut_refable_ref (parent_entity);
  add_entity->deleted_entity = rut_refable_ref (entity);

  /* We assume there aren't currently any controller references to
   * this entity. */
  rut_list_init (&add_entity->controller_properties);

  rut_graphable_add_child (parent_entity, entity);
  rut_shell_queue_redraw (engine->ctx->shell);

  rig_undo_journal_insert (journal, undo_redo, FALSE);
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
        prop_data->path ? rut_refable_ref (prop_data->path) : NULL;
      undo_prop_data->property = prop_data->property;

      rut_list_insert (data->properties->prev,
                       &undo_prop_data->link);
    }
}

static void
delete_entity_component_cb (RutComponent *component,
                            void *user_data)
{
  RigUndoJournal *journal = user_data;
  rig_undo_journal_delete_component_and_log (journal, component);
}

void
rig_undo_journal_delete_entity_and_log (RigUndoJournal *journal,
                                        RutEntity *entity)
{
  RigUndoJournal *sub_journal = rig_undo_journal_new (journal->engine);
  UndoRedo *undo_redo;
  UndoRedoAddDeleteEntity *delete_entity;
  RutEntity *parent = rut_graphable_get_parent (entity);
  UndoRedoPropData *prop_data;
  RigEngine *engine = journal->engine;
  CopyControllerPropertiesData copy_properties_data;
  GList *l;

  rut_entity_foreach_component_safe (entity,
                                     delete_entity_component_cb,
                                     sub_journal);

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_DELETE_ENTITY_OP;
  undo_redo->mergable = FALSE;

  delete_entity = &undo_redo->d.add_delete_entity;

  delete_entity->parent_entity = rut_refable_ref (parent);
  delete_entity->deleted_entity = rut_refable_ref (entity);

  rut_list_init (&delete_entity->controller_properties);

  for (l = engine->controllers; l; l = l->next)
    {
      RigController *controller = l->data;
      UndoRedoControllerState *controller_state =
        g_slice_new (UndoRedoControllerState);

      /* Grab a copy of the controller data for all the properties of the
       * entity */
      rut_list_init (&controller_state->properties);

      copy_properties_data.object = entity;
      copy_properties_data.properties = &controller_state->properties;

      rig_controller_foreach_property (controller,
                                       copy_controller_property_cb,
                                       &copy_properties_data);

      if (rut_list_empty (&controller_state->properties))
        {
          g_slice_free (UndoRedoControllerState, controller_state);
          continue;
        }

      controller_state->controller = rut_refable_ref (controller);
      rut_list_insert (&delete_entity->controller_properties,
                       &controller_state->link);

      rut_list_for_each (prop_data, &controller_state->properties, link)
        rig_controller_remove_property (controller, prop_data->property);
    }

  rut_graphable_remove_child (entity);
  rut_shell_queue_redraw (engine->ctx->shell);

  rig_undo_journal_insert (sub_journal, undo_redo, FALSE);
  rig_undo_journal_log_subjournal (journal, sub_journal, FALSE);
}

void
rig_undo_journal_add_component_and_log (RigUndoJournal *journal,
                                        RutEntity *entity,
                                        RutObject *component)
{
  RigEngine *engine = journal->engine;
  UndoRedo *undo_redo;
  UndoRedoAddDeleteComponent *add_component;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_ADD_COMPONENT_OP;
  undo_redo->mergable = FALSE;

  add_component = &undo_redo->d.add_delete_component;

  add_component->parent_entity = rut_refable_ref (entity);
  add_component->deleted_component = rut_refable_ref (component);

  /* We assume there are no controller references to the entity
   * currently */
  rut_list_init (&add_component->controller_properties);

  rut_entity_add_component (entity, component);
  rut_shell_queue_redraw (engine->ctx->shell);

  rig_undo_journal_insert (journal, undo_redo, FALSE);
}

void
rig_undo_journal_delete_component_and_log (RigUndoJournal *journal,
                                           RutObject *component)
{
  UndoRedo *undo_redo;
  UndoRedoAddDeleteComponent *delete_component;
  RutComponentableProps *componentable =
    rut_object_get_properties (component, RUT_INTERFACE_ID_COMPONENTABLE);
  RutEntity *entity = componentable->entity;
  UndoRedoPropData *prop_data;
  RigEngine *engine = journal->engine;
  CopyControllerPropertiesData copy_properties_data;
  GList *l;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_DELETE_COMPONENT_OP;
  undo_redo->mergable = FALSE;

  delete_component = &undo_redo->d.add_delete_component;

  delete_component->parent_entity = rut_refable_ref (entity);
  delete_component->deleted_component = rut_refable_ref (component);

  rut_list_init (&delete_component->controller_properties);

  for (l = engine->controllers; l; l = l->next)
    {
      RigController *controller = l->data;
      UndoRedoControllerState *controller_state =
        g_slice_new (UndoRedoControllerState);

      /* Copy the controller properties for the component */
      rut_list_init (&controller_state->properties);

      copy_properties_data.object = component;
      copy_properties_data.properties = &controller_state->properties;

      rig_controller_foreach_property (controller,
                                       copy_controller_property_cb,
                                       &copy_properties_data);

      if (rut_list_empty (&controller_state->properties))
        {
          g_slice_free (UndoRedoControllerState, controller_state);
          continue;
        }

      controller_state->controller = rut_refable_ref (controller);
      rut_list_insert (&delete_component->controller_properties,
                       &controller_state->link);

      rut_list_for_each (prop_data, &controller_state->properties, link)
        rig_controller_remove_property (controller, prop_data->property);
    }

  rut_entity_remove_component (entity, component);
  rut_shell_queue_redraw (engine->ctx->shell);

  rig_undo_journal_insert (journal, undo_redo, FALSE);
}

void
rig_undo_journal_log_subjournal (RigUndoJournal *journal,
                                 RigUndoJournal *subjournal,
                                 bool apply)
{
  UndoRedo *undo_redo;

  undo_redo = g_slice_new (UndoRedo);
  undo_redo->op = UNDO_REDO_SUBJOURNAL_OP;
  undo_redo->mergable = FALSE;

  undo_redo->d.subjournal = subjournal;

  rig_undo_journal_insert (journal, undo_redo, FALSE);
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
undo_redo_const_prop_change_apply (RigUndoJournal *journal, UndoRedo *undo_redo)
{
  UndoRedoConstPropertyChange *prop_change = &undo_redo->d.const_prop_change;
  RigEngine *engine = journal->engine;
  RigController *controller = prop_change->controller;
  RigControllerPropData *prop_data;

  g_print ("Property change APPLY\n");

  prop_data = rig_controller_get_prop_data_for_property (controller,
                                                         prop_change->property);
  rut_boxed_destroy (&prop_data->constant_value);
  rut_boxed_copy (&prop_data->constant_value, &prop_change->value1);
  rig_controller_update_property (controller,
                                  prop_change->property);

  rig_reload_inspector_property (engine, prop_change->property);
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

  inverse->controller = rut_refable_ref (src->controller);
  inverse->object = rut_refable_ref (src->object);
  inverse->property = src->property;
  inverse->value0 = src->value1;
  inverse->value1 = src->value0;

  return undo_redo_inverse;
}

static void
undo_redo_const_prop_change_free (UndoRedo *undo_redo)
{
  UndoRedoConstPropertyChange *prop_change = &undo_redo->d.const_prop_change;
  rut_refable_unref (prop_change->object);
  rut_refable_unref (prop_change->controller);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_path_add_apply (RigUndoJournal *journal,
                          UndoRedo *undo_redo)
{
  UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;
  RigEngine *engine = journal->engine;
  RigPath *path;

  g_print ("Path add APPLY\n");

  path = rig_controller_get_path_for_property (add_remove->controller,
                                               add_remove->property);
  rig_path_insert_boxed (path, add_remove->t, &add_remove->value);

  rig_controller_update_property (add_remove->controller,
                                  add_remove->property);

  rig_reload_inspector_property (engine, add_remove->property);
}

static UndoRedo *
undo_redo_path_add_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  inverse->op = UNDO_REDO_PATH_REMOVE_OP;
  rut_boxed_copy (&inverse->d.path_add_remove.value,
                  &undo_redo_src->d.path_add_remove.value);
  rut_refable_ref (inverse->d.path_add_remove.object);
  rut_refable_ref (inverse->d.path_add_remove.controller);

  return inverse;
}

static void
undo_redo_path_remove_apply (RigUndoJournal *journal,
                             UndoRedo *undo_redo)
{
  UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;
  RigEngine *engine = journal->engine;
  RigPath *path;

  g_print ("Path remove APPLY\n");

  path = rig_controller_get_path_for_property (add_remove->controller,
                                               add_remove->property);
  rig_path_remove (path, add_remove->t);

  rig_controller_update_property (add_remove->controller,
                                  add_remove->property);

  rig_reload_inspector_property (engine, add_remove->property);
}

static UndoRedo *
undo_redo_path_remove_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);

  inverse->op = UNDO_REDO_PATH_ADD_OP;
  rut_boxed_copy (&inverse->d.path_add_remove.value,
                  &undo_redo_src->d.path_add_remove.value);
  rut_refable_ref (inverse->d.path_add_remove.object);
  rut_refable_ref (inverse->d.path_add_remove.controller);

  return inverse;
}

static void
undo_redo_path_add_remove_free (UndoRedo *undo_redo)
{
  UndoRedoPathAddRemove *add_remove = &undo_redo->d.path_add_remove;
  rut_boxed_destroy (&add_remove->value);
  rut_refable_unref (add_remove->object);
  rut_refable_unref (add_remove->controller);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_path_modify_apply (RigUndoJournal *journal,
                             UndoRedo *undo_redo)
{
  UndoRedoPathModify *modify = &undo_redo->d.path_modify;
  RigEngine *engine = journal->engine;
  RigPath *path;

  g_print ("Path modify APPLY\n");

  path = rig_controller_get_path_for_property (modify->controller,
                                               modify->property);
  rig_path_insert_boxed (path, modify->t, &modify->value1);

  rig_controller_update_property (modify->controller, modify->property);
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
  rut_refable_ref (inverse->d.path_modify.object);
  rut_refable_ref (inverse->d.path_modify.controller);

  return inverse;
}

static void
undo_redo_path_modify_free (UndoRedo *undo_redo)
{
  UndoRedoPathModify *modify = &undo_redo->d.path_modify;
  rut_boxed_destroy (&modify->value0);
  rut_boxed_destroy (&modify->value1);
  rut_refable_unref (modify->object);
  rut_refable_unref (modify->controller);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_set_controlled_apply (RigUndoJournal *journal,
                                UndoRedo *undo_redo)
{
  UndoRedoSetControlled *set_controlled = &undo_redo->d.set_controlled;
  RigEngine *engine = journal->engine;

  g_print ("Set controlled APPLY\n");

  if (set_controlled->value)
    rig_controller_get_prop_data_for_property (set_controlled->controller,
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

  rut_refable_ref (inverse->d.set_controlled.object);
  rut_refable_ref (inverse->d.set_controlled.controller);

  return inverse;
}

static void
undo_redo_set_controlled_free (UndoRedo *undo_redo)
{
  UndoRedoSetControlled *set_controlled = &undo_redo->d.set_controlled;
  rut_refable_unref (set_controlled->object);
  rut_refable_unref (set_controlled->controller);
  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_set_control_method_apply (RigUndoJournal *journal,
                                    UndoRedo *undo_redo)
{
  UndoRedoSetControlMethod *set_control_method =
    &undo_redo->d.set_control_method;
  RigEngine *engine = journal->engine;

  g_print ("Set control_method APPLY\n");

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

  rut_refable_ref (inverse->d.set_control_method.object);
  rut_refable_ref (inverse->d.set_control_method.controller);

  return inverse;
}

static void
undo_redo_set_control_method_free (UndoRedo *undo_redo)
{
  UndoRedoSetControlMethod *set_control_method = &undo_redo->d.set_control_method;
  rut_refable_unref (set_control_method->object);
  rut_refable_unref (set_control_method->controller);
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
        prop_data->path ? rut_refable_ref (prop_data->path) : NULL;
      rut_boxed_copy (&prop_data_copy->constant_value,
                      &prop_data->constant_value);

      rut_list_insert (dst->prev, &prop_data_copy->link);
    }
}

static void
copy_controller_references (RutList *src_controller_properties,
                            RutList *dst_controller_properties)
{
  UndoRedoControllerState *src_controller_state;

  rut_list_for_each (src_controller_state, src_controller_properties, link)
    {
      UndoRedoControllerState *dst_controller_state =
        g_slice_new (UndoRedoControllerState);

      dst_controller_state->controller =
        rut_refable_ref (src_controller_state->controller);

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

  rut_refable_ref (add_delete_entity->parent_entity);
  rut_refable_ref (add_delete_entity->deleted_entity);

  /* We assume there are no controller references to the entity
   * currently */
  rut_list_init (&add_delete_entity->controller_properties);

  copy_controller_references (&undo_redo->d.add_delete_entity.controller_properties,
                              &add_delete_entity->controller_properties);

  return copy;
}

static void
undo_redo_delete_entity_apply (RigUndoJournal *journal,
                               UndoRedo *undo_redo)
{
  UndoRedoAddDeleteEntity *delete_entity = &undo_redo->d.add_delete_entity;
  UndoRedoControllerState *controller_state;

  g_print ("Delete entity APPLY\n");

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
      RigControllerPropData *prop_data =
        rig_controller_get_prop_data_for_property (controller,
                                                   undo_prop_data->property);

      if (prop_data->path)
        rut_refable_unref (prop_data->path);
      if (undo_prop_data->path)
        prop_data->path = rig_path_copy (undo_prop_data->path);
      else
        prop_data->path = NULL;

      rut_boxed_destroy (&prop_data->constant_value);
      rut_boxed_copy (&prop_data->constant_value,
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

  g_print ("Add entity APPLY\n");

  rut_graphable_add_child (add_entity->parent_entity,
                           add_entity->deleted_entity);
  rut_list_for_each (controller_state,
                     &add_entity->controller_properties, link)
    {
      add_controller_properties (controller_state->controller,
                                 &controller_state->properties);
    }
}

static UndoRedo *
undo_redo_add_entity_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = copy_add_delete_entity (undo_redo_src);

  inverse->op = UNDO_REDO_DELETE_ENTITY_OP;

  return inverse;
}

static void
undo_redo_add_delete_entity_free (UndoRedo *undo_redo)
{
  UndoRedoAddDeleteEntity *add_delete_entity = &undo_redo->d.add_delete_entity;
  UndoRedoControllerState *controller_state, *tmp;

  rut_refable_unref (add_delete_entity->parent_entity);
  rut_refable_unref (add_delete_entity->deleted_entity);

  rut_list_for_each_safe (controller_state, tmp,
                          &add_delete_entity->controller_properties, link)
    {
      UndoRedoPropData *prop_data, *tmp1;

      rut_list_for_each_safe (prop_data, tmp1,
                              &controller_state->properties, link)
        {
          if (prop_data->path)
            rut_refable_unref (prop_data->path);

          rut_boxed_destroy (&prop_data->constant_value);
          g_slice_free (UndoRedoPropData, prop_data);
        }

      g_slice_free (UndoRedoControllerState, controller_state);
    }

  g_slice_free (UndoRedo, undo_redo);
}

static UndoRedo *
copy_add_delete_component (UndoRedo *undo_redo)
{
  UndoRedo *copy = g_slice_dup (UndoRedo, undo_redo);
  UndoRedoAddDeleteComponent *add_delete_component = &copy->d.add_delete_component;

  rut_refable_ref (add_delete_component->parent_entity);
  rut_refable_ref (add_delete_component->deleted_component);

  rut_list_init (&add_delete_component->controller_properties);

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

  g_print ("Delete component APPLY\n");

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

  g_print ("Add component APPLY\n");

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
  UndoRedoControllerState *controller_state, *tmp;

  rut_refable_unref (add_delete_component->parent_entity);
  rut_refable_unref (add_delete_component->deleted_component);

  rut_list_for_each_safe (controller_state, tmp,
                          &add_delete_component->controller_properties, link)
    {
      UndoRedoPropData *prop_data, *tmp1;
      rut_list_for_each_safe (prop_data, tmp1,
                              &controller_state->properties, link)
        {
          if (prop_data->path)
            rut_refable_unref (prop_data->path);

          rut_boxed_destroy (&prop_data->constant_value);

          g_slice_free (UndoRedoPropData, prop_data);
        }

      g_slice_free (UndoRedoControllerState, controller_state);
    }

  g_slice_free (UndoRedo, undo_redo);
}

static void
undo_redo_move_path_nodes_apply (RigUndoJournal *journal,
                                 UndoRedo *undo_redo)
{
  UndoRedoMovePathNodes *move_path_nodes = &undo_redo->d.move_path_nodes;
  RigEngine *engine = journal->engine;
  int i;

  g_print ("Move path nodes APPLY\n");

  for (i = 0; i < move_path_nodes->n_nodes; i++)
    {
      UndoRedoMovedPathNode *node = move_path_nodes->nodes + i;
      RigPath *path;
      RigNode *path_node;

      path = rig_controller_get_path_for_property (move_path_nodes->controller,
                                                   node->property);

      path_node = rig_path_find_node (path, node->old_time);
      if (path_node)
        rig_path_move_node (path, path_node, node->new_time);

      rig_controller_update_property (move_path_nodes->controller,
                                      node->property);

      rig_reload_inspector_property (engine, node->property);
    }
}

static UndoRedo *
undo_redo_move_path_nodes_invert (UndoRedo *undo_redo_src)
{
  UndoRedo *inverse = g_slice_dup (UndoRedo, undo_redo_src);
  UndoRedoMovePathNodes *move_path_nodes = &inverse->d.move_path_nodes;
  int i;

  move_path_nodes->controller =
    rut_refable_ref (undo_redo_src->d.move_path_nodes.controller);

  move_path_nodes->nodes =
    g_memdup (undo_redo_src->d.move_path_nodes.nodes,
              sizeof (UndoRedoMovedPathNode) * move_path_nodes->n_nodes);

  for (i = 0; i < move_path_nodes->n_nodes; i++)
    {
      UndoRedoMovedPathNode *node = move_path_nodes->nodes + i;
      float tmp;

      rut_refable_ref (node->object);
      tmp = node->old_time;
      node->old_time = node->new_time;
      node->new_time = tmp;
    }

  return inverse;
}

static void
undo_redo_move_path_nodes_free (UndoRedo *undo_redo)
{
  UndoRedoMovePathNodes *move_path_nodes = &undo_redo->d.move_path_nodes;
  int i;

  for (i = 0; i < move_path_nodes->n_nodes; i++)
    {
      UndoRedoMovedPathNode *node = move_path_nodes->nodes + i;
      rut_refable_ref (node->object);
    }

  g_free (move_path_nodes->nodes);

  rut_refable_unref (move_path_nodes->controller);

  g_slice_free (UndoRedo, undo_redo);
}

static UndoRedoOpImpl undo_redo_ops[] =
  {
    {
      undo_redo_subjournal_apply,
      undo_redo_subjournal_invert,
      undo_redo_subjournal_free
    },
    {
      undo_redo_set_controlled_apply,
      undo_redo_set_controlled_invert,
      undo_redo_set_controlled_free
    },
    {
      undo_redo_set_control_method_apply,
      undo_redo_set_control_method_invert,
      undo_redo_set_control_method_free
    },
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
      undo_redo_add_entity_apply,
      undo_redo_add_entity_invert,
      undo_redo_add_delete_entity_free
    },
    {
      undo_redo_delete_entity_apply,
      undo_redo_delete_entity_invert,
      undo_redo_add_delete_entity_free
    },
    {
      undo_redo_add_component_apply,
      undo_redo_add_component_invert,
      undo_redo_add_delete_component_free
    },
    {
      undo_redo_delete_component_apply,
      undo_redo_delete_component_invert,
      undo_redo_add_delete_component_free
    },
    {
      undo_redo_move_path_nodes_apply,
      undo_redo_move_path_nodes_invert,
      undo_redo_move_path_nodes_free
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

static bool
rig_undo_journal_insert (RigUndoJournal *journal,
                         UndoRedo *undo_redo,
                         bool apply)
{
  UndoRedo *inverse;

  g_return_val_if_fail (undo_redo != NULL, FALSE);
  g_return_val_if_fail (journal->inserting == FALSE, FALSE);

  rig_engine_sync_slaves (journal->engine);

  rig_undo_journal_flush_redos (journal);

  journal->inserting = TRUE;

  /* Purely for testing purposes we now redundantly apply
   * the inverse of the operation followed by the operation
   * itself which should leave us where we started and
   * if not we should hopefully notice quickly!
   */
  inverse = undo_redo_invert (undo_redo);
  undo_redo_apply (journal, inverse);
  undo_redo_apply (journal, undo_redo);
  if (apply)
    undo_redo_apply (journal, undo_redo);
  undo_redo_free (inverse);

  rut_list_insert (journal->undo_ops.prev, &undo_redo->list_node);

  dump_journal (journal, 0);

  journal->inserting = FALSE;

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

  g_print ("REDO\n");

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
