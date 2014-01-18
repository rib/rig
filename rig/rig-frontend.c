/*
 * Rig
 *
 * Copyright (C) 2013  Intel Corporation.
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

#include <stdlib.h>
#include <sys/socket.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-frontend.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

typedef struct _RigFrontendOp
{
  RutList list_node;

  RigFrontendOpType type;

  union {
    struct {
      RutObject *object;
    } register_object;
    struct {
      RutProperty *property;
      RutBoxed value;
    } set_property;
    struct {
      RutEntity *parent;
      RutEntity *entity;
    } add_entity;
    struct {
      RutEntity *entity;
    } delete_entity;
    struct {
      RutEntity *entity;
      RutComponent *component;
    } add_component;
    struct {
      RutComponent *component;
    } delete_component;
    struct {
      RigController *controller;
    } add_controller;
    struct {
      RigController *controller;
    } delete_controller;
    struct {
      RigController *controller;
      RutProperty *property;
      RutBoxed value;
    } controller_set_const;
    struct {
      RigController *controller;
      RutProperty *property;
      float t;
      RutBoxed value;
    } controller_path_add_node;
    struct {
      RigController *controller;
      RutProperty *property;
      float t;
    } controller_path_delete_node;
    struct {
      RigController *controller;
      RutProperty *property;
      float t;
      RutBoxed value;
    } controller_path_set_node;
    struct {
      RigController *controller;
      RutProperty *property;
    } controller_add_property;
    struct {
      RigController *controller;
      RutProperty *property;
    } controller_remove_property;
    struct {
      RigController *controller;
      RutProperty *property;
      RigControllerMethod method;
    } controller_property_set_method;
  };

} RigFrontendOp;

static void
clear_ops (RigFrontend *frontend)
{
  RigFrontendOp *op, *tmp;

  rut_list_for_each_safe (op, tmp, &frontend->ops, list_node)
    {
      switch (op->type)
        {
        case RIG_FRONTEND_OP_TYPE_REGISTER_OBJECT:
          {
            rut_object_release (op->register_object.object, frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_SET_PROPERTY:
          {
            rut_object_release (op->set_property.property->object, frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_ADD_ENTITY:
          {
            rut_object_release (op->add_entity.parent, frontend);
            rut_object_release (op->add_entity.entity, frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_DELETE_ENTITY:
          {
            rut_object_release (op->delete_entity.entity, frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_ADD_COMPONENT:
          {
            rut_object_release (op->add_component.entity, frontend);
            rut_object_release (op->add_component.component, frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_DELETE_COMPONENT:
          {
            rut_object_release (op->delete_component.component, frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_ADD_CONTROLLER:
          {
            rut_object_release (op->add_controller.controller, frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_DELETE_CONTROLLER:
          {
            rut_object_release (op->delete_controller.controller, frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_CONTROLLER_SET_CONST:
          {
            rut_object_release (op->controller_set_const.controller, frontend);
            rut_object_release (op->controller_set_const.property->object,
                                frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_CONTROLLER_PATH_ADD_NODE:
          {
            rut_object_release (op->controller_path_add_node.controller,
                                frontend);
            rut_object_release (op->controller_path_add_node.property->object,
                                frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_CONTROLLER_PATH_DELETE_NODE:
          {
            rut_object_release (op->controller_path_delete_node.controller,
                                frontend);
            rut_object_release (op->controller_path_delete_node.property->object,
                                frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_CONTROLLER_PATH_SET_NODE:
          {
            rut_object_release (op->controller_path_set_node.controller,
                                frontend);
            rut_object_release (op->controller_path_set_node.property->object,
                                frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_CONTROLLER_ADD_PROPERTY:
          {
            rut_object_release (op->controller_add_property.controller,
                                frontend);
            rut_object_release (op->controller_add_property.property->object,
                                frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_CONTROLLER_REMOVE_PROPERTY:
          {
            rut_object_release (op->controller_remove_property.controller,
                                frontend);
            rut_object_release (op->controller_remove_property.property->object,
                                frontend);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_CONTROLLER_PROPERTY_SET_METHOD:
          {
            rut_object_release (op->controller_property_set_method.controller,
                                frontend);
            rut_object_release (op->controller_property_set_method.property->object,
                                frontend);
            break;
          }
        }

      rut_list_remove (&op->list_node);
      g_slice_free (RigFrontendOp, op);
    }

  frontend->n_ops = 0;
}

void
rig_frontend_serialize_ops (RigFrontend *frontend,
                            RigPBSerializer *serializer,
                            Rig__FrameSetup *pb_frame_setup)
{
  RigEngine *engine = frontend->engine;
  Rig__Operation *pb_ops;
  RigFrontendOp *op, *tmp;
  int i;

  pb_frame_setup->n_ops = frontend->n_ops;
  if (pb_frame_setup->n_ops == 0)
    return;

  pb_frame_setup->ops =
    rut_memory_stack_memalign (engine->serialization_stack,
                               sizeof (void *) * pb_frame_setup->n_ops,
                               RUT_UTIL_ALIGNOF (void *));
  pb_ops =
    rut_memory_stack_memalign (engine->serialization_stack,
                               sizeof (Rig__Operation) *
                               pb_frame_setup->n_ops,
                               RUT_UTIL_ALIGNOF (Rig__Operation));

  i = 0;
  rut_list_for_each_safe (op, tmp, &frontend->ops, list_node)
    {
      Rig__Operation *pb_op = &pb_ops[i];

      rig__operation__init (pb_op);

      pb_op->type = op->type;

      switch (op->type)
        {
        case RIG_FRONTEND_OP_TYPE_REGISTER_OBJECT:
          {
            RutProperty *label_property;
            const char *label;

            pb_op->register_object =
              rig_pb_new (engine, Rig__Operation__RegisterObject,
                          rig__operation__register_object__init);
            label_property =
              rut_introspectable_lookup_property (op->register_object.object,
                                                  "label");
            label = rut_property_get_text (label_property);

            pb_op->register_object->label = (char *)rig_pb_strdup (engine, label);
            pb_op->register_object->object_id =
              (intptr_t)op->register_object.object;
            break;
          }
        case RIG_FRONTEND_OP_TYPE_SET_PROPERTY:
          {
            Rig__Operation__SetProperty *pb_set_property =
              rig_pb_new (engine, Rig__Operation__SetProperty,
                          rig__operation__set_property__init);
            pb_op->set_property = pb_set_property;

            pb_set_property->object_id =
              (intptr_t)op->set_property.property->object;
            pb_set_property->property_id = op->set_property.property->id;
            pb_set_property->value = rig_pb_new (engine,
                                                 Rig__PropertyValue,
                                                 rig__property_value__init);
            rig_pb_property_value_init (serializer,
                                        pb_set_property->value,
                                        &op->set_property.value);
            break;
          }
#if 0
        case RIG_FRONTEND_OP_TYPE_:
          {
            Rig__Operation__FOO *pb_FLIBBLE =
              rig_pb_new (engine, Rig__Operation__FOO,
                          rig__operation__FLIBBLE__init);
            pb_op->FLIBBLE = pb_FLIBBLE;

            pb_FLIBBLE->object_id =
              (intptr_t)op->FLIBBLE.property->object;
            pb_FLIBBLE->property_id = op->FLIBBLE.property->id;
            pb_FLIBBLE->value = rig_pb_new (engine,
                                            Rig__PropertyValue,
                                            rig__property_value__init);
            rig_pb_property_value_init (serializer,
                                        pb_FLIBBLE->value,
                                        &op->FLIBBLE.value);
            break;
          }
#endif
        case RIG_FRONTEND_OP_TYPE_ADD_ENTITY:
          {
            Rig__Operation__AddEntity *pb_add_entity =
              rig_pb_new (engine, Rig__Operation__AddEntity,
                          rig__operation__add_entity__init);
            pb_op->add_entity = pb_add_entity;

            pb_add_entity->parent_entity_id = (intptr_t)op->add_entity.parent;
            pb_add_entity->entity =
              rig_pb_serialize_entity (serializer,
                                       NULL,
                                       op->add_entity.entity);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_DELETE_ENTITY:
          {
            pb_op->delete_entity =
              rig_pb_new (engine, Rig__Operation__DeleteEntity,
                          rig__operation__delete_entity__init);

            pb_op->delete_entity->entity_id =
              (intptr_t)op->delete_entity.entity;
            break;
          }
        case RIG_FRONTEND_OP_TYPE_ADD_COMPONENT:
          {
            pb_op->add_component =
              rig_pb_new (engine, Rig__Operation__AddComponent,
                          rig__operation__add_component__init);

            pb_op->add_component->parent_entity_id =
              (intptr_t)op->add_component.entity;
            pb_op->add_component->component =
              rig_pb_serialize_component (serializer,
                                          op->add_component.component);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_DELETE_COMPONENT:
          {
            pb_op->delete_component =
              rig_pb_new (engine, Rig__Operation__DeleteComponent,
                          rig__operation__delete_component__init);

            pb_op->delete_component->component_id =
              (intptr_t)op->delete_component.component;
            break;
          }
        case RIG_FRONTEND_OP_TYPE_ADD_CONTROLLER:
          {
            pb_op->add_controller =
              rig_pb_new (engine, Rig__Operation__AddController,
                          rig__operation__add_controller__init);

            pb_op->add_controller->controller =
              rig_pb_serialize_controller (serializer,
                                           op->add_controller.controller);
            break;
          }
        case RIG_FRONTEND_OP_TYPE_DELETE_CONTROLLER:
          {
            pb_op->delete_controller =
              rig_pb_new (engine, Rig__Operation__DeleteController,
                          rig__operation__delete_controller__init);

            pb_op->delete_controller->controller_id =
              (intptr_t)op->delete_controller.controller;
            break;
          }
        case RIG_FRONTEND_OP_TYPE_CONTROLLER_SET_CONST:
          {
            pb_op->controller_set_const =
              rig_pb_new (engine, Rig__Operation__ControllerSetConst,
                          rig__operation__controller_set_const__init);

            pb_op->controller_set_const->controller_id =
              (intptr_t)op->controller_set_const.controller;
            pb_op->controller_set_const->object_id =
              (intptr_t)op->controller_set_const.property->object;
            pb_op->controller_set_const->property_id =
              (intptr_t)op->controller_set_const.property->id;
            pb_op->controller_set_const->value =
              rig_pb_new (engine,
                          Rig__PropertyValue,
                          rig__property_value__init);
            rig_pb_property_value_init (serializer,
                                        pb_op->controller_set_const->value,
                                        &op->controller_set_const.value);
            break;
          }

        /* XXX: How will operations such as add_entity be synchonized
         * with the frontend?
         *
         * Simple property changes work because the simulator logs
         * the property change when applying and sends an update
         * back to the editor.
         *
         * As part of a bigger question:
         * It is going to be desirable for logic to be able to
         * instantiate new entities, add/delete components and
         * manipulate Rigs, how is that going to be possible?
         * - Firstly it implies we need to create corresponding
         *   protocol for communicating changes from the simulator
         *   to the frontend.
         * - It raises the question of how, can we determine what
         *   UI state should be saved, vs what state is a side
         *   effect of running UI logic.
         * - If the editor always maintained a pristine UI that
         *   was never modified by UI logic it would even mean that
         *   simple expressions to make object A follow object B
         *   wouldn't be reflected unless in play mode and so the edit
         *   area could become quite disconnected from the thing being
         *   created.
         *
         *   We could have a flag set by the editor on all objects to
         *   distinquish and ignore objects created by scripts when
         *   saving. What about if the scripts modify editor objects?
         *
         *   Considering all ops:
         *   add_entity: ok with a flag to ignore when saving
         *   delete_entity: could dissallow deleting flagged entities
         *                  can we assume this special rule will be
         *                  acceptable since could should only ever
         *                  try to delete things that it owns so it
         *                  would be strange to want to try and delete
         *                  an objected created with the editor.
         *   add_component: ok with a flag to ignore when saving
         *   delete_component: as with delete_entity, we can assume
         *                     code shouldn't ever need to try and
         *                     delete a component created with the
         *                     editor.
         *   add_controller: ok (with flag)
         *   delete_controller: ok if not flagged
         *   controller_set_const: ok if controller not flagged
         *
         *
         *   XXX: I think we really we need to have a clean separation
         *   between the state of "play mode" and "edit mode" and
         *   initially simply say that no logic runs in edit mode.
         *
         *   Both the simulator and frontend should track separate
         *   play vs edit UIs that can be switched between.
         *
         *   Should we run two simulators - for each mode?
         *   This is similar in concept to being connected to slave
         *   while editing. Edits would be forwarded to any slaves
         *   and the simulator in play mode on a best effort basis
         *   (you may be able to get into an inconsistent state but
         *   we want the convenience of getting immediate feedback
         *   for edits without having to always reset the play mode
         *   state.)
         */
