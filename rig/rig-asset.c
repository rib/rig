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

#include <stdlib.h>

#include <clib.h>

#include <cglib/cglib.h>

#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif

#include <math.h>

#include <rut.h>

#include "rig-asset.h"
#include "rig-engine.h"

#include "components/rig-mesh.h"

#if 0
enum {
    ASSET_N_PROPS
};
#endif

/* TODO: Make a RUT_TRAIT_ID_ASSET and split
 * this api into separate objects for different
 * data types.
 */
struct _rig_asset_t {
    rut_object_base_t _base;

    rig_engine_t *engine;

#if 0
    rut_introspectable_props_t introspectable;
    rut_property_t props[ASSET_N_PROPS];
#endif

    rig_asset_type_t type;

    char *path;

    char *mime_type;

    uint8_t *data;
    size_t data_len;

    int natural_width;
    int natural_height;

    rut_mesh_t *mesh;
    bool has_tex_coords;
    bool has_normals;

#ifdef RIG_EDITOR_ENABLED
    cg_texture_t *thumbnail;
    c_list_t thumbnail_cb_list;
    c_llist_t *inferred_tags;
#endif
};

#if 0
static rut_property_spec_t _asset_prop_specs[] = {
    { 0 }
};
#endif

#if 0
#endif

static void
_rig_asset_free(void *object)
{
    rig_asset_t *asset = object;

#ifdef RIG_EDITOR_ENABLED
    if (asset->thumbnail)
        cg_object_unref(asset->thumbnail);
#endif

    if (asset->path)
        c_free(asset->path);

    // rut_introspectable_destroy (asset);

    rut_object_free(rig_asset_t, asset);
}

void
rig_asset_reap(rig_asset_t *asset, rig_engine_t *engine)
{
    /* Assets don't currently contain any other objects that would need
     * to be explicitly unregistered */

    rig_engine_queue_delete(engine, asset);
}

/* This is for copy & paste where we don't currently want a deep copy */
static rut_object_t *
_rig_asset_copy(rut_object_t *mimable)
{
    return rut_object_ref(mimable);
}

static bool
_rig_asset_has(rut_object_t *mimable, rut_mimable_type_t type)
{
    if (type == RUT_MIMABLE_TYPE_OBJECT)
        return true;
    else
        return false;
}

static void *
_rig_asset_get(rut_object_t *mimable, rut_mimable_type_t type)
{
    if (type == RUT_MIMABLE_TYPE_OBJECT)
        return mimable;
    else
        return NULL;
}

rut_type_t rig_asset_type;

void
_rig_asset_init_type(void)
{
    static rut_mimable_vtable_t mimable_vtable = { .copy = _rig_asset_copy,
                                                   .has = _rig_asset_has,
                                                   .get = _rig_asset_get, };

    rut_type_t *type = &rig_asset_type;
#define TYPE rig_asset_t

    rut_type_init(&rig_asset_type, C_STRINGIFY(TYPE), _rig_asset_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_MIMABLE,
                       0, /* no associated properties */
                       &mimable_vtable);

#if 0
    rut_type_add_trait (&_asset_type,
                        RUT_TRAIT_ID_INTROSPECTABLE,
                        offsetof (Asset, introspectable),
                        NULL); /* no implied vtable */
#endif

#undef TYPE
}

static rig_asset_t *
asset_new_from_image_data(rig_engine_t *engine,
                          const char *path,
                          const char *mime_type,
                          rig_asset_type_t type,
                          const uint8_t *data,
                          size_t len,
                          int natural_width,
                          int natural_height,
                          rut_exception_t **e)
{
    rig_asset_t *asset =
        rut_object_alloc0(rig_asset_t, &rig_asset_type, _rig_asset_init_type);
    rut_shell_t *shell = engine->shell;

    asset->engine = engine;

    asset->type = type;

    asset->path = c_strdup(path);
    asset->mime_type = c_strdup(mime_type);

    asset->natural_width = natural_width;
    asset->natural_height = natural_height;

    if (shell->headless) {
#warning "FIXME: don't keep image data in simulator when running a web application"
        asset->data = c_memdup(data, len);
        asset->data_len = len;
        return asset;
    }

    return asset;
}


