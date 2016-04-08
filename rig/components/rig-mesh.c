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

#include <rut.h>

#include "rig-entity.h"
#include "rig-entity-inlines.h"
#include "rig-mesh.h"

static rut_ui_enum_t vertices_mode_enum = {
    .nick = "Mode",
    .values = {
        { .value = CG_VERTICES_MODE_POINTS, .nick = "Points" },
        { .value = CG_VERTICES_MODE_LINES, .nick = "Lines" },
        { .value = CG_VERTICES_MODE_LINE_LOOP, .nick = "Line Loop" },
        { .value = CG_VERTICES_MODE_LINE_STRIP, .nick = "Line Strip" },
        { .value = CG_VERTICES_MODE_TRIANGLES, .nick = "Triangles" },
        { .value = CG_VERTICES_MODE_TRIANGLE_STRIP, .nick = "Triangle Strip" },
        { .value = CG_VERTICES_MODE_TRIANGLE_FAN, .nick = "Triangle Fan" },
    }};

static rut_ui_enum_t indices_type_enum = {
    .nick = "Type",
    .values = {
        { .value = CG_INDICES_TYPE_UNSIGNED_BYTE, .nick = "UINT8" },
        { .value = CG_INDICES_TYPE_UNSIGNED_SHORT, .nick = "UINT16" },
        { .value = CG_INDICES_TYPE_UNSIGNED_INT, .nick = "UINT32" },
    }};

static rig_property_spec_t _rig_mesh_prop_specs[] = {
    { .name = "n_vertices",
      .nick = "Number of vertices",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rig_mesh_get_n_vertices,
      .setter.integer_type = rig_mesh_set_n_vertices,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .int_range = { 0, INT_MAX } },
      .animatable = true },
    { .name = "vertices_mode",
      .nick = "Vertices Topology Mode",
      .type = RUT_PROPERTY_TYPE_ENUM,
      .getter.integer_type = rig_mesh_get_vertices_mode,
      .setter.integer_type = rig_mesh_set_vertices_mode,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation =  { .ui_enum = &vertices_mode_enum },
      .animatable = false },
    { .name = "indices",
      .nick = "Indices Buffer",
      .type = RUT_PROPERTY_TYPE_OBJECT,
      .getter.object_type = rig_mesh_get_indices,
      .setter.object_type = rig_mesh_set_indices,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .object.type = &rut_buffer_type },
      .animatable = true },
    { .name = "indices_type",
      .nick = "Indices Data Type",
      .type = RUT_PROPERTY_TYPE_ENUM,
      .getter.integer_type = rig_mesh_get_indices_type,
      .setter.integer_type = rig_mesh_set_indices_type,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation =  { .ui_enum = &indices_type_enum },
      .animatable = false },
    { .name = "n_indices",
      .nick = "Number of indices",
      .type = RUT_PROPERTY_TYPE_INTEGER,
      .getter.integer_type = rig_mesh_get_n_indices,
      .setter.integer_type = rig_mesh_set_n_indices,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .int_range = { 0, INT_MAX } },
      .animatable = true },
    { .name = "min_x",
      .nick = "Min X Bound",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_mesh_t, min_x),
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, FLT_MAX } },
      .animatable = true },
    { .name = "max_x",
      .nick = "Max X Bound",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_mesh_t, max_x),
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, FLT_MAX } },
      .animatable = true },
    { .name = "min_y",
      .nick = "Min Y Bound",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_mesh_t, min_y),
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, FLT_MAX } },
      .animatable = true },
    { .name = "max_y",
      .nick = "Max Y Bound",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_mesh_t, max_y),
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, FLT_MAX } },
      .animatable = true },
    { .name = "min_z",
      .nick = "Min Z Bound",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_mesh_t, min_z),
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, FLT_MAX } },
      .animatable = true },
    { .name = "max_z",
      .nick = "Max Z Bound",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof(rig_mesh_t, max_z),
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_VALIDATE,
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .validation = { .float_range = { 0, FLT_MAX } },
      .animatable = true },
    { 0 }
};


static rig_mesh_t *_rig_mesh_new(rig_engine_t *engine);

static void
_rig_mesh_free(void *object)
{
    rig_mesh_t *mesh = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    if (mesh->primitive)
        cg_object_unref(mesh->primitive);

    if (mesh->rut_mesh)
        rut_object_unref(mesh->rut_mesh);

    rut_object_free(rig_mesh_t, mesh);
}

