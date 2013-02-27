#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "rig.pb-c.h"
#include "rig-data.h"

typedef struct _SaveState
{
  RigData *data;

  int n_pb_entities;
  GList *pb_entities;

  int n_pb_components;
  GList *pb_components;

  int n_pb_properties;
  GList *pb_properties;

  int next_id;
  GHashTable *id_map;
} SaveState;

typedef void (*PBMessageInitFunc) (void *message);

static void *
pb_new (RigData *data,
        size_t size,
        void *_message_init)
{
  PBMessageInitFunc message_init = _message_init;

  void *msg = rut_memory_stack_alloc (data->serialization_stack, size);
  message_init (msg);
  return msg;
}

static Rig__Color *
pb_color_new (RigData *data, const CoglColor *color)
{
  Rig__Color *pb_color = pb_new (data, sizeof (Rig__Color), rig__color__init);
  pb_color->hex = rut_memory_stack_alloc (data->serialization_stack,
                                          10); /* #rrggbbaa */
  snprintf (pb_color->hex, 10, "#%02x%02x%02x%02x",
            cogl_color_get_red_byte (color),
            cogl_color_get_green_byte (color),
            cogl_color_get_blue_byte (color),
            cogl_color_get_alpha_byte (color));

  return pb_color;
}

static Rig__Rotation *
pb_rotation_new (RigData *data, const CoglQuaternion *quaternion)
{
  Rig__Rotation *pb_rotation =
    pb_new (data, sizeof (Rig__Rotation), rig__rotation__init);
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
pb_vec3_new (RigData *data,
             float x,
             float y,
             float z)
{
  Rig__Vec3 *pb_vec3 = pb_new (data, sizeof (Rig__Vec3), rig__vec3__init);

  pb_vec3->x = x;
  pb_vec3->y = y;
  pb_vec3->z = z;

  return pb_vec3;
}

static Rig__Vec4 *
pb_vec4_new (RigData *data,
             float x,
             float y,
             float z,
             float w)
{
  Rig__Vec4 *pb_vec4 = pb_new (data, sizeof (Rig__Vec4), rig__vec4__init);

  pb_vec4->x = x;
  pb_vec4->y = y;
  pb_vec4->z = z;
  pb_vec4->w = w;

  return pb_vec4;
}

static void
register_save_object (SaveState *state,
                      void *object,
                      uint64_t id)
{
  uint64_t *id_value = g_slice_new (uint64_t);

  *id_value = id;

  g_return_if_fail (id != 0);

  if (g_hash_table_lookup (state->id_map, object))
    {
      g_critical ("Duplicate save object id %d", (int)id);
      return;
    }

  g_hash_table_insert (state->id_map, object, id_value);
}

uint64_t
saver_lookup_object_id (SaveState *state, void *object)
{
  uint64_t *id = g_hash_table_lookup (state->id_map, object);

  g_warn_if_fail (id);

  return *id;
}

static void
save_component_cb (RutComponent *component,
                   void *user_data)
{
  const RutType *type = rut_object_get_type (component);
  SaveState *state = user_data;
  RigData *data = state->data;
  int component_id;
  Rig__Entity__Component *pb_component;

  pb_component =
    rut_memory_stack_alloc (data->serialization_stack,
                            sizeof (Rig__Entity__Component));
  rig__entity__component__init (pb_component);

  state->n_pb_components++;
  state->pb_components = g_list_prepend (state->pb_components, pb_component);

  register_save_object (state, component, state->next_id);
  component_id = state->next_id++;

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
      pb_light = pb_new (data,
                         sizeof (Rig__Entity__Component__Light),
                         rig__entity__component__light__init);
      pb_component->light = pb_light;

      pb_light->ambient = pb_color_new (data, ambient);
      pb_light->diffuse = pb_color_new (data, diffuse);
      pb_light->specular = pb_color_new (data, specular);
    }
  else if (type == &rut_material_type)
    {
      RutMaterial *material = RUT_MATERIAL (component);
      RutAsset *asset;
      const CoglColor *ambient = rut_material_get_ambient (material);
      const CoglColor *diffuse = rut_material_get_diffuse (material);
      const CoglColor *specular = rut_material_get_specular (material);
      Rig__Entity__Component__Material *pb_material;

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__MATERIAL;

      pb_material = pb_new (data,
                            sizeof (Rig__Entity__Component__Material),
                            rig__entity__component__material__init);
      pb_component->material = pb_material;

      pb_material->ambient = pb_color_new (data, ambient);
      pb_material->diffuse = pb_color_new (data, diffuse);
      pb_material->specular = pb_color_new (data, specular);

      pb_material->has_shininess = TRUE;
      pb_material->shininess = rut_material_get_shininess (material);

      asset = rut_material_get_texture_asset (material);
      if (asset)
        {
          uint64_t id = saver_lookup_object_id (state, asset);

          g_warn_if_fail (id != 0);

          if (id)
            {
              pb_material->texture = pb_new (data,
                                             sizeof (Rig__Texture),
                                             rig__texture__init);
              pb_material->texture->has_asset_id = TRUE;
              pb_material->texture->asset_id = id;
            }
        }

      asset = rut_material_get_normal_map_asset (material);
      if (asset)
        {
          uint64_t id = saver_lookup_object_id (state, asset);
          if (id)
            {
              pb_material->normal_map = pb_new (data,
                                                sizeof (Rig__NormalMap),
                                                rig__normal_map__init);
              pb_material->normal_map->asset_id = id;
            }
        }

      asset = rut_material_get_alpha_mask_asset (material);
      if (asset)
        {
          uint64_t id = saver_lookup_object_id (state, asset);
          if (id)
            {
              pb_material->alpha_mask = pb_new (data,
                                                sizeof (Rig__AlphaMask),
                                                rig__alpha_mask__init);
              pb_material->alpha_mask->asset_id = id;
            }
        }
    }
  else if (type == &rut_shape_type)
    {
      CoglBool shaped = rut_shape_get_shaped (RUT_SHAPE (component));

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__SHAPE;
      pb_component->shape = pb_new (data,
                                    sizeof (Rig__Entity__Component__Shape),
                                    rig__entity__component__shape__init);
      pb_component->shape->has_shaped = TRUE;
      pb_component->shape->shaped = shaped;
    }
  else if (type == &rut_diamond_type)
    {
      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__DIAMOND;
      pb_component->diamond = pb_new (data,
                                      sizeof (Rig__Entity__Component__Diamond),
                                      rig__entity__component__diamond__init);
      pb_component->diamond->has_size = TRUE;
      pb_component->diamond->size = rut_diamond_get_size (RUT_DIAMOND (component));
    }
  else if (type == &rut_model_type)
    {
      RutModel *model = RUT_MODEL (component);
      RutAsset *asset = rut_model_get_asset (model);
      uint64_t asset_id = saver_lookup_object_id (state, asset);

      /* XXX: we don't support serializing a model loaded from a RutMesh */
      g_warn_if_fail (asset_id != 0);

      pb_component->type = RIG__ENTITY__COMPONENT__TYPE__MODEL;
      pb_component->model = pb_new (data,
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
      pb_text = pb_new (data,
                        sizeof (Rig__Entity__Component__Text),
                        rig__entity__component__text__init);
      pb_component->text = pb_text;

      color = rut_text_get_color (text);

      pb_text->text = rut_text_get_text (text);
      pb_text->font = rut_text_get_font_name (text);
      pb_text->color = pb_color_new (data, color);
    }
}

static RutTraverseVisitFlags
_rut_entitygraph_pre_save_cb (RutObject *object,
                              int depth,
                              void *user_data)
{
  SaveState *state = user_data;
  RigData *data = state->data;
  const RutType *type = rut_object_get_type (object);
  RutObject *parent = rut_graphable_get_parent (object);
  RutEntity *entity;
  const CoglQuaternion *q;
  float angle;
  float axis[3];
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


  pb_entity = rut_memory_stack_alloc (data->serialization_stack,
                                      sizeof (Rig__Entity));
  rig__entity__init (pb_entity);

  state->n_pb_entities++;
  state->pb_entities = g_list_prepend (state->pb_entities, pb_entity);

  register_save_object (state, entity, state->next_id);
  pb_entity->has_id = TRUE;
  pb_entity->id = state->next_id++;

  if (parent && rut_object_get_type (parent) == &rut_entity_type)
    {
      uint64_t id = saver_lookup_object_id (state, parent);
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
      pb_entity->label = label;
    }

  q = rut_entity_get_rotation (entity);

  position = rut_memory_stack_alloc (data->serialization_stack,
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

  pb_entity->rotation = pb_rotation_new (data, q);

  pb_entity->has_cast_shadow = TRUE;
  pb_entity->cast_shadow = rut_entity_get_cast_shadow (entity);

  angle = cogl_quaternion_get_rotation_angle (q);
  cogl_quaternion_get_rotation_axis (q, axis);

  state->n_pb_components = 0;
  state->pb_components = NULL;
  rut_entity_foreach_component (entity,
                                save_component_cb,
                                state);

  pb_entity->n_components = state->n_pb_components;
  pb_entity->components =
    rut_memory_stack_alloc (data->serialization_stack,
                            sizeof (void *) * pb_entity->n_components);

  for (i = 0, l = state->pb_components; l; i++, l = l->next)
    pb_entity->components[i] = l->data;
  g_list_free (state->pb_components);

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static Rig__Path *
pb_path_new (RigData *data, RigPath *path)
{
  Rig__Path *pb_path = pb_new (data, sizeof (Rig__Path), rig__path__init);
  RigNode *node;
  int i;

  if (!path->length)
    return pb_path;

  pb_path->nodes = rut_memory_stack_alloc (data->serialization_stack,
                                           sizeof (void *) * path->length);
  pb_path->n_nodes = path->length;

  i = 0;
  rut_list_for_each (node, &path->nodes, list_node)
    {
      Rig__Node *pb_node =
        pb_new (data, sizeof (Rig__Node), rig__node__init);

      pb_path->nodes[i] = pb_node;

      pb_node->has_t = TRUE;
      pb_node->t = node->t;

      pb_node->value = pb_new (data,
                               sizeof (Rig__PropertyValue),
                               rig__property_value__init);

      switch (path->type)
        {
        case RUT_PROPERTY_TYPE_FLOAT:
            {
              RigNodeFloat *float_node = (RigNodeFloat *) node;
              pb_node->value->has_float_value = TRUE;
              pb_node->value->float_value = float_node->value;
              break;
            }
        case RUT_PROPERTY_TYPE_DOUBLE:
            {
              RigNodeDouble *double_node = (RigNodeDouble *) node;
              pb_node->value->has_double_value = TRUE;
              pb_node->value->double_value = double_node->value;
              break;
            }
        case RUT_PROPERTY_TYPE_VEC3:
            {
              RigNodeVec3 *vec3_node = (RigNodeVec3 *) node;
              pb_node->value->vec3_value = pb_vec3_new (data,
                                                        vec3_node->value[0],
                                                        vec3_node->value[1],
                                                        vec3_node->value[2]);
              break;
            }
        case RUT_PROPERTY_TYPE_VEC4:
            {
              RigNodeVec4 *vec4_node = (RigNodeVec4 *) node;
              pb_node->value->vec4_value = pb_vec4_new (data,
                                                        vec4_node->value[0],
                                                        vec4_node->value[1],
                                                        vec4_node->value[2],
                                                        vec4_node->value[3]);
              break;
            }
        case RUT_PROPERTY_TYPE_COLOR:
            {
              RigNodeColor *color_node = (RigNodeColor *) node;
              pb_node->value->color_value =
                pb_color_new (data, &color_node->value);
              break;
            }
        case RUT_PROPERTY_TYPE_QUATERNION:
            {
              RigNodeQuaternion *quaternion_node = (RigNodeQuaternion *) node;
              pb_node->value->quaternion_value =
                pb_rotation_new (data, &quaternion_node->value);
              break;
            }
        case RUT_PROPERTY_TYPE_INTEGER:
            {
              RigNodeInteger *integer_node = (RigNodeInteger *) node;
              pb_node->value->has_integer_value = TRUE;
              pb_node->value->integer_value = integer_node->value;
              continue;
              break;
            }
        case RUT_PROPERTY_TYPE_UINT32:
            {
              RigNodeUint32 *uint32_node = (RigNodeUint32 *) node;
              pb_node->value->has_uint32_value = TRUE;
              pb_node->value->uint32_value = uint32_node->value;
              break;
            }

          /* These types of properties can't be interoplated so they
           * probably shouldn't end up in a path */
        case RUT_PROPERTY_TYPE_ENUM:
        case RUT_PROPERTY_TYPE_BOOLEAN:
        case RUT_PROPERTY_TYPE_TEXT:
        case RUT_PROPERTY_TYPE_OBJECT:
        case RUT_PROPERTY_TYPE_POINTER:
          g_warn_if_reached ();
          break;
        }

      i++;
    }

  return pb_path;
}

static Rig__PropertyValue *
pb_property_value_new (RigData *data,
                       const RutBoxed *value)
{
  Rig__PropertyValue *pb_value =
    pb_new (data, sizeof (Rig__PropertyValue), rig__property_value__init);

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
        pb_rotation_new (data, &value->d.quaternion_val);
      break;

    case RUT_PROPERTY_TYPE_VEC3:
      pb_value->vec3_value = pb_vec3_new (data,
                                          value->d.vec3_val[0],
                                          value->d.vec3_val[1],
                                          value->d.vec3_val[2]);
      break;

    case RUT_PROPERTY_TYPE_VEC4:
      pb_value->vec4_value = pb_vec4_new (data,
                                          value->d.vec4_val[0],
                                          value->d.vec4_val[1],
                                          value->d.vec4_val[2],
                                          value->d.vec4_val[3]);
      break;

    case RUT_PROPERTY_TYPE_COLOR:
      pb_value->color_value = pb_color_new (data, &value->d.color_val);
      break;

    case RUT_PROPERTY_TYPE_ENUM:
      /* XXX: this should possibly save the string names rather than
       * the integer value? */
      pb_value->has_enum_value = TRUE;
      pb_value->enum_value = value->d.enum_val;
      break;

    case RUT_PROPERTY_TYPE_OBJECT:
    case RUT_PROPERTY_TYPE_POINTER:
      g_warn_if_reached ();
      break;
    }

  return pb_value;
}

static void
save_property_cb (RigTransitionPropData *prop_data,
                  void *user_data)
{
  SaveState *state = user_data;
  RigData *data = state->data;
  RutObject *object;
  uint64_t id;

  Rig__Transition__Property *pb_property =
    pb_new (data,
            sizeof (Rig__Transition__Property),
            rig__transition__property__init);

  state->n_pb_properties++;
  state->pb_properties = g_list_prepend (state->pb_properties, pb_property);

  object = prop_data->property->object;

  id = saver_lookup_object_id (state, object);
  if (!id)
    g_warning ("Failed to find id of object\n");


  pb_property->has_object_id = TRUE;
  pb_property->object_id = id;

  pb_property->name = prop_data->property->spec->name;

  pb_property->has_animated = TRUE;
  pb_property->animated = prop_data->animated;

  pb_property->constant = pb_property_value_new (data, &prop_data->constant_value);

  if (prop_data->path && prop_data->path->length)
    pb_property->path = pb_path_new (data, prop_data->path);
}

static void
free_id_slice (void *id)
{
  g_slice_free (uint64_t, id);
}

void
rig_save (RigData *data,
          const char *path)
{
  struct stat sb;
  SaveState state;
  GList *l;
  int i;

  Rig__UI ui = RIG__UI__INIT;
  Rig__Device device = RIG__DEVICE__INIT;

  memset (&state, 0, sizeof (state));
  rut_memory_stack_rewind (data->serialization_stack);

  if (stat (data->ctx->assets_location, &sb) == -1)
    mkdir (data->ctx->assets_location, 0777);

  state.data = data;

  /* This hash table maps object pointers to uint64_t ids while saving */
  state.id_map = g_hash_table_new_full (NULL, /* direct hash */
                                        NULL, /* direct key equal */
                                        NULL,
                                        free_id_slice);

  /* NB: We have to reserve 0 here so we can tell if lookups into the
   * id_map fail. */
  state.next_id = 1;

  ui.device = &device;

  device.has_width = TRUE;
  device.width = data->device_width;
  device.has_height = TRUE;
  device.height = data->device_height;
  device.background = pb_color_new (data, &data->background_color);

  /* Assets */

  ui.n_assets = g_list_length (data->assets);
  if (ui.n_assets)
    {
      int i;
      ui.assets = rut_memory_stack_alloc (data->serialization_stack,
                                          ui.n_assets * sizeof (void *));
      for (i = 0, l = data->assets; l; i++, l = l->next)
        {
          RutAsset *asset = l->data;
          Rig__Asset *pb_asset =
            pb_new (data, sizeof (Rig__Asset), rig__asset__init);

          register_save_object (&state, asset, state.next_id);
          pb_asset->has_id = TRUE;
          pb_asset->id = state.next_id++;

          pb_asset->path = rut_asset_get_path (asset);

          ui.assets[i] = pb_asset;
        }
    }


  state.n_pb_entities = 0;
  rut_graphable_traverse (data->scene,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          _rut_entitygraph_pre_save_cb,
                          NULL,
                          &state);

  ui.n_entities = state.n_pb_entities;
  ui.entities = rut_memory_stack_alloc (data->serialization_stack,
                                        sizeof (void *) * ui.n_entities);
  for (i = 0, l = state.pb_entities; l; i++, l = l->next)
    ui.entities[i] = l->data;
  g_list_free (state.pb_entities);

  ui.n_transitions = g_list_length (data->transitions);
  if (ui.n_transitions)
    {
      ui.transitions =
        rut_memory_stack_alloc (data->serialization_stack,
                                sizeof (void *) * ui.n_transitions);

      for (i = 0, l = data->transitions; l; i++, l = l->next)
        {
          RigTransition *transition = l->data;
          Rig__Transition *pb_transition =
            pb_new (data, sizeof (Rig__Transition), rig__transition__init);
          GList *l2;
          int j;

          ui.transitions[i] = pb_transition;

          pb_transition->has_id = TRUE;
          pb_transition->id = transition->id;

          state.n_pb_properties = 0;
          state.pb_properties = NULL;
          rig_transition_foreach_property (transition,
                                           save_property_cb,
                                           &state);

          pb_transition->n_properties = state.n_pb_properties;
          pb_transition->properties =
            rut_memory_stack_alloc (data->serialization_stack,
                                    sizeof (void *) * pb_transition->n_properties);
          for (j = 0, l2 = state.pb_properties; l2; j++, l2 = l2->next)
            pb_transition->properties[j] = l2->data;
          g_list_free (state.pb_properties);
        }
    }

  {
    size_t size = rig__ui__get_packed_size (&ui);
    uint8_t *buf = g_malloc (size);
    char *rig_filename = g_strconcat (path, ".rig", NULL);
    FILE *test = fopen (rig_filename, "w");

    g_free (rig_filename);

    rig__ui__pack (&ui, buf);

    fwrite (buf, size, 1, test);
    fflush (test);
    fclose (test);
    g_free (buf);
  }

  g_print ("File Saved\n");

  g_hash_table_destroy (state.id_map);
}

enum {
  LOADER_STATE_NONE,
  LOADER_STATE_LOADING_ENTITY,
  LOADER_STATE_LOADING_MATERIAL_COMPONENT,
  LOADER_STATE_LOADING_MODEL_COMPONENT,
  LOADER_STATE_LOADING_SHAPE_COMPONENT,
  LOADER_STATE_LOADING_DIAMOND_COMPONENT,
  LOADER_STATE_LOADING_LIGHT_COMPONENT,
  LOADER_STATE_LOADING_CAMERA_COMPONENT,
  LOADER_STATE_LOADING_TRANSITION,
  LOADER_STATE_LOADING_PROPERTY,
  LOADER_STATE_LOADING_CONSTANT,
  LOADER_STATE_LOADING_PATH
};

typedef struct _Loader
{
  RigData *data;

  GList *assets;
  GList *entities;
  RutEntity *light;
  GList *transitions;

  GHashTable *id_map;

  /* Only used by XML loader... */
  GQueue state;
  uint32_t id;
  CoglBool texture_specified;
  CoglBool normal_map_specified;
  CoglBool alpha_mask_specified;
  uint32_t texture_asset_id;
  uint32_t normal_map_asset_id;
  uint32_t alpha_mask_asset_id;

  CoglBool device_found;
  int device_width;
  int device_height;
  CoglColor background;
  CoglBool background_set;

  int component_id;

  CoglColor material_ambient;
  CoglBool ambient_set;
  CoglColor material_diffuse;
  CoglBool diffuse_set;
  CoglColor material_specular;
  CoglBool specular_set;
  float material_shininess;
  CoglBool shininess_set;

  CoglBool shaped;
  float diamond_size;
  RutEntity *current_entity;
  CoglBool is_light;

  RigTransition *current_transition;
  RigTransitionPropData *current_property;
  RigPath *current_path;
} Loader;

static void
loader_push_state (Loader *loader, int state)
{
  g_queue_push_tail (&loader->state, GINT_TO_POINTER (state));
}

static int
loader_get_state (Loader *loader)
{
  void *state = g_queue_peek_tail (&loader->state);
  return GPOINTER_TO_INT (state);
}

static void
loader_pop_state (Loader *loader)
{
  g_queue_pop_tail (&loader->state);
}

static RutEntity *
loader_find_entity (Loader *loader, uint64_t id)
{
  RutObject *object = g_hash_table_lookup (loader->id_map, &id);
  if (object == NULL || rut_object_get_type (object) != &rut_entity_type)
    return NULL;
  return RUT_ENTITY (object);
}

static RutAsset *
loader_find_asset (Loader *loader, uint64_t id)
{
  RutObject *object = g_hash_table_lookup (loader->id_map, &id);
  if (object == NULL || rut_object_get_type (object) != &rut_asset_type)
    return NULL;
  return RUT_ASSET (object);
}

static RutEntity *
loader_find_introspectable (Loader *loader, uint64_t id)
{
  RutObject *object = g_hash_table_lookup (loader->id_map, &id);
  if (object == NULL ||
      !rut_object_is (object, RUT_INTERFACE_ID_INTROSPECTABLE) ||
      !rut_object_is (object, RUT_INTERFACE_ID_REF_COUNTABLE))
    return NULL;
  return RUT_ENTITY (object);
}

static CoglBool
load_float (float *value,
            const char *str)
{
  *value = g_ascii_strtod (str, NULL);
  return TRUE;
}

static CoglBool
load_double (double *value,
             const char *str)
{
  *value = g_ascii_strtod (str, NULL);
  return TRUE;
}

static CoglBool
load_integer (int *value,
              const char *str)
{
  *value = g_ascii_strtoll (str, NULL, 10);
  return TRUE;
}

static CoglBool
load_uint32 (uint32_t *value,
             const char *str)
{
  *value = g_ascii_strtoull (str, NULL, 10);
  return TRUE;
}

static CoglBool
load_boolean (CoglBool *value,
              const char *str)
{
  /* Borrowed from gmarkup.c in glib */
  char const * const falses[] = { "false", "f", "no", "n", "0" };
  char const * const trues[] = { "true", "t", "yes", "y", "1" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (falses); i++)
    {
      if (g_ascii_strcasecmp (str, falses[i]) == 0)
        {
          *value = FALSE;

          return TRUE;
        }
    }

  for (i = 0; i < G_N_ELEMENTS (trues); i++)
    {
      if (g_ascii_strcasecmp (str, trues[i]) == 0)
        {
          *value = TRUE;

          return TRUE;
        }
    }

  return FALSE;
}

static CoglBool
load_text (char **value,
           const char *str)
{
  *value = g_strdup (str);
  return TRUE;
}

static CoglBool
load_vec3 (float *value,
           const char *str)
{
  return sscanf (str, "(%f, %f, %f)",
                 &value[0], &value[1], &value[2]) == 3;
}

static CoglBool
load_vec4 (float *value,
           const char *str)
{
  return sscanf (str, "(%f, %f, %f, %f)",
                 &value[0], &value[1], &value[2], &value[3]) == 4;
}

static CoglBool
load_color (CoglColor *value,
            const char *str)
{
  return sscanf (str, "(%f, %f, %f, %f)",
                 &value->red, &value->green, &value->blue, &value->alpha) == 4;
}

static CoglBool
load_quaternion (CoglQuaternion *value,
                 const char *str)
{
  float angle, x, y, z;

  if (sscanf (str, "[%f (%f, %f, %f)]", &angle, &x, &y, &z) != 4)
    return FALSE;

  cogl_quaternion_init (value, angle, x, y, z);

  return TRUE;
}

static CoglBool
load_boxed_value (RutBoxed *value,
                  RutPropertyType type,
                  const char *str,
                  GError **error)
{
  value->type = type;

  switch (type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      if (!load_float (&value->d.float_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_DOUBLE:
      if (!load_double (&value->d.double_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_INTEGER:
      if (!load_integer (&value->d.integer_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_ENUM:
      /* FIXME: this should probably load the string name rather than
       * the integer value */
      if (!load_integer (&value->d.enum_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_UINT32:
      if (!load_uint32 (&value->d.uint32_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_BOOLEAN:
      if (!load_boolean (&value->d.boolean_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_TEXT:
      if (!load_text (&value->d.text_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_QUATERNION:
      if (!load_quaternion (&value->d.quaternion_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_VEC3:
      if (!load_vec3 (value->d.vec3_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_VEC4:
      if (!load_vec4 (value->d.vec4_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_COLOR:
      if (!load_color (&value->d.color_val, str))
        goto error;
      return TRUE;

    case RUT_PROPERTY_TYPE_OBJECT:
    case RUT_PROPERTY_TYPE_POINTER:
      break;
    }

  g_warn_if_reached ();

 error:
  g_set_error (error,
               G_MARKUP_ERROR,
               G_MARKUP_ERROR_INVALID_CONTENT,
               "Invalid value encountered");

  return FALSE;
}

static CoglBool
load_path_node (RigPath *path,
                float t,
                const char *value_str,
                GError **error)
{
  switch (path->type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      {
        float value;
        if (!load_float (&value, value_str))
          goto error;
        rig_path_insert_float (path, t, value);
        return TRUE;
      }
    case RUT_PROPERTY_TYPE_DOUBLE:
      {
        double value;
        if (!load_double (&value, value_str))
          goto error;
        rig_path_insert_double (path, t, value);
        return TRUE;
      }
    case RUT_PROPERTY_TYPE_INTEGER:
      {
        int value;
        if (!load_integer (&value, value_str))
          goto error;
        rig_path_insert_integer (path, t, value);
        return TRUE;
      }
    case RUT_PROPERTY_TYPE_UINT32:
      {
        uint32_t value;
        if (!load_uint32 (&value, value_str))
          goto error;
        rig_path_insert_uint32 (path, t, value);
        return TRUE;
      }
    case RUT_PROPERTY_TYPE_VEC3:
      {
        float value[3];
        if (!load_vec3 (value, value_str))
          goto error;
        rig_path_insert_vec3 (path, t, value);
        return TRUE;
      }
    case RUT_PROPERTY_TYPE_VEC4:
      {
        float value[4];
        if (!load_vec4 (value, value_str))
          goto error;
        rig_path_insert_vec4 (path, t, value);
        return TRUE;
      }
    case RUT_PROPERTY_TYPE_COLOR:
      {
        CoglColor value;
        if (!load_color (&value, value_str))
          goto error;
        rig_path_insert_color (path, t, &value);
        return TRUE;
      }
    case RUT_PROPERTY_TYPE_QUATERNION:
      {
        CoglQuaternion value;
        if (!load_quaternion (&value, value_str))
          goto error;
        rig_path_insert_quaternion (path, t, &value);
        return TRUE;
      }

      /* These shouldn't be animatable */
    case RUT_PROPERTY_TYPE_BOOLEAN:
    case RUT_PROPERTY_TYPE_TEXT:
    case RUT_PROPERTY_TYPE_ENUM:
    case RUT_PROPERTY_TYPE_OBJECT:
    case RUT_PROPERTY_TYPE_POINTER:
      g_warn_if_reached ();
      goto error;
    }

  g_warn_if_reached ();

 error:
  g_set_error (error,
               G_MARKUP_ERROR,
               G_MARKUP_ERROR_INVALID_CONTENT,
               "Invalid value encountered");

  return FALSE;
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

static void
collect_error (Loader *loader,
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

  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, format, ap);

  va_end (ap);
}

static void
register_loader_object (Loader *loader,
                        void *object,
                        uint64_t id)
{
  uint64_t *key = g_slice_new (uint64_t);

  *key = id;

  g_return_if_fail (id != 0);

  if (g_hash_table_lookup (loader->id_map, key))
    {
      collect_error (loader, "Duplicate loader object id %ld", id);
      return;
    }

  g_hash_table_insert (loader->id_map, key, object);
}

static CoglBool
check_and_set_id (Loader *loader,
                  uint64_t id,
                  void *object,
                  GError **error)
{
  /* XXX: XML COMPATIBILITY: The id for components shouldn't be
   * optional. This is to retain compatibility with version 1 of Rig
   * which didn't add an id */
  if (id == 0)
    return TRUE;

  if (g_hash_table_lookup (loader->id_map, &id))
    {
      g_set_error (error,
                   G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   "Duplicate id %d", (int)id);
      return FALSE;
    }

  register_loader_object (loader, object, id);

  return TRUE;
}

static CoglBool
parse_and_set_id (Loader *loader,
                  const char *id_str,
                  void *object,
                  GError **error)
{
  /* FIXME: XML COMPATIBILITY: The id for components shouldn't be
   * optional. This is to retain compatibility with version 1 of Rig
   * which didn't add an id */
  if (id_str)
    {
      int id = g_ascii_strtoull (id_str, NULL, 10);

      return check_and_set_id (loader, id, object, error);
    }
  else
    return TRUE;
}

static void
parse_start_element (GMarkupParseContext *context,
                     const char *element_name,
                     const char **attribute_names,
                     const char **attribute_values,
                     void *user_data,
                     GError **error)
{
  Loader *loader = user_data;
  RigData *data = loader->data;
  int state = loader_get_state (loader);

  if (state == LOADER_STATE_NONE &&
      strcmp (element_name, "device") == 0)
    {
      const char *width_str;
      const char *height_str;
      const char *background_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "width",
                                        &width_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "height",
                                        &height_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "background",
                                        &background_str,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      loader->device_found = TRUE;
      loader->device_width = g_ascii_strtoull (width_str, NULL, 10);
      loader->device_height = g_ascii_strtoull (height_str, NULL, 10);

      if (background_str)
        {
          rut_color_init_from_string (loader->data->ctx,
                                      &loader->background,
                                      background_str);
          loader->background_set = TRUE;
        }
    }
  else if (state == LOADER_STATE_NONE &&
           strcmp (element_name, "asset") == 0)
    {
      const char *id_str;
      const char *path;
      uint64_t id;
      char *full_path;
      GFile *asset_file;
      GFileInfo *info;
      RutAsset *asset = NULL;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "path",
                                        &path,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      id = g_ascii_strtoull (id_str, NULL, 10);
      if (g_hash_table_lookup (loader->id_map, &id))
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Duplicate id %d", (int)id);
          return;
        }

      full_path = g_build_filename (data->ctx->assets_location, path, NULL);
      asset_file = g_file_new_for_path (full_path);
      info = g_file_query_info (asset_file,
                                "standard::*",
                                G_FILE_QUERY_INFO_NONE,
                                NULL,
                                NULL);
      if (info)
        {
          asset = rig_load_asset (data, info, asset_file);
          if (asset)
            {
              loader->assets = g_list_prepend (loader->assets, asset);
              register_loader_object (loader, asset, id);
            }
          g_object_unref (info);
        }

      g_object_unref (asset_file);
      g_free (full_path);
    }
  else if (state == LOADER_STATE_NONE &&
           strcmp (element_name, "entity") == 0)
    {
      const char *id_str;
      uint64_t id;
      RutEntity *entity;
      const char *parent_id_str;
      const char *label_str;
      const char *position_str;
      const char *rotation_str;
      const char *scale_str;
      const char *cast_shadow_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "parent",
                                        &parent_id_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "label",
                                        &label_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "position",
                                        &position_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "rotation",
                                        &rotation_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "scale",
                                        &scale_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "cast_shadow",
                                        &cast_shadow_str,
                                        G_MARKUP_COLLECT_INVALID))
      {
        return;
      }

      id = g_ascii_strtoull (id_str, NULL, 10);
      if (g_hash_table_lookup (loader->id_map, &id))
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Duplicate entity id %d", (int)id);
          return;
        }

      entity = rut_entity_new (loader->data->ctx);

      if (parent_id_str)
        {
          uint64_t parent_id = g_ascii_strtoull (parent_id_str, NULL, 10);
          RutEntity *parent = loader_find_entity (loader, parent_id);

          if (!parent)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid parent id referenced in entity element");
              rut_refable_unref (entity);
              return;
            }

          rut_graphable_add_child (parent, entity);
        }

      if (label_str)
        rut_entity_set_label (entity, label_str);

      if (position_str)
        {
          float pos[3];
          if (sscanf (position_str, "(%f, %f, %f)",
                      &pos[0], &pos[1], &pos[2]) != 3)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid entity position");
              return;
            }
          rut_entity_set_position (entity, pos);
        }
      if (rotation_str)
        {
          float angle;
          float axis[3];
          CoglQuaternion q;

          if (sscanf (rotation_str, "[%f (%f, %f, %f)]",
                      &angle, &axis[0], &axis[1], &axis[2]) != 4)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid entity rotation");
              return;
            }
          cogl_quaternion_init_from_angle_vector (&q, angle, axis);
          rut_entity_set_rotation (entity, &q);
        }
      if (scale_str)
        {
          double scale = g_ascii_strtod (scale_str, NULL);
          rut_entity_set_scale (entity, scale);
        }
      if (cast_shadow_str)
        {
          if (strcmp (cast_shadow_str, "yes") == 0)
            rut_entity_set_cast_shadow (entity, TRUE);
          else if (strcmp (cast_shadow_str, "no") == 0)
            rut_entity_set_cast_shadow (entity, FALSE);
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid cast_shadow value");
              return;
            }
        }

      loader->current_entity = entity;
      loader->is_light = FALSE;

      register_loader_object (loader, entity, id);

      loader_push_state (loader, LOADER_STATE_LOADING_ENTITY);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "material") == 0)
    {
      const char *id_str, *color_str, *ambient_str, *diffuse_str, *specular_str;
      const char *shininess_str;

      loader->texture_specified = FALSE;
      loader->normal_map_specified = FALSE;
      loader->alpha_mask_specified = FALSE;
      loader_push_state (loader, LOADER_STATE_LOADING_MATERIAL_COMPONENT);

      g_markup_collect_attributes (element_name,
                                   attribute_names,
                                   attribute_values,
                                   error,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                   "id",
                                   &id_str,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                   "color",
                                   &color_str,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                   "ambient",
                                   &ambient_str,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                   "diffuse",
                                   &diffuse_str,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                   "specular",
                                   &specular_str,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                   "shininess",
                                   &shininess_str,
                                   G_MARKUP_COLLECT_INVALID);

      loader->component_id = id_str ? g_ascii_strtoull (id_str, NULL, 10) : 0;

      /* XXX: This is a depecated attribute, remove once existing xml
       * files have stopped using it... */
      if (color_str && diffuse_str == NULL)
        diffuse_str = color_str;

      if (ambient_str)
        {
          rut_color_init_from_string (loader->data->ctx,
                                      &loader->material_ambient,
                                      ambient_str);
          loader->ambient_set = TRUE;
        }
      else
        loader->ambient_set = FALSE;

      if (diffuse_str)
        {
          rut_color_init_from_string (loader->data->ctx,
                                      &loader->material_diffuse,
                                      diffuse_str);
          loader->diffuse_set = TRUE;
        }
      else
        loader->diffuse_set = FALSE;

      if (specular_str)
        {
          rut_color_init_from_string (loader->data->ctx,
                                      &loader->material_specular,
                                      specular_str);
          loader->specular_set = TRUE;
        }
      else
        loader->specular_set = FALSE;

      if (shininess_str)
        {
          loader->material_shininess = g_ascii_strtod (shininess_str, NULL);
          loader->shininess_set = TRUE;
        }
      else
        loader->shininess_set = FALSE;
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "light") == 0)
    {
      const char *id_str;
      const char *ambient_str;
      const char *diffuse_str;
      const char *specular_str;
      CoglColor ambient;
      CoglColor diffuse;
      CoglColor specular;
      RutLight *light;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING |
                                        G_MARKUP_COLLECT_OPTIONAL,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "ambient",
                                        &ambient_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "diffuse",
                                        &diffuse_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "specular",
                                        &specular_str,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      rut_color_init_from_string (loader->data->ctx, &ambient, ambient_str);
      rut_color_init_from_string (loader->data->ctx, &diffuse, diffuse_str);
      rut_color_init_from_string (loader->data->ctx, &specular, specular_str);

      light = rut_light_new (loader->data->ctx);
      rut_light_set_ambient (light, &ambient);
      rut_light_set_diffuse (light, &diffuse);
      rut_light_set_specular (light, &specular);

      if (!parse_and_set_id (loader, id_str, light, error))
        return;

      rut_entity_add_component (loader->current_entity, light);

      loader->is_light = TRUE;

      //loader_push_state (loader, LOADER_STATE_LOADING_LIGHT_COMPONENT);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "shape") == 0)
    {
      const char *id_str, *shaped_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING |
                                        G_MARKUP_COLLECT_OPTIONAL,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "shaped",
                                        &shaped_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      loader->component_id = id_str ? g_ascii_strtoull (id_str, NULL, 10) : 0;

      if (strcmp (shaped_str, "true") == 0)
        loader->shaped = TRUE;
      else if (strcmp (shaped_str, "false") == 0)
        loader->shaped = FALSE;
      else
        {
          g_warn_if_reached ();
          loader->shaped = FALSE;
        }

      loader_push_state (loader, LOADER_STATE_LOADING_SHAPE_COMPONENT);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "diamond") == 0)
    {
      const char *id_str, *size_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING |
                                        G_MARKUP_COLLECT_OPTIONAL,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "size",
                                        &size_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      loader->component_id = id_str ? g_ascii_strtoull (id_str, NULL, 10) : 0;

      loader->diamond_size = g_ascii_strtod (size_str, NULL);

      loader_push_state (loader, LOADER_STATE_LOADING_DIAMOND_COMPONENT);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "model") == 0)
    {
      const char *id_str, *asset_id_str;
      uint32_t asset_id;
      RutAsset *asset;
      RutModel *model;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING |
                                        G_MARKUP_COLLECT_OPTIONAL,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "asset",
                                        &asset_id_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      asset_id = g_ascii_strtoull (asset_id_str, NULL, 10);
      asset = loader_find_asset (loader, asset_id);
      if (!asset)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid asset id");
          return;
        }

      model = rut_model_new_from_asset (loader->data->ctx, asset);
      if (model)
        {
          rut_refable_unref (asset);
          rut_entity_add_component (loader->current_entity, model);
        }

      if (!parse_and_set_id (loader, id_str, model, error))
        return;
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "text") == 0)
    {
      const char *id_str, *text_str, *font_str, *color_str;
      RutText *text;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "text",
                                        &text_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "font",
                                        &font_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "color",
                                        &color_str,
                                        G_MARKUP_COLLECT_INVALID))
          return;

      text = rut_text_new_with_text (data->ctx, font_str, text_str);

      if (!parse_and_set_id (loader, id_str, text, error))
        return;

      if (color_str)
        {
          CoglColor color;
          rut_color_init_from_string (data->ctx, &color, color_str);
          rut_text_set_color (text, &color);
        }

      rut_entity_add_component (loader->current_entity, text);
    }
  else if (state == LOADER_STATE_LOADING_MATERIAL_COMPONENT &&
           strcmp (element_name, "texture") == 0)
    {
      const char *id_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "asset",
                                        &id_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      loader->texture_specified = TRUE;
      loader->texture_asset_id = g_ascii_strtoull (id_str, NULL, 10);
    }
  else if (state == LOADER_STATE_LOADING_MATERIAL_COMPONENT &&
           strcmp (element_name, "normal_map") == 0)
    {
      const char *id_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "asset",
                                        &id_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      loader->normal_map_specified = TRUE;
      loader->normal_map_asset_id = g_ascii_strtoull (id_str, NULL, 10);
    }
  else if (state == LOADER_STATE_LOADING_MATERIAL_COMPONENT &&
           strcmp (element_name, "alpha_mask") == 0)
    {
      const char *id_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "asset",
                                        &id_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      loader->alpha_mask_specified = TRUE;
      loader->alpha_mask_asset_id = g_ascii_strtoull (id_str, NULL, 10);
    }
  else if (state == LOADER_STATE_NONE &&
      strcmp (element_name, "transition") == 0)
    {
      const char *id_str;
      uint32_t id;

      loader_push_state (loader, LOADER_STATE_LOADING_TRANSITION);

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      id = g_ascii_strtoull (id_str, NULL, 10);

      loader->current_transition = rig_create_transition (loader->data, id);
      loader->transitions = g_list_prepend (loader->transitions, loader->current_transition);
    }
  else if (state == LOADER_STATE_LOADING_TRANSITION &&
           strcmp (element_name, "property") == 0)
    {
      const char *object_id_str;
      uint32_t object_id;
      RutEntity *object;
      const char *property_name;
      CoglBool animated;
      RigTransitionPropData *prop_data;

      /* FIXME: the entity attribute should be renamed because not
         everything being animated is necessarily an entity */
      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "entity",
                                        &object_id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "name",
                                        &property_name,
                                        G_MARKUP_COLLECT_BOOLEAN,
                                        "animated",
                                        &animated,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      object_id = g_ascii_strtoull (object_id_str, NULL, 10);

      object = loader_find_introspectable (loader, object_id);
      if (!object)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid object id %d referenced in property element",
                       object_id);
          return;
        }

      prop_data = rig_transition_get_prop_data (loader->current_transition,
                                                object,
                                                property_name);

      if (prop_data->property->spec->animatable)
        {
          if (animated)
            rig_transition_set_property_animated (loader->current_transition,
                                                  prop_data->property,
                                                  TRUE);
        }
      else if (animated)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "A non-animatable property is marked as animated");
          return;
        }

      if (!prop_data)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid Entity property referenced in "
                       "property element");
          return;
        }

      loader->current_property = prop_data;

      loader_push_state (loader, LOADER_STATE_LOADING_PROPERTY);
    }
  else if (state == LOADER_STATE_LOADING_PROPERTY &&
           strcmp (element_name, "constant") == 0)
    {
      const char *value_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "value",
                                        &value_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      if (!load_boxed_value (&loader->current_property->constant_value,
                             loader->current_property->constant_value.type,
                             value_str,
                             error))
        return;

      loader_push_state (loader, LOADER_STATE_LOADING_CONSTANT);
    }
  else if (state == LOADER_STATE_LOADING_PROPERTY &&
           strcmp (element_name, "path") == 0)
    {
      loader->current_path =
        rig_path_new (data->ctx,
                      loader->current_property->property->spec->type);

      loader_push_state (loader, LOADER_STATE_LOADING_PATH);
    }
  else if (state == LOADER_STATE_LOADING_PATH &&
           strcmp (element_name, "node") == 0)
    {
      const char *t_str;
      float t;
      const char *value_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "t",
                                        &t_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "value",
                                        &value_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      t = g_ascii_strtod (t_str, NULL);

      if (!load_path_node (loader->current_path,
                           t,
                           value_str,
                           error))
        return;
    }
}

static void
parse_end_element (GMarkupParseContext *context,
                   const char *element_name,
                   void *user_data,
                   GError **error)
{
  Loader *loader = user_data;
  int state = loader_get_state (loader);

  if (state == LOADER_STATE_LOADING_ENTITY &&
      strcmp (element_name, "entity") == 0)
    {
      if (loader->is_light && loader->light == NULL)
        loader->light = rut_refable_ref (loader->current_entity);

      loader->entities =
        g_list_prepend (loader->entities, loader->current_entity);

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_SHAPE_COMPONENT &&
           strcmp (element_name, "shape") == 0)
    {
      RutMaterial *material =
        rut_entity_get_component (loader->current_entity,
                                  RUT_COMPONENT_TYPE_MATERIAL);
      RutAsset *asset = NULL;
      CoglTexture *texture = NULL;
      RutShape *shape;

      /* We need to know the size of the texture before we can create
       * a shape component */
      if (material)
        asset = rut_material_get_texture_asset (material);

      if (asset)
        texture = rut_asset_get_texture (asset);

      if (!texture)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Can't add shape component without a texture");
          return;
        }

      shape = rut_shape_new (loader->data->ctx,
                             loader->shaped,
                             cogl_texture_get_width (texture),
                             cogl_texture_get_height (texture));
      rut_entity_add_component (loader->current_entity,
                                shape);

      if (!check_and_set_id (loader, loader->component_id, shape, error))
        return;

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_DIAMOND_COMPONENT &&
           strcmp (element_name, "diamond") == 0)
    {
      RutMaterial *material =
        rut_entity_get_component (loader->current_entity,
                                  RUT_COMPONENT_TYPE_MATERIAL);
      RutAsset *asset = NULL;
      CoglTexture *texture = NULL;
      RutDiamond *diamond;

      /* We need to know the size of the texture before we can create
       * a diamond component */
      if (material)
        asset = rut_material_get_texture_asset (material);

      if (asset)
        texture = rut_asset_get_texture (asset);

      if (!texture)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Can't add diamond component without a texture");
          return;
        }

      diamond = rut_diamond_new (loader->data->ctx,
                                 loader->diamond_size,
                                 cogl_texture_get_width (texture),
                                 cogl_texture_get_height (texture));
      rut_entity_add_component (loader->current_entity,
                                diamond);

      if (!check_and_set_id (loader, loader->component_id, diamond, error))
        return;

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_MATERIAL_COMPONENT &&
           strcmp (element_name, "material") == 0)
    {
      RutMaterial *material;
      RutAsset *asset;

      material = rut_material_new (loader->data->ctx, NULL);

      if (!check_and_set_id (loader, loader->component_id, material, error))
        return;

      if (loader->texture_specified)
        {
          asset = loader_find_asset (loader, loader->texture_asset_id);
          if (!asset)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid asset id");
              return;
            }
          rut_material_set_texture_asset (material, asset);
        }

      if (loader->normal_map_specified)
        {
          asset = loader_find_asset (loader, loader->normal_map_asset_id);
          if (!asset)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid asset id");
              return;
            }
          rut_material_set_normal_map_asset (material, asset);
        }

      if (loader->alpha_mask_specified)
        {
          asset = loader_find_asset (loader, loader->alpha_mask_asset_id);
          if (!asset)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid asset id");
              return;
            }
          rut_material_set_alpha_mask_asset (material, asset);
        }

      if (loader->ambient_set)
        rut_material_set_ambient (material, &loader->material_ambient);
      if (loader->diffuse_set)
        rut_material_set_diffuse (material, &loader->material_diffuse);
      if (loader->specular_set)
        rut_material_set_specular (material, &loader->material_specular);
      if (loader->shininess_set)
        rut_material_set_shininess (material, loader->material_shininess);

      rut_entity_add_component (loader->current_entity, material);

      loader_pop_state (loader);
    }
  else
  if (state == LOADER_STATE_LOADING_TRANSITION &&
           strcmp (element_name, "transition") == 0)
    {
      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_PROPERTY &&
           strcmp (element_name, "property") == 0)
    {
      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_PATH &&
           strcmp (element_name, "path") == 0)
    {
      g_assert (loader->current_property->path == NULL);
      loader->current_property->path = loader->current_path;
      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_CONSTANT &&
           strcmp (element_name, "constant") == 0)
    {
      loader_pop_state (loader);
    }
}