rig_asset_t *
asset_new_from_mesh(rig_engine_t *engine, rut_mesh_t *mesh)
{
    rig_asset_t *asset =
        rut_object_alloc0(rig_asset_t, &rig_asset_type, _rig_asset_init_type);
    int i;

    asset->engine = engine;

    asset->type = RIG_ASSET_TYPE_MESH;

    asset->mesh = rut_object_ref(mesh);
    asset->has_normals = false;
    asset->has_tex_coords = false;

    for (i = 0; i < mesh->n_attributes; i++) {
        if (strcmp(mesh->attributes[i]->name, "cg_normal_in") == 0)
            asset->has_normals = true;
        else if (strcmp(mesh->attributes[i]->name, "cg_tex_coord0_in") == 0)
            asset->has_tex_coords = true;
    }

/* XXX: for ply mesh handling the needs_normals/tex_coords refers
 * to needing to initialize these attributes, since we guarantee
 * that the mesh itself will always have cg_normal_in and
 * cg_tex_coord0_in attributes.
 */
#warning "fixme: not consistent with ply mesh handling where we guarantee at least padded normals/tex_coords"

    return asset;
}

rig_asset_t *
asset_new_from_font_data(rig_engine_t *engine,
                         const uint8_t *data,
                         size_t len,
                         rut_exception_t **e)
{
    rig_asset_t *asset =
        rut_object_alloc0(rig_asset_t, &rig_asset_type, _rig_asset_init_type);

    asset->engine = engine;

    asset->type = RIG_ASSET_TYPE_FONT;

    asset->data = c_memdup(data, len);
    asset->data_len = len;

    return asset;
}

rig_asset_t *
rig_asset_new_from_pb_asset(rig_pb_un_serializer_t *unserializer,
                            Rig__Asset *pb_asset,
                            rut_exception_t **e)
{
    rig_engine_t *engine = unserializer->engine;
    rig_asset_t *asset = NULL;

    switch (pb_asset->type) {
    case RIG_ASSET_TYPE_TEXTURE:
    case RIG_ASSET_TYPE_NORMAL_MAP:
    case RIG_ASSET_TYPE_ALPHA_MASK: {
        const char *mime_type = pb_asset->mime_type;
        int width = 640;
        int height = 480;
        uint8_t *data = NULL;
        int data_len = 0;

        if (pb_asset->has_data) {
            data = pb_asset->data.data;
            data_len = pb_asset->data.len;
        }

        /* TODO: eventually remove this compatibility fallback */
        if (!mime_type) {
            int len = strlen(pb_asset->path);
            if (len > 3) {
                if (strcmp(pb_asset->path + len - 4, ".png") == 0)
                    mime_type = "image/png";
                else
                    mime_type = "image/jpeg";
            }
        }

        if (pb_asset->has_width) {
            width = pb_asset->width;
            height = pb_asset->height;
        }

        if (mime_type) {
            asset = asset_new_from_image_data(unserializer->engine,
                                              pb_asset->path,
                                              mime_type,
                                              pb_asset->type,
                                              data,
                                              data_len,
                                              width, height,
                                              e);
        } else {
            rut_throw(e, RUT_IO_EXCEPTION, RUT_IO_EXCEPTION_IO,
                      "Missing image mime type for asset %s",
                      pb_asset->path);
        }

        return asset;
    }
    case RIG_ASSET_TYPE_FONT: {
        asset = asset_new_from_font_data(engine,
                                         pb_asset->data.data,
                                         pb_asset->data.len,
                                         e);
        return asset;
    }
    case RIG_ASSET_TYPE_MESH:
        if (pb_asset->mesh) {
            rut_mesh_t *mesh =
                rig_pb_unserialize_rut_mesh(unserializer, pb_asset->mesh);
            if (!mesh) {
                rut_throw(e,
                          RUT_IO_EXCEPTION,
                          RUT_IO_EXCEPTION_IO,
                          "Error unserializing mesh");
                return NULL;
            }
            asset = asset_new_from_mesh(engine, mesh);
            rut_object_unref(mesh);
            return asset;
        } else {
            rut_throw(e, RUT_IO_EXCEPTION, RUT_IO_EXCEPTION_IO,
                      "Missing mesh data");
            return NULL;
        }

        return asset;
    case RIG_ASSET_TYPE_BUILTIN:
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "Can't instantiate a builtin asset from data");
        return NULL;
    }

    rut_throw(e, RUT_IO_EXCEPTION, RUT_IO_EXCEPTION_IO, "Unknown asset type");

    return NULL;
}

