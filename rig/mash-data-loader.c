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
 * SECTION:mash-data-loader
 * @short_description: An object for loading data from a file in a specific format.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <string.h>
#include <cogl/cogl.h>

#include "mash-data-loader.h"

G_DEFINE_ABSTRACT_TYPE (MashDataLoader, mash_data_loader, G_TYPE_OBJECT);

#define MASH_DATA_LOADER_GET_PRIVATE(obj)                      \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MASH_TYPE_DATA_LOADER,  \
                                MashDataLoaderPrivate))

/* reserved for future use */
struct _MashDataLoaderPrivate
{
};

static void
mash_data_loader_class_init (MashDataLoaderClass *klass)
{
}

static void
mash_data_loader_init (MashDataLoader *self)
{
}

/**
 * mash_data_loader_load:
 * @data_loader: The #MashDataLoader instance
 * @flags: Flags used to specify load-time modifications to the data
 * @filename: The name of a file to load
 * @error: Return location for an error or %NULL
 *
 * Loads the data from the file called @filename into @self.
 * This function is not usually called by applications.
 */
gboolean
mash_data_loader_load (MashDataLoader *data_loader,
                       MashDataFlags flags,
                       const gchar *filename,
                       GError **error)
{
  g_return_val_if_fail (MASH_IS_DATA_LOADER (data_loader), FALSE);

  return MASH_DATA_LOADER_GET_CLASS (data_loader)->load (data_loader,
                                                         flags,
                                                         filename,
                                                         error);
}

/**
 * mash_data_loader_load:
 * @data_loader: The #MashDataLoader instance
 * @loader_data: The #MashDataLoaderData to set the loaded data.
 *
 * Obtains the loaded data after calling mash_data_loader_load().
 * This function is not usually called by applications.
 */
void
mash_data_loader_get_data (MashDataLoader *data_loader,
                           MashDataLoaderData *loader_data)
{
  g_return_if_fail (MASH_IS_DATA_LOADER (data_loader));
  g_return_if_fail (loader_data != NULL);

  MASH_DATA_LOADER_GET_CLASS (data_loader)->get_data (data_loader, loader_data);
}
