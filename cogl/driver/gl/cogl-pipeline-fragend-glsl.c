/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010,2013 Intel Corporation.
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
 *   Neil Roberts <neil@linux.intel.com>
 */

#include "config.h"

#include <string.h>

#include <clib.h>

#include "cogl-device-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-layer-private.h"
#include "cogl-blend-string.h"
#include "cogl-snippet-private.h"

#ifdef CG_PIPELINE_FRAGEND_GLSL

#include "cogl-device-private.h"
#include "cogl-object-private.h"
#include "cogl-pipeline-cache.h"
#include "cogl-pipeline-fragend-glsl-private.h"
#include "cogl-glsl-shader-private.h"

/*
 * GL/GLES compatability defines for pipeline thingies:
 */

/* This might not be defined on GLES */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif

const cg_pipeline_fragend_t _cg_pipeline_glsl_backend;

typedef struct _unit_state_t {
    unsigned int sampled : 1;
    unsigned int combine_constant_used : 1;
} unit_state_t;

typedef struct _layer_data_t {
    c_list_t link;

    /* Layer index for the for the previous layer. This isn't
       necessarily the same as this layer's index - 1 because the
       indices can have gaps. If this is the first layer then it will be
       -1 */
    int previous_layer_index;

    cg_pipeline_layer_t *layer;
} layer_data_t;

typedef struct {
    int ref_count;

    GLuint gl_shader;
    c_string_t *header, *source;
    unit_state_t *unit_state;

    /* List of layers that we haven't generated code for yet. These are
       in reverse order. As soon as we're about to generate code for
       layer we'll remove it from the list so we don't generate it
       again */
    c_list_t layers;

    cg_pipeline_cache_entry_t *cache_entry;
} cg_pipeline_shader_state_t;

static cg_user_data_key_t shader_state_key;

static void ensure_layer_generated(cg_pipeline_shader_state_t *shader_state,
                                   cg_pipeline_t *pipeline,
                                   int layer_num);

static cg_pipeline_shader_state_t *
shader_state_new(int n_layers, cg_pipeline_cache_entry_t *cache_entry)
{
    cg_pipeline_shader_state_t *shader_state;

    shader_state = c_slice_new0(cg_pipeline_shader_state_t);
    shader_state->ref_count = 1;
    shader_state->unit_state = c_new0(unit_state_t, n_layers);
    shader_state->cache_entry = cache_entry;

    return shader_state;
}

static cg_pipeline_shader_state_t *
get_shader_state(cg_pipeline_t *pipeline)
{
    return cg_object_get_user_data(CG_OBJECT(pipeline), &shader_state_key);
}

static void
destroy_shader_state(void *user_data, void *instance)
{
    cg_pipeline_shader_state_t *shader_state = user_data;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    if (shader_state->cache_entry &&
        shader_state->cache_entry->pipeline != instance)
        shader_state->cache_entry->usage_count--;

    if (--shader_state->ref_count == 0) {
        if (shader_state->gl_shader)
            GE(dev, glDeleteShader(shader_state->gl_shader));

        c_free(shader_state->unit_state);

        c_slice_free(cg_pipeline_shader_state_t, shader_state);
    }
}

static void
set_shader_state(cg_pipeline_t *pipeline,
                 cg_pipeline_shader_state_t *shader_state)
{
    if (shader_state) {
        shader_state->ref_count++;

        /* If we're not setting the state on the template pipeline then
         * mark it as a usage of the pipeline cache entry */
        if (shader_state->cache_entry &&
            shader_state->cache_entry->pipeline != pipeline)
            shader_state->cache_entry->usage_count++;
    }

    _cg_object_set_user_data(CG_OBJECT(pipeline),
                             &shader_state_key,
                             shader_state,
                             destroy_shader_state);
}

static void
dirty_shader_state(cg_pipeline_t *pipeline)
{
    cg_object_set_user_data(CG_OBJECT(pipeline), &shader_state_key, NULL, NULL);
}

GLuint
_cg_pipeline_fragend_glsl_get_shader(cg_pipeline_t *pipeline)
{
    cg_pipeline_shader_state_t *shader_state = get_shader_state(pipeline);

    if (shader_state)
        return shader_state->gl_shader;
    else
        return 0;
}

static cg_pipeline_snippet_list_t *
get_fragment_snippets(cg_pipeline_t *pipeline)
{
    pipeline = _cg_pipeline_get_authority(pipeline,
                                          CG_PIPELINE_STATE_FRAGMENT_SNIPPETS);

    return &pipeline->big_state->fragment_snippets;
}

