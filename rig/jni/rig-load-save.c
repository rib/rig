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
#include "rig-load-xml.h"

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

typedef struct _Loader
{
  RigData *data;

  GList *assets;
  GList *entities;
  RutEntity *light;
  GList *transitions;

  GHashTable *id_map;
} Loader;

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

  ext = g_strrstr (file, ".xml");
  if (ext && ext[4] == '\0')
    {
      rig_load_xml (data, file);
      return;
    }

  memset (&loader, 0, sizeof (loader));
  loader.data = data;

  /* This hash table maps from uint64_t ids to objects while loading */
  loader.id_map = g_hash_table_new_full (g_int64_hash,
                                         g_int64_equal,
                                         free_id_slice,
                                         NULL);

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
