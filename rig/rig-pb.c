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

#include <config.h>

#include "rig.pb-c.h"
#include "rig-pb.h"
#include "rig-engine.h"

#include <rut.h>

struct _RigPBSerializer
{
  RigEngine *engine;

  RigPBAssetFilter asset_filter;
  void *asset_filter_data;

  GList *required_assets;

  int n_pb_entities;
  GList *pb_entities;

  int n_pb_components;
  GList *pb_components;

  int n_pb_properties;
  GList *pb_properties;

  int n_properties;
  void **properties_out;

  int next_id;
  GHashTable *id_map;
};

typedef void (*PBMessageInitFunc) (void *message);

static void *
pb_new (RigEngine *engine,
        size_t size,
        void *_message_init)
{
  PBMessageInitFunc message_init = _message_init;

  void *msg = rut_memory_stack_alloc (engine->serialization_stack, size);
  message_init (msg);
  return msg;
}

static Rig__Color *
pb_color_new (RigEngine *engine, const CoglColor *color)
{
  Rig__Color *pb_color = pb_new (engine, sizeof (Rig__Color), rig__color__init);
  pb_color->hex = rut_memory_stack_alloc (engine->serialization_stack,
                                          10); /* #rrggbbaa */
  snprintf (pb_color->hex, 10, "#%02x%02x%02x%02x",
            cogl_color_get_red_byte (color),
            cogl_color_get_green_byte (color),
            cogl_color_get_blue_byte (color),
            cogl_color_get_alpha_byte (color));

  return pb_color;
}

static Rig__Rotation *
pb_rotation_new (RigEngine *engine, const CoglQuaternion *quaternion)
{
  Rig__Rotation *pb_rotation =
    pb_new (engine, sizeof (Rig__Rotation), rig__rotation__init);
  float angle = cogl_quaternion_get_rotation_angle (quaternion);
  float axis[3];

  cogl_quaternion_get_rotation_axis (quaternion, axis);

  pb_rotation->angle = angle;
  pb_rotation->x = axis[0];
  pb_rotation->y = axis[1];
  pb_rotation->z = axis[2];

  return pb_rotation;
}

static Rig__Vec3 *
pb_vec3_new (RigEngine *engine,
             float x,
             float y,
             float z)
{
  Rig__Vec3 *pb_vec3 = pb_new (engine, sizeof (Rig__Vec3), rig__vec3__init);

  pb_vec3->x = x;
  pb_vec3->y = y;
  pb_vec3->z = z;

  return pb_vec3;
}

static Rig__Vec4 *
pb_vec4_new (RigEngine *engine,
             float x,
             float y,
             float z,
             float w)
{
  Rig__Vec4 *pb_vec4 = pb_new (engine, sizeof (Rig__Vec4), rig__vec4__init);

  pb_vec4->x = x;
  pb_vec4->y = y;
  pb_vec4->z = z;
  pb_vec4->w = w;

  return pb_vec4;
}

static Rig__Path *
pb_path_new (RigEngine *engine, RigPath *path)
{
  Rig__Path *pb_path = pb_new (engine, sizeof (Rig__Path), rig__path__init);
  RigNode *node;
  int i;

  if (!path->length)
    return pb_path;

  pb_path->nodes = rut_memory_stack_alloc (engine->serialization_stack,
                                           sizeof (void *) * path->length);
  pb_path->n_nodes = path->length;

  i = 0;
  rut_list_for_each (node, &path->nodes, list_node)
    {
      Rig__Node *pb_node =
        pb_new (engine, sizeof (Rig__Node), rig__node__init);

      pb_path->nodes[i] = pb_node;

      pb_node->has_t = TRUE;
      pb_node->t = node->t;

      pb_node->value = pb_new (engine,
                               sizeof (Rig__PropertyValue),
                               rig__property_value__init);

      switch (path->type)
        {
        case RUT_PROPERTY_TYPE_FLOAT:
            {
              pb_node->value->has_float_value = TRUE;
              pb_node->value->float_value = node->boxed.d.float_val;
              break;
            }
        case RUT_PROPERTY_TYPE_DOUBLE:
            {
              pb_node->value->has_double_value = TRUE;
              pb_node->value->double_value = node->boxed.d.double_val;
              break;
            }
        case RUT_PROPERTY_TYPE_VEC3:
            {
              float *vec3 = node->boxed.d.vec3_val;
              pb_node->value->vec3_value = pb_vec3_new (engine,
                                                        vec3[0],
                                                        vec3[1],
                                                        vec3[2]);
              break;
            }
        case RUT_PROPERTY_TYPE_VEC4:
            {
              float *vec4 = node->boxed.d.vec4_val;
              pb_node->value->vec4_value = pb_vec4_new (engine,
                                                        vec4[0],
                                                        vec4[1],
                                                        vec4[2],
                                                        vec4[3]);
              break;
            }
        case RUT_PROPERTY_TYPE_COLOR:
            {
              pb_node->value->color_value =
                pb_color_new (engine, &node->boxed.d.color_val);
              break;
            }
        case RUT_PROPERTY_TYPE_QUATERNION:
            {
              pb_node->value->quaternion_value =
                pb_rotation_new (engine, &node->boxed.d.quaternion_val);
              break;
            }
        case RUT_PROPERTY_TYPE_INTEGER:
            {
              pb_node->value->has_integer_value = TRUE;
              pb_node->value->integer_value = node->boxed.d.integer_val;
              continue;
              break;
            }
        case RUT_PROPERTY_TYPE_UINT32:
            {
              pb_node->value->has_uint32_value = TRUE;
              pb_node->value->uint32_value = node->boxed.d.uint32_val;
              break;
            }

          /* These types of properties can't be interoplated so they
           * probably shouldn't end up in a path */
        case RUT_PROPERTY_TYPE_ENUM:
        case RUT_PROPERTY_TYPE_BOOLEAN:
        case RUT_PROPERTY_TYPE_TEXT:
        case RUT_PROPERTY_TYPE_ASSET:
        case RUT_PROPERTY_TYPE_OBJECT:
        case RUT_PROPERTY_TYPE_POINTER:
          g_warn_if_reached ();
          break;
        }

      i++;
    }

  return pb_path;
}

static uint64_t
serializer_lookup_object_id (RigPBSerializer *serializer, void *object)
{
  uint64_t *id = g_hash_table_lookup (serializer->id_map, object);

  g_warn_if_fail (id);

  if (rut_object_get_type (object) == &rut_asset_type)
    {
      bool need_asset = true;

      if (serializer->asset_filter)
        {
          need_asset = serializer->asset_filter (object,
                                                 serializer->asset_filter_data);
        }

      if (need_asset)
        {
          serializer->required_assets =
            g_list_prepend (serializer->required_assets, object);
        }
    }

  return *id;
}

static Rig__PropertyValue *
pb_property_value_new (RigPBSerializer *serializer,
                       const RutBoxed *value)
{
  RigEngine *engine = serializer->engine;
  Rig__PropertyValue *pb_value =
    pb_new (engine, sizeof (Rig__PropertyValue), rig__property_value__init);

  switch (value->type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      pb_value->has_float_value = TRUE;
      pb_value->float_value = value->d.float_val;
      break;

    case RUT_PROPERTY_TYPE_DOUBLE:
      pb_value->has_double_value = TRUE;
      pb_value->double_value = value->d.double_val;
      break;

    case RUT_PROPERTY_TYPE_INTEGER:
      pb_value->has_integer_value = TRUE;
      pb_value->integer_value = value->d.integer_val;
      break;

    case RUT_PROPERTY_TYPE_UINT32:
      pb_value->has_uint32_value = TRUE;
      pb_value->uint32_value = value->d.uint32_val;
      break;

    case RUT_PROPERTY_TYPE_BOOLEAN:
      pb_value->has_boolean_value = TRUE;
      pb_value->boolean_value = value->d.boolean_val;
      break;

    case RUT_PROPERTY_TYPE_TEXT:
      pb_value->text_value = value->d.text_val;
      break;

    case RUT_PROPERTY_TYPE_QUATERNION:
      pb_value->quaternion_value =
        pb_rotation_new (engine, &value->d.quaternion_val);
      break;

    case RUT_PROPERTY_TYPE_VEC3:
      pb_value->vec3_value = pb_vec3_new (engine,
                                          value->d.vec3_val[0],
                                          value->d.vec3_val[1],
                                          value->d.vec3_val[2]);
      break;

    case RUT_PROPERTY_TYPE_VEC4:
      pb_value->vec4_value = pb_vec4_new (engine,
                                          value->d.vec4_val[0],
                                          value->d.vec4_val[1],
                                          value->d.vec4_val[2],
                                          value->d.vec4_val[3]);
      break;

    case RUT_PROPERTY_TYPE_COLOR:
      pb_value->color_value = pb_color_new (engine, &value->d.color_val);
      break;

    case RUT_PROPERTY_TYPE_ENUM:
      /* XXX: this should possibly save the string names rather than
       * the integer value? */
      pb_value->has_enum_value = TRUE;
      pb_value->enum_value = value->d.enum_val;
      break;

    case RUT_PROPERTY_TYPE_ASSET:
      pb_value->has_asset_value = true;

      if (value->d.asset_val)
        {
          uint64_t id = serializer_lookup_object_id (serializer, value->d.asset_val);

          g_warn_if_fail (id != 0);

          pb_value->asset_value = id;
        }
      else
        pb_value->asset_value = 0;

      break;

    case RUT_PROPERTY_TYPE_OBJECT:
      pb_value->has_object_value = true;

      if (value->d.object_val)
        {
          uint64_t id = serializer_lookup_object_id (serializer, value->d.object_val);

          g_warn_if_fail (id != 0);

          pb_value->object_value = id;
        }
      else
        pb_value->object_value = 0;

      break;

    case RUT_PROPERTY_TYPE_POINTER:
      g_warn_if_reached ();
      break;
    }

  return pb_value;
}

Rig__PropertyType
rut_property_type_to_pb_type (RutPropertyType type)
{
  switch (type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      return RIG__PROPERTY_TYPE__FLOAT;
    case RUT_PROPERTY_TYPE_DOUBLE:
      return RIG__PROPERTY_TYPE__DOUBLE;
    case RUT_PROPERTY_TYPE_INTEGER:
      return RIG__PROPERTY_TYPE__INTEGER;
    case RUT_PROPERTY_TYPE_ENUM:
      return RIG__PROPERTY_TYPE__ENUM;
    case RUT_PROPERTY_TYPE_UINT32:
      return RIG__PROPERTY_TYPE__UINT32;
    case RUT_PROPERTY_TYPE_BOOLEAN:
      return RIG__PROPERTY_TYPE__BOOLEAN;
    case RUT_PROPERTY_TYPE_TEXT:
      return RIG__PROPERTY_TYPE__TEXT;
    case RUT_PROPERTY_TYPE_QUATERNION:
      return RIG__PROPERTY_TYPE__QUATERNION;
    case RUT_PROPERTY_TYPE_VEC3:
      return RIG__PROPERTY_TYPE__VEC3;
    case RUT_PROPERTY_TYPE_VEC4:
      return RIG__PROPERTY_TYPE__VEC4;
    case RUT_PROPERTY_TYPE_COLOR:
      return RIG__PROPERTY_TYPE__COLOR;
    case RUT_PROPERTY_TYPE_OBJECT:
      return RIG__PROPERTY_TYPE__OBJECT;
    case RUT_PROPERTY_TYPE_ASSET:
      return RIG__PROPERTY_TYPE__ASSET;

    case RUT_PROPERTY_TYPE_POINTER:
      g_warn_if_reached ();
      return RIG__PROPERTY_TYPE__OBJECT;
    }

  g_warn_if_reached ();
  return 0;
}

