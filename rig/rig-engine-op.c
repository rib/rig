/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012,2013  Intel Corporation.
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

#include <config.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-pb.h"
#include "rig-slave-master.h"
#include "rig-engine-op.h"

#include "rig.pb-c.h"

static bool
map_ids (RigEngineOpMapContext *ctx,
         int64_t **id_ptrs)
{
  void *user_data = ctx->user_data;
  int i;

  for (i = 0; id_ptrs[i]; i++)
    {
      int64_t *id_ptr = id_ptrs[i];
      *id_ptr = ctx->map_id_cb (*id_ptr, user_data);
      if (!*id_ptr)
        return false;
    }

  return true;
}

static bool
map_id (RigEngineOpMapContext *ctx,
        int64_t *id_ptr)
{
  *id_ptr = ctx->map_id_cb (*id_ptr, ctx->user_data);
  if (!*id_ptr)
    return false;
  return true;
}

static Rig__PropertyValue *
maybe_copy_property_value (RigEngineOpCopyContext *ctx,
                           Rig__PropertyValue *src_value)
{
  if (src_value->has_object_value || src_value->has_asset_value)
    {
      return rig_pb_dup (ctx->serializer,
                         Rig__PropertyValue,
                         rig__property_value__init,
                         src_value);
    }
  else
    return src_value;
}

static bool
maybe_map_property_value (RigEngineOpMapContext *ctx,
                          Rig__PropertyValue *value)
{
  if (value->has_object_value)
    {
      value->object_value = ctx->map_id_cb (value->object_value, ctx->user_data);
      if (!value->object_value)
        return false;
    }
  else if (value->has_asset_value)
    {
      value->asset_value = ctx->map_id_cb (value->asset_value, ctx->user_data);
      if (!value->asset_value)
        return false;
    }

  return true;
}

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
  RutObject *object = (void *)(intptr_t)set_property->object_id;
  RutProperty *property;
  RutBoxed boxed;

  if (!object)
    return false;

  property = rut_introspectable_get_property (object,
                                              set_property->property_id);

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
_copy_op_set_property (RigEngineOpCopyContext *ctx,
                       Rig__Operation *src_pb_op,
                       Rig__Operation *pb_op)

{
  pb_op->set_property = rig_pb_dup (ctx->serializer,
                                    Rig__Operation__SetProperty,
                                    rig__operation__set_property__init,
                                    src_pb_op->set_property);

  pb_op->set_property->value =
    maybe_copy_property_value (ctx, src_pb_op->set_property->value);
}

static bool
_map_op_set_property (RigEngineOpMapContext *ctx,
                      Rig__Operation *pb_op)
{
  Rig__Operation__SetProperty *set_property = pb_op->set_property;
  Rig__PropertyValue *value = set_property->value;

  if (!map_id (ctx, &set_property->object_id))
    return false;

  if (!maybe_map_property_value (ctx, value))
    return false;

  return true;
}

void
rig_engine_op_add_entity (RigEngine *engine,
                          RigEntity *parent,
                          RigEntity *entity)
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
  RigEntity *parent = NULL;
  RigEntity *entity;

  g_warn_if_fail (add_entity->entity->has_parent_id == false);

  if (add_entity->parent_entity_id)
    {
      parent = (void *)(intptr_t)add_entity->parent_entity_id;
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
_copy_op_add_entity (RigEngineOpCopyContext *ctx,
                     Rig__Operation *src_pb_op,
                     Rig__Operation *pb_op)
{
  pb_op->add_entity = rig_pb_dup (ctx->serializer,
                                  Rig__Operation__AddEntity,
                                  rig__operation__add_entity__init,
                                  src_pb_op->add_entity);
#if 0
  pb_op->add_entity->entity = rig_pb_dup (ctx->serializer,
                                          Rig__Entity,
                                          rig__entity__init,
                                          src_pb_op->add_entity->entity);
#endif

  /* FIXME: This is currently only a shallow copy.
   *
   * We should either clarify that the _copy_ functions only perform a
   * shallow copy suitable for creating a mapping, or we should also
   * be copying the entity's components too
   */
}

static bool
_map_op_add_entity (RigEngineOpMapContext *ctx,
                    Rig__Operation *pb_op)
{
  if (!map_id (ctx, &pb_op->add_entity->parent_entity_id))
    return false;

  /* XXX: we assume that the new entity isn't currently
   * associated with any components and so the serialized
   * entity doesn't have any object ids that need mapping.
   *
   * The id of the entity itself will correspond to an
   * edit-mode object pointer, which can be used later to
   * create a mapping from the new edit-mode entity and the
   * new play-mode entity
   */

  return true;
}

void
rig_engine_op_delete_entity (RigEngine *engine,
                             RigEntity *entity)
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
  RigEntity *entity = (void *)(intptr_t)pb_op->delete_entity->entity_id;

  rig_entity_reap (entity, ctx->engine);

  rut_graphable_remove_child (entity);

  ctx->unregister_id_cb (pb_op->delete_entity->entity_id, ctx->user_data);

  return true;
}

