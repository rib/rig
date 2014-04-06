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

#include <config.h>

#include <math.h>

#include <rut.h>

#include "rig.pb-c.h"
#include "rig-pb.h"
#include "rig-engine.h"
#include "rig-entity.h"

#include "components/rig-button-input.h"
#include "components/rig-camera.h"
#include "components/rig-diamond.h"
#include "components/rig-hair.h"
#include "components/rig-light.h"
#include "components/rig-material.h"
#include "components/rig-model.h"
#include "components/rig-nine-slice.h"
#include "components/rig-pointalism-grid.h"
#include "components/rig-shape.h"

const char *
rig_pb_strdup (RigPBSerializer *serializer,
               const char *string)
{
  int len = strlen (string);
  void *mem = rut_memory_stack_memalign (serializer->stack, len + 1, 1);
  memcpy (mem, string, len + 1);
  return mem;
}

static Rig__Color *
pb_color_new (RigPBSerializer *serializer, const CoglColor *color)
{
  Rig__Color *pb_color = rig_pb_new (serializer, Rig__Color, rig__color__init);
  pb_color->hex = rut_memory_stack_alloc (serializer->stack, 10); /* #rrggbbaa */
  snprintf (pb_color->hex, 10, "#%02x%02x%02x%02x",
            cogl_color_get_red_byte (color),
            cogl_color_get_green_byte (color),
            cogl_color_get_blue_byte (color),
            cogl_color_get_alpha_byte (color));

  return pb_color;
}

