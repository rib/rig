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
#include "rig-model.h"
#include "rig-asset.h"

#define PI 3.14159265359

/* The vertex structure */

typedef struct _vertex_t {
    float pos[3];
    float normal[3];
    float tx, ty, tz;
    float s0, t0; /* source image coordinates */
    float s1, t1; /* patch/fin texture coordinates */
} vertex_t;

/* The polygon structure */

typedef struct _polygon_t {
    int id;
    vertex_t *vertices[3];
    vertex_t flat_vertices[3];
    float tangent[3];
    float normal[3];
    bool uncovered;
} polygon_t;

/* Texture patch structure */

typedef struct _texture_patch_t {
    c_llist_t *polygons;
    polygon_t *root;
    float tangent_angle;
    float width;
    float height;
} texture_patch_t;

struct _rig_model_private_t {
    c_llist_t *texture_patches;
    polygon_t *fin_polygons;
    vertex_t *fin_vertices;
    polygon_t *polygons;
    vertex_t *vertices;
    RutBitmask adj_matrix;
    int n_polygons;
    int n_vertices;
    int n_fin_polygons;
    int n_fin_vertices;
};

/* Some convinient constants */
static float flat_normal[3] = { 0, 0, 1 };

static rig_model_t *_rig_model_new(rig_engine_t *engine);

cg_primitive_t *
rig_model_get_primitive(rut_object_t *object)
{
    rig_model_t *model = object;

    if (!model->primitive) {
        if (model->mesh) {
            rut_shell_t *shell =
                rig_component_props_get_shell(&model->component);

            model->primitive =
                rut_mesh_create_primitive(shell, model->mesh);
        }
    }

    return model->primitive;
}

cg_primitive_t *
rig_model_get_fin_primitive(rut_object_t *object)
{
    rig_model_t *model = object;

    return model->fin_primitive;
}

static void
_rig_model_free(void *object)
{
    rig_model_t *model = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    if (model->primitive)
        cg_object_unref(model->primitive);

    if (model->mesh)
        rut_object_unref(model->mesh);

    if (model->patched_mesh) {
        c_free(model->priv->polygons);
        c_free(model->priv->vertices);
        rut_object_unref(model->patched_mesh);
    }

    if (model->fin_mesh) {
        cg_object_unref(model->fin_primitive);
        c_free(model->priv->fin_polygons);
        c_free(model->priv->fin_vertices);
        rut_object_unref(model->fin_mesh);
    }

    c_free(model->priv);
    rut_object_free(rig_model_t, model);
}

static rut_object_t *
_rig_model_copy(rut_object_t *object)
{
    rig_model_t *model = object;
    rig_engine_t *engine = rig_component_props_get_engine(&model->component);
    rig_model_t *copy = _rig_model_new(engine);

    copy->type = model->type;
    copy->mesh = rut_object_ref(model->mesh);

    if (model->asset)
        copy->asset = rut_object_ref(model->asset);

    copy->min_x = model->min_x;
    copy->max_x = model->max_x;
    copy->min_y = model->min_y;
    copy->max_y = model->max_y;
    copy->min_z = model->min_z;
    copy->max_z = model->max_z;

    copy->builtin_normals = model->builtin_normals;
    copy->builtin_tex_coords = model->builtin_tex_coords;

    if (model->primitive)
        copy->primitive = cg_object_ref(model->primitive);

    if (model->is_hair_model) {
        copy->is_hair_model = model->is_hair_model;
        copy->patched_mesh = rut_object_ref(model->patched_mesh);
        copy->fin_mesh = rut_object_ref(model->fin_mesh);
        if (copy->fin_primitive)
            copy->fin_primitive = cg_object_ref(model->fin_primitive);
        copy->default_hair_length = model->default_hair_length;
    }

    return copy;
}

rut_type_t rig_model_type;

void
_rig_model_init_type(void)
{

    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_model_copy
    };

    static rut_primable_vtable_t primable_vtable = {
        .get_primitive = rig_model_get_primitive
    };

    static rut_meshable_vtable_t meshable_vtable = { .get_mesh =
                                                         rig_model_get_mesh };

    rut_type_t *type = &rig_model_type;
#define TYPE rig_model_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_model_free);
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

#undef TYPE
}

static rig_model_t *
_rig_model_new(rig_engine_t *engine)
{
    rig_model_t *model =
        rut_object_alloc0(rig_model_t, &rig_model_type, _rig_model_init_type);

    model->component.type = RUT_COMPONENT_TYPE_GEOMETRY;
    model->component.parented = false;
    model->component.engine = engine;

    return model;
}

/* Some vertex utility functions */

static void
substract_vertices(float *result, vertex_t *v1, vertex_t *v2)
{
    result[0] = v1->pos[0] - v2->pos[0];
    result[1] = v1->pos[1] - v2->pos[1];
    result[2] = v1->pos[2] - v2->pos[2];
}

static float
calculate_dot_product(float *v1, float *v2)
{
    return (v1[0] * v2[0]) + (v1[1] * v2[1]) + (v1[2] * v2[2]);
}

static void
calculate_cross_product(float *result, float *v1, float *v2)
{
    result[0] = (v1[1] * v2[2]) - (v2[1] * v1[2]);
    result[1] = (v1[2] * v2[0]) - (v2[2] * v1[0]);
    result[2] = (v1[0] * v2[1]) - (v2[0] * v1[1]);
}

static void
calculate_triangle_centroid(float *result,
                            vertex_t *v1,
                            vertex_t *v2,
                            vertex_t *v3)
{
    result[0] = (v1->pos[0] + v2->pos[0] + v3->pos[0]) / 3.f;
    result[1] = (v1->pos[1] + v2->pos[1] + v3->pos[1]) / 3.f;
    result[2] = (v1->pos[2] + v2->pos[2] + v3->pos[2]) / 3.f;
}

static float
calculate_magnitude(float x, float y, float z)
{
    return sqrt(x * x + y * y + z * z);
}