static void
_copy_op_delete_entity (RigEngineOpCopyContext *ctx,
                        Rig__Operation *src_pb_op,
                        Rig__Operation *pb_op)
{
  pb_op->delete_entity = rig_pb_dup (ctx->serializer,
                                     Rig__Operation__DeleteEntity,
                                     rig__operation__delete_entity__init,
                                     src_pb_op->delete_entity);
}

static bool
_map_op_delete_entity (RigEngineOpMapContext *ctx,
                       Rig__Operation *pb_op)
{
  if (!map_id (ctx, &pb_op->delete_entity->entity_id))
    return false;

  return true;
}

void
rig_engine_op_add_component (RigEngine *engine,
                             RigEntity *entity,
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
  RigEntity *entity = (void *)(intptr_t)pb_op->add_component->parent_entity_id;
  RutComponent *component;

  if (!entity)
    return false;

  /* XXX: Note: this will also add the component to the entity
   * since some components can't be configured before being
   * added to an entity.
   */
  component = rig_pb_unserialize_component (ctx->unserializer,
                                            entity,
                                            pb_op->add_component->component);

  if (!component)
    return false;

  return true;
}

static void
_copy_op_add_component (RigEngineOpCopyContext *ctx,
                        Rig__Operation *src_pb_op,
                        Rig__Operation *pb_op)
{
  pb_op->add_component = rig_pb_dup (ctx->serializer,
                                     Rig__Operation__AddComponent,
                                     rig__operation__add_component__init,
                                     src_pb_op->add_component);

  pb_op->add_component->component = rig_pb_dup (ctx->serializer,
                                     Rig__Entity__Component,
                                     rig__entity__component__init,
                                     src_pb_op->add_component->component);
}

static bool
_map_op_add_component (RigEngineOpMapContext *ctx,
                       Rig__Operation *pb_op)
{
  if (!map_id (ctx, &pb_op->add_component->parent_entity_id))
    return false;

  return true;
}

void
rig_engine_op_delete_component (RigEngine *engine,
                                RutComponent *component)
{
  RigPBSerializer *serializer = engine->ops_serializer;
  Rig__Operation *pb_op = rig_pb_new (serializer, Rig__Operation,
                                      rig__operation__init);

  pb_op->type = RIG_ENGINE_OP_TYPE_DELETE_COMPONENT;

  pb_op->delete_component =
    rig_pb_new (serializer, Rig__Operation__DeleteComponent,
                rig__operation__delete_component__init);
  pb_op->delete_component->component_id = (intptr_t)component;

  engine->apply_op_callback (pb_op, engine->apply_op_data);
}

static bool
_apply_op_delete_component (RigEngineOpApplyContext *ctx,
                            Rig__Operation *pb_op)
{
  RutComponent *component =
    (void *)(intptr_t)pb_op->delete_component->component_id;
  RutComponentableProps *props;
  RigEntity *entity;

  if (!component)
    return false;

  props = rut_object_get_properties (component, RUT_TRAIT_ID_COMPONENTABLE);
  entity = props->entity;
  if (!entity)
    return false;

  rig_component_reap (component, ctx->engine);

  rig_entity_remove_component (entity, component);

  ctx->unregister_id_cb (pb_op->delete_component->component_id, ctx->user_data);

  return true;
}

static void
_copy_op_delete_component (RigEngineOpCopyContext *ctx,
                           Rig__Operation *src_pb_op,
                           Rig__Operation *pb_op)
{

  pb_op->delete_component = rig_pb_dup (ctx->serializer,
                                        Rig__Operation__DeleteComponent,
                                        rig__operation__delete_component__init,
                                        src_pb_op->delete_component);
}