static rut_object_t *
_rig_mesh_copy(rut_object_t *object)
{
    rig_mesh_t *mesh = object;
    rig_engine_t *engine = rig_component_props_get_engine(&mesh->component);
    rig_mesh_t *copy = _rig_mesh_new(engine);

    copy->rut_mesh = rut_mesh_copy(mesh->rut_mesh);

    copy->min_x = mesh->min_x;
    copy->max_x = mesh->max_x;
    copy->min_y = mesh->min_y;
    copy->max_y = mesh->max_y;
    copy->min_z = mesh->min_z;
    copy->max_z = mesh->max_z;

    if (mesh->primitive)
        copy->primitive = cg_object_ref(mesh->primitive);

    return copy;
}

rut_type_t rig_mesh_type;

void
_rig_mesh_init_type(void)
{

    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_mesh_copy
    };

    static rut_primable_vtable_t primable_vtable = {
        .get_primitive = rig_mesh_get_primitive
    };

    static rut_meshable_vtable_t meshable_vtable = {
        .get_mesh = rig_mesh_get_rut_mesh
    };

    rut_type_t *type = &rig_mesh_type;
#define TYPE rig_mesh_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_mesh_free);
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
                       NULL); /* no implied vtable */

#undef TYPE
}

static rig_mesh_t *
_rig_mesh_new(rig_engine_t *engine)
{
    rig_mesh_t *mesh =
        rut_object_alloc0(rig_mesh_t, &rig_mesh_type, _rig_mesh_init_type);

    mesh->component.type = RUT_COMPONENT_TYPE_GEOMETRY;
    mesh->component.parented = false;
    mesh->component.engine = engine;

    rut_introspectable_init(mesh, _rig_mesh_prop_specs, mesh->properties);

    return mesh;
}

static bool
measure_mesh_x_cb(void **attribute_data, int vertex_index, void *user_data)
{
    rig_mesh_t *mesh = user_data;
    float *pos = attribute_data[0];

    if (pos[0] < mesh->min_x)
        mesh->min_x = pos[0];
    if (pos[0] > mesh->max_x)
        mesh->max_x = pos[0];

    return true;
}

static bool
measure_mesh_xy_cb(void **attribute_data, int vertex_index, void *user_data)
{
    rig_mesh_t *mesh = user_data;
    float *pos = attribute_data[0];

    measure_mesh_x_cb(attribute_data, vertex_index, user_data);

    if (pos[1] < mesh->min_y)
        mesh->min_y = pos[1];
    if (pos[1] > mesh->max_y)
        mesh->max_y = pos[1];

    return true;
}

static bool
measure_mesh_xyz_cb(void **attribute_data, int vertex_index, void *user_data)
{
    rig_mesh_t *mesh = user_data;
    float *pos = attribute_data[0];

    measure_mesh_xy_cb(attribute_data, vertex_index, user_data);

    if (pos[2] < mesh->min_z)
        mesh->min_z = pos[2];
    if (pos[2] > mesh->max_z)
        mesh->max_z = pos[2];

    return true;
}

void
rig_mesh_update_bounds(rig_mesh_t *mesh)
{
    rut_attribute_t *attribute =
        rut_mesh_find_attribute(mesh->rut_mesh, "cg_position_in");
    rut_mesh_vertex_callback_t measure_callback;
    rig_property_context_t *prop_ctx;

    c_return_if_fail(attribute->is_buffered);

    mesh->min_x = FLT_MAX;
    mesh->max_x = -FLT_MAX;
    mesh->min_y = FLT_MAX;
    mesh->max_y = -FLT_MAX;
    mesh->min_z = FLT_MAX;
    mesh->max_z = -FLT_MAX;

    switch (attribute->buffered.n_components) {
    case 1:
        mesh->min_y = mesh->max_y = 0;
        mesh->min_z = mesh->max_z = 0;
        measure_callback = measure_mesh_x_cb;
        break;
    case 2:
        mesh->min_z = mesh->max_z = 0;
        measure_callback = measure_mesh_xy_cb;
        break;
    case 3:
        measure_callback = measure_mesh_xyz_cb;
        break;
    default:
        c_warn_if_reached();
        measure_callback = measure_mesh_xyz_cb;
        break;
    }

    rut_mesh_foreach_vertex(mesh->rut_mesh,
                            measure_callback,
                            mesh,
                            "cg_position_in",
                            NULL);

    prop_ctx = rig_component_props_get_property_context(&mesh->component);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_X_MIN]);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_X_MAX]);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_Y_MIN]);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_Y_MAX]);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_Z_MIN]);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_Z_MAX]);
}

