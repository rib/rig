/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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
 * Authors:
 *  Ivan Leben    <ivan@openedhand.com>
 *  Øyvind Kolås  <pippin@linux.intel.com>
 *  Neil Roberts  <neil@linux.intel.com>
 *  Robert Bragg  <robert@linux.intel.com>
 */

#include <config.h>

#include <clib.h>

#include <string.h>
#include <math.h>

#include "cogl-util.h"
#include "cogl-object.h"
#include "cogl-device-private.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-path.h"
#include "cogl-path-private.h"
#include "cogl-texture-private.h"
#include "cogl-primitives-private.h"
#include "cogl-private.h"
#include "cogl-attribute-private.h"
#include "cogl-primitive-private.h"
#include "tesselator/tesselator.h"

#define _CG_MAX_BEZ_RECURSE_DEPTH 16

static void _cg_path_free(cg_path_t *path);

static void _cg_path_build_fill_attribute_buffer(cg_path_t *path);
static cg_primitive_t *_cg_path_get_fill_primitive(cg_path_t *path);
static void _cg_path_build_stroke_attribute_buffer(cg_path_t *path);

CG_OBJECT_DEFINE(Path, path);

static void
_cg_path_data_clear_vbos(cg_path_data_t *data)
{
    int i;

    if (data->fill_attribute_buffer) {
        cg_object_unref(data->fill_attribute_buffer);
        cg_object_unref(data->fill_vbo_indices);

        for (i = 0; i < CG_PATH_N_ATTRIBUTES; i++)
            cg_object_unref(data->fill_attributes[i]);

        data->fill_attribute_buffer = NULL;
    }

    if (data->fill_primitive) {
        cg_object_unref(data->fill_primitive);
        data->fill_primitive = NULL;
    }

    if (data->stroke_attribute_buffer) {
        cg_object_unref(data->stroke_attribute_buffer);

        for (i = 0; i < data->stroke_n_attributes; i++)
            cg_object_unref(data->stroke_attributes[i]);

        c_free(data->stroke_attributes);

        data->stroke_attribute_buffer = NULL;
    }
}

static void
_cg_path_data_unref(cg_path_data_t *data)
{
    if (--data->ref_count <= 0) {
        _cg_path_data_clear_vbos(data);

        c_array_free(data->path_nodes, true);

        c_slice_free(cg_path_data_t, data);
    }
}

static void
_cg_path_modify(cg_path_t *path)
{
    /* This needs to be called whenever the path is about to be modified
       to implement copy-on-write semantics */

    /* If there is more than one path using the data then we need to
       copy the data instead */
    if (path->data->ref_count != 1) {
        cg_path_data_t *old_data = path->data;

        path->data = c_slice_dup(cg_path_data_t, old_data);
        path->data->path_nodes =
            c_array_new(false, false, sizeof(cg_path_node_t));
        c_array_append_vals(path->data->path_nodes,
                            old_data->path_nodes->data,
                            old_data->path_nodes->len);

        path->data->fill_attribute_buffer = NULL;
        path->data->fill_primitive = NULL;
        path->data->stroke_attribute_buffer = NULL;
        path->data->ref_count = 1;

        _cg_path_data_unref(old_data);
    } else
        /* The path is altered so the vbos will now be invalid */
        _cg_path_data_clear_vbos(path->data);
}

void
cg_path_set_fill_rule(cg_path_t *path, cg_path_fill_rule_t fill_rule)
{
    c_return_if_fail(cg_is_path(path));

    if (path->data->fill_rule != fill_rule) {
        _cg_path_modify(path);

        path->data->fill_rule = fill_rule;
    }
}

cg_path_fill_rule_t
cg_path_get_fill_rule(cg_path_t *path)
{
    c_return_val_if_fail(cg_is_path(path), CG_PATH_FILL_RULE_NON_ZERO);

    return path->data->fill_rule;
}

static void
_cg_path_add_node(cg_path_t *path, bool new_sub_path, float x, float y)
{
    cg_path_node_t new_node;
    cg_path_data_t *data;

    _cg_path_modify(path);

    data = path->data;

    new_node.x = x;
    new_node.y = y;
    new_node.path_size = 0;

    if (new_sub_path || data->path_nodes->len == 0)
        data->last_path = data->path_nodes->len;

    c_array_append_val(data->path_nodes, new_node);

    c_array_index(data->path_nodes, cg_path_node_t, data->last_path)
    .path_size++;

    if (data->path_nodes->len == 1) {
        data->path_nodes_min.x = data->path_nodes_max.x = x;
        data->path_nodes_min.y = data->path_nodes_max.y = y;
    } else {
        if (x < data->path_nodes_min.x)
            data->path_nodes_min.x = x;
        if (x > data->path_nodes_max.x)
            data->path_nodes_max.x = x;
        if (y < data->path_nodes_min.y)
            data->path_nodes_min.y = y;
        if (y > data->path_nodes_max.y)
            data->path_nodes_max.y = y;
    }

    /* Once the path nodes have been modified then we'll assume it's no
       longer a rectangle. cg_path_rectangle will set this back to
       true if this has been called from there */
    data->is_rectangle = false;
}

