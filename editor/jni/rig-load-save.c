#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "rig-data.h"

#define INDENT_LEVEL 2

typedef struct _SaveState
{
  RigData *data;
  FILE *file;
  int indent;
  RigEntity *current_entity;
  int next_id;
  GHashTable *id_map;
} SaveState;

static void
save_component_cb (RigComponent *component,
                   void *user_data)
{
  const RigType *type = rig_object_get_type (component);
  SaveState *state = user_data;

  state->indent += INDENT_LEVEL;

  if (type == &rig_light_type)
    {
      RigLight *light = RIG_LIGHT (component);
      const RigColor *ambient = rig_light_get_ambient (light);
      const RigColor *diffuse = rig_light_get_diffuse (light);
      const RigColor *specular = rig_light_get_specular (light);

      fprintf (state->file,
               "%*s<light "
               "ambient=\"#%02x%02x%02x%02x\" "
               "diffuse=\"#%02x%02x%02x%02x\" "
               "specular=\"#%02x%02x%02x%02x\"/>\n",
               state->indent, "",
               rig_color_get_red_byte (ambient),
               rig_color_get_green_byte (ambient),
               rig_color_get_blue_byte (ambient),
               rig_color_get_alpha_byte (ambient),
               rig_color_get_red_byte (diffuse),
               rig_color_get_green_byte (diffuse),
               rig_color_get_blue_byte (diffuse),
               rig_color_get_alpha_byte (diffuse),
               rig_color_get_red_byte (specular),
               rig_color_get_green_byte (specular),
               rig_color_get_blue_byte (specular),
               rig_color_get_alpha_byte (specular));
    }
  else if (type == &rig_material_type)
    {
      RigMaterial *material = RIG_MATERIAL (component);
      RigAsset *asset = rig_material_get_asset (material);
      const RigColor *color;

      fprintf (state->file, "%*s<material", state->indent, "");

      color = rig_material_get_color (material);
      if (color->red != 1.0 &&
          color->green != 1.0 &&
          color->blue != 1.0 &&
          color->alpha != 1.0)
        {
          fprintf (state->file, " color=\"#%02x%02x%02x%02x\"",
                   rig_color_get_red_byte (color),
                   rig_color_get_green_byte (color),
                   rig_color_get_blue_byte (color),
                   rig_color_get_alpha_byte (color));
        }

      fprintf (state->file, ">\n");

      state->indent += INDENT_LEVEL;

      if (asset)
        {
          int id = GPOINTER_TO_INT (g_hash_table_lookup (state->id_map, asset));
          if (id)
            {
              fprintf (state->file, "%*s<texture asset=\"%d\"/>\n",
                       state->indent, "",
                       id);
            }
        }


      state->indent -= INDENT_LEVEL;
      fprintf (state->file, "%*s</material>\n", state->indent, "");
    }
  else if (type == &rig_diamond_type)
    {
      fprintf (state->file, "%*s<diamond size=\"%f\"/>\n",
               state->indent, "",
               rig_diamond_get_size (RIG_DIAMOND (component)));
    }
  else if (type == &rig_mesh_renderer_type)
    {
      RigMeshRenderer *mesh = RIG_MESH_RENDERER (component);

      fprintf (state->file, "%*s<mesh", state->indent, "");

      switch (rig_mesh_renderer_get_type (mesh))
        {
        case RIG_MESH_RENDERER_TYPE_TEMPLATE:
          fprintf (state->file, " type=\"template\" template=\"%s\"",
                   rig_mesh_renderer_get_path (mesh));
          break;
        case RIG_MESH_RENDERER_TYPE_FILE:
          fprintf (state->file, " type=\"file\" path=\"%s\"",
                   rig_mesh_renderer_get_path (mesh));
          break;
        default:
          g_warn_if_reached ();
        }

      fprintf (state->file, " />\n");
    }

  state->indent -= INDENT_LEVEL;
}

