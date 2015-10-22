/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2013 Intel Corporation.
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
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include <string.h>

#include <test-fixtures/test-cg-fixtures.h>

#include "cg-device-private.h"
#include "cg-util-gl-private.h"
#include "cg-pipeline-private.h"
#include "cg-pipeline-opengl-private.h"

#ifdef CG_PIPELINE_VERTEND_GLSL

#include "cg-device-private.h"
#include "cg-object-private.h"
#include "cg-pipeline-vertend-glsl-private.h"
#include "cg-pipeline-state-private.h"
#include "cg-glsl-shader-private.h"

const cg_pipeline_vertend_t _cg_pipeline_glsl_vertend;

static const char *const_number_strings[] =
    { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

typedef struct {
    unsigned int ref_count;

    cg_device_t *dev;

    GLuint gl_shader;
    c_string_t *header, *source;

    cg_pipeline_cache_entry_t *cache_entry;
} cg_pipeline_shader_state_t;

static cg_user_data_key_t shader_state_key;

static cg_pipeline_shader_state_t *
shader_state_new(cg_device_t *dev, cg_pipeline_cache_entry_t *cache_entry)
{
    cg_pipeline_shader_state_t *shader_state;

    shader_state = c_slice_new0(cg_pipeline_shader_state_t);
    shader_state->dev = dev;
    shader_state->ref_count = 1;
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
_cg_pipeline_vertend_glsl_get_shader(cg_pipeline_t *pipeline)
{
    cg_pipeline_shader_state_t *shader_state = get_shader_state(pipeline);

    if (shader_state)
        return shader_state->gl_shader;
    else
        return 0;
}

static cg_pipeline_snippet_list_t *
get_vertex_snippets(cg_pipeline_t *pipeline)
{
    pipeline =
        _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_VERTEX_SNIPPETS);

    return &pipeline->big_state->vertex_snippets;
}

static cg_pipeline_snippet_list_t *
get_layer_vertex_snippets(cg_pipeline_layer_t *layer)
{
    unsigned long state = CG_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS;
    layer = _cg_pipeline_layer_get_authority(layer, state);

    return &layer->big_state->vertex_snippets;
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
                           "in vec4 cg_tex_coord%i_in;\n"
                           "out vec4 _cg_tex_coord%i;\n"
                           "#define cg_tex_coord%i_out _cg_tex_coord%i\n"
                           "uniform sampler%s cg_sampler%i;\n",
                           layer->index,
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
    cg_snippet_hook_t hook = CG_SNIPPET_HOOK_VERTEX_GLOBALS;
    cg_pipeline_snippet_list_t *snippets = get_vertex_snippets(pipeline);

    /* Add the global data hooks. All of the code in these snippets is
     * always added and only the declarations data is used */

    _cg_pipeline_snippet_generate_declarations(
        shader_state->header, hook, snippets);
}

static void
_cg_pipeline_vertend_glsl_start(cg_device_t *dev,
                                cg_pipeline_t *pipeline,
                                int n_layers,
                                unsigned long pipelines_difference)
{
    cg_pipeline_shader_state_t *shader_state;
    cg_pipeline_cache_entry_t *cache_entry = NULL;

    /* Now lookup our glsl backend private state (allocating if
     * necessary) */
    shader_state = get_shader_state(pipeline);

    if (shader_state == NULL) {
        cg_pipeline_t *authority;

        /* Get the authority for anything affecting vertex shader
           state */
        authority = _cg_pipeline_find_equivalent_parent(
            pipeline,
            _cg_pipeline_get_state_for_vertex_codegen(dev) &
            ~CG_PIPELINE_STATE_LAYERS,
            CG_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN);

        shader_state = get_shader_state(authority);

        if (shader_state == NULL) {
            /* Check if there is already a similar cached pipeline whose
               shader state we can share */
            if (C_LIKELY(
                    !(CG_DEBUG_ENABLED(CG_DEBUG_DISABLE_PROGRAM_CACHES)))) {
                cache_entry = _cg_pipeline_cache_get_vertex_template(
                    dev->pipeline_cache, authority);

                shader_state = get_shader_state(cache_entry->pipeline);
            }

            if (shader_state)
                shader_state->ref_count++;
            else
                shader_state = shader_state_new(dev, cache_entry);

            set_shader_state(authority, shader_state);

            shader_state->ref_count--;

            if (cache_entry)
                set_shader_state(cache_entry->pipeline, shader_state);
        }

        if (authority != pipeline)
            set_shader_state(pipeline, shader_state);
    }

    if (shader_state->gl_shader)
        return;

    /* If we make it here then we have a shader_state struct without a gl_shader
       because this is the first time we've encountered it */

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
                    "cg_generated_source ()\n"
                    "{\n");

    if (cg_pipeline_get_per_vertex_point_size(pipeline))
        c_string_append(shader_state->header, "in float cg_point_size_in;\n");
    else if (!_cg_has_private_feature(
                 dev, CG_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM)) {
        /* There is no builtin uniform for the point size on GLES2 so we
           need to copy it from the custom uniform in the vertex shader
           if we're not using per-vertex point sizes, however we'll only
           do this if the point-size is non-zero. Toggle the point size
           between zero and non-zero causes a state change which
           generates a new program */
        if (cg_pipeline_get_point_size(pipeline) > 0.0f) {
            c_string_append(shader_state->header,
                            "uniform float cg_point_size_in;\n");
            c_string_append(shader_state->source,
                            "  cg_point_size_out = cg_point_size_in;\n");
        }
    }
}

