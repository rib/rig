/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012-2015 Intel Corporation
 * Copyright (C) 2016 Robert Bragg
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

#include <rut.h>

/* XXX: this is actually in the rig/ directory and we need to
 * rename it... */
#include "rut-renderer.h"

#include "rig-engine.h"
#include "rig-renderer.h"
#include "rig-text-renderer.h"

#include "components/rig-camera.h"
#include "components/rig-light.h"
#include "components/rig-material.h"
#include "components/rig-source.h"
#include "components/rig-mesh.h"
#include "components/rig-nine-slice.h"
#include "components/rig-text.h"

#define GLSL(SRC...) #SRC

struct _rig_renderer_t {
    rut_object_base_t _base;

    rig_engine_t *engine;

    /* shadow mapping */
    cg_offscreen_t *shadow_fb;
    cg_texture_t *shadow_map;

    cg_texture_t *gradient;

    cg_pipeline_t *dof_pipeline_template;
    cg_pipeline_t *dof_pipeline;
    cg_pipeline_t *dof_unshaped_pipeline;

    rig_depth_of_field_t *dof;

    rig_camera_t *composite_camera;

    cg_snippet_t *alpha_mask_snippet;
    cg_snippet_t *alpha_mask_video_snippet;
    cg_snippet_t *lighting_vertex_snippet;
    cg_snippet_t *normal_map_vertex_snippet;
    cg_snippet_t *shadow_mapping_vertex_snippet;
    cg_snippet_t *blended_discard_snippet;
    cg_snippet_t *unblended_discard_snippet;
    cg_snippet_t *premultiply_snippet;
    cg_snippet_t *unpremultiply_snippet;
    cg_snippet_t *normal_map_fragment_snippet;
    cg_snippet_t *normal_map_video_snippet;
    cg_snippet_t *material_lighting_snippet;
    cg_snippet_t *simple_lighting_snippet;
    cg_snippet_t *shadow_mapping_fragment_snippet;
    cg_snippet_t *cache_position_snippet;
    cg_snippet_t *layer_skip_snippet;

    c_array_t *journal;

    rig_text_renderer_state_t *text_state;
};

typedef enum _cache_slot_t {
    CACHE_SLOT_SHADOW,
    CACHE_SLOT_COLOR_BLENDED,
    CACHE_SLOT_COLOR_UNBLENDED,
} cache_slot_t;

struct source_state 
{
    rut_closure_t ready_closure;
    rut_closure_t changed_closure;
    rig_source_t *source;
};

typedef enum _source_type_t {
    SOURCE_TYPE_COLOR,
    SOURCE_TYPE_AMBIENT_OCCLUSION,
    SOURCE_TYPE_ALPHA_MASK,
    SOURCE_TYPE_NORMAL_MAP,

    MAX_SOURCES,
} source_type_t;

typedef struct _rig_journal_entry_t {
    rig_entity_t *entity;
    c_matrix_t matrix;
} rig_journal_entry_t;

typedef enum _get_pipeline_flags_t {
    GET_PIPELINE_FLAG_N_FLAGS
} get_pipeline_flags_t;

/* In the shaders, any alpha value greater than or equal to this is
 * considered to be fully opaque. We can't just compare for equality
 * against 1.0 because at least on a Mac Mini there seems to be some
 * fuzziness in the interpolation of the alpha value across the
 * primitive so that it is sometimes slightly less than 1.0 even
 * though all of the vertices in the triangle are 1.0. This means some
 * of the pixels of the geometry would be painted with the blended
 * pipeline. The blended pipeline doesn't write to the depth value so
 * depending on the order of the triangles within the mesh it might
 * paint the back or the front of the mesh which causes weird sparkly
 * artifacts.
 *
 * I think it doesn't really make sense to paint meshes that have any
 * depth using the blended pipeline. In that case you would also need
 * to sort individual triangles of the mesh according to depth.
 * Perhaps the real solution to this problem is to avoid using the
 * blended pipeline at all for 3D meshes.
 *
 * However even for flat quad shapes it is probably good to have this
 * threshold because if a pixel is close enough to opaque that the
 * appearance will be the same then it is chaper to render it without
 * blending.
 */
#define OPAQUE_THRESHOLD 0.9999

#define N_PIPELINE_CACHE_SLOTS 5
#define N_PRIMITIVE_CACHE_SLOTS 1

/* TODO: reduce the size of this per-entity structure
 */
typedef struct _rig_renderer_priv_t {
    rig_renderer_t *renderer;

    cg_pipeline_t *pipeline_caches[N_PIPELINE_CACHE_SLOTS];
    struct source_state source_caches[MAX_SOURCES];
    cg_primitive_t *primitive_caches[N_PRIMITIVE_CACHE_SLOTS];

    rut_closure_t preferred_size_closure;

    rut_closure_t geom_changed_closure;
} rig_renderer_priv_t;

static void
_rig_renderer_free(void *object)
{
    rig_renderer_t *renderer = object;

    c_array_free(renderer->journal, true);
    renderer->journal = NULL;

    rig_text_renderer_state_destroy(renderer->text_state);

    rut_object_free(rig_renderer_t, object);
}

static void
set_entity_pipeline_cache(rig_entity_t *entity,
                          int slot,
                          cg_pipeline_t *pipeline)
{
    rig_renderer_priv_t *priv = entity->renderer_priv;

    if (priv->pipeline_caches[slot])
        cg_object_unref(priv->pipeline_caches[slot]);

    priv->pipeline_caches[slot] = pipeline;
    if (pipeline)
        cg_object_ref(pipeline);
}

static void
dirty_entity_pipelines(rig_entity_t *entity)
{
    for (int i = 0; i < N_PIPELINE_CACHE_SLOTS; i++)
        set_entity_pipeline_cache(entity, i, NULL);
}


static cg_pipeline_t *
get_entity_pipeline_cache(rig_entity_t *entity, int slot)
{
    rig_renderer_priv_t *priv = entity->renderer_priv;
    return priv->pipeline_caches[slot];
}

static void
source_changed_cb(rig_source_t *source, void *user_data)
{
    rig_engine_t *engine = user_data;

    rut_shell_queue_redraw(engine->shell);
}

static rig_source_t *
get_entity_source_cache(rig_entity_t *entity, int slot)
{
    rig_renderer_priv_t *priv = entity->renderer_priv;

    return priv->source_caches[slot].source;
}

static void
source_ready_cb(rig_source_t *source, void *user_data)
{
    rig_entity_t *entity = user_data;
    rig_source_t *color_src;
    rut_object_t *geometry;
    rig_material_t *material;
    float width, height;

    geometry = rig_entity_get_component(entity, RUT_COMPONENT_TYPE_GEOMETRY);
    material = rig_entity_get_component(entity, RUT_COMPONENT_TYPE_MATERIAL);

    dirty_entity_pipelines(entity);

    if (material->color_source)
        color_src = get_entity_source_cache(entity, SOURCE_TYPE_COLOR);
    else
        color_src = NULL;

    /* If the color source has changed then we may also need to update
     * the geometry according to the size of the color source */
    if (source != color_src)
        return;

    rig_source_get_natural_size(source, &width, &height);

    if (rut_object_is(geometry, RUT_TRAIT_ID_IMAGE_SIZE_DEPENDENT)) {
        rut_image_size_dependant_vtable_t *dependant =
            rut_object_get_vtable(geometry, RUT_TRAIT_ID_IMAGE_SIZE_DEPENDENT);
        dependant->set_image_size(geometry, width, height);
    }
}

