/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef _RUT_MESH_H_
#define _RUT_MESH_H_

#include <clib.h>

#include <cglib/cglib.h>

typedef struct _rut_buffer_t rut_buffer_t;
typedef struct _rut_attribute_t rut_attribute_t;
typedef struct _rut_mesh_t rut_mesh_t;

#include "rut-shell.h"

typedef enum {
    RUT_ATTRIBUTE_TYPE_BYTE,
    RUT_ATTRIBUTE_TYPE_UNSIGNED_BYTE,
    RUT_ATTRIBUTE_TYPE_SHORT,
    RUT_ATTRIBUTE_TYPE_UNSIGNED_SHORT,
    RUT_ATTRIBUTE_TYPE_FLOAT
} rut_attribute_type_t;

extern rut_type_t rut_buffer_type;

struct _rut_buffer_t {
    rut_object_base_t _base;

    uint8_t *data;
    size_t size;
};

struct _rut_attribute_t {
    rut_object_base_t _base;

    const char *name;

    union {
        struct {
            rut_buffer_t *buffer;
            size_t stride;
            size_t offset;
            int n_components;
            rut_attribute_type_t type;
        } buffered;
        struct {
            int n_components;
            int n_columns;
            bool transpose;
            float value[16];
        } constant;
    };

    int instance_stride;

    unsigned int normalized : 1;
    unsigned int is_buffered : 1;
};

extern rut_type_t rut_mesh_type;

/* This kind of mesh is optimized for size and use by a GPU */
struct _rut_mesh_t {
    rut_object_base_t _base;

    cg_vertices_mode_t mode;
    rut_attribute_t **attributes;
    int n_attributes;

    /* NB: this always represents the amount of actual data so
     * you need to consider if indices != NULL then n_indices
     * says how many vertices should be drawn, including duplicates.
     */
    int n_vertices;

    /* optional */
    cg_indices_type_t indices_type;
    int n_indices;
    rut_buffer_t *indices_buffer;
};

void _rut_buffer_init_type(void);

rut_buffer_t *rut_buffer_new(size_t buffer_size);

void _rut_attribute_init_type(void);

rut_attribute_t *rut_attribute_new(rut_buffer_t *buffer,
                                   const char *name,
                                   size_t stride,
                                   size_t offset,
                                   int n_components,
                                   rut_attribute_type_t type);

rut_attribute_t *rut_attribute_new_const(const char *name,
                                         int n_components,
                                         int n_columns,
                                         bool transpose,
                                         const float *value);

void rut_attribute_set_normalized(rut_attribute_t *attribute, bool normalized);

void rut_attribute_set_instance_stride(rut_attribute_t *attribute, int stride);

void _rut_mesh_init_type(void);

rut_mesh_t *rut_mesh_new(cg_vertices_mode_t mode,
                         int n_vertices,
                         rut_attribute_t **attributes,
                         int n_attributes);

rut_mesh_t *rut_mesh_new_empty(void);

rut_mesh_t *rut_mesh_new_from_buffer_p3(cg_vertices_mode_t mode,
                                        int n_vertices,
                                        rut_buffer_t *buffer);

rut_mesh_t *rut_mesh_new_from_buffer_p3n3(cg_vertices_mode_t mode,
                                          int n_vertices,
                                          rut_buffer_t *buffer);

rut_mesh_t *rut_mesh_new_from_buffer_p3c4(cg_vertices_mode_t mode,
                                          int n_vertices,
                                          rut_buffer_t *buffer);

void rut_mesh_set_attributes(rut_mesh_t *mesh,
                             rut_attribute_t **attributes,
                             int n_attributes);

void rut_mesh_set_indices(rut_mesh_t *mesh,
                          cg_indices_type_t type,
                          rut_buffer_t *buffer,
                          int n_indices);

/* Performs a deep copy of all the buffers */
rut_mesh_t *rut_mesh_copy(rut_mesh_t *mesh);

cg_primitive_t *rut_mesh_create_primitive(rut_shell_t *shell,
					  rut_mesh_t *mesh);

rut_attribute_t *rut_mesh_find_attribute(rut_mesh_t *mesh,
                                         const char *attribute_name);

typedef bool (*rut_mesh_tvertex_callback_t)(void **attribute_data,
                                           int vertex_index,
                                           void *user_data);

void rut_mesh_foreach_vertex(rut_mesh_t *mesh,
                             rut_mesh_tvertex_callback_t callback,
                             void *user_data,
                             const char *first_attribute_name,
                             ...);
C_GNUC_NULL_TERMINATED

void rut_mesh_foreach_index(rut_mesh_t *mesh,
                            rut_mesh_tvertex_callback_t callback,
                            void *user_data,
                            const char *first_attribute_name,
                            ...) C_GNUC_NULL_TERMINATED;

typedef bool (*rut_mesh_triangle_callback_t)(void **attribute_data_v0,
                                             void **attribute_data_v1,
                                             void **attribute_data_v2,
                                             int v0_index,
                                             int v1_index,
                                             int v2_index,
                                             void *user_data);

void rut_mesh_foreach_triangle(rut_mesh_t *mesh,
                               rut_mesh_triangle_callback_t callback,
                               void *user_data,
                               const char *first_attribute,
                               ...) C_GNUC_NULL_TERMINATED;

#endif /* _RUT_MESH_H_ */
