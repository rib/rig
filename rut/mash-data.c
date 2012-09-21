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

/**
 * SECTION:mash-data
 * @short_description: An object that contains the data for a model.
 *
 * #MashData is an object that can represent the data contained
 * in a 3D model file. The data is internally converted to a
 * Cogl vertex buffer so that it can be rendered efficiently.
 *
 * The #MashData object is usually associated with a
 * #MashModel so that it can be animated as a regular actor. The
 * data is separated from the actor in this way to make it easy to
 * share data with multiple actors without having to keep two copies
 * of the data.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <string.h>
#include <cogl/cogl.h>

#include "mash-data.h"
#include "mash-ply-loader.h"

static void mash_data_finalize (GObject *object);

G_DEFINE_TYPE (MashData, mash_data, G_TYPE_OBJECT);

#define MASH_DATA_GET_PRIVATE(obj)                      \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_DATA,  \
                                MashDataPrivate))

struct _MashDataPrivate
{
  MashDataLoaderData loaded_data;
};

static void
mash_data_class_init (MashDataClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = mash_data_finalize;

  g_type_class_add_private (klass, sizeof (MashDataPrivate));
}

static void
mash_data_init (MashData *self)
{
  self->priv = MASH_DATA_GET_PRIVATE (self);
}

static void
mash_data_free_vbos (MashData *self)
{
  MashDataPrivate *priv = self->priv;

  if (priv->loaded_data.primitive)
    {
      cogl_object_unref (priv->loaded_data.primitive);
      priv->loaded_data.primitive = NULL;
    }
}

static void
mash_data_finalize (GObject *object)
{
  MashData *self = (MashData *) object;

  mash_data_free_vbos (self);

  G_OBJECT_CLASS (mash_data_parent_class)->finalize (object);
}

/**
 * mash_data_new:
 *
 * Constructs a new #MashData instance. The object initially has
 * no data so nothing will be drawn when mash_data_render() is
 * called. To load data into the object, call mash_data_load().
 *
 * Return value: a new #MashData.
 */
MashData *
mash_data_new (void)
{
  MashData *self = g_object_new (MASH_TYPE_DATA, NULL);

  return self;
}

/**
 * mash_data_load:
 * @self: The #MashData instance
 * @flags: Flags used to specify load-time modifications to the data
 * @filename: The name of a file to load
 * @error: Return location for an error or %NULL
 *
 * Loads the data from the file called @filename into @self. The
 * model can then be rendered using mash_data_render(). If
 * there is an error loading the file it will return %FALSE and @error
 * will be set to a GError instance.
 *
 * Return value: %TRUE if the load succeeded or %FALSE otherwise.
 */
gboolean
mash_data_load (MashData *self,
                MashDataFlags flags,
                const gchar *filename,
                GError **error)
{
  MashDataPrivate *priv;
  MashDataLoader *loader;
  gchar *display_name;
  gboolean ret;

  g_return_val_if_fail (MASH_IS_DATA (self), FALSE);

  priv = self->priv;

  loader = NULL;
  display_name = g_filename_display_name (filename);

  if (g_str_has_suffix (filename, ".ply"))
    loader = g_object_new (MASH_TYPE_PLY_LOADER, NULL);

  if (loader != NULL)
    {
      if (!mash_data_loader_load (loader, flags, filename, error))
        ret = FALSE;
      else
        {
          /* Get rid of the old VBOs (if any) */
          mash_data_free_vbos (self);

          mash_data_loader_get_data (loader, &priv->loaded_data);
          ret = TRUE;
        }
    }
  else
    {
      /* Unknown file format */

      g_set_error (error, MASH_DATA_ERROR,
                   MASH_DATA_ERROR_UNKNOWN_FORMAT,
                   "Unknown format for file %s",
                   display_name);
      ret = FALSE;
    }

  g_free (display_name);
  if (loader)
    g_object_unref (loader);

  return ret;
}

/**
 * mash_data_get_primitive:
 * @self: A #MashData instance
 *
 * Returns: A #CoglPrimitive
 */
CoglPrimitive *
mash_data_get_primitive (MashData *self)
{
  MashDataPrivate *priv;

  g_return_val_if_fail (MASH_IS_DATA (self), NULL);

  priv = self->priv;

  /* Silently fail if we didn't load any data */
  return priv->loaded_data.primitive;
}

/**
 * mash_data_get_extents:
 * @self: A #MashData instance
 * @min_vertex: A location to return the minimum vertex
 * @max_vertex: A location to return the maximum vertex
 *
 * Gets the bounding cuboid of the vertices in @self. The cuboid is
 * represented by two vertices representing the minimum and maximum
 * extents. The x, y and z components of @min_vertex will contain the
 * minimum x, y and z values of all the vertices and @max_vertex will
 * contain the maximum. The extents of the model are cached so it is
 * cheap to call this function.
 */
void
mash_data_get_extents (MashData *self,
                       CoglVertexP3 *min_vertex,
                       CoglVertexP3 *max_vertex)
{
  MashDataPrivate *priv = self->priv;

  *min_vertex = priv->loaded_data.min_vertex;
  *max_vertex = priv->loaded_data.max_vertex;
}

GQuark
mash_data_error_quark (void)
{
  return g_quark_from_static_string ("mash-data-error-quark");
}