static RigTraverseVisitFlags
_rig_entitygraph_pre_save_cb (RigObject *object,
                              int depth,
                              void *user_data)
{
  SaveState *state = user_data;
  const RigType *type = rig_object_get_type (object);
  RigObject *parent = rig_graphable_get_parent (object);
  RigEntity *entity;
  CoglQuaternion *q;
  float angle;
  float axis[3];
  const char *label;

  if (type != &rig_entity_type)
    {
      g_warning ("Can't save non-entity graphables\n");
      return RIG_TRAVERSE_VISIT_CONTINUE;
    }

  entity = object;

  /* NB: labels with a "rig:" prefix imply that this is an internal
   * entity that shouldn't be saved (such as the editing camera
   * entities) */
  label = rig_entity_get_label (entity);
  if (label && strncmp ("rig:", label, 4) == 0)
    return RIG_TRAVERSE_VISIT_CONTINUE;

  g_hash_table_insert (state->id_map, entity, GINT_TO_POINTER (state->next_id));

  state->indent += INDENT_LEVEL;
  fprintf (state->file, "%*s<entity id=\"%d\"\n",
           state->indent, "",
           state->next_id++);

  if (parent && rig_object_get_type (parent) == &rig_entity_type)
    {
      int id = GPOINTER_TO_INT (g_hash_table_lookup (state->id_map, parent));
      if (id)
        {
          fprintf (state->file, "%*s        parent=\"%d\"\n",
                   state->indent, "",
                   id);
        }
      else
        g_warning ("Failed to find id of parent entity\n");
    }

  if (label)
    fprintf (state->file, "%*s        label=\"%s\"\n",
             state->indent, "",
             label);

  q = rig_entity_get_rotation (entity);

  angle = cogl_quaternion_get_rotation_angle (q);
  cogl_quaternion_get_rotation_axis (q, axis);

  fprintf (state->file,
           "%*s        position=\"(%f, %f, %f)\"\n"
           "%*s        scale=\"%f\"\n"
           "%*s        rotation=\"[%f (%f, %f, %f)]]\">\n",
           state->indent, "",
           rig_entity_get_x (entity),
           rig_entity_get_y (entity),
           rig_entity_get_z (entity),
           state->indent, "",
           rig_entity_get_scale (entity),
           state->indent, "",
           angle, axis[0], axis[1], axis[2]);

  state->current_entity = entity;
  rig_entity_foreach_component (entity,
                                save_component_cb,
                                state);

  fprintf (state->file, "%*s</entity>\n", state->indent, "");
  state->indent -= INDENT_LEVEL;

  return RIG_TRAVERSE_VISIT_CONTINUE;
}