rig_asset_type_t
rig_asset_get_type(rig_asset_t *asset)
{
    return asset->type;
}

const char *
rig_asset_get_path(rig_asset_t *asset)
{
    return asset->path;
}

const char *
rig_asset_get_mime_type(rig_asset_t *asset)
{
    return asset->mime_type;
}

rut_shell_t *
rig_asset_get_shell(rig_asset_t *asset)
{
    return asset->engine->shell;
}

rut_mesh_t *
rig_asset_get_mesh(rig_asset_t *asset)
{
    return asset->mesh;
}

void
rig_asset_get_image_size(rig_asset_t *asset, int *width, int *height)
{
    *width = asset->natural_width;
    *height = asset->natural_height;
}

void *
rig_asset_get_data(rig_asset_t *asset)
{
    return asset->data;
}

size_t
rig_asset_get_data_len(rig_asset_t *asset)
{
    return asset->data_len;
}

bool
rig_asset_get_mesh_has_tex_coords(rig_asset_t *asset)
{
    return asset->has_tex_coords;
}

bool
rig_asset_get_mesh_has_normals(rig_asset_t *asset)
{
    return asset->has_normals;
}



/**
 * ============================================================================
 * TODO: split following code into editor
 * ============================================================================
 */

#ifdef RIG_EDITOR_ENABLED

/* These should be sorted in descending order of size to
 * avoid gaps due to attributes being naturally aligned. */
static rut_ply_attribute_t ply_attributes[] = {
    { .name = "cg_position_in",
      .properties = { { "x" }, { "y" }, { "z" }, },
      .n_properties = 3,
      .min_components = 1, },
    { .name = "cg_normal_in",
      .properties = { { "nx" }, { "ny" }, { "nz" }, },
      .n_properties = 3,
      .min_components = 3,
      .pad_n_components = 3,
      .pad_type = RUT_ATTRIBUTE_TYPE_FLOAT, },
    { .name = "cg_tex_coord0_in",
      .properties = { { "s" }, { "t" }, { "r" }, },
      .n_properties = 3,
      .min_components = 2,
      .pad_n_components = 2,
      .pad_type = RUT_ATTRIBUTE_TYPE_FLOAT, },
    { .name = "tangent_in",
      .properties = { { "tanx" }, { "tany" }, { "tanz" } },
      .n_properties = 3,
      .min_components = 3,
      .pad_n_components = 3,
      .pad_type = RUT_ATTRIBUTE_TYPE_FLOAT, },
    { .name = "cg_color_in",
      .properties = { { "red" }, { "green" }, { "blue" }, { "alpha" } },
      .n_properties = 4,
      .normalized = true,
      .min_components = 3, }
};

#ifdef USE_GSTREAMER

typedef struct _rig_thumbnail_generator_t {
    cg_device_t *dev;
    cg_pipeline_t *cg_pipeline;
    rig_asset_t *video;
    GstElement *pipeline;
    GstElement *bin;
    CgGstVideoSink *sink;
    bool seek_done;
} rig_thumbnail_generator_t;

static void
video_thumbnailer_grab(void *instance, void *user_data)
{
    rig_thumbnail_generator_t *generator = user_data;
    cg_offscreen_t *offscreen;
    cg_framebuffer_t *fbo;
    int tex_width;
    int tex_height;

    generator->cg_pipeline = cg_gst_video_sink_get_pipeline(generator->sink);

    tex_height = 200;
    tex_width =
        cg_gst_video_sink_get_width_for_height(generator->sink, tex_height);

    if (generator->video->texture)
        cg_object_unref(generator->video->texture);

    generator->video->texture =
        cg_texture_2d_new_with_size(generator->dev, tex_width, tex_height);

    offscreen = cg_offscreen_new_with_texture(generator->video->texture);
    fbo = offscreen;

    cg_framebuffer_clear4f(fbo, CG_BUFFER_BIT_COLOR, 0, 0, 0, 0);
    cg_framebuffer_orthographic(fbo, 0, 0, tex_width, tex_height, 1, -1);
    cg_framebuffer_draw_textured_rectangle(
        fbo, generator->cg_pipeline, 0, 0, tex_width, tex_height, 0, 0, 1, 1);

    cg_object_unref(offscreen);
    gst_element_set_state(generator->pipeline, GST_STATE_NULL);
    g_object_unref(generator->sink);

    rut_closure_list_invoke(&generator->video->thumbnail_cb_list,
                            rut_thumbnail_callback_t,
                            generator->video);

    c_free(generator);
}