static void
parse_error (GMarkupParseContext *context,
             GError *error,
             void *user_data)
{

}

static void
update_transition_property_cb (RigTransitionPropData *prop_data,
                               void *user_data)
{
  RigData *data = user_data;

  rig_transition_update_property (data->transitions->data,
                                  prop_data->property);
}

static void
load_components (Loader *loader,
                 RutEntity *entity,
                 Rig__Entity *pb_entity)
{
  int i;

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

            light = rut_light_new (loader->data->ctx);

            pb_init_color (loader->data->ctx, &ambient, pb_light->ambient);
            pb_init_color (loader->data->ctx, &diffuse, pb_light->diffuse);
            pb_init_color (loader->data->ctx, &specular, pb_light->specular);

            rut_light_set_ambient (light, &ambient);
            rut_light_set_diffuse (light, &diffuse);
            rut_light_set_specular (light, &specular);

            rut_entity_add_component (entity, light);

            if (loader->light == NULL)
              loader->light = rut_refable_ref (entity);

            register_loader_object (loader, light, component_id);
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__MATERIAL:
          {
            Rig__Entity__Component__Material *pb_material = pb_component->material;
            RutMaterial *material;
            CoglColor ambient;
            CoglColor diffuse;
            CoglColor specular;
            RutAsset *asset;

            material = rut_material_new (loader->data->ctx, NULL);

            if (pb_material->texture &&
                pb_material->texture->has_asset_id)
              {
                Rig__Texture *pb_texture = pb_material->texture;

                asset = loader_find_asset (loader, pb_texture->asset_id);
                if (!asset)
                  {
                    collect_error (loader, "Invalid asset id");
                    rut_refable_unref (material);
                    break;
                  }
                rut_material_set_texture_asset (material, asset);
              }

            if (pb_material->normal_map &&
                pb_material->normal_map->has_asset_id)
              {
                Rig__NormalMap *pb_normal_map = pb_material->normal_map;

                asset = loader_find_asset (loader, pb_normal_map->asset_id);
                if (!asset)
                  {
                    collect_error (loader, "Invalid asset id");
                    rut_refable_unref (material);
                    break;
                  }
                rut_material_set_normal_map_asset (material, asset);
              }

            if (pb_material->alpha_mask &&
                pb_material->alpha_mask->has_asset_id)
              {
                Rig__AlphaMask *pb_alpha_mask = pb_material->alpha_mask;

                asset = loader_find_asset (loader, pb_alpha_mask->asset_id);
                if (!asset)
                  {
                    collect_error (loader, "Invalid asset id");
                    rut_refable_unref (material);
                    return;
                  }
                rut_material_set_alpha_mask_asset (material, asset);
              }

            pb_init_color (loader->data->ctx, &ambient, pb_material->ambient);
            pb_init_color (loader->data->ctx, &diffuse, pb_material->diffuse);
            pb_init_color (loader->data->ctx, &specular, pb_material->specular);

            rut_material_set_ambient (material, &ambient);
            rut_material_set_diffuse (material, &diffuse);
            rut_material_set_specular (material, &specular);
            if (pb_material->has_shininess)
              rut_material_set_shininess (material, pb_material->shininess);

            rut_entity_add_component (entity, material);

            register_loader_object (loader, material, component_id);
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__SHAPE:
        case RIG__ENTITY__COMPONENT__TYPE__DIAMOND:
          break;
        case RIG__ENTITY__COMPONENT__TYPE__MODEL:
          {
            Rig__Entity__Component__Model *pb_model = pb_component->model;
            RutAsset *asset;
            RutModel *model;

            if (!pb_model->has_asset_id)
              break;

            asset = loader_find_asset (loader, pb_model->asset_id);
            if (!asset)
              {
                collect_error (loader, "Invalid asset id");
                break;
              }

            model = rut_model_new_from_asset (loader->data->ctx, asset);
            if (model)
              {
                rut_refable_unref (asset);
                rut_entity_add_component (entity, model);
                register_loader_object (loader, model, component_id);
              }
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__TEXT:
          {
            Rig__Entity__Component__Text *pb_text = pb_component->text;
            RutText *text =
              rut_text_new_with_text (loader->data->ctx,
                                      pb_text->font, pb_text->text);

            if (pb_text->color)
              {
                CoglColor color;
                pb_init_color (loader->data->ctx, &color, pb_text->color);
                rut_text_set_color (text, &color);
              }

            rut_entity_add_component (entity, text);

            register_loader_object (loader, text, component_id);
            break;
          }
        case _RIG__ENTITY__COMPONENT__TYPE_IS_INT_SIZE:
          g_warn_if_reached ();
        }
    }

  /* Now we add components that may depend on a _MATERIAL... */
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
        case RIG__ENTITY__COMPONENT__TYPE__MATERIAL:
          break;

        case RIG__ENTITY__COMPONENT__TYPE__SHAPE:
          {
            Rig__Entity__Component__Shape *pb_shape = pb_component->shape;
            RutMaterial *material;
            RutAsset *asset = NULL;
            CoglTexture *texture = NULL;
            RutShape *shape;
            CoglBool shaped = FALSE;

            if (pb_shape->has_shaped)
              shaped = pb_shape->shaped;

            material = rut_entity_get_component (entity,
                                                 RUT_COMPONENT_TYPE_MATERIAL);

            /* We need to know the size of the texture before we can create
             * a shape component */
            if (material)
              asset = rut_material_get_texture_asset (material);

            if (asset)
              texture = rut_asset_get_texture (asset);

            if (!texture)
              {
                collect_error (loader,
                               "Can't add shape component without a texture");
                break;
              }

            shape = rut_shape_new (loader->data->ctx,
                                   shaped,
                                   cogl_texture_get_width (texture),
                                   cogl_texture_get_height (texture));
            rut_entity_add_component (entity, shape);

            register_loader_object (loader, shape, component_id);
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

            if (pb_diamond->has_size)
              diamond_size = pb_diamond->size;

            material = rut_entity_get_component (entity,
                                                 RUT_COMPONENT_TYPE_MATERIAL);

            /* We need to know the size of the texture before we can create
             * a diamond component */
            if (material)
              asset = rut_material_get_texture_asset (material);

            if (asset)
              texture = rut_asset_get_texture (asset);

            if (!texture)
              {
                collect_error (loader,
                               "Can't add diamond component without a texture");
                rut_refable_unref (material);
                break;
              }

            diamond = rut_diamond_new (loader->data->ctx,
                                       diamond_size,
                                       cogl_texture_get_width (texture),
                                       cogl_texture_get_height (texture));
            rut_entity_add_component (entity, diamond);

            register_loader_object (loader, diamond, component_id);
            break;
          }
        case RIG__ENTITY__COMPONENT__TYPE__MODEL:
        case RIG__ENTITY__COMPONENT__TYPE__TEXT:
          break;
        case _RIG__ENTITY__COMPONENT__TYPE_IS_INT_SIZE:
          g_warn_if_reached ();
        }
    }
}