static bool
_map_op_delete_component (RigEngineOpMapContext *ctx,
                          Rig__Operation *pb_op)
{
  if (!map_id (ctx, &pb_op->delete_component->component_id))
    return false;

  return true;
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
  Rig__Controller *pb_controller = pb_op->add_controller->controller;
  RigController *controller =
    rig_pb_unserialize_controller_bare (ctx->unserializer,
                                        pb_controller);

  rig_pb_unserialize_controller_properties (ctx->unserializer,
                                            controller,
                                            pb_controller->n_properties,
                                            pb_controller->properties);

  rig_ui_add_controller (ctx->ui, controller);
  rut_object_unref (controller);

  return true;
}

static void
_copy_op_add_controller (RigEngineOpCopyContext *ctx,
                         Rig__Operation *src_pb_op,
                         Rig__Operation *pb_op)
{
  /* XXX: Nothing needs to be mapped for this operation */
  pb_op->add_controller->controller = src_pb_op->add_controller->controller;
  return;
}


static bool
_map_op_add_controller (RigEngineOpMapContext *ctx,
                        Rig__Operation *pb_op)
{
  /* XXX: Nothing needs to be mapped for this operation */
  return true;
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
  RigController *controller =
    (void *)(intptr_t)pb_op->delete_controller->controller_id;

  if (!controller)
    return false;

  rig_controller_reap (controller, ctx->engine);

  rig_ui_remove_controller (ctx->ui, controller);

  ctx->unregister_id_cb (pb_op->delete_controller->controller_id,
                         ctx->user_data);

  return true;
}

static void
_copy_op_delete_controller (RigEngineOpCopyContext *ctx,
                            Rig__Operation *src_pb_op,
                            Rig__Operation *pb_op)
{
  pb_op->delete_controller =
    rig_pb_dup (ctx->serializer,
                Rig__Operation__DeleteController,
                rig__operation__delete_controller__init,
                src_pb_op->delete_controller);
}

static bool
_map_op_delete_controller (RigEngineOpMapContext *ctx,
                           Rig__Operation *pb_op)
{
  if (!map_id (ctx, &pb_op->delete_controller->controller_id))
    return false;

  return true;
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
  Rig__Operation__ControllerSetConst *set_const = pb_op->controller_set_const;
  RigController *controller = (void *)(intptr_t)set_const->controller_id;
  RutObject *object = (void *)(intptr_t)set_const->object_id;
  RutProperty *property;
  RutBoxed boxed;

  if (!controller || !object)
    return false;

  property = rut_introspectable_get_property (object, set_const->property_id);

  rig_pb_init_boxed_value (ctx->unserializer,
                           &boxed,
                           property->spec->type,
                           set_const->value);

  rig_controller_set_property_constant (controller,
                                        property,
                                        &boxed);
  return true;
}

static void
_copy_op_controller_set_const (RigEngineOpCopyContext *ctx,
                               Rig__Operation *src_pb_op,
                               Rig__Operation *pb_op)
{
  pb_op->controller_set_const =
    rig_pb_dup (ctx->serializer,
                Rig__Operation__ControllerSetConst,
                rig__operation__controller_set_const__init,
                src_pb_op->controller_set_const);

  pb_op->controller_set_const->value =
    maybe_copy_property_value (ctx, src_pb_op->controller_set_const->value);
}

static bool
_map_op_controller_set_const (RigEngineOpMapContext *ctx,
                              Rig__Operation *pb_op)
{
  int64_t *id_ptrs[] = {
      &pb_op->controller_set_const->object_id,
      &pb_op->controller_set_const->controller_id,
      NULL
  };

  if (!map_ids (ctx, id_ptrs))
    return false;

  if (!maybe_map_property_value (ctx, pb_op->controller_set_const->value))
    return false;

  return true;
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
  Rig__Operation__ControllerPathAddNode *add_node =
    pb_op->controller_path_add_node;
  RigController *controller = (void *)(intptr_t)add_node->controller_id;
  RutObject *object = (void *)(intptr_t)add_node->object_id;
  RutProperty *property;
  RutBoxed boxed;

  if (!controller || !object)
    return false;

  property = rut_introspectable_get_property (object, add_node->property_id);

  rig_pb_init_boxed_value (ctx->unserializer,
                           &boxed,
                           property->spec->type,
                           add_node->value);

  rig_controller_insert_path_value (controller,
                                    property,
                                    add_node->t,
                                    &boxed);

  return true;
}