Rig__Boxed *
pb_boxed_new (RigPBSerializer *serializer,
              const char *name,
              const RutBoxed *boxed)
{
  RigEngine *engine = serializer->engine;
  Rig__Boxed *pb_boxed =
    pb_new (engine, sizeof (Rig__Boxed), rig__boxed__init);

  pb_boxed->name = (char *)name;
  pb_boxed->has_type = TRUE;
  pb_boxed->type = rut_property_type_to_pb_type (boxed->type);
  pb_boxed->value = pb_property_value_new (serializer, boxed);

  return pb_boxed;
}

static uint64_t
register_serializer_object (RigPBSerializer *serializer,
                            void *object)
{
  uint64_t id;
  uint64_t *id_value;

  if (g_hash_table_lookup (serializer->id_map, object))
    {
      g_critical ("Duplicate save object id %d", (int)id);
      return 0;
    }

  id = serializer->next_id++;
  g_return_if_fail (id != 0);

  id_value = g_slice_new (uint64_t);

  *id_value = id;

  g_hash_table_insert (serializer->id_map, object, id_value);

  return id;
}

static void
count_instrospectables_cb (RutProperty *property,
                           void *user_data)
{
  RigPBSerializer *serializer = user_data;
  serializer->n_properties++;
}

static void
serialize_instrospectables_cb (RutProperty *property,
                               void *user_data)
{
  RigPBSerializer *serializer = user_data;
  void **properties_out = serializer->properties_out;
  RutBoxed boxed;

  rut_property_box (property, &boxed);

  properties_out[serializer->n_properties++] =
    pb_boxed_new (serializer,
                  property->spec->name,
                  &boxed);

  rut_boxed_destroy (&boxed);
}

static void
serialize_instrospectable_properties (RutObject *object,
                                      size_t *n_properties_out,
                                      void **properties_out,
                                      RigPBSerializer *serializer)
{
  RigEngine *engine = serializer->engine;

  serializer->n_properties = 0;
  rut_introspectable_foreach_property (object,
                                       count_instrospectables_cb,
                                       serializer);
  *n_properties_out = serializer->n_properties;

  serializer->properties_out = *properties_out =
    rut_memory_stack_alloc (engine->serialization_stack,
                            sizeof (void *) * serializer->n_properties);

  serializer->n_properties = 0;
  rut_introspectable_foreach_property (object,
                                       serialize_instrospectables_cb,
                                       serializer);
}

static void
serialize_component_cb (RutComponent *component,
                        void *user_data)
{
  const RutType *type = rut_object_get_type (component);
  RigPBSerializer *serializer = user_data;
  RigEngine *engine = serializer->engine;
  int component_id;
  Rig__Entity__Component *pb_component;

  pb_component =
    rut_memory_stack_alloc (engine->serialization_stack,
                            sizeof (Rig__Entity__Component));
  rig__entity__component__init (pb_component);

  serializer->n_pb_components++;
  serializer->pb_components = g_list_prepend (serializer->pb_components, pb_component);

  component_id = register_serializer_object (serializer, component);

  pb_component->has_id = TRUE;
  pb_component->id = component_id;

  pb_component->has_type = TRUE;

  if (type == &rut_light_type)
    {
      RutLight *light = RUT_LIGHT (component);
      const CoglColor *ambient = rut_light_get_ambient (light);
      const CoglColor *diffuse = rut_light_get_diffuse (light);
      const CoglColor *specular = rut_light_get_specular (light);
      Rig__Entity__Component__Light *pb_light;

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__LIGHT;
      pb_light = pb_new (engine,
                         sizeof (Rig__Entity__Component__Light),
                         rig__entity__component__light__init);
      pb_component->light = pb_light;

      pb_light->ambient = pb_color_new (engine, ambient);
      pb_light->diffuse = pb_color_new (engine, diffuse);
      pb_light->specular = pb_color_new (engine, specular);
    }
  else if (type == &rut_material_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__MATERIAL;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rut_shape_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__SHAPE;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rut_diamond_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__DIAMOND;
      pb_component->diamond = pb_new (engine,
                                      sizeof (Rig__Entity__Component__Diamond),
                                      rig__entity__component__diamond__init);
      pb_component->diamond->has_size = TRUE;
      pb_component->diamond->size = rut_diamond_get_size (RUT_DIAMOND (component));
    }
  else if (type == &rut_pointalism_grid_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__POINTALISM_GRID;
      pb_component->grid = pb_new (engine,
                                   sizeof (Rig__Entity__Component__PointalismGrid),
                                   rig__entity__component__pointalism_grid__init);

      pb_component->grid->has_scale = TRUE;
      pb_component->grid->scale = rut_pointalism_grid_get_scale (RUT_POINTALISM_GRID (component));

      pb_component->grid->has_z = TRUE;
      pb_component->grid->z = rut_pointalism_grid_get_z (RUT_POINTALISM_GRID (component));

      pb_component->grid->has_cell_size = TRUE;
      pb_component->grid->cell_size = rut_pointalism_grid_get_cell_size (RUT_POINTALISM_GRID (component));

      pb_component->grid->has_lighter = TRUE;
      pb_component->grid->lighter = rut_pointalism_grid_get_lighter (RUT_POINTALISM_GRID (component));
    }
  else if (type == &rut_model_type)
    {
      RutModel *model = RUT_MODEL (component);
      uint64_t asset_id = serializer_lookup_object_id (serializer,
                                                       rut_model_get_asset (model));

      /* XXX: we don't support serializing a model loaded from a RutMesh */
      g_warn_if_fail (asset_id != 0);

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__MODEL;
      pb_component->model = pb_new (engine,
                                    sizeof (Rig__Entity__Component__Model),
                                    rig__entity__component__model__init);

      if (asset_id)
        {
          pb_component->model->has_asset_id = TRUE;
          pb_component->model->asset_id = asset_id;
        }
    }
  else if (type == &rut_text_type)
    {
      RutText *text = RUT_TEXT (component);
      const CoglColor *color;
      Rig__Entity__Component__Text *pb_text;

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__TEXT;
      pb_text = pb_new (engine,
                        sizeof (Rig__Entity__Component__Text),
                        rig__entity__component__text__init);
      pb_component->text = pb_text;

      color = rut_text_get_color (text);

      pb_text->text = (char *)rut_text_get_text (text);
      pb_text->font = (char *)rut_text_get_font_name (text);
      pb_text->color = pb_color_new (engine, color);
    }
  else if (type == &rut_camera_type)
    {
      RutCamera *camera = (RutCamera *)component;
      Rig__Entity__Component__Camera *pb_camera;

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__CAMERA;
      pb_camera = pb_new (engine,
                          sizeof (Rig__Entity__Component__Camera),
                          rig__entity__component__camera__init);
      pb_component->camera = pb_camera;

      pb_camera->has_projection_mode = TRUE;
      switch (rut_camera_get_projection_mode (camera))
        {
        case RUT_PROJECTION_ORTHOGRAPHIC:
          pb_camera->projection_mode =
            RIG__ENTITY__COMPONENT__CAMERA__PROJECTION_MODE__ORTHOGRAPHIC;

          pb_camera->ortho = pb_new (engine,
                                     sizeof (Rig__OrthoCoords),
                                     rig__ortho_coords__init);
          pb_camera->ortho->x0 = camera->x1;
          pb_camera->ortho->y0 = camera->y1;
          pb_camera->ortho->x1 = camera->x2;
          pb_camera->ortho->y1 = camera->y2;
          break;
        case RUT_PROJECTION_PERSPECTIVE:
          pb_camera->projection_mode =
            RIG__ENTITY__COMPONENT__CAMERA__PROJECTION_MODE__PERSPECTIVE;
          pb_camera->has_field_of_view = TRUE;
          pb_camera->field_of_view = camera->fov;
          break;
        }

      pb_camera->viewport = pb_new (engine,
                                    sizeof (Rig__Viewport),
                                    rig__viewport__init);

      pb_camera->viewport->x = camera->viewport[0];
      pb_camera->viewport->y = camera->viewport[1];
      pb_camera->viewport->width = camera->viewport[2];
      pb_camera->viewport->height = camera->viewport[3];

      if (camera->zoom != 1)
        {
          pb_camera->has_zoom = TRUE;
          pb_camera->zoom = camera->zoom;
        }

      pb_camera->has_focal_distance = TRUE;
      pb_camera->focal_distance = camera->focal_distance;

      pb_camera->has_depth_of_field = TRUE;
      pb_camera->depth_of_field = camera->depth_of_field;

      pb_camera->has_near_plane = TRUE;
      pb_camera->near_plane = camera->near;
      pb_camera->has_far_plane = TRUE;
      pb_camera->far_plane = camera->far;

      pb_camera->background = pb_color_new (engine, &camera->bg_color);
    }
  else if (type == &rut_nine_slice_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__NINE_SLICE;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rut_hair_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__HAIR;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }

}

