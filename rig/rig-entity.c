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
#include "rig.h"

static void
_rig_entity_free (void *object)
{
  RigEntity *entity = object;

  g_ptr_array_free (entity->components, TRUE);

  g_slice_free (RigEntity, entity);
}

static RigRefCountableVTable _rig_entity_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_entity_free
};

static RigGraphableVTable _rig_entity_graphable_vtable = {
  NULL, /* child_removed */
  NULL, /* child_added */
  NULL, /* parent_changed */
};

static RigTransformableVTable _rig_entity_transformable_vtable = {
  rig_entity_get_transform
};


RigType rig_entity_type;

void
_rig_entity_init_type (void)
{
  rig_type_init (&rig_entity_type);
  rig_type_add_interface (&rig_entity_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigEntity, ref_count),
                          &_rig_entity_ref_countable_vtable);
  rig_type_add_interface (&rig_entity_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigEntity, graphable),
                          &_rig_entity_graphable_vtable);
  rig_type_add_interface (&rig_entity_type,
                          RIG_INTERFACE_ID_TRANSFORMABLE,
                          0,
                          &_rig_entity_transformable_vtable);
}

RigEntity *
rig_entity_new (RigContext *ctx)
{
  RigEntity *entity = g_slice_new0 (RigEntity);

  rig_object_init (&entity->_parent, &rig_entity_type);
  rig_graphable_init (entity);

  entity->ref_count = 1;

  entity->position.x = 0.0f;
  entity->position.y = 0.0f;
  entity->position.z = 0.0f;

  entity->scale = 1.0f;

  cogl_quaternion_init_identity (&entity->rotation);
  cogl_matrix_init_identity (&entity->transform);
  entity->components = g_ptr_array_new ();

  return entity;
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
  entity->dirty = TRUE;
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
  entity->dirty = TRUE;
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
  entity->dirty = TRUE;
}

void
rig_entity_get_position (RigEntity *entity,
                         float      position[3])
{
  position[0] = entity->position.x;
  position[1] = entity->position.y;
  position[2] = entity->position.z;
}

void rig_entity_set_position (RigEntity *entity,
                              float      position[3])
{
  entity->position.x = position[0];
  entity->position.y = position[1];
  entity->position.z = position[2];
  entity->dirty = TRUE;
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
  entity->dirty = TRUE;
}

float
rig_entity_get_scale (RigEntity *entity)
{
  return entity->scale;
}

void
rig_entity_set_scale (RigEntity *entity,
                      float      scale)
{
  entity->scale = scale;
  entity->dirty = TRUE;
}

CoglMatrix *
rig_entity_get_transform (RigEntity *entity)
{
  CoglMatrix rotation;

  if (!entity->dirty)
    return &entity->transform;

  cogl_matrix_init_translation (&entity->transform,
                                entity->position.x,
                                entity->position.y,
                                entity->position.z);
  cogl_matrix_init_from_quaternion (&rotation, &entity->rotation);
  cogl_matrix_multiply (&entity->transform, &entity->transform, &rotation);
  cogl_matrix_scale (&entity->transform,
                     entity->scale, entity->scale, entity->scale);

  entity->dirty = FALSE;

  return &entity->transform;
}

void
rig_entity_add_component (RigEntity *entity,
                          RigObject *object)
{
  RigComponentableProps *component =
    rig_object_get_properties (object, RIG_INTERFACE_ID_COMPONENTABLE);
  component->entity = entity;
  g_ptr_array_add (entity->components, object);
}

void
rig_entity_update (RigEntity *entity,
                   int64_t    time)
{
  int i;

  for (i = 0; i < entity->components->len; i++)
    {
      RigObject *component = g_ptr_array_index (entity->components, i);
      RigComponentableVTable *componentable =
        rig_object_get_vtable (component, RIG_INTERFACE_ID_COMPONENTABLE);

      if (componentable->update)
        componentable->update(component, time);
    }
}

void
rig_entity_draw (RigEntity       *entity,
                 CoglFramebuffer *fb)
{
  int i;

  for (i = 0; i < entity->components->len; i++)
    {
      RigObject *component = g_ptr_array_index (entity->components, i);
      RigComponentableVTable *componentable =
        rig_object_get_vtable (component, RIG_INTERFACE_ID_COMPONENTABLE);

      if (componentable->draw)
        componentable->draw(component, fb);
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

  entity->dirty = TRUE;
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

  entity->dirty = TRUE;
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

  entity->dirty = TRUE;
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

  entity->dirty = TRUE;
}

void rig_entity_set_cast_shadow (RigEntity *entity,
                                 gboolean cast_shadow)
{
  if (cast_shadow)
    entity->cast_shadow = TRUE;
  else
    entity->cast_shadow = FALSE;
}

RigObject *
rig_entity_get_component (RigEntity *entity,
                          RigComponentType type)
{
  int i;

  for (i = 0; i < entity->components->len; i++)
    {
      RigObject *component = g_ptr_array_index (entity->components, i);
      RigComponentableProps *component_props =
        rig_object_get_properties (component, RIG_INTERFACE_ID_COMPONENTABLE);

      if (component_props->type == type)
        return component;
    }

  return NULL;
}