void
cg_path_stroke(cg_path_t *path,
               cg_framebuffer_t *framebuffer,
               cg_pipeline_t *pipeline)
{
    cg_path_data_t *data;
    cg_pipeline_t *copy = NULL;
    unsigned int path_start;
    int path_num = 0;
    cg_path_node_t *node;

    c_return_if_fail(cg_is_path(path));
    c_return_if_fail(cg_is_framebuffer(framebuffer));
    c_return_if_fail(cg_is_pipeline(pipeline));

    data = path->data;

    if (data->path_nodes->len == 0)
        return;

    if (cg_pipeline_get_n_layers(pipeline) != 0) {
        copy = cg_pipeline_copy(pipeline);
        _cg_pipeline_prune_to_n_layers(copy, 0);
        pipeline = copy;
    }

    _cg_path_build_stroke_attribute_buffer(path);

    for (path_start = 0; path_start < data->path_nodes->len;
         path_start += node->path_size) {
        cg_primitive_t *primitive;

        node = &c_array_index(data->path_nodes, cg_path_node_t, path_start);

        primitive =
            cg_primitive_new_with_attributes(CG_VERTICES_MODE_LINE_STRIP,
                                             node->path_size,
                                             &data->stroke_attributes[path_num],
                                             1);
        cg_primitive_draw(primitive, framebuffer, pipeline);
        cg_object_unref(primitive);

        path_num++;
    }

    if (copy)
        cg_object_unref(copy);
}

void
_cg_path_get_bounds(
    cg_path_t *path, float *min_x, float *min_y, float *max_x, float *max_y)
{
    cg_path_data_t *data = path->data;

    if (data->path_nodes->len == 0) {
        *min_x = 0.0f;
        *min_y = 0.0f;
        *max_x = 0.0f;
        *max_y = 0.0f;
    } else {
        *min_x = data->path_nodes_min.x;
        *min_y = data->path_nodes_min.y;
        *max_x = data->path_nodes_max.x;
        *max_y = data->path_nodes_max.y;
    }
}

static void
_cg_path_fill_nodes_with_clipped_rectangle(
    cg_path_t *path, cg_framebuffer_t *framebuffer, cg_pipeline_t *pipeline)
{
    /* We need at least three stencil bits to combine clips */
    if (_cg_framebuffer_get_stencil_bits(framebuffer) >= 3) {
        static bool seen_warning = false;

        if (!seen_warning) {
            c_warning("Paths can not be filled using materials with "
                      "sliced textures unless there is a stencil "
                      "buffer");
            seen_warning = true;
        }
    }

    cg_framebuffer_push_path_clip(framebuffer, path);
    cg_framebuffer_draw_rectangle(framebuffer,
                                  pipeline,
                                  path->data->path_nodes_min.x,
                                  path->data->path_nodes_min.y,
                                  path->data->path_nodes_max.x,
                                  path->data->path_nodes_max.y);
    cg_framebuffer_pop_clip(framebuffer);
}

static bool
validate_layer_cb(cg_pipeline_layer_t *layer, void *user_data)
{
    bool *needs_fallback = user_data;
    cg_texture_t *texture = _cg_pipeline_layer_get_texture(layer);

    /* If any of the layers of the current pipeline contain sliced
     * textures or textures with waste then it won't work to draw the
     * path directly. Instead we fallback to pushing the path as a clip
     * on the clip-stack and drawing the path's bounding rectangle
     * instead.
     */

    if (texture != NULL && (cg_texture_is_sliced(texture) ||
                            !_cg_texture_can_hardware_repeat(texture)))
        *needs_fallback = true;

    return !*needs_fallback;
}

void
_cg_path_fill_nodes(cg_path_t *path,
                    cg_framebuffer_t *framebuffer,
                    cg_pipeline_t *pipeline,
                    cg_draw_flags_t flags)
{
    if (path->data->path_nodes->len == 0)
        return;

    /* If the path is a simple rectangle then we can divert to using
       cg_framebuffer_draw_rectangle which should be faster because it
       can go through the journal instead of uploading the geometry just
       for two triangles */
    if (path->data->is_rectangle && flags == 0) {
        float x_1, y_1, x_2, y_2;

        _cg_path_get_bounds(path, &x_1, &y_1, &x_2, &y_2);
        cg_framebuffer_draw_rectangle(
            framebuffer, pipeline, x_1, y_1, x_2, y_2);
    } else {
        bool needs_fallback = false;
        cg_primitive_t *primitive;

        _cg_pipeline_foreach_layer_internal(
            pipeline, validate_layer_cb, &needs_fallback);
        if (needs_fallback) {
            _cg_path_fill_nodes_with_clipped_rectangle(
                path, framebuffer, pipeline);
            return;
        }

        primitive = _cg_path_get_fill_primitive(path);

        _cg_primitive_draw(primitive, framebuffer, pipeline, 1, flags);
    }
}

void
cg_path_fill(cg_path_t *path,
             cg_framebuffer_t *framebuffer,
             cg_pipeline_t *pipeline)
{
    c_return_if_fail(cg_is_path(path));
    c_return_if_fail(cg_is_framebuffer(framebuffer));
    c_return_if_fail(cg_is_pipeline(pipeline));

    _cg_path_fill_nodes(path, framebuffer, pipeline, 0 /* flags */);
}

void
cg_path_move_to(cg_path_t *path, float x, float y)
{
    cg_path_data_t *data;

    c_return_if_fail(cg_is_path(path));

    _cg_path_add_node(path, true, x, y);

    data = path->data;

    data->path_start.x = x;
    data->path_start.y = y;

    data->path_pen = data->path_start;
}

void
cg_path_rel_move_to(cg_path_t *path, float x, float y)
{
    cg_path_data_t *data;

    c_return_if_fail(cg_is_path(path));

    data = path->data;

    cg_path_move_to(path, data->path_pen.x + x, data->path_pen.y + y);
}

