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

#include <cglib/cglib.h>

#include <clib.h>

#include "rut-mesh.h"

C_BEGIN_DECLS

#define RUT_MESH_PLY_ERROR rut_mesh_ply_error_quark()

typedef enum _rut_mesh_ply_error_t {
    RUT_MESH_PLY_ERROR_IO,
    RUT_MESH_PLY_ERROR_UNKNOWN,
    RUT_MESH_PLY_ERROR_MISSING_PROPERTY,
    RUT_MESH_PLY_ERROR_INVALID,
    RUT_MESH_PLY_ERROR_UNSUPPORTED
} rut_mesh_ply_error_t;

#define RUT_PLY_MAX_ATTRIBUTE_PROPERTIES 4

typedef struct _rut_ply_property_t {
    const char *name;
    bool invert;
} rut_ply_property_t;

typedef struct _rut_ply_attribute_t {
    const char *name;

    rut_ply_property_t properties[RUT_PLY_MAX_ATTRIBUTE_PROPERTIES];
    int n_properties;

    /* The minimum number of component properties that must be found
     * before we consider loading the attribute.
     *
     * If less properties are found the attribute will be skipped
     * unless required == true. */
    int min_components;

    /* If true and the minimum number of component properties for this
     * attribute aren't found then the loader will exit with an error
     */
    bool required;

    /* If the minimum number of properties for this attribute aren't
     * found then if this is > 0 the loader will create padded space for
     * the attribute with room for this many components of this type...
     */
    int pad_n_components;
    rut_attribute_type_t pad_type;

    /* For integer type attributes this determines if the values should
     * be interpreted as normalized values in the range [0,1] */
    bool normalized;

} rut_ply_attribute_t;

typedef enum _rut_ply_attribute_status_t {
    /* The corresponding properties weren't found in the PLY file */
    RUT_PLY_ATTRIBUTE_STATUS_MISSING,
    /* The corresponding properties for this attribute were found
     * and loaded into the returned mesh */
    RUT_PLY_ATTRIBUTE_STATUS_LOADED,
    /* The corresponding properties weren't found in the PLY file
     * but the attribute was still added to mesh with uninitialized
     * padding space */
    RUT_PLY_ATTRIBUTE_STATUS_PADDED
} rut_ply_attribute_status_t;

rut_mesh_t *
rut_mesh_new_from_ply(rut_shell_t *shell,
                      const char *filename,
                      rut_ply_attribute_t *attributes,
                      int n_attributes,
                      rut_ply_attribute_status_t *attribute_status_out,
                      c_error_t **error);

rut_mesh_t *rut_mesh_new_from_ply_data(rut_shell_t *shell,
                                       const uint8_t *data,
                                       size_t len,
                                       rut_ply_attribute_t *attributes,
                                       int n_attributes,
                                       rut_ply_attribute_status_t *load_status,
                                       c_error_t **error);

C_END_DECLS

#endif /* _RUT_MESH_PLY_ */