static void
rotate_vertex_around_custom_axis(float *vertex, float *axis, float angle)
{
    float w[3], u[3];
    float v[3];
    float div =
        calculate_dot_product(vertex, axis) / calculate_dot_product(axis, axis);
    float length = calculate_magnitude(axis[0], axis[1], axis[2]);
    float cosine = cosf(angle);
    float sine = sinf(angle);

    w[0] = axis[0] * div;
    w[1] = axis[1] * div;
    w[2] = axis[2] * div;

    u[0] = vertex[0] - w[0];
    u[1] = vertex[1] - w[1];
    u[2] = vertex[2] - w[2];

    calculate_cross_product(v, axis, u);

    v[0] /= length;
    v[1] /= length;
    v[2] /= length;

    vertex[0] = w[0] + u[0] * cosine + v[0] * sine;
    vertex[1] = w[1] + u[1] * cosine + v[1] * sine;
    vertex[2] = w[2] + u[2] * cosine + v[2] * sine;
}

static void
normalize_vertex(float *vertex)
{
    float magnitude = calculate_magnitude(vertex[0], vertex[1], vertex[2]);

    vertex[0] = vertex[0] / magnitude;
    vertex[1] = vertex[1] / magnitude;
    vertex[2] = vertex[2] / magnitude;
}

static bool
check_vertex_equality(vertex_t *v1, vertex_t *v2)
{
    return v1->pos[0] == v2->pos[0] && v1->pos[1] == v2->pos[1] &&
           v1->pos[2] == v2->pos[2];
}

/* Calculate polygon tangent */

static void
calculate_poly_tangent(polygon_t *poly)
{
    float edge1[3];
    float edge2[3];
    float tex_edge1[2];
    float tex_edge2[2];
    float coef;

    substract_vertices(edge1, poly->vertices[1], poly->vertices[0]);
    substract_vertices(edge2, poly->vertices[2], poly->vertices[0]);

    tex_edge1[0] = poly->vertices[1]->s0 - poly->vertices[0]->s0;
    tex_edge1[1] = poly->vertices[1]->t0 - poly->vertices[0]->t0;

    tex_edge2[0] = poly->vertices[2]->s0 - poly->vertices[0]->s0;
    tex_edge2[1] = poly->vertices[2]->t0 - poly->vertices[0]->t0;

    coef = 1 / (tex_edge1[0] * tex_edge2[1] - tex_edge2[0] * tex_edge1[1]);

    poly->tangent[0] =
        coef * ((edge1[0] * tex_edge2[1]) + (edge2[0] * (-1 * tex_edge1[1])));
    poly->tangent[1] =
        coef * ((edge1[1] * tex_edge2[1]) + (edge2[1] * (-1 * tex_edge1[1])));
    poly->tangent[2] =
        coef * ((edge1[2] * tex_edge2[1]) + (edge2[2] * (-1 * tex_edge1[1])));

    normalize_vertex(poly->tangent);
}

static void
calculate_vertex_tangent(polygon_t *poly, vertex_t *vertex)
{
    float tangent[3];

    tangent[0] = poly->tangent[0] + vertex->tx;
    tangent[1] = poly->tangent[1] + vertex->ty;
    tangent[2] = poly->tangent[2] + vertex->tz;

    normalize_vertex(tangent);

    vertex->tx = tangent[0];
    vertex->ty = tangent[1];
    vertex->tz = tangent[2];
}

/* Calculate polygon normal */

static void
calculate_poly_normal(polygon_t *poly)
{
    float edge1[3], edge2[3];

    substract_vertices(edge1, poly->vertices[1], poly->vertices[0]);
    substract_vertices(edge2, poly->vertices[2], poly->vertices[0]);

    calculate_cross_product(poly->normal, edge1, edge2);
    normalize_vertex(poly->normal);
}

/* Generate cylindrical uv coordinates for a single vertex */

static void
calculate_cylindrical_uv_coordinates(rig_model_t *model,
                                     float *position,
                                     float *tex)
{
    float center_x = model->min_x + ((model->max_x - model->min_x) / 2.0);
    float center_z = model->min_z + ((model->max_z - model->min_z) / 2.0);

    float dx = position[0] - center_x;
    float dz = position[2] - center_z;

    float angle = atan2(dx, dz);

    angle += PI;
    angle /= (2.0 * PI);
    tex[0] = angle;

    tex[1] = (position[1] - model->min_y) / (model->max_y - model->min_y);
}

static void
generate_cylindrical_uv_coordinates(vertex_t *vertex,
                                    rig_model_t *model)
{
    float pos[3] = { vertex->pos[0], vertex->pos[1], vertex->pos[2] };
    float tex[2];

    calculate_cylindrical_uv_coordinates(model, pos, tex);
    vertex->s0 = tex[0];
    vertex->t0 = tex[1];
}