static void
load_entities (Loader *loader,
               int n_entities,
               Rig__Entity **entities)
{
  int i;

  for (i = 0; i < n_entities; i++)
    {
      Rig__Entity *pb_entity = entities[i];
      RutEntity *entity;
      uint64_t id;

      if (!pb_entity->has_id)
        continue;

      id = pb_entity->id;
      if (g_hash_table_lookup (loader->id_map, &id))
        {
          collect_error (loader, "Duplicate entity id %d", (int)id);
          continue;
        }

      entity = rut_entity_new (loader->data->ctx);

      if (pb_entity->has_parent_id)
        {
          unsigned int parent_id = pb_entity->parent_id;
          RutEntity *parent = loader_find_entity (loader, parent_id);

          if (!parent)
            {
              collect_error (loader,
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
      if (pb_entity->has_cast_shadow)
        rut_entity_set_cast_shadow (entity, pb_entity->cast_shadow);

      load_components (loader, entity, pb_entity);

      register_loader_object (loader, entity, id);

      loader->entities =
        g_list_prepend (loader->entities, entity);
    }
}

static void
load_assets (Loader *loader,
             int n_assets,
             Rig__Asset **assets)
{
  int i;

  for (i = 0; i < n_assets; i++)
    {
      Rig__Asset *pb_asset = assets[i];
      uint64_t id;
      char *full_path;
      GFile *asset_file;
      GFileInfo *info;
      RutAsset *asset = NULL;

      if (!pb_asset->has_id)
        continue;

      id = pb_asset->id;
      if (g_hash_table_lookup (loader->id_map, &id))
        {
          collect_error (loader, "Duplicate asset id %d", (int)id);
          return;
        }

      if (!pb_asset->path)
        continue;

      full_path = g_build_filename (loader->data->ctx->assets_location,
                                    pb_asset->path, NULL);
      asset_file = g_file_new_for_path (full_path);
      info = g_file_query_info (asset_file,
                                "standard::*",
                                G_FILE_QUERY_INFO_NONE,
                                NULL,
                                NULL);
      if (info)
        {
          asset = rig_load_asset (loader->data, info, asset_file);
          if (asset)
            {
              loader->assets = g_list_prepend (loader->assets, asset);
              register_loader_object (loader, asset, id);
            }
          g_object_unref (info);
        }

      g_object_unref (asset_file);
      g_free (full_path);
    }
}

static void
pb_init_boxed_value (Loader *loader,
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
      boxed->d.text_val = pb_value->text_value;
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
      pb_init_color (loader->data->ctx,
                     &boxed->d.color_val, pb_value->color_value);
      break;

    case RUT_PROPERTY_TYPE_ENUM:
      /* XXX: this should possibly work in terms of string names rather than
       * the integer value? */
      boxed->d.enum_val = pb_value->enum_value;
      break;

    case RUT_PROPERTY_TYPE_OBJECT:
    case RUT_PROPERTY_TYPE_POINTER:
      g_warn_if_reached ();
      break;
    }
}

static void
load_path_nodes (Loader *loader,
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
            pb_init_color (loader->data->ctx, &color, pb_value->color_value);
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
        case RUT_PROPERTY_TYPE_OBJECT:
        case RUT_PROPERTY_TYPE_POINTER:
          g_warn_if_reached ();
        }
    }
}

