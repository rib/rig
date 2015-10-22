/*
 * CGlib
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

#include <cglib-config.h>

#include <string.h>

#include <clib.h>

#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-pipeline-private.h"
#include "cg-pipeline-layer-private.h"
#include "cg-blend-string.h"
#include "cg-snippet-private.h"

#ifdef CG_PIPELINE_FRAGEND_GLSL

#include "cg-device-private.h"
#include "cg-object-private.h"
#include "cg-pipeline-cache.h"
#include "cg-pipeline-fragend-glsl-private.h"
#include "cg-glsl-shader-private.h"

/*
 * GL/GLES compatability defines for pipeline thingies:
 */

/* This might not be defined on GLES */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif

const cg_pipeline_fragend_t _cg_pipeline_glsl_backend;

static const char *const_number_strings[] =
    { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

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

    cg_device_t *dev;

    GLuint gl_shader;
    c_string_t *header, *source;
    int last_layer_index;

    cg_pipeline_cache_entry_t *cache_entry;
} cg_pipeline_shader_state_t;

static cg_user_data_key_t shader_state_key;

static cg_pipeline_shader_state_t *
shader_state_new(cg_device_t *dev, int n_layers, cg_pipeline_cache_entry_t *cache_entry)
{
    cg_pipeline_shader_state_t *shader_state;

    shader_state = c_slice_new0(cg_pipeline_shader_state_t);
    shader_state->dev = dev;
    shader_state->ref_count = 1;
    shader_state->cache_entry = cache_entry;
    shader_state->last_layer_index = -1;

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

    if (shader_state->cache_entry &&
        shader_state->cache_entry->pipeline != instance)
        shader_state->cache_entry->usage_count--;

    if (--shader_state->ref_count == 0) {
        if (shader_state->gl_shader)
            GE(shader_state->dev, glDeleteShader(shader_state->gl_shader));

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
                           "uniform sampler%s cg_sampler%i;\n"
                           "vec4 cg_texel%i;\n"
                           "vec4 cg_layer%i;\n",
                           layer->index,
                           layer->index,
                           layer->index,
                           target_string,
                           layer->index,
                           layer->index,
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
_cg_pipeline_fragend_glsl_start(cg_device_t *dev,
                                cg_pipeline_t *pipeline,
                                int n_layers,
                                unsigned long pipelines_difference)
{
    cg_pipeline_shader_state_t *shader_state;
    cg_pipeline_t *authority;
    cg_pipeline_cache_entry_t *cache_entry = NULL;

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
                shader_state = shader_state_new(dev, n_layers, cache_entry);

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

    add_layer_declarations(pipeline, shader_state);
    add_global_declarations(pipeline, shader_state);

    c_string_append(shader_state->source,
                    "void\n"
                    "cg_generated_source()\n"
                    "{\n");
}

static void
add_texture_lookup(cg_device_t *dev,
                   cg_pipeline_shader_state_t *shader_state,
                   cg_pipeline_t *pipeline,
                   cg_pipeline_layer_t *layer)
{
    cg_pipeline_snippet_data_t snippet_data;
    cg_texture_type_t texture_type;
    const char *target_string, *tex_coord_swizzle;
    int layer_index = layer->index;
    const char *suffix;

    texture_type = _cg_pipeline_layer_get_texture_type(layer);
    _cg_gl_util_get_texture_target_string(texture_type, &target_string,
                                          &tex_coord_swizzle);

    c_string_append_printf(shader_state->header,
                           "vec4\n"
                           "_cg_default_texture_lookup%i(sampler%s tex, vec4 coords)\n"
                           "{\n"
                           "  return ",
                           layer_index,
                           target_string);

    if (C_UNLIKELY(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_TEXTURING)))
        c_string_append(shader_state->header,
                        "vec4(1.0, 1.0, 1.0, 1.0);\n");
    else {
        if (dev->glsl_version_to_use >= 130) {
            c_string_append_printf(shader_state->header,
                                   "texture(tex, coords.%s);\n",
                                   tex_coord_swizzle);
        } else {
            c_string_append_printf(shader_state->header,
                                   "texture%s(tex, coords.%s);\n",
                                   target_string,
                                   tex_coord_swizzle);
        }
    }

    c_string_append(shader_state->header, "}\n");

    if (layer_index < 10)
        suffix = const_number_strings[layer_index];
    else
        suffix = c_strdup_printf("%i", layer_index);

    /* Wrap the texture lookup in any snippets that have been hooked */
    memset(&snippet_data, 0, sizeof(snippet_data));
    snippet_data.snippets = get_layer_fragment_snippets(layer);
    snippet_data.hook = CG_SNIPPET_HOOK_TEXTURE_LOOKUP;
    snippet_data.chain_function = "_cg_default_texture_lookup";
    snippet_data.chain_function_suffix = suffix;
    snippet_data.final_function = "cg_texture_lookup";
    snippet_data.final_function_suffix = suffix;
    snippet_data.return_type = "vec4";
    snippet_data.return_variable = "cg_texel";
    snippet_data.arguments = "cg_sampler, cg_tex_coord";
    snippet_data.argument_declarations =
        c_strdup_printf("sampler%s cg_sampler, vec4 cg_tex_coord",
                        target_string);
    snippet_data.source_buf = shader_state->header;

    _cg_pipeline_snippet_generate_code(&snippet_data);

    if (layer_index >= 10)
        c_free((char *)suffix);

    c_free((char *)snippet_data.argument_declarations);

    c_string_append_printf(shader_state->source,
                           "  cg_texel%i = cg_texture_lookup%i("
                           "cg_sampler%i, ",
                           layer_index,
                           layer_index,
                           layer_index);

    if (cg_pipeline_get_layer_point_sprite_coords_enabled(pipeline,
                                                          layer_index))
        c_string_append_printf(shader_state->source,
                               "vec4(cg_point_coord, 0.0, 1.0)");
    else
        c_string_append_printf(shader_state->source,
                               "cg_tex_coord%i_in", layer_index);

    c_string_append(shader_state->source, ");\n");
}

static bool
_cg_pipeline_fragend_glsl_add_layer(cg_device_t *dev,
                                    cg_pipeline_t *pipeline,
                                    cg_pipeline_layer_t *layer,
                                    unsigned long layers_difference)
{
    cg_pipeline_shader_state_t *shader_state = get_shader_state(pipeline);
    cg_pipeline_snippet_data_t snippet_data;
    int layer_index;
    const char *suffix;

    if (!shader_state->source)
        return true;

    layer_index = layer->index;

    add_texture_lookup(dev, shader_state, pipeline, layer);

    c_string_append_printf(shader_state->header,
                           "vec4\n"
                           "_cg_default_generate_layer%i(vec4 prev)\n"
                           "{\n",
                           layer_index);

    c_string_append_printf(shader_state->header,
                           "  return cg_texel%i * prev;\n",
                           layer_index);

    c_string_append(shader_state->header, "}\n");

    if (layer_index < 10)
        suffix = const_number_strings[layer_index];
    else
        suffix = c_strdup_printf("%i", layer_index);

    /* Wrap the layer code in any snippets that have been hooked */
    memset(&snippet_data, 0, sizeof(snippet_data));
    snippet_data.snippets = get_layer_fragment_snippets(layer);
    snippet_data.hook = CG_SNIPPET_HOOK_LAYER_FRAGMENT;
    snippet_data.chain_function = "_cg_default_generate_layer";
    snippet_data.chain_function_suffix = suffix;
    snippet_data.final_function = "cg_generate_layer";
    snippet_data.final_function_suffix = suffix;
    snippet_data.return_type = "vec4";
    snippet_data.return_variable = "frag";
    snippet_data.return_variable_is_argument = true;
    snippet_data.argument_declarations = "vec4 frag";
    snippet_data.arguments = "frag";
    snippet_data.source_buf = shader_state->header;

    _cg_pipeline_snippet_generate_code(&snippet_data);

    if (layer_index >= 10)
        c_free((char *)suffix);

    if (shader_state->last_layer_index >= 0) {
        c_string_append_printf(shader_state->source,
                               "  cg_layer%i = cg_generate_layer%i(cg_layer%i);\n",
                               layer_index,
                               layer_index,
                               shader_state->last_layer_index);
    } else {
        c_string_append_printf(shader_state->source,
                               "  cg_layer%i = cg_generate_layer%i(cg_color_in);\n",
                               layer_index,
                               layer_index);
    }

    shader_state->last_layer_index = layer_index;

    return true;
}

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

static bool
_cg_pipeline_fragend_glsl_end(cg_device_t *dev,
                              cg_pipeline_t *pipeline,
                              unsigned long pipelines_difference)
{
    cg_pipeline_shader_state_t *shader_state = get_shader_state(pipeline);

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

        if (shader_state->last_layer_index >= 0) {
            c_string_append_printf(shader_state->source,
                                   "  cg_color_out = cg_layer%i;\n",
                                   shader_state->last_layer_index);
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
        snippet_data.final_function = "main";
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
_cg_pipeline_fragend_glsl_pre_change_notify(cg_device_t *dev,
                                            cg_pipeline_t *pipeline,
                                            cg_pipeline_state_t change,
                                            const cg_color_t *new_color)
{
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
    cg_device_t *dev,
    cg_pipeline_t *owner,
    cg_pipeline_layer_t *layer,
    cg_pipeline_layer_state_t change)
{
    if ((change & _cg_pipeline_get_layer_state_for_fragment_codegen(dev))) {
        dirty_shader_state(owner);
        return;
    }
}

const cg_pipeline_fragend_t _cg_pipeline_glsl_fragend = {
    _cg_pipeline_fragend_glsl_start,
    _cg_pipeline_fragend_glsl_add_layer,
    _cg_pipeline_fragend_glsl_end,
    _cg_pipeline_fragend_glsl_pre_change_notify,
    _cg_pipeline_fragend_glsl_layer_pre_change_notify
};

#endif /* CG_PIPELINE_FRAGEND_GLSL */