void
cg_path_line_to(cg_path_t *path, float x, float y)
{
    cg_path_data_t *data;

    c_return_if_fail(cg_is_path(path));

    _cg_path_add_node(path, false, x, y);

    data = path->data;

    data->path_pen.x = x;
    data->path_pen.y = y;
}

void
cg_path_rel_line_to(cg_path_t *path, float x, float y)
{
    cg_path_data_t *data;

    c_return_if_fail(cg_is_path(path));

    data = path->data;

    cg_path_line_to(path, data->path_pen.x + x, data->path_pen.y + y);
}

void
cg_path_close(cg_path_t *path)
{
    c_return_if_fail(cg_is_path(path));

    _cg_path_add_node(
        path, false, path->data->path_start.x, path->data->path_start.y);

    path->data->path_pen = path->data->path_start;
}

void
cg_path_line(cg_path_t *path, float x_1, float y_1, float x_2, float y_2)
{
    cg_path_move_to(path, x_1, y_1);
    cg_path_line_to(path, x_2, y_2);
}

void
cg_path_polyline(cg_path_t *path, const float *coords, int num_points)
{
    int c = 0;

    c_return_if_fail(cg_is_path(path));

    cg_path_move_to(path, coords[0], coords[1]);

    for (c = 1; c < num_points; ++c)
        cg_path_line_to(path, coords[2 * c], coords[2 * c + 1]);
}

void
cg_path_polygon(cg_path_t *path, const float *coords, int num_points)
{
    cg_path_polyline(path, coords, num_points);
    cg_path_close(path);
}

void
cg_path_rectangle(cg_path_t *path, float x_1, float y_1, float x_2, float y_2)
{
    bool is_rectangle;

    /* If the path was previously empty and the rectangle isn't mirrored
       then we'll record that this is a simple rectangle path so that we
       can optimise it */
    is_rectangle =
        (path->data->path_nodes->len == 0 && x_2 >= x_1 && y_2 >= y_1);

    cg_path_move_to(path, x_1, y_1);
    cg_path_line_to(path, x_2, y_1);
    cg_path_line_to(path, x_2, y_2);
    cg_path_line_to(path, x_1, y_2);
    cg_path_close(path);

    path->data->is_rectangle = is_rectangle;
}

bool
_cg_path_is_rectangle(cg_path_t *path)
{
    return path->data->is_rectangle;
}

static void
_cg_path_arc(cg_path_t *path,
             float center_x,
             float center_y,
             float radius_x,
             float radius_y,
             float angle_1,
             float angle_2,
             float angle_step,
             unsigned int move_first)
{
    float a = 0x0;
    float cosa = 0x0;
    float sina = 0x0;
    float px = 0x0;
    float py = 0x0;

    /* Fix invalid angles */

    if (angle_1 == angle_2 || angle_step == 0x0)
        return;

    if (angle_step < 0x0)
        angle_step = -angle_step;

    /* Walk the arc by given step */

    a = angle_1;
    while (a != angle_2) {
        cosa = cosf(a * (G_PI / 180.0));
        sina = sinf(a * (G_PI / 180.0));

        px = center_x + (cosa * radius_x);
        py = center_y + (sina * radius_y);

        if (a == angle_1 && move_first)
            cg_path_move_to(path, px, py);
        else
            cg_path_line_to(path, px, py);

        if (G_LIKELY(angle_2 > angle_1)) {
            a += angle_step;
            if (a > angle_2)
                a = angle_2;
        } else {
            a -= angle_step;
            if (a < angle_2)
                a = angle_2;
        }
    }

    /* Make sure the final point is drawn */

    cosa = cosf(angle_2 * (G_PI / 180.0));
    sina = sinf(angle_2 * (G_PI / 180.0));

    px = center_x + (cosa * radius_x);
    py = center_y + (sina * radius_y);

    cg_path_line_to(path, px, py);
}

void
cg_path_arc(cg_path_t *path,
            float center_x,
            float center_y,
            float radius_x,
            float radius_y,
            float angle_1,
            float angle_2)
{
    float angle_step = 10;

    c_return_if_fail(cg_is_path(path));

    /* it is documented that a move to is needed to create a freestanding
     * arc
     */
    _cg_path_arc(path,
                 center_x,
                 center_y,
                 radius_x,
                 radius_y,
                 angle_1,
                 angle_2,
                 angle_step,
                 0 /* no move */);
}

static void
_cg_path_rel_arc(cg_path_t *path,
                 float center_x,
                 float center_y,
                 float radius_x,
                 float radius_y,
                 float angle_1,
                 float angle_2,
                 float angle_step)
{
    cg_path_data_t *data;

    data = path->data;

    _cg_path_arc(path,
                 data->path_pen.x + center_x,
                 data->path_pen.y + center_y,
                 radius_x,
                 radius_y,
                 angle_1,
                 angle_2,
                 angle_step,
                 0 /* no move */);
}

void
cg_path_ellipse(cg_path_t *path,
                float center_x,
                float center_y,
                float radius_x,
                float radius_y)
{
    float angle_step = 10;

    c_return_if_fail(cg_is_path(path));

    /* FIXME: if shows to be slow might be optimized
     * by mirroring just a quarter of it */

    _cg_path_arc(path,
                 center_x,
                 center_y,
                 radius_x,
                 radius_y,
                 0,
                 360,
                 angle_step,
                 1 /* move first */);

    cg_path_close(path);
}

