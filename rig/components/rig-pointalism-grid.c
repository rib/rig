/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
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

#include <rig-config.h>

#include <math.h>
#include <stdlib.h>

#include <rut.h>

#include "rig-entity.h"
#include "rig-entity-inlines.h"
#include "rig-pointalism-grid.h"

#define MESA_CONST_ATTRIB_BUG_WORKAROUND

static rig_property_spec_t _rig_pointalism_grid_prop_specs[] = {
    { .name = "pointalism-scale",
      .nick = "Pointalism Scale Factor",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_pointalism_grid_get_scale,
      .setter.float_type = rig_pointalism_grid_set_scale,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, 100 } },
      .animatable = true },
    { .name = "pointalism-z",
      .nick = "Pointalism Z Factor",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_pointalism_grid_get_z,
      .setter.float_type = rig_pointalism_grid_set_z,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, 100 } },
      .animatable = true },
    { .name = "pointalism-lighter",
      .nick = "Pointalism Lighter",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_pointalism_grid_get_lighter,
      .setter.boolean_type = rig_pointalism_grid_set_lighter,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "pointalism-cell-size",
      .nick = "Cell Size",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .getter.float_type = rig_pointalism_grid_get_cell_size,
      .setter.float_type = rig_pointalism_grid_set_cell_size,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 1, 100 } },
      .animatable = true },
    { NULL }
};

typedef struct _Gridvertex_t {
    float x0, y0;
    float x1, y1;
    float s0, t0;
    float s1, s2, t1, t2;
    float s3, t3;
#ifdef MESA_CONST_ATTRIB_BUG_WORKAROUND
    float nx, ny, nz;
    float tx, ty, tz;
#endif
} Gridvertex_t;

static rut_mesh_t *
mesh_new_grid(cg_vertices_mode_t mode,
              int n_vertices,
              int n_indices,
              Gridvertex_t *vertices,
              unsigned int *indices)
{
    rut_mesh_t *mesh;
    rut_attribute_t *attributes[10];
    rut_buffer_t *vertex_buffer;
    rut_buffer_t *index_buffer;

    vertex_buffer = rut_buffer_new(sizeof(Gridvertex_t) * n_vertices);
    index_buffer = rut_buffer_new(sizeof(unsigned int) * n_indices);

    memcpy(vertex_buffer->data, vertices, sizeof(Gridvertex_t) * n_vertices);
    memcpy(index_buffer->data, indices, sizeof(unsigned int) * n_indices);

    attributes[0] = rut_attribute_new(vertex_buffer,
                                      "cg_position_in",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, x0),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[1] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord0_in",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, s0),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[2] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord1_in",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, s3),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[3] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord4_in",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, s3),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[4] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord7_in",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, s3),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[5] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord11_in",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, s0),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[6] = rut_attribute_new(vertex_buffer,
                                      "cg_normal_in",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, nx),
                                      3,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[7] = rut_attribute_new(vertex_buffer,
                                      "tangent_in",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, tx),
                                      3,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[8] = rut_attribute_new(vertex_buffer,
                                      "cell_xy",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, x1),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[9] = rut_attribute_new(vertex_buffer,
                                      "cell_st",
                                      sizeof(Gridvertex_t),
                                      offsetof(Gridvertex_t, s1),
                                      4,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    mesh = rut_mesh_new(mode, n_vertices, attributes, 10);
    rut_mesh_set_indices(
        mesh, CG_INDICES_TYPE_UNSIGNED_INT, index_buffer, n_indices);

    c_free(vertices);
    c_free(indices);

    return mesh;
}