static void
set_entity_source_cache(rig_engine_t *engine,
                        rig_entity_t *entity,
                        int slot,
                        rig_source_t *source)
{
    rig_renderer_priv_t *priv = entity->renderer_priv;
    struct source_state *source_state = &priv->source_caches[slot];

    if (source_state->source) {
        rut_closure_remove(&source_state->ready_closure);
        rut_closure_remove(&source_state->changed_closure);

        rut_object_unref(source_state->source);
    }

    source_state->source = source;
    if (source) {
        rut_object_ref(source);

        rut_closure_init(&source_state->ready_closure, source_ready_cb, entity);
        rig_source_add_ready_callback(source, &source_state->ready_closure);

        rut_closure_init(&source_state->changed_closure, source_changed_cb, engine);
        rig_source_add_on_changed_callback(source, &source_state->changed_closure);
    }
}

static void
set_entity_primitive_cache(rig_entity_t *entity,
                           int slot,
                           cg_primitive_t *primitive)
{
    rig_renderer_priv_t *priv = entity->renderer_priv;

    if (priv->primitive_caches[slot]) {
        rut_closure_remove(&priv->geom_changed_closure);

        cg_object_unref(priv->primitive_caches[slot]);
    }

    priv->primitive_caches[slot] = primitive;
    if (primitive) {
        cg_object_ref(primitive);

    }
}

static cg_primitive_t *
get_entity_primitive_cache(rig_entity_t *entity, int slot)
{
    rig_renderer_priv_t *priv = entity->renderer_priv;

    return priv->primitive_caches[slot];
}

static void
dirty_entity_primitives(rig_entity_t *entity)
{
    for (int i = 0; i < N_PRIMITIVE_CACHE_SLOTS; i++)
        set_entity_primitive_cache(entity, i, NULL);
}

static void
_rig_renderer_notify_entity_changed(rig_entity_t *entity)
{
    rig_renderer_priv_t *priv = entity->renderer_priv;
    rig_engine_t *engine;

    if (!priv)
        return;

    dirty_entity_pipelines(entity);
    dirty_entity_primitives(entity);

    engine = priv->renderer->engine;

    for (int i = 0; i < MAX_SOURCES; i++)
        set_entity_source_cache(engine, entity, i, NULL);

    rut_closure_remove(&priv->preferred_size_closure);
}

static void
_rig_renderer_free_priv(rig_entity_t *entity)
{
    rig_renderer_priv_t *priv = entity->renderer_priv;

    _rig_renderer_notify_entity_changed(entity);

    c_slice_free(rig_renderer_priv_t, priv);
    entity->renderer_priv = NULL;
}

rut_type_t rig_renderer_type;

static void
_rig_renderer_init_type(void)
{

    static rut_renderer_vtable_t renderer_vtable = {
        .notify_entity_changed = _rig_renderer_notify_entity_changed,
        .free_priv = _rig_renderer_free_priv
    };

    rut_type_t *type = &rig_renderer_type;
#define TYPE rig_renderer_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_renderer_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_RENDERER,
                       0, /* no implied properties */
                       &renderer_vtable);

#undef TYPE
}

rig_renderer_t *
rig_renderer_new(rig_frontend_t *frontend)
{
    rig_renderer_t *renderer = rut_object_alloc0(
        rig_renderer_t, &rig_renderer_type, _rig_renderer_init_type);

    renderer->engine = frontend->engine;

    renderer->journal = c_array_new(false, false, sizeof(rig_journal_entry_t));

    renderer->text_state = rig_text_renderer_state_new(frontend);

    return renderer;
}

static void
rig_journal_log(c_array_t *journal,
                rig_paint_context_t *paint_ctx,
                rig_entity_t *entity,
                const c_matrix_t *matrix)
{

    rig_journal_entry_t *entry;

    c_array_set_size(journal, journal->len + 1);
    entry = &c_array_index(journal, rig_journal_entry_t, journal->len - 1);

    entry->entity = rut_object_ref(entity);
    entry->matrix = *matrix;
}

static int
sort_entry_cb(const rig_journal_entry_t *entry0,
              const rig_journal_entry_t *entry1)
{
    float z0 = entry0->matrix.zw;
    float z1 = entry1->matrix.zw;

    /* TODO: also sort based on the state */

    if (z0 < z1)
        return -1;
    else if (z0 > z1)
        return 1;

    return 0;
}

static void
dirty_geometry_cb(rut_object_t *component, void *user_data)
{
    rig_entity_t *entity = user_data;

    dirty_entity_primitives(entity);
}

static void
dirty_pipelines_cb(rut_object_t *component, void *user_data)
{
    rig_entity_t *entity = user_data;

    dirty_entity_pipelines(entity);
}

static void
dirty_geometry_and_pipelines_cb(rut_object_t *component, void *user_data)
{
    rig_entity_t *entity = user_data;

    dirty_entity_primitives(entity);
    dirty_entity_pipelines(entity);
}

static void
dirty_all_cb(rut_object_t *component, void *user_data)
{
    rig_entity_t *entity = user_data;

    _rig_renderer_notify_entity_changed(entity);
}

static void
set_focal_parameters(cg_pipeline_t *pipeline,
                     float focal_distance,
                     float depth_of_field)
{
    int location;
    float distance;

    /* I want to have the focal distance as positive when it's in front of the
     * camera (it seems more natural, but as, in OpenGL, the camera is facing
     * the negative Ys, the actual value to give to the shader has to be
     * negated */
    distance = -focal_distance;

    location = cg_pipeline_get_uniform_location(pipeline, "dof_focal_distance");
    cg_pipeline_set_uniform_float(
        pipeline, location, 1 /* n_components */, 1 /* count */, &distance);

    location = cg_pipeline_get_uniform_location(pipeline, "dof_depth_of_field");
    cg_pipeline_set_uniform_float(pipeline,
                                  location,
                                  1 /* n_components */,
                                  1 /* count */,
                                  &depth_of_field);
}

static void
init_dof_pipeline_template(rig_renderer_t *renderer)
{
    rig_engine_t *engine = renderer->engine;
    cg_pipeline_t *pipeline;
    cg_depth_state_t depth_state;
    cg_snippet_t *snippet;

    pipeline = cg_pipeline_new(engine->shell->cg_device);

    cg_pipeline_set_color_mask(pipeline, CG_COLOR_MASK_ALPHA);

    cg_pipeline_set_blend(pipeline, "RGBA=ADD(SRC_COLOR, 0)", NULL);

    cg_depth_state_init(&depth_state);
    cg_depth_state_set_test_enabled(&depth_state, true);
    cg_pipeline_set_depth_state(pipeline, &depth_state, NULL);

    snippet = cg_snippet_new(
        CG_SNIPPET_HOOK_VERTEX,

        /* definitions */
        "uniform float dof_focal_distance;\n"
        "uniform float dof_depth_of_field;\n"
        "out float dof_blur;\n",
        //"out vec4 world_pos;\n",

        /* compute the amount of bluriness we want */
        "vec4 world_pos = cg_modelview_matrix * pos;\n"
        //"world_pos = cg_modelview_matrix * cg_position_in;\n"
        "dof_blur = 1.0 - clamp (abs (world_pos.z - dof_focal_distance) /\n"
        "                  dof_depth_of_field, 0.0, 1.0);\n");

    cg_pipeline_add_snippet(pipeline, renderer->cache_position_snippet);
    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

/* This was used to debug the focal distance and bluriness amount in the DoF
 * effect: */
#if 0
    cg_pipeline_set_color_mask (pipeline, CG_COLOR_MASK_ALL);
    snippet = cg_snippet_new (CG_SNIPPET_HOOK_FRAGMENT,
                              "in vec4 world_pos;\n"
                              "in float dof_blur;",

                              "cg_color_out = vec4(dof_blur,0,0,1);\n"
                              //"cg_color_out = vec4(1.0, 0.0, 0.0, 1.0);\n"
                              //"if (world_pos.z < -30.0) cg_color_out = vec4(0,1,0,1);\n"
                              //"if (abs (world_pos.z + 30.f) < 0.1) cg_color_out = vec4(0,1,0,1);\n"
                              "cg_color_out.a = dof_blur;\n"
                              //"cg_color_out.a = 1.0;\n"
                              );

    cg_pipeline_add_snippet (pipeline, snippet);
    cg_object_unref (snippet);
#endif

    renderer->dof_pipeline_template = pipeline;
}

