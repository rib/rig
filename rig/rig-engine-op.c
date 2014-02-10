/*
 * Rig
 *
 * Copyright (C) 2012,2013  Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <rut.h>

#include "rig-engine.h"
#include "rig-pb.h"
#include "rig-slave-master.h"
#include "rig-engine-op.h"

#include "rig.pb-c.h"

typedef struct _RigEngineOpApplyContext
{
  RigEngine *engine;
  RigPBUnSerializer *unserializer;
  RigEngineIdToObjectCallback id_to_object_cb;
  RigEngineRegisterIdCallback register_id_cb;
  RigEngineDeleteIdCallback queue_delete_id_cb;
  void *user_data;
} RigEngineOpApplyContext;

typedef struct _RigEngineOpMapContext
{
  RigPBSerializer *serializer;
  RigEngineMapIDCallback map_id_cb;
  void *user_data;
} RigEngineOpMapContext;

#if 0
void
rig_engine_op_register_object (RigEngine *engine,
                               RutObject *object)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);
  RutProperty *label_property;
  const char *label;

  pb_op->type = RIG_ENGINE_OP_TYPE_REGISTER_OBJECT;

  pb_op->register_object =
    rig_pb_new (serializer, Rig__Operation__RegisterObject,
                rig__operation__register_object__init);
  label_property = rut_introspectable_lookup_property (object, "label");
  label = rut_property_get_text (label_property);

  pb_op->register_object->label = (char *)rig_pb_strdup (serializer, label);
  pb_op->register_object->object_id = (intptr_t)object;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}
#endif

void
rig_engine_op_set_property (RigEngine *engine,
                            RutProperty *property,
                            RutBoxed *value)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_SET_PROPERTY;

  pb_op->set_property =
    rig_pb_new (serializer, Rig__Operation__SetProperty,
                rig__operation__set_property__init);

  pb_op->set_property->object_id = (intptr_t)property->object;
  pb_op->set_property->property_id = property->id;
  pb_op->set_property->value = pb_property_value_new (serializer, value);

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_set_property (RigEngineOpApplyContext *ctx,
                        Rig__Operation *pb_op)
{
  Rig__Operation__SetProperty *set_property = pb_op->set_property;
  RutObject *object = ctx->id_to_object_cb (set_property->object_id,
                                            ctx->user_data);
  RutProperty *property =
    rut_introspectable_get_property (object,
                                     set_property->property_id);
  RutBoxed boxed;

  /* XXX: ideally we shouldn't need to init a RutBoxed and set
   * that on a property, and instead we could just directly
   * apply the value to the property we have. */
  rig_pb_init_boxed_value (ctx->unserializer,
                           &boxed,
                           property->spec->type,
                           set_property->value);

  /* Note: at this point the logging of property changes
   * should be disabled in the simulator, so this shouldn't
   * redundantly feed-back to the frontend process. */
  rut_property_set_boxed (&ctx->engine->ctx->property_ctx,
                          property, &boxed);

  return true;
}

static void
_map_op_set_property (RigEngineOpMapContext *ctx,
                      Rig__Operation *src_pb_op,
                      Rig__Operation *pb_op)

{
  pb_op->set_property =
    rig_pb_dup (ctx->serializer,
                Rig__Operation__SetProperty,
                rig__operation__set_property__init,
                src_pb_op->set_property);

  pb_op->set_property->object_id =
    ctx->map_id_cb (src_pb_op->set_property->object_id, ctx->user_data);
  pb_op->set_property->value = src_pb_op->set_property->value;
}