void
cg_path_round_rectangle(cg_path_t *path,
                        float x_1,
                        float y_1,
                        float x_2,
                        float y_2,
                        float radius,
                        float arc_step)
{
    float inner_width = x_2 - x_1 - radius * 2;
    float inner_height = y_2 - y_1 - radius * 2;

    c_return_if_fail(cg_is_path(path));

    cg_path_move_to(path, x_1, y_1 + radius);
    _cg_path_rel_arc(path, radius, 0, radius, radius, 180, 270, arc_step);

    cg_path_line_to(
        path, path->data->path_pen.x + inner_width, path->data->path_pen.y);
    _cg_path_rel_arc(path, 0, radius, radius, radius, -90, 0, arc_step);

    cg_path_line_to(
        path, path->data->path_pen.x, path->data->path_pen.y + inner_height);

    _cg_path_rel_arc(path, -radius, 0, radius, radius, 0, 90, arc_step);

    cg_path_line_to(
        path, path->data->path_pen.x - inner_width, path->data->path_pen.y);
    _cg_path_rel_arc(path, 0, -radius, radius, radius, 90, 180, arc_step);

    cg_path_close(path);
}

static void
_cg_path_bezier3_sub(cg_path_t *path, cg_bez_cubic_t *cubic)
{
    cg_bez_cubic_t cubics[_CG_MAX_BEZ_RECURSE_DEPTH];
    cg_bez_cubic_t *cleft;
    cg_bez_cubic_t *cright;
    cg_bez_cubic_t *c;
    float_vec2_t dif1;
    float_vec2_t dif2;
    float_vec2_t mm;
    float_vec2_t c1;
    float_vec2_t c2;
    float_vec2_t c3;
    float_vec2_t c4;
    float_vec2_t c5;
    int cindex;

    /* Put first curve on stack */
    cubics[0] = *cubic;
    cindex = 0;

    while (cindex >= 0) {
        c = &cubics[cindex];

        /* Calculate distance of control points from their
         * counterparts on the line between end points */
        dif1.x = (c->p2.x * 3) - (c->p1.x * 2) - c->p4.x;
        dif1.y = (c->p2.y * 3) - (c->p1.y * 2) - c->p4.y;
        dif2.x = (c->p3.x * 3) - (c->p4.x * 2) - c->p1.x;
        dif2.y = (c->p3.y * 3) - (c->p4.y * 2) - c->p1.y;

        if (dif1.x < 0)
            dif1.x = -dif1.x;
        if (dif1.y < 0)
            dif1.y = -dif1.y;
        if (dif2.x < 0)
            dif2.x = -dif2.x;
        if (dif2.y < 0)
            dif2.y = -dif2.y;

        /* Pick the greatest of two distances */
        if (dif1.x < dif2.x)
            dif1.x = dif2.x;
        if (dif1.y < dif2.y)
            dif1.y = dif2.y;

        /* Cancel if the curve is flat enough */
        if (dif1.x + dif1.y <= 1.0 || cindex == _CG_MAX_BEZ_RECURSE_DEPTH - 1) {
            /* Add subdivision point (skip last) */
            if (cindex == 0)
                return;

            _cg_path_add_node(path, false, c->p4.x, c->p4.y);

            --cindex;

            continue;
        }

        /* Left recursion goes on top of stack! */
        cright = c;
        cleft = &cubics[++cindex];

        /* Subdivide into 2 sub-curves */
        c1.x = ((c->p1.x + c->p2.x) / 2);
        c1.y = ((c->p1.y + c->p2.y) / 2);
        mm.x = ((c->p2.x + c->p3.x) / 2);
        mm.y = ((c->p2.y + c->p3.y) / 2);
        c5.x = ((c->p3.x + c->p4.x) / 2);
        c5.y = ((c->p3.y + c->p4.y) / 2);

        c2.x = ((c1.x + mm.x) / 2);
        c2.y = ((c1.y + mm.y) / 2);
        c4.x = ((mm.x + c5.x) / 2);
        c4.y = ((mm.y + c5.y) / 2);

        c3.x = ((c2.x + c4.x) / 2);
        c3.y = ((c2.y + c4.y) / 2);

        /* Add left recursion to stack */
        cleft->p1 = c->p1;
        cleft->p2 = c1;
        cleft->p3 = c2;
        cleft->p4 = c3;

        /* Add right recursion to stack */
        cright->p1 = c3;
        cright->p2 = c4;
        cright->p3 = c5;
        cright->p4 = c->p4;
    }
}

void
cg_path_curve_to(cg_path_t *path,
                 float x_1,
                 float y_1,
                 float x_2,
                 float y_2,
                 float x_3,
                 float y_3)
{
    cg_bez_cubic_t cubic;

    c_return_if_fail(cg_is_path(path));

    /* Prepare cubic curve */
    cubic.p1 = path->data->path_pen;
    cubic.p2.x = x_1;
    cubic.p2.y = y_1;
    cubic.p3.x = x_2;
    cubic.p3.y = y_2;
    cubic.p4.x = x_3;
    cubic.p4.y = y_3;

    /* Run subdivision */
    _cg_path_bezier3_sub(path, &cubic);

    /* Add last point */
    _cg_path_add_node(path, false, cubic.p4.x, cubic.p4.y);
    path->data->path_pen = cubic.p4;
}

