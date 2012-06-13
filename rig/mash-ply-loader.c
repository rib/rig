/*
 * Mash - A library for displaying PLY models in a Clutter scene
 * Copyright (C) 2010  Intel Corporation
 * Copyright (C) 2010  Luca Bruno <lethalman88@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <string.h>
#include <cogl/cogl.h>

#include "mash-ply-loader.h"
#include "rply.h"

#include "rig-global.h"

static void mash_ply_loader_finalize (GObject *object);
static gboolean mash_ply_loader_load (MashDataLoader *data_loader,
                                      MashDataFlags flags,
                                      const gchar *filename,
                                      GError **error);
static void mash_ply_loader_get_data (MashDataLoader *data_loader,
                                      MashDataLoaderData *loader_data);

G_DEFINE_TYPE (MashPlyLoader, mash_ply_loader, MASH_TYPE_DATA_LOADER);

#define MASH_PLY_LOADER_GET_PRIVATE(obj)                      \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_PLY_LOADER,  \
                                MashPlyLoaderPrivate))

static const struct
{
  const gchar *name;
  int size;
}
mash_ply_loader_properties[] =
{
  /* These should be sorted in descending order of size so that it
     never ends doing an unaligned write */
  { "x", sizeof (gfloat) },
  { "y", sizeof (gfloat) },
  { "z", sizeof (gfloat) },
  { "nx", sizeof (gfloat) },
  { "ny", sizeof (gfloat) },
  { "nz", sizeof (gfloat) },
  { "s", sizeof (gfloat) },
  { "t", sizeof (gfloat) },
  { "red", sizeof (guint8) },
  { "green", sizeof (guint8) },
  { "blue", sizeof (guint8) }
};

#define MASH_PLY_LOADER_VERTEX_PROPS    7
#define MASH_PLY_LOADER_NORMAL_PROPS    (7 << 3)
#define MASH_PLY_LOADER_TEX_COORD_PROPS (3 << 6)
#define MASH_PLY_LOADER_COLOR_PROPS     (7 << 8)

typedef struct _MashPlyLoaderData MashPlyLoaderData;

struct _MashPlyLoaderData
{
  p_ply ply;
  GError *error;
  /* Data for the current vertex */
  guint8 current_vertex[G_N_ELEMENTS (mash_ply_loader_properties) * 4];
  /* Map from property number to byte offset in the current_vertex array */
  gint prop_map[G_N_ELEMENTS (mash_ply_loader_properties)];
  /* Number of bytes for a vertex */
  guint n_vertex_bytes;
  gint available_props, got_props;
  guint first_vertex, last_vertex;
  GByteArray *vertices;
  GArray *faces;
  CoglIndicesType indices_type;
  MashDataFlags flags;

  /* Bounding cuboid of the data */
  CoglVertexP3 min_vertex, max_vertex;
};

struct _MashPlyLoaderPrivate
{
  CoglPrimitive *primitive;

  /* Bounding cuboid of the data */
  CoglVertexP3 min_vertex, max_vertex;
};

static void
mash_ply_loader_class_init (MashPlyLoaderClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  MashDataLoaderClass *data_loader_class = (MashDataLoaderClass *) klass;

  gobject_class->finalize = mash_ply_loader_finalize;

  data_loader_class->load = mash_ply_loader_load;
  data_loader_class->get_data = mash_ply_loader_get_data;

  g_type_class_add_private (klass, sizeof (MashPlyLoaderPrivate));
}

static void
mash_ply_loader_init (MashPlyLoader *self)
{
  self->priv = MASH_PLY_LOADER_GET_PRIVATE (self);
}

static void
mash_ply_loader_free_primitive (MashPlyLoader *self)
{
  MashPlyLoaderPrivate *priv = self->priv;

  if (priv->primitive)
    {
      cogl_object_unref (priv->primitive);
      priv->primitive = NULL;
    }
}

static void
mash_ply_loader_finalize (GObject *object)
{
  MashPlyLoader *self = (MashPlyLoader *) object;

  mash_ply_loader_free_primitive (self);

  G_OBJECT_CLASS (mash_ply_loader_parent_class)->finalize (object);
}

static void
mash_ply_loader_error_cb (const char *message, gpointer data)
{
  MashPlyLoaderData *load_ply_loader = data;

  if (load_ply_loader->error == NULL)
    g_set_error_literal (&load_ply_loader->error, MASH_DATA_ERROR,
                         MASH_DATA_ERROR_UNKNOWN, message);
}

