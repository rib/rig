/*
 * Rut - Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
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

#include <glib-object.h>
#include <string.h>
#include <cogl/cogl.h>

#include "rply.h"

#include "rut-mesh-ply.h"
#include "rut-interfaces.h"

typedef struct _LoaderAttribute
{
  const char *name;

  RutAttributeType type;
  size_t offset;
  size_t size;
  int n_components;
  CoglBool padding;

  RutAttribute *rut_attribute;

} LoaderAttribute;

typedef struct _LoaderProperty
{
  int component;
  const char *name;
  LoaderAttribute *loader_attribute;
} LoaderProperty;

typedef struct _Loader
{
  RutContext *ctx;
  p_ply ply;
  GError *error;

  LoaderAttribute *loader_attributes;
  LoaderProperty *loader_properties;

  unsigned int n_vertex_bytes;
  RutBuffer *vertex_buffer;

  uint8_t *current_vertex_pos;
  CoglBool read_property;

  unsigned int first_vertex, last_vertex;
  GArray *faces;
  CoglIndicesType indices_type;

} Loader;

GQuark
rut_mesh_ply_error_quark (void)
{
  return g_quark_from_static_string ("rut-mesh-ply-error-quark");
}

static void
rut_mesh_ply_loader_error_cb (const char *message, void *_loader)
{
  Loader *loader = _loader;

  if (loader->error == NULL)
    g_set_error_literal (&loader->error, RUT_MESH_PLY_ERROR,
                         RUT_MESH_PLY_ERROR_UNKNOWN, message);
}

static int
get_sizeof_attribute_type (RutAttributeType type)
{
  switch (type)
    {
    case RUT_ATTRIBUTE_TYPE_BYTE:
    case RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
      return 1;
    case RUT_ATTRIBUTE_TYPE_SHORT:
    case RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
      return 2;
    case RUT_ATTRIBUTE_TYPE_FLOAT:
      return 4;
    }

  g_warn_if_reached ();
  return 0;
}

static int
rut_mesh_ply_loader_vertex_read_cb (p_ply_argument argument)
{
  long prop_num;
  Loader *loader;
  LoaderProperty *loader_property;
  LoaderAttribute *loader_attribute;
  uint8_t *pos;
  double value;

  ply_get_argument_user_data (argument, (void **) &loader, &prop_num);

  loader_property = &loader->loader_properties[prop_num];
  loader_attribute = loader_property->loader_attribute;

  if (loader->read_property && prop_num == 0)
    loader->current_vertex_pos += loader->n_vertex_bytes;

  pos = loader->current_vertex_pos + loader_attribute->offset;
  pos += get_sizeof_attribute_type (loader_attribute->type) *
    loader_property->component;

  value = ply_get_argument_value (argument);
  switch (loader_attribute->type)
    {
    case RUT_ATTRIBUTE_TYPE_BYTE:
      *((int8_t *)pos) = value;
      break;
    case RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
      *pos = value;
      break;
    case RUT_ATTRIBUTE_TYPE_SHORT:
      *((int16_t *)pos) = value;
      break;
    case RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
      *((uint16_t *)pos) = value;
      break;
    case RUT_ATTRIBUTE_TYPE_FLOAT:
      *((float *)pos) = value;
    }

  loader->read_property = TRUE;

  return 1;
}

static void
rut_mesh_ply_loader_add_face_index (Loader *loader, unsigned int index)
{
  switch (loader->indices_type)
    {
    case COGL_INDICES_TYPE_UNSIGNED_BYTE:
      {
        uint8_t value = index;
        g_array_append_val (loader->faces, value);
      }
      break;
    case COGL_INDICES_TYPE_UNSIGNED_SHORT:
      {
        uint16_t value = index;
        g_array_append_val (loader->faces, value);
      }
      break;
    case COGL_INDICES_TYPE_UNSIGNED_INT:
      {
        uint32_t value = index;
        g_array_append_val (loader->faces, value);
      }
      break;
    }
}

static int
rut_mesh_ply_loader_face_read_cb (p_ply_argument argument)
{
  long prop_num;
  Loader *loader;
  int32_t length, index;

  ply_get_argument_user_data (argument, (void **) &loader, &prop_num);
  ply_get_argument_property (argument, NULL, &length, &index);

  if (index == 0)
    loader->first_vertex = ply_get_argument_value (argument);
  else if (index == 1)
    loader->last_vertex = ply_get_argument_value (argument);
  else if (index != -1)
    {
      unsigned int new_vertex = ply_get_argument_value (argument);

      /* Add a triangle with the first vertex, the last vertex and
         this new vertex */
      rut_mesh_ply_loader_add_face_index (loader, loader->first_vertex);
      rut_mesh_ply_loader_add_face_index (loader, loader->last_vertex);
      rut_mesh_ply_loader_add_face_index (loader, new_vertex);

      /* Use the new vertex as one of the vertices next time around */
      loader->last_vertex = new_vertex;
    }

  return 1;
}