static void
init_dof_unshaped_pipeline(rig_renderer_t *renderer)
{
    cg_pipeline_t *dof_unshaped_pipeline =
        cg_pipeline_copy(renderer->dof_pipeline_template);
    cg_snippet_t *snippet;

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             /* declarations */
                             "in float dof_blur;",

                             /* post */
                             "if (cg_color_out.a < 0.25)\n"
                             "  discard;\n"
                             "\n"
                             "cg_color_out.a = dof_blur;\n");

    cg_pipeline_add_snippet(dof_unshaped_pipeline, snippet);
    cg_object_unref(snippet);

    renderer->dof_unshaped_pipeline = dof_unshaped_pipeline;
}

static void
init_dof_pipeline(rig_renderer_t *renderer)
{
    cg_pipeline_t *dof_pipeline =
        cg_pipeline_copy(renderer->dof_pipeline_template);
    cg_snippet_t *snippet;

    /* store the bluriness in the alpha channel */
    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             "in float dof_blur;",
                             "cg_color_out.a = dof_blur;\n");
    cg_pipeline_add_snippet(dof_pipeline, snippet);
    cg_object_unref(snippet);

    renderer->dof_pipeline = dof_pipeline;
}

#ifdef RIG_ENABLE_DEBUG
static void
create_debug_gradient(rig_renderer_t *renderer)
{
    cg_vertex_p2c4_t quad[] = { { 0,   0,   0xff, 0x00, 0x00, 0xff },
                                { 0,   200, 0x00, 0xff, 0x00, 0xff },
                                { 200, 200, 0x00, 0x00, 0xff, 0xff },
                                { 200, 0,   0xff, 0xff, 0xff, 0xff } };
    cg_offscreen_t *offscreen;
    cg_device_t *dev = renderer->engine->shell->cg_device;
    cg_primitive_t *prim =
        cg_primitive_new_p2c4(dev, CG_VERTICES_MODE_TRIANGLE_FAN, 4, quad);
    cg_pipeline_t *pipeline = cg_pipeline_new(dev);

    renderer->gradient = cg_texture_2d_new_with_size(dev, 256, 256);

    offscreen = cg_offscreen_new_with_texture(renderer->gradient);

    cg_framebuffer_orthographic(offscreen, 0, 0, 200, 200, -1, 100);
    cg_framebuffer_clear4f(offscreen, CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                           0, 0, 0, 1);
    cg_primitive_draw(prim, offscreen, pipeline);

    cg_object_unref(prim);
    cg_object_unref(offscreen);
}
#endif

static void
ensure_shadow_map(rig_renderer_t *renderer)
{
    rig_engine_t *engine = renderer->engine;

    c_warn_if_fail(renderer->shadow_fb == NULL);

    renderer->shadow_fb = cg_offscreen_new(engine->shell->cg_device,
                                           1024, 1024);

    cg_framebuffer_set_depth_texture_enabled(renderer->shadow_fb, true);

    c_warn_if_fail(renderer->shadow_map == NULL);

    renderer->shadow_map = cg_framebuffer_get_depth_texture(renderer->shadow_fb);

#ifdef RIG_ENABLE_DEBUG
    /* Create a color gradient texture that can be used for debugging
     * shadow mapping.
     */
    create_debug_gradient(renderer);
#endif
}

static void
free_shadow_map(rig_renderer_t *renderer)
{
    if (renderer->shadow_map) {
        cg_object_unref(renderer->shadow_map);
        renderer->shadow_map = NULL;
    }
    if (renderer->shadow_fb) {
        cg_object_unref(renderer->shadow_fb);
        renderer->shadow_fb = NULL;
    }
}