static cg_pipeline_snippet_list_t *
get_layer_fragment_snippets(cg_pipeline_layer_t *layer)
{
    unsigned long state = CG_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS;
    layer = _cg_pipeline_layer_get_authority(layer, state);

    return &layer->big_state->fragment_snippets;
}

static bool
has_replace_hook(cg_pipeline_layer_t *layer, cg_snippet_hook_t hook)
{
    c_llist_t *l;

    for (l = get_layer_fragment_snippets(layer)->entries; l; l = l->next) {
        cg_snippet_t *snippet = l->data;

        if (snippet->hook == hook && snippet->replace)
            return true;
    }

    return false;
}

static bool
add_layer_declaration_cb(cg_pipeline_layer_t *layer,
                         void *user_data)
{
    cg_pipeline_shader_state_t *shader_state = user_data;
    cg_texture_type_t texture_type = _cg_pipeline_layer_get_texture_type(layer);
    const char *target_string;

    _cg_gl_util_get_texture_target_string(texture_type, &target_string, NULL);

    c_string_append_printf(shader_state->header,
                           "in vec4 _cg_tex_coord%i;\n"
                           "#define cg_tex_coord%i_in _cg_tex_coord%i\n"
                           "uniform sampler%s cg_sampler%i;\n",
                           layer->index,
                           layer->index,
                           layer->index,
                           target_string,
                           layer->index);

    return true;
}

static void
add_layer_declarations(cg_pipeline_t *pipeline,
                       cg_pipeline_shader_state_t *shader_state)
{
    /* We always emit sampler uniforms in case there will be custom
     * layer snippets that want to sample arbitrary layers. */

    _cg_pipeline_foreach_layer_internal(
        pipeline, add_layer_declaration_cb, shader_state);
}

static void
add_global_declarations(cg_pipeline_t *pipeline,
                        cg_pipeline_shader_state_t *shader_state)
{
    cg_snippet_hook_t hook = CG_SNIPPET_HOOK_FRAGMENT_GLOBALS;
    cg_pipeline_snippet_list_t *snippets = get_fragment_snippets(pipeline);

    /* Add the global data hooks. All of the code in these snippets is
     * always added and only the declarations data is used */

    _cg_pipeline_snippet_generate_declarations(
        shader_state->header, hook, snippets);
}

static void
_cg_pipeline_fragend_glsl_start(cg_pipeline_t *pipeline,
                                int n_layers,
                                unsigned long pipelines_difference)
{
    cg_pipeline_shader_state_t *shader_state;
    cg_pipeline_t *authority;
    cg_pipeline_cache_entry_t *cache_entry = NULL;
    int i;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    /* Now lookup our glsl backend private state */
    shader_state = get_shader_state(pipeline);

    if (shader_state == NULL) {
        /* If we don't have an associated glsl shader yet then find the
         * glsl-authority (the oldest ancestor whose state will result in
         * the same shader being generated as for this pipeline).
         *
         * We always make sure to associate new shader with the
         * glsl-authority to maximize the chance that other pipelines can
         * share it.
         */
        authority = _cg_pipeline_find_equivalent_parent(
            pipeline,
            _cg_pipeline_get_state_for_fragment_codegen(dev) &
            ~CG_PIPELINE_STATE_LAYERS,
            _cg_pipeline_get_layer_state_for_fragment_codegen(dev));

        shader_state = get_shader_state(authority);

        /* If we don't have an existing program associated with the
         * glsl-authority then start generating code for a new shader...
         */
        if (shader_state == NULL) {
            /* Check if there is already a similar cached pipeline whose
               shader state we can share */
            if (C_LIKELY(
                    !(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_PROGRAM_CACHES)))) {
                cache_entry = _cg_pipeline_cache_get_fragment_template(
                    dev->pipeline_cache, authority);

                shader_state = get_shader_state(cache_entry->pipeline);
            }

            if (shader_state)
                shader_state->ref_count++;
            else
                shader_state = shader_state_new(n_layers, cache_entry);

            set_shader_state(authority, shader_state);

            shader_state->ref_count--;

            if (cache_entry)
                set_shader_state(cache_entry->pipeline, shader_state);
        }

        /* If the pipeline isn't actually its own glsl-authority
         * then take a reference to the program state associated
         * with the glsl-authority... */
        if (authority != pipeline)
            set_shader_state(pipeline, shader_state);
    }

    if (shader_state->gl_shader)
        return;

    /* If we make it here then we have a glsl_shader_state struct
       without a gl_shader because this is the first time we've
       encountered it. */

    /* We reuse two grow-only c_string_ts for code-gen. One string
       contains the uniform and attribute declarations while the
       other contains the main function. We need two strings
       because we need to dynamically declare attributes as the
       add_layer callback is invoked */
    c_string_set_size(dev->codegen_header_buffer, 0);
    c_string_set_size(dev->codegen_source_buffer, 0);
    shader_state->header = dev->codegen_header_buffer;
    shader_state->source = dev->codegen_source_buffer;
    c_list_init(&shader_state->layers);

    add_layer_declarations(pipeline, shader_state);
    add_global_declarations(pipeline, shader_state);

    c_string_append(shader_state->source,
                    "void\n"
                    "cg_generated_source ()\n"
                    "{\n");

    for (i = 0; i < n_layers; i++) {
        shader_state->unit_state[i].sampled = false;
        shader_state->unit_state[i].combine_constant_used = false;
    }
}

