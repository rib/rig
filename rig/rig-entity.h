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

#define RIG_COMPONENT(p) ((RigComponent *)(p))

typedef struct _component RigComponent;
typedef struct _entity    RigEntity;

typedef enum
{
  RIG_COMPONENT_TYPE_ANIMATION_CLIP,
  RIG_COMPONENT_TYPE_CAMCORDER,
  RIG_COMPONENT_TYPE_LIGHT,
  RIG_COMPONENT_TYPE_MESH_RENDERER,

  RIG_N_COMPNONENTS
} RigComponentType;

struct _component
{
  RigComponentType type;
  RigEntity *entity;     /* back pointer to the entity the component belongs to */
  void (*start)   (RigComponent *component);
  void (*update)  (RigComponent *component, int64_t time);
  void (*draw)    (RigComponent *component, CoglFramebuffer *fb);
};

typedef enum
{
  RIG_ENTITY_FLAG_NONE        = 0,
  RIG_ENTITY_FLAG_DIRTY       = 1 << 0,
  RIG_ENTITY_FLAG_CAST_SHADOW = 1 << 1,
} RigEntityFlag;

#define ENTITY_HAS_FLAG(entity,flag)    ((entity)->flags & RIG_ENTITY_FLAG_##flag)
#define ENTITY_SET_FLAG(entity,flag)    ((entity)->flags |= RIG_ENTITY_FLAG_##flag)
#define ENTITY_CLEAR_FLAG(entity,flag)  ((entity)->flags &= ~(RIG_ENTITY_FLAG_##flag))

/* DIRTY */
#define entity_is_dirty(entity)         (ENTITY_HAS_FLAG (entity, DIRTY))
#define entity_set_dirty(entity)        (ENTITY_SET_FLAG (entity, DIRTY))
#define entity_clear_dirty(entity)      (ENTITY_CLEAR_FLAG (entity, DIRTY))

/* CAST_SHADOW */
#define entity_cast_shadow(entity)      (ENTITY_HAS_FLAG (entity, CAST_SHADOW))

/* FIXME:
 *  - directly store the position in the transform matrix?
 */
struct _entity
{
  /* private fields */
  uint32_t flags;
  struct { float x, y, z; } position;
  CoglQuaternion rotation;
  float scale;                          /* uniform scaling only */
  CoglMatrix transform;
  GPtrArray *components;
};

void              rig_entity_init             (RigEntity *entity);
float             rig_entity_get_x            (RigEntity *entity);
void              rig_entity_set_x            (RigEntity *entity,
                                               float      x);
float             rig_entity_get_y            (RigEntity *entity);
void              rig_entity_set_y            (RigEntity *entity,
                                               float      y);
float             rig_entity_get_z            (RigEntity *entity);
void              rig_entity_set_z            (RigEntity *entity,
                                               float      z);
void              rig_entity_get_position     (RigEntity *entity,
                                               float      position[3]);
void              rig_entity_set_position     (RigEntity *entity,
                                               float      position[3]);
CoglQuaternion *  rig_entity_get_rotation     (RigEntity *entity);
void              rig_entity_set_rotation     (RigEntity *entity,
                                               CoglQuaternion *rotation);
float             rig_entity_get_scale        (RigEntity *entity);
void              rig_entity_set_scale        (RigEntity *entity,
                                               float      scale);
CoglMatrix *      rig_entity_get_transform    (RigEntity *entity);
void              rig_entity_add_component    (RigEntity    *entity,
                                               RigComponent *component);
void              rig_entity_update           (RigEntity  *entity,
                                               int64_t     time);
void              rig_entity_draw             (RigEntity       *entity,
                                               CoglFramebuffer *fb);
void              rig_entity_translate        (RigEntity *entity,
                                               float      tx,
                                               float      tz,
                                               float      ty);
void              rig_entity_rotate_x_axis    (RigEntity *entity,
                                               float      x_angle);
void              rig_entity_rotate_y_axis    (RigEntity *entity,
                                               float      y_angle);
void              rig_entity_rotate_z_axis    (RigEntity *entity,
                                               float      z_angle);

CoglPipeline *    rig_entity_get_pipeline     (RigEntity *entity);
RigComponent *    rig_entity_get_component    (RigEntity        *entity,
                                               RigComponentType  type);

void              rig_entity_set_cast_shadow  (RigEntity *entity,
                                               gboolean   cast_shadow);

#endif /* __RIG_ENTITY_H__ */