static Rig__Rotation *
pb_rotation_new (RigPBSerializer *serializer, const CoglQuaternion *quaternion)
{
  Rig__Rotation *pb_rotation =
    rig_pb_new (serializer, Rig__Rotation, rig__rotation__init);
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
pb_vec3_new (RigPBSerializer *serializer,
             float x,
             float y,
             float z)
{
  Rig__Vec3 *pb_vec3 = rig_pb_new (serializer, Rig__Vec3, rig__vec3__init);

  pb_vec3->x = x;
  pb_vec3->y = y;
  pb_vec3->z = z;

  return pb_vec3;
}

static Rig__Vec4 *
pb_vec4_new (RigPBSerializer *serializer,
             float x,
             float y,
             float z,
             float w)
{
  Rig__Vec4 *pb_vec4 = rig_pb_new (serializer, Rig__Vec4, rig__vec4__init);

  pb_vec4->x = x;
  pb_vec4->y = y;
  pb_vec4->z = z;
  pb_vec4->w = w;

  return pb_vec4;
}

static Rig__Path *
pb_path_new (RigPBSerializer *serializer, RigPath *path)
{
  Rig__Path *pb_path = rig_pb_new (serializer, Rig__Path, rig__path__init);
  RigNode *node;
  int i;

  if (!path->length)
    return pb_path;

  pb_path->nodes = rut_memory_stack_memalign (serializer->stack,
                                              sizeof (void *) * path->length,
                                              RUT_UTIL_ALIGNOF (void *));
  pb_path->n_nodes = path->length;

  i = 0;
  rut_list_for_each (node, &path->nodes, list_node)
    {
      Rig__Node *pb_node =
        rig_pb_new (serializer, Rig__Node, rig__node__init);

      pb_path->nodes[i] = pb_node;

      pb_node->has_t = TRUE;
      pb_node->t = node->t;

      pb_node->value = rig_pb_new (serializer,
                                   Rig__PropertyValue,
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
              pb_node->value->vec3_value = pb_vec3_new (serializer,
                                                        vec3[0],
                                                        vec3[1],
                                                        vec3[2]);
              break;
            }
        case RUT_PROPERTY_TYPE_VEC4:
            {
              float *vec4 = node->boxed.d.vec4_val;
              pb_node->value->vec4_value = pb_vec4_new (serializer,
                                                        vec4[0],
                                                        vec4[1],
                                                        vec4[2],
                                                        vec4[3]);
              break;
            }
        case RUT_PROPERTY_TYPE_COLOR:
            {
              pb_node->value->color_value =
                pb_color_new (serializer, &node->boxed.d.color_val);
              break;
            }
        case RUT_PROPERTY_TYPE_QUATERNION:
            {
              pb_node->value->quaternion_value =
                pb_rotation_new (serializer, &node->boxed.d.quaternion_val);
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

void
rig_pb_property_value_init (RigPBSerializer *serializer,
                            Rig__PropertyValue *pb_value,
                            const RutBoxed *value)
{
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
        pb_rotation_new (serializer, &value->d.quaternion_val);
      break;

    case RUT_PROPERTY_TYPE_VEC3:
      pb_value->vec3_value = pb_vec3_new (serializer,
                                          value->d.vec3_val[0],
                                          value->d.vec3_val[1],
                                          value->d.vec3_val[2]);
      break;

    case RUT_PROPERTY_TYPE_VEC4:
      pb_value->vec4_value = pb_vec4_new (serializer,
                                          value->d.vec4_val[0],
                                          value->d.vec4_val[1],
                                          value->d.vec4_val[2],
                                          value->d.vec4_val[3]);
      break;

    case RUT_PROPERTY_TYPE_COLOR:
      pb_value->color_value = pb_color_new (serializer, &value->d.color_val);
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
          uint64_t id =
            rig_pb_serializer_lookup_object_id (serializer, value->d.asset_val);

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
          uint64_t id =
            rig_pb_serializer_lookup_object_id (serializer, value->d.object_val);

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
}

Rig__PropertyValue *
pb_property_value_new (RigPBSerializer *serializer,
                       const RutBoxed *value)
{
  Rig__PropertyValue *pb_value =
    rig_pb_new (serializer, Rig__PropertyValue, rig__property_value__init);

  rig_pb_property_value_init (serializer, pb_value, value);

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
  Rig__Boxed *pb_boxed =
    rig_pb_new (serializer, Rig__Boxed, rig__boxed__init);

  pb_boxed->name = (char *)name;
  pb_boxed->has_type = TRUE;
  pb_boxed->type = rut_property_type_to_pb_type (boxed->type);
  pb_boxed->value = pb_property_value_new (serializer, boxed);

  return pb_boxed;
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
  serializer->n_properties = 0;
  rut_introspectable_foreach_property (object,
                                       count_instrospectables_cb,
                                       serializer);
  *n_properties_out = serializer->n_properties;

  serializer->properties_out = *properties_out =
    rut_memory_stack_memalign (serializer->stack,
                               sizeof (void *) * serializer->n_properties,
                               RUT_UTIL_ALIGNOF (void *));

  serializer->n_properties = 0;
  rut_introspectable_foreach_property (object,
                                       serialize_instrospectables_cb,
                                       serializer);
}

Rig__Entity__Component *
rig_pb_serialize_component (RigPBSerializer *serializer,
                            RutComponent *component)
{
  const RutType *type = rut_object_get_type (component);
  Rig__Entity__Component *pb_component;
  int component_id;

  pb_component = rig_pb_new (serializer,
                             Rig__Entity__Component,
                             rig__entity__component__init);

  component_id = rig_pb_serializer_register_object (serializer, component);

  pb_component->has_id = true;
  pb_component->id = component_id;

  pb_component->has_type = true;

  if (type == &rig_light_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__LIGHT;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rig_material_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__MATERIAL;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rig_shape_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__SHAPE;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rig_diamond_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__DIAMOND;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rig_pointalism_grid_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__POINTALISM_GRID;
      pb_component->grid =
        rig_pb_new (serializer,
                    Rig__Entity__Component__PointalismGrid,
                    rig__entity__component__pointalism_grid__init);

      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rig_model_type)
    {
      RigModel *model = (RigModel *)component;
      uint64_t asset_id =
        rig_pb_serializer_lookup_object_id (serializer,
                                            rig_model_get_asset (model));

      /* XXX: we don't support serializing a model loaded from a RutMesh */
      g_warn_if_fail (asset_id != 0);

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__MODEL;
      pb_component->model = rig_pb_new (serializer,
                                        Rig__Entity__Component__Model,
                                        rig__entity__component__model__init);

      if (asset_id)
        {
          pb_component->model->has_asset_id = true;
          pb_component->model->asset_id = asset_id;
        }
    }
  else if (type == &rut_text_type)
    {
      RutText *text = (RutText *)component;
      const CoglColor *color;
      Rig__Entity__Component__Text *pb_text;

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__TEXT;
      pb_text = rig_pb_new (serializer,
                            Rig__Entity__Component__Text,
                            rig__entity__component__text__init);
      pb_component->text = pb_text;

      color = rut_text_get_color (text);

      pb_text->text = (char *)rut_text_get_text (text);
      pb_text->font = (char *)rut_text_get_font_name (text);
      pb_text->color = pb_color_new (serializer, color);
    }
  else if (type == &rig_camera_type)
    {
      RigCamera *camera = (RigCamera *)component;
      Rig__Entity__Component__Camera *pb_camera;
      const float *viewport;
      float zoom;

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__CAMERA;
      pb_camera = rig_pb_new (serializer,
                              Rig__Entity__Component__Camera,
                              rig__entity__component__camera__init);
      pb_component->camera = pb_camera;

      pb_camera->has_projection_mode = true;
      switch (rut_camera_get_projection_mode (camera))
        {
        case RUT_PROJECTION_ORTHOGRAPHIC:
          pb_camera->projection_mode =
            RIG__ENTITY__COMPONENT__CAMERA__PROJECTION_MODE__ORTHOGRAPHIC;

          pb_camera->ortho = rig_pb_new (serializer,
                                         Rig__OrthoCoords,
                                         rig__ortho_coords__init);
          rut_camera_get_orthographic_coordinates (camera,
                                                   &pb_camera->ortho->x0,
                                                   &pb_camera->ortho->y0,
                                                   &pb_camera->ortho->x1,
                                                   &pb_camera->ortho->y1);
          break;
        case RUT_PROJECTION_PERSPECTIVE:
          pb_camera->projection_mode =
            RIG__ENTITY__COMPONENT__CAMERA__PROJECTION_MODE__PERSPECTIVE;
          pb_camera->has_field_of_view = true;
          pb_camera->field_of_view = rut_camera_get_field_of_view (camera);
          break;
        }

      pb_camera->viewport = rig_pb_new (serializer,
                                        Rig__Viewport,
                                        rig__viewport__init);

      viewport = rut_camera_get_viewport (camera);
      pb_camera->viewport->x = viewport[0];
      pb_camera->viewport->y = viewport[1];
      pb_camera->viewport->width = viewport[2];
      pb_camera->viewport->height = viewport[3];

      zoom = rut_camera_get_zoom (camera);
      if (zoom != 1)
        {
          pb_camera->has_zoom = true;
          pb_camera->zoom = zoom;
        }

      pb_camera->has_focal_distance = true;
      pb_camera->focal_distance = rut_camera_get_focal_distance (camera);

      pb_camera->has_depth_of_field = true;
      pb_camera->depth_of_field = rut_camera_get_depth_of_field (camera);

      pb_camera->has_near_plane = true;
      pb_camera->near_plane = rut_camera_get_near_plane (camera);
      pb_camera->has_far_plane = true;
      pb_camera->far_plane = rut_camera_get_far_plane (camera);

      pb_camera->background =
        pb_color_new (serializer, rut_camera_get_background_color (camera));
    }
  else if (type == &rig_nine_slice_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__NINE_SLICE;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rig_hair_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__HAIR;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }
  else if (type == &rig_button_input_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__BUTTON_INPUT;
      serialize_instrospectable_properties (component,
                                            &pb_component->n_properties,
                                            (void **)&pb_component->properties,
                                            serializer);
    }

  return pb_component;
}

void
serialize_component_cb (RutComponent *component,
                        void *user_data)
{
  RigPBSerializer *serializer = user_data;
  Rig__Entity__Component *pb_component =
    rig_pb_serialize_component (serializer, component);

  serializer->n_pb_components++;
  serializer->pb_components = g_list_prepend (serializer->pb_components, pb_component);
}

Rig__Entity *
rig_pb_serialize_entity (RigPBSerializer *serializer,
                         RigEntity *parent,
                         RigEntity *entity)
{
  Rig__Entity *pb_entity =
    rig_pb_new (serializer, Rig__Entity, rig__entity__init);
  const CoglQuaternion *q;
  const char *label;
  Rig__Vec3 *position;
  float scale;
  GList *l;
  int i;

  pb_entity->has_id = true;
  pb_entity->id = rig_pb_serializer_register_object (serializer, entity);

  if (parent && rut_object_get_type (parent) == &rig_entity_type)
    {
      uint64_t id = rig_pb_serializer_lookup_object_id (serializer, parent);
      if (id)
        {
          pb_entity->has_parent_id = true;
          pb_entity->parent_id = id;
        }
      else
        g_warning ("Failed to find id of parent entity\n");
    }

  label = rig_entity_get_label (entity);
  if (label && *label)
    {
      pb_entity->label = (char *)label;
    }

  q = rig_entity_get_rotation (entity);

  position = rig_pb_new (serializer, Rig__Vec3, rig__vec3__init);
  position->x = rig_entity_get_x (entity);
  position->y = rig_entity_get_y (entity);
  position->z = rig_entity_get_z (entity);
  pb_entity->position = position;

  scale = rig_entity_get_scale (entity);
  if (scale != 1)
    {
      pb_entity->has_scale = TRUE;
      pb_entity->scale = scale;
    }

  pb_entity->rotation = pb_rotation_new (serializer, q);

  serializer->n_pb_components = 0;
  serializer->pb_components = NULL;
  rig_entity_foreach_component (entity,
                                serialize_component_cb,
                                serializer);

  pb_entity->n_components = serializer->n_pb_components;
  pb_entity->components =
    rut_memory_stack_memalign (serializer->stack,
                               sizeof (void *) * pb_entity->n_components,
                               RUT_UTIL_ALIGNOF (void *));

  for (i = 0, l = serializer->pb_components; l; i++, l = l->next)
    pb_entity->components[i] = l->data;
  g_list_free (serializer->pb_components);
  serializer->pb_components = NULL;
  serializer->n_pb_components = 0;

  return pb_entity;
}

static RutTraverseVisitFlags
_rig_entitygraph_pre_serialize_cb (RutObject *object,
                                   int depth,
                                   void *user_data)
{
  RigPBSerializer *serializer = user_data;
  const RutType *type = rut_object_get_type (object);
  RutObject *parent = rut_graphable_get_parent (object);
  RigEntity *entity;
  Rig__Entity *pb_entity;
  const char *label;

  if (type != &rig_entity_type)
    {
      g_warning ("Can't save non-entity graphables\n");
      return RUT_TRAVERSE_VISIT_CONTINUE;
    }

  entity = object;

  /* NB: labels with a "rig:" prefix imply that this is an internal
   * entity that shouldn't be saved (such as the editing camera
   * entities) */
  label = rig_entity_get_label (entity);
  if (label && strncmp ("rig:", label, 4) == 0)
    return RUT_TRAVERSE_VISIT_CONTINUE;

  pb_entity = rig_pb_serialize_entity (serializer, parent, entity);

  serializer->n_pb_entities++;
  serializer->pb_entities = g_list_prepend (serializer->pb_entities, pb_entity);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

typedef struct _DepsState
{
  int i;
  RigPBSerializer *serializer;
  Rig__Controller__Property__Dependency *pb_dependencies;
} DepsState;

static void
serialize_binding_dep_cb (RigBinding *binding,
                          RutProperty *dependency,
                          void *user_data)
{
  DepsState *state = user_data;
  Rig__Controller__Property__Dependency *pb_dependency =
    &state->pb_dependencies[state->i++];
  uint64_t id =
    rig_pb_serializer_lookup_object_id (state->serializer,
                                        dependency->object);

  g_warn_if_fail (id != 0);

  rig__controller__property__dependency__init (pb_dependency);

  pb_dependency->has_object_id = true;
  pb_dependency->object_id = id;

  pb_dependency->name =
    (char *)rig_pb_strdup (state->serializer, dependency->spec->name);
}

static void
serialize_controller_property_cb (RigControllerPropData *prop_data,
                                  void *user_data)
{
  RigPBSerializer *serializer = user_data;
  RutObject *object;
  uint64_t id;

  Rig__Controller__Property *pb_property =
    rig_pb_new (serializer,
                Rig__Controller__Property,
                rig__controller__property__init);

  serializer->n_pb_properties++;
  serializer->pb_properties = g_list_prepend (serializer->pb_properties, pb_property);

  object = prop_data->property->object;

  id = rig_pb_serializer_lookup_object_id (serializer, object);
  if (!id)
    g_warning ("Failed to find id of object\n");

  pb_property->has_object_id = true;
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

  if (prop_data->binding)
    {
      int n_deps;
      int i;
      DepsState state;

      pb_property->has_binding_id = true;
      pb_property->binding_id = rig_binding_get_id (prop_data->binding);

      pb_property->c_expression =
        (char *)rig_pb_strdup (serializer,
                               rig_binding_get_expression (prop_data->binding));

      n_deps = rig_binding_get_n_dependencies (prop_data->binding);
      pb_property->n_dependencies = n_deps;
      if (pb_property->n_dependencies)
        {
          Rig__Controller__Property__Dependency *pb_dependencies =
            rut_memory_stack_memalign (
                     serializer->stack,
                     (sizeof (Rig__Controller__Property__Dependency) * n_deps),
                     RUT_UTIL_ALIGNOF (Rig__Controller__Property__Dependency));
          pb_property->dependencies =
            rut_memory_stack_memalign (
                     serializer->stack,
                     (sizeof (void *) * n_deps),
                     RUT_UTIL_ALIGNOF (void *));

          for (i = 0; i < n_deps; i++)
            pb_property->dependencies[i] = &pb_dependencies[i];

          state.i = 0;
          state.serializer = serializer;
          state.pb_dependencies = pb_dependencies;
          rig_binding_foreach_dependency (prop_data->binding,
                                          serialize_binding_dep_cb,
                                          &state);
        }
    }

  pb_property->constant = pb_property_value_new (serializer, &prop_data->constant_value);

  if (prop_data->path && prop_data->path->length)
    pb_property->path = pb_path_new (serializer, prop_data->path);
}

static uint64_t
default_serializer_register_object_cb (void *object,
                                       void *user_data)
{
  RigPBSerializer *serializer = user_data;
  void *id_ptr = (void *)(intptr_t)serializer->next_id++;

  if (!serializer->object_to_id_map)
    {
      serializer->object_to_id_map =
        g_hash_table_new (NULL, /* direct hash */
                          NULL); /* direct key equal */
    }

  if (g_hash_table_lookup (serializer->object_to_id_map, object))
    {
      g_critical ("Duplicate save object id %p", id_ptr);
      return 0;
    }

  g_hash_table_insert (serializer->object_to_id_map, object, id_ptr);

  return (uint64_t)(intptr_t)id_ptr;
}

static uint64_t
serializer_register_pointer_id_object_cb (void *object, void *user_data)
{
  return (uint64_t)(intptr_t)object;
}

uint64_t
rig_pb_serializer_register_object (RigPBSerializer *serializer,
                                   void *object)
{
  void *object_register_data = serializer->object_register_data;
  return serializer->object_register_callback (object, object_register_data);
}

uint64_t
default_serializer_lookup_object_id_cb (void *object,
                                        void *user_data)
{
  RigPBSerializer *serializer = user_data;
  void *id_ptr;

  g_return_val_if_fail (object, 0);

  id_ptr = g_hash_table_lookup (serializer->object_to_id_map, object);

  g_return_val_if_fail (id_ptr, 0);

  return (uint64_t)(intptr_t)id_ptr;
}

uint64_t
serializer_lookup_pointer_object_id_cb (void *object,
                                        void *user_data)
{
  g_return_val_if_fail (object, 0);
  return (uint64_t)(intptr_t)object;
}

uint64_t
rig_pb_serializer_lookup_object_id (RigPBSerializer *serializer, void *object)
{
  if (rut_object_get_type (object) == &rig_asset_type)
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

  return serializer->object_to_id_callback (object, serializer->object_to_id_data);
}

RigPBSerializer *
rig_pb_serializer_new (RigEngine *engine)
{
  RigPBSerializer *serializer = g_slice_new0 (RigPBSerializer);

  serializer->engine = engine;
  serializer->stack = engine->frame_stack;

  serializer->object_register_callback = default_serializer_register_object_cb;
  serializer->object_register_data = serializer;
  serializer->object_to_id_callback = default_serializer_lookup_object_id_cb;
  serializer->object_to_id_data = serializer;

  /* NB: We have to reserve 0 here so we can tell if lookups into the
   * id_map fail. */
  serializer->next_id = 1;

  return serializer;
}

void
rig_pb_serializer_set_stack (RigPBSerializer *serializer,
                             RutMemoryStack *stack)
{
  serializer->stack = stack;
}

void
rig_pb_serializer_set_use_pointer_ids_enabled (RigPBSerializer *serializer,
                                               bool use_pointers)
{
  if (use_pointers)
    {
      serializer->object_register_callback =
        serializer_register_pointer_id_object_cb;
      serializer->object_register_data = serializer;

      serializer->object_to_id_callback =
        serializer_lookup_pointer_object_id_cb;
      serializer->object_to_id_data = serializer;
    }
  else
    {
      /* we don't have a way to save/restore the above callbacks, so
       * really this function is currently just an internal
       * convenience for setting up the callbacks for the common case
       * where we want ids to simply correspond to pointers
       */
      g_warn_if_reached ();
    }
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
rig_pb_serializer_set_only_asset_ids_enabled (RigPBSerializer *serializer,
                                              bool only_ids)
{
  serializer->only_asset_ids = only_ids;
}

void
rig_pb_serializer_set_object_register_callback (RigPBSerializer *serializer,
                                 RigPBSerializerObjecRegisterCallback callback,
                                 void *user_data)
{
  serializer->object_register_callback = callback;
  serializer->object_register_data = user_data;
}

void
rig_pb_serializer_set_object_to_id_callback (RigPBSerializer *serializer,
                                     RigPBSerializerObjecToIDCallback callback,
                                     void *user_data)
{
  serializer->object_to_id_callback = callback;
  serializer->object_to_id_data = user_data;
}

void
rig_pb_serializer_set_skip_image_data (RigPBSerializer *serializer,
                                       bool skip)
{
  serializer->skip_image_data = skip;
}

void
rig_pb_serializer_destroy (RigPBSerializer *serializer)
{
  if (serializer->required_assets)
    g_list_free (serializer->required_assets);

  if (serializer->object_to_id_map)
    g_hash_table_destroy (serializer->object_to_id_map);

  g_slice_free (RigPBSerializer, serializer);
}

static Rig__Buffer *
serialize_buffer (RigPBSerializer *serializer, RutBuffer *buffer)
{
  Rig__Buffer *pb_buffer =
    rig_pb_new (serializer, Rig__Buffer, rig__buffer__init);

  pb_buffer->has_id = true;
  pb_buffer->id =
    rig_pb_serializer_register_object (serializer, buffer);

  /* NB: The serialized asset points directly to the RutMesh
   * data to avoid copying it... */
  pb_buffer->has_data = true;
  pb_buffer->data.data = buffer->data;
  pb_buffer->data.len = buffer->size;

  return pb_buffer;
}

static Rig__Asset *
serialize_mesh_asset (RigPBSerializer *serializer, RigAsset *asset)
{
  RutMesh *mesh = rig_asset_get_mesh (asset);
  Rig__Asset *pb_asset;
  RutBuffer **buffers;
  int n_buffers = 0;
  Rig__Buffer **pb_buffers;
  Rig__Buffer **attribute_buffers_map;
  Rig__Attribute **attributes;
  Rig__Mesh *pb_mesh;
  int i;

  pb_asset = rig_pb_new (serializer, Rig__Asset, rig__asset__init);

  pb_asset->has_id = true;
  pb_asset->id = rig_pb_serializer_lookup_object_id (serializer, asset);

  pb_asset->path = (char *)rig_asset_get_path (asset);

  pb_asset->has_type = true;
  pb_asset->type = RIG_ASSET_TYPE_MESH;

  /* The maximum number of pb_buffers we may need = n_attributes plus 1 in case
   * there is an index buffer... */
  pb_buffers = rut_memory_stack_memalign (serializer->stack,
                                          sizeof (void *) * (mesh->n_attributes + 1),
                                          RUT_UTIL_ALIGNOF (void *));

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

  attributes= rut_memory_stack_memalign (serializer->stack,
                                         sizeof (void *) * mesh->n_attributes,
                                         RUT_UTIL_ALIGNOF (void *));
  for (i = 0; i < mesh->n_attributes; i++)
    {
      Rig__Attribute *pb_attribute =
        rig_pb_new (serializer, Rig__Attribute, rig__attribute__init);
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

  pb_asset->mesh = rig_pb_new (serializer, Rig__Mesh, rig__mesh__init);
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
serialize_asset (RigPBSerializer *serializer, RigAsset *asset)
{
  RutContext *ctx = rig_asset_get_context (asset);
  Rig__Asset *pb_asset;
  GError *error = NULL;
  RigAssetType type;

  if (serializer->only_asset_ids)
    {
      pb_asset = rig_pb_new (serializer, Rig__Asset, rig__asset__init);

      pb_asset->has_id = true;
      pb_asset->id = rig_pb_serializer_lookup_object_id (serializer, asset);

      return pb_asset;
    }

  type = rig_asset_get_type (asset);

  switch (type)
    {
    case RIG_ASSET_TYPE_MESH:
      return serialize_mesh_asset (serializer, asset);
    case RIG_ASSET_TYPE_TEXTURE:
    case RIG_ASSET_TYPE_NORMAL_MAP:
    case RIG_ASSET_TYPE_ALPHA_MASK:
      pb_asset = rig_pb_new (serializer, Rig__Asset, rig__asset__init);

      pb_asset->has_id = true;
      pb_asset->id = rig_pb_serializer_lookup_object_id (serializer, asset);

      pb_asset->has_type = true;
      pb_asset->type = rig_asset_get_type (asset);

      pb_asset->has_is_video = true;
      pb_asset->is_video = rig_asset_get_is_video (asset);

      pb_asset->has_width = true;
      pb_asset->has_height = true;
      rig_asset_get_image_size (asset, &pb_asset->width, &pb_asset->height);

      if (!serializer->skip_image_data)
        {
          const char *path = rig_asset_get_path (asset);
          char *full_path = g_build_filename (ctx->assets_location, path, NULL);
          char *contents;
          size_t len;

          if (!g_file_get_contents (full_path,
                                    &contents,
                                    &len,
                                    &error))
            {
              g_warning ("Failed to read contents of asset: %s",
                         error->message);
              g_error_free (error);
              g_free (full_path);
              return NULL;
            }

          g_free (full_path);

          pb_asset->path = (char *)path;

          pb_asset->has_data = true;
          pb_asset->data.data = (uint8_t *)contents;
          pb_asset->data.len = len;
        }
      break;
    case RIG_ASSET_TYPE_BUILTIN:
      /* XXX: We should be aiming to remove the "builtin" asset type
       * and instead making the editor handle builtins specially
       * in how it lists search results.
       */
      g_warning ("Can't serialize \"builtin\" asset type");
      return NULL;
    }

  return pb_asset;
}

static void
serialized_asset_destroy (Rig__Asset *serialized_asset)
{
  if (serialized_asset->has_data)
    g_free (serialized_asset->data.data);
}

Rig__Controller *
rig_pb_serialize_controller (RigPBSerializer *serializer,
                             RigController *controller)
{
  Rig__Controller *pb_controller =
    rig_pb_new (serializer, Rig__Controller, rig__controller__init);
  GList *l;
  int i;

  pb_controller->has_id = true;
  pb_controller->id = rig_pb_serializer_lookup_object_id (serializer, controller);

  pb_controller->name = controller->label;

  serialize_instrospectable_properties (controller,
                                        &pb_controller->n_controller_properties,
                                        (void **)&pb_controller->controller_properties,
                                        serializer);

  serializer->n_pb_properties = 0;
  serializer->pb_properties = NULL;
  rig_controller_foreach_property (controller,
                                   serialize_controller_property_cb,
                                   serializer);

  pb_controller->n_properties = serializer->n_pb_properties;
  pb_controller->properties =
    rut_memory_stack_memalign (serializer->stack,
                               sizeof (void *) * pb_controller->n_properties,
                               RUT_UTIL_ALIGNOF (void *));
  for (i = 0, l = serializer->pb_properties; l; i++, l = l->next)
    pb_controller->properties[i] = l->data;
  g_list_free (serializer->pb_properties);
  serializer->n_pb_properties = 0;
  serializer->pb_properties = NULL;

  return pb_controller;
}

Rig__UI *
rig_pb_serialize_ui (RigPBSerializer *serializer,
                     bool play_mode,
                     RigUI *ui)
{
  GList *l;
  int i;
  Rig__UI *pb_ui;

  pb_ui = rig_pb_new (serializer, Rig__UI, rig__ui__init);

  pb_ui->has_mode = true;
  pb_ui->mode = play_mode ? RIG__UI__MODE__PLAY : RIG__UI__MODE__EDIT;

  /* Register all assets up front, but we only actually serialize those
   * assets that are referenced - indicated by a corresponding id lookup
   * in rig_pb_serializer_lookup_object_id()
   */
  for (l = ui->assets; l; l = l->next)
    rig_pb_serializer_register_object (serializer, l->data);

  serializer->n_pb_entities = 0;
  rut_graphable_traverse (ui->scene,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          _rig_entitygraph_pre_serialize_cb,
                          NULL,
                          serializer);

  pb_ui->n_entities = serializer->n_pb_entities;
  pb_ui->entities =
    rut_memory_stack_memalign (serializer->stack,
                               sizeof (void *) * pb_ui->n_entities,
                               RUT_UTIL_ALIGNOF (void *));
  for (i = 0, l = serializer->pb_entities; l; i++, l = l->next)
    pb_ui->entities[i] = l->data;
  g_list_free (serializer->pb_entities);
  serializer->pb_entities = NULL;

  for (i = 0, l = ui->controllers; l; i++, l = l->next)
    rig_pb_serializer_register_object (serializer, l->data);

  pb_ui->n_controllers = g_list_length (ui->controllers);
  if (pb_ui->n_controllers)
    {
      pb_ui->controllers =
        rut_memory_stack_memalign (serializer->stack,
                                   sizeof (void *) * pb_ui->n_controllers,
                                   RUT_UTIL_ALIGNOF (void *));

      for (i = 0, l = ui->controllers; l; i++, l = l->next)
        {
          RigController *controller = l->data;
          Rig__Controller *pb_controller =
            rig_pb_serialize_controller (serializer, controller);

          pb_ui->controllers[i] = pb_controller;
        }
    }

  pb_ui->n_assets = g_list_length (serializer->required_assets);
  if (pb_ui->n_assets)
    {
      RigPBAssetFilter save_filter = serializer->asset_filter;
      int i;

      /* Temporarily disable the asset filter that is called in
       * rig_pb_serializer_lookup_object_id() since we have already
       * filtered all of the assets required and we now only need to
       * lookup the ids for serializing the assets themselves. */
      serializer->asset_filter = NULL;

      pb_ui->assets =
        rut_memory_stack_memalign (serializer->stack,
                                   pb_ui->n_assets * sizeof (void *),
                                   RUT_UTIL_ALIGNOF (void *));
      for (i = 0, l = serializer->required_assets; l; i++, l = l->next)
        {
          RigAsset *asset = l->data;
          Rig__Asset *pb_asset = serialize_asset (serializer, asset);

          pb_ui->assets[i] = pb_asset;
        }

      /* restore the asset filter */
      serializer->asset_filter = save_filter;
    }

  if (ui->dso_data)
    {
      pb_ui->has_dso = true;
      pb_ui->dso.data =
        rut_memory_stack_memalign (serializer->stack,
                                   ui->dso_len,
                                   1);
      pb_ui->dso.len = ui->dso_len;
    }

  return pb_ui;
}

void
rig_pb_serialized_ui_destroy (Rig__UI *ui)
{
  int i;

  for (i = 0; i < ui->n_assets; i++)
    serialized_asset_destroy (ui->assets[i]);
}

Rig__Event **
rig_pb_serialize_input_events (RigPBSerializer *serializer,
                               RutInputQueue *input_queue)
{
  int n_events = input_queue->n_events;
  RutInputEvent *event, *tmp;
  Rig__Event **pb_events;
  int i;

  pb_events = rut_memory_stack_memalign (serializer->stack,
                                         n_events * sizeof (void *),
                                         RUT_UTIL_ALIGNOF (void *));

  i = 0;
  rut_list_for_each_safe (event, tmp, &input_queue->events, list_node)
    {
      Rig__Event *pb_event = rig_pb_new (serializer, Rig__Event, rig__event__init);

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
                  rig_pb_new (serializer, Rig__Event__PointerMove,
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
                  rig_pb_new (serializer, Rig__Event__PointerButton,
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
                g_print ("Serialize key up\n");
                pb_event->type = RIG__EVENT__TYPE__KEY_UP;
                break;
              }

            pb_event->key =
              rig_pb_new (serializer, Rig__Event__Key, rig__event__key__init);
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

Rig__Operation **
rig_pb_serialize_ops_queue (RigPBSerializer *serializer,
                            RutQueue *ops)
{
  Rig__Operation **pb_ops;
  RutQueueItem *item;
  int i;

  if (!ops->len)
    return NULL;

  pb_ops =
    rut_memory_stack_memalign (serializer->stack,
                               sizeof (void *) * ops->len,
                               RUT_UTIL_ALIGNOF (void *));

  i = 0;
  rut_list_for_each (item, &ops->items, list_node)
    {
      pb_ops[i++] = item->data;
    }

  return pb_ops;
}


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

static RutObject *
unserializer_find_object (RigPBUnSerializer *unserializer, uint64_t id)
{
  RutObject *ret;

  if (unserializer->id_to_object_callback)
    {
      void *user_data = unserializer->id_to_object_data;
      ret = unserializer->id_to_object_callback (id, user_data);
    }
  else if (unserializer->id_to_object_map)
    ret = g_hash_table_lookup (unserializer->id_to_object_map, &id);
  else
    ret = NULL;

  if (id && !ret)
    rig_pb_unserializer_collect_error (unserializer,
                                       "Invalid object id=%llu", id);

  return ret;
}

void
rig_pb_init_boxed_value (RigPBUnSerializer *unserializer,
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
        unserializer_find_object (unserializer, pb_value->asset_value);
      break;

    case RUT_PROPERTY_TYPE_OBJECT:
      boxed->d.object_val =
        unserializer_find_object (unserializer, pb_value->object_value);
      break;

    case RUT_PROPERTY_TYPE_POINTER:
      g_warn_if_reached ();
      break;
    }
}

void
rig_pb_unserializer_collect_error (RigPBUnSerializer *unserializer,
                                   const char *format,
                                   ...)
{
  va_list ap;

  va_start (ap, format);

  /* XXX: The intention is that we shouldn't just immediately abort loading
   * like this but rather we should collect the errors and try our best to
   * continue loading. At the end we can report the errors to the user so they
   * realize that their document may be corrupt.
   */

  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, format, ap);

  va_end (ap);
}

static void
default_unserializer_unregister_object_cb (uint64_t id,
                                           void *user_data)
{
  RigPBUnSerializer *unserializer = user_data;
  if (!g_hash_table_remove (unserializer->id_to_object_map, &id))
    g_warning ("Tried to unregister an id that wasn't previously registered");
}

void
rig_pb_unserializer_unregister_object (RigPBUnSerializer *unserializer,
                                       uint64_t id)
{
  void *user_data = unserializer->object_unregister_data;
  unserializer->object_unregister_callback (id, user_data);
}

static void
default_unserializer_register_object_cb (void *object,
                                         uint64_t id,
                                         void *user_data)
{
  RigPBUnSerializer *unserializer = user_data;
  uint64_t *key;

  if (!unserializer->id_to_object_map)
    {
      /* This hash table maps from uint64_t ids to objects while loading */
      unserializer->id_to_object_map =
        g_hash_table_new (g_int64_hash, g_int64_equal);
    }

  key = rut_memory_stack_memalign (unserializer->stack,
                                   sizeof (uint64_t),
                                   RUT_UTIL_ALIGNOF (uint64_t));

  *key = id;

  g_return_if_fail (id != 0);

  if (g_hash_table_lookup (unserializer->id_to_object_map, key))
    {
      rig_pb_unserializer_collect_error (unserializer,
                                         "Duplicate unserializer "
                                         "object id %ld", id);
      return;
    }

  g_hash_table_insert (unserializer->id_to_object_map, key, object);
}

void
rig_pb_unserializer_register_object (RigPBUnSerializer *unserializer,
                                     void *object,
                                     uint64_t id)
{
  unserializer->object_register_callback (object, id,
                                          unserializer->object_register_data);
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
      rig_pb_unserializer_collect_error (unserializer,
                                         "Boxed property has no value");
      return;
    }

  if (!pb_boxed->has_type)
    {
      rig_pb_unserializer_collect_error (unserializer,
                                         "Boxed property has no type");
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

  rig_pb_init_boxed_value (unserializer,
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
          rig_pb_unserializer_collect_error (unserializer,
                                             "Unknown property %s for object "
                                             "of type %s",
                                             pb_boxed->name,
                                             rut_object_get_type_name (object));
          continue;
        }

      set_property_from_pb_boxed (unserializer, property, pb_boxed);
    }
}

RutObject *
rig_pb_unserialize_component (RigPBUnSerializer *unserializer,
                              RigEntity *entity,
                              Rig__Entity__Component *pb_component)
{
  uint64_t component_id;

  if (!pb_component->has_id)
    return NULL;

  component_id = pb_component->id;

  if (!pb_component->has_type)
    return NULL;

  switch (pb_component->type)
    {
    case RIG__ENTITY__COMPONENT__TYPE__LIGHT:
      {
        Rig__Entity__Component__Light *pb_light = pb_component->light;
        RigLight *light;

        light = rig_light_new (unserializer->engine->ctx);

        /* XXX: This is only for backwards compatibility... */
        if (!pb_component->properties)
          {
            CoglColor ambient;
            CoglColor diffuse;
            CoglColor specular;

            pb_init_color (unserializer->engine->ctx,
                           &ambient, pb_light->ambient);
            pb_init_color (unserializer->engine->ctx,
                           &diffuse, pb_light->diffuse);
            pb_init_color (unserializer->engine->ctx,
                           &specular, pb_light->specular);

            rig_light_set_ambient (light, &ambient);
            rig_light_set_diffuse (light, &diffuse);
            rig_light_set_specular (light, &specular);
          }

        rig_entity_add_component (entity, light);
        rut_object_unref (light);

        set_properties_from_pb_boxed_values (unserializer,
                                             light,
                                             pb_component->n_properties,
                                             pb_component->properties);

        if (unserializer->light == NULL)
          unserializer->light = rut_object_ref (entity);

        rig_pb_unserializer_register_object (unserializer,
                                             light, component_id);
        return light;
      }
    case RIG__ENTITY__COMPONENT__TYPE__MATERIAL:
      {
        Rig__Entity__Component__Material *pb_material =
          pb_component->material;
        RigMaterial *material;
        CoglColor ambient;
        CoglColor diffuse;
        CoglColor specular;
        RigAsset *asset;

        material = rig_material_new (unserializer->engine->ctx, NULL);

        rig_entity_add_component (entity, material);
        rut_object_unref (material);

#warning "todo: remove Component->Material compatibility"
        if (pb_material)
          {
            if (pb_material->texture &&
                pb_material->texture->has_asset_id)
              {
                Rig__Texture *pb_texture = pb_material->texture;

                asset = unserializer_find_object (unserializer,
                                                  pb_texture->asset_id);
                if (asset)
                  rig_material_set_color_source_asset (material, asset);
                else
                  rig_pb_unserializer_collect_error (unserializer,
                                                     "Invalid asset id");
              }

            if (pb_material->normal_map &&
                pb_material->normal_map->has_asset_id)
              {
                Rig__NormalMap *pb_normal_map = pb_material->normal_map;

                asset = unserializer_find_object (unserializer,
                                                  pb_normal_map->asset_id);
                if (asset)
                  rig_material_set_normal_map_asset (material, asset);
                else
                  rig_pb_unserializer_collect_error (unserializer,
                                                     "Invalid asset id");
              }

            if (pb_material->alpha_mask &&
                pb_material->alpha_mask->has_asset_id)
              {
                Rig__AlphaMask *pb_alpha_mask = pb_material->alpha_mask;

                asset = unserializer_find_object (unserializer,
                                                  pb_alpha_mask->asset_id);
                if (asset)
                  rig_material_set_alpha_mask_asset (material, asset);
                else
                  rig_pb_unserializer_collect_error (unserializer,
                                                     "Invalid asset id");
              }

            pb_init_color (unserializer->engine->ctx,
                           &ambient, pb_material->ambient);
            pb_init_color (unserializer->engine->ctx,
                           &diffuse, pb_material->diffuse);
            pb_init_color (unserializer->engine->ctx,
                           &specular, pb_material->specular);

            rig_material_set_ambient (material, &ambient);
            rig_material_set_diffuse (material, &diffuse);
            rig_material_set_specular (material, &specular);
            if (pb_material->has_shininess)
              rig_material_set_shininess (material, pb_material->shininess);
          }

        set_properties_from_pb_boxed_values (unserializer,
                                             material,
                                             pb_component->n_properties,
                                             pb_component->properties);

        rig_pb_unserializer_register_object (unserializer,
                                             material, component_id);
        return material;
      }
    case RIG__ENTITY__COMPONENT__TYPE__MODEL:
      {
        Rig__Entity__Component__Model *pb_model = pb_component->model;
        RigAsset *asset;
        RutMesh *mesh;
        RigModel *model;

        if (!pb_model->has_asset_id)
          {
            rig_pb_unserializer_collect_error (unserializer,
                                               "Missing asset ID for model");
            return NULL;
          }

        asset = unserializer_find_object (unserializer, pb_model->asset_id);
        if (!asset)
          {
            rig_pb_unserializer_collect_error (unserializer,
                                               "Invalid model asset ID");
            return NULL;
          }

        mesh = rig_asset_get_mesh (asset);
        if (!mesh)
          {
            rig_pb_unserializer_collect_error (unserializer,
                                               "Model component asset "
                                               "isn't a mesh");
            return NULL;
          }

        model = rig_model_new_from_asset (unserializer->engine->ctx, asset);
        if (model)
          {
            rig_entity_add_component (entity, model);
            rut_object_unref (model);
            rig_pb_unserializer_register_object (unserializer,
                                                 model, component_id);
          }
        else
          {
            rig_pb_unserializer_collect_error (unserializer,
                                               "Failed to create model "
                                               "from mesh asset");
          }

        return model;
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

        rig_entity_add_component (entity, text);
        rut_object_unref (text);

        rig_pb_unserializer_register_object (unserializer,
                                             text, component_id);
        return text;
      }
    case RIG__ENTITY__COMPONENT__TYPE__CAMERA:
      {
        Rig__Entity__Component__Camera *pb_camera = pb_component->camera;
        RigCamera *camera =
          rig_camera_new (unserializer->engine,
                          -1, /* ortho/vp width */
                          -1, /* ortho/vp height */
                          NULL);

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

        rig_entity_add_component (entity, camera);
        rut_object_unref (camera);

        rig_pb_unserializer_register_object (unserializer,
                                             camera, component_id);
        return camera;
      }
    case RIG__ENTITY__COMPONENT__TYPE__BUTTON_INPUT:
      {
        RigButtonInput *button_input =
          rig_button_input_new (unserializer->engine->ctx);

        set_properties_from_pb_boxed_values (unserializer,
                                             button_input,
                                             pb_component->n_properties,
                                             pb_component->properties);

        rig_entity_add_component (entity, button_input);
        rut_object_unref (button_input);

        rig_pb_unserializer_register_object (unserializer,
                                             button_input, component_id);
        return button_input;
      }
    case RIG__ENTITY__COMPONENT__TYPE__SHAPE:
      {
        Rig__Entity__Component__Shape *pb_shape = pb_component->shape;
        RigMaterial *material;
        RigAsset *asset = NULL;
        RigShape *shape;
        bool shaped = false;
        int width, height;

        /* XXX: Only for compaibility... */
        if (!pb_component->n_properties)
          {
            if (pb_shape->has_shaped)
              shaped = pb_shape->shaped;

            material = rig_entity_get_component (entity,
                                                 RUT_COMPONENT_TYPE_MATERIAL);

            /* We need to know the size of the texture before we can create
             * a shape component */
            if (material)
              asset = rig_material_get_color_source_asset (material);

            rig_asset_get_image_size (asset, &width, &height);
          }

        shape = rig_shape_new (unserializer->engine->ctx,
                               shaped,
                               width, height);

        set_properties_from_pb_boxed_values (unserializer,
                                             shape,
                                             pb_component->n_properties,
                                             pb_component->properties);

        rig_entity_add_component (entity, shape);
        rut_object_unref (shape);

        rig_pb_unserializer_register_object (unserializer,
                                             shape, component_id);

        return shape;
      }
    case RIG__ENTITY__COMPONENT__TYPE__NINE_SLICE:
      {
        RigNineSlice *nine_slice =
          rig_nine_slice_new (unserializer->engine->ctx,
                              NULL,
                              0, 0, 0, 0, /* left, right, top, bottom */
                              0, 0); /* width, height */
        set_properties_from_pb_boxed_values (unserializer,
                                             nine_slice,
                                             pb_component->n_properties,
                                             pb_component->properties);

        rig_entity_add_component (entity, nine_slice);
        rut_object_unref (nine_slice);

        rig_pb_unserializer_register_object (unserializer,
                                             nine_slice, component_id);

        return nine_slice;
      }
    case RIG__ENTITY__COMPONENT__TYPE__DIAMOND:
      {
        Rig__Entity__Component__Diamond *pb_diamond = pb_component->diamond;
        float diamond_size = 100;
        RigDiamond *diamond;

        if (pb_diamond && pb_diamond->has_size)
          diamond_size = pb_diamond->size;

        diamond = rig_diamond_new (unserializer->engine->ctx, diamond_size);

        rig_entity_add_component (entity, diamond);
        rut_object_unref (diamond);

        set_properties_from_pb_boxed_values (unserializer,
                                             diamond,
                                             pb_component->n_properties,
                                             pb_component->properties);

        rig_pb_unserializer_register_object (unserializer,
                                             diamond, component_id);

        return diamond;
      }
    case RIG__ENTITY__COMPONENT__TYPE__POINTALISM_GRID:
      {
        Rig__Entity__Component__PointalismGrid *pb_grid = pb_component->grid;
        RigPointalismGrid *grid;
        float cell_size;

        if (pb_grid->has_cell_size)
          cell_size = pb_grid->cell_size;
        else
          cell_size = 20;

        grid = rig_pointalism_grid_new (unserializer->engine->ctx, cell_size);

        rig_entity_add_component (entity, grid);
        rut_object_unref (grid);

        /* XXX: Just for compatability... */
        if (pb_grid->has_scale)
          {
            rig_pointalism_grid_set_scale (grid, pb_grid->scale);

            if (pb_grid->has_z)
              rig_pointalism_grid_set_z (grid, pb_grid->z);

            if (pb_grid->has_lighter)
              rig_pointalism_grid_set_lighter (grid, pb_grid->lighter);
          }
        else
          {
            set_properties_from_pb_boxed_values (unserializer,
                                                 grid,
                                                 pb_component->n_properties,
                                                 pb_component->properties);
          }

        rig_pb_unserializer_register_object (unserializer,
                                             grid, component_id);

        return grid;
      }
    case RIG__ENTITY__COMPONENT__TYPE__HAIR:
      {
        RigHair *hair = rig_hair_new (unserializer->engine->ctx);
        RutObject *geom;

        rig_entity_add_component (entity, hair);
        rut_object_unref (hair);

        set_properties_from_pb_boxed_values (unserializer,
                                             hair,
                                             pb_component->n_properties,
                                             pb_component->properties);

        rig_pb_unserializer_register_object (unserializer,
                                             hair, component_id);

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

        geom = rig_entity_get_component (entity,
                                         RUT_COMPONENT_TYPE_GEOMETRY);

        if (geom && rut_object_get_type (geom) == &rig_model_type)
          {
            RigModel *hair_geom = rig_model_new_for_hair (geom);

            rig_entity_remove_component (entity, geom);
            rig_entity_add_component (entity, hair_geom);
            rut_object_unref (hair_geom);
          }

        return hair;
      }
    }

  rig_pb_unserializer_collect_error (unserializer,
                                     "Unknown component type");
  g_warn_if_reached ();

  return NULL;
}

static void
unserialize_components (RigPBUnSerializer *unserializer,
                        RigEntity *entity,
                        Rig__Entity *pb_entity,
                        bool force_material)
{
  int i;

  /* First we add components which don't depend on any other components... */
  for (i = 0; i < pb_entity->n_components; i++)
    {
      Rig__Entity__Component *pb_component = pb_entity->components[i];

      switch (pb_component->type)
        {
        case RIG__ENTITY__COMPONENT__TYPE__LIGHT:
        case RIG__ENTITY__COMPONENT__TYPE__MATERIAL:
        case RIG__ENTITY__COMPONENT__TYPE__MODEL:
        case RIG__ENTITY__COMPONENT__TYPE__TEXT:
        case RIG__ENTITY__COMPONENT__TYPE__CAMERA:
        case RIG__ENTITY__COMPONENT__TYPE__BUTTON_INPUT:
          {
            RutComponent *component =
              rig_pb_unserialize_component (unserializer,
                                            entity,
                                            pb_component);
            if (!component)
              continue;

            /* Note: the component will have been added to the
             * entity which will own a reference and no other
             * reference will have been kept on the component
             */

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
  if (force_material &&
      !rig_entity_get_component (entity, RUT_COMPONENT_TYPE_MATERIAL))
    {
      RigMaterial *material =
        rig_material_new (unserializer->engine->ctx, NULL);

      rig_entity_add_component (entity, material);

      if (pb_entity->has_cast_shadow)
        rig_material_set_cast_shadow (material, pb_entity->cast_shadow);
    }

  for (i = 0; i < pb_entity->n_components; i++)
    {
      Rig__Entity__Component *pb_component = pb_entity->components[i];
      switch (pb_component->type)
        {
        case RIG__ENTITY__COMPONENT__TYPE__SHAPE:
        case RIG__ENTITY__COMPONENT__TYPE__NINE_SLICE:
        case RIG__ENTITY__COMPONENT__TYPE__DIAMOND:
        case RIG__ENTITY__COMPONENT__TYPE__POINTALISM_GRID:
        case RIG__ENTITY__COMPONENT__TYPE__HAIR:
          {
            RutComponent *component =
              rig_pb_unserialize_component (unserializer,
                                            entity,
                                            pb_component);
            if (!component)
              continue;

            /* Note: the component will have been added to the
             * entity which will own a reference and no other
             * reference will have been kept on the component
             */

            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__LIGHT:
        case RIG__ENTITY__COMPONENT__TYPE__MATERIAL:
        case RIG__ENTITY__COMPONENT__TYPE__MODEL:
        case RIG__ENTITY__COMPONENT__TYPE__TEXT:
        case RIG__ENTITY__COMPONENT__TYPE__CAMERA:
        case RIG__ENTITY__COMPONENT__TYPE__BUTTON_INPUT:
          break;
        }
    }
}

RigEntity *
rig_pb_unserialize_entity (RigPBUnSerializer *unserializer,
                           Rig__Entity *pb_entity)
{
  RigEntity *entity;
  uint64_t id;
  bool force_material = false;

  if (!pb_entity->has_id)
    return NULL;

  id = pb_entity->id;
  if (unserializer_find_object (unserializer, id))
    {
      rig_pb_unserializer_collect_error (unserializer,
                                         "Duplicate entity id %d", (int)id);
      return NULL;
    }

  entity = rig_entity_new (unserializer->engine->ctx);

  if (pb_entity->has_parent_id)
    {
      uint64_t parent_id = pb_entity->parent_id;
      RigEntity *parent = unserializer_find_object (unserializer, parent_id);

      if (!parent)
        {
          rig_pb_unserializer_collect_error (unserializer,
                                             "Invalid parent id referenced in "
                                             "entity element");
          rut_object_unref (entity);
          return NULL;
        }

      rut_graphable_add_child (parent, entity);

      /* Now that we know the entity has a parent we can drop our reference on
       * the entity... */
      rut_object_unref (entity);
    }

  if (pb_entity->label)
    rig_entity_set_label (entity, pb_entity->label);

  if (pb_entity->position)
    {
      Rig__Vec3 *pos = pb_entity->position;
      float position[3] = {
          pos->x,
          pos->y,
          pos->z
      };
      rig_entity_set_position (entity, position);
    }
  if (pb_entity->rotation)
    {
      CoglQuaternion q;

      pb_init_quaternion (&q, pb_entity->rotation);

      rig_entity_set_rotation (entity, &q);
    }
  if (pb_entity->has_scale)
    rig_entity_set_scale (entity, pb_entity->scale);

#warning "remove entity::cast_shadow compatibility"
  if (pb_entity->has_cast_shadow)
    force_material = true;

  unserialize_components (unserializer, entity, pb_entity, force_material);

  return entity;
}

static void
unserialize_entities (RigPBUnSerializer *unserializer,
                      int n_entities,
                      Rig__Entity **entities)
{
  int i;

  for (i = 0; i < n_entities; i++)
    {
      RigEntity *entity = rig_pb_unserialize_entity (unserializer, entities[i]);

      if (!entity)
        continue;

      rig_pb_unserializer_register_object (unserializer,
                                           entity, entities[i]->id);

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
      RigAsset *asset = NULL;

      if (!pb_asset->has_id)
        continue;

      id = pb_asset->id;

      if (unserializer->unserialize_asset_callback)
        {
          void *user_data = unserializer->unserialize_asset_data;
          asset = unserializer->unserialize_asset_callback (unserializer,
                                                            pb_asset,
                                                            user_data);
        }
      else if (unserializer_find_object (unserializer, id))
        {
          rig_pb_unserializer_collect_error (unserializer,
                                             "Duplicate asset id %d", (int)id);
          continue;
        }
      else if (pb_asset->path &&
               pb_asset->has_type &&
               pb_asset->has_is_video &&
               pb_asset->has_data)
        {
          asset = rig_asset_new_from_data (engine->ctx,
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
              rig_pb_unserializer_collect_error (unserializer,
                                                 "Error unserializing mesh for "
                                                 "asset id %d",
                                                 (int)id);
              continue;
            }
          asset = rig_asset_new_from_mesh (engine->ctx, mesh);
          rut_object_unref (mesh);
        }
      else if (pb_asset->path &&
               unserializer->engine->ctx->assets_location)
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
          rig_pb_unserializer_register_object (unserializer, asset, id);
        }
      else
        {
          g_warning ("Failed to load \"%s\" asset",
                     pb_asset->path ? pb_asset->path : "");
        }
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

void
rig_pb_unserialize_controller_properties (RigPBUnSerializer *unserializer,
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

      object = unserializer_find_object (unserializer, object_id);
      if (!object)
        {
          rig_pb_unserializer_collect_error (unserializer,
                                             "Invalid object id %d referenced "
                                             "in property element",
                                             (int)object_id);
          continue;
        }

      property = rut_introspectable_lookup_property (object, pb_property->name);

#warning "todo: remove entity::cast_shadow compatibility"
      if (!property &&
          rut_object_get_type (object) == &rig_entity_type &&
          strcmp (pb_property->name, "cast_shadow") == 0)
        {
          object = rig_entity_get_component (object,
                                             RUT_COMPONENT_TYPE_MATERIAL);
          if (object)
            property = rut_introspectable_lookup_property (object,
                                                           pb_property->name);
        }

      if (!property)
        {
          rig_pb_unserializer_collect_error (unserializer,
                                             "Invalid object property name "
                                             "given for controller property");
          continue;
        }

      if (!property->spec->animatable &&
          method != RIG_CONTROLLER_METHOD_CONSTANT)
        {
          rig_pb_unserializer_collect_error (unserializer,
                                             "Can't dynamically control "
                                             "non-animatable property");
          continue;
        }

      rig_controller_add_property (controller, property);

      rig_controller_set_property_method (controller,
                                          property,
                                          method);

      rig_pb_init_boxed_value (unserializer,
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

          rut_object_unref (path);
        }

      if (pb_property->has_binding_id)
        {
          int j;
          RigBinding *binding =
            rig_binding_new (unserializer->engine,
                             property,
                             pb_property->binding_id);

          rig_binding_set_expression (binding, pb_property->c_expression);

          for (j = 0; j < pb_property->n_dependencies; j++)
            {
              RutProperty *dependency;
              RutObject *dependency_object;
              Rig__Controller__Property__Dependency *pb_dependency =
                pb_property->dependencies[j];

              if (!pb_dependency->has_object_id)
                {
                  rig_pb_unserializer_collect_error (unserializer,
                                                     "Property dependency with "
                                                     "no object ID");
                  break;
                }

              if (!pb_dependency->name)
                {
                  rig_pb_unserializer_collect_error (unserializer,
                                                     "Property dependency with "
                                                     "no name");
                  break;
                }

              dependency_object =
                unserializer_find_object (unserializer,
                                          pb_dependency->object_id);
              if (!dependency_object)
                {
                  rig_pb_unserializer_collect_error (unserializer,
                                                     "Failed to find dependency "
                                                     "object for property");
                  break;
                }

              dependency =
                rut_introspectable_lookup_property (dependency_object,
                                                    pb_dependency->name);
              if (!dependency)
                {
                  rig_pb_unserializer_collect_error (unserializer,
                                                     "Failed to introspect "
                                                     "dependency object "
                                                     "for binding property");
                  break;
                }

              rig_binding_add_dependency (binding, dependency,
                                          pb_dependency->name);
            }

          if (j != pb_property->n_dependencies)
            {
              rut_object_unref (binding);
              rig_pb_unserializer_collect_error (unserializer,
                                                 "Not able to resolve all "
                                                 "dependencies for "
                                                 "property binding "
                                                 "(skipping)");
              continue;
            }

          rig_controller_set_property_binding (controller, property, binding);
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

RigController *
rig_pb_unserialize_controller_bare (RigPBUnSerializer *unserializer,
                                    Rig__Controller *pb_controller)
{
  const char *name;
  RigController *controller;

  if (pb_controller->name)
    name = pb_controller->name;
  else
    name = "Controller 0";

  controller = rig_controller_new (unserializer->engine, name);

  rig_controller_set_suspended (controller, true);

  if (strcmp (name, "Controller 0"))
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

  return controller;
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
      uint64_t id;

      if (!pb_controller->has_id)
        continue;

      id = pb_controller->id;

      controller = rig_pb_unserialize_controller_bare (unserializer,
                                                       pb_controller);

      unserializer->controllers =
        g_list_prepend (unserializer->controllers, controller);

      if (id)
        rig_pb_unserializer_register_object (unserializer, controller, id);
    }

  for (i = 0; i < n_controllers; i++)
    {
      Rig__Controller *pb_controller = controllers[i];
      RigController *controller;

      if (!pb_controller->has_id)
        continue;

      controller = unserializer_find_object (unserializer, pb_controller->id);
      if (!controller)
        {
          g_warn_if_reached ();
          continue;
        }

      /* Properties controlled by the RigController... */
      rig_pb_unserialize_controller_properties (unserializer,
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
  unserializer->stack = engine->frame_stack;

  unserializer->object_register_callback =
    default_unserializer_register_object_cb;
  unserializer->object_register_data = unserializer;
  unserializer->object_unregister_callback =
    default_unserializer_unregister_object_cb;
  unserializer->object_unregister_data = unserializer;

  return unserializer;
}

void
rig_pb_unserializer_set_stack (RigPBUnSerializer *unserializer,
                               RutMemoryStack *stack)
{
  unserializer->stack = stack;
}


void
rig_pb_unserializer_set_object_register_callback (
                                 RigPBUnSerializer *unserializer,
                                 RigPBUnSerializerObjectRegisterCallback callback,
                                 void *user_data)
{
  unserializer->object_register_callback = callback;
  unserializer->object_register_data = user_data;
}

void
rig_pb_unserializer_set_object_unregister_callback (
                                 RigPBUnSerializer *unserializer,
                                 RigPBUnSerializerObjectUnRegisterCallback callback,
                                 void *user_data)
{
  unserializer->object_unregister_callback = callback;
  unserializer->object_unregister_data = user_data;
}

void
rig_pb_unserializer_set_id_to_object_callback (
                                     RigPBUnSerializer *unserializer,
                                     RigPBUnSerializerIDToObjecCallback callback,
                                     void *user_data)
{
  unserializer->id_to_object_callback = callback;
  unserializer->id_to_object_data = user_data;
}

void
rig_pb_unserializer_set_asset_unserialize_callback (
                                     RigPBUnSerializer *unserializer,
                                     RigPBUnSerializerAssetCallback callback,
                                     void *user_data)
{
  unserializer->unserialize_asset_callback = callback;
  unserializer->unserialize_asset_data = user_data;
}

void
rig_pb_unserializer_destroy (RigPBUnSerializer *unserializer)
{
  if (unserializer->id_to_object_map)
    g_hash_table_destroy (unserializer->id_to_object_map);
}

RigUI *
rig_pb_unserialize_ui (RigPBUnSerializer *unserializer,
                       const Rig__UI *pb_ui)
{
  RigUI *ui = rig_ui_new (unserializer->engine);
  RigEngine *engine = unserializer->engine;
  GList *l;

  unserialize_assets (unserializer,
                      pb_ui->n_assets,
                      pb_ui->assets);

  unserialize_entities (unserializer,
                        pb_ui->n_entities,
                        pb_ui->entities);

  unserialize_controllers (unserializer,
                           pb_ui->n_controllers,
                           pb_ui->controllers);

  ui->scene = rut_graph_new (engine->ctx);
  for (l = unserializer->entities; l; l = l->next)
    {
      //g_print ("unserialized entiy %p\n", l->data);
      if (rut_graphable_get_parent (l->data) == NULL)
        {
          rut_graphable_add_child (ui->scene, l->data);

          /* Now that the entity has a parent we can drop our
           * reference on it... */
          rut_object_unref (l->data);
          //g_print ("%p added to scene %p\n", l->data, ui->scene);
        }
    }
  unserializer->entities = NULL;

  ui->light = unserializer->light;
  unserializer->light = NULL;

  ui->controllers = unserializer->controllers;
  unserializer->controllers = NULL;

  g_print ("unserialized ui assets list  %p\n", unserializer->assets);
  ui->assets = unserializer->assets;
  unserializer->assets = NULL;

  /* Make sure the ui is complete, in case anything was missing from what we
   * loaded... */
  rig_ui_prepare (ui);

  if (pb_ui->has_dso)
    rig_ui_set_dso_data (ui, pb_ui->dso.data, pb_ui->dso.len);

  return ui;
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
    rut_object_unref (attributes[i]);

  /* The attributes will take their own references on the buffers */
  for (i = 0; i < n_buffers; i++)
    rut_object_unref (named_buffers[i].buffer);

  return mesh;

ERROR:

  g_warn_if_reached ();

  if (mesh)
    rut_object_unref (mesh);

  for (i = 0; i < n_attributes; i++)
    rut_object_unref (attributes[i]);

  for (i = 0; i < n_buffers; i++)
    rut_object_unref (named_buffers[i].buffer);

  return NULL;
}
