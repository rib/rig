/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation.
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
 *
 */

#include <rig-config.h>

#include <clib.h>

#include <cglib/cglib.h>

#include <rut.h>

#include "rig-entity.h"
#include "rig-entity-inlines.h"
#include "rig-nine-slice.h"

enum {
    RIG_NINE_SLICE_PROP_WIDTH,
    RIG_NINE_SLICE_PROP_HEIGHT,
    RIG_NINE_SLICE_PROP_LEFT,
    RIG_NINE_SLICE_PROP_RIGHT,
    RIG_NINE_SLICE_PROP_TOP,
    RIG_NINE_SLICE_PROP_BOTTOM,
    RIG_NINE_SLICE_N_PROPS
};

struct _rig_nine_slice_t {
    rut_object_base_t _base;

    rut_componentable_props_t component;

    /* Since ::texture is optional so we track the width/height
     * separately */
    int tex_width;
    int tex_height;

    float left;
    float right;
    float top;
    float bottom;

    float width;
    float height;

    rut_mesh_t *mesh;

    c_list_t updated_cb_list;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RIG_NINE_SLICE_N_PROPS];
};

static rig_property_spec_t _rig_nine_slice_prop_specs[] = {
    { .name = "width",
      .nick = "Width",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rig_nine_slice_t, width),
      .setter.float_type = rig_nine_slice_set_width,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "height",
      .nick = "Height",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rig_nine_slice_t, height),
      .setter.float_type = rig_nine_slice_set_height,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "left",
      .nick = "Left",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rig_nine_slice_t, left),
      .setter.float_type = rig_nine_slice_set_left,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "right",
      .nick = "Right",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rig_nine_slice_t, right),
      .setter.float_type = rig_nine_slice_set_right,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "top",
      .nick = "Top",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rig_nine_slice_t, top),
      .setter.float_type = rig_nine_slice_set_top,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { .name = "bottom",
      .nick = "Bottom",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rig_nine_slice_t, bottom),
      .setter.float_type = rig_nine_slice_set_bottom,
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
    },
    { NULL }
};

typedef struct _vertex_p2t2t2_t {
    float x, y, s0, t0, s1, t1;
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
    float normal[3] = { 0, 0, 1 };
    float tangent[3] = { 1, 0, 0 };

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
    rut_mesh_set_indices(mesh,
                         CG_INDICES_TYPE_UNSIGNED_BYTE,
                         index_buffer,
                         sizeof(_rut_nine_slice_indices_data) /
                         sizeof(_rut_nine_slice_indices_data[0]));

    return mesh;
}

static void
create_mesh(rig_nine_slice_t *nine_slice)
{
    float tex_width = nine_slice->tex_width;
    float tex_height = nine_slice->tex_height;
    float width = nine_slice->width;
    float height = nine_slice->height;
    float left = nine_slice->left;
    float right = nine_slice->right;
    float top = nine_slice->top;
    float bottom = nine_slice->bottom;
    int n_vertices;

    /* x0,y0,x1,y1 and s0,t0,s1,t1 define the postion and texture
     * coordinates for the center rectangle... */
    float x0 = left;
    float y0 = top;
    float x1 = width - right;
    float y1 = height - bottom;

    /* tex coords 0 */
    float s0_1 = left / tex_width;
    float t0_1 = top / tex_height;
    float s1_1 = (tex_width - right) / tex_width;
    float t1_1 = (tex_height - bottom) / tex_height;

    /* tex coords 1 */
    float s0_0 = left / width;
    float t0_0 = top / height;
    float s1_0 = (width - right) / width;
    float t1_0 = (height - bottom) / height;

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

    vertex_p2t2t2_t vertices[] = { { 0, 0, 0, 0, 0, 0 },
                                   { x0, 0, s0_0, 0, s0_1, 0 },
                                   { x1, 0, s1_0, 0, s1_1, 0 },
                                   { width, 0, 1, 0, 1, 0 },
                                   { 0, y0, 0, t0_0, 0, t0_1 },
                                   { x0, y0, s0_0, t0_0, s0_1, t0_1 },
                                   { x1, y0, s1_0, t0_0, s1_1, t0_1 },
                                   { width, y0, 1, t0_0, 1, t0_1 },
                                   { 0, y1, 0, t1_0, 0, t1_1 },
                                   { x0, y1, s0_0, t1_0, s0_1, t1_1 },
                                   { x1, y1, s1_0, t1_0, s1_1, t1_1 },
                                   { width, y1, 1, t1_0, 1, t1_1 },
                                   { 0, height, 0, 1, 0, 1 },
                                   { x0, height, s0_0, 1, s0_1, 1 },
                                   { x1, height, s1_0, 1, s1_1, 1 },
                                   { width, height, 1, 1, 1, 1 }, };

    n_vertices = sizeof(vertices) / sizeof(vertex_p2t2t2_t);

    nine_slice->mesh =
        mesh_new_p2t2t2(CG_VERTICES_MODE_TRIANGLES, n_vertices, vertices);
}

