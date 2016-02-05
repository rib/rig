/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010,2011 Intel Corporation.
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
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-device-private.h"
#include "cg-pipeline-private.h"
#include "cg-pipeline-layer-private.h"
#include "cg-node-private.h"

#include <clib.h>

typedef struct {
    int parent_id;
    int *node_id_ptr;
    c_string_t *graph;
    int indent;
} print_debug_state_t;

static bool
dump_layer_cb(cg_node_t *node, void *user_data)
{
    cg_pipeline_layer_t *layer = CG_PIPELINE_LAYER(node);
    print_debug_state_t *state = user_data;
    int layer_id = *state->node_id_ptr;
    print_debug_state_t state_out;
    c_string_t *changes_label;
    bool changes = false;

    if (state->parent_id >= 0)
        c_string_append_printf(state->graph,
                               "%*slayer%p -> layer%p;\n",
                               state->indent,
                               "",
                               layer->_parent.parent,
                               layer);

    c_string_append_printf(state->graph,
                           "%*slayer%p [label=\"layer=0x%p\\n"
                           "ref count=%d\" "
                           "color=\"blue\"];\n",
                           state->indent,
                           "",
                           layer,
                           layer,
                           CG_OBJECT(layer)->ref_count);

    changes_label = c_string_new("");
    c_string_append_printf(changes_label,
                           "%*slayer%p -> layer_state%d [weight=100];\n"
                           "%*slayer_state%d [shape=box label=\"",
                           state->indent,
                           "",
                           layer,
                           layer_id,
                           state->indent,
                           "",
                           layer_id);

    if (layer->differences & CG_PIPELINE_LAYER_STATE_UNIT) {
        changes = true;
        c_string_append_printf(
            changes_label, "\\lunit=%u\\n", layer->unit_index);
    }

    if (layer->differences & CG_PIPELINE_LAYER_STATE_TEXTURE_DATA) {
        changes = true;
        c_string_append_printf(
            changes_label, "\\ltexture=%p\\n", layer->texture);
    }

    if (changes) {
        c_string_append_printf(changes_label, "\"];\n");
        c_string_append(state->graph, changes_label->str);
        c_string_free(changes_label, true);
    }

    state_out.parent_id = layer_id;

    state_out.node_id_ptr = state->node_id_ptr;
    (*state_out.node_id_ptr)++;

    state_out.graph = state->graph;
    state_out.indent = state->indent + 2;

    _cg_pipeline_node_foreach_child(CG_NODE(layer), dump_layer_cb, &state_out);

    return true;
}

static bool
dump_layer_ref_cb(cg_pipeline_layer_t *layer, void *data)
{
    print_debug_state_t *state = data;
    int pipeline_id = *state->node_id_ptr;

    c_string_append_printf(state->graph,
                           "%*spipeline_state%d -> layer%p;\n",
                           state->indent,
                           "",
                           pipeline_id,
                           layer);

    return true;
}

