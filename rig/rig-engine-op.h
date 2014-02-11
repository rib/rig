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

#ifndef _RIG_ENGINE_OP_H_
#define _RIG_ENGINE_OP_H_


typedef enum _RigEngineOpType
{
  //RIG_ENGINE_OP_TYPE_REGISTER_OBJECT=1,
  RIG_ENGINE_OP_TYPE_SET_PROPERTY = 1,
  RIG_ENGINE_OP_TYPE_ADD_ENTITY,
  RIG_ENGINE_OP_TYPE_DELETE_ENTITY,
  RIG_ENGINE_OP_TYPE_ADD_COMPONENT,
  RIG_ENGINE_OP_TYPE_DELETE_COMPONENT,
  RIG_ENGINE_OP_TYPE_ADD_CONTROLLER,
  RIG_ENGINE_OP_TYPE_DELETE_CONTROLLER,
  RIG_ENGINE_OP_TYPE_CONTROLLER_SET_CONST,
  RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_ADD_NODE,
  RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_DELETE_NODE,
  RIG_ENGINE_OP_TYPE_CONTROLLER_PATH_SET_NODE,
  RIG_ENGINE_OP_TYPE_CONTROLLER_ADD_PROPERTY,
  RIG_ENGINE_OP_TYPE_CONTROLLER_REMOVE_PROPERTY,
  RIG_ENGINE_OP_TYPE_CONTROLLER_PROPERTY_SET_METHOD,
  //RIG_ENGINE_OP_TYPE_SET_PLAY_MODE,
} RigEngineOpType;


#if 0
void
rig_engine_op_register_object (RigEngine *engine,
                               RutObject *object);
#endif

void
rig_engine_op_set_property (RigEngine *engine,
                            RutProperty *property,
                            RutBoxed *value);

void
rig_engine_op_add_entity (RigEngine *engine,
                          RutEntity *parent,
                          RutEntity *entity);

void
rig_engine_op_delete_entity (RigEngine *engine,
                             RutEntity *entity);

void
rig_engine_op_add_component (RigEngine *engine,
                             RutEntity *entity,
                             RutComponent *component);

void
rig_engine_op_delete_component (RigEngine *engine,
                                RutComponent *component);

void
rig_engine_op_add_controller (RigEngine *engine,
                              RigController *controller);

void
rig_engine_op_delete_controller (RigEngine *engine,
                                 RigController *controller);

void
rig_engine_op_controller_set_const (RigEngine *engine,
                                    RigController *controller,
                                    RutProperty *property,
                                    RutBoxed *value);

void
rig_engine_op_controller_path_add_node (RigEngine *engine,
                                        RigController *controller,
                                        RutProperty *property,
                                        float t,
                                        RutBoxed *value);

void
rig_engine_op_controller_path_delete_node (RigEngine *engine,
                                           RigController *controller,
                                           RutProperty *property,
                                           float t);

void
rig_engine_op_controller_path_set_node (RigEngine *engine,
                                        RigController *controller,
                                        RutProperty *property,
                                        float t,
                                        RutBoxed *value);

void
rig_engine_op_controller_add_property (RigEngine *engine,
                                       RigController *controller,
                                       RutProperty *property);

void
rig_engine_op_controller_remove_property (RigEngine *engine,
                                          RigController *controller,
                                          RutProperty *property);

void
rig_engine_op_controller_property_set_method (RigEngine *engine,
                                              RigController *controller,
                                              RutProperty *property,
                                              RigControllerMethod method);

#if 0
void
rig_engine_op_set_play_mode (RigEngine *engine,
                             bool play_mode_enabled);
#endif

Rig__Operation **
rig_engine_serialize_ops (RigEngine *engine,
                          RigPBSerializer *serializer);

typedef void *(*RigEngineIdToObjectCallback) (uint64_t id, void *user_data);
typedef void (*RigEngineRegisterIdCallback) (void *object,
                                             uint64_t id,
                                             void *user_data);
typedef void (*RigEngineDeleteIdCallback) (uint64_t id, void *user_data);

typedef struct _RigEngineOpApplyContext
{
  RigEngine *engine;
  RigPBUnSerializer *unserializer;
  RigEngineIdToObjectCallback id_to_object_cb;
  RigEngineRegisterIdCallback register_id_cb;
  RigEngineDeleteIdCallback queue_delete_id_cb;
  void *user_data;
} RigEngineOpApplyContext;

void
rig_engine_op_apply_context_init (RigEngineOpApplyContext *ctx,
                                  RigPBUnSerializer *unserializer,
                                  RigEngineRegisterIdCallback register_id_cb,
                                  RigEngineIdToObjectCallback id_to_object_cb,
                                  RigEngineDeleteIdCallback queue_delete_id_cb,
                                  void *user_data);

void
rig_engine_op_apply_context_destroy (RigEngineOpApplyContext *ctx);

bool
rig_engine_pb_op_apply (RigEngineOpApplyContext *ctx,
                        Rig__Operation *pb_op);

bool
rig_engine_apply_pb_ui_edit (RigEngineOpApplyContext *ctx,
                             Rig__UIEdit *pb_ui_edit);

typedef uint64_t (*RigEngineMapIDCallback) (uint64_t edit_id, void *user_data);

Rig__UIEdit *
rig_engine_map_pb_ui_edit (RigEngine *engine,
                           Rig__UIEdit *pb_ui_edit,
                           RigEngineMapIDCallback map_id_cb,
                           RigEngineRegisterIdCallback register_id_cb,
                           RigEngineIdToObjectCallback id_to_object_cb,
                           RigEngineDeleteIdCallback queue_delete_id_cb,
                           void *user_data);

#endif /* _RIG_ENGINE_OP_H_ */