static RutTraverseVisitFlags
_rut_entitygraph_pre_serialize_cb (RutObject *object,
                                   int depth,
                                   void *user_data)
{
  RigPBSerializer *serializer = user_data;
  RigEngine *engine = serializer->engine;
  const RutType *type = rut_object_get_type (object);
  RutObject *parent = rut_graphable_get_parent (object);
  RutEntity *entity;
  const CoglQuaternion *q;
  const char *label;
  Rig__Entity *pb_entity;
  Rig__Vec3 *position;
  float scale;
  GList *l;
  int i;

  if (type != &rut_entity_type)
    {
      g_warning ("Can't save non-entity graphables\n");
      return RUT_TRAVERSE_VISIT_CONTINUE;
    }

  entity = object;

  /* NB: labels with a "rig:" prefix imply that this is an internal
   * entity that shouldn't be saved (such as the editing camera
   * entities) */
  label = rut_entity_get_label (entity);
  if (label && strncmp ("rig:", label, 4) == 0)
    return RUT_TRAVERSE_VISIT_CONTINUE;


  pb_entity = rut_memory_stack_alloc (engine->serialization_stack,
                                      sizeof (Rig__Entity));
  rig__entity__init (pb_entity);

  serializer->n_pb_entities++;
  serializer->pb_entities = g_list_prepend (serializer->pb_entities, pb_entity);

  pb_entity->has_id = TRUE;
  pb_entity->id = register_serializer_object (serializer, entity);

  if (parent && rut_object_get_type (parent) == &rut_entity_type)
    {
      uint64_t id = serializer_lookup_object_id (serializer, parent);
      if (id)
        {
          pb_entity->has_parent_id = TRUE;
          pb_entity->parent_id = id;
        }
      else
        g_warning ("Failed to find id of parent entity\n");
    }

  if (label && *label)
    {
      pb_entity->label = (char *)label;
    }

  q = rut_entity_get_rotation (entity);

  position = rut_memory_stack_alloc (engine->serialization_stack,
                                     sizeof (Rig__Vec3));
  rig__vec3__init (position);
  position->x = rut_entity_get_x (entity);
  position->y = rut_entity_get_y (entity);
  position->z = rut_entity_get_z (entity);
  pb_entity->position = position;

  scale = rut_entity_get_scale (entity);
  if (scale != 1)
    {
      pb_entity->has_scale = TRUE;
      pb_entity->scale = scale;
    }

  pb_entity->rotation = pb_rotation_new (engine, q);

  serializer->n_pb_components = 0;
  serializer->pb_components = NULL;
  rut_entity_foreach_component (entity,
                                serialize_component_cb,
                                serializer);

  pb_entity->n_components = serializer->n_pb_components;
  pb_entity->components =
    rut_memory_stack_alloc (engine->serialization_stack,
                            sizeof (void *) * pb_entity->n_components);

  for (i = 0, l = serializer->pb_components; l; i++, l = l->next)
    pb_entity->components[i] = l->data;
  g_list_free (serializer->pb_components);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static void
serialize_property_cb (RigControllerPropData *prop_data,
                       void *user_data)
{
  RigPBSerializer *serializer = user_data;
  RigEngine *engine = serializer->engine;
  RutObject *object;
  uint64_t id;

  Rig__Controller__Property *pb_property =
    pb_new (engine,
            sizeof (Rig__Controller__Property),
            rig__controller__property__init);

  serializer->n_pb_properties++;
  serializer->pb_properties = g_list_prepend (serializer->pb_properties, pb_property);

  object = prop_data->property->object;

  id = serializer_lookup_object_id (serializer, object);
  if (!id)
    g_warning ("Failed to find id of object\n");


  pb_property->has_object_id = TRUE;
  pb_property->object_id = id;

  pb_property->name = (char *)prop_data->property->spec->name;

  pb_property->has_method = TRUE;
  switch (prop_data->method)
    {
      case RIG_CONTROLLER_METHOD_CONSTANT:
        pb_property->method = RIG__CONTROLLER__PROPERTY__METHOD__CONSTANT;
        break;
      case RIG_CONTROLLER_METHOD_PATH:
        pb_property->method = RIG__CONTROLLER__PROPERTY__METHOD__PATH;
        break;
      case RIG_CONTROLLER_METHOD_BINDING:
        pb_property->method = RIG__CONTROLLER__PROPERTY__METHOD__C_BINDING;
        break;
    }

  pb_property->constant = pb_property_value_new (serializer, &prop_data->constant_value);

  if (prop_data->path && prop_data->path->length)
    pb_property->path = pb_path_new (engine, prop_data->path);
}

static void
free_id_slice (void *id)
{
  g_slice_free (uint64_t, id);
}

RigPBSerializer *
rig_pb_serializer_new (RigEngine *engine)
{
  RigPBSerializer *serializer = g_slice_new0 (RigPBSerializer);

  serializer->engine = engine;

  /* This hash table maps object pointers to uint64_t ids while saving */
  serializer->id_map = g_hash_table_new_full (NULL, /* direct hash */
                                              NULL, /* direct key equal */
                                              NULL,
                                              free_id_slice);

  /* NB: We have to reserve 0 here so we can tell if lookups into the
   * id_map fail. */
  serializer->next_id = 1;

  rut_memory_stack_rewind (engine->serialization_stack);

  return serializer;
}

void
rig_pb_serializer_set_asset_filter (RigPBSerializer *serializer,
                                    RigPBAssetFilter filter,
                                    void *user_data)
{
  serializer->asset_filter = filter;
  serializer->asset_filter_data = user_data;
}

void
rig_pb_serializer_destroy (RigPBSerializer *serializer)
{
  if (serializer->required_assets)
    g_list_free (serializer->required_assets);

  g_hash_table_destroy (serializer->id_map);

  g_slice_free (RigPBSerializer, serializer);
}

static Rig__Buffer *
serialize_buffer (RigPBSerializer *serializer, RutBuffer *buffer)
{
  Rig__Buffer *pb_buffer =
    pb_new (serializer->engine, sizeof (Rig__Buffer), rig__buffer__init);

  pb_buffer->has_id = true;
  pb_buffer->id =
    register_serializer_object (serializer, buffer);

  /* NB: The serialized asset points directly to the RutMesh
   * data to avoid copying it... */
  pb_buffer->has_data = true;
  pb_buffer->data.data = buffer->data;
  pb_buffer->data.len = buffer->size;

  return pb_buffer;
}

static Rig__Asset *
serialize_mesh_asset (RigPBSerializer *serializer, RutAsset *asset)
{
  RigEngine *engine = serializer->engine;
  RutMesh *mesh = rut_asset_get_mesh (asset);
  Rig__Asset *pb_asset;
  RutBuffer **buffers;
  int n_buffers = 0;
  Rig__Buffer **pb_buffers;
  Rig__Buffer **attribute_buffers_map;
  Rig__Attribute **attributes;
  Rig__Mesh *pb_mesh;
  int i;

  pb_asset = pb_new (engine, sizeof (Rig__Asset), rig__asset__init);

  pb_asset->path = (char *)rut_asset_get_path (asset);

  pb_asset->has_type = true;
  pb_asset->type = RUT_ASSET_TYPE_PLY_MODEL;

  /* The maximum number of pb_buffers we may need = n_attributes plus 1 in case
   * there is an index buffer... */
  pb_buffers = rut_memory_stack_alloc (engine->serialization_stack,
                                       sizeof (void *) * (mesh->n_attributes + 1));

  buffers = g_alloca (sizeof (void *) * mesh->n_attributes);
  attribute_buffers_map = g_alloca (sizeof (void *) * mesh->n_attributes);

  /* NB:
   * attributes may refer to shared buffers so we need to first
   * figure out how many unique buffers the mesh refers too...
   */

  for (i = 0; i < mesh->n_attributes; i++)
    {
      int j;

      for (j = 0; i < n_buffers; j++)
        if (buffers[j] == mesh->attributes[i]->buffer)
          break;

      if (j < n_buffers)
        attribute_buffers_map[i] = pb_buffers[j];
      else
        {
          Rig__Buffer *pb_buffer =
            serialize_buffer (serializer, mesh->attributes[i]->buffer);

          pb_buffers[n_buffers] = pb_buffer;

          attribute_buffers_map[i] = pb_buffer;
          buffers[n_buffers++] = mesh->attributes[i]->buffer;
        }
    }

  if (mesh->indices_buffer)
    {
      Rig__Buffer *pb_buffer =
        serialize_buffer (serializer, mesh->indices_buffer);
      pb_buffers[n_buffers++] = pb_buffer;
    }

  attributes= rut_memory_stack_alloc (engine->serialization_stack,
                                      sizeof (void *) * mesh->n_attributes);
  for (i = 0; i < mesh->n_attributes; i++)
    {
      Rig__Attribute *pb_attribute =
        pb_new (engine, sizeof (Rig__Attribute), rig__attribute__init);
      Rig__Attribute__Type type;

      pb_attribute->has_buffer_id = true;
      pb_attribute->buffer_id = attribute_buffers_map[i]->id;

      pb_attribute->name = (char *)mesh->attributes[i]->name;

      pb_attribute->has_stride = true;
      pb_attribute->stride = mesh->attributes[i]->stride;
      pb_attribute->has_offset = true;
      pb_attribute->offset = mesh->attributes[i]->offset;
      pb_attribute->has_n_components = true;
      pb_attribute->n_components = mesh->attributes[i]->n_components;
      pb_attribute->has_type = true;

      switch (mesh->attributes[i]->type)
        {
        case RUT_ATTRIBUTE_TYPE_BYTE:
          type = RIG__ATTRIBUTE__TYPE__BYTE;
          break;
        case RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
          type = RIG__ATTRIBUTE__TYPE__UNSIGNED_BYTE;
          break;
        case RUT_ATTRIBUTE_TYPE_SHORT:
          type = RIG__ATTRIBUTE__TYPE__SHORT;
          break;
        case RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
          type = RIG__ATTRIBUTE__TYPE__UNSIGNED_SHORT;
          break;
        case RUT_ATTRIBUTE_TYPE_FLOAT:
          type = RIG__ATTRIBUTE__TYPE__FLOAT;
          break;
        }
      pb_attribute->type = type;

      attributes[i] = pb_attribute;
    }

  pb_asset->mesh = pb_new (engine, sizeof (Rig__Mesh), rig__mesh__init);
  pb_mesh = pb_asset->mesh;

  pb_mesh->has_mode = true;
  switch (mesh->mode)
    {
    case COGL_VERTICES_MODE_POINTS:
      pb_mesh->mode = RIG__MESH__MODE__POINTS;
      break;
    case COGL_VERTICES_MODE_LINES:
      pb_mesh->mode = RIG__MESH__MODE__LINES;
      break;
    case COGL_VERTICES_MODE_LINE_LOOP:
      pb_mesh->mode = RIG__MESH__MODE__LINE_LOOP;
      break;
    case COGL_VERTICES_MODE_LINE_STRIP:
      pb_mesh->mode = RIG__MESH__MODE__LINE_STRIP;
      break;
    case COGL_VERTICES_MODE_TRIANGLES:
      pb_mesh->mode = RIG__MESH__MODE__TRIANGLES;
      break;
    case COGL_VERTICES_MODE_TRIANGLE_STRIP:
      pb_mesh->mode = RIG__MESH__MODE__TRIANGLE_STRIP;
      break;
    case COGL_VERTICES_MODE_TRIANGLE_FAN:
      pb_mesh->mode = RIG__MESH__MODE__TRIANGLE_FAN;
      break;
    }

  pb_mesh->n_buffers = n_buffers;
  pb_mesh->buffers = pb_buffers;

  pb_mesh->attributes = attributes;
  pb_mesh->n_attributes = mesh->n_attributes;

  pb_mesh->has_n_vertices = true;
  pb_mesh->n_vertices = mesh->n_vertices;

  if (mesh->indices_buffer)
    {
      pb_mesh->has_indices_type = true;
      switch (mesh->indices_type)
        {
        case COGL_INDICES_TYPE_UNSIGNED_BYTE:
          pb_mesh->indices_type = RIG__MESH__INDICES_TYPE__UNSIGNED_BYTE;
          break;
        case COGL_INDICES_TYPE_UNSIGNED_SHORT:
          pb_mesh->indices_type = RIG__MESH__INDICES_TYPE__UNSIGNED_SHORT;
          break;
        case COGL_INDICES_TYPE_UNSIGNED_INT:
          pb_mesh->indices_type = RIG__MESH__INDICES_TYPE__UNSIGNED_INT;
          break;
        }

      pb_mesh->has_n_indices = true;
      pb_mesh->n_indices = mesh->n_indices;

      pb_mesh->has_indices_buffer_id = true;
      pb_mesh->indices_buffer_id = pb_buffers[n_buffers - 1]->id;
    }

  return pb_asset;
}

static Rig__Asset *
serialize_asset (RigPBSerializer *serializer, RutAsset *asset)
{
#ifdef __ANDROID__
  g_warn_if_reached ();
  return NULL;
#else
  RigEngine *engine = serializer->engine;
  RutContext *ctx = rut_asset_get_context (asset);
  const char *path = rut_asset_get_path (asset);
  char *full_path;
  Rig__Asset *pb_asset;
  GError *error = NULL;
  char *contents;
  size_t len;

  /* XXX: This should be renamed to _TYPE_MESH */
  if (rut_asset_get_type (asset) == RUT_ASSET_TYPE_PLY_MODEL)
    return serialize_mesh_asset (serializer, asset);

  full_path = g_build_filename (ctx->assets_location, path, NULL);
  if (!g_file_get_contents (full_path,
                            &contents,
                            &len,
                            &error))
    {
      g_warning ("Failed to read contents of asset: %s", error->message);
      g_error_free (error);
      g_free (full_path);
      return NULL;
    }

  g_free (full_path);

  pb_asset = pb_new (engine, sizeof (Rig__Asset), rig__asset__init);

  pb_asset->path = (char *)path;

  pb_asset->has_type = TRUE;
  pb_asset->type = rut_asset_get_type (asset);

  pb_asset->has_is_video = true;
  pb_asset->is_video = rut_asset_get_is_video (asset);

  pb_asset->has_data = true;
  pb_asset->data.data = (uint8_t *)contents;
  pb_asset->data.len = len;

  return pb_asset;
#endif
}

static void
serialized_asset_destroy (Rig__Asset *serialized_asset)
{
  if (serialized_asset->has_data)
    g_free (serialized_asset->data.data);
}

Rig__UI *
rig_pb_serialize_ui (RigPBSerializer *serializer)
{
  RigEngine *engine = serializer->engine;
  GList *l;
  int i;
  Rig__UI *ui;
  Rig__Device *device;

  ui = pb_new (engine, sizeof (Rig__UI), rig__ui__init);
  device = pb_new (engine, sizeof (Rig__Device), rig__device__init);

  ui->device = device;

  device->has_width = TRUE;
  device->width = engine->device_width;
  device->has_height = TRUE;
  device->height = engine->device_height;
  device->background = pb_color_new (engine, &engine->background_color);

  /* Register all assets up front, but we only actually serialize those
   * assets that are referenced - indicated by a corresponding id lookup
   * in serializer_lookup_object_id()
   */
  for (l = engine->assets; l; l = l->next)
    register_serializer_object (serializer, l->data);

  serializer->n_pb_entities = 0;
  rut_graphable_traverse (engine->scene,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          _rut_entitygraph_pre_serialize_cb,
                          NULL,
                          serializer);

  ui->n_entities = serializer->n_pb_entities;
  ui->entities = rut_memory_stack_alloc (engine->serialization_stack,
                                         sizeof (void *) * ui->n_entities);
  for (i = 0, l = serializer->pb_entities; l; i++, l = l->next)
    ui->entities[i] = l->data;
  g_list_free (serializer->pb_entities);

  for (i = 0, l = engine->controllers; l; i++, l = l->next)
    register_serializer_object (serializer, l->data);

  ui->n_controllers = g_list_length (engine->controllers);
  if (ui->n_controllers)
    {
      ui->controllers =
        rut_memory_stack_alloc (engine->serialization_stack,
                                sizeof (void *) * ui->n_controllers);

      for (i = 0, l = engine->controllers; l; i++, l = l->next)
        {
          RigController *controller = l->data;
          Rig__Controller *pb_controller =
            pb_new (engine, sizeof (Rig__Controller), rig__controller__init);
          GList *l2;
          int j;

          ui->controllers[i] = pb_controller;

          pb_controller->has_id = TRUE;
          pb_controller->id = serializer_lookup_object_id (serializer, controller);

          pb_controller->name = controller->label;

          serialize_instrospectable_properties (controller,
                                                &pb_controller->n_controller_properties,
                                                (void **)&pb_controller->controller_properties,
                                                serializer);

          serializer->n_pb_properties = 0;
          serializer->pb_properties = NULL;
          rig_controller_foreach_property (controller,
                                           serialize_property_cb,
                                           serializer);

          pb_controller->n_properties = serializer->n_pb_properties;
          pb_controller->properties =
            rut_memory_stack_alloc (engine->serialization_stack,
                                    sizeof (void *) * pb_controller->n_properties);
          for (j = 0, l2 = serializer->pb_properties; l2; j++, l2 = l2->next)
            pb_controller->properties[j] = l2->data;
          g_list_free (serializer->pb_properties);
        }
    }

  ui->n_assets = g_list_length (serializer->required_assets);
  if (ui->n_assets)
    {
      RigPBAssetFilter save_filter = serializer->asset_filter;
      int i;

      /* Temporarily disable the asset filter that is called in
       * serializer_lookup_object_id() since we have already filtered
       * all of the assets required and we now only need to lookup
       * the ids for serializing the assets themselves. */
      serializer->asset_filter = NULL;

      ui->assets = rut_memory_stack_alloc (engine->serialization_stack,
                                           ui->n_assets * sizeof (void *));
      for (i = 0, l = serializer->required_assets; l; i++, l = l->next)
        {
          RutAsset *asset = l->data;
          Rig__Asset *pb_asset = serialize_asset (serializer, asset);

          pb_asset->has_id = true;
          pb_asset->id = serializer_lookup_object_id (serializer, asset);

          ui->assets[i] = pb_asset;
        }

      /* restore the asset filter */
      serializer->asset_filter = save_filter;
    }

  return ui;
}

void
rig_pb_serialized_ui_destroy (Rig__UI *ui)
{
  int i;

  for (i = 0; i < ui->n_assets; i++)
    serialized_asset_destroy (ui->assets[i]);
}

Rig__Event **
rig_pb_serialize_input_events (RigEngine *engine,
                               RutList *input_queue,
                               int n_events)
{
  RutInputEvent *event, *tmp;
  Rig__Event **pb_events;
  int i;

#warning "would it be better to assume the caller is responsible for clearing the serialization stack?"
  rut_memory_stack_rewind (engine->serialization_stack);
  pb_events = rut_memory_stack_alloc (engine->serialization_stack,
                                      n_events * sizeof (void *));

  i = 0;
  rut_list_for_each_safe (event, tmp, input_queue, list_node)
    {
      Rig__Event *pb_event = pb_new (engine, sizeof (Rig__Event),
                                     rig__event__init);

      pb_event->has_type = true;

      switch (event->type)
        {
        case RUT_INPUT_EVENT_TYPE_MOTION:
          {
            RutMotionEventAction action = rut_motion_event_get_action (event);

            switch (action)
              {
              case RUT_MOTION_EVENT_ACTION_MOVE:
                g_print ("Serialize move\n");
                pb_event->type = RIG__EVENT__TYPE__POINTER_MOVE;
                pb_event->pointer_move =
                  pb_new (engine, sizeof (Rig__Event__PointerMove),
                          rig__event__pointer_move__init);

                pb_event->pointer_move->has_x = true;
                pb_event->pointer_move->x = rut_motion_event_get_x (event);
                pb_event->pointer_move->has_y = true;
                pb_event->pointer_move->y = rut_motion_event_get_y (event);
                break;
              case RUT_MOTION_EVENT_ACTION_DOWN:
                g_print ("Serialize pointer down\n");
                pb_event->type = RIG__EVENT__TYPE__POINTER_DOWN;
                break;
              case RUT_MOTION_EVENT_ACTION_UP:
                g_print ("Serialize pointer up\n");
                pb_event->type = RIG__EVENT__TYPE__POINTER_UP;
                break;
              }

            switch (action)
              {
              case RUT_MOTION_EVENT_ACTION_MOVE:
                break;
              case RUT_MOTION_EVENT_ACTION_UP:
              case RUT_MOTION_EVENT_ACTION_DOWN:
                pb_event->pointer_button =
                  pb_new (engine, sizeof (Rig__Event__PointerButton),
                          rig__event__pointer_button__init);
                pb_event->pointer_button->has_button = true;
                pb_event->pointer_button->button =
                  rut_motion_event_get_button (event);
              }

          break;
          }
        case RUT_INPUT_EVENT_TYPE_KEY:
          {
            RutKeyEventAction action = rut_key_event_get_action (event);

            switch (action)
              {
              case RUT_KEY_EVENT_ACTION_DOWN:
                g_print ("Serialize key down\n");
                pb_event->type = RIG__EVENT__TYPE__KEY_DOWN;
                break;
              case RUT_KEY_EVENT_ACTION_UP:
                g_print ("Serialize key down\n");
                pb_event->type = RIG__EVENT__TYPE__KEY_UP;
                break;
              }

            pb_event->key =
              pb_new (engine, sizeof (Rig__Event__Key),
                      rig__event__key__init);
            pb_event->key->has_keysym = true;
            pb_event->key->keysym =
              rut_key_event_get_keysym (event);
            pb_event->key->has_mod_state = true;
            pb_event->key->mod_state =
              rut_key_event_get_modifier_state (event);
          }

        case RUT_INPUT_EVENT_TYPE_TEXT:
        case RUT_INPUT_EVENT_TYPE_DROP_OFFER:
        case RUT_INPUT_EVENT_TYPE_DROP_CANCEL:
        case RUT_INPUT_EVENT_TYPE_DROP:
          break;
        }

      pb_events[i] = pb_event;

      i++;
    }

  return pb_events;
}

struct _RigPBUnSerializer
{
  RigEngine *engine;

  GList *assets;
  GList *entities;
  RutEntity *light;
  GList *controllers;

  GHashTable *id_map;
};

static void
pb_init_color (RutContext *ctx,
               CoglColor *color,
               Rig__Color *pb_color)
{
  if (pb_color && pb_color->hex)
    rut_color_init_from_string (ctx, color, pb_color->hex);
  else
    cogl_color_init_from_4f (color, 0, 0, 0, 1);
}

static void
pb_init_quaternion (CoglQuaternion *quaternion,
                    Rig__Rotation *pb_rotation)
{
  if (pb_rotation)
    {
      cogl_quaternion_init (quaternion,
                            pb_rotation->angle,
                            pb_rotation->x,
                            pb_rotation->y,
                            pb_rotation->z);
    }
  else
    cogl_quaternion_init (quaternion, 0, 1, 0, 0);
}

static void
pb_init_boxed_vec3 (RutBoxed *boxed,
                    Rig__Vec3 *pb_vec3)
{
  boxed->type = RUT_PROPERTY_TYPE_VEC3;
  if (pb_vec3)
    {
      boxed->d.vec3_val[0] = pb_vec3->x;
      boxed->d.vec3_val[1] = pb_vec3->y;
      boxed->d.vec3_val[2] = pb_vec3->z;
    }
  else
    {
      boxed->d.vec3_val[0] = 0;
      boxed->d.vec3_val[1] = 0;
      boxed->d.vec3_val[2] = 0;
    }
}

static void
pb_init_boxed_vec4 (RutBoxed *boxed,
                    Rig__Vec4 *pb_vec4)
{
  boxed->type = RUT_PROPERTY_TYPE_VEC4;
  if (pb_vec4)
    {
      boxed->d.vec4_val[0] = pb_vec4->x;
      boxed->d.vec4_val[1] = pb_vec4->y;
      boxed->d.vec4_val[2] = pb_vec4->z;
      boxed->d.vec4_val[3] = pb_vec4->w;
    }
  else
    {
      boxed->d.vec4_val[0] = 0;
      boxed->d.vec4_val[1] = 0;
      boxed->d.vec4_val[2] = 0;
      boxed->d.vec4_val[3] = 0;
    }
}

static RutEntity *
unserializer_find_entity (RigPBUnSerializer *unserializer, uint64_t id)
{
  RutObject *object = g_hash_table_lookup (unserializer->id_map, &id);
  if (object == NULL || rut_object_get_type (object) != &rut_entity_type)
    return NULL;
  return RUT_ENTITY (object);
}

static RutAsset *
unserializer_find_asset (RigPBUnSerializer *unserializer, uint64_t id)
{
  RutObject *object = g_hash_table_lookup (unserializer->id_map, &id);
  if (object == NULL || rut_object_get_type (object) != &rut_asset_type)
    return NULL;
  return RUT_ASSET (object);
}

static RutObject *
unserializer_find_introspectable (RigPBUnSerializer *unserializer, uint64_t id)
{
  RutObject *object = g_hash_table_lookup (unserializer->id_map, &id);
  if (object == NULL ||
      !rut_object_is (object, RUT_INTERFACE_ID_INTROSPECTABLE) ||
      !rut_object_is (object, RUT_INTERFACE_ID_REF_COUNTABLE))
    return NULL;
  return object;
}

static void
pb_init_boxed_value (RigPBUnSerializer *unserializer,
                     RutBoxed *boxed,
                     RutPropertyType type,
                     Rig__PropertyValue *pb_value)
{
  boxed->type = type;

  switch (type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      boxed->d.float_val = pb_value->float_value;
      break;

    case RUT_PROPERTY_TYPE_DOUBLE:
      boxed->d.double_val = pb_value->double_value;
      break;

    case RUT_PROPERTY_TYPE_INTEGER:
      boxed->d.integer_val = pb_value->integer_value;
      break;

    case RUT_PROPERTY_TYPE_UINT32:
      boxed->d.uint32_val = pb_value->uint32_value;
      break;

    case RUT_PROPERTY_TYPE_BOOLEAN:
      boxed->d.boolean_val = pb_value->boolean_value;
      break;

    case RUT_PROPERTY_TYPE_TEXT:
      boxed->d.text_val = g_strdup (pb_value->text_value);
      break;

    case RUT_PROPERTY_TYPE_QUATERNION:
      pb_init_quaternion (&boxed->d.quaternion_val, pb_value->quaternion_value);
      break;

    case RUT_PROPERTY_TYPE_VEC3:
      pb_init_boxed_vec3 (boxed, pb_value->vec3_value);
      break;

    case RUT_PROPERTY_TYPE_VEC4:
      pb_init_boxed_vec4 (boxed, pb_value->vec4_value);
      break;

    case RUT_PROPERTY_TYPE_COLOR:
      pb_init_color (unserializer->engine->ctx,
                     &boxed->d.color_val, pb_value->color_value);
      break;

    case RUT_PROPERTY_TYPE_ENUM:
      /* XXX: this should possibly work in terms of string names rather than
       * the integer value? */
      boxed->d.enum_val = pb_value->enum_value;
      break;

    case RUT_PROPERTY_TYPE_ASSET:
      boxed->d.asset_val =
        unserializer_find_asset (unserializer, pb_value->asset_value);
      break;

    case RUT_PROPERTY_TYPE_OBJECT:
      boxed->d.object_val =
        unserializer_find_introspectable (unserializer, pb_value->object_value);
      break;

    case RUT_PROPERTY_TYPE_POINTER:
      g_warn_if_reached ();
      break;
    }
}

static void
collect_error (RigPBUnSerializer *unserializer,
               const char *format,
               ...)
{
  va_list ap;

  va_start (ap, format);

  /* XXX: The intention is that we shouldn't just immediately abort loading
   * like this but rather we should collect the errors and try our best to
   * continue loading. At the end we can report the errors to the user so the
   * realize that their document may be corrupt.
   */

  if (rut_util_is_boolean_env_set ("RUT_IGNORE_LOAD_ERRORS"))
    g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, format, ap);
  else
    g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, format, ap);

  va_end (ap);
}

