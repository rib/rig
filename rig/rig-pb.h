/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation
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

#ifndef __RIG_PB_H__
#define __RIG_PB_H__

#include <rut.h>

typedef struct _RigPBSerializer RigPBSerializer;
typedef struct _RigPBUnSerializer RigPBUnSerializer;

#include "rig-engine.h"
#include "rig-ui.h"
#include "rig.pb-c.h"
#include "rig-entity.h"

#include "components/rig-camera.h"
#include "components/rig-light.h"

typedef bool (*RigPBAssetFilter) (RigAsset *asset, void *user_data);

typedef uint64_t (*RigPBSerializerObjecRegisterCallback) (void *object,
                                                          void *user_data);

typedef uint64_t (*RigPBSerializerObjecToIDCallback) (void *object,
                                                      void *user_data);

struct _RigPBSerializer
{
  RigEngine *engine;

  RutMemoryStack *stack;

  RigPBAssetFilter asset_filter;
  void *asset_filter_data;

  RigPBSerializerObjecRegisterCallback object_register_callback;
  void *object_register_data;

  RigPBSerializerObjecToIDCallback object_to_id_callback;
  void *object_to_id_data;

  bool only_asset_ids;
  c_list_t *required_assets;

  bool skip_image_data;

  int n_pb_entities;
  c_list_t *pb_entities;

  int n_pb_components;
  c_list_t *pb_components;

  int n_pb_properties;
  c_list_t *pb_properties;

  int n_properties;
  void **properties_out;

  int next_id;
  GHashTable *object_to_id_map;
};


typedef void (*PBMessageInitFunc) (void *message);

static inline void *
_rig_pb_new (RigPBSerializer *serializer,
             size_t size,
             size_t alignment,
             void *_message_init)
{
  RutMemoryStack *stack = serializer->stack;
  PBMessageInitFunc message_init = _message_init;

  void *msg = rut_memory_stack_memalign (stack,
                                         size,
                                         alignment);
  message_init (msg);
  return msg;
}

#define rig_pb_new(serializer, type, init) \
  _rig_pb_new (serializer, \
               sizeof (type), \
               RUT_UTIL_ALIGNOF (type), \
               init)

static inline void *
_rig_pb_dup (RigPBSerializer *serializer,
             size_t size,
             size_t alignment,
             void *_message_init,
             void *src)
{
  RutMemoryStack *stack = serializer->stack;
  PBMessageInitFunc message_init = _message_init;

  void *msg = rut_memory_stack_memalign (stack,
                                         size,
                                         alignment);
  message_init (msg);

  memcpy (msg, src, size);

  return msg;
}

#define rig_pb_dup(serializer, type, init, src) \
  _rig_pb_dup (serializer, \
               sizeof (type), \
               RUT_UTIL_ALIGNOF (type), \
               init, \
               src)

const char *
rig_pb_strdup (RigPBSerializer *serializer,
               const char *string);

typedef void (*RigAssetReferenceCallback) (RigAsset *asset,
                                           void *user_data);

RigPBSerializer *
rig_pb_serializer_new (RigEngine *engine);

void
rig_pb_serializer_set_stack (RigPBSerializer *serializer,
                             RutMemoryStack *stack);

void
rig_pb_serializer_set_use_pointer_ids_enabled (RigPBSerializer *serializer,
                                               bool use_pointers);

void
rig_pb_serializer_set_asset_filter (RigPBSerializer *serializer,
                                    RigPBAssetFilter filter,
                                    void *user_data);

void
rig_pb_serializer_set_only_asset_ids_enabled (RigPBSerializer *serializer,
                                              bool only_ids);

void
rig_pb_serializer_set_object_register_callback (RigPBSerializer *serializer,
                                 RigPBSerializerObjecRegisterCallback callback,
                                 void *user_data);

void
rig_pb_serializer_set_object_to_id_callback (RigPBSerializer *serializer,
                                     RigPBSerializerObjecToIDCallback callback,
                                     void *user_data);

void
rig_pb_serializer_set_skip_image_data (RigPBSerializer *serializer,
                                       bool skip);

uint64_t
rig_pb_serializer_register_object (RigPBSerializer *serializer,
                                   void *object);

uint64_t
rig_pb_serializer_lookup_object_id (RigPBSerializer *serializer, void *object);

void
rig_pb_serializer_destroy (RigPBSerializer *serializer);