static gboolean
video_thumbnailer_seek(GstBus *bus, GstMessage *msg, void *user_data)
{
    rig_thumbnail_generator_t *generator =
        (rig_thumbnail_generator_t *)user_data;
    int64_t duration, seek;

    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ASYNC_DONE &&
        !generator->seek_done) {
        gst_element_query_duration(generator->bin, GST_FORMAT_TIME, &duration);
        seek = (rand() % (duration / (GST_SECOND))) * GST_SECOND;
        gst_element_seek_simple(generator->pipeline,
                                GST_FORMAT_TIME,
                                GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                                seek);

        gst_element_get_state(generator->bin, NULL, 0, 0.2 * GST_SECOND);
        generator->seek_done = true;
    }

    return true;
}

static void
generate_video_thumbnail(rig_asset_t *asset)
{
    rig_thumbnail_generator_t *generator = c_new(rig_thumbnail_generator_t, 1);
    rut_shell_t *shell = asset->engine->shell;
    char *filename;
    char *uri;
    GstBus *bus;

    generator->seek_done = false;
    generator->dev = shell->cg_device;
    generator->video = asset;
    generator->sink = cg_gst_video_sink_new(shell->cg_device);
    generator->pipeline = gst_pipeline_new("thumbnailer");
    generator->bin = gst_element_factory_make("playbin", NULL);

    filename = c_build_filename(shell->assets_location, asset->path, NULL);
    uri = gst_filename_to_uri(filename, NULL);
    c_free(filename);

    g_object_set(G_OBJECT(generator->bin),
                 "video-sink",
                 GST_ELEMENT(generator->sink),
                 NULL);
    g_object_set(G_OBJECT(generator->bin), "uri", uri, NULL);
    gst_bin_add(GST_BIN(generator->pipeline), generator->bin);

    gst_element_set_state(generator->pipeline, GST_STATE_PAUSED);

    bus = gst_element_get_bus(generator->pipeline);
    gst_bus_add_watch(bus, video_thumbnailer_seek, generator);

    g_signal_connect(generator->sink,
                     "new-frame",
                     G_CALLBACK(video_thumbnailer_grab),
                     generator);

    c_free(uri);
}

#endif /* USE_GSTREAMER */