void
rig_engine_op_add_entity (RigEngine *engine,
                          RutEntity *parent,
                          RutEntity *entity)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op;

  g_return_if_fail (rut_graphable_get_parent (entity) == NULL);

  pb_op = rig_pb_new (serializer, Rig__Operation, rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_ADD_ENTITY;

  pb_op->add_entity = rig_pb_new (serializer, Rig__Operation__AddEntity,
                                  rig__operation__add_entity__init);

  pb_op->add_entity->parent_entity_id = (intptr_t)parent;
  pb_op->add_entity->entity = rig_pb_serialize_entity (serializer,
                                                       NULL,
                                                       entity);

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_add_entity (RigEngineOpApplyContext *ctx,
                      Rig__Operation *pb_op)
{
  Rig__Operation__AddEntity *add_entity = pb_op->add_entity;
  RutEntity *parent = NULL;
  RutEntity *entity;

  g_warn_if_fail (add_entity->entity->has_parent_id == false);

  if (add_entity->parent_entity_id)
    {
      parent = ctx->id_to_object_cb (add_entity->parent_entity_id,
                                     ctx->user_data);
      if (!parent)
        return false;
    }

  entity = rig_pb_unserialize_entity (ctx->unserializer,
                                      add_entity->entity);

  if (!entity)
    return false;

  ctx->register_id_cb (entity, add_entity->entity->id, ctx->user_data);

  if (parent)
    rut_graphable_add_child (parent, entity);

  return true;
}

static void
_map_op_add_entity (RigEngineOpMapContext *ctx,
                    Rig__Operation *src_pb_op,
                    Rig__Operation *pb_op)
{
  pb_op->add_entity = rig_pb_dup (ctx->serializer,
                                  Rig__Operation__AddEntity,
                                  rig__operation__add_entity__init,
                                  src_pb_op->add_entity);

  pb_op->add_entity->parent_entity_id =
    ctx->map_id_cb (src_pb_op->add_entity->parent_entity_id, ctx->user_data);

  /* XXX: we assume that the new entity isn't currently
   * associated with any components and so the serialized
   * entity doesn't have any object ids that need mapping.
   *
   * The id of the entity itself will correspond to an
   * edit-mode object pointer, which can be used later to
   * create a mapping from the new edit-mode entity and the
   * new play-mode entity
   */

  pb_op->add_entity->entity = src_pb_op->add_entity->entity;
}

void
rig_engine_op_delete_entity (RigEngine *engine,
                             RutEntity *entity)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_DELETE_ENTITY;

  pb_op->delete_entity = rig_pb_new (serializer, Rig__Operation__DeleteEntity,
                                     rig__operation__delete_entity__init);

  pb_op->delete_entity->entity_id = (intptr_t)entity;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_delete_entity (RigEngineOpApplyContext *ctx,
                         Rig__Operation *pb_op)
{
  RutEntity *entity =
    ctx->id_to_object_cb (pb_op->delete_entity->entity_id, ctx->user_data);

  if (!entity)
    return false;

  /* We want deletion to happen lazily so we take a reference before
   * removing it from the graph. */
  rut_object_ref (entity);

  rut_graphable_remove_child (entity);

  ctx->queue_delete_id_cb (pb_op->add_entity->parent_entity_id, ctx->user_data);

  return true;
}

static void
_map_op_delete_entity (RigEngineOpMapContext *ctx,
                       Rig__Operation *src_pb_op,
                       Rig__Operation *pb_op)
{
  pb_op->delete_entity = rig_pb_dup (ctx->serializer,
                                     Rig__Operation__DeleteEntity,
                                     rig__operation__delete_entity__init,
                                     src_pb_op->delete_entity);

  pb_op->delete_entity->entity_id =
    ctx->map_id_cb (src_pb_op->delete_entity->entity_id, ctx->user_data);
}

void
rig_engine_op_add_component (RigEngine *engine,
                             RutEntity *entity,
                             RutComponent *component)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_ADD_COMPONENT;

  pb_op->add_component = rig_pb_new (serializer, Rig__Operation__AddComponent,
                                     rig__operation__add_component__init);

  pb_op->add_component->parent_entity_id = (intptr_t)entity;
  pb_op->add_component->component =
    rig_pb_serialize_component (serializer, component);

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_add_component (RigEngineOpApplyContext *ctx,
                         Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_add_component (RigEngineOpMapContext *ctx,
                       Rig__Operation *src_pb_op,
                       Rig__Operation *pb_op)
{

}

void
rig_engine_op_delete_component (RigEngine *engine,
                                RutComponent *component)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_DELETE_COMPONENT;

  pb_op->delete_component = rig_pb_new (serializer, Rig__Operation__DeleteComponent,
                                        rig__operation__delete_component__init);
  pb_op->delete_component->component_id = (intptr_t)component;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_delete_component (RigEngineOpApplyContext *ctx,
                            Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_delete_component (RigEngineOpMapContext *ctx,
                          Rig__Operation *src_pb_op,
                          Rig__Operation *pb_op)
{

}

void
rig_engine_op_add_controller (RigEngine *engine,
                              RigController *controller)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_ADD_CONTROLLER;

  pb_op->add_controller = rig_pb_new (serializer, Rig__Operation__AddController,
                                      rig__operation__add_controller__init);
  pb_op->add_controller->controller =
    rig_pb_serialize_controller (serializer, controller);

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_add_controller (RigEngineOpApplyContext *ctx,
                          Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_add_controller (RigEngineOpMapContext *ctx,
                        Rig__Operation *src_pb_op,
                        Rig__Operation *pb_op)
{

}

void
rig_engine_op_delete_controller (RigEngine *engine,
                                 RigController *controller)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_DELETE_CONTROLLER;

  pb_op->delete_controller =
    rig_pb_new (serializer, Rig__Operation__DeleteController,
                rig__operation__delete_controller__init);
  pb_op->delete_controller->controller_id = (intptr_t)controller;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_delete_controller (RigEngineOpApplyContext *ctx,
                             Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_delete_controller (RigEngineOpMapContext *ctx,
                           Rig__Operation *src_pb_op,
                           Rig__Operation *pb_op)
{

}

void
rig_engine_op_controller_set_const (RigEngine *engine,
                                    RigController *controller,
                                    RutProperty *property,
                                    RutBoxed *value)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_SET_CONST;

  pb_op->controller_set_const =
    rig_pb_new (serializer, Rig__Operation__ControllerSetConst,
                rig__operation__controller_set_const__init);

  pb_op->controller_set_const->controller_id = (intptr_t)controller;
  pb_op->controller_set_const->object_id = (intptr_t)property->object;
  pb_op->controller_set_const->property_id = property->id;
  pb_op->controller_set_const->value =
    pb_property_value_new (serializer, value);

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_controller_set_const (RigEngineOpApplyContext *ctx,
                                Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_controller_set_const (RigEngineOpMapContext *ctx,
                              Rig__Operation *src_pb_op,
                              Rig__Operation *pb_op)
{

}

void
rig_engine_op_controller_path_add_node (RigEngine *engine,
                                        RigController *controller,
                                        RutProperty *property,
                                        float t,
                                        RutBoxed *value)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_ADD_NODE;

  pb_op->controller_path_add_node =
    rig_pb_new (serializer, Rig__Operation__ControllerPathAddNode,
                rig__operation__controller_path_add_node__init);
  pb_op->controller_path_add_node->controller_id = (intptr_t)controller;
  pb_op->controller_path_add_node->object_id = (intptr_t)property->object;
  pb_op->controller_path_add_node->property_id = property->id;
  pb_op->controller_path_add_node->t = t;
  pb_op->controller_path_add_node->value =
    pb_property_value_new (serializer, value);

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_controller_path_add_node (RigEngineOpApplyContext *ctx,
                                    Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_controller_path_add_node (RigEngineOpMapContext *ctx,
                                  Rig__Operation *src_pb_op,
                                  Rig__Operation *pb_op)
{

}

void
rig_engine_op_controller_path_delete_node (RigEngine *engine,
                                           RigController *controller,
                                           RutProperty *property,
                                           float t)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_DELETE_NODE;

  pb_op->controller_path_delete_node =
    rig_pb_new (serializer, Rig__Operation__ControllerPathDeleteNode,
                rig__operation__controller_path_delete_node__init);
  pb_op->controller_path_delete_node->controller_id = (intptr_t)controller;
  pb_op->controller_path_delete_node->object_id = (intptr_t)property->object;
  pb_op->controller_path_delete_node->property_id = property->id;
  pb_op->controller_path_delete_node->t = t;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_controller_path_delete_node (RigEngineOpApplyContext *ctx,
                                       Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_controller_path_delete_node (RigEngineOpMapContext *ctx,
                                     Rig__Operation *src_pb_op,
                                     Rig__Operation *pb_op)
{

}

void
rig_engine_op_controller_path_set_node (RigEngine *engine,
                                        RigController *controller,
                                        RutProperty *property,
                                        float t,
                                        RutBoxed *value)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_SET_NODE;

  pb_op->controller_path_set_node =
    rig_pb_new (serializer, Rig__Operation__ControllerPathSetNode,
                rig__operation__controller_path_set_node__init);
  pb_op->controller_path_set_node->controller_id = (intptr_t)controller;
  pb_op->controller_path_set_node->object_id = (intptr_t)property->object;
  pb_op->controller_path_set_node->property_id = property->id;
  pb_op->controller_path_set_node->t = t;
  pb_op->controller_path_set_node->value =
    pb_property_value_new (serializer, value);

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_controller_path_set_node (RigEngineOpApplyContext *ctx,
                                    Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_controller_path_set_node (RigEngineOpMapContext *ctx,
                                  Rig__Operation *src_pb_op,
                                  Rig__Operation *pb_op)
{

}

void
rig_engine_op_controller_add_property (RigEngine *engine,
                                       RigController *controller,
                                       RutProperty *property)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_ADD_PROPERTY;

  pb_op->controller_add_property =
    rig_pb_new (serializer, Rig__Operation__ControllerAddProperty,
                rig__operation__controller_add_property__init);
  pb_op->controller_add_property->controller_id = (intptr_t)controller;
  pb_op->controller_add_property->object_id = (intptr_t)property->object;
  pb_op->controller_add_property->property_id = property->id;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_controller_add_property (RigEngineOpApplyContext *ctx,
                                   Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_controller_add_property (RigEngineOpMapContext *ctx,
                                 Rig__Operation *src_pb_op,
                                 Rig__Operation *pb_op)
{

}

void
rig_engine_op_controller_remove_property (RigEngine *engine,
                                          RigController *controller,
                                          RutProperty *property)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_REMOVE_PROPERTY;

  pb_op->controller_remove_property =
    rig_pb_new (serializer, Rig__Operation__ControllerRemoveProperty,
                rig__operation__controller_remove_property__init);
  pb_op->controller_remove_property->controller_id = (intptr_t)controller;
  pb_op->controller_remove_property->object_id = (intptr_t)property->object;
  pb_op->controller_remove_property->property_id = property->id;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_controller_remove_property (RigEngineOpApplyContext *ctx,
                                      Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_controller_remove_property (RigEngineOpMapContext *ctx,
                                    Rig__Operation *src_pb_op,
                                    Rig__Operation *pb_op)
{

}

void
rig_engine_op_controller_property_set_method (RigEngine *engine,
                                              RigController *controller,
                                              RutProperty *property,
                                              RigControllerMethod method)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_CONTROLLER_PROPERTY_SET_METHOD;

  pb_op->controller_property_set_method =
    rig_pb_new (serializer, Rig__Operation__ControllerPropertySetMethod,
                rig__operation__controller_property_set_method__init);
  pb_op->controller_property_set_method->controller_id = (intptr_t)controller;
  pb_op->controller_property_set_method->object_id = (intptr_t)property->object;
  pb_op->controller_property_set_method->property_id = property->id;
  pb_op->controller_property_set_method->method = method;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_controller_property_set_method (RigEngineOpApplyContext *ctx,
                                          Rig__Operation *pb_op)
{
  g_warn_if_reached ();
  return true;
}

static void
_map_op_controller_property_set_method (RigEngineOpMapContext *ctx,
                                        Rig__Operation *src_pb_op,
                                        Rig__Operation *pb_op)
{

}

#if 0
void
rig_engine_op_set_play_mode (RigEngine *engine,
                             bool play_mode_enabled)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_SET_PLAY_MODE;

  pb_op->set_play_mode = rig_pb_new (serializer, Rig__Operation__SetPlayMode,
                                     rig__operation__set_play_mode__init);
  pb_op->set_play_mode->play_mode_enabled = play_mode_enabled;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_set_play_mode (RigEngineOpApplyContext *ctx,
                         Rig__Operation *pb_op)
{
  bool play_mode_enabled = pb_op->set_play_mode->play_mode_enabled;
  rig_editor_set_play_mode_enabled (ctx->engine->editor, play_mode_enabled);

  return true;
}

static void
_map_op_set_play_mode (RigEngineOpMapContext *ctx,
                       Rig__Operation *src_pb_op,
                       Rig__Operation *pb_op)
{
  /* NOP */
}
#endif

typedef struct _RigEngineOperation
{
  bool (*apply_op) (RigEngineOpApplyContext *ctx,
                    Rig__Operation *pb_op);

  void (*map_op) (RigEngineOpMapContext *ctx,
                  Rig__Operation *src_pb_op,
                  Rig__Operation *pb_op);

} RigEngineOperation;

static RigEngineOperation _rig_engine_ops[] =
{
  {
    _apply_op_set_property,
    _map_op_set_property,
  },
  {
    _apply_op_add_entity,
    _map_op_add_entity,
  },
  {
    _apply_op_delete_entity,
    _map_op_delete_entity,
  },
  {
    _apply_op_add_component,
    _map_op_add_component,
  },
  {
    _apply_op_delete_component,
    _map_op_delete_component,
  },
  {
    _apply_op_add_controller,
    _map_op_add_controller,
  },
  {
    _apply_op_delete_controller,
    _map_op_delete_controller,
  },
  {
    _apply_op_controller_set_const,
    _map_op_controller_set_const,
  },
  {
    _apply_op_controller_path_add_node,
    _map_op_controller_path_add_node,
  },
  {
    _apply_op_controller_path_delete_node,
    _map_op_controller_path_delete_node,
  },
  {
    _apply_op_controller_path_set_node,
    _map_op_controller_path_set_node,
  },
  {
    _apply_op_controller_add_property,
    _map_op_controller_add_property,
  },
  {
    _apply_op_controller_remove_property,
    _map_op_controller_remove_property,
  },
  {
    _apply_op_controller_property_set_method,
    _map_op_controller_property_set_method,
  },
#if 0
  {
    _apply_op_set_play_mode,
    _map_op_set_play_mode,
  }
#endif
};

/* This function maps Rig__UIEdit operations from one ID space to
 * another and at the same time applies those operations with the
 * mapped ids.  This is used to apply edit-mode ui operations to the
 * play-mode ui and build a corresponding Rig__UIEdit that can also be
 * sent to the simulator process.
 *
 * XXX: Note that *not* everything is deep-copied. For example
 * Rig__PropertyValues that don't reference any object ids
 * will be shared.
 *
 * This function returns NULL if it fails to apply anything.
 *
 * All the operations will be allocated on the engine->frame_stack
 * so there is nothing to explicitly free.
 */
Rig__UIEdit *
rig_engine_map_pb_ui_edit (RigEngine *engine,
                           Rig__UIEdit *pb_ui_edit,
                           RigEngineMapIDCallback map_id_cb,
                           RigEngineRegisterIdCallback register_id_cb,
                           RigEngineIdToObjectCallback id_to_object_cb,
                           RigEngineDeleteIdCallback queue_delete_id_cb,
                           void *user_data)
{
  RigPBSerializer *serializer;
  Rig__UIEdit *mapped_pb_ui_edits;
  Rig__Operation *pb_ops;
  RigEngineOpApplyContext apply_ctx;
  RigEngineOpMapContext map_ctx;
  int i;

  apply_ctx.engine = engine;
  apply_ctx.id_to_object_cb = id_to_object_cb;
  apply_ctx.register_id_cb = register_id_cb;
  apply_ctx.queue_delete_id_cb = queue_delete_id_cb;
  apply_ctx.user_data = user_data;

  serializer = rig_pb_serializer_new (engine);
  rig_pb_serializer_set_use_pointer_ids_enabled (serializer, true);

  mapped_pb_ui_edits = rig_pb_new (serializer, Rig__UIEdit, rig__uiedit__init);
  mapped_pb_ui_edits->n_ops = pb_ui_edit->n_ops;

  if (!pb_ui_edit->n_ops)
    return mapped_pb_ui_edits;

  mapped_pb_ui_edits->ops =
    rut_memory_stack_memalign (serializer->stack,
                               sizeof (void *) * mapped_pb_ui_edits->n_ops,
                               RUT_UTIL_ALIGNOF (void *));

  pb_ops = rut_memory_stack_memalign (serializer->stack,
                                      (sizeof (Rig__Operation) *
                                       mapped_pb_ui_edits->n_ops),
                                      RUT_UTIL_ALIGNOF (Rig__Operation));

  map_ctx.serializer = serializer;
  map_ctx.map_id_cb = map_id_cb;
  map_ctx.user_data = user_data;

  for (i = 0; i < pb_ui_edit->n_ops; i++)
    {
      Rig__Operation *src_pb_op = pb_ui_edit->ops[i];
      Rig__Operation *pb_op = &pb_ops[i];

      rig__operation__init (pb_op);

      mapped_pb_ui_edits->ops[i] = pb_op;

      pb_op->type = src_pb_op->type;

      _rig_engine_ops [pb_op->type].map_op (&map_ctx, src_pb_op, pb_op);
      if (!_rig_engine_ops [pb_op->type].apply_op (&apply_ctx, pb_op))
        {
          /* Note: all of the operations are allocated on the
           * frame-stack so we don't need to explicitly free anything.
           */
          rig_pb_serializer_destroy (serializer);
          return NULL;
        }
    }

  rig_pb_serializer_destroy (serializer);

  return mapped_pb_ui_edits;
}

void
rig_engine_op_apply_context_init (RigEngineOpApplyContext *ctx,
                                  RigPBUnSerializer *unserializer,
                                  RigEngineRegisterIdCallback register_id_cb,
                                  RigEngineIdToObjectCallback id_to_object_cb,
                                  RigEngineDeleteIdCallback queue_delete_id_cb,
                                  void *user_data)
{
  ctx->engine = unserializer->engine;
  ctx->unserializer = &unserializer;
  ctx->id_to_object_cb = id_to_object_cb;
  ctx->register_id_cb = register_id_cb;
  ctx->queue_delete_id_cb = queue_delete_id_cb;
  ctx->user_data = user_data;
}

void
rig_engine_op_apply_context_destroy (RigEngineOpApplyContext *ctx)
{
  /* Currently, nothing to clean up */
}

bool
rig_engine_pb_op_apply (RigEngineOpApplyContext *ctx,
                        Rig__Operation *pb_op)
{
  return _rig_engine_ops[pb_op->type].apply_op (ctx, pb_op);
}

bool
rig_engine_apply_pb_ui_edit (RigEngineOpApplyContext *ctx,
                             Rig__UIEdit *pb_ui_edit)
{
  int i;

  for (i = 0; i < pb_ui_edit->n_ops; i++)
    {
      Rig__Operation *pb_op = pb_ui_edit->ops[i];

      if (!_rig_engine_ops[pb_op->type].apply_op (&ctx, pb_op))
        return false;
    }

  return true;
}