static void
register_unserializer_object (RigPBUnSerializer *unserializer,
                              void *object,
                              uint64_t id)
{
  uint64_t *key = g_slice_new (uint64_t);

  *key = id;

  g_return_if_fail (id != 0);

  if (g_hash_table_lookup (unserializer->id_map, key))
    {
      collect_error (unserializer, "Duplicate unserializer object id %ld", id);
      return;
    }

  g_hash_table_insert (unserializer->id_map, key, object);
}

static void
set_property_from_pb_boxed (RigPBUnSerializer *unserializer,
                            RutProperty *property,
                            Rig__Boxed *pb_boxed)
{
  RutPropertyType type;
  RutBoxed boxed;

  if (!pb_boxed->value)
    {
      collect_error (unserializer, "Boxed property has no value");
      return;
    }

  if (!pb_boxed->has_type)
    {
      collect_error (unserializer, "Boxed property has no type");
      return;
    }

  switch (pb_boxed->type)
    {
    case RIG__PROPERTY_TYPE__FLOAT:
      type = RUT_PROPERTY_TYPE_FLOAT;
      break;
    case RIG__PROPERTY_TYPE__DOUBLE:
      type = RUT_PROPERTY_TYPE_DOUBLE;
      break;
    case RIG__PROPERTY_TYPE__INTEGER:
      type = RUT_PROPERTY_TYPE_INTEGER;
      break;
    case RIG__PROPERTY_TYPE__ENUM:
      type = RUT_PROPERTY_TYPE_ENUM;
      break;
    case RIG__PROPERTY_TYPE__UINT32:
      type = RUT_PROPERTY_TYPE_UINT32;
      break;
    case RIG__PROPERTY_TYPE__BOOLEAN:
      type = RUT_PROPERTY_TYPE_BOOLEAN;
      break;
    case RIG__PROPERTY_TYPE__OBJECT:
      type = RUT_PROPERTY_TYPE_OBJECT;
      break;
    case RIG__PROPERTY_TYPE__POINTER:
      type = RUT_PROPERTY_TYPE_POINTER;
      break;
    case RIG__PROPERTY_TYPE__QUATERNION:
      type = RUT_PROPERTY_TYPE_QUATERNION;
      break;
    case RIG__PROPERTY_TYPE__COLOR:
      type = RUT_PROPERTY_TYPE_COLOR;
      break;
    case RIG__PROPERTY_TYPE__VEC3:
      type = RUT_PROPERTY_TYPE_VEC3;
      break;
    case RIG__PROPERTY_TYPE__VEC4:
      type = RUT_PROPERTY_TYPE_VEC4;
      break;
    case RIG__PROPERTY_TYPE__TEXT:
      type = RUT_PROPERTY_TYPE_TEXT;
      break;
    case RIG__PROPERTY_TYPE__ASSET:
      type = RUT_PROPERTY_TYPE_ASSET;
      break;
    }

  pb_init_boxed_value (unserializer,
                       &boxed,
                       type,
                       pb_boxed->value);

  rut_property_set_boxed (&unserializer->engine->ctx->property_ctx,
                          property, &boxed);
}