void
create_meshes(rig_pointalism_grid_t *grid)
{
    int tex_width = grid->tex_width;
    int tex_height = grid->tex_height;
    float size = grid->cell_size;
    int columns = fabs(tex_width / size);
    int rows = fabs(tex_height / size);
    int i, j, k, l;
    float cell_s = 1.0 / columns;
    float cell_t = 1.0 / rows;
    float start_x = -1.0 * ((size * columns) / 2.0);
    float start_y = -1.0 * ((size * rows) / 2.0);
    Gridvertex_t *vertices = c_new(Gridvertex_t, (columns * rows) * 4);
    unsigned int *indices = c_new(unsigned int, (columns * rows) * 6);
    rut_buffer_t *pick_mesh_buffer;
    cg_vertex_p3_t *pick_vertices;
    float half_tex_width;
    float half_tex_height;

    k = 0;
    l = 0;

    for (i = 0; i < rows; i++) {
        for (j = 0; j < columns; j++) {
            float x = start_x + (size / 2);
            float y = start_y + (size / 2);

            vertices[k].x0 = -1 * (size / 2);
            vertices[k].y0 = -1 * (size / 2);
            vertices[k].s0 = 0;
            vertices[k].t0 = 0;
            vertices[k].x1 = x;
            vertices[k].y1 = y;
            vertices[k].s1 = j * cell_s;
            vertices[k].t1 = i * cell_t;
            vertices[k].s2 = (j + 1) * cell_s;
            vertices[k].t2 = (i + 1) * cell_t;
            vertices[k].s3 = j * cell_s;
            vertices[k].t3 = i * cell_t;
            vertices[k].nx = 0;
            vertices[k].ny = 0;
            vertices[k].nz = 1;
            vertices[k].tx = 1;
            vertices[k].ty = 0;
            vertices[k].tz = 0;
            k++;

            vertices[k].x0 = size / 2;
            vertices[k].y0 = -1 * (size / 2);
            vertices[k].s0 = 1;
            vertices[k].t0 = 0;
            vertices[k].x1 = x;
            vertices[k].y1 = y;
            vertices[k].s1 = j * cell_s;
            vertices[k].t1 = i * cell_t;
            vertices[k].s2 = (j + 1) * cell_s;
            vertices[k].t2 = (i + 1) * cell_t;
            vertices[k].s3 = (j + 1) * cell_s;
            vertices[k].t3 = i * cell_t;
            vertices[k].nx = 0;
            vertices[k].ny = 0;
            vertices[k].nz = 1;
            vertices[k].tx = 1;
            vertices[k].ty = 0;
            vertices[k].tz = 0;
            k++;

            vertices[k].x0 = size / 2;
            vertices[k].y0 = size / 2;
            vertices[k].s0 = 1;
            vertices[k].t0 = 1;
            vertices[k].x1 = x;
            vertices[k].y1 = y;
            vertices[k].s1 = j * cell_s;
            vertices[k].t1 = i * cell_t;
            vertices[k].s2 = (j + 1) * cell_s;
            vertices[k].t2 = (i + 1) * cell_t;
            vertices[k].s3 = (j + 1) * cell_s;
            vertices[k].t3 = (i + 1) * cell_t;
            vertices[k].nx = 0;
            vertices[k].ny = 0;
            vertices[k].nz = 1;
            vertices[k].tx = 1;
            vertices[k].ty = 0;
            vertices[k].tz = 0;
            k++;

            vertices[k].x0 = -1 * (size / 2);
            vertices[k].y0 = size / 2;
            vertices[k].s0 = 0;
            vertices[k].t0 = 1;
            vertices[k].x1 = x;
            vertices[k].y1 = y;
            vertices[k].s1 = j * cell_s;
            vertices[k].t1 = i * cell_t;
            vertices[k].s2 = (j + 1) * cell_s;
            vertices[k].t2 = (i + 1) * cell_t;
            vertices[k].s3 = j * cell_s;
            vertices[k].t3 = (i + 1) * cell_t;
            vertices[k].nx = 0;
            vertices[k].ny = 0;
            vertices[k].nz = 1;
            vertices[k].tx = 1;
            vertices[k].ty = 0;
            vertices[k].tz = 0;
            k++;

            indices[l] = k;
            indices[l + 1] = k + 1;
            indices[l + 2] = k + 2;
            indices[l + 3] = k + 2;
            indices[l + 4] = k + 3;
            indices[l + 5] = k;

            l += 6;
            start_x += size;
        }
        start_x = -1.0 * ((size * columns) / 2.0);
        start_y += size;
    }

    grid->mesh = mesh_new_grid(CG_VERTICES_MODE_TRIANGLES,
                               (columns * rows) * 4,
                               (columns * rows) * 6,
                               vertices,
                               indices);
    pick_mesh_buffer = rut_buffer_new(sizeof(cg_vertex_p3_t) * 6);
    grid->pick_mesh = rut_mesh_new_from_buffer_p3(
        CG_VERTICES_MODE_TRIANGLES, 6, pick_mesh_buffer);
    pick_vertices = (cg_vertex_p3_t *)pick_mesh_buffer->data;

    half_tex_width = tex_width / 2.0f;
    half_tex_height = tex_height / 2.0f;

    pick_vertices[0].x = -half_tex_width;
    pick_vertices[0].y = -half_tex_height;
    pick_vertices[1].x = -half_tex_width;
    pick_vertices[1].y = half_tex_height;
    pick_vertices[2].x = half_tex_width;
    pick_vertices[2].y = half_tex_height;
    pick_vertices[3] = pick_vertices[0];
    pick_vertices[4] = pick_vertices[2];
    pick_vertices[5].x = half_tex_width;
    pick_vertices[5].y = -half_tex_height;
}

