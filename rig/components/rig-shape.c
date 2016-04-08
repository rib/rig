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

#include "rig-entity.h"
#include "rig-entity-inlines.h"
#include "rig-shape.h"
#include "rut-meshable.h"

#define MESA_CONST_ATTRIB_BUG_WORKAROUND

static rig_property_spec_t _rig_shape_prop_specs[] = {
    { .name = "shaped",
      .nick = "Shaped",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .data_offset = C_STRUCT_OFFSET(rig_shape_t, shaped),
      .setter.boolean_type = rig_shape_set_shaped,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "width",
      .nick = "Width",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rig_shape_t, width),
      .setter.float_type = rig_shape_set_width,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "height",
      .nick = "Height",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rig_shape_t, height),
      .setter.float_type = rig_shape_set_height,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "shape_mask",
      .nick = "Shape Mask",
      .type = RUT_PROPERTY_TYPE_ASSET,
      .validation = { .asset.type = RIG_ASSET_TYPE_ALPHA_MASK },
      .getter.asset_type = rig_shape_get_shape_mask,
      .setter.asset_type = rig_shape_set_shape_mask,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = false },
    { NULL }
};

static void
_rig_shape_model_free(void *object)
{
    rig_shape_model_t *shape_model = object;

    cg_object_unref(shape_model->shape_texture);
    rut_object_unref(shape_model->pick_mesh);
    rut_object_unref(shape_model->shape_mesh);

    rut_object_free(rig_shape_model_t, object);
}

rut_type_t rig_shape_model_type;

void
_rig_shape_model_init_type(void)
{

    rut_type_t *type = &rig_shape_model_type;

#define TYPE rig_shape_model_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_shape_model_free);

#undef TYPE
}

typedef struct _vertex_p2t2t2_t {
    float x, y, s0, t0, s1, t1;
} vertex_p2t2t2_t;