void
cg_path_rel_curve_to(cg_path_t *path,
                     float x_1,
                     float y_1,
                     float x_2,
                     float y_2,
                     float x_3,
                     float y_3)
{
    cg_path_data_t *data;

    c_return_if_fail(cg_is_path(path));

    data = path->data;

    cg_path_curve_to(path,
                     data->path_pen.x + x_1,
                     data->path_pen.y + y_1,
                     data->path_pen.x + x_2,
                     data->path_pen.y + y_2,
                     data->path_pen.x + x_3,
                     data->path_pen.y + y_3);
}

cg_path_t *
cg_path_new(cg_device_t *dev)
{
    cg_path_t *path;
    cg_path_data_t *data;

    path = c_slice_new(cg_path_t);
    data = path->data = c_slice_new(cg_path_data_t);

    data->ref_count = 1;
    data->dev = dev;
    data->fill_rule = CG_PATH_FILL_RULE_EVEN_ODD;
    data->path_nodes = c_array_new(false, false, sizeof(cg_path_node_t));
    data->last_path = 0;
    data->fill_attribute_buffer = NULL;
    data->stroke_attribute_buffer = NULL;
    data->fill_primitive = NULL;
    data->is_rectangle = false;

    return _cg_path_object_new(path);
}

cg_path_t *
cg_path_copy(cg_path_t *old_path)
{
    cg_path_t *new_path;

    c_return_val_if_fail(cg_is_path(old_path), NULL);

    new_path = c_slice_new(cg_path_t);
    new_path->data = old_path->data;
    new_path->data->ref_count++;

    return _cg_path_object_new(new_path);
}

static void
_cg_path_free(cg_path_t *path)
{
    _cg_path_data_unref(path->data);
    c_slice_free(cg_path_t, path);
}

/* If second order beziers were needed the following code could
 * be re-enabled:
 */
#if 0

static void
_cg_path_bezier2_sub (cg_path_t *path,
                      cg_bez_quad_t *quad)
{
    cg_bez_quad_t quads[_CG_MAX_BEZ_RECURSE_DEPTH];
    cg_bez_quad_t *qleft;
    cg_bez_quad_t *qright;
    cg_bez_quad_t *q;
    float_vec2_t mid;
    float_vec2_t dif;
    float_vec2_t c1;
    float_vec2_t c2;
    float_vec2_t c3;
    int qindex;

    /* Put first curve on stack */
    quads[0] = *quad;
    qindex =  0;

    /* While stack is not empty */
    while (qindex >= 0)
    {

        q = &quads[qindex];

        /* Calculate distance of control point from its
        * counterpart on the line between end points */
        mid.x = ((q->p1.x + q->p3.x) / 2);
        mid.y = ((q->p1.y + q->p3.y) / 2);
        dif.x = (q->p2.x - mid.x);
        dif.y = (q->p2.y - mid.y);
        if (dif.x < 0) dif.x = -dif.x;
        if (dif.y < 0) dif.y = -dif.y;

        /* Cancel if the curve is flat enough */
        if (dif.x + dif.y <= 1.0 ||
            qindex == _CG_MAX_BEZ_RECURSE_DEPTH - 1)
        {
            /* Add subdivision point (skip last) */
            if (qindex == 0) return;
            _cg_path_add_node (path, false, q->p3.x, q->p3.y);
            --qindex; continue;
        }

        /* Left recursion goes on top of stack! */
        qright = q; qleft = &quads[++qindex];

        /* Subdivide into 2 sub-curves */
        c1.x = ((q->p1.x + q->p2.x) / 2);
        c1.y = ((q->p1.y + q->p2.y) / 2);
        c3.x = ((q->p2.x + q->p3.x) / 2);
        c3.y = ((q->p2.y + q->p3.y) / 2);
        c2.x = ((c1.x + c3.x) / 2);
        c2.y = ((c1.y + c3.y) / 2);

        /* Add left recursion onto stack */
        qleft->p1 = q->p1;
        qleft->p2 = c1;
        qleft->p3 = c2;

        /* Add right recursion onto stack */
        qright->p1 = c2;
        qright->p2 = c3;
        qright->p3 = q->p3;
    }
}

void
cg_path_curve2_to (cg_path_t *path,
                   float x_1,
                   float y_1,
                   float x_2,
                   float y_2)
{
    cg_bez_quad_t quad;

    /* Prepare quadratic curve */
    quad.p1 = path->data->path_pen;
    quad.p2.x = x_1;
    quad.p2.y = y_1;
    quad.p3.x = x_2;
    quad.p3.y = y_2;

    /* Run subdivision */
    _cg_path_bezier2_sub (&quad);

    /* Add last point */
    _cg_path_add_node (false, quad.p3.x, quad.p3.y);
    path->data->path_pen = quad.p3;
}

void
cg_rel_curve2_to (cg_path_t *path,
                  float x_1,
                  float y_1,
                  float x_2,
                  float y_2)
{
    cg_path_data_t *data;

    c_return_if_fail (cg_is_path (path));

    data = path->data;

    cg_path_curve2_to (data->path_pen.x + x_1,
                       data->path_pen.y + y_1,
                       data->path_pen.x + x_2,
                       data->path_pen.y + y_2);
}

#endif

typedef struct _cg_path_tesselator_t cg_path_tesselator_t;
typedef struct _cg_path_tesselator_vertex_t cg_path_tesselator_vertex_t;

