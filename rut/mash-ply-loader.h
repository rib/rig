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

#ifndef __MASH_PLY_LOADER_H__
#define __MASH_PLY_LOADER_H__

#include "mash-data-loader.h"

G_BEGIN_DECLS

#define MASH_TYPE_PLY_LOADER                          \
  (mash_ply_loader_get_type())
#define MASH_PLY_LOADER(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                 \
                               MASH_TYPE_PLY_LOADER,  \
                               MashPlyLoader))
#define MASH_PLY_LOADER_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                  \
                            MASH_TYPE_PLY_LOADER,     \
                            MashPlyLoaderClass))
#define MASH_IS_PLY_LOADER(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                 \
                               MASH_TYPE_PLY_LOADER))
#define MASH_IS_PLY_LOADER_CLASS(klass)                 \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                    \
                            MASH_TYPE_PLY_LOADER))
#define MASH_PLY_LOADER_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                  \
                              MASH_TYPE_PLY_LOADER,   \
                              MashPlyLoaderClass))

typedef struct _MashPlyLoader        MashPlyLoader;
typedef struct _MashPlyLoaderClass   MashPlyLoaderClass;
typedef struct _MashPlyLoaderPrivate MashPlyLoaderPrivate;

struct _MashPlyLoaderClass
{
  /*< private >*/
  MashDataLoaderClass parent_class;
};

struct _MashPlyLoader
{
  /*< private >*/
  GObject parent;

  MashPlyLoaderPrivate *priv;
};

GType mash_ply_loader_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __MASH_PLY_LOADER_H__ */