static void
free_meshes(rig_pointalism_grid_t *grid)
{
    if (grid->mesh) {
        rut_object_unref(grid->mesh);
        grid->mesh = NULL;

        rut_object_unref(grid->pick_mesh);
        grid->pick_mesh = NULL;
    }
}

static void
_rig_pointalism_grid_free(void *object)
{
    rig_pointalism_grid_t *grid = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    rut_closure_list_remove_all(&grid->updated_cb_list);

    free_meshes(grid);

    rut_introspectable_destroy(grid);

    rut_object_free(rig_pointalism_grid_t, grid);
}

static rut_object_t *
_rig_pointalism_grid_copy(rut_object_t *object)
{
    rig_pointalism_grid_t *grid = object;
    rig_engine_t *engine = rig_component_props_get_engine(&grid->component);
    rig_pointalism_grid_t *copy =
        rig_pointalism_grid_new(engine, grid->cell_size);
    rig_property_context_t *prop_ctx;

    rig_pointalism_grid_set_image_size(copy, grid->tex_width, grid->tex_height);

    prop_ctx = rig_component_props_get_property_context(&grid->component);
    rut_introspectable_copy_properties(prop_ctx, grid, copy);

    return copy;
}

rut_type_t rig_pointalism_grid_type;

void
_rig_pointalism_grid_init_type(void)
{

    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_pointalism_grid_copy
    };

    static rut_primable_vtable_t primable_vtable = {
        .get_primitive = rig_pointalism_grid_get_primitive
    };

    static rut_meshable_vtable_t meshable_vtable = {
        .get_mesh = rig_pointalism_grid_get_pick_mesh
    };

    static rut_image_size_dependant_vtable_t image_dependant_vtable = {
        .set_image_size = rig_pointalism_grid_set_image_size
    };

    rut_type_t *type = &rig_pointalism_grid_type;

#define TYPE rig_pointalism_grid_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_pointalism_grid_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPONENTABLE,
                       offsetof(TYPE, component),
                       &componentable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PRIMABLE,
                       0, /* no associated properties */
                       &primable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_MESHABLE,
                       0, /* no associated properties */
                       &meshable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_IMAGE_SIZE_DEPENDENT,
                       0, /* no implied properties */
                       &image_dependant_vtable);

#undef TYPE
}

rig_pointalism_grid_t *
rig_pointalism_grid_new(rig_engine_t *engine, float size)
{
    rig_pointalism_grid_t *grid =
        rut_object_alloc0(rig_pointalism_grid_t,
                          &rig_pointalism_grid_type,
                          _rig_pointalism_grid_init_type);

    grid->component.type = RUT_COMPONENT_TYPE_GEOMETRY;
    grid->component.parented = false;
    grid->component.engine = engine;

    c_list_init(&grid->updated_cb_list);

    grid->pointalism_scale = 1;
    grid->pointalism_z = 1;
    grid->pointalism_lighter = true;
    grid->cell_size = size;

    /* We just specify an arbitrary size initially and expect this to be
     * updated before we call create_meshes() */
    grid->tex_width = 640;
    grid->tex_height = 480;

    rut_introspectable_init(
        grid, _rig_pointalism_grid_prop_specs, grid->properties);

    return grid;
}