static RutAttributeType
get_attribute_type_for_ply_type (e_ply_type type)
{
   switch (type)
    {
    case PLY_INT8:
      return RUT_ATTRIBUTE_TYPE_BYTE;
    case PLY_UINT8:
    case PLY_CHAR:
    case PLY_UCHAR:
      return RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE;
    case PLY_INT16:
      return RUT_ATTRIBUTE_TYPE_SHORT;
    case PLY_UINT16:
    case PLY_SHORT:
    case PLY_USHORT:
      return RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT;
    case PLY_INT32:
    case PLY_UIN32:
    case PLY_FLOAT32:
    case PLY_INT:
    case PLY_UINT:
    case PLY_FLOAT:
    case PLY_FLOAT64:
    case PLY_DOUBLE:
      return RUT_ATTRIBUTE_TYPE_FLOAT;
    case PLY_LIST:
      g_warn_if_reached ();
      return 0;
    }

  g_warn_if_reached ();
  return 0;
}

static p_ply_element
find_element (Loader *loader, const char *element_name)
{
  p_ply_element element = NULL;
  while ((element = ply_get_next_element (loader->ply, element)))
    {
      const char *name;

      ply_get_element_info (element, &name, NULL);

      if (strcmp (name, element_name) == 0)
        return element;
    }

  return NULL;
}

static p_ply_property
find_property (p_ply_element element, const char *property_name)
{
  p_ply_property property = NULL;
  while ((property = ply_get_next_property (element, property)))
    {
      const char *name;

      ply_get_property_info (property, &name, NULL, NULL, NULL);

      if (strcmp (name, property_name) == 0)
        return property;
    }

  return NULL;
}

static CoglBool
init_indices_array (Loader *loader,
                    int n_vertices,
                    GError **error)
{
  if (n_vertices <= 0x100)
    {
      loader->indices_type = COGL_INDICES_TYPE_UNSIGNED_BYTE;
      loader->faces = g_array_new (FALSE, FALSE, sizeof (uint8_t));
    }
  else if (n_vertices <= 0x10000)
    {
      loader->indices_type = COGL_INDICES_TYPE_UNSIGNED_SHORT;
      loader->faces = g_array_new (FALSE, FALSE, sizeof (uint16_t));
    }
  else if (cogl_has_feature (loader->ctx->cogl_context,
                             COGL_FEATURE_ID_UNSIGNED_INT_INDICES))
    {
      loader->indices_type = COGL_INDICES_TYPE_UNSIGNED_INT;
      loader->faces = g_array_new (FALSE, FALSE, sizeof (uint32_t));
    }
  else
    {
      g_set_error (error, RUT_MESH_PLY_ERROR,
                   RUT_MESH_PLY_ERROR_UNSUPPORTED,
                   "The PLY file requires unsigned int indices "
                   "but this is not supported by the driver");
      return FALSE;
    }

  return TRUE;
}