#warning "FIXME: resolve this issue"

#if 0
  //Add a new controller property path node / key frame
  message ControllerPathAddNode
  {
    required sint64 controller_id=1;
    required sint64 object_id=2;
    required int32 property_id=3;
    required float t=4;
    required PropertyValue value=5;
  }
  optional ControllerPathAddNode controller_path_add_node=11;

  //Remove a node / key frame from a controller property path
  message ControllerPathDeleteNode
  {
    required sint64 controller_id=1;
    required sint64 object_id=2;
    required int32 property_id=3;
    required float t=4;
  }
  optional ControllerPathDeleteNode controller_path_delete_node=12;

  //Change the value of a controller property path node / key frame
  message ControllerPathSetNode
  {
    required sint64 controller_id=1;
    required sint64 object_id=2;
    required int32 property_id=3;
    required float t=4;
    required PropertyValue value=5;
  }
  optional ControllerPathSetNode controller_path_set_node=13;

  //Associate a property with a controller
  message ControllerAddProperty
  {
    required sint64 controller_id=1;
    required sint64 object_id=2;
    required int32 property_id=3;
  }
  optional ControllerAddProperty controller_add_property=14;

  //Disassociate a property from a controller
  message ControllerRemoveProperty
  {
    required sint64 controller_id=1;
    required sint64 object_id=2;
    required int32 property_id=3;
  }
  optional ControllerRemoveProperty controller_remove_property=15;

  //Change the method of controlling a property
  message ControllerPropertySetMethod
  {
    required sint64 controller_id=1;
    required sint64 object_id=2;
    required int32 property_id=3;

    enum Method { CONSTANT=1; PATH=2; BINDING=3; }

    required Method method=4;
  }
  optional ControllerPropertySetMethod controller_property_set_method=16;

    struct {
      RigController *controller;
      RutProperty *property;
      RutBoxed value;
    } controller_set_const;
    struct {
      RigController *controller;
      RutProperty *property;
      float t;
      RutBoxed value;
    } controller_path_add_node;
    struct {
      RigController *controller;
      RutProperty *property;
      float t;
    } controller_path_delete_node;
    struct {
      RigController *controller;
      RutProperty *property;
      float t;
      RutBoxed value;
    } controller_path_set_node;
    struct {
      RigController *controller;
      RutProperty *property;
    } controller_add_property;
    struct {
      RigController *controller;
      RutProperty *property;
    } controller_remove_property;
    struct {
      RigController *controller;
      RutProperty *property;
      RigControllerMethod method;
    } controller_property_set_method;
