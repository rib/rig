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

#include "components/rig-mesh-renderer.h"
#include "rig-entity.h"

void rig_entity_init (RigEntity *entity)
{
  entity->position.x = 0.0f;
  entity->position.y = 0.0f;
  entity->position.z = 0.0f;

  cogl_quaternion_init_identity (&entity->rotation);
  cogl_matrix_init_identity (&entity->transform);
  entity->components = g_ptr_array_new ();
}

float
rig_entity_get_x (RigEntity *entity)
{
  return entity->position.x;
}

void
rig_entity_set_x (RigEntity *entity,
                 float   x)
{
  entity->position.x = x;
  entity_set_dirty (entity);
}

float
rig_entity_get_y (RigEntity *entity)
{
  return entity->position.y;
}

void
rig_entity_set_y (RigEntity *entity,
                  float      y)
{
  entity->position.y = y;
  entity_set_dirty (entity);
}

float
rig_entity_get_z (RigEntity *entity)
{
  return entity->position.z;
}

void
rig_entity_set_z (RigEntity *entity,
                  float      z)
{
  entity->position.z = z;
  entity_set_dirty (entity);
}

void rig_entity_set_position (RigEntity *entity,
                              float      position[3])
{
  entity->position.x = position[0];
  entity->position.y = position[1];
  entity->position.z = position[2];
  entity_set_dirty (entity);
}

CoglQuaternion *
rig_entity_get_rotation (RigEntity *entity)
{
  return &entity->rotation;
}

void rig_entity_set_rotation (RigEntity      *entity,
                              CoglQuaternion *rotation)
{
  cogl_quaternion_init_from_quaternion (&entity->rotation, rotation);
  entity_set_dirty (entity);
}

CoglMatrix *
rig_entity_get_transform (RigEntity *entity)
{
  CoglMatrix rotation;

  if (!entity_is_dirty (entity))
    return &entity->transform;

  cogl_matrix_init_translation (&entity->transform,
                                entity->position.x,
                                entity->position.y,
                                entity->position.z);
  cogl_matrix_init_from_quaternion (&rotation, &entity->rotation);
  cogl_matrix_multiply (&entity->transform, &entity->transform, &rotation);

  entity_clear_dirty (entity);

  return &entity->transform;
}

void
rig_entity_add_component (RigEntity    *entity,
                          RigComponent *component)
{
  component->entity = entity;
  g_ptr_array_add (entity->components, component);
}

void
rig_entity_update (RigEntity *entity,
                   int64_t    time)
{
  int i;

  for (i = 0; i < entity->components->len; i++)
    {
      RigComponent *component = g_ptr_array_index (entity->components, i);

      if (component->update)
        component->update(component, time);
    }
}

void
rig_entity_draw (RigEntity       *entity,
                 CoglFramebuffer *fb)
{
  int i;

  for (i = 0; i < entity->components->len; i++)
    {
      RigComponent *component = g_ptr_array_index (entity->components, i);

      if (component->draw)
        component->draw(component, fb);
    }
}

void
rig_entity_translate (RigEntity *entity,
                      float      tx,
                      float      tz,
                      float      ty)
{
  entity->position.x += tx;
  entity->position.y += ty;
  entity->position.z += tz;

  entity_set_dirty (entity);
}

void
rig_entity_rotate_x_axis (RigEntity *entity,
                          float      x_angle)
{
  CoglQuaternion x_rotation, *current;

  /* XXX: avoid the allocation here, and/or make the muliplication in place */
  current = cogl_quaternion_copy (&entity->rotation);
  cogl_quaternion_init_from_x_rotation (&x_rotation, x_angle);
  cogl_quaternion_multiply (&entity->rotation, current, &x_rotation);
  cogl_quaternion_free (current);

  entity_set_dirty (entity);
}

void
rig_entity_rotate_y_axis (RigEntity *entity,
                          float      y_angle)
{
  CoglQuaternion y_rotation, *current;

  /* XXX: avoid the allocation here, and/or make the muliplication in place */
  current = cogl_quaternion_copy (&entity->rotation);
  cogl_quaternion_init_from_y_rotation (&y_rotation, y_angle);
  cogl_quaternion_multiply (&entity->rotation, current, &y_rotation);
  cogl_quaternion_free (current);

  entity_set_dirty (entity);
}

void
rig_entity_rotate_z_axis (RigEntity *entity,
                          float      z_angle)
{
  CoglQuaternion z_rotation, *current;

  /* XXX: avoid the allocation here, and/or make the muliplication in place */
  current = cogl_quaternion_copy (&entity->rotation);
  cogl_quaternion_init_from_z_rotation (&z_rotation, z_angle);
  cogl_quaternion_multiply (&entity->rotation, current, &z_rotation);
  cogl_quaternion_free (current);

  entity_set_dirty (entity);
}

CoglPipeline *
rig_entity_get_pipeline (RigEntity *entity)
{
  int i;

  for (i = 0; i < entity->components->len; i++)
    {
      RigComponent *component = g_ptr_array_index (entity->components, i);

      if (component->type == RIG_COMPONENT_TYPE_MESH_RENDERER)
        return RIG_MESH_RENDERER (component)->pipeline;
    }

    return NULL;
}

void rig_entity_set_cast_shadow (RigEntity *entity,
                                 gboolean   cast_shadow)
{
  if (cast_shadow)
    ENTITY_SET_FLAG (entity, CAST_SHADOW);
  else
    ENTITY_CLEAR_FLAG (entity, CAST_SHADOW);
}

RigComponent *
rig_entity_get_component (RigEntity        *entity,
                          RigComponentType  type)
{
  int i;

  for (i = 0; i < entity->components->len; i++)
    {
      RigComponent *component = g_ptr_array_index (entity->components, i);

      if (component->type == type)
        return component;
    }

    return NULL;
}