static RutMesh *
_rut_mesh_new_from_p_ply (RutContext *ctx,
                          Loader *loader,
                          p_ply ply,
                          const char *display_name,
                          RutPLYAttribute *attributes,
                          int n_attributes,
                          RutPLYAttributeStatus *load_status,
                          GError **error)
{
  LoaderAttribute loader_attributes[n_attributes];
  int n_loader_attributes;
  LoaderProperty loader_properties[n_attributes *
                                   RUT_PLY_MAX_ATTRIBUTE_PROPERTIES];
  RutAttribute *rut_attributes[n_attributes];
  p_ply_element vertex_element;
  RutBuffer *indices_buffer;
  RutMesh *mesh = NULL;
  int i;
  int32_t n_vertices;
  int max_component_size = 1;

  memset (rut_attributes, 0, sizeof (void *) * n_attributes);

  loader->ctx = ctx;
  loader->loader_attributes = loader_attributes;
  loader->loader_properties = loader_properties;

  loader->ply = ply;

  if (!ply_read_header (loader->ply))
    {
      g_set_error (&loader->error, RUT_MESH_PLY_ERROR,
                   RUT_MESH_PLY_ERROR_UNKNOWN,
                   "Failed to parse header of PLY file %s", display_name);
      goto EXIT;
    }

  vertex_element = find_element (loader, "vertex");
  if (!vertex_element)
    {
      g_set_error (&loader->error, RUT_MESH_PLY_ERROR,
                   RUT_MESH_PLY_ERROR_MISSING_PROPERTY,
                   "PLY file %s is missing the vertex properties",
                   display_name);
      goto EXIT;
    }

  ply_get_element_info (vertex_element, NULL, &n_vertices);

  if (!init_indices_array (loader, n_vertices, &loader->error))
    goto EXIT;

  /* Group properties into attributes */

  n_loader_attributes = 0;
  for (i = 0; i < n_attributes; i++)
    {
      RutPLYAttribute *attribute = &attributes[i];
      int j;
      int n_components = 0;
      LoaderAttribute *loader_attribute;
      e_ply_type ply_attribute_type;
      int component_size;

      for (j = 0; j < attribute->n_properties; j++)
        {
          RutPLYProperty *property = &attribute->properties[j];
          p_ply_property ply_prop;
          e_ply_type ply_property_type;

          ply_prop = find_property (vertex_element, property->name);
          if (!ply_prop)
            break;

          n_components++;

          ply_get_property_info (ply_prop, NULL, &ply_property_type, NULL, NULL);
          if (n_components == 1)
            ply_attribute_type = ply_property_type;
          else if (ply_property_type != ply_attribute_type)
            {
              g_set_error (&loader->error, RUT_MESH_PLY_ERROR,
                           RUT_MESH_PLY_ERROR_INVALID,
                           "Mismatching attribute property types "
                           "in PLY file %s", display_name);
              goto EXIT;
            }
        }

      if (n_components == 0 && attribute->pad_n_components)
        {
          n_components = attribute->pad_n_components;
          load_status[i] = RUT_PLY_ATTRIBUTE_STATUS_PADDED;
        }
      else if (n_components < attribute->min_components)
        load_status[i] = RUT_PLY_ATTRIBUTE_STATUS_MISSING;
      else
        load_status[i] = RUT_PLY_ATTRIBUTE_STATUS_LOADED;

      if (load_status[i] != RUT_PLY_ATTRIBUTE_STATUS_LOADED &&
          attribute->required)
        {
          g_set_error (&loader->error, RUT_MESH_PLY_ERROR,
                       RUT_MESH_PLY_ERROR_INVALID,
                       "Required attribute properties not found in PLY file %s",
                       display_name);
          goto EXIT;
        }

      if (load_status[i] == RUT_PLY_ATTRIBUTE_STATUS_MISSING)
        continue;

      loader_attribute = &loader_attributes[n_loader_attributes];
      loader_attribute->name = attribute->name;
      if (load_status[i] == RUT_PLY_ATTRIBUTE_STATUS_PADDED)
        {
          loader_attribute->type = attribute->pad_type;
          loader_attribute->padding = TRUE;
        }
      else
        {
          if (ply_attribute_type == PLY_LIST)
            {
              g_set_error (&loader->error, RUT_MESH_PLY_ERROR,
                           RUT_MESH_PLY_ERROR_INVALID,
                           "List property given for vertex attribute "
                           "in PLY file %s", display_name);
              goto EXIT;
            }

          loader_attribute->type =
            get_attribute_type_for_ply_type (ply_attribute_type);
          loader_attribute->padding = FALSE;

          for (j = 0; j < n_components; j++)
            {
              int p = n_loader_attributes * RUT_PLY_MAX_ATTRIBUTE_PROPERTIES + j;
              LoaderProperty *loader_property = &loader_properties[p];
              loader_property->component = j;
              loader_property->name = attribute->properties[j].name;
              loader_property->loader_attribute = loader_attribute;
            }
        }

      loader_attribute->n_components = n_components;
      component_size =  get_sizeof_attribute_type (loader_attribute->type);
      loader_attribute->size = component_size * n_components;

      if (component_size > max_component_size)
        max_component_size = component_size;

      /* Align the offset according to the natural alignment of the
       * attribute type... */
      loader->n_vertex_bytes =
        (loader->n_vertex_bytes + component_size - 1) &
        ~(unsigned int) (component_size - 1);

      loader_attribute->offset = loader->n_vertex_bytes;
      loader->n_vertex_bytes += loader_attribute->size;

      n_loader_attributes++;
    }

  /* Align the size of a vertex to the size of the largest component type */
  loader->n_vertex_bytes = ((loader->n_vertex_bytes + max_component_size - 1) &
                           ~(unsigned int) (max_component_size - 1));

  loader->vertex_buffer = rut_buffer_new (loader->n_vertex_bytes * n_vertices);
  loader->current_vertex_pos = loader->vertex_buffer->data;

  /* Now that we know what attributes we are loading and their size we
   * know the full vertex size so we can create corresponding
   * RutAttributes */

  for (i = 0; i < n_loader_attributes; i++)
    {
      LoaderAttribute *loader_attribute = &loader_attributes[i];
      RutAttribute *rut_attribute;
      int j;

      if (!loader_attribute->padding)
        {
            for (j = 0; j < loader_attribute->n_components; j++)
              {
                int p = i * RUT_PLY_MAX_ATTRIBUTE_PROPERTIES + j;
                LoaderProperty *loader_property = &loader_properties[p];

                if (!ply_set_read_cb (loader->ply, "vertex",
                                      loader_property->name,
                                      rut_mesh_ply_loader_vertex_read_cb,
                                      loader, p))
                  {
                    g_set_error (&loader->error, RUT_MESH_PLY_ERROR,
                                 RUT_MESH_PLY_ERROR_UNKNOWN,
                                 "Failed to parse PLY file %s", display_name);
                    goto EXIT;
                  }
              }
        }

      rut_attribute =
        rut_attribute_new (loader->vertex_buffer,
                           loader_attribute->name,
                           loader->n_vertex_bytes,
                           loader_attribute->offset,
                           loader_attribute->n_components,
                           loader_attribute->type);

      loader_attribute->rut_attribute = rut_attribute;
      rut_attributes[i] = rut_attribute;
    }

  if (!ply_set_read_cb (loader->ply, "face", "vertex_indices",
                        rut_mesh_ply_loader_face_read_cb,
                        loader, i))
    {
      g_set_error (&loader->error, RUT_MESH_PLY_ERROR,
                   RUT_MESH_PLY_ERROR_MISSING_PROPERTY,
                   "PLY file %s is missing face property "
                   "'vertex_indices'",
                   display_name);
      goto EXIT;
    }

  if (!ply_read (loader->ply))
    {
      g_set_error (&loader->error, RUT_MESH_PLY_ERROR,
                   RUT_MESH_PLY_ERROR_UNKNOWN,
                   "Unknown error loading PLY file %s", display_name);
      goto EXIT;
    }

  ply_close (loader->ply);

  if (loader->faces->len == 0)
    {
      g_set_error (&loader->error, RUT_MESH_PLY_ERROR,
                   RUT_MESH_PLY_ERROR_INVALID,
                   "No faces found in PLY file %s",
                   display_name);
      goto EXIT;
    }

  mesh = rut_mesh_new (COGL_VERTICES_MODE_TRIANGLES,
                       n_vertices,
                       rut_attributes,
                       n_loader_attributes);

  indices_buffer = rut_buffer_new (loader->faces->len *
                                   g_array_get_element_size (loader->faces));
  memcpy (indices_buffer->data, loader->faces->data, indices_buffer->size);

  rut_mesh_set_indices (mesh,
                        loader->indices_type,
                        indices_buffer,
                        loader->faces->len);

  rut_object_unref (indices_buffer);

EXIT:

  if (loader->error)
    g_propagate_error (error, loader->error);

  if (loader->vertex_buffer)
    rut_object_unref (loader->vertex_buffer);

  for (i = 0; i < n_loader_attributes; i++)
    if (rut_attributes[i])
      rut_object_unref (rut_attributes[i]);

  if (loader->faces)
    g_array_free (loader->faces, TRUE);

  return mesh;
}