static bool
_cg_pipeline_vertend_glsl_add_layer(cg_device_t *dev,
                                    cg_pipeline_t *pipeline,
                                    cg_pipeline_layer_t *layer,
                                    unsigned long layers_difference,
                                    cg_framebuffer_t *framebuffer)
{
    cg_pipeline_shader_state_t *shader_state;
    cg_pipeline_snippet_data_t snippet_data;
    int layer_index = layer->index;
    const char *suffix;

    shader_state = get_shader_state(pipeline);

    if (shader_state->source == NULL)
        return true;

    /* Hook to transform the texture coordinates. By default this just
     * directly uses the texture coordinates.
     */

    c_string_append_printf(shader_state->header,
                           "vec4\n"
                           "_cg_default_transform_layer%i (vec4 tex_coord)\n"
                           "{\n"
                           "  return tex_coord;\n"
                           "}\n",
                           layer_index);

    if (layer_index < 10)
        suffix = const_number_strings[layer_index];
    else
        suffix = c_strdup_printf("%i", layer_index);

    /* Wrap the layer code in any snippets that have been hooked */
    memset(&snippet_data, 0, sizeof(snippet_data));
    snippet_data.snippets = get_layer_vertex_snippets(layer);
    snippet_data.hook = CG_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM;
    snippet_data.chain_function = "_cg_default_transform_layer";
    snippet_data.chain_function_suffix = suffix;
    snippet_data.final_function = "cg_transform_layer";
    snippet_data.final_function_suffix = suffix;
    snippet_data.return_type = "vec4";
    snippet_data.return_variable = "cg_tex_coord";
    snippet_data.return_variable_is_argument = true;
    snippet_data.arguments = "cg_tex_coord";
    snippet_data.argument_declarations = "vec4 cg_tex_coord";
    snippet_data.source_buf = shader_state->header;

    _cg_pipeline_snippet_generate_code(&snippet_data);

    if (layer_index >= 10)
        c_free((char *)suffix);

    c_string_append_printf(shader_state->source,
                           "  cg_tex_coord%i_out = "
                           "cg_transform_layer%i (cg_tex_coord%i_in);\n",
                           layer_index,
                           layer_index,
                           layer_index);
    return true;
}