void
rig_mesh_set_attributes(rig_mesh_t *mesh,
                        rut_attribute_t **attributes,
                        int n_attributes)
{
    rut_mesh_set_attributes(mesh->rut_mesh,
                            attributes,
                            n_attributes);
}

rig_mesh_t *
rig_mesh_new(rig_engine_t *engine)
{
    rig_mesh_t *mesh;

    mesh = _rig_mesh_new(engine);
    mesh->rut_mesh = rut_mesh_new_empty();

    return mesh;
}

rig_mesh_t *
rig_mesh_new_with_rut_mesh(rig_engine_t *engine, rut_mesh_t *rut_mesh)
{
    rig_mesh_t *mesh;

    mesh = _rig_mesh_new(engine);
    mesh->rut_mesh = rut_object_ref(rut_mesh);

    return mesh;
}

/* TODO: move into rig-renderer.c since the specific attribute
 * requirements might conceptually vary between renderers */
cg_primitive_t *
rig_mesh_get_primitive(rut_object_t *object)
{
    rig_mesh_t *mesh = object;

    if (!mesh->primitive) {
        rut_mesh_t *rut_mesh = mesh->rut_mesh;

        if (rut_mesh) {
            rut_shell_t *shell =
                rig_component_props_get_shell(&mesh->component);
            enum {
                HAS_TEX_COORD1  = 1<<0,
                HAS_TEX_COORD4  = 1<<1,
                HAS_TEX_COORD7  = 1<<2,
                HAS_TEX_COORD11 = 1<<3,
                HAS_NORMALS     = 1<<4,
            } required_attribs = 0;
            const int max_extra = 4;
            int n_attributes = rut_mesh->n_attributes;
            rut_attribute_t **attributes = alloca((n_attributes + max_extra) * sizeof(void *));
            rut_attribute_t *tex_attrib = NULL;
            int i;

            /* When rendering we expect that every mesh has a specific
             * set of texture coordinate attributes that may be
             * required depending on the material state used in
             * conjunction with the mesh.
             *
             * We currently assume a mesh has at least one set of
             * texture coordinates (buffered) which are aliased for
             * any other texture coordinates that are missing.
             *
             * XXX: Note that in general we don't want to be doing
             * anything costly to make up for missing attributes at
             * this point, and should generally make it an editor
             * responsibility to ensure any mesh has all required
             * attributes for whatever renderer will be used ahead
             * of time.
             */

            for (i = 0; i < n_attributes; i++) {
                rut_attribute_t *attribute = rut_mesh->attributes[i];

                if (strcmp(attribute->name, "cg_tex_coord0_in") == 0)
                    tex_attrib = attribute;
                else if (strcmp(attribute->name, "cg_tex_coord1_in") == 0)
                    required_attribs |= HAS_TEX_COORD1;
                else if (strcmp(attribute->name, "cg_tex_coord4_in") == 0)
                    required_attribs |= HAS_TEX_COORD4;
                else if (strcmp(attribute->name, "cg_tex_coord7_in") == 0)
                    required_attribs |= HAS_TEX_COORD7;
                else if (strcmp(attribute->name, "cg_tex_coord11_in") == 0)
                    required_attribs |= HAS_TEX_COORD11;
                else if (strcmp(attribute->name, "cg_normal_in") == 0)
                    required_attribs |= HAS_NORMALS;

                attributes[i] = attribute;
            }

            c_return_val_if_fail(required_attribs & HAS_NORMALS, NULL);

            c_return_val_if_fail(tex_attrib != NULL, NULL);
            c_return_val_if_fail(tex_attrib->is_buffered, NULL);

            if (!(required_attribs & HAS_TEX_COORD1)) {
                attributes[i++] = rut_attribute_new(tex_attrib->buffered.buffer,
                                                    "cg_tex_coord1_in",
                                                    tex_attrib->buffered.stride,
                                                    tex_attrib->buffered.offset,
                                                    2,
                                                    RUT_ATTRIBUTE_TYPE_FLOAT);
            }

            if (!(required_attribs & HAS_TEX_COORD4)) {
                attributes[i++] = rut_attribute_new(tex_attrib->buffered.buffer,
                                                    "cg_tex_coord4_in",
                                                    tex_attrib->buffered.stride,
                                                    tex_attrib->buffered.offset,
                                                    2,
                                                    RUT_ATTRIBUTE_TYPE_FLOAT);
            }

            if (!(required_attribs & HAS_TEX_COORD7)) {
                attributes[i++] = rut_attribute_new(tex_attrib->buffered.buffer,
                                                    "cg_tex_coord7_in",
                                                    tex_attrib->buffered.stride,
                                                    tex_attrib->buffered.offset,
                                                    2,
                                                    RUT_ATTRIBUTE_TYPE_FLOAT);
            }

            if (!(required_attribs & HAS_TEX_COORD11)) {
                attributes[i++] = rut_attribute_new(tex_attrib->buffered.buffer,
                                                    "cg_tex_coord11_in",
                                                    tex_attrib->buffered.stride,
                                                    tex_attrib->buffered.offset,
                                                    2,
                                                    RUT_ATTRIBUTE_TYPE_FLOAT);
            }

            /* NB: Don't just add extra required attributes without
             * updating the max_extra constant above... */
            c_assert(i <= n_attributes + max_extra);

            rut_mesh_set_attributes(rut_mesh, attributes, i);

            mesh->primitive = rut_mesh_create_primitive(shell, rut_mesh);
        }
    }

    return mesh->primitive;
}