static void
_copy_op_controller_path_add_node (RigEngineOpCopyContext *ctx,
                                   Rig__Operation *src_pb_op,
                                   Rig__Operation *pb_op)
{
  pb_op->controller_path_add_node =
    rig_pb_dup (ctx->serializer,
                Rig__Operation__ControllerPathAddNode,
                rig__operation__controller_path_add_node__init,
                src_pb_op->controller_path_add_node);

  pb_op->controller_path_add_node->value =
    maybe_copy_property_value (ctx, src_pb_op->controller_path_add_node->value);
}

static bool
_map_op_controller_path_add_node (RigEngineOpMapContext *ctx,
                                  Rig__Operation *pb_op)
{
  int64_t *id_ptrs[] = {
      &pb_op->controller_path_add_node->object_id,
      &pb_op->controller_path_add_node->controller_id,
      NULL
  };

  if (!map_ids (ctx, id_ptrs))
    return false;

  if (!maybe_map_property_value (ctx, pb_op->controller_path_add_node->value))
    return false;

  return true;
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
  Rig__Operation__ControllerPathDeleteNode *delete_node =
    pb_op->controller_path_delete_node;
  RigController *controller = (void *)(intptr_t)delete_node->controller_id;
  RutObject *object = (void *)(intptr_t)delete_node->object_id;
  RutProperty *property;

  if (!controller || !object)
    return false;

  property = rut_introspectable_get_property (object, delete_node->property_id);

  rig_controller_remove_path_value (controller,
                                    property,
                                    delete_node->t);
  return true;
}

static void
_copy_op_controller_path_delete_node (RigEngineOpCopyContext *ctx,
                                      Rig__Operation *src_pb_op,
                                      Rig__Operation *pb_op)
{
  pb_op->controller_path_delete_node =
    rig_pb_dup (ctx->serializer,
                Rig__Operation__ControllerPathDeleteNode,
                rig__operation__controller_path_delete_node__init,
                src_pb_op->controller_path_delete_node);
}

