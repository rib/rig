/*
 * Mash - A library for displaying PLY models in a Clutter scene
 * Copyright (C) 2010  Intel Corporation
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

#ifndef __MASH_DATA_LOADER_H__
#define __MASH_DATA_LOADER_H__

#include <cogl/cogl.h>

#include "mash-data.h"

G_BEGIN_DECLS

#define MASH_TYPE_DATA_LOADER                   \
  (mash_data_get_type())
#define MASH_DATA_LOADER(obj)                           \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                   \
                               MASH_TYPE_DATA_LOADER,   \
                               MashDataLoader))
#define MASH_DATA_LOADER_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
                            MASH_TYPE_DATA_LOADER,     \
                            MashDataLoaderClass))
#define MASH_IS_DATA_LOADER(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
                               MASH_TYPE_DATA_LOADER))
#define MASH_IS_DATA_LOADER_CLASS(klass)               \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
                            MASH_TYPE_DATA_LOADER))
#define MASH_DATA_LOADER_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
                              MASH_TYPE_DATA_LOADER,   \
                              MashDataLoaderClass))

typedef struct _MashDataLoader        MashDataLoader;
typedef struct _MashDataLoaderClass   MashDataLoaderClass;
typedef struct _MashDataLoaderPrivate MashDataLoaderPrivate;
typedef struct _MashDataLoaderData    MashDataLoaderData;

/**
 * MashDataLoaderClass:
 * @load: Virtual used for loading the model from the file
 * @get_data: Virtual used to get the loaded data
 */
struct _MashDataLoaderClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  gboolean (* load) (MashDataLoader *data_loader,
                     MashDataFlags flags,
                     const gchar *filename,
                     GError **error);
  void (* get_data) (MashDataLoader *data_loader,
                     MashDataLoaderData *loader_data);
};

/**
 * MashDataLoader:
 *
 * The #MashDataLoader structure contains only private data.
 */
struct _MashDataLoader
{
  /*< private >*/
  GObject parent;

  MashDataLoaderPrivate *priv;
};

/**
 * MashDataLoaderData:
 *
 * The #MashDataLoaderData structure contains the loaded data.
 */
struct _MashDataLoaderData
{
  CoglPrimitive *primitive;

  /* Bounding cuboid of the data */
  CoglVertexP3 min_vertex, max_vertex;
};

GType mash_data_loader_get_type (void) G_GNUC_CONST;

gboolean mash_data_loader_load (MashDataLoader *self,
                                MashDataFlags flags,
                                const gchar *filename,
                                GError **error);

void mash_data_loader_get_data (MashDataLoader *self,
                                MashDataLoaderData *loader_data);

G_END_DECLS

#endif /* __MASH_DATA_LOADER_H__ */