static void
add_constant_lookup(cg_pipeline_shader_state_t *shader_state,
                    cg_pipeline_t *pipeline,
                    cg_pipeline_layer_t *layer,
                    const char *swizzle)
{
    c_string_append_printf(shader_state->header,
                           "_cg_layer_constant_%i.%s",
                           layer->index,
                           swizzle);
}

static void
ensure_texture_lookup_generated(cg_pipeline_shader_state_t *shader_state,
                                cg_pipeline_t *pipeline,
                                cg_pipeline_layer_t *layer)
{
    int unit_index = _cg_pipeline_layer_get_unit_index(layer);
    cg_pipeline_snippet_data_t snippet_data;
    cg_texture_type_t texture_type;
    const char *target_string, *tex_coord_swizzle;

    _CG_GET_DEVICE(dev, NO_RETVAL);

    if (shader_state->unit_state[unit_index].sampled)
        return;

    texture_type = _cg_pipeline_layer_get_texture_type(layer);
    _cg_gl_util_get_texture_target_string(
        texture_type, &target_string, &tex_coord_swizzle);

    shader_state->unit_state[unit_index].sampled = true;

    c_string_append_printf(
        shader_state->header, "vec4 cg_texel%i;\n", layer->index);

    c_string_append_printf(shader_state->source,
                           "  cg_texel%i = cg_texture_lookup%i ("
                           "cg_sampler%i, ",
                           layer->index,
                           layer->index,
                           layer->index);

    if (cg_pipeline_get_layer_point_sprite_coords_enabled(pipeline,
                                                          layer->index))
        c_string_append_printf(shader_state->source,
                               "vec4 (cg_point_coord, 0.0, 1.0)");
    else
        c_string_append_printf(
            shader_state->source, "cg_tex_coord%i_in", layer->index);

    c_string_append(shader_state->source, ");\n");

    /* There's no need to generate the real texture lookup if it's going
       to be replaced */
    if (!has_replace_hook(layer, CG_SNIPPET_HOOK_TEXTURE_LOOKUP)) {
        c_string_append_printf(shader_state->header,
                               "vec4\n"
                               "cg_real_texture_lookup%i (sampler%s tex,\n"
                               "                            vec4 coords)\n"
                               "{\n"
                               "  return ",
                               layer->index,
                               target_string);

        if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_TEXTURING)))
            c_string_append(shader_state->header,
                            "vec4 (1.0, 1.0, 1.0, 1.0);\n");
        else {
            if (dev->glsl_version_to_use >= 130) {
                c_string_append_printf(shader_state->header,
                                       "texture (tex, coords.%s);\n",
                                       tex_coord_swizzle);
            } else {
                c_string_append_printf(shader_state->header,
                                       "texture%s (tex, coords.%s);\n",
                                       target_string,
                                       tex_coord_swizzle);
            }
        }

        c_string_append(shader_state->header, "}\n");
    }

    /* Wrap the texture lookup in any snippets that have been hooked */
    memset(&snippet_data, 0, sizeof(snippet_data));
    snippet_data.snippets = get_layer_fragment_snippets(layer);
    snippet_data.hook = CG_SNIPPET_HOOK_TEXTURE_LOOKUP;
    snippet_data.chain_function =
        c_strdup_printf("cg_real_texture_lookup%i", layer->index);
    snippet_data.final_name =
        c_strdup_printf("cg_texture_lookup%i", layer->index);
    snippet_data.function_prefix =
        c_strdup_printf("cg_texture_lookup_hook%i", layer->index);
    snippet_data.return_type = "vec4";
    snippet_data.return_variable = "cg_texel";
    snippet_data.arguments = "cg_sampler, cg_tex_coord";
    snippet_data.argument_declarations = c_strdup_printf(
        "sampler%s cg_sampler, vec4 cg_tex_coord", target_string);
    snippet_data.source_buf = shader_state->header;

    _cg_pipeline_snippet_generate_code(&snippet_data);

    c_free((char *)snippet_data.chain_function);
    c_free((char *)snippet_data.final_name);
    c_free((char *)snippet_data.function_prefix);
    c_free((char *)snippet_data.argument_declarations);
}