static bool
_map_op_controller_path_delete_node (RigEngineOpMapContext *ctx,
                                     Rig__Operation *pb_op)
{
  int64_t *id_ptrs[] = {
      &pb_op->controller_path_delete_node->object_id,
      &pb_op->controller_path_delete_node->controller_id,
      NULL
  };

  if (!map_ids (ctx, id_ptrs))
    return false;

  return true;
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

/* XXX: This is equivalent to _add_path_node so should be redundant! */
static bool
_apply_op_controller_path_set_node (RigEngineOpApplyContext *ctx,
                                    Rig__Operation *pb_op)
{
  Rig__Operation__ControllerPathSetNode *set_node =
    pb_op->controller_path_set_node;
  RigController *controller = (void *)(intptr_t)set_node->controller_id;
  RutObject *object = (void *)(intptr_t)set_node->object_id;
  RutProperty *property;
  RutBoxed boxed;

  if (!controller || !object)
    return false;

  property = rut_introspectable_get_property (object, set_node->property_id);

  rig_pb_init_boxed_value (ctx->unserializer,
                           &boxed,
                           property->spec->type,
                           set_node->value);

  rig_controller_insert_path_value (controller,
                                    property,
                                    set_node->t,
                                    &boxed);

  return true;
}

/* XXX: This is equivalent to _add_path_node so should be redundant! */
static void
_copy_op_controller_path_set_node (RigEngineOpCopyContext *ctx,
                                   Rig__Operation *src_pb_op,
                                   Rig__Operation *pb_op)
{
  pb_op->controller_path_set_node =
    rig_pb_dup (ctx->serializer,
                Rig__Operation__ControllerPathSetNode,
                rig__operation__controller_path_set_node__init,
                src_pb_op->controller_path_set_node);

  pb_op->controller_path_set_node->value =
    maybe_copy_property_value (ctx, src_pb_op->controller_path_set_node->value);
}

/* XXX: This is equivalent to _add_path_node so should be redundant! */
static bool
_map_op_controller_path_set_node (RigEngineOpMapContext *ctx,
                                  Rig__Operation *pb_op)
{
  int64_t *id_ptrs[] = {
      &pb_op->controller_path_set_node->object_id,
      &pb_op->controller_path_set_node->controller_id,
      NULL
  };

  if (!map_ids (ctx, id_ptrs))
    return false;

  if (!maybe_map_property_value (ctx, pb_op->controller_path_set_node->value))
    return false;

  return true;
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
  Rig__Operation__ControllerAddProperty *add_property =
    pb_op->controller_add_property;
  RigController *controller = (void *)(intptr_t)add_property->controller_id;
  RutObject *object = (void *)(intptr_t)add_property->object_id;
  RutProperty *property;

  if (!controller || !object)
    return false;

  property = rut_introspectable_get_property (object, add_property->property_id);

  rig_controller_add_property (controller, property);

  return true;
}

static void
_copy_op_controller_add_property (RigEngineOpCopyContext *ctx,
                                  Rig__Operation *src_pb_op,
                                  Rig__Operation *pb_op)
{
  pb_op->controller_add_property =
    rig_pb_dup (ctx->serializer,
                Rig__Operation__ControllerAddProperty,
                rig__operation__controller_add_property__init,
                src_pb_op->controller_add_property);
}

static bool
_map_op_controller_add_property (RigEngineOpMapContext *ctx,
                                 Rig__Operation *pb_op)
{
  int64_t *id_ptrs[] = {
      &pb_op->controller_add_property->object_id,
      &pb_op->controller_add_property->controller_id,
      NULL
  };

  if (!map_ids (ctx, id_ptrs))
    return false;

  return true;
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
  Rig__Operation__ControllerRemoveProperty *remove_property =
    pb_op->controller_remove_property;
  RigController *controller = (void *)(intptr_t)remove_property->controller_id;
  RutObject *object = (void *)(intptr_t)remove_property->object_id;
  RutProperty *property;

  if (!controller || !object)
    return false;

  property = rut_introspectable_get_property (object, remove_property->property_id);

  rig_controller_remove_property (controller, property);

  return true;
}

static void
_copy_op_controller_remove_property (RigEngineOpCopyContext *ctx,
                                     Rig__Operation *src_pb_op,
                                     Rig__Operation *pb_op)
{
  pb_op->controller_remove_property =
    rig_pb_dup (ctx->serializer,
                Rig__Operation__ControllerRemoveProperty,
                rig__operation__controller_remove_property__init,
                src_pb_op->controller_remove_property);
}

static bool
_map_op_controller_remove_property (RigEngineOpMapContext *ctx,
                                    Rig__Operation *pb_op)
{
  int64_t *id_ptrs[] = {
      &pb_op->controller_remove_property->object_id,
      &pb_op->controller_remove_property->controller_id,
      NULL
  };

  if (!map_ids (ctx, id_ptrs))
    return false;

  return true;
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
  Rig__Operation__ControllerPropertySetMethod *set_method =
    pb_op->controller_property_set_method;
  RigController *controller = (void *)(intptr_t)set_method->controller_id;
  RutObject *object = (void *)(intptr_t)set_method->object_id;
  RutProperty *property;

  if (!controller || !object)
    return false;

  property = rut_introspectable_get_property (object, set_method->property_id);

  rig_controller_set_property_method (controller, property, set_method->method);

  return true;
}

static void
_copy_op_controller_property_set_method (RigEngineOpCopyContext *ctx,
                                         Rig__Operation *src_pb_op,
                                         Rig__Operation *pb_op)
{
  pb_op->controller_property_set_method =
    rig_pb_dup (ctx->serializer,
                Rig__Operation__ControllerPropertySetMethod,
                rig__operation__controller_property_set_method__init,
                src_pb_op->controller_property_set_method);
}

static bool
_map_op_controller_property_set_method (RigEngineOpMapContext *ctx,
                                        Rig__Operation *pb_op)
{
  int64_t *id_ptrs[] = {
      &pb_op->controller_property_set_method->object_id,
      &pb_op->controller_property_set_method->controller_id,
      NULL
  };

  if (!map_ids (ctx, id_ptrs))
    return false;

  return true;
}

typedef struct _RigEngineOperation
{
  bool (*apply_op) (RigEngineOpApplyContext *ctx,
                    Rig__Operation *pb_op);

  bool (*map_op) (RigEngineOpMapContext *ctx,
                  Rig__Operation *pb_op);

  void (*copy_op) (RigEngineOpCopyContext *ctx,
                   Rig__Operation *src_pb_op,
                   Rig__Operation *pb_op);

} RigEngineOperation;

static RigEngineOperation _rig_engine_ops[] =
{
  /* OP type 0 is invalid... */
  {
    NULL,
    NULL,
    NULL,
  },
  {
    _apply_op_set_property,
    _map_op_set_property,
    _copy_op_set_property,
  },
  {
    _apply_op_add_entity,
    _map_op_add_entity,
    _copy_op_add_entity,
  },
  {
    _apply_op_delete_entity,
    _map_op_delete_entity,
    _copy_op_delete_entity,
  },
  {
    _apply_op_add_component,
    _map_op_add_component,
    _copy_op_add_component,
  },
  {
    _apply_op_delete_component,
    _map_op_delete_component,
    _copy_op_delete_component,
  },
  {
    _apply_op_add_controller,
    _map_op_add_controller,
    _copy_op_add_controller,
  },
  {
    _apply_op_delete_controller,
    _map_op_delete_controller,
    _copy_op_delete_controller,
  },
  {
    _apply_op_controller_set_const,
    _map_op_controller_set_const,
    _copy_op_controller_set_const,
  },
  {
    _apply_op_controller_path_add_node,
    _map_op_controller_path_add_node,
    _copy_op_controller_path_add_node,
  },
  {
    _apply_op_controller_path_delete_node,
    _map_op_controller_path_delete_node,
    _copy_op_controller_path_delete_node,
  },
  {
    _apply_op_controller_path_set_node,
    _map_op_controller_path_set_node,
    _copy_op_controller_path_set_node,
  },
  {
    _apply_op_controller_add_property,
    _map_op_controller_add_property,
    _copy_op_controller_add_property,
  },
  {
    _apply_op_controller_remove_property,
    _map_op_controller_remove_property,
    _copy_op_controller_remove_property,
  },
  {
    _apply_op_controller_property_set_method,
    _map_op_controller_property_set_method,
    _copy_op_controller_property_set_method,
  },
};

void
rig_engine_op_copy_context_init (RigEngineOpCopyContext *copy_ctx,
                                 RigEngine *engine)
{
  copy_ctx->engine = engine;
  copy_ctx->serializer = rig_pb_serializer_new (engine);
  rig_pb_serializer_set_use_pointer_ids_enabled (copy_ctx->serializer, true);
}

void
rig_engine_op_copy_context_destroy (RigEngineOpCopyContext *copy_ctx)
{
  rig_pb_serializer_destroy (copy_ctx->serializer);
}

/* Shallow copys a list of edit operations so that it's safe to
 * be able to map IDs
 *
 * All the operations will be allocated on the engine->frame_stack
 * so there is nothing to explicitly free.
 */
Rig__UIEdit *
rig_engine_copy_pb_ui_edit (RigEngineOpCopyContext *copy_ctx,
                            Rig__UIEdit *pb_ui_edit)
{
  RigPBSerializer *serializer = copy_ctx->serializer;
  Rig__UIEdit *copied_pb_ui_edits;
  Rig__Operation *pb_ops;
  int i;

  copied_pb_ui_edits = rig_pb_new (serializer, Rig__UIEdit, rig__uiedit__init);
  copied_pb_ui_edits->n_ops = pb_ui_edit->n_ops;

  if (!pb_ui_edit->n_ops)
    return copied_pb_ui_edits;

  copied_pb_ui_edits->ops =
    rut_memory_stack_memalign (serializer->stack,
                               sizeof (void *) * copied_pb_ui_edits->n_ops,
                               RUT_UTIL_ALIGNOF (void *));

  pb_ops = rut_memory_stack_memalign (serializer->stack,
                                      (sizeof (Rig__Operation) *
                                       copied_pb_ui_edits->n_ops),
                                      RUT_UTIL_ALIGNOF (Rig__Operation));

  for (i = 0; i < pb_ui_edit->n_ops; i++)
    {
      Rig__Operation *src_pb_op = pb_ui_edit->ops[i];
      Rig__Operation *pb_op = &pb_ops[i];

      rig__operation__init (pb_op);

      copied_pb_ui_edits->ops[i] = pb_op;

      pb_op->type = src_pb_op->type;

      _rig_engine_ops [pb_op->type].copy_op (copy_ctx, src_pb_op, pb_op);
    }

  pb_ui_edit->n_ops = i;

  return copied_pb_ui_edits;
}

void
rig_engine_op_map_context_init (RigEngineOpMapContext *map_ctx,
                                RigEngine *engine,
                                uint64_t (*map_id_cb) (uint64_t id_in,
                                                       void *user_data),
                                void *user_data)
{
  map_ctx->engine = engine;

  map_ctx->map_id_cb = map_id_cb;
  map_ctx->user_data = user_data;
}

void
rig_engine_op_map_context_destroy (RigEngineOpMapContext *map_ctx)
{
  /* Nothing to destroy currently */
}

bool
rig_engine_pb_op_map (RigEngineOpMapContext *ctx,
                      Rig__Operation *pb_op)
{
  return _rig_engine_ops[pb_op->type].map_op (ctx, pb_op);
}

/* This function maps Rig__UIEdit operations from one ID space to
 * another. Operations can optionally also be applied at the same
 * time as being mapped.
 *
 * This function won't apply any operations that weren't successfully
 * mapped.
 *
 * Note: it may be necessary to map at the same time as applying
 * considering that applying ops can create new objects which may need
 * to registered to be able to perform the mapping of subsequent
 * operations.
 */
bool
rig_engine_map_pb_ui_edit (RigEngineOpMapContext *map_ctx,
                           RigEngineOpApplyContext *apply_ctx,
                           Rig__UIEdit *pb_ui_edit)
{
  bool status = true;
  int i;

  for (i = 0; i < pb_ui_edit->n_ops; i++)
    {
      Rig__Operation *pb_op = pb_ui_edit->ops[i];

      if (!_rig_engine_ops [pb_op->type].map_op (map_ctx, pb_op))
        {
          status = false;

          /* Note: all of the operations are allocated on the
           * frame-stack so we don't need to explicitly free anything.
           */
          continue;
        }

      if (apply_ctx)
        {
          if (!_rig_engine_ops [pb_op->type].apply_op (apply_ctx, pb_op))
            status = false;
        }
    }

  return status;
}

static void
nop_unregister_id_cb (uint64_t id, void *user_data)
{
}

void
rig_engine_op_apply_context_init (RigEngineOpApplyContext *ctx,
                                  RigEngine *engine,
                                  void (*register_id_cb) (void *object,
                                                          uint64_t id,
                                                          void *user_data),
                                  void (*unregister_id_cb) (uint64_t id,
                                                            void *user_data),
                                  void *user_data)
{
  ctx->engine = engine;

  ctx->unserializer = rig_pb_unserializer_new (engine);

  ctx->register_id_cb = register_id_cb;
  ctx->unregister_id_cb =
    unregister_id_cb ? unregister_id_cb : nop_unregister_id_cb;
  ctx->user_data = user_data;
}

void
rig_engine_op_apply_context_destroy (RigEngineOpApplyContext *ctx)
{
  rig_pb_unserializer_destroy (ctx->unserializer);
}

void
rig_engine_op_apply_context_set_ui (RigEngineOpApplyContext *ctx,
                                    RigUI *ui)
{
  if (ctx->ui == ui)
    return;

  if (ctx->ui)
    rut_object_unref (ctx->ui);

  ctx->ui = ui;

  if (ui)
    rut_object_ref (ui);
}

bool
rig_engine_pb_op_apply (RigEngineOpApplyContext *ctx,
                        Rig__Operation *pb_op)
{
  return _rig_engine_ops[pb_op->type].apply_op (ctx, pb_op);
}

bool
rig_engine_apply_pb_ui_edit (RigEngineOpApplyContext *ctx,
                             const Rig__UIEdit *pb_ui_edit)
{
  int i;
  bool status = true;

  for (i = 0; i < pb_ui_edit->n_ops; i++)
    {
      Rig__Operation *pb_op = pb_ui_edit->ops[i];

      if (!_rig_engine_ops[pb_op->type].apply_op (ctx, pb_op))
        {
          status = false;
          continue;
        }
    }

  return status;
}