int
rig_mesh_get_n_vertices(rut_object_t *obj)
{
    rig_mesh_t *mesh = obj;
    return mesh->rut_mesh->n_vertices;
}

void
rig_mesh_set_n_vertices(rut_object_t *obj, int n_vertices)
{
    rig_mesh_t *mesh = obj;
    rig_property_context_t *prop_ctx;

    if (mesh->rut_mesh->n_vertices == n_vertices)
        return;

    mesh->rut_mesh->n_vertices = n_vertices;

    prop_ctx = rig_component_props_get_property_context(&mesh->component);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_N_VERTICES]);
}

int
rig_mesh_get_vertices_mode(rut_object_t *obj)
{
    rig_mesh_t *mesh = obj;
    return mesh->rut_mesh->mode;
}

void
rig_mesh_set_vertices_mode(rut_object_t *obj, int mode)
{
    rig_mesh_t *mesh = obj;
    rig_property_context_t *prop_ctx;

    if (mesh->rut_mesh->mode == mode)
        return;

    mesh->rut_mesh->mode = mode;

    prop_ctx = rig_component_props_get_property_context(&mesh->component);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_VERTICES_MODE]);
}

rut_object_t *
rig_mesh_get_indices(rut_object_t *obj)
{
    rig_mesh_t *mesh = obj;
    return mesh->rut_mesh->indices_buffer;
}

void
rig_mesh_set_indices(rut_object_t *obj, rut_object_t *buffer)
{
    rig_mesh_t *mesh = obj;
    rig_property_context_t *prop_ctx;

    if (mesh->rut_mesh->indices_buffer == buffer)
        return;

    mesh->rut_mesh->indices_buffer = buffer;

    prop_ctx = rig_component_props_get_property_context(&mesh->component);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_INDICES]);
}

int
rig_mesh_get_indices_type(rut_object_t *obj)
{
    rig_mesh_t *mesh = obj;
    return mesh->rut_mesh->indices_type;
}

void
rig_mesh_set_indices_type(rut_object_t *obj, int indices_type)
{
    rig_mesh_t *mesh = obj;
    rig_property_context_t *prop_ctx;

    if (mesh->rut_mesh->indices_type == indices_type)
        return;

    mesh->rut_mesh->indices_type = indices_type;

    prop_ctx = rig_component_props_get_property_context(&mesh->component);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_INDICES_TYPE]);
}

int
rig_mesh_get_n_indices(rut_object_t *obj)
{
    rig_mesh_t *mesh = obj;
    return mesh->rut_mesh->n_indices;
}

void
rig_mesh_set_n_indices(rut_object_t *obj, int n_indices)
{
    rig_mesh_t *mesh = obj;
    rig_property_context_t *prop_ctx;

    if (mesh->rut_mesh->n_indices == n_indices)
        return;

    mesh->rut_mesh->n_indices = n_indices;

    prop_ctx = rig_component_props_get_property_context(&mesh->component);
    rig_property_dirty(prop_ctx, &mesh->properties[RIG_MESH_PROP_N_INDICES]);
}

rut_mesh_t *
rig_mesh_get_rut_mesh(rut_object_t *obj)
{
    rig_mesh_t *mesh = obj;

    return mesh->rut_mesh;
}