static void
load_transition_properties (Loader *loader,
                            RigTransition *transition,
                            int n_properties,
                            Rig__Transition__Property **properties)
{
  int i;

  for (i = 0; i < n_properties; i++)
    {
      Rig__Transition__Property *pb_property = properties[i];
      uint64_t object_id;
      RutObject *object;
      CoglBool animated;
      RigTransitionPropData *prop_data;

      if (!pb_property->has_object_id ||
          pb_property->name == NULL)
        continue;

      if (pb_property->has_animated)
        animated = pb_property->animated;
      else
        animated = FALSE;

      object_id = pb_property->object_id;

      object = loader_find_introspectable (loader, object_id);
      if (!object)
        {
          collect_error (loader,
                         "Invalid object id %d referenced in property element",
                         (int)object_id);
          continue;
        }

      prop_data = rig_transition_get_prop_data (transition,
                                                object,
                                                pb_property->name);
      if (!prop_data)
        {
          collect_error (loader,
                         "Invalid object property name given for"
                         "transition property");
          continue;
        }

      if (prop_data->property->spec->animatable)
        {
          if (animated)
            rig_transition_set_property_animated (transition,
                                                  prop_data->property,
                                                  TRUE);
        }
      else if (animated)
        {
          collect_error (loader,
                         "A non-animatable property is marked as animated");
        }

      pb_init_boxed_value (loader,
                           &prop_data->constant_value,
                           prop_data->constant_value.type,
                           pb_property->constant);

      if (pb_property->path)
        {
          Rig__Path *pb_path = pb_property->path;

          prop_data->path =
            rig_path_new (loader->data->ctx,
                          prop_data->property->spec->type);

          load_path_nodes (loader,
                           prop_data->path,
                           pb_path->n_nodes,
                           pb_path->nodes);
        }
    }
}

