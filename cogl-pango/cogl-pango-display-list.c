/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <string.h>

#include "cogl-pango-display-list.h"
#include "cogl-pango-pipeline-cache.h"
#include "cogl/cogl-device-private.h"

typedef enum {
    CG_PANGO_DISPLAY_LIST_TEXTURE,
    CG_PANGO_DISPLAY_LIST_RECTANGLE,
    CG_PANGO_DISPLAY_LIST_TRAPEZOID
} cg_pango_display_list_node_type_t;

typedef struct _cg_pango_display_list_node_t cg_pango_display_list_node_t;
typedef struct _cg_pango_display_list_rectangle_t
    cg_pango_display_list_rectangle_t;

struct _cg_pango_display_list_t {
    bool color_override;
    cg_color_t color;
    GSList *nodes;
    GSList *last_node;
    cg_pango_pipeline_cache_t *pipeline_cache;
};

/* Matches the format expected by cg_framebuffer_draw_textured_rectangles */
struct _cg_pango_display_list_rectangle_t {
    float x_1, y_1, x_2, y_2;
    float s_1, t_1, s_2, t_2;
};

struct _cg_pango_display_list_node_t {
    cg_pango_display_list_node_type_t type;

    bool color_override;
    cg_color_t color;

    cg_pipeline_t *pipeline;

    union {
        struct {
            /* The texture to render these coords from */
            cg_texture_t *texture;
            /* Array of rectangles in the format expected by
               cg_framebuffer_draw_textured_rectangles */
            GArray *rectangles;
            /* A primitive representing those vertices */
            cg_primitive_t *primitive;
        } texture;

        struct {
            float x_1, y_1;
            float x_2, y_2;
        } rectangle;

        struct {
            cg_primitive_t *primitive;
        } trapezoid;
    } d;
};

cg_pango_display_list_t *
_cg_pango_display_list_new(cg_pango_pipeline_cache_t *pipeline_cache)
{
    cg_pango_display_list_t *dl = g_slice_new0(cg_pango_display_list_t);

    dl->pipeline_cache = pipeline_cache;

    return dl;
}

static void
_cg_pango_display_list_append_node(cg_pango_display_list_t *dl,
                                   cg_pango_display_list_node_t *node)
{
    if (dl->last_node)
        dl->last_node = dl->last_node->next = g_slist_prepend(NULL, node);
    else
        dl->last_node = dl->nodes = g_slist_prepend(NULL, node);
}

void
_cg_pango_display_list_set_color_override(cg_pango_display_list_t *dl,
                                          const cg_color_t *color)
{
    dl->color_override = true;
    dl->color = *color;
}

void
_cg_pango_display_list_remove_color_override(cg_pango_display_list_t *dl)
{
    dl->color_override = false;
}

void
_cg_pango_display_list_add_texture(cg_pango_display_list_t *dl,
                                   cg_texture_t *texture,
                                   float x_1,
                                   float y_1,
                                   float x_2,
                                   float y_2,
                                   float tx_1,
                                   float ty_1,
                                   float tx_2,
                                   float ty_2)
{
    cg_pango_display_list_node_t *node;
    cg_pango_display_list_rectangle_t *rectangle;

    /* Add to the last node if it is a texture node with the same
       target texture */
    if (dl->last_node &&
        (node = dl->last_node->data)->type == CG_PANGO_DISPLAY_LIST_TEXTURE &&
        node->d.texture.texture == texture &&
        (dl->color_override ? (node->color_override &&
                               cg_color_equal(&dl->color, &node->color))
         : !node->color_override)) {
        /* Get rid of the vertex buffer so that it will be recreated */
        if (node->d.texture.primitive != NULL) {
            cg_object_unref(node->d.texture.primitive);
            node->d.texture.primitive = NULL;
        }
    } else {
        /* Otherwise create a new node */
        node = g_slice_new(cg_pango_display_list_node_t);

        node->type = CG_PANGO_DISPLAY_LIST_TEXTURE;
        node->color_override = dl->color_override;
        node->color = dl->color;
        node->pipeline = NULL;
        node->d.texture.texture = cg_object_ref(texture);
        node->d.texture.rectangles = g_array_new(
            false, false, sizeof(cg_pango_display_list_rectangle_t));
        node->d.texture.primitive = NULL;

        _cg_pango_display_list_append_node(dl, node);
    }

    g_array_set_size(node->d.texture.rectangles,
                     node->d.texture.rectangles->len + 1);
    rectangle = &g_array_index(node->d.texture.rectangles,
                               cg_pango_display_list_rectangle_t,
                               node->d.texture.rectangles->len - 1);
    rectangle->x_1 = x_1;
    rectangle->y_1 = y_1;
    rectangle->x_2 = x_2;
    rectangle->y_2 = y_2;
    rectangle->s_1 = tx_1;
    rectangle->t_1 = ty_1;
    rectangle->s_2 = tx_2;
    rectangle->t_2 = ty_2;
}

