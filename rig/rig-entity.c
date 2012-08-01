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

enum
{
  PROP_LABEL,
  PROP_X,
  PROP_Y,
  PROP_Z,
  PROP_ROTATION,
  PROP_SCALE,

  N_PROPS
};

/* FIXME:
 *  - directly store the position in the transform matrix?
 */
struct _RigEntity
{
  RigObjectProps _parent;

  int ref_count;

  uint32_t id;

  char *label;

  RigGraphableProps graphable;

  /* private fields */
  struct { float x, y, z; } position;
  CoglQuaternion rotation;
  float scale;                          /* uniform scaling only */
  CoglMatrix transform;

  GPtrArray *components;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[N_PROPS];

  unsigned int dirty:1;
  unsigned int cast_shadow:1;
};

static RigPropertySpec _rig_entity_prop_specs[] = {
  {
    .name = "label",
    .type = RIG_PROPERTY_TYPE_TEXT,
    .getter = rig_entity_get_label,
    .setter = rig_entity_set_label,
    .nick = "Label",
    .blurb = "A label for the entity",
    .flags = RIG_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "x",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .getter = rig_entity_get_x,
    .setter = rig_entity_set_x,
    .nick = "X",
    .blurb = "The entities X coordinate",
    .flags = RIG_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "y",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .getter = rig_entity_get_y,
    .setter = rig_entity_set_y,
    .nick = "Y",
    .blurb = "The entities Y coordinate",
    .flags = RIG_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "z",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .getter = rig_entity_get_z,
    .setter = rig_entity_set_z,
    .nick = "Z",
    .blurb = "The entities Z coordinate",
    .flags = RIG_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "rotation",
    .type = RIG_PROPERTY_TYPE_QUATERNION,
    .getter = rig_entity_get_rotation,
    .setter = rig_entity_set_rotation,
    .nick = "Rotation",
    .blurb = "The entities rotation",
    .flags = RIG_PROPERTY_FLAG_READWRITE
  },
  {
    .name = "scale",
    .type = RIG_PROPERTY_TYPE_FLOAT,
    .getter = rig_entity_get_scale,
    .setter = rig_entity_set_scale,
    .nick = "Scale",
    .blurb = "The entities uniform scale factor",
    .flags = RIG_PROPERTY_FLAG_READWRITE
  },
  { 0 }
};

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
  rig_type_add_interface (&rig_entity_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigEntity, introspectable),
                          NULL); /* no implied vtable */
}

RigEntity *
rig_entity_new (RigContext *ctx,
                uint32_t id)
{
  RigEntity *entity = g_slice_new0 (RigEntity);

  rig_object_init (&entity->_parent, &rig_entity_type);

  entity->ref_count = 1;

  rig_simple_introspectable_init (entity,
                                  _rig_entity_prop_specs,
                                  entity->properties);

  entity->id = id;
  entity->position.x = 0.0f;
  entity->position.y = 0.0f;
  entity->position.z = 0.0f;

  entity->scale = 1.0f;

  cogl_quaternion_init_identity (&entity->rotation);
  cogl_matrix_init_identity (&entity->transform);
  entity->components = g_ptr_array_new ();

  rig_graphable_init (entity);

  return entity;
}

void
rig_entity_set_label (RigEntity *entity,
                      const char *label)
{
  if (entity->label)
    g_free (entity->label);

  entity->label = g_strdup (label);
}

const char *
rig_entity_get_label (RigEntity *entity)
{
  return entity->label;
}

uint32_t
rig_entity_get_id (RigEntity *entity)
{
  return entity->id;
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

void
rig_entity_get_transformed_position (RigEntity *entity,
                                     float position[3])
{
  CoglMatrix transform;
  float w = 1;

  rig_graphable_get_transform (entity, &transform);

  cogl_matrix_transform_point (&transform,
                               &position[0],
                               &position[1],
                               &position[2],
                               &w);
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
                      float      ty,
                      float      tz)
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

void
rig_entity_foreach_component (RigEntity *entity,
                              RigComponentCallback callback,
                              void *user_data)
{
  int i;
  for (i = 0; i < entity->components->len; i++)
    callback (g_ptr_array_index (entity->components, i), user_data);
}

CoglBool
rig_entity_get_cast_shadow (RigEntity *entity)
{
  return entity->cast_shadow;
}