static void
add_polygon_fins(rig_model_t *model, polygon_t *polygon)
{
    vertex_t *fin_verts[12];
    polygon_t *fin_polys[6];
    int poly_iter = model->priv->n_fin_polygons;
    int vert_iter = model->priv->n_fin_vertices;
    int edges[3][2];
    int i, j;

    edges[0][0] = 0;
    edges[0][1] = 1;
    edges[1][0] = 1;
    edges[1][1] = 2;
    edges[2][0] = 2;
    edges[2][1] = 0;

    j = 0;

    for (i = 0; i < 3; i++) {
        int cv = i * 4;
        fin_verts[cv] = &model->priv->fin_vertices[vert_iter + cv];
        fin_verts[cv + 1] = &model->priv->fin_vertices[vert_iter + cv + 1];
        fin_verts[cv + 2] = &model->priv->fin_vertices[vert_iter + cv + 2];
        fin_verts[cv + 3] = &model->priv->fin_vertices[vert_iter + cv + 3];
        fin_polys[j] = &model->priv->fin_polygons[poly_iter + j];
        fin_polys[j + 1] = &model->priv->fin_polygons[poly_iter + j + 1];

        fin_polys[j]->vertices[0] = fin_verts[cv];
        fin_polys[j]->vertices[1] = fin_verts[cv + 1];
        fin_polys[j]->vertices[2] = fin_verts[cv + 2];
        fin_polys[j + 1]->vertices[0] = fin_verts[cv + 2];
        fin_polys[j + 1]->vertices[1] = fin_verts[cv + 3];
        fin_polys[j + 1]->vertices[2] = fin_verts[cv];

        fin_verts[cv]->pos[0] = fin_verts[cv + 1]->pos[0] =
                                    polygon->vertices[edges[i][0]]->pos[0];
        fin_verts[cv]->pos[1] = fin_verts[cv + 1]->pos[1] =
                                    polygon->vertices[edges[i][0]]->pos[1];
        fin_verts[cv]->pos[2] = fin_verts[cv + 1]->pos[2] =
                                    polygon->vertices[edges[i][0]]->pos[2];

        fin_verts[cv]->normal[0] = fin_verts[cv + 1]->normal[0] =
                                       polygon->vertices[edges[i][0]]->normal[0];
        fin_verts[cv]->normal[1] = fin_verts[cv + 1]->normal[1] =
                                       polygon->vertices[edges[i][0]]->normal[1];
        fin_verts[cv]->normal[2] = fin_verts[cv + 1]->normal[2] =
                                       polygon->vertices[edges[i][0]]->normal[2];

        fin_verts[cv]->tx = fin_verts[cv + 1]->tx =
                                polygon->vertices[edges[i][0]]->tx;
        fin_verts[cv]->ty = fin_verts[cv + 1]->ty =
                                polygon->vertices[edges[i][0]]->ty;
        fin_verts[cv]->tz = fin_verts[cv + 1]->tz =
                                polygon->vertices[edges[i][0]]->tz;

        fin_verts[cv + 2]->pos[0] = fin_verts[cv + 3]->pos[0] =
                                        polygon->vertices[edges[i][1]]->pos[0];
        fin_verts[cv + 2]->pos[1] = fin_verts[cv + 3]->pos[1] =
                                        polygon->vertices[edges[i][1]]->pos[1];
        fin_verts[cv + 2]->pos[2] = fin_verts[cv + 3]->pos[2] =
                                        polygon->vertices[edges[i][1]]->pos[2];

        fin_verts[cv + 2]->normal[0] = fin_verts[cv + 3]->normal[0] =
                                           polygon->vertices[edges[i][1]]->normal[0];
        fin_verts[cv + 2]->normal[1] = fin_verts[cv + 3]->normal[1] =
                                           polygon->vertices[edges[i][1]]->normal[1];
        fin_verts[cv + 2]->normal[2] = fin_verts[cv + 3]->normal[2] =
                                           polygon->vertices[edges[i][1]]->normal[2];

        fin_verts[cv + 2]->tx = fin_verts[cv + 3]->tx =
                                    polygon->vertices[edges[i][1]]->tx;
        fin_verts[cv + 2]->ty = fin_verts[cv + 3]->ty =
                                    polygon->vertices[edges[i][1]]->ty;
        fin_verts[cv + 2]->tz = fin_verts[cv + 3]->tz =
                                    polygon->vertices[edges[i][1]]->tz;

        fin_verts[cv]->s0 = fin_verts[cv + 1]->s0 =
                                polygon->vertices[edges[i][0]]->s0;

        fin_verts[cv]->t0 = fin_verts[cv + 1]->t0 =
                                polygon->vertices[edges[i][0]]->t0;

        fin_verts[cv + 2]->s0 = fin_verts[cv + 3]->s0 =
                                    polygon->vertices[edges[i][1]]->s0;

        fin_verts[cv + 2]->t0 = fin_verts[cv + 3]->t0 =
                                    polygon->vertices[edges[i][1]]->t0;

        fin_verts[cv]->s1 = fin_verts[cv + 1]->s1 = 0;
        fin_verts[cv + 1]->s1 = fin_verts[cv + 3]->s1 = 1;

        fin_verts[cv + 1]->t1 = fin_verts[cv + 2]->t1 = 0;
        fin_verts[cv]->t1 = fin_verts[cv + 3]->t1 = 1;

        j += 2;
    }

    model->priv->n_fin_polygons += 6;
    model->priv->n_fin_vertices += 12;
}

static void
calculate_tangents(float *position0,
                   float *position1,
                   float *position2,
                   float *tex0,
                   float *tex1,
                   float *tex2,
                   float *tangent0,
                   float *tangent1,
                   float *tangent2)
{
    float edge1[3];
    float edge2[3];
    float tex_edge1[3];
    float tex_edge2[3];
    float coef;
    float poly_tangent[3];

    edge1[0] = position1[0] - position0[0];
    edge1[1] = position1[1] - position0[1];
    edge1[2] = position1[2] - position0[2];

    edge2[0] = position2[0] - position0[0];
    edge2[1] = position2[1] - position0[1];
    edge2[2] = position2[2] - position0[2];

    tex_edge1[0] = tex1[0] - tex0[0];
    tex_edge1[1] = tex1[1] - tex0[1];

    tex_edge2[0] = tex2[0] - tex0[0];
    tex_edge2[1] = tex2[1] - tex0[1];

    coef = 1 / (tex_edge1[0] * tex_edge2[1] - tex_edge2[0] * tex_edge1[1]);

    poly_tangent[0] =
        coef * ((edge1[0] * tex_edge2[1]) - (edge2[0] * tex_edge1[1]));
    poly_tangent[1] =
        coef * ((edge1[1] * tex_edge2[1]) - (edge2[1] * tex_edge1[1]));
    poly_tangent[2] =
        coef * ((edge1[2] * tex_edge2[1]) - (edge2[2] * tex_edge1[1]));

    normalize_vertex(poly_tangent);

    tangent0[0] = poly_tangent[0];
    tangent0[1] = poly_tangent[1];
    tangent0[2] = poly_tangent[2];

    tangent1[0] = poly_tangent[0];
    tangent1[1] = poly_tangent[1];
    tangent1[2] = poly_tangent[2];

    tangent2[0] = poly_tangent[0];
    tangent2[1] = poly_tangent[1];
    tangent2[2] = poly_tangent[2];
}