static void
free_mesh(rig_nine_slice_t *nine_slice)
{
    if (nine_slice->mesh) {
        rut_object_unref(nine_slice->mesh);
        nine_slice->mesh = NULL;
    }
}

static void
_rig_nine_slice_free(void *object)
{
    rig_nine_slice_t *nine_slice = object;

#ifdef RIG_ENABLE_DEBUG
    {
        rut_componentable_props_t *component =
            rut_object_get_properties(object, RUT_TRAIT_ID_COMPONENTABLE);
        c_return_if_fail(!component->parented);
    }
#endif

    rut_closure_list_remove_all(&nine_slice->updated_cb_list);

    free_mesh(nine_slice);

    rig_introspectable_destroy(nine_slice);

    rut_object_free(rig_nine_slice_t, object);
}

static rut_object_t *
_rig_nine_slice_copy(rut_object_t *object)
{
    rig_nine_slice_t *nine_slice = object;
    rig_engine_t *engine = rig_component_props_get_engine(&nine_slice->component);

    return rig_nine_slice_new(engine,
                              nine_slice->top,
                              nine_slice->right,
                              nine_slice->bottom,
                              nine_slice->left,
                              nine_slice->width,
                              nine_slice->height);
}

rut_type_t rig_nine_slice_type;

static void
_rig_nine_slice_init_type(void)
{
    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_nine_slice_copy
    };

    static rut_primable_vtable_t primable_vtable = {
        .get_primitive = rig_nine_slice_get_primitive
    };

    static rut_meshable_vtable_t meshable_vtable = {
        .get_mesh = rig_nine_slice_get_pick_mesh
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rig_nine_slice_set_size,
        rig_nine_slice_get_size,
        rut_simple_sizable_get_preferred_width,
        rut_simple_sizable_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    static rut_image_size_dependant_vtable_t image_dependant_vtable = {
        .set_image_size = rig_nine_slice_set_image_size
    };

    rut_type_t *type = &rig_nine_slice_type;
#define TYPE rig_nine_slice_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_nine_slice_free);
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
                       RUT_TRAIT_ID_SIZABLE,
                       0, /* no implied properties */
                       &sizable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_IMAGE_SIZE_DEPENDENT,
                       0, /* no implied properties */
                       &image_dependant_vtable);

#undef TYPE
}

rig_nine_slice_t *
rig_nine_slice_new(rig_engine_t *engine,
                   float top,
                   float right,
                   float bottom,
                   float left,
                   float width,
                   float height)
{
    rig_nine_slice_t *nine_slice = rut_object_alloc0(
        rig_nine_slice_t, &rig_nine_slice_type, _rig_nine_slice_init_type);

    nine_slice->component.type = RIG_COMPONENT_TYPE_GEOMETRY;
    nine_slice->component.parented = false;
    nine_slice->component.engine = engine;

    c_list_init(&nine_slice->updated_cb_list);

    nine_slice->left = left;
    nine_slice->right = right;
    nine_slice->top = top;
    nine_slice->bottom = bottom;

    nine_slice->width = width;
    nine_slice->height = height;

    nine_slice->mesh = NULL;

    nine_slice->tex_width = width;
    nine_slice->tex_height = height;

    rig_introspectable_init(nine_slice, _rig_nine_slice_prop_specs,
                            nine_slice->properties);

    return nine_slice;
}