void
rig_save (RigData *data)
{
  struct stat sb;
  char *path = g_build_filename (data->ctx->assets_location, "ui.xml", NULL);
  FILE *file;
  SaveState state;
  GList *l;

  if (stat (data->ctx->assets_location, &sb) == -1)
    mkdir (data->ctx->assets_location, 0777);

  file = fopen (path, "w");
  if (!file)
    {
      g_warning ("Failed to open %s file for saving\n", path);
      return;
    }

  state.data = data;
  state.id_map = g_hash_table_new (g_direct_hash, g_direct_equal);
  state.file = file;
  state.indent = 0;

  fprintf (file, "<ui>\n");

  /* NB: We have to reserve 0 here so we can tell if lookups into the
   * id_map fail. */
  state.next_id = 1;

  /* Assets */

  for (l = data->assets; l; l = l->next)
    {
      RigAsset *asset = l->data;

      if (rig_asset_get_type (asset) != RIG_ASSET_TYPE_TEXTURE)
        continue;

      g_hash_table_insert (state.id_map, asset, GINT_TO_POINTER (state.next_id));

      state.indent += INDENT_LEVEL;
      fprintf (file, "%*s<asset id=\"%d\" type=\"texture\" path=\"%s\" />\n",
               state.indent, "",
               state.next_id++,
               rig_asset_get_path (asset));
      state.indent -= INDENT_LEVEL;
    }

  rig_graphable_traverse (data->scene,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_entitygraph_pre_save_cb,
                          NULL,
                          &state);

  for (l = data->transitions; l; l = l->next)
    {
      RigTransition *transition = l->data;
      GList *l2;
      //int i;

      state.indent += INDENT_LEVEL;
      fprintf (file, "%*s<transition id=\"%d\">\n", state.indent, "", transition->id);

      for (l2 = transition->paths; l2; l2 = l2->next)
        {
          RigPath *path = l2->data;
          GList *l3;
          RigEntity *entity;
          int id;

          if (path == NULL)
            continue;

          entity = path->prop->object;

          id = GPOINTER_TO_INT (g_hash_table_lookup (state.id_map, entity));
          if (!id)
            g_warning ("Failed to find id of entity\n");

          state.indent += INDENT_LEVEL;
          fprintf (file, "%*s<path entity=\"%d\" property=\"%s\">\n", state.indent, "",
                   id,
                   path->prop->spec->name);

          state.indent += INDENT_LEVEL;
          for (l3 = path->nodes.head; l3; l3 = l3->next)
            {
              switch (path->prop->spec->type)
                {
                case RIG_PROPERTY_TYPE_FLOAT:
                  {
                    RigNodeFloat *node = l3->data;
                    fprintf (file, "%*s<node t=\"%f\" value=\"%f\" />\n", state.indent, "", node->t, node->value);
                    break;
                  }
                case RIG_PROPERTY_TYPE_VEC3:
                  {
                    RigNodeVec3 *node = l3->data;
                    fprintf (file, "%*s<node t=\"%f\" value=\"(%f, %f, %f)\" />\n",
                             state.indent, "", node->t,
                             node->value[0],
                             node->value[1],
                             node->value[2]);
                    break;
                  }
                case RIG_PROPERTY_TYPE_QUATERNION:
                  {
                    RigNodeQuaternion *node = l3->data;
                    CoglQuaternion *q = &node->value;
                    float angle;
                    float axis[3];

                    angle = cogl_quaternion_get_rotation_angle (q);
                    cogl_quaternion_get_rotation_axis (q, axis);

                    fprintf (file, "%*s<node t=\"%f\" value=\"[%f (%f, %f, %f)]\" />\n", state.indent, "",
                             node->t, angle, axis[0], axis[1], axis[2]);
                    break;
                  }
                default:
                  g_warn_if_reached ();
                }
            }
          state.indent -= INDENT_LEVEL;

          fprintf (file, "%*s</path>\n", state.indent, "");
          state.indent -= INDENT_LEVEL;
        }

      fprintf (file, "%*s</transition>\n", state.indent, "");
      fprintf (file, "</ui>\n");
    }

  fclose (file);

  g_print ("File Saved\n");

  g_hash_table_destroy (state.id_map);
}

enum {
  LOADER_STATE_NONE,
  LOADER_STATE_LOADING_ENTITY,
  LOADER_STATE_LOADING_MATERIAL_COMPONENT,
  LOADER_STATE_LOADING_MESH_COMPONENT,
  LOADER_STATE_LOADING_DIAMOND_COMPONENT,
  LOADER_STATE_LOADING_LIGHT_COMPONENT,
  LOADER_STATE_LOADING_CAMERA_COMPONENT,
  LOADER_STATE_LOADING_TRANSITION,
  LOADER_STATE_LOADING_PATH
};