void
_cg_pango_display_list_add_rectangle(
    cg_pango_display_list_t *dl, float x_1, float y_1, float x_2, float y_2)
{
    cg_pango_display_list_node_t *node =
        g_slice_new(cg_pango_display_list_node_t);

    node->type = CG_PANGO_DISPLAY_LIST_RECTANGLE;
    node->color_override = dl->color_override;
    node->color = dl->color;
    node->d.rectangle.x_1 = x_1;
    node->d.rectangle.y_1 = y_1;
    node->d.rectangle.x_2 = x_2;
    node->d.rectangle.y_2 = y_2;
    node->pipeline = NULL;

    _cg_pango_display_list_append_node(dl, node);
}

void
_cg_pango_display_list_add_trapezoid(cg_pango_display_list_t *dl,
                                     float y_1,
                                     float x_11,
                                     float x_21,
                                     float y_2,
                                     float x_12,
                                     float x_22)
{
    cg_device_t *dev = dl->pipeline_cache->dev;
    cg_pango_display_list_node_t *node =
        g_slice_new(cg_pango_display_list_node_t);
    cg_vertex_p2_t vertices[4] = {
        { x_11, y_1 }, { x_12, y_2 }, { x_22, y_2 }, { x_21, y_1 }
    };

    node->type = CG_PANGO_DISPLAY_LIST_TRAPEZOID;
    node->color_override = dl->color_override;
    node->color = dl->color;
    node->pipeline = NULL;

    node->d.trapezoid.primitive =
        cg_primitive_new_p2(dev, CG_VERTICES_MODE_TRIANGLE_FAN, 4, vertices);

    _cg_pango_display_list_append_node(dl, node);
}

static void
emit_rectangles_through_journal(cg_framebuffer_t *fb,
                                cg_pipeline_t *pipeline,
                                cg_pango_display_list_node_t *node)
{
    const float *rectangles = (const float *)node->d.texture.rectangles->data;

    cg_framebuffer_draw_textured_rectangles(
        fb, pipeline, rectangles, node->d.texture.rectangles->len);
}

static void
emit_vertex_buffer_geometry(cg_framebuffer_t *fb,
                            cg_pipeline_t *pipeline,
                            cg_pango_display_list_node_t *node)
{
    cg_device_t *dev = fb->dev;

    /* It's expensive to go through the Cogl journal for large runs
     * of text in part because the journal transforms the quads in software
     * to avoid changing the modelview matrix. So for larger runs of text
     * we load the vertices into a VBO, and this has the added advantage
     * that if the text doesn't change from frame to frame the VBO can
     * be re-used avoiding the repeated cost of validating the data and
     * mapping it into the GPU... */

    if (node->d.texture.primitive == NULL) {
        cg_attribute_buffer_t *buffer;
        cg_vertex_p2t2_t *verts, *v;
        int n_verts;
        bool allocated = false;
        cg_attribute_t *attributes[2];
        cg_primitive_t *prim;
        int i;
        cg_error_t *ignore_error = NULL;

        n_verts = node->d.texture.rectangles->len * 4;

        buffer = cg_attribute_buffer_new_with_size(dev,
                                                   n_verts * sizeof(cg_vertex_p2t2_t));

        if ((verts = cg_buffer_map(CG_BUFFER(buffer),
                                   CG_BUFFER_ACCESS_WRITE,
                                   CG_BUFFER_MAP_HINT_DISCARD,
                                   &ignore_error)) == NULL) {
            cg_error_free(ignore_error);
            verts = g_new(cg_vertex_p2t2_t, n_verts);
            allocated = true;
        }

        v = verts;

        /* Copy the rectangles into the buffer and expand into four
           vertices instead of just two */
        for (i = 0; i < node->d.texture.rectangles->len; i++) {
            const cg_pango_display_list_rectangle_t *rectangle =
                &g_array_index(node->d.texture.rectangles,
                               cg_pango_display_list_rectangle_t,
                               i);

            v->x = rectangle->x_1;
            v->y = rectangle->y_1;
            v->s = rectangle->s_1;
            v->t = rectangle->t_1;
            v++;
            v->x = rectangle->x_1;
            v->y = rectangle->y_2;
            v->s = rectangle->s_1;
            v->t = rectangle->t_2;
            v++;
            v->x = rectangle->x_2;
            v->y = rectangle->y_2;
            v->s = rectangle->s_2;
            v->t = rectangle->t_2;
            v++;
            v->x = rectangle->x_2;
            v->y = rectangle->y_1;
            v->s = rectangle->s_2;
            v->t = rectangle->t_1;
            v++;
        }

        if (allocated) {
            cg_buffer_set_data(CG_BUFFER(buffer),
                               0, /* offset */
                               verts,
                               sizeof(cg_vertex_p2t2_t) * n_verts,
                               NULL);
            g_free(verts);
        } else
            cg_buffer_unmap(CG_BUFFER(buffer));

        attributes[0] = cg_attribute_new(buffer,
                                         "cg_position_in",
                                         sizeof(cg_vertex_p2t2_t),
                                         G_STRUCT_OFFSET(cg_vertex_p2t2_t, x),
                                         2, /* n_components */
                                         CG_ATTRIBUTE_TYPE_FLOAT);
        attributes[1] = cg_attribute_new(buffer,
                                         "cg_tex_coord0_in",
                                         sizeof(cg_vertex_p2t2_t),
                                         G_STRUCT_OFFSET(cg_vertex_p2t2_t, s),
                                         2, /* n_components */
                                         CG_ATTRIBUTE_TYPE_FLOAT);

        prim = cg_primitive_new_with_attributes(CG_VERTICES_MODE_TRIANGLES,
                                                n_verts,
                                                attributes,
                                                2 /* n_attributes */);

#ifdef CG_HAS_GL
        if (_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_QUADS))
            cg_primitive_set_mode(prim, GL_QUADS);
        else
#endif
        {
            /* GLES doesn't support GL_QUADS so instead we use a VBO
               with indexed vertices to generate GL_TRIANGLES from the
               quads */

            cg_indices_t *indices =
                cg_get_rectangle_indices(dev, node->d.texture.rectangles->len);

            cg_primitive_set_indices(
                prim, indices, node->d.texture.rectangles->len * 6);
        }

        node->d.texture.primitive = prim;

        cg_object_unref(buffer);
        cg_object_unref(attributes[0]);
        cg_object_unref(attributes[1]);
    }

    cg_primitive_draw(node->d.texture.primitive, fb, pipeline);
}

