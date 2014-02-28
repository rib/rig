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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RUT_ENTITY_H__
#define __RUT_ENTITY_H__

#include <stdint.h>

#include <cogl/cogl.h>

#include <rut.h>

#include "rig-types.h"

typedef struct _RutComponent RutComponent;

typedef struct _RigEntity RigEntity;
extern RutType rig_entity_type;

typedef enum
{
  RUT_COMPONENT_TYPE_ANIMATION_CLIP,
  RUT_COMPONENT_TYPE_CAMERA,
  RUT_COMPONENT_TYPE_LIGHT,
  RUT_COMPONENT_TYPE_GEOMETRY,
  RUT_COMPONENT_TYPE_MATERIAL,
  RUT_COMPONENT_TYPE_HAIR,
  RUT_COMPONENT_TYPE_INPUT,
  RUT_N_COMPNONENTS
} RutComponentType;

typedef struct _RutComponentableProps
{
  RutComponentType type;
  RigEntity *entity; /* back pointer to the entity the component belongs to */
} RutComponentableProps;

typedef struct _RutComponentableVTable
{
  RutObject *(*copy) (RutObject *component);
} RutComponentableVTable;

typedef enum
{
  RUT_ENTITY_FLAG_NONE        = 0,
  RUT_ENTITY_FLAG_DIRTY       = 1 << 0,
  RUT_ENTITY_FLAG_CAST_SHADOW = 1 << 1,
} RigEntityFlag;

enum
{
  RUT_ENTITY_PROP_LABEL,
  RUT_ENTITY_PROP_POSITION,
  RUT_ENTITY_PROP_ROTATION,
  RUT_ENTITY_PROP_SCALE,

  RUT_ENTITY_N_PROPS
};

struct _RigEntity
{
  RutObjectBase _base;

  RutContext *ctx;

  char *label;

  RutGraphableProps graphable;

  /* private fields */
  float position[3];
  CoglQuaternion rotation;
  float scale;                          /* uniform scaling only */
  CoglMatrix transform;

  GPtrArray *components;

  void *renderer_priv;

  RutIntrospectableProps introspectable;
  RutProperty properties[RUT_ENTITY_N_PROPS];

  unsigned int dirty:1;
};

void
_rig_entity_init_type (void);

RigEntity *
rig_entity_new (RutContext *ctx);

RigEntity *
rig_entity_copy (RigEntity *entity);

RutContext *
rig_entity_get_context (RigEntity *entity);

const char *
rig_entity_get_label (RutObject *entity);

void
rig_entity_set_label (RutObject *entity,
                      const char *label);

float
rig_entity_get_x (RutObject *entity);

void
rig_entity_set_x (RutObject *entity,
                  float x);

float
rig_entity_get_y (RutObject *entity);

void
rig_entity_set_y (RutObject *entity,
                  float y);

float
rig_entity_get_z (RutObject *entity);

void
rig_entity_set_z (RutObject *entity,
                  float z);

const float *
rig_entity_get_position (RutObject *entity);

void
rig_entity_set_position (RutObject *entity,
                         const float position[3]);

void
rig_entity_get_transformed_position (RigEntity *entity,
                                     float position[3]);

const CoglQuaternion *
rig_entity_get_rotation (RutObject *entity);

void
rig_entity_set_rotation (RutObject *entity,
                         const CoglQuaternion *rotation);

void
rig_entity_apply_rotations (RutObject *entity,
                            CoglQuaternion *rotations);

void
rig_entity_get_rotations (RutObject *entity,
                          CoglQuaternion *rotation);

void
rig_entity_get_view_rotations (RutObject *entity,
                               RutObject *camera_entity,
                               CoglQuaternion *rotation);

float
rig_entity_get_scale (RutObject *entity);

void
rig_entity_set_scale (RutObject *entity,
                      float scale);

float
rig_entity_get_scales (RutObject *entity);

const CoglMatrix *
rig_entity_get_transform (RutObject *self);

void
rig_entity_add_component (RigEntity *entity,
                          RutObject *component);

void
rig_entity_remove_component (RigEntity *entity,
                             RutObject *component);

void
rig_entity_translate (RigEntity *entity,
                      float tx,
                      float tz,
                      float ty);

void
rig_entity_set_translate (RigEntity *entity,
                          float tx,
                          float ty,
                          float tz);

void
rig_entity_rotate_x_axis (RigEntity *entity,
                          float x_angle);
void
rig_entity_rotate_y_axis (RigEntity *entity,
                          float y_angle);
void
rig_entity_rotate_z_axis (RigEntity *entity,
                          float z_angle);

RutObject *
rig_entity_get_component (RigEntity *entity,
                          RutComponentType type);

typedef void (*RutComponentCallback) (RutComponent *component,
                                      void *user_data);

void
rig_entity_foreach_component (RigEntity *entity,
                              RutComponentCallback callback,
                              void *user_data);

void
rig_entity_foreach_component_safe (RigEntity *entity,
                                   RutComponentCallback callback,
                                   void *user_data);

void
rig_entity_notify_changed (RigEntity *entity);

void
rig_entity_reap (RigEntity *entity, RigEngine *engine);

void
rig_component_reap (RutObject *component, RigEngine *engine);

#endif /* __RUT_ENTITY_H__ */
