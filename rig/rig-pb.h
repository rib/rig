/*
 * Rig
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef __RIG_PB_H__
#define __RIG_PB_H__

#include <rut.h>

typedef struct _RigPBSerializer RigPBSerializer;
typedef struct _RigPBUnSerializer RigPBUnSerializer;

#include "rig-engine.h"
#include "rig.pb-c.h"

typedef void (*PBMessageInitFunc) (void *message);

static inline void *
_rig_pb_new (RutMemoryStack *stack,
             size_t size,
             size_t alignment,
             void *_message_init)
{
  PBMessageInitFunc message_init = _message_init;

  void *msg = rut_memory_stack_memalign (stack,
                                         size,
                                         alignment);
  message_init (msg);
  return msg;
}

#define rig_pb_new(engine, type, init) \
  _rig_pb_new (engine->serialization_stack, \
               sizeof (type), \
               RUT_UTIL_ALIGNOF (type), \
               init)

const char *
rig_pb_strdup (RigEngine *engine,
               const char *string);

void
rig_pb_unserializer_collect_error (RigPBUnSerializer *unserializer,
                                   const char *format,
                                   ...);

typedef void (*RigAssetReferenceCallback) (RutAsset *asset,
                                           void *user_data);

RigPBSerializer *
rig_pb_serializer_new (RigEngine *engine);

typedef bool (*RigPBAssetFilter) (RutAsset *asset, void *user_data);

void
rig_pb_serializer_set_asset_filter (RigPBSerializer *serializer,
                                    RigPBAssetFilter filter,
                                    void *user_data);

typedef uint64_t (*RigPBSerializerObjecRegisterCallback) (void *object,
                                                          void *user_data);

void
rig_pb_serializer_set_object_register_callback (RigPBSerializer *serializer,
                                 RigPBSerializerObjecRegisterCallback callback,
                                 void *user_data);

typedef uint64_t (*RigPBSerializerObjecToIDCallback) (void *object,
                                                      void *user_data);

void
rig_pb_serializer_set_object_to_id_callback (RigPBSerializer *serializer,
                                     RigPBSerializerObjecToIDCallback callback,
                                     void *user_data);

void
rig_pb_serializer_destroy (RigPBSerializer *serializer);

Rig__UI *
rig_pb_serialize_ui (RigPBSerializer *serializer);

Rig__Entity *
rig_pb_serialize_entity (RigPBSerializer *serializer,
                         RutEntity *parent,
                         RutEntity *entity);

Rig__Entity__Component *
rig_pb_serialize_component (RigPBSerializer *serializer,
                            RutComponent *component);

Rig__Controller *
rig_pb_serialize_controller (RigPBSerializer *serializer,
                             RigController *controller);

void
rig_pb_serialized_ui_destroy (Rig__UI *ui);

Rig__Event **
rig_pb_serialize_input_events (RigPBSerializer *serializer,
                               RutInputQueue *input_queue);

void
rig_pb_property_value_init (RigPBSerializer *serializer,
                            Rig__PropertyValue *pb_value,
                            const RutBoxed *value);


typedef bool (*RigPBUnSerializerObjectRegisterCallback) (void *object,
                                                         uint64_t id,
                                                         void *user_data);

typedef void *(*RigPBUnSerializerIDToObjecCallback) (uint64_t id,
                                                     void *user_data);


struct _RigPBUnSerializer
{
  RigEngine *engine;

  RigPBUnSerializerObjectRegisterCallback object_register_callback;
  void *object_register_data;

  RigPBUnSerializerIDToObjecCallback id_to_object_callback;
  void *id_to_object_data;

  GList *assets;
  GList *entities;
  RutEntity *light;
  GList *controllers;

  GHashTable *id_map;
};

void
rig_pb_unserializer_init (RigPBUnSerializer *unserializer,
                          RigEngine *engine,
                          bool with_id_map);

void
rig_pb_unserializer_set_object_register_callback (
                                 RigPBUnSerializer *unserializer,
                                 RigPBUnSerializerObjectRegisterCallback callback,
                                 void *user_data);

void
rig_pb_unserializer_set_id_to_object_callback (
                                     RigPBUnSerializer *serializer,
                                     RigPBUnSerializerIDToObjecCallback callback,
                                     void *user_data);

void
rig_pb_unserializer_destroy (RigPBUnSerializer *unserializer);

void
rig_pb_unserialize_ui (RigPBUnSerializer *unserializer,
                       const Rig__UI *pb_ui,
                       bool skip_assets);

RutMesh *
rig_pb_unserialize_mesh (RigPBUnSerializer *unserializer,
                         Rig__Mesh *pb_mesh);

void
rig_pb_init_boxed_value (RigPBUnSerializer *unserializer,
                         RutBoxed *boxed,
                         RutPropertyType type,
                         Rig__PropertyValue *pb_value);

#endif /* __RIG_PB_H__ */