static void
add_arg(cg_pipeline_shader_state_t *shader_state,
        cg_pipeline_t *pipeline,
        cg_pipeline_layer_t *layer,
        int previous_layer_index,
        cg_pipeline_combine_source_t src,
        cg_pipeline_combine_op_t operand,
        const char *swizzle)
{
    c_string_t *shader_source = shader_state->header;
    char alpha_swizzle[5] = "aaaa";

    c_string_append_c(shader_source, '(');

    if (operand == CG_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_COLOR ||
        operand == CG_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_ALPHA)
        c_string_append_printf(
            shader_source, "vec4(1.0, 1.0, 1.0, 1.0).%s - ", swizzle);

    /* If the operand is reading from the alpha then replace the swizzle
       with the same number of copies of the alpha */
    if (operand == CG_PIPELINE_COMBINE_OP_SRC_ALPHA ||
        operand == CG_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_ALPHA) {
        alpha_swizzle[strlen(swizzle)] = '\0';
        swizzle = alpha_swizzle;
    }

    switch (src) {
    case CG_PIPELINE_COMBINE_SOURCE_TEXTURE:
        c_string_append_printf(
            shader_source, "cg_texel%i.%s", layer->index, swizzle);
        break;

    case CG_PIPELINE_COMBINE_SOURCE_CONSTANT:
        add_constant_lookup(shader_state, pipeline, layer, swizzle);
        break;

    case CG_PIPELINE_COMBINE_SOURCE_PREVIOUS:
        if (previous_layer_index >= 0) {
            c_string_append_printf(
                shader_source, "cg_layer%i.%s", previous_layer_index, swizzle);
            break;
        }
    /* flow through */
    case CG_PIPELINE_COMBINE_SOURCE_PRIMARY_COLOR:
        c_string_append_printf(shader_source, "cg_color_in.%s", swizzle);
        break;

    default: {
        int layer_num = src - CG_PIPELINE_COMBINE_SOURCE_TEXTURE0;
        cg_pipeline_get_layer_flags_t flags = CG_PIPELINE_GET_LAYER_NO_CREATE;
        cg_pipeline_layer_t *other_layer =
            _cg_pipeline_get_layer_with_flags(pipeline, layer_num, flags);

        if (other_layer == NULL) {
            static bool warning_seen = false;
            if (!warning_seen) {
                c_warning("The application is trying to use a texture "
                          "combine with a layer number that does not exist");
                warning_seen = true;
            }
            c_string_append_printf(
                shader_source, "vec4 (1.0, 1.0, 1.0, 1.0).%s", swizzle);
        } else
            c_string_append_printf(
                shader_source, "cg_texel%i.%s", other_layer->index, swizzle);
    } break;
    }

    c_string_append_c(shader_source, ')');
}

static void
ensure_arg_generated(cg_pipeline_t *pipeline,
                     cg_pipeline_layer_t *layer,
                     int previous_layer_index,
                     cg_pipeline_combine_source_t src)
{
    cg_pipeline_shader_state_t *shader_state = get_shader_state(pipeline);

    switch (src) {
    case CG_PIPELINE_COMBINE_SOURCE_PRIMARY_COLOR:
        /* This doesn't involve any other layers */
        break;

    case CG_PIPELINE_COMBINE_SOURCE_CONSTANT: {
        int unit_index = _cg_pipeline_layer_get_unit_index(layer);
        /* Create a sampler uniform for this layer if we haven't already */
        if (!shader_state->unit_state[unit_index].combine_constant_used) {
            c_string_append_printf(shader_state->header,
                                   "uniform vec4 _cg_layer_constant_%i;\n",
                                   layer->index);
            shader_state->unit_state[unit_index].combine_constant_used = true;
        }
    } break;

    case CG_PIPELINE_COMBINE_SOURCE_PREVIOUS:
        if (previous_layer_index >= 0)
            ensure_layer_generated(
                shader_state, pipeline, previous_layer_index);
        break;

    case CG_PIPELINE_COMBINE_SOURCE_TEXTURE:
        ensure_texture_lookup_generated(shader_state, pipeline, layer);
        break;

    default:
        if (src >= CG_PIPELINE_COMBINE_SOURCE_TEXTURE0) {
            int layer_num = src - CG_PIPELINE_COMBINE_SOURCE_TEXTURE0;
            cg_pipeline_get_layer_flags_t flags =
                CG_PIPELINE_GET_LAYER_NO_CREATE;
            cg_pipeline_layer_t *other_layer =
                _cg_pipeline_get_layer_with_flags(pipeline, layer_num, flags);

            if (other_layer)
                ensure_texture_lookup_generated(
                    shader_state, pipeline, other_layer);
        }
        break;
    }
}

