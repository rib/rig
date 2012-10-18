#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
  RutEntity *current_entity;
  int next_id;
  GHashTable *id_map;
} SaveState;

static void
save_component_cb (RutComponent *component,
                   void *user_data)
{
  const RutType *type = rut_object_get_type (component);
  SaveState *state = user_data;

  state->indent += INDENT_LEVEL;

  if (type == &rut_light_type)
    {
      RutLight *light = RUT_LIGHT (component);
      const RutColor *ambient = rut_light_get_ambient (light);
      const RutColor *diffuse = rut_light_get_diffuse (light);
      const RutColor *specular = rut_light_get_specular (light);

      fprintf (state->file,
               "%*s<light "
               "ambient=\"#%02x%02x%02x%02x\" "
               "diffuse=\"#%02x%02x%02x%02x\" "
               "specular=\"#%02x%02x%02x%02x\"/>\n",
               state->indent, "",
               rut_color_get_red_byte (ambient),
               rut_color_get_green_byte (ambient),
               rut_color_get_blue_byte (ambient),
               rut_color_get_alpha_byte (ambient),
               rut_color_get_red_byte (diffuse),
               rut_color_get_green_byte (diffuse),
               rut_color_get_blue_byte (diffuse),
               rut_color_get_alpha_byte (diffuse),
               rut_color_get_red_byte (specular),
               rut_color_get_green_byte (specular),
               rut_color_get_blue_byte (specular),
               rut_color_get_alpha_byte (specular));
    }
  else if (type == &rut_material_type)
    {
      RutMaterial *material = RUT_MATERIAL (component);
      RutAsset *asset = rut_material_get_texture_asset (material);
      const RutColor *ambient, *diffuse, *specular;

      fprintf (state->file, "%*s<material", state->indent, "");

      ambient = rut_material_get_ambient (material);
      fprintf (state->file, " ambient=\"#%02x%02x%02x%02x\"",
               rut_color_get_red_byte (ambient),
               rut_color_get_green_byte (ambient),
               rut_color_get_blue_byte (ambient),
               rut_color_get_alpha_byte (ambient));

      diffuse = rut_material_get_diffuse (material);
      fprintf (state->file, "%*s          diffuse=\"#%02x%02x%02x%02x\"",
               state->indent, "",
               rut_color_get_red_byte (diffuse),
               rut_color_get_green_byte (diffuse),
               rut_color_get_blue_byte (diffuse),
               rut_color_get_alpha_byte (diffuse));

      specular = rut_material_get_specular (material);
      fprintf (state->file, "%*s          specular=\"#%02x%02x%02x%02x\"",
               state->indent, "",
               rut_color_get_red_byte (specular),
               rut_color_get_green_byte (specular),
               rut_color_get_blue_byte (specular),
               rut_color_get_alpha_byte (specular));

      fprintf (state->file, "%*s          shininess=\"%f\"",
               state->indent, "",
               rut_material_get_shininess (material));

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
  else if (type == &rut_shape_type)
    {
      fprintf (state->file, "%*s<shape size=\"%f\"/>\n",
               state->indent, "",
               rut_shape_get_size (RUT_SHAPE (component)));
    }
  else if (type == &rut_diamond_type)
    {
      fprintf (state->file, "%*s<diamond size=\"%f\"/>\n",
               state->indent, "",
               rut_diamond_get_size (RUT_DIAMOND (component)));
    }
  else if (type == &rut_model_type)
    {
      RutModel *model = RUT_MODEL (component);

      fprintf (state->file, "%*s<model", state->indent, "");

      switch (rut_model_get_type (model))
        {
        case RUT_MODEL_TYPE_TEMPLATE:
          fprintf (state->file, " type=\"template\" template=\"%s\"",
                   rut_model_get_path (model));
          break;
        case RUT_MODEL_TYPE_FILE:
          fprintf (state->file, " type=\"file\" path=\"%s\"",
                   rut_model_get_path (model));
          break;
        default:
          g_warn_if_reached ();
        }

      fprintf (state->file, " />\n");
    }

  state->indent -= INDENT_LEVEL;
}

static RutTraverseVisitFlags
_rut_entitygraph_pre_save_cb (RutObject *object,
                              int depth,
                              void *user_data)
{
  SaveState *state = user_data;
  const RutType *type = rut_object_get_type (object);
  RutObject *parent = rut_graphable_get_parent (object);
  RutEntity *entity;
  CoglQuaternion *q;
  float angle;
  float axis[3];
  const char *label;

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

  g_hash_table_insert (state->id_map, entity, GINT_TO_POINTER (state->next_id));

  state->indent += INDENT_LEVEL;
  fprintf (state->file, "%*s<entity id=\"%d\"\n",
           state->indent, "",
           state->next_id++);

  if (parent && rut_object_get_type (parent) == &rut_entity_type)
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

  q = rut_entity_get_rotation (entity);

  angle = cogl_quaternion_get_rotation_angle (q);
  cogl_quaternion_get_rotation_axis (q, axis);

  fprintf (state->file,
           "%*s        position=\"(%f, %f, %f)\"\n"
           "%*s        scale=\"%f\"\n"
           "%*s        rotation=\"[%f (%f, %f, %f)]]\"\n"
           "%*s        cast_shadow=\"%s\">\n",
           state->indent, "",
           rut_entity_get_x (entity),
           rut_entity_get_y (entity),
           rut_entity_get_z (entity),
           state->indent, "",
           rut_entity_get_scale (entity),
           state->indent, "",
           angle, axis[0], axis[1], axis[2],
           state->indent, "",
           rut_entity_get_cast_shadow (entity) ? "yes" : "no");

  state->current_entity = entity;
  rut_entity_foreach_component (entity,
                                save_component_cb,
                                state);

  fprintf (state->file, "%*s</entity>\n", state->indent, "");
  state->indent -= INDENT_LEVEL;

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static void
save_float (SaveState *state,
            float value)
{
  fprintf (state->file, "%f", value);
}

static void
save_double (SaveState *state,
             double value)
{
  fprintf (state->file, "%f", value);
}

static void
save_integer (SaveState *state,
              int value)
{
  fprintf (state->file, "%i", value);
}

static void
save_uint32 (SaveState *state,
             uint32_t value)
{
  fprintf (state->file, "%" G_GUINT32_FORMAT, value);
}

static void
save_boolean (SaveState *state,
              CoglBool value)
{
  fputs (value ? "yes" : "no", state->file);
}

static void
save_text (SaveState *state,
           const char *value)
{
  FILE *file = state->file;

  while (*value)
    {
      gunichar ch = g_utf8_get_char (value);

      switch (ch)
        {
        case '"':
          fputs ("&quot;", file);
          break;

        case '\'':
          fputs ("&apos;", file);
          break;

        case '<':
          fputs ("&lt;", file);
          break;

        case '>':
          fputs ("&gt;", file);
          break;

        default:
          if (ch >= ' ' && ch <= 127)
            fputc (ch, file);
          else
            fprintf (file, "&#%i;", ch);
        }

      value = g_utf8_next_char (value);
    }
}

static void
save_vec3 (SaveState *state,
           const float *value)
{
  fprintf (state->file, "(%f, %f, %f)",
           value[0],
           value[1],
           value[2]);
}

static void
save_vec4 (SaveState *state,
           const float *value)
{
  fprintf (state->file, "(%f, %f, %f, %f)",
           value[0],
           value[1],
           value[2],
           value[3]);
}

static void
save_color (SaveState *state,
            const RutColor *value)
{
  fprintf (state->file, "(%f, %f, %f, %f)",
           value->red,
           value->green,
           value->blue,
           value->alpha);
}

static void
save_quaternion (SaveState *state,
                 const CoglQuaternion *value)
{
  float angle;
  float axis[3];

  angle = cogl_quaternion_get_rotation_angle (value);
  cogl_quaternion_get_rotation_axis (value, axis);

  fprintf (state->file,
           "[%f (%f, %f, %f)]",
           angle, axis[0], axis[1], axis[2]);
}

static void
save_path (SaveState *state,
           RigPath *path)
{
  FILE *file = state->file;
  GList *l;

  fprintf (file,
           "%*s<path>\n",
           state->indent, "");

  state->indent += INDENT_LEVEL;

  for (l = path->nodes.head; l; l = l->next)
    {
      RigNode *node = l->data;

      fprintf (file,
               "%*s<node t=\"%f\" value=\"",
               state->indent, "",
               node->t);

      switch (path->type)
        {
        case RUT_PROPERTY_TYPE_FLOAT:
          {
            RigNodeFloat *node = l->data;
            save_float (state, node->value);
            goto handled;
          }
        case RUT_PROPERTY_TYPE_DOUBLE:
          {
            RigNodeDouble *node = l->data;
            save_double (state, node->value);
            goto handled;
          }
        case RUT_PROPERTY_TYPE_VEC3:
          {
            RigNodeVec3 *node = l->data;
            save_vec3 (state, node->value);
            goto handled;
          }
        case RUT_PROPERTY_TYPE_VEC4:
          {
            RigNodeVec4 *node = l->data;
            save_vec4 (state, node->value);
            goto handled;
          }
        case RUT_PROPERTY_TYPE_COLOR:
          {
            RigNodeColor *node = l->data;
            save_color (state, &node->value);
            goto handled;
          }
        case RUT_PROPERTY_TYPE_QUATERNION:
          {
            RigNodeQuaternion *node = l->data;
            save_quaternion (state, &node->value);
            goto handled;
          }
        case RUT_PROPERTY_TYPE_INTEGER:
          {
            RigNodeInteger *node = l->data;
            save_uint32 (state, node->value);
            goto handled;
          }
        case RUT_PROPERTY_TYPE_UINT32:
          {
            RigNodeUint32 *node = l->data;
            save_uint32 (state, node->value);
            goto handled;
          }

          /* These types of properties can't be interoplated so they
           * probably shouldn't end up in a path */
        case RUT_PROPERTY_TYPE_ENUM:
        case RUT_PROPERTY_TYPE_BOOLEAN:
        case RUT_PROPERTY_TYPE_TEXT:
        case RUT_PROPERTY_TYPE_OBJECT:
        case RUT_PROPERTY_TYPE_POINTER:
          break;
        }

      g_warn_if_reached ();

    handled:
      fprintf (file, "\" />\n");
    }

  state->indent -= INDENT_LEVEL;

  fprintf (file,
           "%*s</path>\n",
           state->indent, "");
}

static void
save_boxed_value (SaveState *state,
                  const RutBoxed *value)
{
  switch (value->type)
    {
    case RUT_PROPERTY_TYPE_FLOAT:
      save_float (state, value->d.float_val);
      return;

    case RUT_PROPERTY_TYPE_DOUBLE:
      save_double (state, value->d.double_val);
      return;

    case RUT_PROPERTY_TYPE_INTEGER:
      save_integer (state, value->d.integer_val);
      return;

    case RUT_PROPERTY_TYPE_UINT32:
      save_uint32 (state, value->d.uint32_val);
      return;

    case RUT_PROPERTY_TYPE_BOOLEAN:
      save_boolean (state, value->d.boolean_val);
      return;

    case RUT_PROPERTY_TYPE_TEXT:
      save_text (state, value->d.text_val);
      return;

    case RUT_PROPERTY_TYPE_QUATERNION:
      save_quaternion (state, &value->d.quaternion_val);
      return;

    case RUT_PROPERTY_TYPE_VEC3:
      save_vec3 (state, value->d.vec3_val);
      return;

    case RUT_PROPERTY_TYPE_VEC4:
      save_vec4 (state, value->d.vec4_val);
      return;

    case RUT_PROPERTY_TYPE_COLOR:
      save_color (state, &value->d.color_val);
      return;

    case RUT_PROPERTY_TYPE_ENUM:
      /* FIXME: this should probably save the string names rather than
       * the integer value */
      save_integer (state, value->d.enum_val);
      return;

    case RUT_PROPERTY_TYPE_OBJECT:
    case RUT_PROPERTY_TYPE_POINTER:
      break;
    }

  g_warn_if_reached ();
}

static void
save_property_cb (RutProperty *property,
                  RigPath *path,
                  const RutBoxed *constant_value,
                  void *user_data)
{
  SaveState *state = user_data;
  FILE *file = state->file;
  RutEntity *entity;
  int id;

  if (path == NULL)
    return;

  entity = property->object;

  id = GPOINTER_TO_INT (g_hash_table_lookup (state->id_map, entity));
  if (!id)
    g_warning ("Failed to find id of entity\n");

  state->indent += INDENT_LEVEL;
  fprintf (file, "%*s<property entity=\"%d\" name=\"%s\" animated=\"%s\">\n",
           state->indent, "",
           id,
           property->spec->name,
           property->animated ? "yes" : "no");

  state->indent += INDENT_LEVEL;

  if (path)
    save_path (state, path);

  fprintf (file, "%*s<constant value=\"", state->indent, "");
  save_boxed_value (state, constant_value);
  fprintf (file, "\" />\n");

  state->indent -= INDENT_LEVEL;

  fprintf (file, "%*s</property>\n", state->indent, "");

  state->indent -= INDENT_LEVEL;
}

void
rig_save (RigData *data,
          const char *path)
{
  struct stat sb;
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
      RutAsset *asset = l->data;
      const char *type;

      if (rut_asset_get_type (asset) == RUT_ASSET_TYPE_TEXTURE)
        type = "texture";
      else if (rut_asset_get_type (asset) == RUT_ASSET_TYPE_NORMAL_MAP)
        type = "normal-map";
      else if (rut_asset_get_type (asset) == RUT_ASSET_TYPE_ALPHA_MASK)
        type = "alpha-mask";
      else
        continue;

      g_hash_table_insert (state.id_map, asset, GINT_TO_POINTER (state.next_id));

      state.indent += INDENT_LEVEL;
      fprintf (file, "%*s<asset id=\"%d\" type=\"%s\" path=\"%s\" />\n",
               state.indent, "",
               state.next_id++,
               type,
               rut_asset_get_path (asset));
      state.indent -= INDENT_LEVEL;
    }

  rut_graphable_traverse (data->scene,
                          RUT_TRAVERSE_DEPTH_FIRST,
                          _rut_entitygraph_pre_save_cb,
                          NULL,
                          &state);

  for (l = data->transitions; l; l = l->next)
    {
      RigTransition *transition = l->data;
      //int i;

      state.indent += INDENT_LEVEL;
      fprintf (file, "%*s<transition id=\"%d\">\n", state.indent, "", transition->id);

      rig_transition_foreach_property (transition,
                                       save_property_cb,
                                       &state);

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
  GQueue state;
  uint32_t id;
  CoglBool texture_specified;
  uint32_t texture_asset_id;

  GList *assets;
  GList *entities;
  GList *transitions;

  RutColor material_ambient;
  CoglBool ambient_set;
  RutColor material_diffuse;
  CoglBool diffuse_set;
  RutColor material_specular;
  CoglBool specular_set;
  float material_shininess;
  CoglBool shininess_set;

  float shape_size;
  RutEntity *current_entity;

  RigTransition *current_transition;
  RigTransitionPropData *current_property;
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

static RutEntity *
loader_find_entity (Loader *loader, uint32_t id)
{
  RutObject *object =
    g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id));
  if (object == NULL || rut_object_get_type (object) != &rut_entity_type)
    return NULL;
  return RUT_ENTITY (object);
}

static RutAsset *
loader_find_asset (Loader *loader, uint32_t id)
{
  RutObject *object =
    g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id));
  if (object == NULL || rut_object_get_type (object) != &rut_asset_type)
    return NULL;
  return RUT_ASSET (object);
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
load_color (RutColor *value,
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
        RutColor value;
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
      RutAsset *asset = NULL;

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
          asset = rut_asset_new_texture (data->ctx, path);
        }
      else if (strcmp (type, "normal-map") == 0)
        {
          asset = rut_asset_new_normal_map (data->ctx, path);
        }
      else if (strcmp (type, "alpha-mask") == 0)
        {
          asset = rut_asset_new_alpha_mask (data->ctx, path);
        }
      else
        g_warning ("Ignoring unknown asset type: %s\n", type);

      if (asset)
        {
          loader->assets = g_list_prepend (loader->assets, asset);
          g_hash_table_insert (loader->id_map, GUINT_TO_POINTER (id), asset);
        }
    }
  else if (state == LOADER_STATE_NONE &&
           strcmp (element_name, "entity") == 0)
    {
      const char *id_str;
      unsigned int id;
      RutEntity *entity;
      const char *parent_id_str;
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
      if (g_hash_table_lookup (loader->id_map, GUINT_TO_POINTER (id)))
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Duplicate entity id %d", id);
          return;
        }

      entity = rut_entity_new (loader->data->ctx, loader->data->entity_next_id++);

      if (parent_id_str)
        {
          unsigned int parent_id = g_ascii_strtoull (parent_id_str, NULL, 10);
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
      g_hash_table_insert (loader->id_map, GUINT_TO_POINTER (id), entity);

      loader_push_state (loader, LOADER_STATE_LOADING_ENTITY);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "material") == 0)
    {
      const char *color_str, *ambient_str, *diffuse_str, *specular_str;
      const char *shininess_str;

      loader->texture_specified = FALSE;
      loader_push_state (loader, LOADER_STATE_LOADING_MATERIAL_COMPONENT);

      g_markup_collect_attributes (element_name,
                                   attribute_names,
                                   attribute_values,
                                   error,
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
      const char *ambient_str;
      const char *diffuse_str;
      const char *specular_str;
      RutColor ambient;
      RutColor diffuse;
      RutColor specular;
      RutLight *light;

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

      rut_color_init_from_string (loader->data->ctx, &ambient, ambient_str);
      rut_color_init_from_string (loader->data->ctx, &diffuse, diffuse_str);
      rut_color_init_from_string (loader->data->ctx, &specular, specular_str);

      light = rut_light_new ();
      rut_light_set_ambient (light, &ambient);
      rut_light_set_diffuse (light, &diffuse);
      rut_light_set_specular (light, &specular);

      rut_entity_add_component (loader->current_entity, light);

      //loader_push_state (loader, LOADER_STATE_LOADING_LIGHT_COMPONENT);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "shape") == 0)
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

      loader->shape_size = g_ascii_strtod (size_str, NULL);

      loader_push_state (loader, LOADER_STATE_LOADING_SHAPE_COMPONENT);
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

      loader->shape_size = g_ascii_strtod (size_str, NULL);

      loader_push_state (loader, LOADER_STATE_LOADING_DIAMOND_COMPONENT);
    }
  else if (state == LOADER_STATE_LOADING_ENTITY &&
           strcmp (element_name, "model") == 0)
    {
      const char *type_str;
      const char *template_str;
      const char *path_str;
      RutModel *model;

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
                           "Missing model template name");
              return;
            }
          model = rut_model_new_from_template (loader->data->ctx,
                                             template_str);
        }
      else if (strcmp (type_str, "file") == 0)
        {
          if (!path_str)
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_INVALID_CONTENT,
                           "Missing model path name");
              return;
            }
          model = rut_model_new_from_asset (loader->data->ctx, path_str);
        }
      else
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid model type \"%s\"", type_str);
          return;
        }

      if (model)
        rut_entity_add_component (loader->current_entity, model);
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
           strcmp (element_name, "property") == 0)
    {
      const char *entity_id_str;
      uint32_t entity_id;
      RutEntity *entity;
      const char *property_name;
      CoglBool animated;
      RigTransitionPropData *prop_data;

      if (!g_markup_collect_attributes (element_name,
                                        attribute_names,
                                        attribute_values,
                                        error,
                                        G_MARKUP_COLLECT_STRING,
                                        "entity",
                                        &entity_id_str,
                                        G_MARKUP_COLLECT_STRING,
                                        "name",
                                        &property_name,
                                        G_MARKUP_COLLECT_BOOLEAN,
                                        "animated",
                                        &animated,
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

      prop_data = rig_transition_get_prop_data (loader->current_transition,
                                                entity,
                                                property_name);

      if (prop_data->property->spec->animatable)
        {
          rut_property_set_animated (&data->ctx->property_ctx,
                                     prop_data->property,
                                     animated);
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
                             loader->shape_size,
                             cogl_texture_get_width (texture),
                             cogl_texture_get_height (texture));
      rut_entity_add_component (loader->current_entity,
                                shape);

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
                                 loader->shape_size,
                                 cogl_texture_get_width (texture),
                                 cogl_texture_get_height (texture));
      rut_entity_add_component (loader->current_entity,
                                diamond);

      loader_pop_state (loader);
    }
  else if (state == LOADER_STATE_LOADING_MATERIAL_COMPONENT &&
           strcmp (element_name, "material") == 0)
    {
      RutMaterial *material;
      RutAsset *texture_asset;

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

      material = rut_material_new (loader->data->ctx,
                                   texture_asset);

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
  else if (state == LOADER_STATE_LOADING_TRANSITION &&
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
      if (rut_graphable_get_parent (l->data) == NULL)
        rut_graphable_add_child (data->scene, l->data);
    }

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

#ifdef RIG_EDITOR_ENABLED
  if (!_rig_in_device_mode)
    rig_update_asset_list (data);
#endif

  rut_shell_queue_redraw (data->ctx->shell);

  g_hash_table_destroy (loader.id_map);

  g_print ("File Loaded\n");
}