static void
mash_ply_loader_check_unknown_error (MashPlyLoaderData *data)
{
  if (data->error == NULL)
    g_set_error_literal (&data->error,
                         MASH_DATA_ERROR,
                         MASH_DATA_ERROR_UNKNOWN,
                         "Unknown error loading PLY file");
}

static int
mash_ply_loader_vertex_read_cb (p_ply_argument argument)
{
  long prop_num;
  MashPlyLoaderData *data;
  gint32 length, index;
  double value;

  ply_get_argument_user_data (argument, (void **) &data, &prop_num);
  ply_get_argument_property (argument, NULL, &length, &index);

  if (length != 1 || index != 0)
    {
      g_set_error (&data->error, MASH_DATA_ERROR,
                   MASH_DATA_ERROR_INVALID,
                   "List type property not supported for vertex element '%s'",
                   mash_ply_loader_properties[prop_num].name);

      return 0;
    }

  value = ply_get_argument_value (argument);

  /* Colors are specified as a byte so we need to treat them specially */
  if (((1 << prop_num) & MASH_PLY_LOADER_COLOR_PROPS))
    data->current_vertex[data->prop_map[prop_num]] = value;
  else
    *(gfloat *) (data->current_vertex + data->prop_map[prop_num]) = value;

  data->got_props |= 1 << prop_num;

  /* If we've got enough properties for a complete vertex then add it
     to the array */
  if (data->got_props == data->available_props)
    {
      int i;

      /* Flip any axes that have been specified in the MashPlyLoaderFlags */
      if ((data->available_props & MASH_PLY_LOADER_VERTEX_PROPS)
          == MASH_PLY_LOADER_VERTEX_PROPS)
        for (i = 0; i < 3; i++)
          if ((data->flags & (MASH_DATA_NEGATE_X << i)))
            {
              gfloat *pos = (gfloat *) (data->current_vertex
                                        + data->prop_map[i]);
              *pos = -*pos;
            }
      if ((data->available_props & MASH_PLY_LOADER_NORMAL_PROPS)
          == MASH_PLY_LOADER_NORMAL_PROPS)
        for (i = 0; i < 3; i++)
          if ((data->flags & (MASH_DATA_NEGATE_X << i)))
            {
              gfloat *pos = (gfloat *) (data->current_vertex
                                        + data->prop_map[i + 3]);
              *pos = -*pos;
            }

      g_byte_array_append (data->vertices, data->current_vertex,
                           data->n_vertex_bytes);
      data->got_props = 0;

      /* Update the bounding box for the data */
      for (i = 0; i < 3; i++)
        {
          gfloat *min = &data->min_vertex.x + i;
          gfloat *max = &data->max_vertex.x + i;
          gfloat value = *(gfloat *) (data->current_vertex + data->prop_map[i]);

          if (value < *min)
            *min = value;
          if (value > *max)
            *max = value;
        }
    }

  return 1;
}

static void
mash_ply_loader_add_face_index (MashPlyLoaderData *data,
                                guint index)
{
  switch (data->indices_type)
    {
    case COGL_INDICES_TYPE_UNSIGNED_BYTE:
      {
        guint8 value = index;
        g_array_append_val (data->faces, value);
      }
      break;
    case COGL_INDICES_TYPE_UNSIGNED_SHORT:
      {
        guint16 value = index;
        g_array_append_val (data->faces, value);
      }
      break;
    case COGL_INDICES_TYPE_UNSIGNED_INT:
      {
        guint32 value = index;
        g_array_append_val (data->faces, value);
      }
      break;
    }
}