static cg_texture_t *
generate_mesh_thumbnail(rig_asset_t *asset)
{

    rig_mesh_t *mesh = rig_mesh_new_with_rut_mesh(asset->engine,
                                                  asset->mesh);
    rut_shell_t *shell = asset->engine->shell;
    cg_texture_t *thumbnail;
    cg_offscreen_t *offscreen;
    cg_framebuffer_t *frame_buffer;
    cg_pipeline_t *pipeline;
    cg_primitive_t *primitive;
    cg_snippet_t *snippet;
    cg_depth_state_t depth_state;
    c_matrix_t view;
    int tex_width = 800;
    int tex_height = 800;
    float fovy = 60;
    float aspect = (float)tex_width / (float)tex_height;
    float z_near = 0.1;
    float z_2d = 1000;
    float z_far = 2000;
    float translate_x = 0;
    float translate_y = 0;
    float translate_z = 0;
    float rec_scale = 800;
    float scale_facor = 1;
    float model_scale;
    float width = model->max_x - model->min_x;
    float height = model->max_y - model->min_y;
    float length = model->max_z - model->min_z;
    float light_pos[3] = { model->max_x, model->max_y, model->max_z };
    float light_amb[4] = { 0.2, 0.2, 0.2, 1.0 };
    float light_diff[4] = { 0.5, 0.5, 0.5, 1.0 };
    float light_spec[4] = { 0.5, 0.5, 0.5, 1.0 };
    float mat_amb[4] = { 0.2, 0.2, 0.2, 1.0 };
    float mat_diff[4] = { 0.39, 0.64, 0.62, 1.0 };
    float mat_spec[4] = { 0.5, 0.5, 0.5, 1.0 };
    int location;

    thumbnail =
        cg_texture_2d_new_with_size(shell->cg_device, tex_width, tex_height);

    offscreen = cg_offscreen_new_with_texture(thumbnail);
    frame_buffer = offscreen;

    cg_framebuffer_perspective(frame_buffer, fovy, aspect, z_near, z_far);
    c_matrix_init_identity(&view);
    c_matrix_view_2d_in_perspective(
        &view, fovy, aspect, z_near, z_2d, tex_width, tex_height);
    cg_framebuffer_set_modelview_matrix(frame_buffer, &view);

    pipeline = cg_pipeline_new(shell->cg_device);

    snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_VERTEX,
                       "in vec3 tangent_in;\n"
                       "in vec2 cg_tex_coord0_in;\n"
                       "in vec2 cg_tex_coord1_in;\n"
                       "in vec2 cg_tex_coord2_in;\n"
                       "in vec2 cg_tex_coord5_in;\n"
                       "uniform vec3 light_pos;\n"
                       "uniform vec4 light_amb;\n"
                       "uniform vec4 light_diff;\n"
                       "uniform vec4 light_spec;\n"
                       "uniform vec4 mat_amb;\n"
                       "uniform vec4 mat_diff;\n"
                       "uniform vec4 mat_spec;\n"
                       "out vec3 trans_light;\n"
                       "out vec3 eye;\n"
                       "out vec3 normal;\n",
                       "normal = vec3 (normalize (cg_modelview_matrix * \
                                      vec4 (cg_normal_in.x, cg_normal_in.y,\
                                      cg_normal_in.z, 1.0)));\n"
                       "eye = -vec3 (cg_modelview_matrix * cg_position_in);\n"
                       "trans_light = vec3 (normalize (cg_modelview_matrix *\
                                           vec4 (light_pos.x, light_pos.y,\
                                           light_pos.z, 1.0)));\n");

    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT,
                             "uniform vec3 light_pos;\n"
                             "uniform vec4 light_amb;\n"
                             "uniform vec4 light_diff;\n"
                             "uniform vec4 light_spec;\n"
                             "uniform vec4 mat_amb;\n"
                             "uniform vec4 mat_diff;\n"
                             "uniform vec4 mat_spec;\n"
                             "in vec3 trans_light;\n"
                             "in vec3 eye;\n"
                             "in vec3 normal;\n",
                             "vec4 final_color;\n"
                             "vec3 L = normalize (trans_light);\n"
                             "vec3 N = normalize (normal);\n"
                             "vec4 ambient = light_amb * mat_amb;\n"
                             "float lambert = dot (N, L);\n"
                             "if (lambert > 0.0)\n"
                             "{\n"
                             "vec4 diffuse = light_diff * mat_diff;\n"
                             "vec4 spec = light_spec * mat_spec;\n"
                             "final_color = ambient;\n"
                             "final_color += diffuse * lambert;\n"
                             "vec3 E = normalize (eye);\n"
                             "vec3 R = reflect (-L, N);\n"
                             "float spec_factor = pow (max (dot (R, E), 0.0),\
                                                        1000.0);\n"
                             "final_color += spec * spec_factor;\n"
                             "}\n"
                             "cg_color_out = final_color;\n");

    cg_pipeline_add_snippet(pipeline, snippet);
    cg_object_unref(snippet);

    location = cg_pipeline_get_uniform_location(pipeline, "light_pos");
    cg_pipeline_set_uniform_float(pipeline, location, 3, 1, light_pos);
    location = cg_pipeline_get_uniform_location(pipeline, "light_amb");
    cg_pipeline_set_uniform_float(pipeline, location, 4, 1, light_amb);
    location = cg_pipeline_get_uniform_location(pipeline, "light_diff");
    cg_pipeline_set_uniform_float(pipeline, location, 4, 1, light_diff);
    location = cg_pipeline_get_uniform_location(pipeline, "light_spec");
    cg_pipeline_set_uniform_float(pipeline, location, 4, 1, light_spec);
    location = cg_pipeline_get_uniform_location(pipeline, "mat_amb");
    cg_pipeline_set_uniform_float(pipeline, location, 4, 1, mat_amb);
    location = cg_pipeline_get_uniform_location(pipeline, "mat_diff");
    cg_pipeline_set_uniform_float(pipeline, location, 4, 1, mat_diff);
    location = cg_pipeline_get_uniform_location(pipeline, "mat_spec");
    cg_pipeline_set_uniform_float(pipeline, location, 4, 1, mat_spec);

    cg_depth_state_init(&depth_state);
    cg_depth_state_set_test_enabled(&depth_state, true);
    cg_pipeline_set_depth_state(pipeline, &depth_state, NULL);

    primitive = rig_mesh_get_primitive(shell, mesh);

    if (width > height)
        model_scale = width;
    else
        model_scale = height;

    if (rec_scale > model_scale)
        scale_facor = rec_scale / model_scale;

    if (model->max_x < 0)
        translate_x = -1 * (width * 0.5) - model->min_x;
    else if (model->min_x > 0)
        translate_x = model->min_x - (-1 * (width * 0.5));

    if (model->max_y < 0)
        translate_y = -1 * (height * 0.5) - model->min_y;
    else if (model->min_y > 0)
        translate_y = model->min_y - (-1 * (height * 0.5));

    if (model->max_z < 0)
        translate_z = -1 * (length * 0.5) - model->min_z;
    else if (model->min_z > 0)
        translate_z = model->min_z - (-1 * (length * 0.5));

    cg_framebuffer_clear4f(
        frame_buffer, CG_BUFFER_BIT_COLOR | CG_BUFFER_BIT_DEPTH, 0, 0, 0, 0);

    cg_framebuffer_translate(
        frame_buffer, tex_width / 2.0, tex_height / 2.0, 0);
    cg_framebuffer_push_matrix(frame_buffer);
    cg_framebuffer_translate(
        frame_buffer, translate_x, translate_y, translate_z);
    cg_framebuffer_scale(frame_buffer, scale_facor, scale_facor, scale_facor);
    cg_primitive_draw(primitive, frame_buffer, pipeline);
    cg_framebuffer_pop_matrix(frame_buffer);

    cg_object_unref(primitive);
    cg_object_unref(pipeline);
    cg_object_unref(frame_buffer);

    rut_object_unref(model);

    return thumbnail;
}