struct _cg_path_tesselator_t {
    GLUtesselator *glc_tess;
    GLenum primitive_type;
    int vertex_number;
    /* Array of cg_path_tesselator_vertex_t. This needs to grow when the
       combine callback is called */
    c_array_t *vertices;
    /* Array of integers for the indices into the vertices array. Each
       element will either be uint8_t, uint16_t or uint32_t depending on
       the number of vertices */
    c_array_t *indices;
    cg_indices_type_t indices_type;
    /* Indices used to split fans and strips */
    int index_a, index_b;
};

struct _cg_path_tesselator_vertex_t {
    float x, y, s, t;
};

static void
_cg_path_tesselator_begin(GLenum type, cg_path_tesselator_t *tess)
{
    c_assert(type == GL_TRIANGLES || type == GL_TRIANGLE_FAN ||
             type == GL_TRIANGLE_STRIP);

    tess->primitive_type = type;
    tess->vertex_number = 0;
}

static cg_indices_type_t
_cg_path_tesselator_get_indices_type_for_size(int n_vertices)
{
    if (n_vertices <= 256)
        return CG_INDICES_TYPE_UNSIGNED_BYTE;
    else if (n_vertices <= 65536)
        return CG_INDICES_TYPE_UNSIGNED_SHORT;
    else
        return CG_INDICES_TYPE_UNSIGNED_INT;
}

static void
_cg_path_tesselator_allocate_indices_array(cg_path_tesselator_t *tess)
{
    switch (tess->indices_type) {
    case CG_INDICES_TYPE_UNSIGNED_BYTE:
        tess->indices = c_array_new(false, false, sizeof(uint8_t));
        break;

    case CG_INDICES_TYPE_UNSIGNED_SHORT:
        tess->indices = c_array_new(false, false, sizeof(uint16_t));
        break;

    case CG_INDICES_TYPE_UNSIGNED_INT:
        tess->indices = c_array_new(false, false, sizeof(uint32_t));
        break;
    }
}

static void
_cg_path_tesselator_add_index(cg_path_tesselator_t *tess,
                              int vertex_index)
{
    switch (tess->indices_type) {
    case CG_INDICES_TYPE_UNSIGNED_BYTE: {
        uint8_t val = vertex_index;
        c_array_append_val(tess->indices, val);
    } break;

    case CG_INDICES_TYPE_UNSIGNED_SHORT: {
        uint16_t val = vertex_index;
        c_array_append_val(tess->indices, val);
    } break;

    case CG_INDICES_TYPE_UNSIGNED_INT: {
        uint32_t val = vertex_index;
        c_array_append_val(tess->indices, val);
    } break;
    }
}

static void
_cg_path_tesselator_vertex(void *vertex_data,
                           cg_path_tesselator_t *tess)
{
    int vertex_index;

    vertex_index = GPOINTER_TO_INT(vertex_data);

    /* This tries to convert all of the primitives into GL_TRIANGLES
       with indices to share vertices */
    switch (tess->primitive_type) {
    case GL_TRIANGLES:
        /* Directly use the vertex */
        _cg_path_tesselator_add_index(tess, vertex_index);
        break;

    case GL_TRIANGLE_FAN:
        if (tess->vertex_number == 0)
            tess->index_a = vertex_index;
        else if (tess->vertex_number == 1)
            tess->index_b = vertex_index;
        else {
            /* Create a triangle with the first vertex, the previous
               vertex and this vertex */
            _cg_path_tesselator_add_index(tess, tess->index_a);
            _cg_path_tesselator_add_index(tess, tess->index_b);
            _cg_path_tesselator_add_index(tess, vertex_index);
            /* Next time we will use this vertex as the previous
               vertex */
            tess->index_b = vertex_index;
        }
        break;

    case GL_TRIANGLE_STRIP:
        if (tess->vertex_number == 0)
            tess->index_a = vertex_index;
        else if (tess->vertex_number == 1)
            tess->index_b = vertex_index;
        else {
            _cg_path_tesselator_add_index(tess, tess->index_a);
            _cg_path_tesselator_add_index(tess, tess->index_b);
            _cg_path_tesselator_add_index(tess, vertex_index);
            if (tess->vertex_number & 1)
                tess->index_b = vertex_index;
            else
                tess->index_a = vertex_index;
        }
        break;

    default:
        c_assert_not_reached();
    }

    tess->vertex_number++;
}

static void
_cg_path_tesselator_end(cg_path_tesselator_t *tess)
{
    tess->primitive_type = FALSE;
}