void
rig_renderer_init(rig_renderer_t *renderer)
{
    ensure_shadow_map(renderer);

    /* We always want to use exactly the same snippets when creating
     * similar pipelines so that we can take advantage of Cogl's program
     * caching. The program cache only compares the snippet pointers,
     * not the contents of the snippets. Therefore we just create the
     * snippets we're going to use upfront and retain them */

    renderer->alpha_mask_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                       /* definitions */
                       "uniform float material_alpha_threshold;\n",
                       /* post */
                       "if (rig_source_sample4 (\n"
                       "    cg_tex_coord4_in.st).r < \n"
                       "    material_alpha_threshold)\n"
                       "  discard;\n");

    renderer->lighting_vertex_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_VERTEX,
                       /* definitions */
                       "uniform mat3 normal_matrix;\n"
                       "in vec3 tangent_in;\n"
                       "out vec3 normal, eye_direction;\n",
                       /* post */
                       "normal = normalize(normal_matrix * cg_normal_in);\n"
                       "eye_direction = -vec3(cg_modelview_matrix *\n"
                       "                      pos);\n");

    renderer->normal_map_vertex_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_VERTEX,
                       /* definitions */
                       "uniform vec3 light0_direction_norm;\n"
                       "out vec3 light_direction;\n",

                       /* post */
                       "vec3 tangent = normalize(normal_matrix * tangent_in);\n"
                       "vec3 binormal = cross(normal, tangent);\n"

                       /* Transform the light direction into tangent space */
                       "vec3 v;\n"
                       "v.x = dot (light0_direction_norm, tangent);\n"
                       "v.y = dot (light0_direction_norm, binormal);\n"
                       "v.z = dot (light0_direction_norm, normal);\n"
                       "light_direction = normalize (v);\n"

                       /* Transform the eye direction into tangent space */
                       "v.x = dot (eye_direction, tangent);\n"
                       "v.y = dot (eye_direction, binormal);\n"
                       "v.z = dot (eye_direction, normal);\n"
                       "eye_direction = normalize (v);\n");

    renderer->cache_position_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_VERTEX_TRANSFORM,
                       "out vec4 pos;\n",
                       "pos = cg_position_in;\n");

    renderer->shadow_mapping_vertex_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_VERTEX,

                       /* definitions */
                       "uniform mat4 light_shadow_matrix;\n"
                       "out vec4 shadow_coords;\n",

                       /* post */
                       "shadow_coords = light_shadow_matrix *\n"
                       "                pos;\n");

    renderer->blended_discard_snippet = cg_snippet_new(
        CG_SNIPPET_HOOK_FRAGMENT,
        /* definitions */
        NULL,

        /* post */
        "if (cg_color_out.a <= 0.0 ||\n"
        "    cg_color_out.a >= " C_STRINGIFY(OPAQUE_THRESHOLD) ")\n"
        "  discard;\n");

    renderer->unblended_discard_snippet = cg_snippet_new(
        CG_SNIPPET_HOOK_FRAGMENT,
        /* definitions */
        NULL,

        /* post */
        "if (cg_color_out.a < " C_STRINGIFY(OPAQUE_THRESHOLD) ")\n"
        "  discard;\n");

    renderer->premultiply_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                       /* definitions */
                       NULL,

                       /* post */

                       /* FIXME: Avoid premultiplying here by fiddling the
                        * blend mode instead which should be more efficient */
                       "cg_color_out.rgb *= cg_color_out.a;\n");

    renderer->unpremultiply_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                       /* definitions */
                       NULL,

                       /* post */

                       /* FIXME: We need to unpremultiply our colour at this
                        * point to perform lighting, but this is obviously not
                        * ideal and we should instead avoid being premultiplied
                        * at this stage by not premultiplying our textures on
                        * load for example. */
                       "cg_color_out.rgb /= cg_color_out.a;\n");

    renderer->normal_map_fragment_snippet = cg_snippet_new(
        CG_SNIPPET_HOOK_FRAGMENT,
        /* definitions */
        "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
        "uniform vec4 material_ambient, material_diffuse, material_specular;\n"
        "uniform float material_shininess;\n"
        "in vec3 light_direction, eye_direction;\n",

        /* post */
        "vec4 final_color;\n"
        "vec3 L = normalize(light_direction);\n"
        "vec3 N = rig_source_sample7(cg_tex_coord7_in.st).rgb;\n"
        "N = 2.0 * N - 1.0;\n"
        "N = normalize(N);\n"
        "vec4 ambient = light0_ambient * material_ambient;\n"
        "final_color = ambient * cg_color_out;\n"
        "float lambert = dot(N, L);\n"
        "if (lambert > 0.0)\n"
        "{\n"
        "  vec4 diffuse = light0_diffuse * material_diffuse;\n"
        "  vec4 specular = light0_specular * material_specular;\n"
        "  final_color += cg_color_out * diffuse * lambert;\n"
        "  vec3 E = normalize(eye_direction);\n"
        "  vec3 R = reflect (-L, N);\n"
        "  float specular_factor = pow (max(dot(R, E), 0.0),\n"
        "                               material_shininess);\n"
        "  final_color += specular * specular_factor;\n"
        "}\n"
        "cg_color_out.rgb = final_color.rgb;\n");

    renderer->material_lighting_snippet = cg_snippet_new(
        CG_SNIPPET_HOOK_FRAGMENT,
        /* definitions */
        "/* material lighting declarations */\n"
        "in vec3 normal, eye_direction;\n"
        "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
        "uniform vec3 light0_direction_norm;\n"
        "uniform vec4 material_ambient, material_diffuse, material_specular;\n"
        "uniform float material_shininess;\n",

        /* post */
        "vec4 final_color;\n"
        "vec3 L = light0_direction_norm;\n"
        "vec3 N = normalize(normal);\n"
        "vec4 ambient = light0_ambient * material_ambient;\n"
        "final_color = ambient * cg_color_out;\n"
        "float lambert = dot(N, L);\n"
        "if (lambert > 0.0)\n"
        "{\n"
        "  vec4 diffuse = light0_diffuse * material_diffuse;\n"
        "  vec4 specular = light0_specular * material_specular;\n"
        "  final_color += cg_color_out * diffuse * lambert;\n"
        "  vec3 E = normalize(eye_direction);\n"
        "  vec3 R = reflect (-L, N);\n"
        "  float specular_factor = pow (max(dot(R, E), 0.0),\n"
        "                               material_shininess);\n"
        "  final_color += specular * specular_factor;\n"
        "}\n"
        "cg_color_out.rgb = final_color.rgb;\n");

    renderer->simple_lighting_snippet = cg_snippet_new(
        CG_SNIPPET_HOOK_FRAGMENT,
        /* definitions */
        "/* simple lighting declarations */\n"
        "in vec3 normal, eye_direction;\n"
        "uniform vec4 light0_ambient, light0_diffuse, light0_specular;\n"
        "uniform vec3 light0_direction_norm;\n",

        /* post */
        "vec4 final_color;\n"
        "vec3 L = light0_direction_norm;\n"
        "vec3 N = normalize(normal);\n"
        "final_color = light0_ambient * cg_color_out;\n"
        "float lambert = dot(N, L);\n"
        "if (lambert > 0.0)\n"
        "{\n"
        "  final_color += cg_color_out * light0_diffuse * lambert;\n"
        "  vec3 E = normalize(eye_direction);\n"
        "  vec3 R = reflect (-L, N);\n"
        "  float specular = pow (max(dot(R, E), 0.0),\n"
        "                        2.);\n"
        "  final_color += light0_specular * vec4(.6, .6, .6, 1.0) *\n"
        "                 specular;\n"
        "}\n"
        "cg_color_out.rgb = final_color.rgb;\n");

    renderer->shadow_mapping_fragment_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                       /* declarations */
                       "in vec4 shadow_coords;\n",
                       /* post */
                       "#if __VERSION__ >= 130\n"
                       "  vec4 texel10 =\n"
                       "    texture (cg_sampler10, shadow_coords.xy);\n"
                       "#else\n"
                       "  vec4 texel10 =\n"
                       "    texture2D (cg_sampler10, shadow_coords.xy);\n"
                       "#endif\n"
                       "  float distance_from_light = texel10.r + 0.0005;\n"
                       "  float shadow = 1.0;\n"
                       "  if (distance_from_light < shadow_coords.z)\n"
                       "    shadow = 0.5;\n"
                       "  cg_color_out.rgb = shadow * cg_color_out.rgb;\n");

    renderer->layer_skip_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
    cg_snippet_set_replace(renderer->layer_skip_snippet, "");

    init_dof_pipeline_template(renderer);

    init_dof_unshaped_pipeline(renderer);

    init_dof_pipeline(renderer);

    renderer->composite_camera = rig_camera_new(renderer->engine,
                                                1, 1, /* ortho w/h */
                                                NULL); /* fb */
    rut_camera_set_clear(renderer->composite_camera, false);
}