Rig__UI *
rig_pb_serialize_ui (RigPBSerializer *serializer,
                     bool play_mode,
                     RigUI *ui);

Rig__Entity *
rig_pb_serialize_entity (RigPBSerializer *serializer,
                         RigEntity *parent,
                         RigEntity *entity);

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

Rig__Operation **
rig_pb_serialize_ops_queue (RigPBSerializer *serializer,
                            RutQueue *ops);

Rig__PropertyValue *
pb_property_value_new (RigPBSerializer *serializer,
                       const RutBoxed *value);

typedef void (*RigPBUnSerializerObjectRegisterCallback) (void *object,
                                                         uint64_t id,
                                                         void *user_data);

typedef void (*RigPBUnSerializerObjectUnRegisterCallback) (uint64_t id,
                                                           void *user_data);

typedef void *(*RigPBUnSerializerIDToObjecCallback) (uint64_t id,
                                                     void *user_data);

typedef RigAsset *(*RigPBUnSerializerAssetCallback) (RigPBUnSerializer *unserializer,
                                                     Rig__Asset *pb_asset,
                                                     void *user_data);


struct _RigPBUnSerializer
{
  RigEngine *engine;

  RutMemoryStack *stack;

  RigPBUnSerializerObjectRegisterCallback object_register_callback;
  void *object_register_data;

  RigPBUnSerializerObjectUnRegisterCallback object_unregister_callback;
  void *object_unregister_data;

  RigPBUnSerializerIDToObjecCallback id_to_object_callback;
  void *id_to_object_data;

  RigPBUnSerializerAssetCallback unserialize_asset_callback;
  void *unserialize_asset_data;

  c_list_t *assets;
  c_list_t *entities;
  RigEntity *light;
  c_list_t *controllers;

  GHashTable *id_to_object_map;
};

RigPBUnSerializer *
rig_pb_unserializer_new (RigEngine *engine);

void
rig_pb_unserializer_set_object_register_callback (
                                 RigPBUnSerializer *unserializer,
                                 RigPBUnSerializerObjectRegisterCallback callback,
                                 void *user_data);

void
rig_pb_unserializer_set_object_unregister_callback (
                                 RigPBUnSerializer *unserializer,
                                 RigPBUnSerializerObjectUnRegisterCallback callback,
                                 void *user_data);

void
rig_pb_unserializer_set_id_to_object_callback (
                                     RigPBUnSerializer *serializer,
                                     RigPBUnSerializerIDToObjecCallback callback,
                                     void *user_data);

void
rig_pb_unserializer_set_asset_unserialize_callback (
                                     RigPBUnSerializer *unserializer,
                                     RigPBUnSerializerAssetCallback callback,
                                     void *user_data);

void
rig_pb_unserializer_collect_error (RigPBUnSerializer *unserializer,
                                   const char *format,
                                   ...);

void
rig_pb_unserializer_register_object (RigPBUnSerializer *unserializer,
                                     void *object,
                                     uint64_t id);

void
rig_pb_unserializer_unregister_object (RigPBUnSerializer *unserializer,
                                       uint64_t id);

void
rig_pb_unserializer_destroy (RigPBUnSerializer *unserializer);

RigUI *
rig_pb_unserialize_ui (RigPBUnSerializer *unserializer,
                       const Rig__UI *pb_ui);

RutMesh *
rig_pb_unserialize_mesh (RigPBUnSerializer *unserializer,
                         Rig__Mesh *pb_mesh);

void
rig_pb_init_boxed_value (RigPBUnSerializer *unserializer,
                         RutBoxed *boxed,
                         RutPropertyType type,
                         Rig__PropertyValue *pb_value);

/* Note: this will also add the component to the given entity, since
 * many components can't be configured until they are associated with
 * an entity. */
RutObject *
rig_pb_unserialize_component (RigPBUnSerializer *unserializer,
                              RigEntity *entity,
                              Rig__Entity__Component *pb_component);

RigEntity *
rig_pb_unserialize_entity (RigPBUnSerializer *unserializer,
                           Rig__Entity *pb_entity);

RigController *
rig_pb_unserialize_controller_bare (RigPBUnSerializer *unserializer,
                                    Rig__Controller *pb_controller);

void
rig_pb_unserialize_controller_properties (RigPBUnSerializer *unserializer,
                                          RigController *controller,
                                          int n_properties,
                                          Rig__Controller__Property **properties);

#endif /* __RIG_PB_H__ */