static void
calculate_normals(float *position0,
                  float *position1,
                  float *position2,
                  float *normal0,
                  float *normal1,
                  float *normal2)
{
    float edge1[3];
    float edge2[3];
    float poly_normal[3];

    edge1[0] = position1[0] - position0[0];
    edge1[1] = position1[1] - position0[1];
    edge1[2] = position1[2] - position0[2];

    edge2[0] = position2[0] - position0[0];
    edge2[1] = position2[1] - position0[1];
    edge2[2] = position2[2] - position0[2];

    poly_normal[0] = (edge1[1] * edge2[2]) - (edge1[2] * edge2[1]);
    poly_normal[1] = (edge1[2] * edge2[0]) - (edge1[0] * edge2[2]);
    poly_normal[2] = (edge1[0] * edge2[1]) - (edge1[1] * edge2[0]);

    normalize_vertex(poly_normal);

    normal0[0] = poly_normal[0];
    normal0[1] = poly_normal[1];
    normal0[2] = poly_normal[2];

    normal1[0] = poly_normal[0];
    normal1[1] = poly_normal[1];
    normal1[2] = poly_normal[2];

    normal2[0] = poly_normal[0];
    normal2[1] = poly_normal[1];
    normal2[2] = poly_normal[2];
}

/* Automatically generate all required properties not included in the
 * PLY file */

static bool
generate_missing_properties(void **attribute_data_v0,
                            void **attribute_data_v1,
                            void **attribute_data_v2,
                            int v0_index,
                            int v1_index,
                            int v2_index,
                            void *user_data)
{
    float *vert_p0 = attribute_data_v0[0];
    float *vert_p1 = attribute_data_v1[0];
    float *vert_p2 = attribute_data_v2[0];

    float *vert_n0 = attribute_data_v0[1];
    float *vert_n1 = attribute_data_v1[1];
    float *vert_n2 = attribute_data_v2[1];

    float *vert_t0 = attribute_data_v0[2];
    float *vert_t1 = attribute_data_v1[2];
    float *vert_t2 = attribute_data_v2[2];

    float *tex_coord0 = attribute_data_v0[3];
    float *tex_coord1 = attribute_data_v1[3];
    float *tex_coord2 = attribute_data_v2[3];

    rig_model_t *model = user_data;

    if (!model->builtin_tex_coords) {
        calculate_cylindrical_uv_coordinates(model, vert_p0, tex_coord0);
        calculate_cylindrical_uv_coordinates(model, vert_p1, tex_coord1);
        calculate_cylindrical_uv_coordinates(model, vert_p2, tex_coord2);
    }

    if (!model->builtin_normals)
        calculate_normals(vert_p0, vert_p1, vert_p2, vert_n0, vert_n1, vert_n2);

    calculate_tangents(vert_p0,
                       vert_p1,
                       vert_p2,
                       tex_coord0,
                       tex_coord1,
                       tex_coord2,
                       vert_t0,
                       vert_t1,
                       vert_t2);

    return true;
}

static bool
generate_polygons_for_patching(void **attribute_data_v0,
                               void **attribute_data_v1,
                               void **attribute_data_v2,
                               int v0_index,
                               int v1_index,
                               int v2_index,
                               void *user_data)
{
    float *positions[3] = { attribute_data_v0[0], attribute_data_v1[0],
                            attribute_data_v2[0] };
    float *normals[3] = { attribute_data_v0[1], attribute_data_v1[1],
                          attribute_data_v2[1] };
    float *tangents[3] = { attribute_data_v0[2], attribute_data_v1[2],
                           attribute_data_v2[2] };
    float *tex_coords0[3] = { attribute_data_v0[3], attribute_data_v1[3],
                              attribute_data_v2[3] };
    int i;
    rig_model_t *model = user_data;
    polygon_t *polygon = &model->priv->polygons[model->priv->n_polygons];

    polygon->id = model->priv->n_polygons;
    polygon->uncovered = true;

    for (i = 0; i < 3; i++) {
        polygon->vertices[i] =
            &model->priv->vertices[model->priv->n_vertices + i];
        polygon->vertices[i]->pos[0] = positions[i][0];
        polygon->vertices[i]->pos[1] = positions[i][1];
        polygon->vertices[i]->pos[2] = positions[i][2];

        polygon->vertices[i]->normal[0] = normals[i][0];
        polygon->vertices[i]->normal[1] = normals[i][1];
        polygon->vertices[i]->normal[2] = normals[i][2];

        if (model->builtin_tex_coords) {
            polygon->vertices[i]->s0 = tex_coords0[i][0];
            polygon->vertices[i]->t0 = tex_coords0[i][1];
        } else {
            generate_cylindrical_uv_coordinates(polygon->vertices[i], model);
            tex_coords0[i][0] = polygon->vertices[i]->s0;
            tex_coords0[i][1] = polygon->vertices[i]->t0;
        }
    }

    if (!model->builtin_normals)
        calculate_poly_normal(polygon);
    else {
        polygon->normal[0] =
            (polygon->vertices[0]->normal[0] + polygon->vertices[1]->normal[0] +
             polygon->vertices[2]->normal[0]) /
            3.0;
        polygon->normal[1] =
            (polygon->vertices[0]->normal[1] + polygon->vertices[1]->normal[1] +
             polygon->vertices[2]->normal[1]) /
            3.0;
        polygon->normal[2] =
            (polygon->vertices[0]->normal[2] + polygon->vertices[1]->normal[2] +
             polygon->vertices[2]->normal[2]) /
            3.0;
        normalize_vertex(polygon->normal);
    }

    calculate_poly_tangent(polygon);

    for (i = 0; i < 3; i++) {
        if (!model->builtin_normals) {
            normals[i][0] = polygon->normal[0];
            normals[i][1] = polygon->normal[1];
            normals[i][2] = polygon->normal[2];
        }

        calculate_vertex_tangent(polygon, polygon->vertices[i]);
        tangents[i][0] = polygon->vertices[i]->tx;
        tangents[i][1] = polygon->vertices[i]->ty;
        tangents[i][2] = polygon->vertices[i]->tz;
    }

    model->priv->n_polygons++;
    model->priv->n_vertices += 3;

    return true;
}