static bool
dump_pipeline_cb(cg_node_t *node, void *user_data)
{
    cg_pipeline_t *pipeline = CG_PIPELINE(node);
    print_debug_state_t *state = user_data;
    int pipeline_id = *state->node_id_ptr;
    print_debug_state_t state_out;
    c_string_t *changes_label;
    bool changes = false;
    bool layers = false;

    if (state->parent_id >= 0)
        c_string_append_printf(state->graph,
                               "%*spipeline%d -> pipeline%d;\n",
                               state->indent,
                               "",
                               state->parent_id,
                               pipeline_id);

    c_string_append_printf(state->graph,
                           "%*spipeline%d [label=\"pipeline=0x%p\\n"
                           "ref count=%d\\n"
                           "breadcrumb=\\\"%s\\\"\" color=\"red\"];\n",
                           state->indent,
                           "",
                           pipeline_id,
                           pipeline,
                           CG_OBJECT(pipeline)->ref_count,
                           pipeline->has_static_breadcrumb ?
#ifdef CG_DEBUG_ENABLED
                           pipeline->static_breadcrumb
                           : "NULL"
#else
                           "NULL"
#endif
                           );

    changes_label = c_string_new("");
    c_string_append_printf(changes_label,
                           "%*spipeline%d -> pipeline_state%d [weight=100];\n"
                           "%*spipeline_state%d [shape=box label=\"",
                           state->indent,
                           "",
                           pipeline_id,
                           pipeline_id,
                           state->indent,
                           "",
                           pipeline_id);

    if (pipeline->differences & CG_PIPELINE_STATE_COLOR) {
        changes = true;
        c_string_append_printf(changes_label,
                               "\\lcolor=0x%02X%02X%02X%02X\\n",
                               cg_color_get_red_byte(&pipeline->color),
                               cg_color_get_green_byte(&pipeline->color),
                               cg_color_get_blue_byte(&pipeline->color),
                               cg_color_get_alpha_byte(&pipeline->color));
    }

    if (pipeline->differences & CG_PIPELINE_STATE_BLEND) {
        const char *blend_enable_name;

        changes = true;

        switch (pipeline->blend_enable) {
        case CG_PIPELINE_BLEND_ENABLE_AUTOMATIC:
            blend_enable_name = "AUTO";
            break;
        case CG_PIPELINE_BLEND_ENABLE_ENABLED:
            blend_enable_name = "ENABLED";
            break;
        case CG_PIPELINE_BLEND_ENABLE_DISABLED:
            blend_enable_name = "DISABLED";
            break;
        default:
            blend_enable_name = "UNKNOWN";
        }
        c_string_append_printf(
            changes_label, "\\lblend=%s\\n", blend_enable_name);
    }

    if (pipeline->differences & CG_PIPELINE_STATE_LAYERS) {
        changes = true;
        layers = true;
        c_string_append_printf(
            changes_label, "\\ln_layers=%d\\n", pipeline->n_layers);
    }

    if (changes) {
        c_string_append_printf(changes_label, "\"];\n");
        c_string_append(state->graph, changes_label->str);
        c_string_free(changes_label, true);
    }

    if (layers) {
        c_llist_foreach(pipeline->layer_differences,
                       (c_iter_func_t)dump_layer_ref_cb, state);
    }

    state_out.parent_id = pipeline_id;

    state_out.node_id_ptr = state->node_id_ptr;
    (*state_out.node_id_ptr)++;

    state_out.graph = state->graph;
    state_out.indent = state->indent + 2;

    _cg_pipeline_node_foreach_child(
        CG_NODE(pipeline), dump_pipeline_cb, &state_out);

    return true;
}

/* This function is just here to be called from GDB so we don't really
   want to put a declaration in a header and we just add it here to
   avoid a warning */
void _cg_debug_dump_pipelines_dot_file(cg_device_t *dev,
                                       const char *filename);

void
_cg_debug_dump_pipelines_dot_file(cg_device_t *dev,
                                  const char *filename)
{
    c_string_t *graph;
    print_debug_state_t layer_state;
    print_debug_state_t pipeline_state;
    int layer_id = 0;
    int pipeline_id = 0;

    if (!dev->default_pipeline)
        return;

    graph = c_string_new("");
    c_string_append_printf(graph, "digraph {\n");

    layer_state.graph = graph;
    layer_state.parent_id = -1;
    layer_state.node_id_ptr = &layer_id;
    layer_state.indent = 0;
    dump_layer_cb((cg_node_t *)dev->default_layer_0, &layer_state);

    pipeline_state.graph = graph;
    pipeline_state.parent_id = -1;
    pipeline_state.node_id_ptr = &pipeline_id;
    pipeline_state.indent = 0;
    dump_pipeline_cb((cg_node_t *)dev->default_pipeline, &pipeline_state);

    c_string_append_printf(graph, "}\n");

    if (filename)
        c_file_set_contents(filename, graph->str, -1, NULL);
    else
        c_print("%s", graph->str);

    c_string_free(graph, true);
}
