/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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
                          RigEntity *parent,
                          RigEntity *entity);

void
rig_engine_op_delete_entity (RigEngine *engine,
                             RigEntity *entity);

void
rig_engine_op_add_component (RigEngine *engine,
                             RigEntity *entity,
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
  void (*register_id_cb) (void *object, uint64_t id, void *user_data);
  void (*unregister_id_cb) (uint64_t id, void *user_data);
  void *user_data;

  RigUI *ui;
} RigEngineOpApplyContext;

void
rig_engine_op_apply_context_init (RigEngineOpApplyContext *ctx,
                                  RigEngine *engine,
                                  void (*register_id_cb) (void *object,
                                                          uint64_t id,
                                                          void *user_data),
                                  void (*unregister_id_cb) (uint64_t id,
                                                            void *user_data),
                                  void *user_data);

void
rig_engine_op_apply_context_destroy (RigEngineOpApplyContext *ctx);

void
rig_engine_op_apply_context_set_ui (RigEngineOpApplyContext *ctx,
                                    RigUI *ui);

bool
rig_engine_pb_op_apply (RigEngineOpApplyContext *ctx,
                        Rig__Operation *pb_op);

bool
rig_engine_apply_pb_ui_edit (RigEngineOpApplyContext *ctx,
                             const Rig__UIEdit *pb_ui_edit);

typedef struct _RigEngineOpCopyContext
{
  RigEngine *engine;
  RigPBSerializer *serializer;
} RigEngineOpCopyContext;

void
rig_engine_op_copy_context_init (RigEngineOpCopyContext *copy_ctx,
                                 RigEngine *engine);

void
rig_engine_op_copy_context_destroy (RigEngineOpCopyContext *copy_ctx);

/* XXX: Note: that this performs a *shallow* copy; only enough to be
 * able to map object ids. */
Rig__UIEdit *
rig_engine_copy_pb_ui_edit (RigEngineOpCopyContext *copy_ctx,
                            Rig__UIEdit *pb_ui_edit);

typedef struct _RigEngineOpMapContext
{
  RigEngine *engine;

  uint64_t (*map_id_cb) (uint64_t id_in, void *user_data);
  void *user_data;
} RigEngineOpMapContext;

void
rig_engine_op_map_context_init (RigEngineOpMapContext *ctx,
                                RigEngine *engine,
                                uint64_t (*map_id_cb) (uint64_t id_in,
                                                       void *user_data),
                                void *user_data);

void
rig_engine_op_map_context_destroy (RigEngineOpMapContext *ctx);

bool
rig_engine_pb_op_map (RigEngineOpMapContext *ctx,
                      RigEngineOpApplyContext *apply_ctx,
                      Rig__Operation *pb_op);

bool
rig_engine_map_pb_ui_edit (RigEngineOpMapContext *map_ctx,
                           RigEngineOpApplyContext *apply_ctx,
                           Rig__UIEdit *pb_ui_edit);

#endif /* _RIG_ENGINE_OP_H_ */