static void
ensure_args_for_func(cg_pipeline_t *pipeline,
                     cg_pipeline_layer_t *layer,
                     int previous_layer_index,
                     cg_pipeline_combine_func_t function,
                     cg_pipeline_combine_source_t *src)
{
    int n_args = _cg_get_n_args_for_combine_func(function);
    int i;

    for (i = 0; i < n_args; i++)
        ensure_arg_generated(pipeline, layer, previous_layer_index, src[i]);
}

static void
append_masked_combine(cg_pipeline_t *pipeline,
                      cg_pipeline_layer_t *layer,
                      int previous_layer_index,
                      const char *swizzle,
                      cg_pipeline_combine_func_t function,
                      cg_pipeline_combine_source_t *src,
                      cg_pipeline_combine_op_t *op)
{
    cg_pipeline_shader_state_t *shader_state = get_shader_state(pipeline);
    c_string_t *shader_source = shader_state->header;

    c_string_append_printf(shader_state->header, "  cg_layer.%s = ", swizzle);

    switch (function) {
    case CG_PIPELINE_COMBINE_FUNC_REPLACE:
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[0],
                op[0],
                swizzle);
        break;

    case CG_PIPELINE_COMBINE_FUNC_MODULATE:
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[0],
                op[0],
                swizzle);
        c_string_append(shader_source, " * ");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[1],
                op[1],
                swizzle);
        break;

    case CG_PIPELINE_COMBINE_FUNC_ADD:
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[0],
                op[0],
                swizzle);
        c_string_append(shader_source, " + ");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[1],
                op[1],
                swizzle);
        break;

    case CG_PIPELINE_COMBINE_FUNC_ADD_SIGNED:
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[0],
                op[0],
                swizzle);
        c_string_append(shader_source, " + ");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[1],
                op[1],
                swizzle);
        c_string_append_printf(
            shader_source, " - vec4(0.5, 0.5, 0.5, 0.5).%s", swizzle);
        break;

    case CG_PIPELINE_COMBINE_FUNC_SUBTRACT:
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[0],
                op[0],
                swizzle);
        c_string_append(shader_source, " - ");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[1],
                op[1],
                swizzle);
        break;

    case CG_PIPELINE_COMBINE_FUNC_INTERPOLATE:
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[0],
                op[0],
                swizzle);
        c_string_append(shader_source, " * ");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[2],
                op[2],
                swizzle);
        c_string_append(shader_source, " + ");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[1],
                op[1],
                swizzle);
        c_string_append_printf(
            shader_source, " * (vec4(1.0, 1.0, 1.0, 1.0).%s - ", swizzle);
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[2],
                op[2],
                swizzle);
        c_string_append_c(shader_source, ')');
        break;

    case CG_PIPELINE_COMBINE_FUNC_DOT3_RGB:
    case CG_PIPELINE_COMBINE_FUNC_DOT3_RGBA:
        c_string_append(shader_source, "vec4(4.0 * ((");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[0],
                op[0],
                "r");
        c_string_append(shader_source, " - 0.5) * (");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[1],
                op[1],
                "r");
        c_string_append(shader_source, " - 0.5) + (");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[0],
                op[0],
                "g");
        c_string_append(shader_source, " - 0.5) * (");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[1],
                op[1],
                "g");
        c_string_append(shader_source, " - 0.5) + (");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[0],
                op[0],
                "b");
        c_string_append(shader_source, " - 0.5) * (");
        add_arg(shader_state,
                pipeline,
                layer,
                previous_layer_index,
                src[1],
                op[1],
                "b");
        c_string_append_printf(shader_source, " - 0.5))).%s", swizzle);
        break;
    }

    c_string_append_printf(shader_source, ";\n");
}