static rut_mesh_t *
mesh_new_p2t2t2(cg_vertices_mode_t mode,
                int n_vertices,
                const vertex_p2t2t2_t *data)
{
    rut_mesh_t *mesh;
    rut_attribute_t *attributes[8];
    rut_buffer_t *vertex_buffer;
    float normal[3] = { 0, 0, 1 };
    float tangent[3] = { 1, 0, 0 };

    vertex_buffer = rut_buffer_new(sizeof(vertex_p2t2t2_t) * n_vertices);
    memcpy(vertex_buffer->data, data, sizeof(vertex_p2t2t2_t) * n_vertices);

    attributes[0] = rut_attribute_new(vertex_buffer,
                                      "cg_position_in",
                                      sizeof(vertex_p2t2t2_t),
                                      offsetof(vertex_p2t2t2_t, x),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[1] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord0_in",
                                      sizeof(vertex_p2t2t2_t),
                                      offsetof(vertex_p2t2t2_t, s0),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[2] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord1_in",
                                      sizeof(vertex_p2t2t2_t),
                                      offsetof(vertex_p2t2t2_t, s1),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[3] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord4_in",
                                      sizeof(vertex_p2t2t2_t),
                                      offsetof(vertex_p2t2t2_t, s1),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[4] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord7_in",
                                      sizeof(vertex_p2t2t2_t),
                                      offsetof(vertex_p2t2t2_t, s1),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[5] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord11_in",
                                      sizeof(vertex_p2t2t2_t),
                                      offsetof(vertex_p2t2t2_t, s1),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[6] = rut_attribute_new_const("cg_normal_in",
                                            3, /* n components */
                                            1, /* n columns */
                                            false, /* no transpose */
                                            normal);

    attributes[7] = rut_attribute_new_const("tangent_in",
                                            3, /* n components */
                                            1, /* n columns */
                                            false, /* no transpose */
                                            tangent);

    mesh = rut_mesh_new(mode, n_vertices, attributes, 8);

    return mesh;
}

static rig_shape_model_t *
shape_model_new(rut_shell_t *shell, bool shaped, float width, float height)
{
    rig_shape_model_t *shape_model = rut_object_alloc0(
        rig_shape_model_t, &rig_shape_model_type, _rig_shape_model_init_type);
    rut_buffer_t *buffer = rut_buffer_new(sizeof(cg_vertex_p3_t) * 6);
    rut_mesh_t *pick_mesh =
        rut_mesh_new_from_buffer_p3(CG_VERTICES_MODE_TRIANGLES, 6, buffer);
    cg_vertex_p3_t *pick_vertices = (cg_vertex_p3_t *)buffer->data;
    c_matrix_t matrix;
    float tex_aspect;
    float size_x;
    float size_y;
    float half_size_x;
    float half_size_y;
    float geom_size_x;
    float geom_size_y;
    float half_geom_size_x;
    float half_geom_size_y;

    if (shaped) {
        /* In this case we are using a shape mask texture which is has a
         * square size and is padded with transparent pixels to provide
         * antialiasing. The shape mask is half the size of the texture
         * itself so we make the geometry twice as large to compensate.
         */
        size_x = MIN(width, height);
        size_y = size_x;
        geom_size_x = size_x * 2.0;
        geom_size_y = geom_size_x;
    } else {
        size_x = width;
        size_y = height;
        geom_size_x = width;
        geom_size_y = height;
    }

    half_size_x = size_x / 2.0;
    half_size_y = size_y / 2.0;
    half_geom_size_x = geom_size_x / 2.0;
    half_geom_size_y = geom_size_y / 2.0;

    {
        int n_vertices;
        int i;

        vertex_p2t2t2_t vertices[] = {
            { -half_geom_size_x, -half_geom_size_y, 0, 0, 0, 0 },
            { -half_geom_size_x, half_geom_size_y, 0, 1, 0, 1 },
            { half_geom_size_x, half_geom_size_y, 1, 1, 1, 1 },
            { -half_geom_size_x, -half_geom_size_y, 0, 0, 0, 0 },
            { half_geom_size_x, half_geom_size_y, 1, 1, 1, 1 },
            { half_geom_size_x, -half_geom_size_y, 1, 0, 1, 0 },
        };

        c_matrix_init_identity(&matrix);
        tex_aspect = (float)width / (float)height;

        if (shaped) {
            float s_scale, t_scale;
            float s0, t0;

            /* NB: The circle mask texture has a centered circle that is
             * half the width of the texture itself. We want the primary
             * texture to be mapped to this center circle. */

            s_scale = 2;
            t_scale = 2;

            if (tex_aspect < 1) /* taller than it is wide */
                t_scale *= tex_aspect;
            else /* wider than it is tall */
            {
                float inverse_aspect = 1.0f / tex_aspect;
                s_scale *= inverse_aspect;
            }

            s0 = 0.5 - (s_scale / 2.0);
            t0 = 0.5 - (t_scale / 2.0);

            c_matrix_translate(&matrix, s0, t0, 0);
            c_matrix_scale(&matrix, s_scale, t_scale, 1);
        }

        n_vertices = sizeof(vertices) / sizeof(vertex_p2t2t2_t);
        for (i = 0; i < n_vertices; i++) {
            float z = 0, w = 1;

            c_matrix_transform_point(
                &matrix, &vertices[i].s1, &vertices[i].t1, &z, &w);
        }

        shape_model->shape_mesh =
            mesh_new_p2t2t2(CG_VERTICES_MODE_TRIANGLES, n_vertices, vertices);
    }

    if (!shell->headless)
        shape_model->shape_texture = cg_object_ref(shell->circle_texture);

    pick_vertices[0].x = -half_size_x;
    pick_vertices[0].y = -half_size_y;
    pick_vertices[1].x = -half_size_x;
    pick_vertices[1].y = half_size_y;
    pick_vertices[2].x = half_size_x;
    pick_vertices[2].y = half_size_y;
    pick_vertices[3] = pick_vertices[0];
    pick_vertices[4] = pick_vertices[2];
    pick_vertices[5].x = half_size_x;
    pick_vertices[5].y = -half_size_y;

    shape_model->pick_mesh = pick_mesh;

    return shape_model;
}

static void
_rig_shape_free(void *object)
{
    rig_shape_t *shape = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    if (shape->model)
        rut_object_unref(shape->model);

    if (shape->shape_asset)
        rut_object_unref(shape->shape_asset);

    rut_introspectable_destroy(shape);

    rut_closure_list_remove_all(&shape->reshaped_cb_list);

    rut_object_free(rig_shape_t, shape);
}

static rut_object_t *
_rig_shape_copy(rut_object_t *object)
{
    rig_shape_t *shape = object;
    rig_engine_t *engine = rig_component_props_get_engine(&shape->component);
    rig_shape_t *copy =
        rig_shape_new(engine, shape->shaped, shape->width, shape->height);

    if (shape->model)
        copy->model = rut_object_ref(shape->model);

    return copy;
}

rut_type_t rig_shape_type;

void
_rig_shape_init_type(void)
{

    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_shape_copy
    };

    static rut_primable_vtable_t primable_vtable = {
        .get_primitive = rig_shape_get_primitive
    };

    static rut_meshable_vtable_t meshable_vtable = {
        .get_mesh = rig_shape_get_pick_mesh
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rig_shape_set_size,
        rig_shape_get_size,
        rut_simple_sizable_get_preferred_width,
        rut_simple_sizable_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    rut_type_t *type = &rig_shape_type;

#define TYPE rig_shape_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_shape_free);
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
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);

#undef TYPE
}

rig_shape_t *
rig_shape_new(rig_engine_t *engine, bool shaped, int width, int height)
{
    rig_shape_t *shape =
        rut_object_alloc0(rig_shape_t, &rig_shape_type, _rig_shape_init_type);

    shape->component.type = RUT_COMPONENT_TYPE_GEOMETRY;
    shape->component.parented = false;
    shape->component.engine = engine;

    shape->width = width;
    shape->height = height;
    shape->shaped = shaped;

    c_list_init(&shape->reshaped_cb_list);

    rut_introspectable_init(shape, _rig_shape_prop_specs, shape->properties);

    return shape;
}