RutMesh *
rut_mesh_new_from_ply (RutContext *ctx,
                       const char *filename,
                       RutPLYAttribute *attributes,
                       int n_attributes,
                       RutPLYAttributeStatus *load_status,
                       GError **error)
{
  Loader loader;
  p_ply ply;
  RutMesh *mesh;
  char *display_name;

  memset (&loader, 0, sizeof (Loader));

  ply = ply_open (filename, rut_mesh_ply_loader_error_cb, error);

  if (!ply)
    return NULL;

  display_name = g_filename_display_name (filename);

  mesh = _rut_mesh_new_from_p_ply (ctx,
                                   &loader,
                                   ply,
                                   display_name,
                                   attributes,
                                   n_attributes,
                                   load_status,
                                   error);

  g_free (display_name);

  return mesh;
}

RutMesh *
rut_mesh_new_from_ply_data (RutContext *ctx,
                            const uint8_t *data,
                            size_t len,
                            RutPLYAttribute *attributes,
                            int n_attributes,
                            RutPLYAttributeStatus *load_status,
                            GError **error)
{
  Loader loader;
  p_ply ply;
  RutMesh *mesh;
  char *display_name;

  memset (&loader, 0, sizeof (Loader));

  ply = ply_start (data, len, rut_mesh_ply_loader_error_cb, error);

  if (!ply)
    return NULL;

  display_name = g_strdup_printf ("<serialized asset %p>", data);

  mesh = _rut_mesh_new_from_p_ply (ctx,
                                   &loader,
                                   ply,
                                   display_name,
                                   attributes,
                                   n_attributes,
                                   load_status,
                                   error);

  g_free (display_name);

  return mesh;
}