static void
generate_layer(cg_pipeline_shader_state_t *shader_state,
               cg_pipeline_t *pipeline,
               layer_data_t *layer_data)
{
    cg_pipeline_layer_t *combine_authority;
    cg_pipeline_layer_big_state_t *big_state;
    cg_pipeline_layer_t *layer = layer_data->layer;
    int layer_index = layer->index;
    cg_pipeline_snippet_data_t snippet_data;

    /* Remove the layer from the list so we don't generate it again */
    c_list_remove(&layer_data->link);

    combine_authority = _cg_pipeline_layer_get_authority(
        layer, CG_PIPELINE_LAYER_STATE_COMBINE);
    big_state = combine_authority->big_state;

    /* Make a global variable for the result of the layer code */
    c_string_append_printf(
        shader_state->header, "vec4 cg_layer%i;\n", layer_index);

    /* Skip the layer generation if there is a snippet that replaces the
       default layer code. This is important because generating this
       code may cause the code for other layers to be generated and
       stored in the global variable. If this code isn't actually used
       then the global variables would be uninitialised and they may be
       used from other layers */
    if (!has_replace_hook(layer, CG_SNIPPET_HOOK_LAYER_FRAGMENT)) {
        ensure_args_for_func(pipeline,
                             layer,
                             layer_data->previous_layer_index,
                             big_state->texture_combine_rgb_func,
                             big_state->texture_combine_rgb_src);
        ensure_args_for_func(pipeline,
                             layer,
                             layer_data->previous_layer_index,
                             big_state->texture_combine_alpha_func,
                             big_state->texture_combine_alpha_src);

        c_string_append_printf(shader_state->header,
                               "vec4\n"
                               "cg_real_generate_layer%i ()\n"
                               "{\n"
                               "  vec4 cg_layer;\n",
                               layer_index);

        if (!_cg_pipeline_layer_needs_combine_separate(combine_authority) ||
            /* GL_DOT3_RGBA Is a bit weird as a GL_COMBINE_RGB function
             * since if you use it, it overrides your ALPHA function...
             */
            big_state->texture_combine_rgb_func ==
            CG_PIPELINE_COMBINE_FUNC_DOT3_RGBA)
            append_masked_combine(pipeline,
                                  layer,
                                  layer_data->previous_layer_index,
                                  "rgba",
                                  big_state->texture_combine_rgb_func,
                                  big_state->texture_combine_rgb_src,
                                  big_state->texture_combine_rgb_op);
        else {
            append_masked_combine(pipeline,
                                  layer,
                                  layer_data->previous_layer_index,
                                  "rgb",
                                  big_state->texture_combine_rgb_func,
                                  big_state->texture_combine_rgb_src,
                                  big_state->texture_combine_rgb_op);
            append_masked_combine(pipeline,
                                  layer,
                                  layer_data->previous_layer_index,
                                  "a",
                                  big_state->texture_combine_alpha_func,
                                  big_state->texture_combine_alpha_src,
                                  big_state->texture_combine_alpha_op);
        }

        c_string_append(shader_state->header,
                        "  return cg_layer;\n"
                        "}\n");
    }

    /* Wrap the layer code in any snippets that have been hooked */
    memset(&snippet_data, 0, sizeof(snippet_data));
    snippet_data.snippets = get_layer_fragment_snippets(layer);
    snippet_data.hook = CG_SNIPPET_HOOK_LAYER_FRAGMENT;
    snippet_data.chain_function =
        c_strdup_printf("cg_real_generate_layer%i", layer_index);
    snippet_data.final_name =
        c_strdup_printf("cg_generate_layer%i", layer_index);
    snippet_data.function_prefix =
        c_strdup_printf("cg_generate_layer%i", layer_index);
    snippet_data.return_type = "vec4";
    snippet_data.return_variable = "cg_layer";
    snippet_data.source_buf = shader_state->header;

    _cg_pipeline_snippet_generate_code(&snippet_data);

    c_free((char *)snippet_data.chain_function);
    c_free((char *)snippet_data.final_name);
    c_free((char *)snippet_data.function_prefix);

    c_string_append_printf(shader_state->source,
                           "  cg_layer%i = cg_generate_layer%i ();\n",
                           layer_index,
                           layer_index);

    c_slice_free(layer_data_t, layer_data);
}