static gboolean
mash_ply_loader_get_indices_type (MashPlyLoaderData *data,
                                  GError **error)
{
  p_ply_element elem = NULL;

  /* Look for the 'vertices' element */
  while ((elem = ply_get_next_element (data->ply, elem)))
    {
      const char *name;
      gint32 n_instances;

      if (ply_get_element_info (elem, &name, &n_instances))
        {
          if (!strcmp (name, "vertex"))
            {
              if (n_instances <= 0x100)
                {
                  data->indices_type = COGL_INDICES_TYPE_UNSIGNED_BYTE;
                  data->faces = g_array_new (FALSE, FALSE, sizeof (guint8));
                }
              else if (n_instances <= 0x10000)
                {
                  data->indices_type = COGL_INDICES_TYPE_UNSIGNED_SHORT;
                  data->faces = g_array_new (FALSE, FALSE, sizeof (guint16));
                }
              else if (cogl_has_feature (rig_cogl_context,
                                         COGL_FEATURE_ID_UNSIGNED_INT_INDICES))
                {
                  data->indices_type = COGL_INDICES_TYPE_UNSIGNED_INT;
                  data->faces = g_array_new (FALSE, FALSE, sizeof (guint32));
                }
              else
                {
                  g_set_error (error, MASH_DATA_ERROR,
                               MASH_DATA_ERROR_UNSUPPORTED,
                               "The PLY file requires unsigned int indices "
                               "but this is not supported by your GL driver");
                  return FALSE;
                }

              return TRUE;
            }
        }
      else
        {
          g_set_error (error, MASH_DATA_ERROR,
                       MASH_DATA_ERROR_UNKNOWN,
                       "Error getting element info");
          return FALSE;
        }
    }

  g_set_error (error, MASH_DATA_ERROR,
               MASH_DATA_ERROR_MISSING_PROPERTY,
               "PLY file is missing the vertex element");

  return FALSE;
}

static int
mash_ply_loader_face_read_cb (p_ply_argument argument)
{
  long prop_num;
  MashPlyLoaderData *data;
  gint32 length, index;

  ply_get_argument_user_data (argument, (void **) &data, &prop_num);
  ply_get_argument_property (argument, NULL, &length, &index);

  if (index == 0)
    data->first_vertex = ply_get_argument_value (argument);
  else if (index == 1)
    data->last_vertex = ply_get_argument_value (argument);
  else if (index != -1)
    {
      guint new_vertex = ply_get_argument_value (argument);

      /* Add a triangle with the first vertex, the last vertex and
         this new vertex */
      mash_ply_loader_add_face_index (data, data->first_vertex);
      mash_ply_loader_add_face_index (data, data->last_vertex);
      mash_ply_loader_add_face_index (data, new_vertex);

      /* Use the new vertex as one of the vertices next time around */
      data->last_vertex = new_vertex;
    }

  return 1;
}