static bool
copy_tangents_to_polygons(void **attribute_data_v0,
                          void **attribute_data_v1,
                          void **attribute_data_v2,
                          int v0_index,
                          int v1_index,
                          int v2_index,
                          void *user_data)
{
    rig_model_t *model = user_data;
    polygon_t *polygon = &model->priv->polygons[model->priv->n_polygons];
    float *normals[3] = { attribute_data_v0[0], attribute_data_v1[0],
                          attribute_data_v2[0] };
    float *tangents[3] = { attribute_data_v0[1], attribute_data_v1[1],
                           attribute_data_v2[1] };
    int i;

    for (i = 0; i < 3; i++) {
        polygon->vertices[i]->tx = tangents[i][0];
        polygon->vertices[i]->ty = tangents[i][1];
        polygon->vertices[i]->tz = tangents[i][2];
        if (!model->builtin_normals) {
            polygon->vertices[i]->normal[0] = normals[i][0];
            polygon->vertices[i]->normal[1] = normals[i][1];
            polygon->vertices[i]->normal[2] = normals[i][2];
        }
    }

    model->priv->n_polygons++;

    return true;
}

static bool
measure_mesh_x_cb(void **attribute_data, int vertex_index, void *user_data)
{
    rig_model_t *model = user_data;
    float *pos = attribute_data[0];

    if (pos[0] < model->min_x)
        model->min_x = pos[0];
    if (pos[0] > model->max_x)
        model->max_x = pos[0];

    return true;
}

static bool
measure_mesh_xy_cb(void **attribute_data, int vertex_index, void *user_data)
{
    rig_model_t *model = user_data;
    float *pos = attribute_data[0];

    measure_mesh_x_cb(attribute_data, vertex_index, user_data);

    if (pos[1] < model->min_y)
        model->min_y = pos[1];
    if (pos[1] > model->max_y)
        model->max_y = pos[1];

    return true;
}

static bool
measure_mesh_xyz_cb(void **attribute_data, int vertex_index, void *user_data)
{
    rig_model_t *model = user_data;
    float *pos = attribute_data[0];

    measure_mesh_xy_cb(attribute_data, vertex_index, user_data);

    if (pos[2] < model->min_z)
        model->min_z = pos[2];
    if (pos[2] > model->max_z)
        model->max_z = pos[2];

    return true;
}

/* Gets the angle between 2 vectors relative to a rotation axis (usually their
 * cross product). This function takes the "direction" of the angle / rotation
 * into consideration and adjusts the angle accordingly, avoiding clockwise
 * rotation. */

static float
get_angle_between_vectors(float *start_vector, float *end_vector, float *axis)
{
    float cosine;
    float angle;
    float rotated_vertex[3];

    cosine = calculate_dot_product(start_vector, end_vector);
    angle = acos(cosine);

    if (angle == 0.f)
        return 0.f;

    rotated_vertex[0] = start_vector[0];
    rotated_vertex[1] = start_vector[1];
    rotated_vertex[2] = start_vector[2];

    rotate_vertex_around_custom_axis(rotated_vertex, axis, angle);

    if (calculate_dot_product(rotated_vertex, end_vector) > 0.9998)
        return angle;

    /* Otherwise this is a clockwise rotation and the angle needs to be adjusted
     */

    angle = PI * 2.f - angle;

    return angle;
}

/* Determine whether 2 polygons are connected i.e. whether they share an edge */

static int
check_for_shared_vertices(polygon_t *poly1, polygon_t *poly2)
{
    vertex_t *edges1[3][2];
    vertex_t *edges2[3][2];
    int i, j;

    edges1[0][0] = poly1->vertices[0];
    edges1[0][1] = poly1->vertices[1];

    edges1[1][0] = poly1->vertices[1];
    edges1[1][1] = poly1->vertices[2];

    edges1[2][0] = poly1->vertices[2];
    edges1[2][1] = poly1->vertices[0];

    edges2[0][0] = poly2->vertices[0];
    edges2[0][1] = poly2->vertices[1];

    edges2[1][0] = poly2->vertices[1];
    edges2[1][1] = poly2->vertices[2];

    edges2[2][0] = poly2->vertices[2];
    edges2[2][1] = poly2->vertices[0];

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (check_vertex_equality(edges1[i][0], edges2[j][0]) &&
                check_vertex_equality(edges1[i][1], edges2[j][1]))
                return 1;
            else if (check_vertex_equality(edges1[i][0], edges2[j][1]) &&
                     check_vertex_equality(edges1[i][1], edges2[j][0]))
                return 1;
        }
    }

    return 0;
}

/* Generate the adjacency matrix */

static void
generate_adjacency_matrix(rig_model_t *model)
{
    int i, j;
    rig_model_private_t *priv = model->priv;

    _rut_bitmask_clear_all(&priv->adj_matrix);

    for (i = 0; i < priv->n_polygons; i++) {
        polygon_t *origin = &priv->polygons[i];
        unsigned int adj_offset = priv->n_polygons * origin->id;

        for (j = 0; j < priv->n_polygons; j++) {
            polygon_t *child = &priv->polygons[j];

            if (origin->id == child->id)
                _rut_bitmask_set(&priv->adj_matrix, adj_offset + child->id, 0);
            else
                _rut_bitmask_set(&priv->adj_matrix,
                                 adj_offset + child->id,
                                 check_for_shared_vertices(origin, child));
        }
    }
}

/* Finds a polygon which hasn't been covered by a patch yet */

static polygon_t *
find_uncovered_polygon(rig_model_t *model)
{
    int i;

    for (i = 0; i < model->priv->n_polygons; i++) {
        polygon_t *polygon = &model->priv->polygons[i];
        if (polygon->uncovered)
            return polygon;
    }

    return NULL;
}

/* Places the polygon at the origin of a metaphorical 2D plane */

