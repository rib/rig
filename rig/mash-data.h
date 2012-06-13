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

#ifndef __MASH_DATA_H__
#define __MASH_DATA_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MASH_TYPE_DATA                          \
  (mash_data_get_type())
#define MASH_DATA(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
                               MASH_TYPE_DATA,  \
                               MashData))
#define MASH_DATA_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
                            MASH_TYPE_DATA,     \
                            MashDataClass))
#define MASH_IS_DATA(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
                               MASH_TYPE_DATA))
#define MASH_IS_DATA_CLASS(klass)               \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
                            MASH_TYPE_DATA))
#define MASH_DATA_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
                              MASH_TYPE_DATA,   \
                              MashDataClass))

/**
 * MASH_DATA_ERROR:
 *
 * Error domain for #MashData errors
 */
#define MASH_DATA_ERROR mash_data_error_quark ()

typedef struct _MashData        MashData;
typedef struct _MashDataClass   MashDataClass;
typedef struct _MashDataPrivate MashDataPrivate;

/**
 * MashDataClass:
 *
 * The #MashDataClass structure contains only private data.
 */
struct _MashDataClass
{
  /*< private >*/
  GObjectClass parent_class;
};

/**
 * MashData:
 *
 * The #MashData structure contains only private data.
 */
struct _MashData
{
  /*< private >*/
  GObject parent;

  MashDataPrivate *priv;
};

/**
 * MashDataError:
 * @MASH_DATA_ERROR_UNKNOWN_FORMAT: The file has an unknown format.
 * @MASH_DATA_ERROR_UNKNOWN: The underlying library reported an error.
 * @MASH_DATA_ERROR_MISSING_PROPERTY: A property that is needed
 *  by #MashData is not present in the file. For example, this
 *  will happen if the file does not contain the x, y and z properties.
 * @MASH_DATA_ERROR_INVALID: The file is not valid.
 * @MASH_DATA_ERROR_UNSUPPORTED: The file is not supported
 *  by your GL driver. This will happen if your driver can't support
 *  GL_UNSIGNED_INT indices but the model has more than 65,536
 *  vertices.
 *
 * Error enumeration for #MashData
 */
typedef enum
  {
    MASH_DATA_ERROR_UNKNOWN_FORMAT,
    MASH_DATA_ERROR_UNKNOWN,
    MASH_DATA_ERROR_MISSING_PROPERTY,
    MASH_DATA_ERROR_INVALID,
    MASH_DATA_ERROR_UNSUPPORTED
  } MashDataError;

/**
 * MashDataFlags:
 * @MASH_DATA_NONE: No flags
 * @MASH_DATA_NEGATE_X: Negate the X axis
 * @MASH_DATA_NEGATE_Y: Negate the Y axis
 * @MASH_DATA_NEGATE_Z: Negate the Z axis
 *
 * Flags used for modifying the data as it is loaded. These can be
 * passed to mash_data_load().
 *
 * If any of the negate flags are set then they cause the vertex and
 * normal coordinates for the specified axis to be negated. This could
 * be useful when loading a model from a tool which uses a different
 * coordinate system than the one used in your application. For
 * example, in Blender if the view is rotated such that the x-axis is
 * pointing to the right, and the z-axis is pointing out of the screen
 * then y-axis would be pointing directly up. However in Clutter the
 * default transformation is set up such that the y-axis would be
 * pointing down. Therefore if a model is loaded from Blender it would
 * appear upside-down. Also all of the front faces would be in
 * clockwise order. If backface culling is then enabled then the wrong
 * faces would be culled with the default Cogl settings.
 *
 * To avoid these issues when exporting from Blender it is common to
 * pass the %MASH_DATA_NEGATE_Y flag.
 */
/* The flip flags must be in sequential order */
typedef enum
  {
    MASH_DATA_NONE = 0,
    MASH_DATA_NEGATE_X = 1,
    MASH_DATA_NEGATE_Y = 2,
    MASH_DATA_NEGATE_Z = 4
  } MashDataFlags;

GType mash_data_get_type (void) G_GNUC_CONST;

MashData *mash_data_new (void);

gboolean mash_data_load (MashData *self,
                         MashDataFlags flags,
                         const gchar *filename,
                         GError **error);

CoglPrimitive * mash_data_get_primitive (MashData *self);

GQuark mash_data_error_quark (void);

void mash_data_get_extents (MashData *self,
                            CoglVertexP3 *min_vertex,
                            CoglVertexP3 *max_vertex);

G_END_DECLS

#endif /* __MASH_DATA_H__ */