void
rig_nine_slice_set_image_size(rut_object_t *self, int width, int height)
{
    rig_nine_slice_t *nine_slice = self;

    if (nine_slice->tex_width == width && nine_slice->tex_height == height)
        return;

    free_mesh(nine_slice);

    nine_slice->tex_width = width;
    nine_slice->tex_height = height;

    rut_closure_list_invoke(&nine_slice->updated_cb_list,
                            rig_nine_slice_update_callback_t,
                            nine_slice);
}

void
rig_nine_slice_set_size(rut_object_t *self, float width, float height)
{
    rig_nine_slice_t *nine_slice = self;
    rig_property_context_t *prop_ctx;

    if (nine_slice->width == width && nine_slice->height == height)
        return;

    free_mesh(nine_slice);

    nine_slice->width = width;
    nine_slice->height = height;

    prop_ctx = rig_component_props_get_property_context(&nine_slice->component);
    rig_property_dirty(prop_ctx,
                       &nine_slice->properties[RIG_NINE_SLICE_PROP_WIDTH]);
    rig_property_dirty(prop_ctx,
                       &nine_slice->properties[RIG_NINE_SLICE_PROP_HEIGHT]);

    rut_closure_list_invoke(&nine_slice->updated_cb_list,
                            rig_nine_slice_update_callback_t,
                            nine_slice);
}

void
rig_nine_slice_get_size(rut_object_t *self, float *width, float *height)
{
    rig_nine_slice_t *nine_slice = self;
    *width = nine_slice->width;
    *height = nine_slice->height;
}

cg_primitive_t *
rig_nine_slice_get_primitive(rut_object_t *object)
{
    rig_nine_slice_t *nine_slice = object;
    rut_shell_t *shell =
        rig_component_props_get_shell(&nine_slice->component);

    if (!nine_slice->mesh)
        create_mesh(nine_slice);

    return rut_mesh_create_primitive(shell, nine_slice->mesh);
}

rut_mesh_t *
rig_nine_slice_get_pick_mesh(rut_object_t *object)
{
    rig_nine_slice_t *nine_slice = object;

    if (!nine_slice->mesh)
        create_mesh(nine_slice);

    return nine_slice->mesh;
}

void
rig_nine_slice_add_update_callback(rig_nine_slice_t *nine_slice,
                                   rut_closure_t *closure)
{
    c_return_if_fail(closure != NULL);

    return rut_closure_list_add(&nine_slice->updated_cb_list, closure);
}

#define SLICE_PROPERTY(PROP_LC, PROP_UC)                                       \
    void rig_nine_slice_set_##PROP_LC(rut_object_t *obj, float PROP_LC)        \
    {                                                                          \
        rig_nine_slice_t *nine_slice = obj;                                    \
        rig_property_context_t *prop_ctx;                                      \
        if (nine_slice->PROP_LC == PROP_LC)                                    \
            return;                                                            \
        nine_slice->PROP_LC = PROP_LC;                                         \
        free_mesh(nine_slice);                                                 \
        prop_ctx = rig_component_props_get_property_context(&nine_slice->component); \
        rig_property_dirty(prop_ctx,                                           \
            &nine_slice->properties[RIG_NINE_SLICE_PROP_##PROP_UC]);           \
        rut_closure_list_invoke(&nine_slice->updated_cb_list,                  \
                                rig_nine_slice_update_callback_t,              \
                                nine_slice);                                   \
    }

SLICE_PROPERTY(width, WIDTH)
SLICE_PROPERTY(height, HEIGHT)
SLICE_PROPERTY(left, LEFT)
SLICE_PROPERTY(right, RIGHT)
SLICE_PROPERTY(top, TOP)
SLICE_PROPERTY(bottom, BOTTOM)

#undef SLICE_PROPERTY
