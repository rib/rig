/*
 * Rut
 *
 * Rig Utilities
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

#include <config.h>

#include <clib.h>

#include <cglib/cglib.h>

#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-camera.h"
#include "rut-nine-slice.h"
#include "rut-closure.h"
#include "rut-meshable.h"
#include "rut-introspectable.h"

enum {
    RUT_NINE_SLICE_PROP_WIDTH,
    RUT_NINE_SLICE_PROP_HEIGHT,
    RUT_NINE_SLICE_PROP_LEFT,
    RUT_NINE_SLICE_PROP_RIGHT,
    RUT_NINE_SLICE_PROP_TOP,
    RUT_NINE_SLICE_PROP_BOTTOM,
    RUT_NINE_SLICE_N_PROPS
};

struct _rut_nine_slice_t {
    rut_object_base_t _base;

    rut_shell_t *shell;

    /* NB: The texture and pipeline properties are only used when using
     * a nine-slice as a traditional widget. When using a nine-slice as
     * a component then this will be NULL and the texture will be
     * defined by a material component. */
    cg_texture_t *texture;
    cg_pipeline_t *pipeline;

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

    rut_graphable_props_t graphable;
    rut_paintable_props_t paintable;

    c_list_t updated_cb_list;

    rut_introspectable_props_t introspectable;
    rig_property_t properties[RUT_NINE_SLICE_N_PROPS];
};

