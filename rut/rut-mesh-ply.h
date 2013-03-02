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

#ifndef _RUT_MESH_PLY_
#define _RUT_MESH_PLY_

#include <cogl/cogl.h>

#include <glib.h>

#include "rut-mesh.h"

G_BEGIN_DECLS

#define RUT_MESH_PLY_ERROR rut_mesh_ply_error_quark ()

typedef enum _RutMeshPlyError
{
  RUT_MESH_PLY_ERROR_IO,
  RUT_MESH_PLY_ERROR_UNKNOWN,
  RUT_MESH_PLY_ERROR_MISSING_PROPERTY,
  RUT_MESH_PLY_ERROR_INVALID,
  RUT_MESH_PLY_ERROR_UNSUPPORTED
} RutMeshPlyError;

#define RUT_PLY_MAX_ATTRIBUTE_PROPERTIES 4

typedef struct _RutPLYProperty
{
  const char *name;
  CoglBool invert;
} RutPLYProperty;

typedef struct _RutPLYAttribute
{
  const char *name;

  RutPLYProperty properties[RUT_PLY_MAX_ATTRIBUTE_PROPERTIES];
  int n_properties;

  /* The minimum number of component properties that must be found
   * before we consider loading the attribute.
   *
   * If less properties are found the attribute will be skipped
   * unless required == TRUE. */
  int min_components;

  /* If TRUE and the minimum number of component properties for this
   * attribute aren't found then the loader will exit with an error
   */
  CoglBool required;

  /* If the minimum number of properties for this attribute aren't
   * found then if this is > 0 the loader will create padded space for
   * the attribute with room for this many components of this type...
   */
  int pad_n_components;
  RutAttributeType pad_type;

  /* For integer type attributes this determines if the values should
   * be interpreted as normalized values in the range [0,1] */
  CoglBool normalized;

} RutPLYAttribute;

typedef enum _RutPLYAttributeStatus
{
  /* The corresponding properties weren't found in the PLY file */
  RUT_PLY_ATTRIBUTE_STATUS_MISSING,
  /* The corresponding properties for this attribute were found
   * and loaded into the returned mesh */
  RUT_PLY_ATTRIBUTE_STATUS_LOADED,
  /* The corresponding properties weren't found in the PLY file
   * but the attribute was still added to mesh with uninitialized
   * padding space */
  RUT_PLY_ATTRIBUTE_STATUS_PADDED
} RutPLYAttributeStatus;

RutMesh *
rut_mesh_new_from_ply (RutContext *ctx,
                       const char *filename,
                       RutPLYAttribute *attributes,
                       int n_attributes,
                       RutPLYAttributeStatus *attribute_status_out,
                       GError **error);

RutMesh *
rut_mesh_new_from_ply_data (RutContext *ctx,
                            const uint8_t *data,
                            size_t len,
                            RutPLYAttribute *attributes,
                            int n_attributes,
                            RutPLYAttributeStatus *load_status,
                            GError **error);

G_END_DECLS

#endif /* _RUT_MESH_PLY_ */