static const char *
get_extension(const char *path)
{
    const char *ext = strrchr(path, '.');
    return ext ? ext + 1 : NULL;
}

bool
rig_file_is_asset(const char *filename, const char *mime_type)
{
    const char *ext;

    if (mime_type) {
        if (strncmp(mime_type, "image/", 6) == 0)
            return true;
        else if (strncmp(mime_type, "video/", 6) == 0)
            return true;
        else if (strcmp(mime_type, "application/x-font-ttf") == 0)
            return true;
    }

    ext = get_extension(filename);
    if (ext && strcmp(ext, "ply") == 0)
        return true;

    return false;
}

/* XXX: path should be relative to assets_location and should
 * have been normalized. */
static c_llist_t *
infer_asset_tags(const char *path,
                 const char *mime_type)
{
    char *dir = c_path_get_dirname(path);
    char *basename = c_path_get_basename(path);
    char *tmp;
    const char *ext;
    c_llist_t *inferred_tags = NULL;
    char *tokenizer;
    char *subdir;

    ext = get_extension(basename);
    if (ext && strcmp(ext, "ply") == 0) {
        inferred_tags =
            c_llist_prepend(inferred_tags, (char *)c_intern_string("ply"));
        inferred_tags =
            c_llist_prepend(inferred_tags, (char *)c_intern_string("mesh"));
        inferred_tags =
            c_llist_prepend(inferred_tags, (char *)c_intern_string("model"));
        inferred_tags =
            c_llist_prepend(inferred_tags, (char *)c_intern_string("geometry"));
        inferred_tags =
            c_llist_prepend(inferred_tags, (char *)c_intern_string("geom"));
    }
    c_free(basename);

    tmp = c_strdupa(dir);
    for (subdir = strtok_r(tmp, C_DIR_SEPARATOR_S, &tokenizer);
         subdir;
         subdir = strtok_r(NULL, C_DIR_SEPARATOR_S, &tokenizer)) {

        inferred_tags =
            c_llist_prepend(inferred_tags, (char *)c_intern_string(subdir));
    }

    c_free(dir);
    dir = NULL;

    if (mime_type) {
        if (strncmp(mime_type, "image/", 6) == 0)
            inferred_tags =
                c_llist_prepend(inferred_tags, (char *)c_intern_string("image"));
        else if (strncmp(mime_type, "video/", 6) == 0)
            inferred_tags =
                c_llist_prepend(inferred_tags, (char *)c_intern_string("video"));
        else if (strcmp(mime_type, "application/x-font-ttf") == 0)
            inferred_tags =
                c_llist_prepend(inferred_tags, (char *)c_intern_string("font"));
    }

    if (rut_util_find_tag(inferred_tags, "image")) {
        inferred_tags =
            c_llist_prepend(inferred_tags, (char *)c_intern_string("img"));
    }

    if (rut_util_find_tag(inferred_tags, "image") ||
        rut_util_find_tag(inferred_tags, "video")) {
        inferred_tags =
            c_llist_prepend(inferred_tags, (char *)c_intern_string("texture"));

        if (rut_util_find_tag(inferred_tags, "normal-maps")) {
            inferred_tags =
                c_llist_prepend(inferred_tags, (char *)c_intern_string("map"));
            inferred_tags = c_llist_prepend(
                inferred_tags, (char *)c_intern_string("normal-map"));
            inferred_tags = c_llist_prepend(inferred_tags,
                                           (char *)c_intern_string("bump-map"));
        } else if (rut_util_find_tag(inferred_tags, "alpha-masks")) {
            inferred_tags = c_llist_prepend(
                inferred_tags, (char *)c_intern_string("alpha-mask"));
            inferred_tags =
                c_llist_prepend(inferred_tags, (char *)c_intern_string("mask"));
        }
    }

    return inferred_tags;
}

