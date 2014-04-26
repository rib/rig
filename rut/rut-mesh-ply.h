/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _RUT_MESH_PLY_
#define _RUT_MESH_PLY_

#include <cogl/cogl.h>

#include <clib.h>

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