cg_primitive_t *
rig_pointalism_grid_get_primitive(rut_object_t *object)
{
    rig_pointalism_grid_t *grid = object;
    rut_shell_t *shell = rig_component_props_get_shell(&grid->component);

    if (!grid->mesh)
        create_meshes(grid);

    return rut_mesh_create_primitive(shell, grid->mesh);
}

rut_mesh_t *
rig_pointalism_grid_get_pick_mesh(rut_object_t *self)
{
    rig_pointalism_grid_t *grid = self;

    if (!grid->pick_mesh)
        create_meshes(grid);

    return grid->pick_mesh;
}

float
rig_pointalism_grid_get_scale(rut_object_t *obj)
{
    rig_pointalism_grid_t *grid = obj;

    return grid->pointalism_scale;
}

void
rig_pointalism_grid_set_scale(rut_object_t *obj, float scale)
{
    rig_pointalism_grid_t *grid = obj;
    rig_property_context_t *prop_ctx;

    if (scale == grid->pointalism_scale)
        return;

    grid->pointalism_scale = scale;

    prop_ctx = rig_component_props_get_property_context(&grid->component);
    rig_property_dirty(prop_ctx,
                       &grid->properties[RIG_POINTALISM_GRID_PROP_SCALE]);
}

float
rig_pointalism_grid_get_z(rut_object_t *obj)
{
    rig_pointalism_grid_t *grid = obj;

    return grid->pointalism_z;
}

void
rig_pointalism_grid_set_z(rut_object_t *obj, float z)
{
    rig_pointalism_grid_t *grid = obj;
    rig_property_context_t *prop_ctx;

    if (z == grid->pointalism_z)
        return;

    grid->pointalism_z = z;

    prop_ctx = rig_component_props_get_property_context(&grid->component);
    rig_property_dirty(prop_ctx,
                       &grid->properties[RIG_POINTALISM_GRID_PROP_Z]);
}

bool
rig_pointalism_grid_get_lighter(rut_object_t *obj)
{
    rig_pointalism_grid_t *grid = obj;

    return grid->pointalism_lighter;
}

void
rig_pointalism_grid_set_lighter(rut_object_t *obj, bool lighter)
{
    rig_pointalism_grid_t *grid = obj;
    rig_property_context_t *prop_ctx;

    if (lighter == grid->pointalism_lighter)
        return;

    grid->pointalism_lighter = lighter;

    prop_ctx = rig_component_props_get_property_context(&grid->component);
    rig_property_dirty(prop_ctx,
                       &grid->properties[RIG_POINTALISM_GRID_PROP_LIGHTER]);
}

float
rig_pointalism_grid_get_cell_size(rut_object_t *obj)
{
    rig_pointalism_grid_t *grid = obj;

    return grid->cell_size;
}

void
rig_pointalism_grid_set_cell_size(rut_object_t *obj, float cell_size)
{
    rig_pointalism_grid_t *grid = obj;
    rig_property_context_t *prop_ctx;

    if (cell_size == grid->cell_size)
        return;

    grid->cell_size = cell_size;

    free_meshes(grid);

    prop_ctx = rig_component_props_get_property_context(&grid->component);
    rig_property_dirty(prop_ctx,
                       &grid->properties[RIG_POINTALISM_GRID_PROP_CELL_SIZE]);

    rut_closure_list_invoke(
        &grid->updated_cb_list, rig_pointalism_grid_update_callback_t, grid);
}

void
rig_pointalism_grid_add_update_callback(rig_pointalism_grid_t *grid,
                                        rut_closure_t *closure)
{
    return rut_closure_list_add(&grid->updated_cb_list, closure);
}

void
rig_pointalism_grid_set_image_size(rut_object_t *self, int width, int height)
{
    rig_pointalism_grid_t *grid = self;

    if (grid->tex_width == width && grid->tex_height == height)
        return;

    free_meshes(grid);

    grid->tex_width = width;
    grid->tex_height = height;

    rut_closure_list_invoke(
        &grid->updated_cb_list, rig_pointalism_grid_update_callback_t, grid);
}