static void
position_polygon_at_2D_origin(polygon_t *polygon)
{
    float centroid[3], axis[3];
    float angle;
    int i;

    /* 1. Calculate polygon centroid */

    calculate_triangle_centroid(centroid,
                                polygon->vertices[0],
                                polygon->vertices[1],
                                polygon->vertices[2]);

    /* 2. Move centroid to origin */

    for (i = 0; i < 3; i++) {
        polygon->flat_vertices[i].pos[0] =
            polygon->vertices[i]->pos[0] - centroid[0];
        polygon->flat_vertices[i].pos[1] =
            polygon->vertices[i]->pos[1] - centroid[1];
        polygon->flat_vertices[i].pos[2] =
            polygon->vertices[i]->pos[2] - centroid[2];
    }

    /* 3. Find the angle between the polgyon normal and the 2D plane normal */

    calculate_cross_product(axis, flat_normal, polygon->normal);
    angle = get_angle_between_vectors(polygon->normal, flat_normal, axis);

    if (angle == 0.f)
        return;

    /* 4. Rotate the polygon so that its normal parallelly aligns with the
       2D plane one */

    for (i = 0; i < 3; i++)
        rotate_vertex_around_custom_axis(
            polygon->flat_vertices[i].pos, axis, angle);
}

/* Map the new polygon by "extruding" the new vertex it indroduces */

static void
extrude_new_vertex(polygon_t *parent, polygon_t *child)
{
    float distance[3];
    bool found_first_set = false;
    int shared_vertices[2][2];
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (check_vertex_equality(parent->vertices[i],
                                      child->vertices[j])) {
                if (!found_first_set) {
                    shared_vertices[0][0] = i;
                    shared_vertices[0][1] = j;
                    found_first_set = true;
                } else {
                    shared_vertices[1][0] = i;
                    shared_vertices[1][1] = j;
                    goto FOUND_SHARED_VERTICES;
                }
            }
        }
    }

FOUND_SHARED_VERTICES:

    substract_vertices(distance,
                       &parent->flat_vertices[shared_vertices[0][0]],
                       &child->flat_vertices[shared_vertices[0][1]]);

    for (i = 0; i < 3; i++) {
        child->flat_vertices[i].pos[0] += distance[0];
        child->flat_vertices[i].pos[1] += distance[1];
        child->flat_vertices[i].pos[2] += distance[2];
    }
}

static bool
extract_texture_coordinates(texture_patch_t *patch,
                            polygon_t *polygon)
{
    int i;
    float x_min = -1 * (patch->width / 2.0);
    float y_min = -1 * (patch->height / 2.0);
    float x_max = patch->width / 2.0;
    float y_max = patch->height / 2.0;
    float new_s[3];
    float new_t[3];

    /* 7. Extract the texture coordinates from the flattened polygon using
     * linear interpolation */

    for (i = 0; i < 3; i++) {
        new_s[i] = (polygon->flat_vertices[i].pos[0] - x_min) / (x_max - x_min);
        new_t[i] = (polygon->flat_vertices[i].pos[1] - y_min) / (y_max - x_min);
        if (new_s[i] > 1.0 || new_t[i] > 1.0 || new_s[i] < 0.0 ||
            new_t[i] < 0.0)
            return false;
    }

    for (i = 0; i < 3; i++) {
        polygon->vertices[i]->s1 = new_s[i];
        polygon->vertices[i]->t1 = new_t[i];
    }

    return true;
}

static void
grow_texture_patch(rig_model_t *model, texture_patch_t *patch)
{
    rig_model_private_t *priv = model->priv;
    bool *visited = c_new(bool, priv->n_polygons);
    rut_queue_t *stack = rut_queue_new();
    int i;

    for (i = 0; i < priv->n_polygons; i++)
        visited[i] = false;

    rut_queue_push_tail(stack, patch->root);

    while (!rut_queue_is_empty(stack)) {
        polygon_t *parent = rut_queue_pop_tail(stack);

        if (visited[parent->id])
            continue;

        visited[parent->id] = true;

        for (i = 0; i < priv->n_polygons; i++) {
            polygon_t *child = &priv->polygons[i];
            unsigned int adj_offset = priv->n_polygons * parent->id;

            if (_rut_bitmask_get(&priv->adj_matrix, adj_offset + child->id) !=
                1)
                continue;

            if (!visited[child->id]) {
                position_polygon_at_2D_origin(child);
                extrude_new_vertex(parent, child);
                if (extract_texture_coordinates(patch, child)) {
                    patch->polygons = c_llist_prepend(patch->polygons, child);
                    rut_queue_push_tail(stack, child);
                    child->uncovered = false;
                }
            }
        }
    }

    c_free(visited);
    rut_queue_free(stack);
}

texture_patch_t *
create_texture_patch(rig_model_t *model)
{
    texture_patch_t *patch = NULL;
    polygon_t *root = NULL;

    root = find_uncovered_polygon(model);

    if (!root)
        return NULL;

    patch = c_new(texture_patch_t, 1);
    patch->polygons = NULL;
    patch->root = root;

/* One problem with using a constant fraction for the patch sizing
 * is that if we have low resolution geometry then its possible that
 * many of the polygons are themselves larger than the patch size
 * and so we fail to extract any texture coordinates.
 */
#warning                                                                       \
    "TODO: Make the patch sizing adaptive if it fails extract texture coordinates"
    patch->width = fabsf(model->max_x - model->min_x) / 5.0;
    patch->height = fabsf(model->max_y - model->min_y) / 5.0;

    position_polygon_at_2D_origin(root);

    extract_texture_coordinates(patch, root);
    patch->root->uncovered = false;

    patch->polygons = c_llist_prepend(patch->polygons, root);
    grow_texture_patch(model, patch);

    model->priv->texture_patches =
        c_llist_prepend(model->priv->texture_patches, patch);

    return patch;
}

/* FIXME: the specific details about what attributes are required is
 * really tightly coupled with the renderer that will be used in
 * the end and so conceptually this code should really live in Rig
 * somehow.
 */