static rig_property_spec_t _rut_nine_slice_prop_specs[] = {
    { .name = "width",
      .nick = "Width",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rut_nine_slice_t, width),
      .setter.float_type = rut_nine_slice_set_width,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { .name = "height",
      .nick = "Height",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rut_nine_slice_t, height),
      .setter.float_type = rut_nine_slice_set_height,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { .name = "left",
      .nick = "Left",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rut_nine_slice_t, left),
      .setter.float_type = rut_nine_slice_set_left,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { .name = "right",
      .nick = "Right",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rut_nine_slice_t, right),
      .setter.float_type = rut_nine_slice_set_right,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { .name = "top",
      .nick = "Top",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rut_nine_slice_t, top),
      .setter.float_type = rut_nine_slice_set_top,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { .name = "bottom",
      .nick = "Bottom",
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = C_STRUCT_OFFSET(rut_nine_slice_t, bottom),
      .setter.float_type = rut_nine_slice_set_bottom,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
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
create_mesh(rut_nine_slice_t *nine_slice)
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
free_mesh(rut_nine_slice_t *nine_slice)
{
    if (nine_slice->mesh) {
        rut_object_unref(nine_slice->mesh);
        nine_slice->mesh = NULL;
    }
}

static void
_rut_nine_slice_free(void *object)
{
    rut_nine_slice_t *nine_slice = object;

    rut_closure_list_disconnect_all_FIXME(&nine_slice->updated_cb_list);

    if (nine_slice->texture)
        cg_object_unref(nine_slice->texture);

    if (nine_slice->pipeline)
        cg_object_unref(nine_slice->pipeline);

    free_mesh(nine_slice);

    rut_graphable_destroy(nine_slice);

    rut_introspectable_destroy(nine_slice);

    rut_object_free(rut_nine_slice_t, object);
}

static void
_rut_nine_slice_paint(rut_object_t *object,
                      rut_paint_context_t *paint_ctx)
{
    rut_nine_slice_t *nine_slice = object;
    rut_object_t *camera = paint_ctx->camera;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);

    float left = nine_slice->left;
    float right = nine_slice->right;
    float top = nine_slice->top;
    float bottom = nine_slice->bottom;

    /* simple stretch */
    if (left == 0 && right == 0 && top == 0 && bottom == 0) {
        cg_framebuffer_draw_rectangle(fb,
                                      nine_slice->pipeline,
                                      0,
                                      0,
                                      nine_slice->width,
                                      nine_slice->height);
    } else {
        float width = nine_slice->width;
        float height = nine_slice->height;
        cg_texture_t *texture = nine_slice->texture;
        float tex_width = cg_texture_get_width(texture);
        float tex_height = cg_texture_get_height(texture);

        /* s0,t0,s1,t1 define the texture coordinates for the center
         * rectangle... */
        float s0 = left / tex_width;
        float t0 = top / tex_height;
        float s1 = (tex_width - right) / tex_width;
        float t1 = (tex_height - bottom) / tex_height;

        float ex;
        float ey;

        ex = width - right;
        if (ex < left)
            ex = left;

        ey = height - bottom;
        if (ey < top)
            ey = top;

        {
            float rectangles[] = {
                /* top left corner */
                0,    0,   left,                   top,
                0.0,  0.0, s0,                     t0,

                /* top middle */
                left, 0,   MAX(left, ex),          top,
                s0,   0.0, s1,                     t0,

                /* top right */
                ex,   0,   MAX(ex + right, width), top,
                s1,   0.0, 1.0,                    t0,

                /* mid left */
                0,    top, left,                   ey,
                0.0,  t0,  s0,                     t1,

                /* center */
                left, top, ex,                     ey,
                s0,   t0,  s1,                     t1,

                /* mid right */
                ex,   top, MAX(ex + right, width), ey,
                s1,   t0,  1.0,                    t1,

                /* bottom left */
                0,    ey,  left,                   MAX(ey + bottom, height),
                0.0,  t1,  s0,                     1.0,

                /* bottom center */
                left, ey,  ex,                     MAX(ey + bottom, height),
                s0,   t1,  s1,                     1.0,

                /* bottom right */
                ex,   ey,  MAX(ex + right, width), MAX(ey + bottom, height),
                s1,   t1,  1.0,                    1.0
            };

            cg_framebuffer_draw_textured_rectangles(
                fb, nine_slice->pipeline, rectangles, 9);
        }
    }
}

rut_type_t rut_nine_slice_type;

static void
_rut_nine_slice_init_type(void)
{

    static rut_graphable_vtable_t graphable_vtable = { NULL, /* child remove */
                                                       NULL, /* child add */
                                                       NULL /* parent changed */
    };

    static rut_paintable_vtable_t paintable_vtable = { _rut_nine_slice_paint };

    static rut_primable_vtable_t primable_vtable = {
        .get_primitive = rut_nine_slice_get_primitive
    };

    static rut_meshable_vtable_t meshable_vtable = {
        .get_mesh = rut_nine_slice_get_pick_mesh
    };

    static rut_sizable_vtable_t sizable_vtable = {
        rut_nine_slice_set_size,
        rut_nine_slice_get_size,
        rut_simple_sizable_get_preferred_width,
        rut_simple_sizable_get_preferred_height,
        NULL /* add_preferred_size_callback */
    };

    static rut_image_size_dependant_vtable_t image_dependant_vtable = {
        .set_image_size = rut_nine_slice_set_image_size
    };

    rut_type_t *type = &rut_nine_slice_type;
#define TYPE rut_nine_slice_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rut_nine_slice_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_GRAPHABLE,
                       offsetof(TYPE, graphable),
                       &graphable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_PAINTABLE,
                       offsetof(TYPE, paintable),
                       &paintable_vtable);
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

rut_nine_slice_t *
rut_nine_slice_new(rut_shell_t *shell,
                   cg_texture_t *texture,
                   float top,
                   float right,
                   float bottom,
                   float left,
                   float width,
                   float height)
{
    rut_nine_slice_t *nine_slice = rut_object_alloc0(
        rut_nine_slice_t, &rut_nine_slice_type, _rut_nine_slice_init_type);

    nine_slice->shell = shell;

    c_list_init(&nine_slice->updated_cb_list);

    rut_graphable_init(nine_slice);

    nine_slice->left = left;
    nine_slice->right = right;
    nine_slice->top = top;
    nine_slice->bottom = bottom;

    nine_slice->width = width;
    nine_slice->height = height;

    nine_slice->mesh = NULL;

    nine_slice->texture = NULL;
    nine_slice->pipeline = NULL;
    if (texture)
        rut_nine_slice_set_texture(nine_slice, texture);
    else {
        nine_slice->tex_width = width;
        nine_slice->tex_height = height;
    }

    rut_introspectable_init(
        nine_slice, _rut_nine_slice_prop_specs, nine_slice->properties);

    return nine_slice;
}

cg_texture_t *
rut_nine_slice_get_texture(rut_nine_slice_t *nine_slice)
{
    return nine_slice->texture;
}

void
rut_nine_slice_set_texture(rut_nine_slice_t *nine_slice,
                           cg_texture_t *texture)
{
    if (nine_slice->texture == texture)
        return;

    free_mesh(nine_slice);

    if (nine_slice->texture)
        cg_object_unref(nine_slice->texture);
    if (nine_slice->pipeline)
        cg_object_unref(nine_slice->pipeline);

    nine_slice->pipeline =
        cg_pipeline_copy(nine_slice->shell->single_texture_2d_template);

    if (texture) {
        nine_slice->tex_width = cg_texture_get_width(texture);
        nine_slice->tex_height = cg_texture_get_height(texture);

        nine_slice->texture = cg_object_ref(texture);
        cg_pipeline_set_layer_texture(nine_slice->pipeline, 0, texture);
    } else {
        nine_slice->tex_width = nine_slice->width;
        nine_slice->tex_height = nine_slice->height;
        nine_slice->texture = NULL;
    }
}

void
rut_nine_slice_set_image_size(rut_object_t *self, int width, int height)
{
    rut_nine_slice_t *nine_slice = self;

    if (nine_slice->tex_width == width && nine_slice->tex_height == height)
        return;

    free_mesh(nine_slice);

    nine_slice->tex_width = width;
    nine_slice->tex_height = height;

    rut_closure_list_invoke(&nine_slice->updated_cb_list,
                            rut_nine_slice_update_callback_t,
                            nine_slice);
}

void
rut_nine_slice_set_size(rut_object_t *self, float width, float height)
{
    rut_nine_slice_t *nine_slice = self;

    if (nine_slice->width == width && nine_slice->height == height)
        return;

    free_mesh(nine_slice);

    nine_slice->width = width;
    nine_slice->height = height;

    rig_property_dirty(&nine_slice->shell->property_ctx,
                       &nine_slice->properties[RUT_NINE_SLICE_PROP_WIDTH]);
    rig_property_dirty(&nine_slice->shell->property_ctx,
                       &nine_slice->properties[RUT_NINE_SLICE_PROP_HEIGHT]);

    rut_closure_list_invoke(&nine_slice->updated_cb_list,
                            rut_nine_slice_update_callback_t,
                            nine_slice);
}

void
rut_nine_slice_get_size(rut_object_t *self, float *width, float *height)
{
    rut_nine_slice_t *nine_slice = self;
    *width = nine_slice->width;
    *height = nine_slice->height;
}

cg_pipeline_t *
rut_nine_slice_get_pipeline(rut_nine_slice_t *nine_slice)
{
    return nine_slice->pipeline;
}

cg_primitive_t *
rut_nine_slice_get_primitive(rut_object_t *object)
{
    rut_nine_slice_t *nine_slice = object;

    if (!nine_slice->mesh)
        create_mesh(nine_slice);

    return rut_mesh_create_primitive(nine_slice->shell, nine_slice->mesh);
}

rut_mesh_t *
rut_nine_slice_get_pick_mesh(rut_object_t *object)
{
    rut_nine_slice_t *nine_slice = object;

    if (!nine_slice->mesh)
        create_mesh(nine_slice);

    return nine_slice->mesh;
}

rut_closure_t *
rut_nine_slice_add_update_callback(rut_nine_slice_t *nine_slice,
                                   rut_nine_slice_update_callback_t callback,
                                   void *user_data,
                                   rut_closure_destroy_callback_t destroy_cb)
{
    c_return_val_if_fail(callback != NULL, NULL);
    return rut_closure_list_add_FIXME(
        &nine_slice->updated_cb_list, callback, user_data, destroy_cb);
}

#define SLICE_PROPERTY(PROP_LC, PROP_UC)                                       \
    void rut_nine_slice_set_##PROP_LC(rut_object_t *obj, float PROP_LC)        \
    {                                                                          \
        rut_nine_slice_t *nine_slice = obj;                                    \
        if (nine_slice->PROP_LC == PROP_LC)                                    \
            return;                                                            \
        nine_slice->PROP_LC = PROP_LC;                                         \
        free_mesh(nine_slice);                                                 \
        rig_property_dirty(                                                    \
            &nine_slice->shell->property_ctx,                                    \
            &nine_slice->properties[RUT_NINE_SLICE_PROP_##PROP_UC]);           \
        rut_closure_list_invoke(&nine_slice->updated_cb_list,                  \
                                rut_nine_slice_update_callback_t,              \
                                nine_slice);                                   \
    }

SLICE_PROPERTY(width, WIDTH)
SLICE_PROPERTY(height, HEIGHT)
SLICE_PROPERTY(left, LEFT)
SLICE_PROPERTY(right, RIGHT)
SLICE_PROPERTY(top, TOP)
SLICE_PROPERTY(bottom, BOTTOM)

#undef SLICE_PROPERTY