static gboolean
mash_ply_loader_load (MashDataLoader *data_loader,
                      MashDataFlags flags,
                      const gchar *filename,
                      GError **error)
{
  CoglIndices *indices;
  MashPlyLoader *self = MASH_PLY_LOADER (data_loader);
  MashPlyLoaderPrivate *priv;
  MashPlyLoaderData data;
  gchar *display_name;
  gboolean ret;

  priv = self->priv;

  data.error = NULL;
  data.n_vertex_bytes = 0;
  data.available_props = 0;
  data.got_props = 0;
  data.vertices = g_byte_array_new ();
  data.faces = NULL;
  data.min_vertex.x = G_MAXFLOAT;
  data.min_vertex.y = G_MAXFLOAT;
  data.min_vertex.z = G_MAXFLOAT;
  data.max_vertex.x = -G_MAXFLOAT;
  data.max_vertex.y = -G_MAXFLOAT;
  data.max_vertex.z = -G_MAXFLOAT;
  data.flags = flags;

  display_name = g_filename_display_name (filename);

  if ((data.ply = ply_open (filename,
                            mash_ply_loader_error_cb,
                            &data)) == NULL)
    mash_ply_loader_check_unknown_error (&data);
  else
    {
      if (!ply_read_header (data.ply))
        mash_ply_loader_check_unknown_error (&data);
      else
        {
          int i;

          for (i = 0; i < G_N_ELEMENTS (mash_ply_loader_properties); i++)
            if (ply_set_read_cb (data.ply, "vertex",
                                 mash_ply_loader_properties[i].name,
                                 mash_ply_loader_vertex_read_cb,
                                 &data, i))
              {
                data.prop_map[i] = data.n_vertex_bytes;
                data.n_vertex_bytes += mash_ply_loader_properties[i].size;
                data.available_props |= 1 << i;
              }

          /* Align the size of a vertex to 32 bits */
          data.n_vertex_bytes = (data.n_vertex_bytes + 3) & ~(guint) 3;

          if ((data.available_props & MASH_PLY_LOADER_VERTEX_PROPS)
              != MASH_PLY_LOADER_VERTEX_PROPS)
            g_set_error (&data.error, MASH_DATA_ERROR,
                         MASH_DATA_ERROR_MISSING_PROPERTY,
                         "PLY file %s is missing the vertex properties",
                         display_name);
          else if (!ply_set_read_cb (data.ply, "face", "vertex_indices",
                                     mash_ply_loader_face_read_cb,
                                     &data, i))
            g_set_error (&data.error, MASH_DATA_ERROR,
                         MASH_DATA_ERROR_MISSING_PROPERTY,
                         "PLY file %s is missing face property "
                         "'vertex_indices'",
                         display_name);
          else if (mash_ply_loader_get_indices_type (&data, &data.error)
                   && !ply_read (data.ply))
            mash_ply_loader_check_unknown_error (&data);
        }

      ply_close (data.ply);
    }

  if (data.error)
    {
      g_propagate_error (error, data.error);
      ret = FALSE;
    }
  else if (data.faces->len < 3)
    {
      g_set_error (error, MASH_DATA_ERROR,
                   MASH_DATA_ERROR_INVALID,
                   "No faces found in %s",
                   display_name);
      ret = FALSE;
    }
  else
    {
      CoglAttributeBuffer *attribute_buffer;
      CoglAttribute *attributes[4];
      int n_attributes = 0, i;

      /* Get rid of the old primitive (if any) */
      mash_ply_loader_free_primitive (self);

      /* Create a new attribute buffer for the vertices */
      attribute_buffer = cogl_attribute_buffer_new (rig_cogl_context,
                                                    data.vertices->len,
                                                    data.vertices->data);

      /* And describe the attributes */
      if ((data.available_props & MASH_PLY_LOADER_VERTEX_PROPS) ==
          MASH_PLY_LOADER_VERTEX_PROPS)
        {
          attributes[n_attributes++] =
            cogl_attribute_new (attribute_buffer,
                                "cogl_position_in",
                                data.n_vertex_bytes,
                                data.prop_map[0],
                                3,
                                COGL_ATTRIBUTE_TYPE_FLOAT);
        }

      if ((data.available_props & MASH_PLY_LOADER_NORMAL_PROPS) ==
          MASH_PLY_LOADER_NORMAL_PROPS)
        {
          attributes[n_attributes++] =
            cogl_attribute_new (attribute_buffer,
                                "cogl_normal_in",
                                data.n_vertex_bytes,
                                data.prop_map[3],
                                3,
                                COGL_ATTRIBUTE_TYPE_FLOAT);
        }

      if ((data.available_props & MASH_PLY_LOADER_TEX_COORD_PROPS) ==
          MASH_PLY_LOADER_TEX_COORD_PROPS)
        {
          attributes[n_attributes++] =
            cogl_attribute_new (attribute_buffer,
                                "cogl_tex_coord0_in",
                                data.n_vertex_bytes,
                                data.prop_map[6],
                                2,
                                COGL_ATTRIBUTE_TYPE_FLOAT);
        }

      if ((data.available_props & MASH_PLY_LOADER_COLOR_PROPS) ==
          MASH_PLY_LOADER_COLOR_PROPS)
        {
          attributes[n_attributes++] =
            cogl_attribute_new (attribute_buffer,
                                "cogl_color_in",
                                data.n_vertex_bytes,
                                data.prop_map[8],
                                3,
                                COGL_ATTRIBUTE_TYPE_FLOAT);
        }

      priv->primitive =
        cogl_primitive_new_with_attributes (COGL_VERTICES_MODE_TRIANGLES,
                                            data.vertices->len,
                                            attributes, n_attributes);
      for (i = 0; i < n_attributes; i++)
        cogl_object_unref (attributes[i]);

      indices = cogl_indices_new (rig_cogl_context,
                                  data.indices_type,
                                  data.faces->data,
                                  data.faces->len);
      cogl_primitive_set_indices (priv->primitive,
                                  indices,
                                  data.faces->len);
      cogl_object_unref (indices);

      priv->min_vertex = data.min_vertex;
      priv->max_vertex = data.max_vertex;

      ret = TRUE;
    }

  g_free (display_name);
  g_byte_array_free (data.vertices, TRUE);
  if (data.faces)
    g_array_free (data.faces, TRUE);

  return ret;
}

static void
mash_ply_loader_get_data (MashDataLoader *data_loader, MashDataLoaderData *loader_data)
{
  MashPlyLoader *self = MASH_PLY_LOADER (data_loader);
  MashPlyLoaderPrivate *priv = self->priv;

  loader_data->primitive = cogl_object_ref (priv->primitive);

  loader_data->min_vertex = priv->min_vertex;
  loader_data->max_vertex = priv->max_vertex;
}