typedef struct _Loader
{
  RigData *data;
  GQueue state;
  uint32_t id;
  CoglBool texture_specified;
  uint32_t texture_asset_id;

  GList *assets;
  GList *entities;
  GList *lights;
  GList *transitions;

  RigColor material_color;

  float diamond_size;
  RigEntity *current_entity;
  CoglBool is_light;

  RigTransition *current_transition;
  RigPath *current_path;

  GHashTable *id_map;
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

static RigEntity *
loader_find_entity (Loader *loader, uint32_t id)
{
  RigObject *object =
    g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id));
  if (object == NULL || rig_object_get_type (object) != &rig_entity_type)
    return NULL;
  return RIG_ENTITY (object);
}

static RigAsset *
loader_find_asset (Loader *loader, uint32_t id)
{
  RigObject *object =
    g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id));
  if (object == NULL || rig_object_get_type (object) != &rig_asset_type)
    return NULL;
  return RIG_ASSET (object);
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
      strcmp (element_name, "asset") == 0)
    {
      const char *id_str;
      const char *type;
      const char *path;
      uint32_t id;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "id",
                                        &id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "type",
                                        &type,
                                        G_MARKUP_COLLECT_STRING,
                                        "path",
                                        &path,
                                        G_MARKUP_COLLECT_INVALID))
        {
          return;
        }

      id = g_ascii_strtoull (id_str, NULL, 10);
      if (g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id)))
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Duplicate id %d", id);
          return;
        }

      if (strcmp (type, "texture") == 0)
        {
          RigAsset *asset = rig_asset_new_texture (data->ctx, path);
          loader->assets = g_list_prepend (loader->assets, asset);
          g_hash_table_insert (loader->id_map, GUINT_TO_POINTER (id), asset);
        }
      else
        g_warning ("Ignoring unknown asset type: %s\n", type);
    }
  else if (state == LOADER_STATE_NONE &&
           strcmp (element_name, "entity") == 0)
    {
      const char *id_str;
      unsigned int id;
      RigEntity *entity;
      const char *parent_id_str;
      const char *position_str;
      const char *rotation_str;
      const char *scale_str;

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
                                        "position",
                                        &position_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "rotation",
                                        &rotation_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "scale",
                                        &scale_str,
                                        G_MARKUP_COLLECT_INVALID))
      {
        return;
      }

      id = g_ascii_strtoull (id_str, NULL, 10);
      if (g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id)))
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Duplicate entity id %d", id);
          return;
        }

      entity = rig_entity_new (loader->data->ctx, loader->data->entity_next_id++);

      if (parent_id_str)
        {
          unsigned int parent_id = g_ascii_strtoull (parent_id_str, NULL, 10);
          RigEntity *parent = loader_find_entity (loader, parent_id);

          if (!parent)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid parent id referenced in entity element");
              rig_ref_countable_unref (entity);
              return;
            }

          rig_graphable_add_child (parent, entity);
        }

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
          rig_entity_set_position (entity, pos);
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
          rig_entity_set_rotation (entity, &q);
        }
      if (scale_str)
        {
          double scale = g_ascii_strtod (scale_str, NULL);
          rig_entity_set_scale (entity, scale);
        }

      loader->current_entity = entity;
      loader->is_light = FALSE;
      g_hash_table_insert (loader->id_map, GUINT_TO_POINTER (id), entity);

      loader_push_state (loader, LOADER_STATE_LOADING_ENTITY);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "material") == 0)
    {
      const char *color_str;

      loader->texture_specified = FALSE;
      loader_push_state (loader, LOADER_STATE_LOADING_MATERIAL_COMPONENT);

      g_markup_collect_attributes (element_name,
                                   attribute_names,
                                   attribute_values,
                                   error,
                                   G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                   "color",
                                   &color_str,
                                   G_MARKUP_COLLECT_INVALID);

      if (color_str)
        {
          rig_color_init_from_string (loader->data->ctx,
                                      &loader->material_color,
                                      color_str);
        }
      else
        rig_color_init_from_4f (&loader->material_color, 1, 1, 1, 1);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "light") == 0)
    {
      const char *ambient_str;
      const char *diffuse_str;
      const char *specular_str;
      RigColor ambient;
      RigColor diffuse;
      RigColor specular;
      RigLight *light;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
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

      rig_color_init_from_string (loader->data->ctx, &ambient, ambient_str);
      rig_color_init_from_string (loader->data->ctx, &diffuse, diffuse_str);
      rig_color_init_from_string (loader->data->ctx, &specular, specular_str);

      light = rig_light_new ();
      rig_light_set_ambient (light, &ambient);
      rig_light_set_diffuse (light, &diffuse);
      rig_light_set_specular (light, &specular);

      rig_entity_add_component (loader->current_entity, light);
      loader->is_light = TRUE;

      //loader_push_state (loader, LOADER_STATE_LOADING_LIGHT_COMPONENT);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "diamond") == 0)
    {
      const char *size_str;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "size",
                                        &size_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      loader->diamond_size = g_ascii_strtod (size_str, NULL);

      loader_push_state (loader, LOADER_STATE_LOADING_DIAMOND_COMPONENT);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "mesh") == 0)
    {
      const char *type_str;
      const char *template_str;
      const char *path_str;
      RigMeshRenderer *mesh;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "type",
                                        &type_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "template",
                                        &template_str,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL,
                                        "path",
                                        &path_str,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      if (strcmp (type_str, "template") == 0)
        {
          if (!template_str)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Missing mesh template name");
              return;
            }
          mesh = rig_mesh_renderer_new_from_template (loader->data->ctx,
                                                      template_str);
        }
      else if (strcmp (type_str, "file") == 0)
        {
          if (!path_str)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Missing mesh path name");
              return;
            }
          mesh = rig_mesh_renderer_new_from_file (loader->data->ctx, path_str);
        }
      else
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid mesh type \"%s\"", type_str);
          return;
        }

      if (mesh)
        rig_entity_add_component (loader->current_entity, mesh);
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
           strcmp (element_name, "path") == 0)
    {
      const char *entity_id_str;
      uint32_t entity_id;
      RigEntity *entity;
      const char *property_name;
      RigProperty *prop;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "entity",
                                        &entity_id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "property",
                                        &property_name,
                                        G_MARKUP_COLLECT_INVALID))
        return;

      entity_id = g_ascii_strtoull (entity_id_str, NULL, 10);

      entity = loader_find_entity (loader, entity_id);
      if (!entity)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid Entity id %d referenced in path element",
                       entity_id);
          return;
        }

      prop = rig_introspectable_lookup_property (entity, property_name);
      if (!prop)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid Entity property referenced in path element");
          return;
        }

      loader->current_path =
        rig_path_new_for_property (data->ctx,
                                   &loader->current_transition
                                   ->props[RIG_TRANSITION_PROP_PROGRESS],
                                   prop);

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

      switch (loader->current_path->prop->spec->type)
        {
        case RIG_PROPERTY_TYPE_FLOAT:
          {
            float value = g_ascii_strtod (value_str, NULL);
            rig_path_insert_float (loader->current_path, t, value);
            break;
          }
        case RIG_PROPERTY_TYPE_VEC3:
          {
            float value[3];
            if (sscanf (value_str, "(%f, %f, %f)",
                        &value[0], &value[1], &value[2]) != 3)
              {
                g_set_error (error,
                             G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             "Invalid vec3 value");
                return;
              }
            rig_path_insert_vec3 (loader->current_path, t, value);
            break;
          }
        case RIG_PROPERTY_TYPE_QUATERNION:
          {
            float angle, x, y, z;

            if (sscanf (value_str, "[%f (%f, %f, %f)]", &angle, &x, &y, &z) != 4)
              {
                g_set_error (error,
                             G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             "Invalid rotation value");
                return;
              }

            rig_path_insert_quaternion (loader->current_path,
                                        t,
                                        angle,
                                        x, y, z);
            break;
          }
        }
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
      if (loader->is_light)
        loader->lights = g_list_prepend (loader->entities, loader->current_entity);
      else
        loader->entities = g_list_prepend (loader->entities, loader->current_entity);

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_DIAMOND_COMPONENT &&
           strcmp (element_name, "diamond") == 0)
    {
      RigMaterial *material =
        rig_entity_get_component (loader->current_entity,
                                  RIG_COMPONENT_TYPE_MATERIAL);
      RigAsset *asset = NULL;
      CoglTexture *texture = NULL;
      RigDiamond *diamond;

      /* We need to know the size of the texture before we can create
       * a diamond component */
      if (material)
        asset = rig_material_get_asset (material);

      if (asset)
        texture = rig_asset_get_texture (asset);

      if (!texture)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Can't add diamond component without a texture");
          return;
        }

      diamond = rig_diamond_new (loader->data->ctx,
                                 loader->diamond_size,
                                 cogl_texture_get_width (texture),
                                 cogl_texture_get_height (texture));
      rig_entity_add_component (loader->current_entity,
                                diamond);

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_MATERIAL_COMPONENT &&
           strcmp (element_name, "material") == 0)
    {
      RigMaterial *material;
      RigAsset *texture_asset;

      if (loader->texture_specified)
        {
          texture_asset = loader_find_asset (loader, loader->texture_asset_id);
          if (!texture_asset)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid asset id");
              return;
            }
        }
      else
        texture_asset = NULL;

      material = rig_material_new (loader->data->ctx,
                                   texture_asset,
                                   &loader->material_color);
      rig_entity_add_component (loader->current_entity, material);

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_TRANSITION &&
           strcmp (element_name, "transition") == 0)
    {
      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_PATH &&
           strcmp (element_name, "path") == 0)
    {
      rig_transition_add_path (loader->current_transition,
                               loader->current_path);
      loader_pop_state (loader);
    }
}