static void
_cg_framebuffer_draw_display_list_texture(cg_framebuffer_t *fb,
                                          cg_pipeline_t *pipeline,
                                          cg_pango_display_list_node_t *node)
{
    /* For small runs of text like icon labels, we can get better performance
     * going through the Cogl journal since text may then be batched together
     * with other geometry. */
    /* FIXME: 25 is a number I plucked out of thin air; it would be good
     * to determine this empirically! */
    if (node->d.texture.rectangles->len < 25)
        emit_rectangles_through_journal(fb, pipeline, node);
    else
        emit_vertex_buffer_geometry(fb, pipeline, node);
}

void
_cg_pango_display_list_render(cg_framebuffer_t *fb,
                              cg_pango_display_list_t *dl,
                              const cg_color_t *color)
{
    GSList *l;

    for (l = dl->nodes; l; l = l->next) {
        cg_pango_display_list_node_t *node = l->data;
        cg_color_t draw_color;

        if (node->pipeline == NULL) {
            if (node->type == CG_PANGO_DISPLAY_LIST_TEXTURE)
                node->pipeline = _cg_pango_pipeline_cache_get(
                    dl->pipeline_cache, node->d.texture.texture);
            else
                node->pipeline =
                    _cg_pango_pipeline_cache_get(dl->pipeline_cache, NULL);
        }

        if (node->color_override)
            /* Use the override color but preserve the alpha from the
               draw color */
            cg_color_init_from_4ub(&draw_color,
                                   cg_color_get_red_byte(&node->color),
                                   cg_color_get_green_byte(&node->color),
                                   cg_color_get_blue_byte(&node->color),
                                   cg_color_get_alpha_byte(color));
        else
            draw_color = *color;
        cg_color_premultiply(&draw_color);

        cg_pipeline_set_color(node->pipeline, &draw_color);

        switch (node->type) {
        case CG_PANGO_DISPLAY_LIST_TEXTURE:
            _cg_framebuffer_draw_display_list_texture(fb, node->pipeline, node);
            break;

        case CG_PANGO_DISPLAY_LIST_RECTANGLE:
            cg_framebuffer_draw_rectangle(fb,
                                          node->pipeline,
                                          node->d.rectangle.x_1,
                                          node->d.rectangle.y_1,
                                          node->d.rectangle.x_2,
                                          node->d.rectangle.y_2);
            break;

        case CG_PANGO_DISPLAY_LIST_TRAPEZOID:
            cg_primitive_draw(node->d.trapezoid.primitive, fb, node->pipeline);
            break;
        }
    }
}

static void
_cg_pango_display_list_node_free(cg_pango_display_list_node_t *node)
{
    if (node->type == CG_PANGO_DISPLAY_LIST_TEXTURE) {
        g_array_free(node->d.texture.rectangles, true);
        if (node->d.texture.texture != NULL)
            cg_object_unref(node->d.texture.texture);
        if (node->d.texture.primitive != NULL)
            cg_object_unref(node->d.texture.primitive);
    } else if (node->type == CG_PANGO_DISPLAY_LIST_TRAPEZOID)
        cg_object_unref(node->d.trapezoid.primitive);

    if (node->pipeline)
        cg_object_unref(node->pipeline);

    g_slice_free(cg_pango_display_list_node_t, node);
}

void
_cg_pango_display_list_clear(cg_pango_display_list_t *dl)
{
    g_slist_foreach(dl->nodes, (GFunc)_cg_pango_display_list_node_free, NULL);
    g_slist_free(dl->nodes);
    dl->nodes = NULL;
    dl->last_node = NULL;
}

void
_cg_pango_display_list_free(cg_pango_display_list_t *dl)
{
    _cg_pango_display_list_clear(dl);
    g_slice_free(cg_pango_display_list_t, dl);
}