static void
_cg_path_tesselator_combine(double coords[3],
                            void *vertex_data[4],
                            float weight[4],
                            void **out_data,
                            cg_path_tesselator_t *tess)
{
    cg_path_tesselator_vertex_t *vertex;
    cg_indices_type_t new_indices_type;
    int i;

    /* Add a new vertex to the array */
    c_array_set_size(tess->vertices, tess->vertices->len + 1);
    vertex = &c_array_index(tess->vertices,
                            cg_path_tesselator_vertex_t,
                            tess->vertices->len - 1);
    /* The data is just the index to the vertex */
    *out_data = GINT_TO_POINTER(tess->vertices->len - 1);
    /* Set the coordinates of the new vertex */
    vertex->x = coords[0];
    vertex->y = coords[1];
    /* Generate the texture coordinates as the weighted average of the
       four incoming coordinates */
    vertex->s = 0.0f;
    vertex->t = 0.0f;
    for (i = 0; i < 4; i++) {
        cg_path_tesselator_vertex_t *old_vertex =
            &c_array_index(tess->vertices,
                           cg_path_tesselator_vertex_t,
                           GPOINTER_TO_INT(vertex_data[i]));
        vertex->s += old_vertex->s * weight[i];
        vertex->t += old_vertex->t * weight[i];
    }

    /* Check if we've reached the limit for the data type of our indices */
    new_indices_type =
        _cg_path_tesselator_get_indices_type_for_size(tess->vertices->len);
    if (new_indices_type != tess->indices_type) {
        cg_indices_type_t old_indices_type = new_indices_type;
        c_array_t *old_vertices = tess->indices;

        /* Copy the indices to an array of the new type */
        tess->indices_type = new_indices_type;
        _cg_path_tesselator_allocate_indices_array(tess);

        switch (old_indices_type) {
        case CG_INDICES_TYPE_UNSIGNED_BYTE:
            for (i = 0; i < old_vertices->len; i++)
                _cg_path_tesselator_add_index(
                    tess, c_array_index(old_vertices, uint8_t, i));
            break;

        case CG_INDICES_TYPE_UNSIGNED_SHORT:
            for (i = 0; i < old_vertices->len; i++)
                _cg_path_tesselator_add_index(
                    tess, c_array_index(old_vertices, uint16_t, i));
            break;

        case CG_INDICES_TYPE_UNSIGNED_INT:
            for (i = 0; i < old_vertices->len; i++)
                _cg_path_tesselator_add_index(
                    tess, c_array_index(old_vertices, uint32_t, i));
            break;
        }

        c_array_free(old_vertices, true);
    }
}