void
rig_renderer_fini(rig_renderer_t *renderer)
{
    rut_object_unref(renderer->composite_camera);

    cg_object_unref(renderer->dof_pipeline_template);
    renderer->dof_pipeline_template = NULL;

    cg_object_unref(renderer->dof_pipeline);
    renderer->dof_pipeline = NULL;

    cg_object_unref(renderer->dof_unshaped_pipeline);
    renderer->dof_unshaped_pipeline = NULL;


    if (renderer->dof) {
        rig_dof_effect_free(renderer->dof);
        renderer->dof = NULL;
    }

    cg_object_unref(renderer->layer_skip_snippet);
    renderer->layer_skip_snippet = NULL;

    cg_object_unref(renderer->alpha_mask_snippet);
    renderer->alpha_mask_snippet = NULL;

    cg_object_unref(renderer->lighting_vertex_snippet);
    renderer->lighting_vertex_snippet = NULL;

    cg_object_unref(renderer->normal_map_vertex_snippet);
    renderer->normal_map_vertex_snippet = NULL;

    cg_object_unref(renderer->shadow_mapping_vertex_snippet);
    renderer->shadow_mapping_vertex_snippet = NULL;

    cg_object_unref(renderer->blended_discard_snippet);
    renderer->blended_discard_snippet = NULL;

    cg_object_unref(renderer->unblended_discard_snippet);
    renderer->unblended_discard_snippet = NULL;

    cg_object_unref(renderer->premultiply_snippet);
    renderer->premultiply_snippet = NULL;

    cg_object_unref(renderer->unpremultiply_snippet);
    renderer->unpremultiply_snippet = NULL;

    cg_object_unref(renderer->normal_map_fragment_snippet);
    renderer->normal_map_fragment_snippet = NULL;

    cg_object_unref(renderer->material_lighting_snippet);
    renderer->material_lighting_snippet = NULL;

    cg_object_unref(renderer->simple_lighting_snippet);
    renderer->simple_lighting_snippet = NULL;

    cg_object_unref(renderer->shadow_mapping_fragment_snippet);
    renderer->shadow_mapping_fragment_snippet = NULL;

    cg_object_unref(renderer->cache_position_snippet);
    renderer->cache_position_snippet = NULL;

    free_shadow_map(renderer);
}

static void
add_material_for_mask(cg_pipeline_t *pipeline,
                      rig_renderer_t *renderer,
                      rig_material_t *material,
                      rig_source_t **sources)
{
    if (sources[SOURCE_TYPE_ALPHA_MASK]) {
        rig_source_setup_pipeline(sources[SOURCE_TYPE_ALPHA_MASK], pipeline);
        cg_pipeline_add_snippet(pipeline, renderer->alpha_mask_snippet);
    }

    if (sources[SOURCE_TYPE_COLOR])
        rig_source_setup_pipeline(sources[SOURCE_TYPE_COLOR], pipeline);
}

static cg_pipeline_t *
get_entity_mask_pipeline(rig_renderer_t *renderer,
                         rig_entity_t *entity,
                         rut_object_t *geometry,
                         rig_material_t *material,
                         rig_source_t **sources,
                         get_pipeline_flags_t flags)
{
    cg_pipeline_t *pipeline;

    pipeline = get_entity_pipeline_cache(entity, CACHE_SLOT_SHADOW);

    if (pipeline) {
        if (sources[SOURCE_TYPE_ALPHA_MASK]) {
            int location;

            rig_source_attach_frame(sources[SOURCE_TYPE_ALPHA_MASK], pipeline);

            location = cg_pipeline_get_uniform_location(
                pipeline, "material_alpha_threshold");
            cg_pipeline_set_uniform_1f(
                pipeline, location, material->alpha_mask_threshold);
        }

        return cg_object_ref(pipeline);
    }

    if (rut_object_get_type(geometry) == &rig_nine_slice_type) {
        pipeline = cg_pipeline_copy(renderer->dof_unshaped_pipeline);

        if (material)
            add_material_for_mask(pipeline, renderer, material, sources);
    } else 
        pipeline = cg_object_ref(renderer->dof_pipeline);

    set_entity_pipeline_cache(entity, CACHE_SLOT_SHADOW, pipeline);

    return pipeline;
}

static void
get_light_modelviewprojection(const c_matrix_t *model_transform,
                              rig_entity_t *light,
                              const c_matrix_t *light_projection,
                              c_matrix_t *light_mvp)
{
    const c_matrix_t *light_transform;
    c_matrix_t light_view;

    /* TODO: cache the bias * light_projection * light_view matrix! */

    /* Transform from NDC coords to texture coords (with 0,0)
     * top-left. (column major order) */
    float bias[16] = { .5f,  .0f, .0f, .0f,
                       .0f, -.5f, .0f, .0f,
                       .0f,  .0f, .5f, .0f,
                       .5f,  .5f, .5f, 1.f };

    light_transform = rig_entity_get_transform(light);
    c_matrix_get_inverse(light_transform, &light_view);

    c_matrix_init_from_array(light_mvp, bias);
    c_matrix_multiply(light_mvp, light_mvp, light_projection);
    c_matrix_multiply(light_mvp, light_mvp, &light_view);

    c_matrix_multiply(light_mvp, light_mvp, model_transform);
}

static cg_pipeline_t *
get_entity_color_pipeline(rig_renderer_t *renderer,
                          rig_entity_t *entity,
                          rut_object_t *geometry,
                          rig_material_t *material,
                          rig_source_t **sources,
                          get_pipeline_flags_t flags,
                          bool blended)
{
    rig_engine_t *engine = renderer->engine;
    cg_depth_state_t depth_state;
    cg_pipeline_t *pipeline;
    cg_framebuffer_t *shadow_fb;
    cg_snippet_t *blend = renderer->blended_discard_snippet;
    cg_snippet_t *unblend = renderer->unblended_discard_snippet;
    int i;

    //return cg_pipeline_new(engine->shell->cg_device);

    /* TODO: come up with a scheme for minimizing how many separate
     * cg_pipeline_ts we create or at least deriving the pipelines
     * from a small set of templates.
     */

    if (blended) {
        pipeline = get_entity_pipeline_cache(entity, CACHE_SLOT_COLOR_BLENDED);
    } else {
        pipeline =
            get_entity_pipeline_cache(entity, CACHE_SLOT_COLOR_UNBLENDED);
    }

    if (pipeline) {
        cg_object_ref(pipeline);
        goto FOUND;
    }

    pipeline = cg_pipeline_new(engine->shell->cg_device);

    if (sources[SOURCE_TYPE_COLOR])
        rig_source_setup_pipeline(sources[SOURCE_TYPE_COLOR], pipeline);
    if (sources[SOURCE_TYPE_ALPHA_MASK])
        rig_source_setup_pipeline(sources[SOURCE_TYPE_ALPHA_MASK], pipeline);
    if (sources[SOURCE_TYPE_NORMAL_MAP])
        rig_source_setup_pipeline(sources[SOURCE_TYPE_NORMAL_MAP], pipeline);

    cg_pipeline_set_color4f(pipeline, 0.8f, 0.8f, 0.8f, 1.f);

    /* enable depth testing */
    cg_depth_state_init(&depth_state);
    cg_depth_state_set_test_enabled(&depth_state, true);

    if (blended)
        cg_depth_state_set_write_enabled(&depth_state, false);

    cg_pipeline_set_depth_state(pipeline, &depth_state, NULL);

    cg_pipeline_add_snippet(pipeline, renderer->cache_position_snippet);

    /* vertex_t shader setup for lighting */

    cg_pipeline_add_snippet(pipeline, renderer->lighting_vertex_snippet);

    if (sources[SOURCE_TYPE_NORMAL_MAP])
        cg_pipeline_add_snippet(pipeline, renderer->normal_map_vertex_snippet);

    if (rig_material_get_receive_shadow(material))
        cg_pipeline_add_snippet(pipeline,
                                renderer->shadow_mapping_vertex_snippet);

    /* and fragment shader */

    /* XXX: ideally we wouldn't have to rely on conditionals + discards
     * in the fragment shader to differentiate blended and unblended
     * regions and instead we should let users mark out opaque regions
     * in geometry.
     */
    cg_pipeline_add_snippet(pipeline, blended ? blend : unblend);

    cg_pipeline_add_snippet(pipeline, renderer->unpremultiply_snippet);

    if (sources[SOURCE_TYPE_COLOR] ||
        sources[SOURCE_TYPE_ALPHA_MASK] ||
        sources[SOURCE_TYPE_NORMAL_MAP])
    {
        if (sources[SOURCE_TYPE_ALPHA_MASK])
            cg_pipeline_add_snippet(pipeline, renderer->alpha_mask_snippet);

        if (sources[SOURCE_TYPE_NORMAL_MAP])
            cg_pipeline_add_snippet(pipeline,
                                    renderer->normal_map_fragment_snippet);
        else
            cg_pipeline_add_snippet(pipeline,
                                    renderer->material_lighting_snippet);
    } else
        cg_pipeline_add_snippet(pipeline, renderer->simple_lighting_snippet);

    if (rig_material_get_receive_shadow(material)) {
        /* Hook the shadow map sampling */

        cg_pipeline_set_layer_texture(pipeline, 10, renderer->shadow_map);
        /* For debugging the shadow mapping... */
        // cg_pipeline_set_layer_texture (pipeline, 7, renderer->gradient);

        cg_pipeline_add_layer_snippet(pipeline, 10, renderer->layer_skip_snippet);

        /* Handle shadow mapping */
        cg_pipeline_add_snippet(pipeline,
                                renderer->shadow_mapping_fragment_snippet);
    }

    cg_pipeline_add_snippet(pipeline, renderer->premultiply_snippet);

    if (!blended) {
        cg_pipeline_set_blend(pipeline, "RGBA = ADD (SRC_COLOR, 0)", NULL);
        set_entity_pipeline_cache(entity, CACHE_SLOT_COLOR_UNBLENDED, pipeline);
    } else
        set_entity_pipeline_cache(entity, CACHE_SLOT_COLOR_BLENDED, pipeline);

FOUND:

    /* FIXME: there's lots to optimize about this! */
    shadow_fb = renderer->shadow_fb;

    /* update uniforms in pipelines */
    {
        c_matrix_t light_shadow_matrix, light_projection;
        c_matrix_t model_transform;
        const float *light_matrix;
        int location;

        cg_framebuffer_get_projection_matrix(shadow_fb, &light_projection);

        /* TODO: use Cogl's MatrixStack api so we can update the entity
         * model matrix incrementally as we traverse the scenegraph */
        rut_graphable_get_transform(entity, &model_transform);

        get_light_modelviewprojection(&model_transform,
                                      engine->ui->light,
                                      &light_projection,
                                      &light_shadow_matrix);

        light_matrix = c_matrix_get_array(&light_shadow_matrix);

        location =
            cg_pipeline_get_uniform_location(pipeline, "light_shadow_matrix");
        cg_pipeline_set_uniform_matrix(
            pipeline, location, 4, 1, false, light_matrix);

        for (i = 0; i < 3; i++)
            if (sources[i])
                rig_source_attach_frame(sources[i], pipeline);
    }

    return pipeline;
}