static void
ensure_layer_generated(cg_pipeline_shader_state_t *shader_state,
                       cg_pipeline_t *pipeline,
                       int layer_index)
{
    layer_data_t *layer_data;

    /* Find the layer that corresponds to this layer_num */
    c_list_for_each(layer_data, &shader_state->layers, link)
    {
        cg_pipeline_layer_t *layer = layer_data->layer;

        if (layer->index == layer_index)
            goto found;
    }

    /* If we didn't find it then we can assume the layer has already
       been generated */
    return;

found:

    generate_layer(shader_state, pipeline, layer_data);
}

static bool
_cg_pipeline_fragend_glsl_add_layer(cg_pipeline_t *pipeline,
                                    cg_pipeline_layer_t *layer,
                                    unsigned long layers_difference)
{
    cg_pipeline_shader_state_t *shader_state = get_shader_state(pipeline);
    layer_data_t *layer_data;

    if (!shader_state->source)
        return true;

    /* Store the layers in reverse order */
    layer_data = c_slice_new(layer_data_t);
    layer_data->layer = layer;

    if (c_list_empty(&shader_state->layers)) {
        layer_data->previous_layer_index = -1;
    } else {
        layer_data_t *first =
            c_container_of(shader_state->layers.next, layer_data_t, link);
        layer_data->previous_layer_index = first->layer->index;
    }

    c_list_insert(&shader_state->layers, &layer_data->link);

    return true;
}

/* GLES2 and GL3 don't have alpha testing so we need to implement it
   in the shader */

#if defined(HAVE_CG_GLES2) || defined(HAVE_CG_GL)

static void
add_alpha_test_snippet(cg_pipeline_t *pipeline,
                       cg_pipeline_shader_state_t *shader_state)
{
    cg_pipeline_alpha_func_t alpha_func;

    alpha_func = cg_pipeline_get_alpha_test_function(pipeline);

    if (alpha_func == CG_PIPELINE_ALPHA_FUNC_ALWAYS)
        /* Do nothing */
        return;

    if (alpha_func == CG_PIPELINE_ALPHA_FUNC_NEVER) {
        /* Always discard the fragment */
        c_string_append(shader_state->source, "  discard;\n");
        return;
    }

    /* For all of the other alpha functions we need a uniform for the
       reference */

    c_string_append(shader_state->header,
                    "uniform float _cg_alpha_test_ref;\n");

    c_string_append(shader_state->source, "  if (cg_color_out.a ");

    switch (alpha_func) {
    case CG_PIPELINE_ALPHA_FUNC_LESS:
        c_string_append(shader_state->source, ">=");
        break;
    case CG_PIPELINE_ALPHA_FUNC_EQUAL:
        c_string_append(shader_state->source, "!=");
        break;
    case CG_PIPELINE_ALPHA_FUNC_LEQUAL:
        c_string_append(shader_state->source, ">");
        break;
    case CG_PIPELINE_ALPHA_FUNC_GREATER:
        c_string_append(shader_state->source, "<=");
        break;
    case CG_PIPELINE_ALPHA_FUNC_NOTEQUAL:
        c_string_append(shader_state->source, "==");
        break;
    case CG_PIPELINE_ALPHA_FUNC_GEQUAL:
        c_string_append(shader_state->source, "< ");
        break;

    case CG_PIPELINE_ALPHA_FUNC_ALWAYS:
    case CG_PIPELINE_ALPHA_FUNC_NEVER:
        c_assert_not_reached();
        break;
    }

    c_string_append(shader_state->source,
                    " _cg_alpha_test_ref)\n    discard;\n");
}

#endif /*  HAVE_CG_GLES2 */

