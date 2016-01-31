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
#include "rig-diamond.h"
#include "rut-meshable.h"

static rig_diamond_t *_rig_diamond_new_with_slice(rig_engine_t *engine,
                                                  float size,
                                                  int tex_width,
                                                  int tex_height,
                                                  rig_diamond_slice_t *slice);

static rut_property_spec_t _rig_diamond_prop_specs[] = {
    { .name = "size",
      .nick = "Size",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rig_diamond_t, size),
      .setter.float_type = rig_diamond_set_size,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { NULL }
};

static void
_rig_diamond_slice_free(void *object)
{
    rig_diamond_slice_t *diamond_slice = object;

    rut_object_unref(diamond_slice->mesh);
    rut_object_unref(diamond_slice->pick_mesh);

    rut_object_free(rig_diamond_slice_t, object);
}

static rut_type_t rig_diamond_slice_type;

void
_rig_diamond_slice_init_type(void)
{

    rut_type_t *type = &rig_diamond_slice_type;
#define TYPE rig_diamond_slice_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_diamond_slice_free);

#undef TYPE
}

typedef struct _vertex_p2t2t2_t {
    float x, y, s0, t0, s1, t1;
    float Nx, Ny, Nz;
    float Tx, Ty, Tz;
} vertex_p2t2t2_t;