static void
_cg_path_build_fill_attribute_buffer(cg_path_t *path)
{
    cg_path_tesselator_t tess;
    unsigned int path_start = 0;
    cg_path_data_t *data = path->data;
    int i;

    /* If we've already got a vbo then we don't need to do anything */
    if (data->fill_attribute_buffer)
        return;

    tess.primitive_type = false;

    /* Generate a vertex for each point on the path */
    tess.vertices =
        c_array_new(false, false, sizeof(cg_path_tesselator_vertex_t));
    c_array_set_size(tess.vertices, data->path_nodes->len);
    for (i = 0; i < data->path_nodes->len; i++) {
        cg_path_node_t *node =
            &c_array_index(data->path_nodes, cg_path_node_t, i);
        cg_path_tesselator_vertex_t *vertex =
            &c_array_index(tess.vertices, cg_path_tesselator_vertex_t, i);

        vertex->x = node->x;
        vertex->y = node->y;

        /* Add texture coordinates so that a texture would be drawn to
           fit the bounding box of the path and then cropped by the
           path */
        if (data->path_nodes_min.x == data->path_nodes_max.x)
            vertex->s = 0.0f;
        else
            vertex->s = ((node->x - data->path_nodes_min.x) /
                         (data->path_nodes_max.x - data->path_nodes_min.x));
        if (data->path_nodes_min.y == data->path_nodes_max.y)
            vertex->t = 0.0f;
        else
            vertex->t = ((node->y - data->path_nodes_min.y) /
                         (data->path_nodes_max.y - data->path_nodes_min.y));
    }

    tess.indices_type =
        _cg_path_tesselator_get_indices_type_for_size(data->path_nodes->len);
    _cg_path_tesselator_allocate_indices_array(&tess);

    tess.glc_tess = gluNewTess();

    if (data->fill_rule == CG_PATH_FILL_RULE_EVEN_ODD)
        gluTessProperty(
            tess.glc_tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
    else
        gluTessProperty(
            tess.glc_tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);

    /* All vertices are on the xy-plane */
    gluTessNormal(tess.glc_tess, 0.0, 0.0, 1.0);

    gluTessCallback(
        tess.glc_tess, GLU_TESS_BEGIN_DATA, _cg_path_tesselator_begin);
    gluTessCallback(
        tess.glc_tess, GLU_TESS_VERTEX_DATA, _cg_path_tesselator_vertex);
    gluTessCallback(tess.glc_tess, GLU_TESS_END_DATA, _cg_path_tesselator_end);
    gluTessCallback(
        tess.glc_tess, GLU_TESS_COMBINE_DATA, _cg_path_tesselator_combine);

    gluTessBeginPolygon(tess.glc_tess, &tess);

    while (path_start < data->path_nodes->len) {
        cg_path_node_t *node =
            &c_array_index(data->path_nodes, cg_path_node_t, path_start);

        gluTessBeginContour(tess.glc_tess);

        for (i = 0; i < node->path_size; i++) {
            double vertex[3] = { node[i].x, node[i].y, 0.0 };
            gluTessVertex(
                tess.glc_tess, vertex, GINT_TO_POINTER(i + path_start));
        }

        gluTessEndContour(tess.glc_tess);

        path_start += node->path_size;
    }

    gluTessEndPolygon(tess.glc_tess);

    gluDeleteTess(tess.glc_tess);

    data->fill_attribute_buffer = cg_attribute_buffer_new(
        data->dev,
        sizeof(cg_path_tesselator_vertex_t) * tess.vertices->len,
        tess.vertices->data);
    c_array_free(tess.vertices, true);

    data->fill_attributes[0] =
        cg_attribute_new(data->fill_attribute_buffer,
                         "cg_position_in",
                         sizeof(cg_path_tesselator_vertex_t),
                         G_STRUCT_OFFSET(cg_path_tesselator_vertex_t, x),
                         2, /* n_components */
                         CG_ATTRIBUTE_TYPE_FLOAT);
    data->fill_attributes[1] =
        cg_attribute_new(data->fill_attribute_buffer,
                         "cg_tex_coord0_in",
                         sizeof(cg_path_tesselator_vertex_t),
                         G_STRUCT_OFFSET(cg_path_tesselator_vertex_t, s),
                         2, /* n_components */
                         CG_ATTRIBUTE_TYPE_FLOAT);

    data->fill_vbo_indices = cg_indices_new(data->dev,
                                            tess.indices_type,
                                            tess.indices->data,
                                            tess.indices->len);
    data->fill_vbo_n_indices = tess.indices->len;
    c_array_free(tess.indices, true);
}

static cg_primitive_t *
_cg_path_get_fill_primitive(cg_path_t *path)
{
    if (path->data->fill_primitive)
        return path->data->fill_primitive;

    _cg_path_build_fill_attribute_buffer(path);

    path->data->fill_primitive =
        cg_primitive_new_with_attributes(CG_VERTICES_MODE_TRIANGLES,
                                         path->data->fill_vbo_n_indices,
                                         path->data->fill_attributes,
                                         CG_PATH_N_ATTRIBUTES);
    cg_primitive_set_indices(path->data->fill_primitive,
                             path->data->fill_vbo_indices,
                             path->data->fill_vbo_n_indices);

    return path->data->fill_primitive;
}

static cg_clip_stack_t *
_cg_clip_stack_push_from_path(cg_clip_stack_t *stack,
                              cg_path_t *path,
                              cg_matrix_entry_t *modelview_entry,
                              cg_matrix_entry_t *projection_entry,
                              const float *viewport)
{
    float x_1, y_1, x_2, y_2;

    _cg_path_get_bounds(path, &x_1, &y_1, &x_2, &y_2);

    /* If the path is a simple rectangle then we can divert to pushing a
       rectangle clip instead which usually won't involve the stencil
       buffer */
    if (_cg_path_is_rectangle(path))
        return _cg_clip_stack_push_rectangle(stack,
                                             x_1,
                                             y_1,
                                             x_2,
                                             y_2,
                                             modelview_entry,
                                             projection_entry,
                                             viewport);
    else {
        cg_primitive_t *primitive = _cg_path_get_fill_primitive(path);

        return _cg_clip_stack_push_primitive(stack,
                                             primitive,
                                             x_1,
                                             y_1,
                                             x_2,
                                             y_2,
                                             modelview_entry,
                                             projection_entry,
                                             viewport);
    }
}

void
cg_framebuffer_push_path_clip(cg_framebuffer_t *framebuffer,
                              cg_path_t *path)
{
    cg_matrix_entry_t *modelview_entry =
        _cg_framebuffer_get_modelview_entry(framebuffer);
    cg_matrix_entry_t *projection_entry =
        _cg_framebuffer_get_projection_entry(framebuffer);
    /* XXX: It would be nicer if we stored the private viewport as a
     * vec4 so we could avoid this redundant copy. */
    float viewport[] = { framebuffer->viewport_x,
                         framebuffer->viewport_y,
                         framebuffer->viewport_width,
                         framebuffer->viewport_height };

    framebuffer->clip_stack =
        _cg_clip_stack_push_from_path(framebuffer->clip_stack,
                                      path,
                                      modelview_entry,
                                      projection_entry,
                                      viewport);

    if (framebuffer->dev->current_draw_buffer == framebuffer)
        framebuffer->dev->current_draw_buffer_changes |=
            CG_FRAMEBUFFER_STATE_CLIP;
}

static void
_cg_path_build_stroke_attribute_buffer(cg_path_t *path)
{
    cg_path_data_t *data = path->data;
    cg_buffer_t *buffer;
    unsigned int n_attributes = 0;
    unsigned int path_start;
    cg_path_node_t *node;
    float_vec2_t *buffer_p;
    unsigned int i;

    /* If we've already got a cached vbo then we don't need to do anything */
    if (data->stroke_attribute_buffer)
        return;

    data->stroke_attribute_buffer = cg_attribute_buffer_new_with_size(
        data->dev, data->path_nodes->len * sizeof(float_vec2_t));

    buffer = CG_BUFFER(data->stroke_attribute_buffer);
    buffer_p = _cg_buffer_map_for_fill_or_fallback(buffer);

    /* Copy the vertices in and count the number of sub paths. Each sub
       path will form a separate attribute so we can paint the disjoint
       line strips */
    for (path_start = 0; path_start < data->path_nodes->len;
         path_start += node->path_size) {
        node = &c_array_index(data->path_nodes, cg_path_node_t, path_start);

        for (i = 0; i < node->path_size; i++) {
            buffer_p[path_start + i].x = node[i].x;
            buffer_p[path_start + i].y = node[i].y;
        }

        n_attributes++;
    }

    _cg_buffer_unmap_for_fill_or_fallback(buffer);

    data->stroke_attributes = c_new(cg_attribute_t *, n_attributes);

    /* Now we can loop the sub paths again to create the attributes */
    for (i = 0, path_start = 0; path_start < data->path_nodes->len;
         i++, path_start += node->path_size) {
        node = &c_array_index(data->path_nodes, cg_path_node_t, path_start);

        data->stroke_attributes[i] =
            cg_attribute_new(data->stroke_attribute_buffer,
                             "cg_position_in",
                             sizeof(float_vec2_t),
                             path_start * sizeof(float_vec2_t),
                             2, /* n_components */
                             CG_ATTRIBUTE_TYPE_FLOAT);
    }

    data->stroke_n_attributes = n_attributes;
}