static bool
_cg_pipeline_fragend_glsl_end(cg_pipeline_t *pipeline,
                              unsigned long pipelines_difference)
{
    cg_pipeline_shader_state_t *shader_state = get_shader_state(pipeline);

    _CG_GET_DEVICE(dev, false);

    if (shader_state->source) {
        const char *source_strings[2];
        GLint lengths[2];
        GLint compile_status;
        GLuint shader;
        cg_pipeline_snippet_data_t snippet_data;

        CG_STATIC_COUNTER(fragend_glsl_compile_counter,
                          "glsl fragment compile counter",
                          "Increments each time a new GLSL "
                          "fragment shader is compiled",
                          0 /* no application private data */);
        CG_COUNTER_INC(_cg_uprof_context, fragend_glsl_compile_counter);

        if (!c_list_empty(&shader_state->layers)) {
            cg_pipeline_layer_t *last_layer;
            layer_data_t *layer_data;

            /* NB: layers are added in reverse order... */
            layer_data = c_list_first(&shader_state->layers, layer_data_t, link);
            last_layer = layer_data->layer;

            /* Note: generate_layer() works recursively, so if the value
             * of this layer depends on any previous layers then it will
             * also generate the code for those layers.
             */
            generate_layer(shader_state, pipeline, layer_data);

            c_string_append_printf(shader_state->source,
                                   "  cg_color_out = cg_layer%i;\n",
                                   last_layer->index);

            /* We now ensure we have code for all remaining layers that may
             * only be referenced by user snippets... */
            while (!c_list_empty(&shader_state->layers)) {
                layer_data = c_list_first(&shader_state->layers, layer_data_t, link);
                generate_layer(shader_state, pipeline, layer_data);
            }
        } else
            c_string_append(shader_state->source,
                            "  cg_color_out = cg_color_in;\n");

        add_alpha_test_snippet(pipeline, shader_state);

        /* Close the function surrounding the generated fragment processing */
        c_string_append(shader_state->source, "}\n");

        /* Add all of the hooks for fragment processing */
        memset(&snippet_data, 0, sizeof(snippet_data));
        snippet_data.snippets = get_fragment_snippets(pipeline);
        snippet_data.hook = CG_SNIPPET_HOOK_FRAGMENT;
        snippet_data.chain_function = "cg_generated_source";
        snippet_data.final_name = "main";
        snippet_data.function_prefix = "cg_fragment_hook";
        snippet_data.source_buf = shader_state->source;
        _cg_pipeline_snippet_generate_code(&snippet_data);

        GE_RET(shader, dev, glCreateShader(GL_FRAGMENT_SHADER));

        lengths[0] = shader_state->header->len;
        source_strings[0] = shader_state->header->str;
        lengths[1] = shader_state->source->len;
        source_strings[1] = shader_state->source->str;

        _cg_glsl_shader_set_source_with_boilerplate(dev,
                                                    shader,
                                                    GL_FRAGMENT_SHADER,
                                                    2, /* count */
                                                    source_strings,
                                                    lengths);

        GE(dev, glCompileShader(shader));
        GE(dev, glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status));

        if (!compile_status) {
            GLint len = 0;
            char *shader_log;

            GE(dev, glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len));
            shader_log = c_alloca(len);
            GE(dev, glGetShaderInfoLog(shader, len, &len, shader_log));
            c_warning("Shader compilation failed:\n%s", shader_log);
        }

        shader_state->header = NULL;
        shader_state->source = NULL;
        shader_state->gl_shader = shader;
    }

    return true;
}

static void
_cg_pipeline_fragend_glsl_pre_change_notify(cg_pipeline_t *pipeline,
                                            cg_pipeline_state_t change,
                                            const cg_color_t *new_color)
{
    _CG_GET_DEVICE(dev, NO_RETVAL);

    if ((change & _cg_pipeline_get_state_for_fragment_codegen(dev)))
        dirty_shader_state(pipeline);
}

/* NB: layers are considered immutable once they have any dependants
 * so although multiple pipelines can end up depending on a single
 * static layer, we can guarantee that if a layer is being *changed*
 * then it can only have one pipeline depending on it.
 *
 * XXX: Don't forget this is *pre* change, we can't read the new value
 * yet!
 */
static void
_cg_pipeline_fragend_glsl_layer_pre_change_notify(
    cg_pipeline_t *owner,
    cg_pipeline_layer_t *layer,
    cg_pipeline_layer_state_t change)
{
    _CG_GET_DEVICE(dev, NO_RETVAL);

    if ((change & _cg_pipeline_get_layer_state_for_fragment_codegen(dev))) {
        dirty_shader_state(owner);
        return;
    }

    /* TODO: we could be saving snippets of texture combine code along
     * with each layer and then when a layer changes we would just free
     * the snippet. */
}

const cg_pipeline_fragend_t _cg_pipeline_glsl_fragend = {
    _cg_pipeline_fragend_glsl_start,
    _cg_pipeline_fragend_glsl_add_layer,
    NULL, /* passthrough */
    _cg_pipeline_fragend_glsl_end,
    _cg_pipeline_fragend_glsl_pre_change_notify,
    NULL, /* pipeline_set_parent_notify */
    _cg_pipeline_fragend_glsl_layer_pre_change_notify
};

#endif /* CG_PIPELINE_FRAGEND_GLSL */