static void
set_properties_from_pb_boxed_values (RigPBUnSerializer *unserializer,
                                     RutObject *object,
                                     size_t n_properties,
                                     Rig__Boxed **properties)
{
  int i;

  for (i = 0; i < n_properties; i++)
    {
      Rig__Boxed *pb_boxed = properties[i];
      RutProperty *property =
        rut_introspectable_lookup_property (object, pb_boxed->name);

      if (!property)
        {
          collect_error (unserializer,
                         "Unknown property %s for object of type %s",
                         pb_boxed->name,
                         rut_object_get_type_name (object));
          continue;
        }

      set_property_from_pb_boxed (unserializer, property, pb_boxed);
    }
}

static void
unserialize_components (RigPBUnSerializer *unserializer,
                        RutEntity *entity,
                        Rig__Entity *pb_entity,
                        bool force_material)
{
  int i;
  bool have_material = false;

  /* First we add components which don't depend on any other components... */
  for (i = 0; i < pb_entity->n_components; i++)
    {
      Rig__Entity__Component *pb_component = pb_entity->components[i];
      uint64_t component_id;

      if (!pb_component->has_id)
        continue;

      component_id = pb_component->id;

      if (!pb_component->has_type)
        continue;

      switch (pb_component->type)
        {
        case RIG__ENTITY__COMPONENT__TYPE__LIGHT:
          {
            Rig__Entity__Component__Light *pb_light = pb_component->light;
            RutLight *light;
            CoglColor ambient;
            CoglColor diffuse;
            CoglColor specular;

            light = rut_light_new (unserializer->engine->ctx);

            pb_init_color (unserializer->engine->ctx,
                           &ambient, pb_light->ambient);
            pb_init_color (unserializer->engine->ctx,
                           &diffuse, pb_light->diffuse);
            pb_init_color (unserializer->engine->ctx,
                           &specular, pb_light->specular);

            rut_light_set_ambient (light, &ambient);
            rut_light_set_diffuse (light, &diffuse);
            rut_light_set_specular (light, &specular);

            rut_entity_add_component (entity, light);
            rut_refable_unref (light);

            if (unserializer->light == NULL)
              unserializer->light = rut_refable_ref (entity);

            register_unserializer_object (unserializer, light, component_id);
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__MATERIAL:
          {
            Rig__Entity__Component__Material *pb_material =
              pb_component->material;
            RutMaterial *material;
            CoglColor ambient;
            CoglColor diffuse;
            CoglColor specular;
            RutAsset *asset;

            material = rut_material_new (unserializer->engine->ctx, NULL);

            rut_entity_add_component (entity, material);
            rut_refable_unref (material);

#warning "todo: remove Component->Material compatibility"
            if (pb_material)
              {
                if (pb_material->texture &&
                    pb_material->texture->has_asset_id)
                  {
                    Rig__Texture *pb_texture = pb_material->texture;

                    asset = unserializer_find_asset (unserializer,
                                                     pb_texture->asset_id);
                    if (asset)
                      rut_material_set_color_source_asset (material, asset);
                    else
                      collect_error (unserializer, "Invalid asset id");
                  }

                if (pb_material->normal_map &&
                    pb_material->normal_map->has_asset_id)
                  {
                    Rig__NormalMap *pb_normal_map = pb_material->normal_map;

                    asset = unserializer_find_asset (unserializer,
                                                     pb_normal_map->asset_id);
                    if (asset)
                      rut_material_set_normal_map_asset (material, asset);
                    else
                      collect_error (unserializer, "Invalid asset id");
                  }

                if (pb_material->alpha_mask &&
                    pb_material->alpha_mask->has_asset_id)
                  {
                    Rig__AlphaMask *pb_alpha_mask = pb_material->alpha_mask;

                    asset = unserializer_find_asset (unserializer,
                                                     pb_alpha_mask->asset_id);
                    if (asset)
                      rut_material_set_alpha_mask_asset (material, asset);
                    else
                      collect_error (unserializer, "Invalid asset id");
                  }

                pb_init_color (unserializer->engine->ctx,
                               &ambient, pb_material->ambient);
                pb_init_color (unserializer->engine->ctx,
                               &diffuse, pb_material->diffuse);
                pb_init_color (unserializer->engine->ctx,
                               &specular, pb_material->specular);

                rut_material_set_ambient (material, &ambient);
                rut_material_set_diffuse (material, &diffuse);
                rut_material_set_specular (material, &specular);
                if (pb_material->has_shininess)
                  rut_material_set_shininess (material, pb_material->shininess);
              }

            set_properties_from_pb_boxed_values (unserializer,
                                                 material,
                                                 pb_component->n_properties,
                                                 pb_component->properties);

            have_material = true;

            register_unserializer_object (unserializer, material, component_id);
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__MODEL:
          {
            Rig__Entity__Component__Model *pb_model = pb_component->model;
            RutAsset *asset;
            RutModel *model;

            if (!pb_model->has_asset_id)
              break;

            asset = unserializer_find_asset (unserializer, pb_model->asset_id);
            if (!asset)
              {
                collect_error (unserializer, "Invalid asset id");
                break;
              }

            model = rut_asset_get_model (asset);
            if (model)
              {
                rut_refable_unref (asset);
                rut_entity_add_component (entity, model);
                rut_refable_unref (model);
                register_unserializer_object (unserializer, model, component_id);
              }
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__TEXT:
          {
            Rig__Entity__Component__Text *pb_text = pb_component->text;
            RutText *text =
              rut_text_new_with_text (unserializer->engine->ctx,
                                      pb_text->font, pb_text->text);

            if (pb_text->color)
              {
                CoglColor color;
                pb_init_color (unserializer->engine->ctx, &color, pb_text->color);
                rut_text_set_color (text, &color);
              }

            rut_entity_add_component (entity, text);
            rut_refable_unref (text);

            register_unserializer_object (unserializer, text, component_id);
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__CAMERA:
          {
            Rig__Entity__Component__Camera *pb_camera = pb_component->camera;
            RutCamera *camera =
              rut_camera_new (unserializer->engine->ctx, NULL);

            if (pb_camera->viewport)
              {
                Rig__Viewport *pb_viewport = pb_camera->viewport;

                rut_camera_set_viewport (camera,
                                         pb_viewport->x,
                                         pb_viewport->y,
                                         pb_viewport->width,
                                         pb_viewport->height);
              }

            if (pb_camera->has_projection_mode)
              {
                switch (pb_camera->projection_mode)
                  {
                  case RIG__ENTITY__COMPONENT__CAMERA__PROJECTION_MODE__ORTHOGRAPHIC:
                    rut_camera_set_projection_mode (camera,
                                                    RUT_PROJECTION_ORTHOGRAPHIC);
                    break;
                  case RIG__ENTITY__COMPONENT__CAMERA__PROJECTION_MODE__PERSPECTIVE:
                    rut_camera_set_projection_mode (camera,
                                                    RUT_PROJECTION_PERSPECTIVE);
                    break;
                  }
              }

            if (pb_camera->ortho)
              {
                rut_camera_set_orthographic_coordinates (camera,
                                                         pb_camera->ortho->x0,
                                                         pb_camera->ortho->y0,
                                                         pb_camera->ortho->x1,
                                                         pb_camera->ortho->y1);
              }

            if (pb_camera->has_field_of_view)
              rut_camera_set_field_of_view (camera, pb_camera->field_of_view);

            if (pb_camera->zoom)
              rut_camera_set_zoom (camera, pb_camera->zoom);

            if (pb_camera->focal_distance)
              rut_camera_set_focal_distance (camera, pb_camera->focal_distance);

            if (pb_camera->depth_of_field)
              rut_camera_set_depth_of_field (camera, pb_camera->depth_of_field);

            if (pb_camera->near_plane)
              rut_camera_set_near_plane (camera, pb_camera->near_plane);

            if (pb_camera->far_plane)
              rut_camera_set_far_plane (camera, pb_camera->far_plane);

            if (pb_camera->background)
              {
                CoglColor color;
                pb_init_color (unserializer->engine->ctx,
                               &color, pb_camera->background);
                rut_camera_set_background_color (camera, &color);
              }

            rut_entity_add_component (entity, camera);
            rut_refable_unref (camera);

            register_unserializer_object (unserializer, camera, component_id);
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__SHAPE:
        case RIG__ENTITY__COMPONENT__TYPE__NINE_SLICE:
        case RIG__ENTITY__COMPONENT__TYPE__DIAMOND:
        case RIG__ENTITY__COMPONENT__TYPE__POINTALISM_GRID:
        case RIG__ENTITY__COMPONENT__TYPE__HAIR:
          break;
        }
    }

#warning "todo: remove entity:cast_shadow compatibility"
  if (force_material && !have_material)
    {
      RutMaterial *material =
        rut_material_new (unserializer->engine->ctx, NULL);

      rut_entity_add_component (entity, material);

      if (pb_entity->has_cast_shadow)
        rut_material_set_cast_shadow (material, pb_entity->cast_shadow);
    }

  /* Now we add components that may depend on a _MATERIAL or _MODEL ... */
  for (i = 0; i < pb_entity->n_components; i++)
    {
      Rig__Entity__Component *pb_component = pb_entity->components[i];
      uint64_t component_id;

      if (!pb_component->has_id)
        continue;

      component_id = pb_component->id;

      if (!pb_component->has_type)
        continue;

      switch (pb_component->type)
        {
        case RIG__ENTITY__COMPONENT__TYPE__SHAPE:
          {
            Rig__Entity__Component__Shape *pb_shape = pb_component->shape;
            RutMaterial *material;
            RutAsset *asset = NULL;
            CoglTexture *texture = NULL;
            RutShape *shape;
            CoglBool shaped = FALSE;
            int width, height;

            /* XXX: Only for compaibility... */
            if (!pb_component->n_properties)
              {
                if (pb_shape->has_shaped)
                  shaped = pb_shape->shaped;

                material = rut_entity_get_component (entity,
                                                     RUT_COMPONENT_TYPE_MATERIAL);

                /* We need to know the size of the texture before we can create
                 * a shape component */
                if (material)
                  asset = rut_material_get_color_source_asset (material);

                if (asset)
                  {
                    if (rut_asset_get_is_video (asset))
                      {
                        width = 640;
                        height = 480;
                      }
                    else
                      {
                        texture = rut_asset_get_texture (asset);
                        if (texture)
                          {
                            width = cogl_texture_get_width (texture);
                            height = cogl_texture_get_height (texture);
                          }
                        else
                          goto ERROR_SHAPE;
                      }
                  }
                else
                  goto ERROR_SHAPE;
              }


            shape = rut_shape_new (unserializer->engine->ctx,
                                   shaped,
                                   width, height);

            set_properties_from_pb_boxed_values (unserializer,
                                                 shape,
                                                 pb_component->n_properties,
                                                 pb_component->properties);

            rut_entity_add_component (entity, shape);
            rut_refable_unref (shape);

            register_unserializer_object (unserializer, shape, component_id);

            ERROR_SHAPE:
              {
                if (!shape)
                  {
                    collect_error (unserializer,
                                   "Can't add shape component without "
                                   "an image source");

                    rut_refable_unref (material);
                  }
              }
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__NINE_SLICE:
          {
            RutNineSlice *nine_slice =
              rut_nine_slice_new (unserializer->engine->ctx,
                                  NULL,
                                  0, 0, 0, 0, /* left, right, top, bottom */
                                  0, 0); /* width, height */
            set_properties_from_pb_boxed_values (unserializer,
                                                 nine_slice,
                                                 pb_component->n_properties,
                                                 pb_component->properties);

            rut_entity_add_component (entity, nine_slice);
            rut_refable_unref (nine_slice);

            register_unserializer_object (unserializer, nine_slice, component_id);

            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__DIAMOND:
          {
            Rig__Entity__Component__Diamond *pb_diamond = pb_component->diamond;
            float diamond_size = 100;
            RutMaterial *material;
            RutAsset *asset = NULL;
            CoglTexture *texture = NULL;
            RutDiamond *diamond;
            float tex_width = 200;
            float tex_height = 200;

            if (pb_diamond->has_size)
              diamond_size = pb_diamond->size;

            material = rut_entity_get_component (entity,
                                                 RUT_COMPONENT_TYPE_MATERIAL);

            /* We need to know the size of the texture before we can create
             * a diamond component */
            if (material)
              asset = rut_material_get_color_source_asset (material);

            if (asset)
              {
                if (rut_asset_get_is_video (asset))
                  {
                    tex_width = 640;
                    tex_height = 480;
                  }
                else
                  {
                    texture = rut_asset_get_texture (asset);
                    if (texture)
                      {
                        tex_width = cogl_texture_get_width (texture);
                        tex_height = cogl_texture_get_height (texture);
                      }
                  }
              }

            diamond = rut_diamond_new (unserializer->engine->ctx,
                                       diamond_size, tex_width, tex_height);

            rut_entity_add_component (entity, diamond);
            rut_refable_unref (diamond);

            register_unserializer_object (unserializer, diamond, component_id);

            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__POINTALISM_GRID:
          {
            Rig__Entity__Component__PointalismGrid *pb_grid = pb_component->grid;
            RutMaterial *material;
            RutAsset *asset = NULL;
            CoglTexture *texture = NULL;
            RutPointalismGrid *grid;
            float width, height;
            float cell_size;

            if (pb_grid->has_cell_size)
              cell_size = pb_grid->cell_size;
            else
              cell_size = 20;

            material = rut_entity_get_component (entity,
                                                 RUT_COMPONENT_TYPE_MATERIAL);

            if (material)
              asset = rut_material_get_color_source_asset (material);

            if (asset)
              {
                if (rut_asset_get_is_video (asset))
                  {
                    width = 640;
                    height = 480;
                  }
                else
                  {
                    texture = rut_asset_get_texture (asset);
                    if (texture)
                      {
                        width = cogl_texture_get_width (texture);
                        height = cogl_texture_get_height (texture);
                      }
                    else
                      goto ERROR_POINTALISM;
                  }
              }
            else
              goto ERROR_POINTALISM;

            grid = rut_pointalism_grid_new (unserializer->engine->ctx, cell_size,
                                            width, height);

            rut_entity_add_component (entity, grid);
            rut_refable_unref (grid);

            if (pb_grid->has_scale)
              rut_pointalism_grid_set_scale (grid, pb_grid->scale);

            if (pb_grid->has_z)
              rut_pointalism_grid_set_z (grid, pb_grid->z);

            if (pb_grid->has_lighter)
              rut_pointalism_grid_set_lighter (grid, pb_grid->lighter);

            register_unserializer_object (unserializer, grid, component_id);

            ERROR_POINTALISM:
              {
                if (!grid)
                  {
                    collect_error (unserializer,
                                   "Can't add pointalism grid component without "
                                   "an image source");

                    rut_refable_unref (material);
                  }
              }
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__HAIR:
          {
            RutHair *hair = rut_hair_new (unserializer->engine->ctx);
            RutObject *geom;

            rut_entity_add_component (entity, hair);
            rut_refable_unref (hair);

            set_properties_from_pb_boxed_values (unserializer,
                                                 hair,
                                                 pb_component->n_properties,
                                                 pb_component->properties);

            register_unserializer_object (unserializer, hair, component_id);


#warning "FIXME: don't derive complex hair meshes on the fly at runtime!"
            /* XXX: This is a duplication of the special logic we have
             * in rig-engine.c when first adding a hair component to
             * an entity where we derive out special hair geometry
             * from the current geometry.
             *
             * FIXME: This should not be done on the fly when loading
             * a UI since this can be hugely expensive. We should be
             * saving an loading a hair mesh that is derived offline.
             */

            geom = rut_entity_get_component (entity,
                                             RUT_COMPONENT_TYPE_GEOMETRY);

            if (geom && rut_object_get_type (geom) == &rut_model_type)
              {
                RutModel *hair_geom = rut_model_new_for_hair (geom);

                rut_entity_remove_component (entity, geom);
                rut_entity_add_component (entity, hair_geom);
                rut_refable_unref (hair_geom);
              }

            break;
          }

        case RIG__ENTITY__COMPONENT__TYPE__LIGHT:
        case RIG__ENTITY__COMPONENT__TYPE__MATERIAL:
        case RIG__ENTITY__COMPONENT__TYPE__MODEL:
        case RIG__ENTITY__COMPONENT__TYPE__TEXT:
        case RIG__ENTITY__COMPONENT__TYPE__CAMERA:
          break;
        }
    }
}

static void
unserialize_entities (RigPBUnSerializer *unserializer,
                      int n_entities,
                      Rig__Entity **entities)
{
  int i;

  for (i = 0; i < n_entities; i++)
    {
      Rig__Entity *pb_entity = entities[i];
      RutEntity *entity;
      uint64_t id;
      bool force_material = false;

      if (!pb_entity->has_id)
        continue;

      id = pb_entity->id;
      if (g_hash_table_lookup (unserializer->id_map, &id))
        {
          collect_error (unserializer, "Duplicate entity id %d", (int)id);
          continue;
        }

      entity = rut_entity_new (unserializer->engine->ctx);

      if (pb_entity->has_parent_id)
        {
          unsigned int parent_id = pb_entity->parent_id;
          RutEntity *parent = unserializer_find_entity (unserializer, parent_id);

          if (!parent)
            {
              collect_error (unserializer,
                             "Invalid parent id referenced in entity element");
              rut_refable_unref (entity);
              continue;
            }

          rut_graphable_add_child (parent, entity);
        }

      if (pb_entity->label)
        rut_entity_set_label (entity, pb_entity->label);

      if (pb_entity->position)
        {
          Rig__Vec3 *pos = pb_entity->position;
          float position[3] = {
              pos->x,
              pos->y,
              pos->z
          };
          rut_entity_set_position (entity, position);
        }
      if (pb_entity->rotation)
        {
          CoglQuaternion q;

          pb_init_quaternion (&q, pb_entity->rotation);

          rut_entity_set_rotation (entity, &q);
        }
      if (pb_entity->has_scale)
        rut_entity_set_scale (entity, pb_entity->scale);

#warning "remove entity::cast_shadow compatibility"
      if (pb_entity->has_cast_shadow)
        force_material = true;

      unserialize_components (unserializer, entity, pb_entity, force_material);

      register_unserializer_object (unserializer, entity, id);

      unserializer->entities =
        g_list_prepend (unserializer->entities, entity);
    }
}

static void
unserialize_assets (RigPBUnSerializer *unserializer,
                    int n_assets,
                    Rig__Asset **assets)
{
  RigEngine *engine = unserializer->engine;
  int i;

  for (i = 0; i < n_assets; i++)
    {
      Rig__Asset *pb_asset = assets[i];
      uint64_t id;
      RutAsset *asset = NULL;

      if (!pb_asset->has_id)
        continue;

      id = pb_asset->id;
      if (g_hash_table_lookup (unserializer->id_map, &id))
        {
          collect_error (unserializer, "Duplicate asset id %d", (int)id);
          return;
        }

      if (!pb_asset->path)
        continue;

      if (pb_asset->has_data)
        {
          asset = rut_asset_new_from_data (engine->ctx,
                                           pb_asset->path,
                                           pb_asset->type,
                                           pb_asset->is_video,
                                           pb_asset->data.data,
                                           pb_asset->data.len);
        }
      else if (pb_asset->mesh)
        {
          RutMesh *mesh =
            rig_pb_unserialize_mesh (unserializer, pb_asset->mesh);
          if (!mesh)
            {
              collect_error (unserializer,
                             "Error unserializing mesh for asset id %d",
                             (int)id);
              continue;
            }
          asset = rut_asset_new_from_mesh (engine->ctx, mesh);
          rut_refable_unref (mesh);
        }
      else if (unserializer->engine->ctx->assets_location)
        {
          char *full_path =
            g_build_filename (unserializer->engine->ctx->assets_location,
                              pb_asset->path, NULL);
          GFile *asset_file = g_file_new_for_path (full_path);
          GFileInfo *info = g_file_query_info (asset_file,
                                               "standard::*",
                                               G_FILE_QUERY_INFO_NONE,
                                               NULL,
                                               NULL);
          if (info)
            {
              asset = rig_load_asset (unserializer->engine, info, asset_file);
              g_object_unref (info);
            }

          g_object_unref (asset_file);
          g_free (full_path);
        }

      if (asset)
        {
          unserializer->assets =
            g_list_prepend (unserializer->assets, asset);
          register_unserializer_object (unserializer, asset, id);
        }
      else
        g_warning ("Failed to load \"%s\" asset", pb_asset->path);
    }
}

static void
unserialize_path_nodes (RigPBUnSerializer *unserializer,
                        RigPath *path,
                        int n_nodes,
                        Rig__Node **nodes)
{
  int i;

  for (i = 0; i < n_nodes; i++)
    {
      Rig__Node *pb_node = nodes[i];
      Rig__PropertyValue *pb_value;
      float t;

      if (!pb_node->has_t)
        continue;
      t = pb_node->t;

      if (!pb_node->value)
        continue;
      pb_value = pb_node->value;

      switch (path->type)
        {
        case RUT_PROPERTY_TYPE_FLOAT:
          rig_path_insert_float (path, t, pb_value->float_value);
          break;
        case RUT_PROPERTY_TYPE_DOUBLE:
          rig_path_insert_double (path, t, pb_value->double_value);
          break;
        case RUT_PROPERTY_TYPE_INTEGER:
          rig_path_insert_integer (path, t, pb_value->integer_value);
          break;
        case RUT_PROPERTY_TYPE_UINT32:
          rig_path_insert_uint32 (path, t, pb_value->uint32_value);
          break;
        case RUT_PROPERTY_TYPE_VEC3:
          {
            float vec3[3] = {
                pb_value->vec3_value->x,
                pb_value->vec3_value->y,
                pb_value->vec3_value->z
            };
            rig_path_insert_vec3 (path, t, vec3);
            break;
          }
        case RUT_PROPERTY_TYPE_VEC4:
          {
            float vec4[4] = {
                pb_value->vec4_value->x,
                pb_value->vec4_value->y,
                pb_value->vec4_value->z,
                pb_value->vec4_value->w
            };
            rig_path_insert_vec4 (path, t, vec4);
            break;
          }
        case RUT_PROPERTY_TYPE_COLOR:
          {
            CoglColor color;
            pb_init_color (unserializer->engine->ctx, &color, pb_value->color_value);
            rig_path_insert_color (path, t, &color);
            break;
          }
        case RUT_PROPERTY_TYPE_QUATERNION:
          {
            CoglQuaternion quaternion;
            pb_init_quaternion (&quaternion, pb_value->quaternion_value);
            rig_path_insert_quaternion (path, t, &quaternion);
            break;
          }

          /* These shouldn't be animatable */
        case RUT_PROPERTY_TYPE_BOOLEAN:
        case RUT_PROPERTY_TYPE_TEXT:
        case RUT_PROPERTY_TYPE_ENUM:
        case RUT_PROPERTY_TYPE_ASSET:
        case RUT_PROPERTY_TYPE_OBJECT:
        case RUT_PROPERTY_TYPE_POINTER:
          g_warn_if_reached ();
        }
    }
}

static void
unserialize_controller_properties (RigPBUnSerializer *unserializer,
                                   RigController *controller,
                                   int n_properties,
                                   Rig__Controller__Property **properties)
{
  int i;

  for (i = 0; i < n_properties; i++)
    {
      Rig__Controller__Property *pb_property = properties[i];
      uint64_t object_id;
      RutObject *object;
      RigControllerMethod method;
      RutProperty *property;
      RutBoxed boxed_value;

      if (!pb_property->has_object_id ||
          pb_property->name == NULL)
        continue;

      if (pb_property->has_method)
        {
          switch (pb_property->method)
            {
            case RIG__CONTROLLER__PROPERTY__METHOD__CONSTANT:
              method = RIG_CONTROLLER_METHOD_CONSTANT;
              break;
            case RIG__CONTROLLER__PROPERTY__METHOD__PATH:
              method = RIG_CONTROLLER_METHOD_PATH;
              break;
            case RIG__CONTROLLER__PROPERTY__METHOD__C_BINDING:
              method = RIG_CONTROLLER_METHOD_BINDING;
              break;
            default:
              g_warn_if_reached ();
              method = RIG_CONTROLLER_METHOD_CONSTANT;
            }
        }
      else if (pb_property->has_animated) /* deprecated */
        {
          if (pb_property->animated)
            method = RIG_CONTROLLER_METHOD_PATH;
          else
            method = RIG_CONTROLLER_METHOD_CONSTANT;
        }
      else
        method = RIG_CONTROLLER_METHOD_CONSTANT;

      object_id = pb_property->object_id;

      object = unserializer_find_introspectable (unserializer, object_id);
      if (!object)
        {
          collect_error (unserializer,
                         "Invalid object id %d referenced in property element",
                         (int)object_id);
          continue;
        }

      property = rut_introspectable_lookup_property (object, pb_property->name);

#warning "todo: remove entity::cast_shadow compatibility"
      if (!property &&
          rut_object_get_type (object) == &rut_entity_type &&
          strcmp (pb_property->name, "cast_shadow") == 0)
        {
          object = rut_entity_get_component (object,
                                             RUT_COMPONENT_TYPE_MATERIAL);
          if (object)
            property = rut_introspectable_lookup_property (object,
                                                           pb_property->name);
        }

      if (!property)
        {
          collect_error (unserializer,
                         "Invalid object property name given for "
                         "controller property");
          continue;
        }

      if (!property->spec->animatable &&
          method != RIG_CONTROLLER_METHOD_CONSTANT)
        {
          collect_error (unserializer,
                         "Can't dynamically control non-animatable property");
          continue;
        }

      rig_controller_add_property (controller, property);

      rig_controller_set_property_method (controller,
                                          property,
                                          method);

      pb_init_boxed_value (unserializer,
                           &boxed_value,
                           property->spec->type,
                           pb_property->constant);

      rig_controller_set_property_constant (controller,
                                            property,
                                            &boxed_value);

      rut_boxed_destroy (&boxed_value);

      if (pb_property->path)
        {
          Rig__Path *pb_path = pb_property->path;
          RigPath *path = rig_path_new (unserializer->engine->ctx,
                                        property->spec->type);

          unserialize_path_nodes (unserializer,
                                  path,
                                  pb_path->n_nodes,
                                  pb_path->nodes);

          rig_controller_set_property_path (controller,
                                            property,
                                            path);

          rut_refable_unref (path);
        }

      if (pb_property->c_expression)
        {
          int j;
          RutProperty **dependencies;
          RutProperty *dependency;

          if (pb_property->n_dependencies)
            dependencies = alloca (sizeof (void *) *
                                   pb_property->n_dependencies);
          else
            dependencies = NULL;

          for (j = 0; j < pb_property->n_dependencies; j++)
            {
              RutObject *dependency_object;
              Rig__Controller__Property__Dependency *pb_dependency =
                pb_property->dependencies[j];

              if (!pb_dependency->has_object_id)
                {
                  collect_error (unserializer,
                                 "Property dependency with no object ID");
                  break;
                }

              if (!pb_dependency->name)
                {
                  collect_error (unserializer,
                                 "Property dependency with no name");
                  break;
                }

              dependency_object =
                unserializer_find_introspectable (unserializer,
                                                  pb_dependency->object_id);
              if (!dependency_object)
                {
                  collect_error (unserializer,
                                 "Failed to find dependency object "
                                 "for property");
                  break;
                }

              dependency = rut_introspectable_lookup_property (dependency_object,
                                                               pb_dependency->name);
              if (!dependency)
                {
                  collect_error (unserializer,
                                 "Failed to introspect dependency object "
                                 "for binding property");
                  break;
                }

              dependencies[j] = dependency;
            }

          if (j != pb_property->n_dependencies)
            {
              collect_error (unserializer,
                             "Not able to resolve all dependencies for "
                             "property binding (skipping)");
              continue;
            }

          rig_controller_set_property_binding (controller,
                                               property,
                                               pb_property->c_expression,
                                               dependencies,
                                               pb_property->n_dependencies);
        }
    }
}

static bool
have_boxed_pb_property (Rig__Boxed **properties,
                        int n_properties,
                        const char *name)
{
  int i;
  for (i = 0; i < n_properties; i++)
    {
      Rig__Boxed *pb_boxed = properties[i];
      if (strcmp (pb_boxed->name, name) == 0)
        return true;
    }
  return false;
}

static void
unserialize_controllers (RigPBUnSerializer *unserializer,
                         int n_controllers,
                         Rig__Controller **controllers)
{
  int i;

  /* First we just allocate empty controllers and register them all
   * with ids before adding properties to them which may belong
   * to other controllers.
   */

  for (i = 0; i < n_controllers; i++)
    {
      Rig__Controller *pb_controller = controllers[i];
      RigController *controller;
      const char *name;
      uint64_t id;

      if (!pb_controller->has_id)
        continue;

      id = pb_controller->id;

      if (pb_controller->name)
        name = pb_controller->name;
      else
        name = "Controller 0";

      controller = rig_controller_new (unserializer->engine, name);

      rig_controller_set_active (controller, true);

      /* Properties of the RigController itself */
      set_properties_from_pb_boxed_values (unserializer,
                                           controller,
                                           pb_controller->n_controller_properties,
                                           pb_controller->controller_properties);

      if (!have_boxed_pb_property (pb_controller->controller_properties,
                                   pb_controller->n_controller_properties,
                                   "length"))
        {
          /* XXX: for compatibility we set a default controller length of 20
           * seconds */
          rig_controller_set_length (controller, 20);
        }

      unserializer->controllers =
        g_list_prepend (unserializer->controllers, controller);

      if (id)
        register_unserializer_object (unserializer, controller, id);
    }

  for (i = 0; i < n_controllers; i++)
    {
      Rig__Controller *pb_controller = controllers[i];
      RigController *controller;

      if (!pb_controller->has_id)
        continue;

      controller = unserializer_find_introspectable (unserializer,
                                                     pb_controller->id);
      if (!controller)
        {
          g_warn_if_reached ();
          continue;
        }

      /* Properties controlled by the RigController... */
      unserialize_controller_properties (unserializer,
                                         controller,
                                         pb_controller->n_properties,
                                         pb_controller->properties);
    }
}

RigPBUnSerializer *
rig_pb_unserializer_new (RigEngine *engine)
{
  RigPBUnSerializer *unserializer = g_slice_new0 (RigPBUnSerializer);

  unserializer->engine = engine;

  /* This hash table maps from uint64_t ids to objects while loading */
  unserializer->id_map = g_hash_table_new_full (g_int64_hash,
                                                g_int64_equal,
                                                free_id_slice,
                                                NULL);

  rut_memory_stack_rewind (engine->serialization_stack);

  return unserializer;
}

void
rig_pb_unserializer_destroy (RigPBUnSerializer *unserializer)
{
  g_hash_table_destroy (unserializer->id_map);

  g_slice_free (RigPBUnSerializer, unserializer);
}

void
rig_pb_unserialize_ui (RigPBUnSerializer *unserializer,
                       const Rig__UI *pb_ui,
                       bool skip_assets)
{
  RigEngine *engine = unserializer->engine;
  GList *l;

  if (pb_ui->device)
    {
      Rig__Device *device = pb_ui->device;
      if (device->has_width)
        engine->device_width = device->width;
      if (device->has_height)
        engine->device_height = device->height;

      if (device->background)
        pb_init_color (engine->ctx,
                       &engine->background_color,
                       device->background);
    }

  unserialize_assets (unserializer,
                      pb_ui->n_assets,
                      pb_ui->assets);

  unserialize_entities (unserializer,
                        pb_ui->n_entities,
                        pb_ui->entities);

  unserialize_controllers (unserializer,
                           pb_ui->n_controllers,
                           pb_ui->controllers);

  rig_engine_free_ui (engine);

  engine->scene = rut_graph_new (engine->ctx);
  for (l = unserializer->entities; l; l = l->next)
    {
      if (rut_graphable_get_parent (l->data) == NULL)
        rut_graphable_add_child (engine->scene, l->data);
    }

  if (unserializer->light)
    engine->light = unserializer->light;

  engine->controllers = unserializer->controllers;

  if (!skip_assets)
    engine->assets = unserializer->assets;

  rig_engine_handle_ui_update (engine);

  rut_shell_queue_redraw (engine->ctx->shell);
}

typedef struct _NamedBuffer
{
  uint64_t id;
  RutBuffer *buffer;
} NamedBuffer;

RutMesh *
rig_pb_unserialize_mesh (RigPBUnSerializer *unserializer,
                         Rig__Mesh *pb_mesh)
{
  int i;
  NamedBuffer named_buffers[pb_mesh->n_buffers];
  int n_buffers = 0;
  RutAttribute *attributes[pb_mesh->n_attributes];
  int n_attributes = 0;
  CoglVerticesMode mode;
  RutMesh *mesh = NULL;

  for (i = 0; i < pb_mesh->n_buffers; i++)
    {
      Rig__Buffer *pb_buffer = pb_mesh->buffers[i];
      RutBuffer *buffer;

      if (!pb_buffer->has_id || !pb_buffer->has_data)
        {
          goto ERROR;
        }

      buffer = rut_buffer_new (pb_buffer->data.len);
      memcpy (buffer->data, pb_buffer->data.data, pb_buffer->data.len);
      named_buffers[i].id = pb_buffer->id;
      named_buffers[i].buffer = buffer;
      n_buffers++;
    }

  for (i = 0; i < pb_mesh->n_attributes; i++)
    {
      Rig__Attribute *pb_attribute = pb_mesh->attributes[i];
      RutBuffer *buffer = NULL;
      RutAttributeType type;
      int j;

      if (!pb_attribute->has_buffer_id ||
          !pb_attribute->name ||
          !pb_attribute->has_stride ||
          !pb_attribute->has_offset ||
          !pb_attribute->has_n_components ||
          !pb_attribute->has_type)
        {
          goto ERROR;
        }

      for (j = 0; j < pb_mesh->n_buffers; j++)
        {
          if (named_buffers[j].id == pb_attribute->buffer_id)
            {
              buffer = named_buffers[j].buffer;
              break;
            }
        }
      if (!buffer)
        goto ERROR;

      switch (pb_attribute->type)
        {
        case RIG__ATTRIBUTE__TYPE__BYTE:
          type = RUT_ATTRIBUTE_TYPE_BYTE;
          break;
        case RIG__ATTRIBUTE__TYPE__UNSIGNED_BYTE:
          type = RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE;
          break;
        case RIG__ATTRIBUTE__TYPE__SHORT:
          type = RUT_ATTRIBUTE_TYPE_SHORT;
          break;
        case RIG__ATTRIBUTE__TYPE__UNSIGNED_SHORT:
          type = RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT;
          break;
        case RIG__ATTRIBUTE__TYPE__FLOAT:
          type = RUT_ATTRIBUTE_TYPE_FLOAT;
          break;
        }

      attributes[i] =
        rut_attribute_new (buffer,
                           pb_attribute->name,
                           pb_attribute->stride,
                           pb_attribute->offset,
                           pb_attribute->n_components,
                           type);
      if (pb_attribute->has_normalized &&
          pb_attribute->normalized)
        rut_attribute_set_normalized (attributes[i], true);

      n_attributes++;
    }

  if (!pb_mesh->has_mode ||
      !pb_mesh->has_n_vertices)
    goto ERROR;

  switch (pb_mesh->mode)
    {
    case RIG__MESH__MODE__POINTS:
      mode = COGL_VERTICES_MODE_POINTS;
      break;
    case RIG__MESH__MODE__LINES:
      mode = COGL_VERTICES_MODE_LINES;
      break;
    case RIG__MESH__MODE__LINE_LOOP:
      mode = COGL_VERTICES_MODE_LINE_LOOP;
      break;
    case RIG__MESH__MODE__LINE_STRIP:
      mode = COGL_VERTICES_MODE_LINE_STRIP;
      break;
    case RIG__MESH__MODE__TRIANGLES:
      mode = COGL_VERTICES_MODE_TRIANGLES;
      break;
    case RIG__MESH__MODE__TRIANGLE_STRIP:
      mode = COGL_VERTICES_MODE_TRIANGLE_STRIP;
      break;
    case RIG__MESH__MODE__TRIANGLE_FAN:
      mode = COGL_VERTICES_MODE_TRIANGLE_FAN;
      break;
    }

  mesh = rut_mesh_new (mode,
                       pb_mesh->n_vertices,
                       attributes,
                       pb_mesh->n_attributes);

  if (pb_mesh->has_indices_buffer_id)
    {
      RutBuffer *buffer;
      CoglIndicesType indices_type;
      int j;

      for (j = 0; j < pb_mesh->n_buffers; j++)
        {
          if (named_buffers[j].id == pb_mesh->indices_buffer_id)
            {
              buffer = named_buffers[j].buffer;
              break;
            }
        }
      if (!buffer)
        goto ERROR;

      if (!pb_mesh->has_indices_type ||
          !pb_mesh->has_n_indices)
        {
          goto ERROR;
        }

      switch (pb_mesh->indices_type)
        {
        case RIG__MESH__INDICES_TYPE__UNSIGNED_BYTE:
          indices_type = COGL_INDICES_TYPE_UNSIGNED_BYTE;
          break;
        case RIG__MESH__INDICES_TYPE__UNSIGNED_SHORT:
          indices_type = COGL_INDICES_TYPE_UNSIGNED_SHORT;
          break;
        case RIG__MESH__INDICES_TYPE__UNSIGNED_INT:
          indices_type = COGL_INDICES_TYPE_UNSIGNED_INT;
          break;
        }

      rut_mesh_set_indices (mesh,
                            indices_type,
                            buffer,
                            pb_mesh->n_indices);
    }

  /* The mesh will take references on the attributes */
  for (i = 0; i < n_attributes; i++)
    rut_refable_unref (attributes[i]);

  /* The attributes will take their own references on the buffers */
  for (i = 0; i < n_buffers; i++)
    rut_refable_unref (named_buffers[i].buffer);

  return mesh;

ERROR:

  g_warn_if_reached ();

  if (mesh)
    rut_refable_unref (mesh);

  for (i = 0; i < n_attributes; i++)
    rut_refable_unref (attributes[i]);

  for (i = 0; i < n_buffers; i++)
    rut_refable_unref (named_buffers[i].buffer);

  return NULL;
}
