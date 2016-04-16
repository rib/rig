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

#pragma once

#include "rig-ui.h"
#include "rig.pb-c.h"
#include "rig-pb.h"

#include "components/rig-mesh.h"

typedef enum _rig_engine_op_type_t {
    // RIG_ENGINE_OP_TYPE_REGISTER_OBJECT=1,
    RIG_ENGINE_OP_TYPE_SET_PROPERTY = 1,
    RIG_ENGINE_OP_TYPE_DELETE_ENTITY,
    RIG_ENGINE_OP_TYPE_ADD_ENTITY,
    RIG_ENGINE_OP_TYPE_REGISTER_COMPONENT,
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
    RIG_ENGINE_OP_TYPE_ADD_VIEW,
    RIG_ENGINE_OP_TYPE_DELETE_VIEW,
    RIG_ENGINE_OP_TYPE_ADD_BUFFER,
    RIG_ENGINE_OP_TYPE_DELETE_BUFFER,
    RIG_ENGINE_OP_TYPE_BUFFER_SET_DATA,
    RIG_ENGINE_OP_TYPE_MESH_SET_ATTRIBUTES,
} rig_engine_op_type_t;

#if 0
void
rig_engine_op_register_object (rig_engine_t *engine,
                               rut_object_t *object);
#endif

void rig_engine_op_set_property(rig_engine_t *engine,
                                rig_property_t *property,
                                rut_boxed_t *value);

void rig_engine_op_delete_entity(rig_engine_t *engine, rig_entity_t *entity);

void rig_engine_op_add_entity(rig_engine_t *engine,
                              rig_entity_t *parent,
                              rig_entity_t *entity);

void rig_engine_op_register_component(rig_engine_t *engine,
                                      rut_object_t *component);

void rig_engine_op_add_component(rig_engine_t *engine,
                                 rig_entity_t *entity,
                                 rut_object_t *component);

void rig_engine_op_delete_component(rig_engine_t *engine,
                                    rut_object_t *component);

void rig_engine_op_add_controller(rig_engine_t *engine,
                                  rig_controller_t *controller);

void rig_engine_op_delete_controller(rig_engine_t *engine,
                                     rig_controller_t *controller);

void rig_engine_op_controller_set_const(rig_engine_t *engine,
                                        rig_controller_t *controller,
                                        rig_property_t *property,
                                        rut_boxed_t *value);

void rig_engine_op_controller_path_add_node(rig_engine_t *engine,
                                            rig_controller_t *controller,
                                            rig_property_t *property,
                                            float t,
                                            rut_boxed_t *value);

void rig_engine_op_controller_path_delete_node(rig_engine_t *engine,
                                               rig_controller_t *controller,
                                               rig_property_t *property,
                                               float t);

void rig_engine_op_controller_path_set_node(rig_engine_t *engine,
                                            rig_controller_t *controller,
                                            rig_property_t *property,
                                            float t,
                                            rut_boxed_t *value);

void rig_engine_op_controller_add_property(rig_engine_t *engine,
                                           rig_controller_t *controller,
                                           rig_property_t *property);

void rig_engine_op_controller_remove_property(rig_engine_t *engine,
                                              rig_controller_t *controller,
                                              rig_property_t *property);

void
rig_engine_op_controller_property_set_method(rig_engine_t *engine,
                                             rig_controller_t *controller,
                                             rig_property_t *property,
                                             rig_controller_method_t method);

void
rig_engine_op_add_view(rig_engine_t *engine,
                       rig_view_t *view);
void
rig_engine_op_delete_view(rig_engine_t *engine,
                          rig_view_t *view);

void
rig_engine_op_add_buffer(rig_engine_t *engine,
                         rut_buffer_t *buffer);
void
rig_engine_op_delete_buffer(rig_engine_t *engine,
                            rut_buffer_t *buffer);
void
rig_engine_op_buffer_set_data(rig_engine_t *engine,
                              rut_buffer_t *buffer,
                              int offset,
                              uint8_t *data,
                              int data_len);
void
rig_engine_op_mesh_set_attributes(rig_engine_t *engine,
                                  rig_mesh_t *mesh,
                                  rut_attribute_t **attributes,
                                  int n_attributes);

Rig__Operation **rig_engine_serialize_ops(rig_engine_t *engine,
                                          rig_pb_serializer_t *serializer);

typedef struct _rig_engine_op_apply_context_t {
    rig_engine_t *engine;
    rig_pb_unserializer_t *unserializer;
    void (*register_id_cb)(void *object, uint64_t id, void *user_data);
    void (*unregister_id_cb)(uint64_t id, void *user_data);
    void (*id_to_object_map)(uint64_t id, void *user_data);
    void *user_data;

    rig_ui_t *ui;
} rig_engine_op_apply_context_t;

void rig_engine_op_apply_context_init(
    rig_engine_op_apply_context_t *ctx,
    rig_engine_t *engine,
    void (*register_id_cb)(rig_ui_t *ui, void *object, uint64_t id, void *user_data),
    void *(*object_lookup_cb)(rig_ui_t *ui, uint64_t id, void *user_data),
    void *user_data);

void rig_engine_op_apply_context_destroy(rig_engine_op_apply_context_t *ctx);

void rig_engine_op_apply_context_set_ui(rig_engine_op_apply_context_t *ctx,
                                        rig_ui_t *ui);

bool rig_engine_pb_op_apply(rig_engine_op_apply_context_t *ctx,
                            Rig__Operation *pb_op);

bool rig_engine_apply_pb_ui_edit(rig_engine_op_apply_context_t *ctx,
                                 const Rig__UIEdit *pb_ui_edit);

typedef struct _rig_engine_op_copy_context_t {
    rig_engine_t *engine;
    rig_pb_serializer_t *serializer;
} rig_engine_op_copy_context_t;

void rig_engine_op_copy_context_init(rig_engine_op_copy_context_t *copy_ctx,
                                     rig_engine_t *engine);

void rig_engine_op_copy_context_destroy(rig_engine_op_copy_context_t *copy_ctx);

/* XXX: Note: that this performs a *shallow* copy; only enough to be
 * able to map object ids. */
Rig__UIEdit *rig_engine_copy_pb_ui_edit(rig_engine_op_copy_context_t *copy_ctx,
                                        Rig__UIEdit *pb_ui_edit);

typedef struct _rig_engine_op_map_context_t {
    rig_engine_t *engine;

    uint64_t (*map_id_cb)(uint64_t id_in, void *user_data);
    void *user_data;
} rig_engine_op_map_context_t;

void rig_engine_op_map_context_init(rig_engine_op_map_context_t *ctx,
                                    rig_engine_t *engine,
                                    uint64_t (*map_id_cb)(uint64_t id_in,
                                                          void *user_data),
                                    void *user_data);

void rig_engine_op_map_context_destroy(rig_engine_op_map_context_t *ctx);

bool rig_engine_pb_op_map(rig_engine_op_map_context_t *ctx,
                          rig_engine_op_apply_context_t *apply_ctx,
                          Rig__Operation *pb_op);

bool rig_engine_map_pb_ui_edit(rig_engine_op_map_context_t *map_ctx,
                               rig_engine_op_apply_context_t *apply_ctx,
                               Rig__UIEdit *pb_ui_edit);