rig_asset_t *
asset_new_from_file(rig_engine_t *engine,
                    rig_asset_type_t type,
                    const char *path,
                    const c_llist_t *inferred_tags,
                    rig_asset_type_t type)
{
    rig_asset_t *asset =
        rut_object_alloc0(rig_asset_t, &rig_asset_type, _rig_asset_init_type);
    const char *real_path;
    char *full_path;
    const c_llist_t *inferred_tags;
    bool is_video;

#ifndef __ANDROID__
    if (type == RIG_ASSET_TYPE_BUILTIN) {
        full_path = rut_find_data_file(path);
        if (full_path == NULL)
            full_path = c_strdup(path);
    } else
        full_path = c_build_filename(shell->assets_location, path, NULL);
    real_path = full_path;
#else
    real_path = path;
#endif

    asset->engine = engine;

    asset->type = type;

    rig_asset_set_inferred_tags(asset, inferred_tags);
    is_video = rut_util_find_tag(inferred_tags, "video");

    c_list_init(&asset->thumbnail_cb_list);

    switch (type) {
    case RIG_ASSET_TYPE_BUILTIN:
    case RIG_ASSET_TYPE_TEXTURE:
    case RIG_ASSET_TYPE_NORMAL_MAP:
    case RIG_ASSET_TYPE_ALPHA_MASK: {
        c_error_t *error = NULL;

        if (is_video)
            asset->thumbnail = rut_load_texture(engine->shell, real_path, &error);

        if (!asset->thumbnail) {
            rut_object_free(rig_asset_t, asset);
            c_warning("Failed to load asset texture: %s", error->message);
            c_error_free(error);
            asset = NULL;
            goto DONE;
        }

        break;
    }
    case RIG_ASSET_TYPE_MESH: {
        rut_ply_attribute_status_t padding_status[C_N_ELEMENTS(ply_attributes)];
        c_error_t *error = NULL;

        asset->mesh = rut_mesh_new_from_ply(engine->shell,
                                            real_path,
                                            ply_attributes,
                                            C_N_ELEMENTS(ply_attributes),
                                            padding_status,
                                            &error);

        if (!asset->mesh) {
            rut_object_free(rig_asset_t, asset);
            c_warning("could not load model %s: %s", path, error->message);
            c_error_free(error);
            asset = NULL;
            goto DONE;
        }

        if (padding_status[1] == RUT_PLY_ATTRIBUTE_STATUS_PADDED)
            asset->has_normals = false;
        else
            asset->has_normals = true;

        if (padding_status[2] == RUT_PLY_ATTRIBUTE_STATUS_PADDED)
            asset->has_tex_coords = false;
        else
            asset->has_tex_coords = true;

        asset->thumbnail = generate_mesh_thumbnail(asset);

        break;
    }
    case RIG_ASSET_TYPE_FONT: {
        c_error_t *error = NULL;
        asset->thumbnail =
            rut_load_texture(engine->shell, rut_find_data_file("fonts.png"), &error);
        if (!asset->thumbnail) {
            rut_object_free(rig_asset_t, asset);
            asset = NULL;
            c_warning("Failed to load font icon: %s", error->message);
            c_error_free(error);
            goto DONE;
        }

        break;
    }
    }
    asset->path = c_strdup(path);

// rut_introspectable_init (asset);

DONE:

#ifndef __ANDROID__
    c_free(full_path);
#endif

    return asset;
}

