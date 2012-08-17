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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RIG_ENTITY_H__
#define __RIG_ENTITY_H__

#include <stdint.h>

#include <cogl/cogl.h>

#include "rig-type.h"
#include "rig-object.h"
#include "rig-interfaces.h"
#include "rig-context.h"

#define RIG_COMPONENT(X) ((RigComponent *)(X))
typedef struct _RigComponent RigComponent;

#define RIG_ENTITY(X) ((RigEntity *)X)
typedef struct _RigEntity RigEntity;
extern RigType rig_entity_type;

typedef enum
{
  RIG_COMPONENT_TYPE_ANIMATION_CLIP,
  RIG_COMPONENT_TYPE_CAMERA,
  RIG_COMPONENT_TYPE_LIGHT,
  RIG_COMPONENT_TYPE_GEOMETRY,
  RIG_COMPONENT_TYPE_MATERIAL,

  RIG_N_COMPNONENTS
} RigComponentType;

typedef struct _RigComponentableProps
{
  RigComponentType type;
  RigEntity *entity; /* back pointer to the entity the component belongs to */
} RigComponentableProps;

typedef struct _RigComponentableVTable
{
  void (*start)   (RigObject *component);
  void (*update)  (RigObject *component, int64_t time);
  void (*draw)    (RigObject *component, CoglFramebuffer *fb);

} RigComponentableVTable;

typedef enum
{
  RIG_ENTITY_FLAG_NONE        = 0,
  RIG_ENTITY_FLAG_DIRTY       = 1 << 0,
  RIG_ENTITY_FLAG_CAST_SHADOW = 1 << 1,
} RigEntityFlag;

void
_rig_entity_init_type (void);

CoglBool
rig_entity_get_cast_shadow (RigEntity *entity);

RigEntity *
rig_entity_new (RigContext *ctx,
                uint32_t id);

const char *
rig_entity_get_label (RigEntity *entity);

void
rig_entity_set_label (RigEntity *entity,
                      const char *label);

uint32_t
rig_entity_get_id (RigEntity *entity);

float
rig_entity_get_x (RigEntity *entity);

void
rig_entity_set_x (RigEntity *entity,
                  float x);

float
rig_entity_get_y (RigEntity *entity);

void
rig_entity_set_y (RigEntity *entity,
                  float y);

float
rig_entity_get_z (RigEntity *entity);

void
rig_entity_set_z (RigEntity *entity,
                  float z);

const float *
rig_entity_get_position (RigEntity *entity);

void
rig_entity_set_position (RigEntity *entity,
                         float position[3]);

void
rig_entity_get_transformed_position (RigEntity *entity,
                                     float position[3]);

CoglQuaternion *
rig_entity_get_rotation (RigEntity *entity);

void
rig_entity_set_rotation (RigEntity *entity,
                         CoglQuaternion *rotation);

void
rig_entity_apply_rotations (RigObject *entity,
                            CoglQuaternion *rotations);

void
rig_entity_get_rotations (RigObject *entity,
                          CoglQuaternion *rotation);

void
rig_entity_get_view_rotations (RigObject *entity,
                               RigObject *camera_entity,
                               CoglQuaternion *rotation);

float
rig_entity_get_scale (RigEntity *entity);

void
rig_entity_set_scale (RigEntity *entity,
                      float scale);

CoglMatrix *
rig_entity_get_transform (RigEntity *entity);

void
rig_entity_add_component (RigEntity *entity,
                          RigObject *component);
void
rig_entity_update (RigEntity *entity,
                   int64_t time);
void
rig_entity_draw (RigEntity *entity,
                 CoglFramebuffer *fb);
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

RigObject *
rig_entity_get_component (RigEntity *entity,
                          RigComponentType type);

void
rig_entity_set_cast_shadow (RigEntity *entity,
                            gboolean cast_shadow);


typedef void (*RigComponentCallback) (RigComponent *component,
                                      void *user_data);

void
rig_entity_foreach_component (RigEntity *entity,
                              RigComponentCallback callback,
                              void *user_data);

void
rig_entity_set_pipeline_cache (RigEntity *entity,
                               CoglPipeline *pipeline);

CoglPipeline *
rig_entity_get_pipeline_cache (RigEntity *entity);

CoglBool
rig_entity_get_visible (RigEntity *entity);

void
rig_entity_set_visible (RigEntity *entity, CoglBool visible);

#endif /* __RIG_ENTITY_H__ */