#endif
        }

      pb_frame_setup->ops[i] = pb_op;

      i++;
    }

  clear_ops (frontend);
}

void
rig_frontend_op_register_object (RigFrontend *frontend,
                                 RutObject *object)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_REGISTER_OBJECT;
  op->register_object.object = rut_object_claim (object, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_set_property (RigFrontend *frontend,
                              RutProperty *property,
                              RutBoxed *value)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_SET_PROPERTY;
  op->set_property.property = property;
  rut_boxed_copy (&op->set_property.value, value);

  rut_object_claim (property->object, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_add_entity (RigFrontend *frontend,
                            RutEntity *parent,
                            RutEntity *entity)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_ADD_ENTITY;
  op->add_entity.parent = rut_object_claim (parent, frontend);
  op->add_entity.entity = rut_object_claim (entity, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_delete_entity (RigFrontend *frontend,
                               RutEntity *entity)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_DELETE_ENTITY;
  op->delete_entity.entity = rut_object_claim (entity, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_add_component (RigFrontend *frontend,
                               RutEntity *entity,
                               RutComponent *component)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_ADD_COMPONENT;
  op->add_component.entity = rut_object_claim (entity, frontend);
  op->add_component.component = rut_object_claim (component, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_delete_component (RigFrontend *frontend,
                                  RutComponent *component)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_DELETE_COMPONENT;
  op->delete_component.component = rut_object_claim (component, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_add_controller (RigFrontend *frontend,
                                RigController *controller)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_ADD_CONTROLLER;
  op->add_controller.controller = rut_object_claim (controller, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_delete_controller (RigFrontend *frontend,
                                   RigController *controller)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_DELETE_CONTROLLER;
  op->delete_controller.controller = rut_object_claim (controller, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_controller_set_const (RigFrontend *frontend,
                                      RigController *controller,
                                      RutProperty *property,
                                      RutBoxed *value)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_CONTROLLER_SET_CONST;
  op->controller_set_const.controller = rut_object_claim (controller, frontend);
  op->controller_set_const.property = property;

  rut_object_claim (property->object, frontend);

  rut_boxed_copy (&op->controller_set_const.value, value);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_add_path_node (RigFrontend *frontend,
                               RigController *controller,
                               RutProperty *property,
                               float t,
                               RutBoxed *value)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_CONTROLLER_PATH_ADD_NODE;
  op->controller_path_add_node.controller =
    rut_object_claim (controller, frontend);
  op->controller_path_add_node.property = property;

  rut_object_claim (property->object, frontend);

  op->controller_path_add_node.t = t;

  rut_boxed_copy (&op->controller_path_add_node.value, value);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_controller_path_delete_node (RigFrontend *frontend,
                                             RigController *controller,
                                             RutProperty *property,
                                             float t)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_CONTROLLER_PATH_DELETE_NODE;
  op->controller_path_delete_node.controller =
    rut_object_claim (controller, frontend);
  op->controller_path_delete_node.property = property;

  rut_object_claim (property->object, frontend);

  op->controller_path_delete_node.t = t;

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_controller_path_set_node (RigFrontend *frontend,
                                          RigController *controller,
                                          RutProperty *property,
                                          float t,
                                          RutBoxed *value)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_CONTROLLER_PATH_SET_NODE;
  op->controller_path_set_node.controller =
    rut_object_claim (controller, frontend);
  op->controller_path_set_node.property = property;

  rut_object_claim (property->object, frontend);

  op->controller_path_set_node.t = t;

  rut_boxed_copy (&op->controller_path_set_node.value, value);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_controller_add_property (RigFrontend *frontend,
                                         RigController *controller,
                                         RutProperty *property)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_CONTROLLER_ADD_PROPERTY;
  op->controller_add_property.controller =
    rut_object_claim (controller, frontend);
  op->controller_add_property.property = property;

  rut_object_claim (property->object, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_controller_remove_property (RigFrontend *frontend,
                                            RigController *controller,
                                            RutProperty *property)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_CONTROLLER_REMOVE_PROPERTY;
  op->controller_remove_property.controller =
    rut_object_claim (controller, frontend);
  op->controller_remove_property.property = property;

  rut_object_claim (property->object, frontend);

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

void
rig_frontend_op_controller_property_set_method (RigFrontend *frontend,
                                                RigController *controller,
                                                RutProperty *property,
                                                RigControllerMethod method)
{
  RigFrontendOp *op = g_slice_new (RigFrontendOp);

  op->type = RIG_FRONTEND_OP_TYPE_CONTROLLER_PROPERTY_SET_METHOD;
  op->controller_property_set_method.controller =
    rut_object_claim (controller, frontend);
  op->controller_property_set_method.property = property;

  rut_object_claim (property->object, frontend);

  op->controller_property_set_method.method = method;

  rut_list_insert (frontend->ops.prev, &op->list_node);
  frontend->n_ops++;
}

static void
frontend__test (Rig__Frontend_Service *service,
                const Rig__Query *query,
                Rig__TestResult_Closure closure,
                void *closure_data)
{
  Rig__TestResult result = RIG__TEST_RESULT__INIT;
  //RigFrontend *frontend = rig_pb_rpc_closure_get_connection_data (closure_data);

  g_return_if_fail (query != NULL);

  g_print ("Frontend Service: Test Query\n");

  closure (&result, closure_data);
}

static void *
pointer_id_to_object_cb (uint64_t id, void *user_data)
{
  return (void *)(intptr_t)id;
}

static void
frontend__update_ui (Rig__Frontend_Service *service,
                     const Rig__UIDiff *ui_diff,
                     Rig__UpdateUIAck_Closure closure,
                     void *closure_data)
{
  Rig__UpdateUIAck ack = RIG__UPDATE_UIACK__INIT;
  RigFrontend *frontend =
    rig_pb_rpc_closure_get_connection_data (closure_data);
  RigEngine *engine = frontend->engine;
  int i;
  int n_changes;
  int n_actions;
  RigPBUnSerializer unserializer;
  RutBoxed boxed;
  RutPropertyContext *prop_ctx = &frontend->engine->ctx->property_ctx;

  //g_print ("Frontend: Update UI Request\n");

  g_return_if_fail (ui_diff != NULL);

  rig_pb_unserializer_init (&unserializer, frontend->engine,
                            false); /* no need for an id-map */

  rig_pb_unserializer_set_id_to_object_callback (&unserializer,
                                                 pointer_id_to_object_cb,
                                                 NULL);

  n_changes = ui_diff->n_property_changes;

  for (i = 0; i < n_changes; i++)
    {
      Rig__PropertyChange *pb_change = ui_diff->property_changes[i];
      void *object;
      RutProperty *property;

      if (!pb_change->has_object_id ||
          pb_change->object_id == 0 ||
          !pb_change->has_property_id ||
          !pb_change->value)
        {
          g_warning ("Frontend: Invalid property change received");
          continue;
        }

      object = (void *)(intptr_t)pb_change->object_id;

#if 1
      g_print ("Frontend: PropertyChange: %p(%s) prop_id=%d\n",
               object,
               rut_object_get_type_name (object),
               pb_change->property_id);
#endif

      property =
        rut_introspectable_get_property (object, pb_change->property_id);
      if (!property)
        {
          g_warning ("Frontend: Failed to find object property by id");
          continue;
        }

      /* XXX: ideally we shouldn't need to init a RutBoxed and set
       * that on a property, and instead we can just directly
       * apply the value to the property we have. */
      rig_pb_init_boxed_value (&unserializer,
                               &boxed,
                               property->spec->type,
                               pb_change->value);

//#warning "XXX: frontend updates are disabled"
      rut_property_set_boxed  (prop_ctx, property, &boxed);
    }

  n_actions = ui_diff->n_actions;

  for (i = 0; i < n_actions; i++)
    {
      Rig__SimulatorAction *pb_action = ui_diff->actions[i];
      switch (pb_action->type)
        {
        case RIG_SIMULATOR_ACTION_TYPE_SET_PLAY_MODE:
          rig_camera_view_set_play_mode_enabled (engine->main_camera_view,
                                                 pb_action->set_play_mode->enabled);
          break;
        case RIG_SIMULATOR_ACTION_TYPE_SELECT_OBJECT:
          {
            Rig__SimulatorAction__SelectObject *pb_select_object =
              pb_action->select_object;

            RutObject *object = (void *)(intptr_t)pb_select_object->object_id;
            rig_select_object (engine,
                               object,
                               pb_select_object->action);
            _rig_engine_update_inspector (engine);
            break;
          }
        }
    }

  rig_pb_unserializer_destroy (&unserializer);

  closure (&ack, closure_data);
}

static Rig__Frontend_Service rig_frontend_service =
  RIG__FRONTEND__INIT(frontend__);


#if 0
static void
handle_simulator_test_response (const Rig__TestResult *result,
                                void *closure_data)
{
  g_print ("Simulator test response received\n");
}
#endif

typedef struct _LoadState
{
  GList *required_assets;
} LoadState;

bool
asset_filter_cb (RutAsset *asset,
                 void *user_data)
{
   switch (rut_asset_get_type (asset))
    {
    case RUT_ASSET_TYPE_BUILTIN:
    case RUT_ASSET_TYPE_TEXTURE:
    case RUT_ASSET_TYPE_NORMAL_MAP:
    case RUT_ASSET_TYPE_ALPHA_MASK:
      return false; /* these assets aren't needed in the simulator */
    case RUT_ASSET_TYPE_PLY_MODEL:
      return true; /* keep mesh assets for picking */
      g_print ("Serialization requires asset %s\n",
               rut_asset_get_path (asset));
      break;
    }

   g_warn_if_reached ();
   return false;
}

static void
handle_load_response (const Rig__LoadResult *result,
                      void *closure_data)
{
  g_print ("Simulator: UI loaded\n");
}

uint64_t
object_to_pointer_id_cb (void *object,
                         void *user_data)
{
  return (uint64_t)(intptr_t)object;
}

static void
frontend_peer_connected (PB_RPC_Client *pb_client,
                         void *user_data)
{
  RigPBSerializer *serializer;
  RigFrontend *frontend = user_data;
  ProtobufCService *simulator_service =
    rig_pb_rpc_client_get_service (pb_client);
  Rig__UI *ui;

  serializer = rig_pb_serializer_new (frontend->engine);

  rig_pb_serializer_set_object_register_callback (serializer,
                                                  object_to_pointer_id_cb,
                                                  NULL);
  rig_pb_serializer_set_object_to_id_callback (serializer,
                                               object_to_pointer_id_cb,
                                               NULL);

  rig_pb_serializer_set_asset_filter (serializer,
                                      asset_filter_cb,
                                      NULL);

  ui = rig_pb_serialize_ui (serializer);

  rig__simulator__load (simulator_service, ui,
                        handle_load_response,
                        NULL);

  rig_pb_serialized_ui_destroy (ui);

#if 0
  Rig__Query query = RIG__QUERY__INIT;


  rig__simulator__test (simulator_service, &query,
                        handle_simulator_test_response, NULL);
#endif

  rig_pb_serializer_destroy (serializer);

  g_print ("Frontend peer connected\n");
}

static void
_rig_frontend_free (void *object)
{
  RigFrontend *frontend = object;

  rig_frontend_stop_service (frontend);

  rut_object_unref (frontend->engine);

  rig_frontend_stop_service (frontend);

  rut_object_free (RigFrontend, object);
}

RutType rig_frontend_type;

static void
_rig_frontend_init_type (void)
{
  RutType *type = &rig_frontend_type;
#define TYPE RigFrontend

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_frontend_free);

#undef TYPE
}

RigFrontend *
rig_frontend_new (RutShell *shell,
                  RigFrontendID id,
                  const char *ui_filename)
{
  pid_t pid;
  int sp[2];

  /*
   * Spawn a simulator process...
   */

  if (socketpair (AF_UNIX, SOCK_STREAM, 0, sp) < 0)
    g_error ("Failed to open simulator ipc");

  pid = fork ();
  if (pid == 0)
    {
      char fd_str[10];
      char *path = RIG_BIN_DIR "rig-simulator";

      /* child - simulator process */
      close (sp[0]);

      if (snprintf (fd_str, sizeof (fd_str), "%d", sp[1]) >= sizeof (fd_str))
        g_error ("Failed to setup environment for simulator process");

      setenv ("_RIG_IPC_FD", fd_str, true);

      switch (id)
        {
        case RIG_FRONTEND_ID_EDITOR:
          setenv ("_RIG_FRONTEND", "editor", true);
          break;
        case RIG_FRONTEND_ID_SLAVE:
          setenv ("_RIG_FRONTEND", "slave", true);
          break;
        case RIG_FRONTEND_ID_DEVICE:
          setenv ("_RIG_FRONTEND", "device", true);
          break;
        }

#ifdef RIG_ENABLE_DEBUG
      if (getenv ("RIG_SIMULATOR"))
        path = getenv ("RIG_SIMULATOR");
#endif

      if (execl (path, path, NULL) < 0)
        g_error ("Failed to run simulator process");

      return NULL;
    }
  else
    {
      RigFrontend *frontend = rut_object_alloc0 (RigFrontend,
                                                 &rig_frontend_type,
                                                 _rig_frontend_init_type);


      frontend->id = id;

      frontend->simulator_pid = pid;
      frontend->fd = sp[0];

      rut_list_init (&frontend->ops);
      frontend->n_ops = 0;

      rig_frontend_start_service (frontend);

      frontend->engine =
        rig_engine_new_for_frontend (shell, frontend, ui_filename);

      return frontend;
    }
}

static void
frontend_peer_error_handler (PB_RPC_Error_Code code,
                             const char *message,
                             void *user_data)
{
  RigFrontend *frontend = user_data;

  g_warning ("Frontend peer error: %s", message);

  rig_frontend_stop_service (frontend);
}

void
rig_frontend_start_service (RigFrontend *frontend)
{
  frontend->frontend_peer =
    rig_rpc_peer_new (frontend->fd,
                      &rig_frontend_service.base,
                      (ProtobufCServiceDescriptor *)&rig__simulator__descriptor,
                      frontend_peer_error_handler,
                      frontend_peer_connected,
                      frontend);
}

void
rig_frontend_stop_service (RigFrontend *frontend)
{
  rut_object_unref (frontend->frontend_peer);
  frontend->frontend_peer = NULL;
}