static cg_pipeline_t *
get_entity_pipeline(rig_renderer_t *renderer,
                    rig_entity_t *entity,
                    rut_component_t *geometry,
                    rig_pass_t pass)
{
    rig_engine_t *engine = renderer->engine;
    rig_material_t *material =
        rig_entity_get_component(entity, RUT_COMPONENT_TYPE_MATERIAL);
    rig_source_t *sources[MAX_SOURCES];
    get_pipeline_flags_t flags = 0;
    rig_source_t *source;

    c_return_val_if_fail(material != NULL, NULL);

    /* FIXME: Instead of having rig_entity apis for caching image
     * sources, we should allow the renderer to track arbitrary
     * private state with entities so it can better manage caches
     * of different kinds of derived, renderer specific state.
     */

    sources[SOURCE_TYPE_COLOR] =
        get_entity_source_cache(entity, SOURCE_TYPE_COLOR);
    sources[SOURCE_TYPE_AMBIENT_OCCLUSION] =
        get_entity_source_cache(entity, SOURCE_TYPE_AMBIENT_OCCLUSION);
    sources[SOURCE_TYPE_ALPHA_MASK] =
        get_entity_source_cache(entity, SOURCE_TYPE_ALPHA_MASK);
    sources[SOURCE_TYPE_NORMAL_MAP] =
        get_entity_source_cache(entity, SOURCE_TYPE_NORMAL_MAP);

    /* Materials may be associated with various image sources which need
     * to be setup before we try creating pipelines and querying the
     * geometry of entities because many components are influenced by
     * the size of associated images being mapped.
     */
    source = material->color_source;

    if (source && !sources[SOURCE_TYPE_COLOR]) {
        sources[SOURCE_TYPE_COLOR] = source;

        set_entity_source_cache(engine, entity, SOURCE_TYPE_COLOR, source);

        rig_source_set_first_layer(source, 1);
    }

    source = material->ambient_occlusion_source;

    if (source && !sources[SOURCE_TYPE_AMBIENT_OCCLUSION]) {
        sources[SOURCE_TYPE_AMBIENT_OCCLUSION] = source;

        set_entity_source_cache(engine, entity, SOURCE_TYPE_AMBIENT_OCCLUSION, source);

        rig_source_set_first_layer(source, 10);
    }

    source = material->alpha_mask_source;

    if (source && !sources[SOURCE_TYPE_ALPHA_MASK]) {
        sources[SOURCE_TYPE_ALPHA_MASK] = source;

        set_entity_source_cache(engine, entity, SOURCE_TYPE_ALPHA_MASK, source);

        rig_source_set_first_layer(source, 4);
        rig_source_set_default_sample(source, false);
    }

    source = material->normal_map_source;

    if (source && !sources[SOURCE_TYPE_NORMAL_MAP]) {
        sources[SOURCE_TYPE_NORMAL_MAP] = source;

        set_entity_source_cache(engine, entity, SOURCE_TYPE_NORMAL_MAP, source);

        rig_source_set_first_layer(source, 7);
        rig_source_set_default_sample(source, false);
    }

    if (pass == RIG_PASS_COLOR_UNBLENDED)
        return get_entity_color_pipeline(renderer, entity, geometry, material,
                                         sources, flags, false);
    else if (pass == RIG_PASS_COLOR_BLENDED)
        return get_entity_color_pipeline(renderer, entity, geometry, material,
                                         sources, flags, true);
    else if (pass == RIG_PASS_DOF_DEPTH || pass == RIG_PASS_SHADOW)
        return get_entity_mask_pipeline(renderer, entity, geometry, material,
                                        sources, flags);

    c_warn_if_reached();
    return NULL;
}
static void
get_normal_matrix(const c_matrix_t *matrix, float *normal_matrix)
{
    c_matrix_t inverse_matrix;

    /* Invert the matrix */
    c_matrix_get_inverse(matrix, &inverse_matrix);

    /* Transpose it while converting it to 3x3 */
    normal_matrix[0] = inverse_matrix.xx;
    normal_matrix[1] = inverse_matrix.xy;
    normal_matrix[2] = inverse_matrix.xz;

    normal_matrix[3] = inverse_matrix.yx;
    normal_matrix[4] = inverse_matrix.yy;
    normal_matrix[5] = inverse_matrix.yz;

    normal_matrix[6] = inverse_matrix.zx;
    normal_matrix[7] = inverse_matrix.zy;
    normal_matrix[8] = inverse_matrix.zz;
}