static void
parse_error (GMarkupParseContext *context,
             GError *error,
             void *user_data)
{

}

void
rig_load (RigData *data, const char *file)
{
  GMarkupParser parser = {
    .start_element = parse_start_element,
    .end_element = parse_end_element,
    .error = parse_error
  };
  Loader loader;
  char *contents;
  gsize len;
  GError *error = NULL;
  GMarkupParseContext *context;
  GList *l;

  memset (&loader, 0, sizeof (loader));
  loader.data = data;
  g_queue_init (&loader.state);
  loader_push_state (&loader, LOADER_STATE_NONE);
  g_queue_push_tail (&loader.state, LOADER_STATE_NONE);

  loader.id_map = g_hash_table_new (g_direct_hash, g_direct_equal);

  if (!g_file_get_contents (file,
                            &contents,
                            &len,
                            &error))
    {
      g_warning ("Failed to load ui description: %s", error->message);
      return;
    }

  context = g_markup_parse_context_new (&parser, 0, &loader, NULL);

  if (!g_markup_parse_context_parse (context, contents, len, &error))
    {
      g_warning ("Failed to parse ui description: %s", error->message);
      g_markup_parse_context_free (context);
    }

  g_queue_clear (&loader.state);

  rig_free_ux (data);

  for (l = loader.entities; l; l = l->next)
    {
      if (rig_graphable_get_parent (l->data) == NULL)
        rig_graphable_add_child (data->scene, l->data);
    }

  g_list_free (data->lights);
  data->lights = loader.lights;

  data->transitions = loader.transitions;
  if (data->transitions)
    data->selected_transition = loader.transitions->data;
  else
    {
      RigTransition *transition = rig_create_transition (data, 0);
      data->transitions = g_list_prepend (data->transitions, transition);
      data->selected_transition = transition;
    }

  data->assets = loader.assets;

  rig_update_asset_list (data);

  rig_shell_queue_redraw (data->ctx->shell);

  g_hash_table_destroy (loader.id_map);

  g_print ("File Loaded\n");
}