rig_asset_t *
rig_asset_new_builtin(rig_engine_t *engine, const char *icon_path)
{
    return asset_new_from_file(engine, RIG_ASSET_TYPE_BUILTIN, path, NULL);
}

rig_asset_t *
rig_asset_new_from_file(rig_engine_t *engine,
                        const char *path,
                        const char *mime_type,
                        rut_exception_t **e)
{
    c_llist_t *inferred_tags = NULL;
    rig_asset_t *asset = NULL;

    inferred_tags = infer_asset_tags(path, mime_type);

    if (rut_util_find_tag(inferred_tags, "image") ||
        rut_util_find_tag(inferred_tags, "video")) {
        if (rut_util_find_tag(inferred_tags, "normal-maps"))
            asset = asset_new_from_file(engine, RIG_ASSET_TYPE_NORMAL_MAP, path, inferred_tags);
        else if (rut_util_find_tag(inferred_tags, "alpha-masks"))
            asset = asset_new_from_file(engine, RIG_ASSET_TYPE_ALPHA_MASK, path, inferred_tags);
        else
            asset = asset_new_from_file(engine, RIG_ASSET_TYPE_TEXTURE, path, inferred_tags);
    } else if (rut_util_find_tag(inferred_tags, "ply"))
        asset = asset_new_from_file(engine, RIG_ASSET_TYPE_MESH, path, inferred_tags);
    else if (rut_util_find_tag(inferred_tags, "font"))
        asset = asset_new_from_file(engine, RIG_ASSET_TYPE_FONT, path, inferred_tags);

    c_llist_free(inferred_tags);

    if (!asset) {
        /* TODO: make the constructors above handle throwing exceptions */
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "Failed to instantiate asset");
    }

    return asset;
}


static c_llist_t *
copy_tags(const c_llist_t *tags)
{
    const c_llist_t *l;
    c_llist_t *copy = NULL;
    for (l = tags; l; l = l->next) {
        const char *tag = c_intern_string(l->data);
        copy = c_llist_prepend(copy, (char *)tag);
    }
    return copy;
}

void
rig_asset_set_inferred_tags(rig_asset_t *asset,
                            const c_llist_t *inferred_tags)
{
    asset->inferred_tags =
        c_llist_concat(asset->inferred_tags, copy_tags(inferred_tags));
}

const c_llist_t *
rig_asset_get_inferred_tags(rig_asset_t *asset)
{
    return asset->inferred_tags;
}

bool
rig_asset_has_tag(rig_asset_t *asset, const char *tag)
{
    c_llist_t *l;

    for (l = asset->inferred_tags; l; l = l->next)
        if (strcmp(tag, l->data) == 0)
            return true;
    return false;
}

void
rig_asset_add_inferred_tag(rig_asset_t *asset, const char *tag)
{
    asset->inferred_tags =
        c_llist_prepend(asset->inferred_tags, (char *)c_intern_string(tag));
}

bool
rig_asset_needs_thumbnail(rig_asset_t *asset)
{
    if (strncmp(asset->mime_type, "video/", 6) == 0)
        return true;
    else
        return false;
}

rut_closure_t *
rig_asset_thumbnail(rig_asset_t *asset,
                    rut_thumbnail_callback_t ready_callback,
                    void *user_data,
                    rut_closure_destroy_callback_t destroy_cb)
{
#ifdef USE_GSTREAMER
    rut_closure_t *closure;

    c_return_val_if_fail(rig_asset_needs_thumbnail(asset), NULL);

    closure = rut_closure_list_add_FIXME(
        &asset->thumbnail_cb_list, ready_callback, user_data, destroy_cb);
    generate_video_thumbnail(asset);

    /* Make sure the thumnail wasn't simply generated synchronously to
     * make sure the closure is still valid. */
    c_warn_if_fail(!c_list_empty(&asset->thumbnail_cb_list));

    return closure;
#else
    c_return_val_if_fail(rig_asset_needs_thumbnail(asset), NULL);
    c_error("FIXME: add non gstreamer based video thumbnailing support");
#endif
}

cg_texture_t *
rig_asset_get_thumbnail(rig_asset_t *asset)
{
    return asset->thumbnail;
}


#endif /* RIG_EDITOR_ENABLED */