static rig_shape_model_t *
rig_shape_get_model(rig_shape_t *shape)
{
    if (!shape->model) {
        rut_shell_t *shell = rig_component_props_get_shell(&shape->component);

        shape->model = shape_model_new(shell, shape->shaped,
                                       shape->width, shape->height);
    }

    return shape->model;
}

cg_primitive_t *
rig_shape_get_primitive(rut_object_t *object)
{
    rig_shape_t *shape = object;
    rig_shape_model_t *model = rig_shape_get_model(shape);
    rut_shell_t *shell = rig_component_props_get_shell(&shape->component);
    cg_primitive_t *primitive = rut_mesh_create_primitive(shell, model->shape_mesh);

    return primitive;
}

cg_texture_t *
rig_shape_get_shape_texture(rig_shape_t *shape)
{
    rig_shape_model_t *model = rig_shape_get_model(shape);

    return model->shape_texture;
}

rut_mesh_t *
rig_shape_get_pick_mesh(rut_object_t *self)
{
    rig_shape_t *shape = self;
    rig_shape_model_t *model = rig_shape_get_model(shape);
    return model->pick_mesh;
}

static void
free_model(rig_shape_t *shape)
{
    if (shape->model) {
        rut_object_unref(shape->model);
        shape->model = NULL;
    }
}

void
rig_shape_set_shaped(rut_object_t *obj, bool shaped)
{
    rig_shape_t *shape = obj;
    rig_property_context_t *prop_ctx;

    if (shape->shaped == shaped)
        return;

    shape->shaped = shaped;

    free_model(shape);

    rut_closure_list_invoke(
        &shape->reshaped_cb_list, rig_shape_re_shaped_callback_t, shape);

    prop_ctx = rig_component_props_get_property_context(&shape->component);
    rig_property_dirty(prop_ctx, &shape->properties[RIG_SHAPE_PROP_SHAPED]);
}

bool
rig_shape_get_shaped(rut_object_t *obj)
{
    rig_shape_t *shape = obj;

    return shape->shaped;
}

void
rig_shape_add_reshaped_callback(rig_shape_t *shape,
                                rut_closure_t *closure)
{
    c_return_if_fail(closure != NULL);

    return rut_closure_list_add(&shape->reshaped_cb_list, closure);
}

void
rig_shape_set_size(rut_object_t *self, float width, float height)
{
    rig_shape_t *shape = self;
    rig_property_context_t *prop_ctx;

    if (shape->width == width && shape->height == height)
        return;

    shape->width = width;
    shape->height = height;

    prop_ctx = rig_component_props_get_property_context(&shape->component);
    rig_property_dirty(prop_ctx, &shape->properties[RIG_SHAPE_PROP_WIDTH]);
    rig_property_dirty(prop_ctx, &shape->properties[RIG_SHAPE_PROP_HEIGHT]);

    free_model(shape);

    rut_closure_list_invoke(
        &shape->reshaped_cb_list, rig_shape_re_shaped_callback_t, shape);
}

void
rig_shape_get_size(rut_object_t *self, float *width, float *height)
{
    rig_shape_t *shape = self;
    *width = shape->width;
    *height = shape->height;
}

void
rig_shape_set_width(rut_object_t *obj, float width)
{
    rig_shape_t *shape = obj;
    rig_property_context_t *prop_ctx;

    if (shape->width == width)
        return;

    shape->width = width;
    free_model(shape);

    prop_ctx = rig_component_props_get_property_context(&shape->component);
    rig_property_dirty(prop_ctx, &shape->properties[RIG_SHAPE_PROP_WIDTH]);

    rut_closure_list_invoke(
        &shape->reshaped_cb_list, rig_shape_re_shaped_callback_t, shape);
}

void
rig_shape_set_height(rut_object_t *obj, float height)
{
    rig_shape_t *shape = obj;
    rig_property_context_t *prop_ctx;

    if (shape->height == height)
        return;

    shape->height = height;
    free_model(shape);

    prop_ctx = rig_component_props_get_property_context(&shape->component);
    rig_property_dirty(prop_ctx, &shape->properties[RIG_SHAPE_PROP_HEIGHT]);

    rut_closure_list_invoke(
        &shape->reshaped_cb_list, rig_shape_re_shaped_callback_t, shape);
}

void
rig_shape_set_shape_mask(rut_object_t *object, rig_asset_t *asset)
{
    rig_shape_t *shape = object;

    if (shape->shape_asset == asset)
        return;

    if (shape->shape_asset) {
        rut_object_unref(shape->shape_asset);
        shape->shape_asset = NULL;
    }

    shape->shape_asset = asset;

    if (asset)
        rut_object_ref(asset);

    if (shape->component.parented)
        rig_entity_notify_changed(shape->component.entity);
}

rig_asset_t *
rig_shape_get_shape_mask(rut_object_t *object)
{
    rig_shape_t *shape = object;

    return shape->shape_asset;
}
