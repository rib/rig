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

    rut_object_free(rig_model_t, model);
}

static rut_object_t *
_rig_model_copy(rut_object_t *object)
{
    rig_model_t *model = object;
    rig_engine_t *engine = rig_component_props_get_engine(&model->component);
    rig_model_t *copy = _rig_model_new(engine);

    copy->mesh = rut_object_ref(model->mesh);

    copy->min_x = model->min_x;
    copy->max_x = model->max_x;
    copy->min_y = model->min_y;
    copy->max_y = model->max_y;
    copy->min_z = model->min_z;
    copy->max_z = model->max_z;

    if (model->primitive)
        copy->primitive = cg_object_ref(model->primitive);

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

    static rut_meshable_vtable_t meshable_vtable = {
        .get_mesh = rig_model_get_mesh
    };

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

void
rig_model_update_bounds(rig_model_t *model)
{
    rut_attribute_t *attribute =
        rut_mesh_find_attribute(model->mesh, "cg_position_in");
    rut_mesh_vertex_callback_t measure_callback;

    c_return_if_fail(attribute->is_buffered);

    model->min_x = FLT_MAX;
    model->max_x = -FLT_MAX;
    model->min_y = FLT_MAX;
    model->max_y = -FLT_MAX;
    model->min_z = FLT_MAX;
    model->max_z = -FLT_MAX;

    switch (attribute->buffered.n_components) {
    case 1:
        model->min_y = model->max_y = 0;
        model->min_z = model->max_z = 0;
        measure_callback = measure_mesh_x_cb;
        break;
    case 2:
        model->min_z = model->max_z = 0;
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

    rut_mesh_foreach_vertex(model->mesh,
                            measure_callback,
                            model,
                            "cg_position_in",
                            NULL);
}

rig_model_t *
rig_model_new(rig_engine_t *engine, rut_mesh_t *mesh)
{
    rig_model_t *model;
    rut_attribute_t **attributes;
    int i;
    rut_attribute_t *tex_attrib;

    model = _rig_model_new(engine);
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

    /* When rendering we expect that every model has a specific set of
     * texture coordinate attributes that may be required depending
     * on the material state used in conjunction with the model.
     *
     * We currently assume a mesh will only have one set of texture
     * coodinates and so all the remaining sets of texture coordinates
     * will simply be an alias of those...
     *
     * FIXME: the renderer should be what's responsible for ensuring
     * we have all the necessary attributes
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

rut_mesh_t *
rig_model_get_mesh(rut_object_t *self)
{
    rig_model_t *model = self;
    return model->mesh;
}