static void
ensure_renderer_priv(rig_entity_t *entity, rig_renderer_t *renderer)
{
    /* If this entity was last renderered with some other renderer then
     * we free any private state associated with the previous renderer
     * before creating our own private state */
    if (entity->renderer_priv) {
        rut_object_t *renderer = *(rut_object_t **)entity->renderer_priv;
        if (rut_object_get_type(renderer) != &rig_renderer_type)
            rut_renderer_free_priv(renderer, entity);
    }

    if (!entity->renderer_priv) {
        rig_renderer_priv_t *priv = c_slice_new0(rig_renderer_priv_t);

        priv->renderer = renderer;
        entity->renderer_priv = priv;
    }
}

static cg_primitive_t *
get_entity_primitive(rig_renderer_t *renderer,
                     rig_entity_t *entity,
                     rut_component_t *geometry)
{
    cg_primitive_t *primitive = get_entity_primitive_cache(entity, 0);
    rig_renderer_priv_t *priv;

    if (primitive)
        return primitive;

    priv = entity->renderer_priv;

    primitive = rut_primable_get_primitive(geometry);
    set_entity_primitive_cache(entity, 0, primitive);

    if (rut_object_get_type(geometry) == &rig_nine_slice_type) {
        rut_closure_init(&priv->geom_changed_closure,
                         dirty_geometry_cb, entity);
        rig_nine_slice_add_update_callback((rig_nine_slice_t *)geometry,
                                           &priv->geom_changed_closure);
    }

    return primitive;
}

static void
rig_renderer_flush_journal(rig_renderer_t *renderer,
                           rig_paint_context_t *paint_ctx)
{
    c_array_t *journal = renderer->journal;
    rut_object_t *camera = paint_ctx->camera;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);
    rig_engine_t *engine = paint_ctx->engine;
    int start, dir, end;
    int i;

    /* TODO: use an inline qsort implementation */
    c_array_sort(journal, (void *)sort_entry_cb);

    /* We draw opaque geometry front-to-back so we are more likely to be
     * able to discard later fragments earlier by depth testing.
     *
     * We draw transparent geometry back-to-front so it blends
     * correctly.
     */
    if (paint_ctx->pass == RIG_PASS_COLOR_BLENDED) {
        start = 0;
        dir = 1;
        end = journal->len;
    } else {
        start = journal->len - 1;
        dir = -1;
        end = -1;
    }

    cg_framebuffer_push_matrix(fb);

    for (i = start; i != end; i += dir) {
        rig_journal_entry_t *entry =
            &c_array_index(journal, rig_journal_entry_t, i);
        rig_entity_t *entity = entry->entity;
        rut_object_t *geometry =
            rig_entity_get_component(entity, RUT_COMPONENT_TYPE_GEOMETRY);
        cg_pipeline_t *pipeline;
        cg_primitive_t *primitive;
        float normal_matrix[9];
        rig_material_t *material;

        if (rut_object_get_type(geometry) == &rig_text_type &&
            paint_ctx->pass == RIG_PASS_COLOR_BLENDED) {
            cg_framebuffer_set_modelview_matrix(fb, &entry->matrix);
            rig_text_renderer_draw(paint_ctx, renderer->text_state, geometry);
            continue;
        }

        if (!rut_object_is(geometry, RUT_TRAIT_ID_PRIMABLE))
            continue;

        /*
         * Setup Pipelines...
         */

        pipeline =
            get_entity_pipeline(renderer, entity, geometry, paint_ctx->pass);

        material =
            rig_entity_get_component(entity, RUT_COMPONENT_TYPE_MATERIAL);

        /*
         * Update Uniforms...
         */

        if ((paint_ctx->pass == RIG_PASS_DOF_DEPTH ||
             paint_ctx->pass == RIG_PASS_SHADOW)) {
            /* FIXME: avoid updating these uniforms for every primitive if
             * the focal parameters haven't change! */
            set_focal_parameters(pipeline,
                                 rut_camera_get_focal_distance(camera),
                                 rut_camera_get_depth_of_field(camera));

        } else if ((paint_ctx->pass == RIG_PASS_COLOR_UNBLENDED ||
                    paint_ctx->pass == RIG_PASS_COLOR_BLENDED)) {
            rig_ui_t *ui = engine->ui;
            rig_light_t *light =
                rig_entity_get_component(ui->light, RUT_COMPONENT_TYPE_LIGHT);
            int location;

            /* FIXME: only update the lighting uniforms when the light has
             * actually moved! */
            rig_light_set_uniforms(light, pipeline);

            /* FIXME: only update the material uniforms when the material has
             * actually changed! */
            if (material)
                rig_material_flush_uniforms(material, pipeline);

            get_normal_matrix(&entry->matrix, normal_matrix);

            location =
                cg_pipeline_get_uniform_location(pipeline, "normal_matrix");
            cg_pipeline_set_uniform_matrix(pipeline,
                                           location,
                                           3, /* dimensions */
                                           1, /* count */
                                           false, /* don't transpose again */
                                           normal_matrix);
        }

        /*
         * Draw Primitive...
         */
        primitive = get_entity_primitive(renderer, entity, geometry);

        cg_framebuffer_set_modelview_matrix(fb, &entry->matrix);

        cg_primitive_draw(primitive, fb, pipeline);

        cg_object_unref(pipeline);

        rut_object_unref(entry->entity);
    }

    cg_framebuffer_pop_matrix(fb);

    c_array_set_size(journal, 0);
}

static void
draw_entity_camera_frustum(rig_engine_t *engine,
                           rig_entity_t *entity,
                           cg_framebuffer_t *fb)
{
    rut_object_t *camera =
        rig_entity_get_component(entity, RUT_COMPONENT_TYPE_CAMERA);
    cg_primitive_t *primitive = rut_camera_create_frustum_primitive(camera);
    cg_pipeline_t *pipeline = cg_pipeline_new(engine->shell->cg_device);
    cg_depth_state_t depth_state;

    /* enable depth testing */
    cg_depth_state_init(&depth_state);
    cg_depth_state_set_test_enabled(&depth_state, true);
    cg_pipeline_set_depth_state(pipeline, &depth_state, NULL);

    rut_util_draw_jittered_primitive3f(fb, primitive, 0.8, 0.6, 0.1);

    cg_object_unref(primitive);
    cg_object_unref(pipeline);
}

static void
text_preferred_size_changed_cb(rut_object_t *sizable,
                               void *user_data)
{
    rig_text_t *text = sizable;
    float width, height;
    rig_property_t *width_prop = &text->properties[RIG_TEXT_PROP_WIDTH];

    if (width_prop->binding)
        width = rig_property_get_float(width_prop);
    else {
        rut_sizable_get_preferred_width(text, -1, NULL, &width);
    }

    rut_sizable_get_preferred_height(text, width, NULL, &height);
    rut_sizable_set_size(text, width, height);
}