static rut_mesh_t *
mesh_new_p2t2t2(cg_vertices_mode_t mode,
                int n_vertices,
                vertex_p2t2t2_t *vertices)
{
    rut_mesh_t *mesh;
    rut_attribute_t *attributes[8];
    rut_buffer_t *vertex_buffer;
    rut_buffer_t *index_buffer;

    vertex_buffer = rut_buffer_new(sizeof(vertex_p2t2t2_t) * n_vertices);
    memcpy(vertex_buffer->data, vertices, sizeof(vertex_p2t2t2_t) * n_vertices);
    index_buffer = rut_buffer_new(sizeof(_rut_nine_slice_indices_data));
    memcpy(index_buffer->data,
           _rut_nine_slice_indices_data,
           sizeof(_rut_nine_slice_indices_data));

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

    attributes[6] = rut_attribute_new(vertex_buffer,
                                      "cg_normal_in",
                                      sizeof(vertex_p2t2t2_t),
                                      offsetof(vertex_p2t2t2_t, Nx),
                                      3,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[7] = rut_attribute_new(vertex_buffer,
                                      "tangent_in",
                                      sizeof(vertex_p2t2t2_t),
                                      offsetof(vertex_p2t2t2_t, Tx),
                                      3,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    mesh = rut_mesh_new(mode, n_vertices, attributes, 8);
    rut_mesh_set_indices(mesh,
                         CG_INDICES_TYPE_UNSIGNED_BYTE,
                         index_buffer,
                         sizeof(_rut_nine_slice_indices_data) /
                         sizeof(_rut_nine_slice_indices_data[0]));

    return mesh;
}

static rig_diamond_slice_t *
diamond_slice_new(float size, int tex_width, int tex_height)
{
    rig_diamond_slice_t *diamond_slice =
        rut_object_alloc0(rig_diamond_slice_t,
                          &rig_diamond_slice_type,
                          _rig_diamond_slice_init_type);
    float width = size;
    float height = size;
#define DIAMOND_SLICE_CORNER_RADIUS 20
    c_matrix_t matrix;
    float tex_aspect;
    rut_buffer_t *pick_mesh_buffer;
    cg_vertex_p3_t *pick_vertices;

    diamond_slice->size = size;

    {
        /* x0,y0,x1,y1 and s0,t0,s1,t1 define the postion and texture
         * coordinates for the center rectangle... */
        float x0 = DIAMOND_SLICE_CORNER_RADIUS;
        float y0 = DIAMOND_SLICE_CORNER_RADIUS;
        float x1 = width - DIAMOND_SLICE_CORNER_RADIUS;
        float y1 = height - DIAMOND_SLICE_CORNER_RADIUS;

        /* The center region of the nine-slice can simply map to the
         * degenerate center of the circle */
        float s0 = 0.5;
        float t0 = 0.5;
        float s1 = 0.5;
        float t1 = 0.5;

        int n_vertices;
        int i;

        /*
         * 0,0      x0,0      x1,0      width,0
         * 0,0      s0,0      s1,0      1,0
         * 0        1         2         3
         *
         * 0,y0     x0,y0     x1,y0     width,y0
         * 0,t0     s0,t0     s1,t0     1,t0
         * 4        5         6         7
         *
         * 0,y1     x0,y1     x1,y1     width,y1
         * 0,t1     s0,t1     s1,t1     1,t1
         * 8        9         10        11
         *
         * 0,height x0,height x1,height width,height
         * 0,1      s0,1      s1,1      1,1
         * 12       13        14        15
         */

        vertex_p2t2t2_t vertices[] = {
            { 0, 0, 0, 0, 0, 0 },
            { x0, 0, s0, 0, x0, 0 },
            { x1, 0, s1, 0, x1, 0 },
            { width, 0, 1, 0, width, 0 },
            { 0, y0, 0, t0, 0, y0 },
            { x0, y0, s0, t0, x0, y0 },
            { x1, y0, s1, t0, x1, y0 },
            { width, y0, 1, t0, width, y0 },
            { 0, y1, 0, t1, 0, y1 },
            { x0, y1, s0, t1, x0, y1 },
            { x1, y1, s1, t1, x1, y1 },
            { width, y1, 1, t1, width, y1 },
            { 0, height, 0, 1, 0, height },
            { x0, height, s0, 1, x0, height },
            { x1, height, s1, 1, x1, height },
            { width, height, 1, 1, width, height },
        };

        c_matrix_init_identity(&diamond_slice->rotate_matrix);
        c_matrix_rotate(&diamond_slice->rotate_matrix, 45, 0, 0, 1);
        c_matrix_translate(
            &diamond_slice->rotate_matrix, -width / 2.0, -height / 2.0, 0);

        n_vertices = sizeof(vertices) / sizeof(vertex_p2t2t2_t);
        for (i = 0; i < n_vertices; i++) {
            float z = 0, w = 1;

            c_matrix_transform_point(&diamond_slice->rotate_matrix,
                                      &vertices[i].x,
                                      &vertices[i].y,
                                      &z,
                                      &w);

            vertices[i].Nx = 0;
            vertices[i].Ny = 0;
            vertices[i].Nz = 1;

            vertices[i].Tx = 1;
            vertices[i].Ty = 0;
            vertices[i].Tz = 0;
        }

        c_matrix_init_identity(&matrix);

        {
            float s_scale = 1.0, t_scale = 1.0;
            float s0, t0;
            float diagonal_size_scale = 1.0 / (sinf(C_PI_4) * 2.0);

            tex_aspect = (float)tex_width / (float)tex_height;

            if (tex_aspect < 1) /* taller than it is wide */
                t_scale *= tex_aspect;
            else /* wider than it is tall */
            {
                float inverse_aspect = 1.0f / tex_aspect;
                s_scale *= inverse_aspect;
            }

            s_scale *= diagonal_size_scale;
            t_scale *= diagonal_size_scale;

            s0 = 0.5 - (s_scale / 2.0);
            t0 = 0.5 - (t_scale / 2.0);

            c_matrix_translate(&matrix, s0, t0, 0);
            c_matrix_scale(&matrix, s_scale / width, t_scale / height, 1);

            c_matrix_translate(&matrix, width / 2.0, height / 2.0, 1);
            c_matrix_rotate(&matrix, 45, 0, 0, 1);
            c_matrix_translate(&matrix, -width / 2.0, -height / 2.0, 1);
        }

        n_vertices = sizeof(vertices) / sizeof(vertex_p2t2t2_t);
        for (i = 0; i < n_vertices; i++) {
            float z = 0, w = 1;

            c_matrix_transform_point(
                &matrix, &vertices[i].s1, &vertices[i].t1, &z, &w);
        }

        diamond_slice->mesh =
            mesh_new_p2t2t2(CG_VERTICES_MODE_TRIANGLES, n_vertices, vertices);
    }

    pick_mesh_buffer = rut_buffer_new(sizeof(cg_vertex_p3_t) * 6);
    diamond_slice->pick_mesh = rut_mesh_new_from_buffer_p3(
        CG_VERTICES_MODE_TRIANGLES, 6, pick_mesh_buffer);
    pick_vertices = (cg_vertex_p3_t *)pick_mesh_buffer->data;

    pick_vertices[0].x = 0;
    pick_vertices[0].y = 0;
    pick_vertices[1].x = 0;
    pick_vertices[1].y = size;
    pick_vertices[2].x = size;
    pick_vertices[2].y = size;
    pick_vertices[3] = pick_vertices[0];
    pick_vertices[4] = pick_vertices[2];
    pick_vertices[5].x = size;
    pick_vertices[5].y = 0;

    c_matrix_transform_points(&diamond_slice->rotate_matrix,
                               2,
                               sizeof(cg_vertex_p3_t),
                               pick_vertices,
                               sizeof(cg_vertex_p3_t),
                               pick_vertices,
                               6);

    return diamond_slice;
}

static void
_rig_diamond_free(void *object)
{
    rig_diamond_t *diamond = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    rut_closure_list_remove_all(&diamond->updated_cb_list);

    if (diamond->slice)
        rut_object_unref(diamond->slice);

    rut_introspectable_destroy(diamond);

    rut_object_free(rig_diamond_t, diamond);
}

static rut_object_t *
_rig_diamond_copy(rut_object_t *object)
{
    rig_diamond_t *diamond = object;
    rig_engine_t *engine = rig_component_props_get_engine(&diamond->component);

    return _rig_diamond_new_with_slice(engine,
                                       diamond->size,
                                       diamond->tex_width,
                                       diamond->tex_height,
                                       diamond->slice);
}

rut_type_t rig_diamond_type;

void
_rig_diamond_init_type(void)
{

    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_diamond_copy
    };

    static rut_primable_vtable_t primable_vtable = {
        .get_primitive = rig_diamond_get_primitive
    };

    static rut_meshable_vtable_t meshable_vtable = {
        .get_mesh = rig_diamond_get_pick_mesh
    };

    static rut_image_size_dependant_vtable_t image_dependant_vtable = {
        .set_image_size = rig_diamond_set_image_size
    };

    rut_type_t *type = &rig_diamond_type;
#define TYPE rig_diamond_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_diamond_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPONENTABLE,
                       offsetof(TYPE, component),
                       &componentable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PRIMABLE,
                       0, /* no associated properties */
                       &primable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_MESHABLE,
                       0, /* no associated properties */
                       &meshable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_IMAGE_SIZE_DEPENDENT,
                       0, /* no implied properties */
                       &image_dependant_vtable);

#undef TYPE
}