static void
load_transitions (Loader *loader,
                  int n_transitions,
                  Rig__Transition **transitions)
{
  int i;

  for (i = 0; i < n_transitions; i++)
    {
      Rig__Transition *pb_transition = transitions[i];
      RigTransition *transition;
      uint64_t id;

      if (!pb_transition->has_id)
        continue;

      id = pb_transition->id;

      transition = rig_create_transition (loader->data, id);

      load_transition_properties (loader,
                                  transition,
                                  pb_transition->n_properties,
                                  pb_transition->properties);

      loader->transitions = g_list_prepend (loader->transitions, transition);
    }
}

static void
ignore_free (void *allocator_data, void *ptr)
{
  /* NOP */
}

static void
rig_load_xml (Loader *loader, const char *file)
{
  RigData *data = loader->data;
  GMarkupParser parser = {
    .start_element = parse_start_element,
    .end_element = parse_end_element,
    .error = parse_error
  };
  char *contents;
  gsize len;
  GError *error = NULL;
  GMarkupParseContext *context;

  g_queue_init (&loader->state);
  loader_push_state (loader, LOADER_STATE_NONE);
  g_queue_push_tail (&loader->state, LOADER_STATE_NONE);

  if (!g_file_get_contents (file,
                            &contents,
                            &len,
                            &error))
    {
      g_warning ("Failed to load ui description: %s", error->message);
      return;
    }

  context = g_markup_parse_context_new (&parser, 0, loader, NULL);

  if (!g_markup_parse_context_parse (context, contents, len, &error))
    g_warning ("Failed to parse ui description: %s", error->message);

  g_markup_parse_context_free (context);

  g_queue_clear (&loader->state);

  if (loader->device_found)
    {
      data->device_width = loader->device_width;
      data->device_height = loader->device_height;

      if (loader->background_set)
        data->background_color = loader->background;
    }
}

