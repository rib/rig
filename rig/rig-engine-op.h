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

typedef struct _RigEngineOpApplyContext
{
  RigEngine *engine;
  RigPBUnSerializer *unserializer;
  void *(*id_to_object_cb) (uint64_t id, void *user_data);
  void (*register_id_cb) (void *object, uint64_t id, void *user_data);
  void (*queue_delete_id_cb) (uint64_t id, void *user_data);
  void *user_data;
} RigEngineOpApplyContext;

void
rig_engine_op_apply_context_init (RigEngineOpApplyContext *ctx,
                                  RigEngine *engine,
                                  void (*register_id_cb) (void *object,
                                                          uint64_t id,
                                                          void *user_data),
                                  void *(*id_to_object_cb) (uint64_t id,
                                                            void *user_data),
                                  void (*queue_delete_id_cb) (uint64_t id,
                                                              void *user_data),
                                  void *user_data);

void
rig_engine_op_apply_context_destroy (RigEngineOpApplyContext *ctx);

bool
rig_engine_pb_op_apply (RigEngineOpApplyContext *ctx,
                        Rig__Operation *pb_op);

bool
rig_engine_apply_pb_ui_edit (RigEngineOpApplyContext *ctx,
                             const Rig__UIEdit *pb_ui_edit);

typedef struct _RigEngineOpMapContext
{
  RigEngine *engine;

  RigEngineOpApplyContext apply_ctx;

  RigPBSerializer *serializer;

  uint64_t (*map_id_cb) (uint64_t id_in, void *user_data);
  void (*register_id_cb) (void *object,
                          uint64_t id,
                          void *user_data);
  void *(*id_to_object_cb) (uint64_t id,
                            void *user_data);
  void (*queue_delete_id_cb) (uint64_t id,
                              void *user_data);

  void *user_data;
} RigEngineOpMapContext;

void
rig_engine_op_map_context_init (RigEngineOpMapContext *ctx,
                                RigEngine *engine,
                                uint64_t (*map_id_cb) (uint64_t id_in,
                                                       void *user_data),
                                void (*register_id_cb) (void *object,
                                                        uint64_t id,
                                                        void *user_data),
                                void *(*id_to_object_cb) (uint64_t id,
                                                          void *user_data),
                                void (*queue_delete_id_cb) (uint64_t id,
                                                            void *user_data),
                                void *user_data);

void
rig_engine_op_map_context_destroy (RigEngineOpMapContext *ctx);

Rig__UIEdit *
rig_engine_map_pb_ui_edit (RigEngineOpMapContext *map_op_ctx,
                           Rig__UIEdit *pb_ui_edit);

#endif /* _RIG_ENGINE_OP_H_ */