static rig_diamond_t *
_rig_diamond_new_with_slice(rig_engine_t *engine,
                            float size,
                            int tex_width,
                            int tex_height,
                            rig_diamond_slice_t *slice)
{
    rig_diamond_t *diamond = rut_object_alloc0(
        rig_diamond_t, &rig_diamond_type, _rig_diamond_init_type);

    c_list_init(&diamond->updated_cb_list);

    diamond->component.type = RUT_COMPONENT_TYPE_GEOMETRY;
    diamond->component.parented = false;
    diamond->component.engine = engine;

    diamond->size = size;
    diamond->tex_width = tex_width;
    diamond->tex_height = tex_height;

    if (slice)
        diamond->slice = rut_object_ref(slice);

    rut_introspectable_init(
        diamond, _rig_diamond_prop_specs, diamond->properties);

    return diamond;
}

rig_diamond_t *
rig_diamond_new(rig_engine_t *engine, float size)
{
    /* Initially we just specify an arbitrary texture width/height
     * which should be updated by the time we create the
     * diamond_slice geometry */
    rig_diamond_t *diamond =
        _rig_diamond_new_with_slice(engine, size, 640, 480, NULL);

    return diamond;
}

float
rig_diamond_get_size(rig_diamond_t *diamond)
{
    return diamond->size;
}

void
rig_diamond_set_size(rut_object_t *object, float size)
{
    rig_diamond_t *diamond = object;
    rut_property_context_t *prop_ctx;

    if (diamond->size == size)
        return;

    if (diamond->slice) {
        rut_object_unref(diamond->slice);
        diamond->slice = NULL;
    }

    diamond->size = size;

    prop_ctx = rig_component_props_get_property_context(&diamond->component);
    rut_property_dirty(prop_ctx, &diamond->properties[RIG_DIAMOND_PROP_SIZE]);

    rut_closure_list_invoke(
        &diamond->updated_cb_list, rig_diamond_update_callback_t, diamond);
}

cg_primitive_t *
rig_diamond_get_primitive(rut_object_t *object)
{
    rig_diamond_t *diamond = object;
    rut_shell_t *shell;

    /* XXX: It could be worth maintaining a cache of diamond slices
     * indexed by the <size, tex_width, tex_height> tuple... */
    if (!diamond->slice)
        diamond->slice = diamond_slice_new(
            diamond->size, diamond->tex_width, diamond->tex_height);

    shell = rig_component_props_get_shell(&diamond->component);
    return rut_mesh_create_primitive(shell, diamond->slice->mesh);
}

void
rig_diamond_apply_mask(rig_diamond_t *diamond, cg_pipeline_t *pipeline)
{
    rut_shell_t *shell = rig_component_props_get_shell(&diamond->component);

    cg_pipeline_set_layer_texture(pipeline, 0, shell->circle_texture);
}

rut_mesh_t *
rig_diamond_get_pick_mesh(rut_object_t *self)
{
    rig_diamond_t *diamond = self;

    /* XXX: It could be worth maintaining a cache of diamond slices
     * indexed by the <size, tex_width, tex_height> tuple... */
    if (!diamond->slice)
        diamond->slice = diamond_slice_new(
            diamond->size, diamond->tex_width, diamond->tex_height);

    return diamond->slice->pick_mesh;
}

void
rig_diamond_add_update_callback(rig_diamond_t *diamond,
                                rut_closure_t *closure)
{
    return rut_closure_list_add(&diamond->updated_cb_list, closure);
}

void
rig_diamond_set_image_size(rut_object_t *self, int width, int height)
{
    rig_diamond_t *diamond = self;

    if (diamond->tex_width == width && diamond->tex_height == height)
        return;

    if (diamond->slice) {
        rut_object_unref(diamond->slice);
        diamond->slice = NULL;
    }

    diamond->tex_width = width;
    diamond->tex_height = height;

    rut_closure_list_invoke(
        &diamond->updated_cb_list, rig_diamond_update_callback_t, diamond);
}