static rut_traverse_visit_flags_t
entitygraph_pre_paint_cb(rut_object_t *object, int depth, void *user_data)
{
    rig_paint_context_t *paint_ctx = user_data;
    rig_renderer_t *renderer = paint_ctx->renderer;
    rut_object_t *camera = paint_ctx->camera;
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);

    if (rut_object_is(object, RUT_TRAIT_ID_TRANSFORMABLE)) {
        const c_matrix_t *matrix = rut_transformable_get_matrix(object);
        cg_framebuffer_push_matrix(fb);
        cg_framebuffer_transform(fb, matrix);
    }

    if (rut_object_get_type(object) == &rig_entity_type) {
        rig_entity_t *entity = object;
        rig_material_t *material;
        rut_object_t *geometry;
        c_matrix_t matrix;
        rig_renderer_priv_t *priv;

        material =
            rig_entity_get_component(entity, RUT_COMPONENT_TYPE_MATERIAL);
        if (!material || !rig_material_get_visible(material))
            return RUT_TRAVERSE_VISIT_CONTINUE;

        if (paint_ctx->pass == RIG_PASS_SHADOW &&
            !rig_material_get_cast_shadow(material))
            return RUT_TRAVERSE_VISIT_CONTINUE;

        geometry = rig_entity_get_component(object, RUT_COMPONENT_TYPE_GEOMETRY);
        if (!geometry)
            return RUT_TRAVERSE_VISIT_CONTINUE;

        ensure_renderer_priv(entity, renderer);
        priv = entity->renderer_priv;

        /* XXX: Ideally the renderer code wouldn't have to handle this
         * but for now we make sure to allocate all text components
         * their preferred size before rendering them.
         *
         * Note: we first check to see if the text component has a
         * binding for the width property, and if so we assume the
         * UI is constraining the width and wants the text to be
         * wrapped.
         */
        if (rut_object_get_type(geometry) == &rig_text_type) {
            rig_text_t *text = geometry;

            if (!priv->preferred_size_closure.list_node.next) {
                rut_closure_init(&priv->preferred_size_closure,
                                 text_preferred_size_changed_cb,
                                 NULL /* user data */);
                rut_sizable_add_preferred_size_callback(text, &priv->preferred_size_closure);
                text_preferred_size_changed_cb(text, NULL);
            }
        }

        cg_framebuffer_get_modelview_matrix(fb, &matrix);
        rig_journal_log(renderer->journal, paint_ctx, entity, &matrix);

        return RUT_TRAVERSE_VISIT_CONTINUE;
    }

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

static rut_traverse_visit_flags_t
entitygraph_post_paint_cb(rut_object_t *object, int depth, void *user_data)
{
    if (rut_object_is(object, RUT_TRAIT_ID_TRANSFORMABLE)) {
        rig_paint_context_t *paint_ctx = user_data;
        cg_framebuffer_t *fb =
            rut_camera_get_framebuffer(paint_ctx->camera);
        cg_framebuffer_pop_matrix(fb);
    }

    return RUT_TRAVERSE_VISIT_CONTINUE;
}

void
paint_camera_entity_pass(rig_paint_context_t *paint_ctx,
                         rig_entity_t *camera_entity)
{
    rut_object_t *saved_camera = paint_ctx->camera;
    rut_object_t *camera =
        rig_entity_get_component(camera_entity, RUT_COMPONENT_TYPE_CAMERA);
    rig_renderer_t *renderer = paint_ctx->renderer;
    rig_engine_t *engine = paint_ctx->engine;

    paint_ctx->camera = camera;

    rut_camera_flush(camera);

    rut_graphable_traverse(engine->ui->scene,
                           RUT_TRAVERSE_DEPTH_FIRST,
                           entitygraph_pre_paint_cb,
                           entitygraph_post_paint_cb,
                           paint_ctx);

    rig_renderer_flush_journal(renderer, paint_ctx);

    rut_camera_end_frame(camera);

    paint_ctx->camera = saved_camera;
}

static void
set_light_framebuffer(rig_renderer_t *renderer, rig_entity_t *light)
{
    rut_object_t *light_camera =
        rig_entity_get_component(light, RUT_COMPONENT_TYPE_CAMERA);
    cg_framebuffer_t *fb = renderer->shadow_fb;
    int width = cg_framebuffer_get_width(fb);
    int height = cg_framebuffer_get_height(fb);

    rut_camera_set_framebuffer(light_camera, fb);
    rut_camera_set_viewport(light_camera, 0, 0, width, height);
}

void
rig_renderer_paint_camera(rig_paint_context_t *paint_ctx,
                          rig_entity_t *camera_entity)
{
    rut_object_t *camera =
        rig_entity_get_component(camera_entity, RUT_COMPONENT_TYPE_CAMERA);
    cg_framebuffer_t *fb = rut_camera_get_framebuffer(camera);
    rig_renderer_t *renderer = paint_ctx->renderer;
    rig_engine_t *engine = paint_ctx->engine;
    rig_ui_t *ui = engine->ui;

    if (!ui->light) {
        c_warning("Can't render scene without any light");
        return;
    }

    /* TODO: support multiple lights */
    paint_ctx->pass = RIG_PASS_SHADOW;
    set_light_framebuffer(renderer, ui->light); /* FIXME: should have per-light fb */
    rig_entity_set_camera_view_from_transform(ui->light);
    paint_camera_entity_pass(paint_ctx, ui->light);

    //if (paint_ctx->enable_dof) {
    if (0) {
        const float *viewport = rut_camera_get_viewport(camera);
        int width = viewport[2];
        int height = viewport[3];
        int save_viewport_x = viewport[0];
        int save_viewport_y = viewport[1];
        cg_framebuffer_t *pass_fb;
        const cg_color_t *bg_color;
        rig_depth_of_field_t *dof = renderer->dof;

        if (!dof)
            renderer->dof = dof = rig_dof_effect_new(engine);

        rig_dof_effect_set_framebuffer_size(dof, width, height);

        pass_fb = rig_dof_effect_get_depth_pass_fb(dof);
        rut_camera_set_framebuffer(camera, pass_fb);
        rut_camera_set_viewport(camera, 0, 0, width, height);

        rut_camera_flush(camera);
        cg_framebuffer_clear4f(pass_fb,
                               CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                               1, 1, 1, 1);
        rut_camera_end_frame(camera);

        paint_ctx->pass = RIG_PASS_DOF_DEPTH;
        paint_camera_entity_pass(paint_ctx, camera_entity);

        pass_fb = rig_dof_effect_get_color_pass_fb(dof);
        rut_camera_set_framebuffer(camera, pass_fb);

        rut_camera_flush(camera);
        bg_color = rut_camera_get_background_color(camera);
        cg_framebuffer_clear4f(pass_fb,
                               CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH,
                               bg_color->red,
                               bg_color->green,
                               bg_color->blue,
                               bg_color->alpha);
        rut_camera_end_frame(camera);

        paint_ctx->pass = RIG_PASS_COLOR_UNBLENDED;
        paint_camera_entity_pass(paint_ctx, camera_entity);

        paint_ctx->pass = RIG_PASS_COLOR_BLENDED;
        paint_camera_entity_pass(paint_ctx, camera_entity);

        rut_camera_set_framebuffer(camera, fb);
        rut_camera_set_viewport(camera,
                                save_viewport_x, save_viewport_y,
                                width, height);

        rut_camera_set_framebuffer(renderer->composite_camera, fb);
        rut_camera_set_viewport(renderer->composite_camera,
                                save_viewport_x, save_viewport_y,
                                width, height);

        rut_camera_flush(renderer->composite_camera);
        rig_dof_effect_draw_rectangle(dof, fb, 0, 0, 1, 1);
        rut_camera_end_frame(renderer->composite_camera);

    } else {
        paint_ctx->pass = RIG_PASS_COLOR_UNBLENDED;
        paint_camera_entity_pass(paint_ctx, camera_entity);

#warning "FIXME: re-enable the blended pass in renderer"
        //paint_ctx->pass = RIG_PASS_COLOR_BLENDED;
        //paint_camera_entity_pass(paint_ctx, camera_entity);
    }
}