static bool
_cg_pipeline_vertend_glsl_end(cg_device_t *dev,
                              cg_pipeline_t *pipeline,
                              unsigned long pipelines_difference)
{
    cg_pipeline_shader_state_t *shader_state;

    shader_state = get_shader_state(pipeline);

    if (shader_state->source) {
        const char *source_strings[2];
        GLint lengths[2];
        GLint compile_status;
        GLuint shader;
        cg_pipeline_snippet_data_t snippet_data;
        cg_pipeline_snippet_list_t *vertex_snippets;
        bool has_per_vertex_point_size =
            cg_pipeline_get_per_vertex_point_size(pipeline);

        CG_STATIC_COUNTER(vertend_glsl_compile_counter,
                          "glsl vertex compile counter",
                          "Increments each time a new GLSL "
                          "vertex shader is compiled",
                          0 /* no application private data */);
        CG_COUNTER_INC(_cg_uprof_context, vertend_glsl_compile_counter);

        c_string_append(shader_state->header,
                        "void\n"
                        "_cg_default_vertex_transform ()\n"
                        "{\n"
                        "  cg_position_out = "
                        "cg_modelview_projection_matrix * "
                        "cg_position_in;\n"
                        "}\n");

        c_string_append(shader_state->source, "  cg_vertex_transform ();\n");

        if (has_per_vertex_point_size) {
            c_string_append(shader_state->header,
                            "void\n"
                            "cg_real_point_size_calculation ()\n"
                            "{\n"
                            "  cg_point_size_out = cg_point_size_in;\n"
                            "}\n");
            c_string_append(shader_state->source,
                            "  cg_point_size_calculation ();\n");
        }

        c_string_append(shader_state->source,
                        "  cg_color_out = cg_color_in;\n"
                        "}\n");

        vertex_snippets = get_vertex_snippets(pipeline);

        /* Add hooks for the vertex transform part */
        memset(&snippet_data, 0, sizeof(snippet_data));
        snippet_data.snippets = vertex_snippets;
        snippet_data.hook = CG_SNIPPET_HOOK_VERTEX_TRANSFORM;
        snippet_data.chain_function = "_cg_default_vertex_transform";
        snippet_data.final_function = "cg_vertex_transform";
        snippet_data.source_buf = shader_state->header;
        _cg_pipeline_snippet_generate_code(&snippet_data);

        /* Add hooks for the point size calculation part */
        if (has_per_vertex_point_size) {
            memset(&snippet_data, 0, sizeof(snippet_data));
            snippet_data.snippets = vertex_snippets;
            snippet_data.hook = CG_SNIPPET_HOOK_POINT_SIZE;
            snippet_data.chain_function = "cg_real_point_size_calculation";
            snippet_data.final_function = "cg_point_size_calculation";
            snippet_data.source_buf = shader_state->header;
            _cg_pipeline_snippet_generate_code(&snippet_data);
        }

        /* Add all of the hooks for vertex processing */
        memset(&snippet_data, 0, sizeof(snippet_data));
        snippet_data.snippets = vertex_snippets;
        snippet_data.hook = CG_SNIPPET_HOOK_VERTEX;
        snippet_data.chain_function = "cg_generated_source";
        snippet_data.final_function = "cg_vertex_hook";
        snippet_data.source_buf = shader_state->source;
        _cg_pipeline_snippet_generate_code(&snippet_data);

        c_string_append(shader_state->source,
                        "void\n"
                        "main ()\n"
                        "{\n"
                        "  cg_vertex_hook ();\n");

        /* If there are any snippets then we can't rely on the
           projection matrix to flip the rendering for offscreen buffers
           so we'll need to flip it using an extra statement and a
           uniform */
        if (_cg_pipeline_has_vertex_snippets(pipeline)) {
            c_string_append(shader_state->header,
                            "uniform vec4 _cg_flip_vector;\n");
            c_string_append(shader_state->source,
                            "  cg_position_out *= _cg_flip_vector;\n");
        }

        c_string_append(shader_state->source, "}\n");

        GE_RET(shader, dev, glCreateShader(GL_VERTEX_SHADER));

        lengths[0] = shader_state->header->len;
        source_strings[0] = shader_state->header->str;
        lengths[1] = shader_state->source->len;
        source_strings[1] = shader_state->source->str;

        _cg_glsl_shader_set_source_with_boilerplate(dev,
                                                    shader,
                                                    GL_VERTEX_SHADER,
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

#ifdef CG_HAS_GL_SUPPORT
    if (_cg_has_private_feature(
            dev, CG_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM) &&
        (pipelines_difference & CG_PIPELINE_STATE_POINT_SIZE)) {
        cg_pipeline_t *authority =
            _cg_pipeline_get_authority(pipeline, CG_PIPELINE_STATE_POINT_SIZE);

        if (authority->big_state->point_size > 0.0f)
            GE(dev, glPointSize(authority->big_state->point_size));
    }
#endif /* CG_HAS_GL_SUPPORT */

    return true;
}

static void
_cg_pipeline_vertend_glsl_pre_change_notify(cg_device_t *dev,
                                            cg_pipeline_t *pipeline,
                                            cg_pipeline_state_t change,
                                            const cg_color_t *new_color)
{
    if ((change & _cg_pipeline_get_state_for_vertex_codegen(dev)))
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
_cg_pipeline_vertend_glsl_layer_pre_change_notify(cg_device_t *dev,
                                                  cg_pipeline_t *owner,
                                                  cg_pipeline_layer_t *layer,
                                                  cg_pipeline_layer_state_t change)
{
    cg_pipeline_shader_state_t *shader_state;

    shader_state = get_shader_state(owner);
    if (!shader_state)
        return;

    if ((change & CG_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN)) {
        dirty_shader_state(owner);
        return;
    }

    /* TODO: we could be saving snippets of texture combine code along
     * with each layer and then when a layer changes we would just free
     * the snippet. */
}

const cg_pipeline_vertend_t _cg_pipeline_glsl_vertend = {
    _cg_pipeline_vertend_glsl_start,
    _cg_pipeline_vertend_glsl_add_layer,
    _cg_pipeline_vertend_glsl_end,
    _cg_pipeline_vertend_glsl_pre_change_notify,
    _cg_pipeline_vertend_glsl_layer_pre_change_notify
};

TEST(check_point_size_shader)
{
    cg_pipeline_t *pipelines[4];
    cg_pipeline_shader_state_t *shader_states[C_N_ELEMENTS(pipelines)];
    int i;

    test_cg_init();

    /* Default pipeline with zero point size */
    pipelines[0] = cg_pipeline_new(test_dev);

    /* Point size 1 */
    pipelines[1] = cg_pipeline_new(test_dev);
    cg_pipeline_set_point_size(pipelines[1], 1.0f);

    /* Point size 2 */
    pipelines[2] = cg_pipeline_new(test_dev);
    cg_pipeline_set_point_size(pipelines[2], 2.0f);

    /* Same as the first pipeline, but reached by restoring the old
     * state from a copy */
    pipelines[3] = cg_pipeline_copy(pipelines[1]);
    cg_pipeline_set_point_size(pipelines[3], 0.0f);

    /* Draw something with all of the pipelines to make sure their state
     * is flushed */
    for (i = 0; i < C_N_ELEMENTS(pipelines); i++)
        cg_framebuffer_draw_rectangle(
            test_fb, pipelines[i], 0.0f, 0.0f, 10.0f, 10.0f);
    cg_framebuffer_finish(test_fb);

    /* Get all of the shader states. These might be NULL if the driver
     * is not using GLSL */
    for (i = 0; i < C_N_ELEMENTS(pipelines); i++)
        shader_states[i] = get_shader_state(pipelines[i]);

    /* If the first two pipelines are using GLSL then they should have
     * the same shader unless there is no builtin uniform for the point
     * size */
    if (shader_states[0]) {
        if (_cg_has_private_feature(
                test_dev, CG_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM))
            c_assert(shader_states[0] == shader_states[1]);
        else
            c_assert(shader_states[0] != shader_states[1]);
    }

    /* The second and third pipelines should always have the same shader
     * state because only toggling between zero and non-zero should
     * change the shader */
    c_assert(shader_states[1] == shader_states[2]);

    /* The fourth pipeline should be exactly the same as the first */
    c_assert(shader_states[0] == shader_states[3]);

    test_cg_fini();
}

#endif /* CG_PIPELINE_VERTEND_GLSL */