static void
rig_load_pb (Loader *loader, const char *file)
{
  RigData *data = loader->data;
  uint8_t *contents;
  size_t len;
  GError *error = NULL;
  gboolean needs_munmap = FALSE;
  struct stat sb;
  int fd;
  Rig__UI *pb_ui;

  /* We use a special allocator while unpacking protocol buffers
   * that lets us use the serialization_stack. This means much
   * less mucking about with the heap since the serialization_stack
   * is a persistant, growable stack which we can just rewind
   * very cheaply before unpacking */
  ProtobufCAllocator protobuf_c_allocator =
    {
      rut_memory_stack_alloc,
      ignore_free,
      rut_memory_stack_alloc, /* tmp_alloc */
      8192, /* max_alloca */
      data->serialization_stack /* allocator_data */
    };

  rut_memory_stack_rewind (data->serialization_stack);

  fd = open (file, O_CLOEXEC);
  if (fd > 0 &&
      fstat (fd, &sb) == 0 &&
      (contents = mmap (NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)))
    {
      len = sb.st_size;
      needs_munmap = TRUE;
    }
  else if (!g_file_get_contents (file,
                                 (gchar **)&contents,
                                 &len,
                                 &error))
    {
      g_warning ("Failed to load ui description: %s", error->message);
      return;
    }

  pb_ui = rig__ui__unpack (&protobuf_c_allocator, len, contents);

  if (pb_ui->device)
    {
      Rig__Device *device = pb_ui->device;
      if (device->has_width)
        data->device_width = device->width;
      if (device->has_height)
        data->device_height = device->height;

      if (device->background)
        pb_init_color (data->ctx,
                       &data->background_color,
                       device->background);
    }

  load_assets (loader,
               pb_ui->n_assets,
               pb_ui->assets);

  load_entities (loader,
                 pb_ui->n_entities,
                 pb_ui->entities);

  load_transitions (loader,
                    pb_ui->n_transitions,
                    pb_ui->transitions);

  rig__ui__free_unpacked (pb_ui, &protobuf_c_allocator);

  if (needs_munmap)
    munmap (contents, len);
}

void
rig_load (RigData *data, const char *file)
{
  Loader loader;
  GList *l;
  char *ext;

  memset (&loader, 0, sizeof (loader));
  loader.data = data;

  /* This hash table maps from uint64_t ids to objects while loading */
  loader.id_map = g_hash_table_new_full (g_int64_hash,
                                         g_int64_equal,
                                         free_id_slice,
                                         NULL);

  ext = g_strrstr (file, ".xml");
  if (ext && ext[4] == '\0')
    rig_load_xml (&loader, file);
  else
    rig_load_pb (&loader, file);

  rig_free_ux (data);

  for (l = loader.entities; l; l = l->next)
    {
      if (rut_graphable_get_parent (l->data) == NULL)
        rut_graphable_add_child (data->scene, l->data);
    }

  if (loader.light)
    data->light = loader.light;

  data->transitions = loader.transitions;

  data->assets = loader.assets;

  /* Reset all of the property values to their current value according
   * to the first transition */
  if (data->transitions)
    rig_transition_foreach_property (data->transitions->data,
                                     update_transition_property_cb,
                                     data);

  rut_shell_queue_redraw (data->ctx->shell);

  g_hash_table_destroy (loader.id_map);

  g_print ("File Loaded\n");
}