static rut_mesh_t *
create_renderer_mesh_from_vertices(vertex_t *vertices,
                                   int n_vertices,
                                   unsigned int *indices,
                                   int n_indices)
{
    rut_mesh_t *mesh;
    rut_attribute_t *attributes[8];
    rut_buffer_t *vertex_buffer;
    rut_buffer_t *index_buffer;

    vertex_buffer = rut_buffer_new(sizeof(vertex_t) * n_vertices);
    memcpy(vertex_buffer->data, vertices, sizeof(vertex_t) * n_vertices);

    if (indices) {
        index_buffer = rut_buffer_new(sizeof(unsigned int) * n_indices);
        memcpy(index_buffer->data, indices, sizeof(unsigned int) * n_indices);
    }

    attributes[0] = rut_attribute_new(vertex_buffer,
                                      "cg_position_in",
                                      sizeof(vertex_t),
                                      offsetof(vertex_t, pos),
                                      3,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[1] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord0_in",
                                      sizeof(vertex_t),
                                      offsetof(vertex_t, s0),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

#warning "TODO: audit why we have all of these texture coordinate attributes..."
    attributes[2] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord1_in",
                                      sizeof(vertex_t),
                                      offsetof(vertex_t, s0),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[3] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord4_in",
                                      sizeof(vertex_t),
                                      offsetof(vertex_t, s0),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[4] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord7_in",
                                      sizeof(vertex_t),
                                      offsetof(vertex_t, s0),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[5] = rut_attribute_new(vertex_buffer,
                                      "cg_tex_coord11_in",
                                      sizeof(vertex_t),
                                      offsetof(vertex_t, s1),
                                      2,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[6] = rut_attribute_new(vertex_buffer,
                                      "cg_normal_in",
                                      sizeof(vertex_t),
                                      offsetof(vertex_t, normal),
                                      3,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[7] = rut_attribute_new(vertex_buffer,
                                      "tangent_in",
                                      sizeof(vertex_t),
                                      offsetof(vertex_t, tx),
                                      3,
                                      RUT_ATTRIBUTE_TYPE_FLOAT);

    mesh = rut_mesh_new(CG_VERTICES_MODE_TRIANGLES, n_vertices, attributes, 8);

    if (indices) {
        rut_mesh_set_indices(
            mesh, CG_INDICES_TYPE_UNSIGNED_INT, index_buffer, n_indices);
    }

    return mesh;
}

static rut_mesh_t *
create_patched_mesh_from_model(rut_object_t *object)
{
    rig_model_t *model = object;
    c_llist_t *iter;

    if (model->patched_mesh)
        return model->patched_mesh;

    generate_adjacency_matrix(model);

    while (create_texture_patch(model))
        ;

    for (iter = model->priv->texture_patches; iter; iter = iter->next)
        c_free(iter->data);

    c_llist_free(model->priv->texture_patches);

    model->patched_mesh =
        create_renderer_mesh_from_vertices(model->priv->vertices,
                                           model->priv->n_vertices,
                                           NULL, /* no indices */
                                           0);

    return model->patched_mesh;
}

rut_mesh_t *
create_fin_mesh_from_model(rut_object_t *object)
{
    rig_model_t *model = object;
    unsigned int *indices;
    int i, j, k;

    if (model->fin_mesh)
        return model->fin_mesh;

    indices = c_new(unsigned int, model->priv->n_fin_polygons * 3);

    j = 0;
    k = 0;

    for (i = 0; i < model->priv->n_fin_polygons; i += 2) {
        indices[k] = j;
        indices[k + 1] = j + 1;
        indices[k + 2] = j + 2;
        indices[k + 3] = j + 2;
        indices[k + 4] = j + 3;
        indices[k + 5] = j;

        j += 4;
        k += 6;
    }

    model->fin_mesh =
        create_renderer_mesh_from_vertices(model->priv->fin_vertices,
                                           model->priv->n_fin_vertices,
                                           indices,
                                           model->priv->n_fin_polygons * 3);

    c_free(indices);

    return model->fin_mesh;
}

rig_model_t *
rig_model_new_from_asset_mesh(rig_engine_t *engine,
                              rut_mesh_t *mesh,
                              bool needs_normals,
                              bool needs_tex_coords)
{
    rig_model_t *model;
    rut_attribute_t *attribute;
    rut_mesh_tvertex_callback_t measure_callback;
    rut_attribute_t **attributes;
    int i;
    rut_attribute_t *tex_attrib;

    model = _rig_model_new(engine);
    model->type = RIG_MODEL_TYPE_FILE;
    model->mesh = rut_mesh_copy(mesh);

/* XXX: needs_normals/needs_tex_coords currently only determine
 * whether we should initialize these attributes, not actually add
 * them if they are completely missing.
 *
 * FIXME: we should handle the case where the attributes are
 * completely missing.
 */
#ifdef RIG_ENABLE_DEBUG
    c_return_val_if_fail(rut_mesh_find_attribute(model->mesh, "cg_normal_in"),
                         NULL);

    c_return_val_if_fail(
        rut_mesh_find_attribute(model->mesh, "cg_tex_coord0_in"), NULL);
#endif

    attribute = rut_mesh_find_attribute(model->mesh, "cg_position_in");
    c_return_val_if_fail(attribute->is_buffered, NULL);

    model->min_x = FLT_MAX;
    model->max_x = -FLT_MAX;
    model->min_y = FLT_MAX;
    model->max_y = -FLT_MAX;
    model->min_z = FLT_MAX;
    model->max_z = -FLT_MAX;

    model->builtin_normals = !needs_normals;
    model->builtin_tex_coords = !needs_tex_coords;

    if (attribute->buffered.n_components == 1) {
        model->min_y = model->max_y = 0;
        model->min_z = model->max_z = 0;
        measure_callback = measure_mesh_x_cb;
    } else if (attribute->buffered.n_components == 2) {
        model->min_z = model->max_z = 0;
        measure_callback = measure_mesh_xy_cb;
    } else if (attribute->buffered.n_components == 3)
        measure_callback = measure_mesh_xyz_cb;
    else {
        c_warn_if_reached();
        measure_callback = measure_mesh_xyz_cb;
    }

    rut_mesh_foreach_vertex(model->mesh,
                            measure_callback,
                            model,
                            "cg_position_in",
                            "cg_normal_in",
                            "tangent_in",
                            NULL);

    rut_mesh_foreach_triangle(model->mesh,
                              generate_missing_properties,
                              model,
                              "cg_position_in",
                              "cg_normal_in",
                              "tangent_in",
                              "cg_tex_coord0_in",
                              NULL);

    /* When rendering we expect that every model has a specific set of
     * texture coordinate attributes that may be required depending
     * on the material state used in conjunction with the model.
     *
     * We currently assume a newly loaded asset mesh will only have one
     * set of texture coodinates and so all the remaining sets of
     * texture coordinates will simply be an alias of those...
     */
    attributes = c_alloca(sizeof(rut_attribute_t *) * mesh->n_attributes + 4);

    tex_attrib = NULL;
    for (i = 0; i < mesh->n_attributes; i++) {
        rut_attribute_t *attribute = model->mesh->attributes[i];
        if (strcmp(attribute->name, "cg_tex_coord0_in") == 0)
            tex_attrib = attribute;

        attributes[i] = model->mesh->attributes[i];
    }

    c_return_val_if_fail(tex_attrib != NULL, NULL);
    c_return_val_if_fail(tex_attrib->is_buffered, NULL);

    attributes[i++] = rut_attribute_new(tex_attrib->buffered.buffer,
                                        "cg_tex_coord1_in",
                                        tex_attrib->buffered.stride,
                                        tex_attrib->buffered.offset,
                                        2,
                                        RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[i++] = rut_attribute_new(tex_attrib->buffered.buffer,
                                        "cg_tex_coord4_in",
                                        tex_attrib->buffered.stride,
                                        tex_attrib->buffered.offset,
                                        2,
                                        RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[i++] = rut_attribute_new(tex_attrib->buffered.buffer,
                                        "cg_tex_coord7_in",
                                        tex_attrib->buffered.stride,
                                        tex_attrib->buffered.offset,
                                        2,
                                        RUT_ATTRIBUTE_TYPE_FLOAT);

    attributes[i++] = rut_attribute_new(tex_attrib->buffered.buffer,
                                        "cg_tex_coord11_in",
                                        tex_attrib->buffered.stride,
                                        tex_attrib->buffered.offset,
                                        2,
                                        RUT_ATTRIBUTE_TYPE_FLOAT);

    /* NB: Don't just append extra attributes here without allocating a
     * larger array above... */
    c_assert(i == mesh->n_attributes + 4);

    rut_mesh_set_attributes(model->mesh, attributes, i);

    return model;
}

rig_model_t *
rig_model_new_from_asset(rig_engine_t *engine, rig_asset_t *asset)
{
    rut_mesh_t *mesh = rig_asset_get_mesh(asset);
    rig_model_t *model;
    bool needs_normals = !rig_asset_get_mesh_has_normals(asset);
    bool needs_tex_coords = !rig_asset_get_mesh_has_tex_coords(asset);

    if (!mesh)
        return NULL;

    model = rig_model_new_from_asset_mesh(engine, mesh, needs_normals,
                                          needs_tex_coords);
    model->asset = rut_object_ref(asset);

    return model;
}

rig_model_t *
rig_model_new_for_hair(rig_model_t *base)
{
    rig_model_t *model = _rig_model_copy(base);
    rut_shell_t *shell = rig_component_props_get_shell(&model->component);
    int n_vertices;
    int i;

    c_return_val_if_fail(!base->is_hair_model, NULL);

    if (model->primitive) {
        cg_object_unref(model->primitive);
        model->primitive = NULL;
    }

    model->is_hair_model = true;
    model->patched_mesh = NULL;
    model->fin_mesh = NULL;

    model->priv = c_new(rig_model_private_t, 1);

    _rut_bitmask_init(&model->priv->adj_matrix);

    model->priv->texture_patches = NULL;

    n_vertices = model->mesh->indices_buffer ? model->mesh->n_indices
                 : model->mesh->n_vertices;

    model->priv->polygons = c_new(polygon_t, n_vertices / 3);
    model->priv->vertices = c_new(vertex_t, n_vertices);

    model->priv->n_fin_polygons = 0;
    model->priv->n_fin_vertices = 0;
    model->priv->fin_polygons = c_new(polygon_t, (n_vertices / 3) * 6);
    model->priv->fin_vertices = c_new(vertex_t, n_vertices * 4);

    model->priv->n_vertices = 0;
    model->priv->n_polygons = 0;
    rut_mesh_foreach_triangle(model->mesh,
                              generate_polygons_for_patching,
                              model,
                              "cg_position_in",
                              "cg_normal_in",
                              "tangent_in",
                              "cg_tex_coord0_in",
                              NULL);

    /* TODO: we can fold this into generate_polygons_for_patching */
    model->priv->n_polygons = 0;
    rut_mesh_foreach_triangle(model->mesh,
                              copy_tangents_to_polygons,
                              model,
                              "cg_normal_in",
                              "tangent_in",
                              NULL);

    model->patched_mesh = create_patched_mesh_from_model(model);

    for (i = 0; i < model->priv->n_polygons; i++)
        add_polygon_fins(model, &model->priv->polygons[i]);

    model->fin_mesh = create_fin_mesh_from_model(model);

    model->fin_primitive =
        rut_mesh_create_primitive(shell, model->fin_mesh);

    rut_object_unref(model->mesh);
    model->mesh = model->patched_mesh;

    model->default_hair_length = rig_model_get_default_hair_length(model);

    return model;
}

rut_mesh_t *
rig_model_get_mesh(rut_object_t *self)
{
    rig_model_t *model = self;
    return model->mesh;
}

rig_asset_t *
rig_model_get_asset(rig_model_t *model)
{
    return model->asset;
}

float
rig_model_get_default_hair_length(rut_object_t *object)
{
    rig_model_t *model = object;
    float x_size, y_size, z_size;

    if (model->default_hair_length > 0)
        return model->default_hair_length;

    model->default_hair_length = 0;

    x_size = fabsf(model->max_x - model->min_x) / 5.0;
    y_size = fabsf(model->max_y - model->min_y) / 5.0;
    z_size = fabsf(model->max_z - model->min_z) / 5.0;

    if (x_size < y_size && x_size > 0)
        model->default_hair_length = x_size;

    if (y_size < x_size && y_size > 0)
        model->default_hair_length = y_size;

    if (z_size < model->default_hair_length && z_size > 0)
        model->default_hair_length = z_size;

    return model->default_hair_length;
}
