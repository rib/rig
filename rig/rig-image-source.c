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

#include <config.h>

#include <cogl/cogl.h>
#include <cogl/cogl-webgl.h>

#include "rig-image-source.h"

#ifdef USE_GDK_PIXBUF
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif


struct _rig_image_source_t {
    rut_object_base_t _base;

    rig_frontend_t *frontend;

    cg_texture_t *texture;

#ifdef USE_GSTREAMER
    CgGstVideoSink *sink;
    GstElement *pipeline;
    GstElement *bin;
#endif

    bool is_video;

    int first_layer;
    bool default_sample;

    c_list_t changed_cb_list;
    c_list_t ready_cb_list;
};

typedef struct _image_source_wrappers_t {
    cg_snippet_t *image_source_vertex_wrapper;
    cg_snippet_t *image_source_fragment_wrapper;

    cg_snippet_t *video_source_vertex_wrapper;
    cg_snippet_t *video_source_fragment_wrapper;
} image_source_wrappers_t;

static void
destroy_source_wrapper(void *data)
{
    image_source_wrappers_t *wrapper = data;

    if (wrapper->image_source_vertex_wrapper)
        cg_object_unref(wrapper->image_source_vertex_wrapper);
    if (wrapper->image_source_vertex_wrapper)
        cg_object_unref(wrapper->image_source_vertex_wrapper);

    if (wrapper->video_source_vertex_wrapper)
        cg_object_unref(wrapper->video_source_vertex_wrapper);
    if (wrapper->video_source_fragment_wrapper)
        cg_object_unref(wrapper->video_source_fragment_wrapper);

    c_slice_free(image_source_wrappers_t, wrapper);
}

void
_rig_init_image_source_wrappers_cache(rig_frontend_t *frontend)
{
    frontend->image_source_wrappers =
        c_hash_table_new_full(c_direct_hash,
                              c_direct_equal,
                              NULL, /* key destroy */
                              destroy_source_wrapper);
}

void
_rig_destroy_image_source_wrappers(rig_frontend_t *frontend)
{
    c_hash_table_destroy(frontend->image_source_wrappers);
}

static image_source_wrappers_t *
get_image_source_wrappers(rig_frontend_t *frontend,
                          int layer_index)
{
    image_source_wrappers_t *wrappers = c_hash_table_lookup(
        frontend->image_source_wrappers, C_UINT_TO_POINTER(layer_index));
    char *wrapper;

    if (wrappers)
        return wrappers;

    wrappers = c_slice_new0(image_source_wrappers_t);

    /* XXX: Note: we use texture2D() instead of the
     * cg_texture_lookup%i wrapper because the _GLOBALS hook comes
     * before the _lookup functions are emitted by Cogl */
    wrapper = c_strdup_printf("vec4\n"
                              "rig_image_source_sample%d(vec2 UV)\n"
                              "{\n"
                              "#if __VERSION__ >= 130\n"
                              "  return texture(cg_sampler%d, UV);\n"
                              "#else\n"
                              "  return texture2D(cg_sampler%d, UV);\n"
                              "#endif\n"
                              "}\n",
                              layer_index,
                              layer_index,
                              layer_index);

    wrappers->image_source_vertex_wrapper =
        cg_snippet_new(CG_SNIPPET_HOOK_VERTEX_GLOBALS, wrapper, NULL);
    wrappers->image_source_fragment_wrapper =
        cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT_GLOBALS, wrapper, NULL);

    c_free(wrapper);

    wrapper = c_strdup_printf("vec4\n"
                              "rig_image_source_sample%d (vec2 UV)\n"
                              "{\n"
                              "  return cg_gst_sample_video%d (UV);\n"
                              "}\n",
                              layer_index,
                              layer_index);

    wrappers->video_source_vertex_wrapper =
        cg_snippet_new(CG_SNIPPET_HOOK_VERTEX_GLOBALS, wrapper, NULL);
    wrappers->video_source_fragment_wrapper =
        cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT_GLOBALS, wrapper, NULL);

    c_free(wrapper);

    c_hash_table_insert(frontend->image_source_wrappers,
                        C_UINT_TO_POINTER(layer_index), wrappers);

    return wrappers;
}

#ifdef USE_GSTREAMER
static gboolean
_rig_image_source_video_loop(GstBus *bus, GstMessage *msg, void *data)
{
    rig_image_source_t *source = (rig_image_source_t *)data;
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        gst_element_seek(source->pipeline,
                         1.0,
                         GST_FORMAT_TIME,
                         GST_SEEK_FLAG_FLUSH,
                         GST_SEEK_TYPE_SET,
                         0,
                         GST_SEEK_TYPE_NONE,
                         GST_CLOCK_TIME_NONE);
        break;
    default:
        break;
    }
    return true;
}

static void
_rig_image_source_video_stop(rig_image_source_t *source)
{
    if (source->sink) {
        gst_element_set_state(source->pipeline, GST_STATE_NULL);
        gst_object_unref(source->sink);
    }
}

static void
_rig_image_source_video_play(rig_image_source_t *source,
                             rig_engine_t *engine,
                             const char *path,
                             const uint8_t *data,
                             size_t len)
{
    GstBus *bus;
    char *uri;
    char *filename = NULL;

    _rig_image_source_video_stop(source);

    source->sink = cg_gst_video_sink_new(engine->shell->cg_device);
    source->pipeline = gst_pipeline_new("renderer");
    source->bin = gst_element_factory_make("playbin", NULL);

    if (data && len)
        uri = c_strdup_printf("mem://%p:%lu", data, (unsigned long)len);
    else {
        filename = c_build_filename(engine->shell->assets_location, path, NULL);
        uri = gst_filename_to_uri(filename, NULL);
    }

    g_object_set(
        G_OBJECT(source->bin), "video-sink", GST_ELEMENT(source->sink), NULL);
    g_object_set(G_OBJECT(source->bin), "uri", uri, NULL);
    gst_bin_add(GST_BIN(source->pipeline), source->bin);

    bus = gst_pipeline_get_bus(GST_PIPELINE(source->pipeline));

    gst_element_set_state(source->pipeline, GST_STATE_PLAYING);
    gst_bus_add_watch(bus, _rig_image_source_video_loop, source);

    c_free(uri);
    if (filename)
        c_free(filename);
    gst_object_unref(bus);
}
#endif /* USE_GSTREAMER */

static void
_rig_image_source_free(void *object)
{
    rig_image_source_t *source = object;

#ifdef USE_GSTREAMER
    _rig_image_source_video_stop(source);
#endif

    if (source->texture) {
        cg_object_unref(source->texture);
        source->texture = NULL;
    }
}

rut_type_t rig_image_source_type;

void
_rig_image_source_init_type(void)
{
    rut_type_init(&rig_image_source_type, "rig_image_source_t", _rig_image_source_free);
}

#ifdef USE_GSTREAMER
static void
pipeline_ready_cb(gpointer instance, gpointer user_data)
{
    rig_image_source_t *source = (rig_image_source_t *)user_data;

    source->is_video = true;

    rut_closure_list_invoke(
        &source->ready_cb_list, rig_image_source_ready_callback_t, source);
}

static void
new_frame_cb(gpointer instance, gpointer user_data)
{
    rig_image_source_t *source = (rig_image_source_t *)user_data;

    rut_closure_list_invoke(
        &source->changed_cb_list, rig_image_source_changed_callback_t, source);
}
#endif /* USE_GSTREAMER */

#ifdef USE_GDK_PIXBUF
static cg_bitmap_t *
bitmap_new_from_pixbuf(cg_device_t *dev, GdkPixbuf *pixbuf)
{
    bool has_alpha;
    GdkColorspace color_space;
    cg_pixel_format_t pixel_format;
    int width;
    int height;
    int rowstride;
    int bits_per_sample;
    int n_channels;
    cg_bitmap_t *bmp;

    /* Get pixbuf properties */
    has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    color_space = gdk_pixbuf_get_colorspace(pixbuf);
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    bits_per_sample = gdk_pixbuf_get_bits_per_sample(pixbuf);
    n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    /* According to current docs this should be true and so
     * the translation to cogl pixel format below valid */
    c_assert(bits_per_sample == 8);

    if (has_alpha)
        c_assert(n_channels == 4);
    else
        c_assert(n_channels == 3);

    /* Translate to cogl pixel format */
    switch (color_space) {
    case GDK_COLORSPACE_RGB:
        /* The only format supported by GdkPixbuf so far */
        pixel_format =
            has_alpha ? CG_PIXEL_FORMAT_RGBA_8888 : CG_PIXEL_FORMAT_RGB_888;
        break;

    default:
        /* Ouch, spec changed! */
        g_object_unref(pixbuf);
        return false;
    }

    /* We just use the data directly from the pixbuf so that we don't
     * have to copy to a seperate buffer.
     */
    bmp = cg_bitmap_new_for_data(dev,
                                 width,
                                 height,
                                 pixel_format,
                                 rowstride,
                                 gdk_pixbuf_get_pixels(pixbuf));

    return bmp;
}

GdkPixbuf *
create_gdk_pixbuf_for_data(const uint8_t *data,
                           size_t len,
                           rut_exception_t **e)
{
    GInputStream *istream =
        g_memory_input_stream_new_from_data(data, len, NULL);
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_stream(istream, NULL, &error);

    if (!pixbuf) {
        rut_throw(e,
                  RUT_IO_EXCEPTION,
                  RUT_IO_EXCEPTION_IO,
                  "Failed to load pixbuf from data: %s",
                  error->message);
        g_error_free(error);
        return NULL;
    }

    g_object_unref(istream);

    return pixbuf;
}
#endif /* USE_GDK_PIXBUF */

#ifdef CG_HAS_WEBGL_SUPPORT

static inline bool
is_pot(unsigned int num)
{
    /* Make sure there is only one bit set */
    return (num & (num - 1)) == 0;
}

static int
next_p2(int a)
{
    int rval = 1;

    while (rval < a)
        rval <<= 1;

    return rval;
}

static void
on_webgl_image_load_cb(cg_webgl_image_t *image, void *user_data)
{
    rig_image_source_t *source = user_data;
    rut_shell_t *shell = source->frontend->engine->shell;
    cg_texture_2d_t *tex2d =
        cg_webgl_texture_2d_new_from_image(shell->cg_device, image);
    int width = cg_webgl_image_get_width(image);
    int height = cg_webgl_image_get_height(image);
    int pot_width;
    int pot_height;
    cg_error_t *error = NULL;

    if (!cg_texture_allocate(tex2d, &error)) {
        c_warning("Failed to load image source texture: %s", error->message);
        cg_error_free(error);
        return;
    }

    _c_web_console_warn("fallback to scaling image to nearest power of two...\n");

    pot_width = is_pot(width) ? width : next_p2(width);
    pot_height = is_pot(height) ? height : next_p2(height);

    /* XXX: We should warn if we hit this path, since ideally we
     * should avoid loading assets that require us to rescale on the
     * fly like this
     */
    if (pot_width != width || pot_height != height) {
        cg_texture_2d_t *pot_tex;
        cg_pipeline_t *pipeline;
        cg_framebuffer_t *fb;
        char *str;

        asprintf(&str, "pot width=%d height=%d\n", pot_width, pot_height);
        _c_web_console_warn(str);
        free(str);

        pot_tex = cg_texture_2d_new_with_size(shell->cg_device,
                                              pot_width,
                                              pot_height);
        fb = cg_offscreen_new_with_texture(pot_tex);

        if (!cg_framebuffer_allocate(fb, &error)) {
            _c_web_console_warn("failed to allocate\n");
            _c_web_console_warn(error->message);
            c_warning("Failed alloc framebuffer to re-scale image source "
                      "texture to nearest power-of-two size: %s",
                      error->message);
            cg_error_free(error);
            cg_object_unref(tex2d);
            return;
        }

        cg_framebuffer_orthographic(fb, 0, 0, pot_width, pot_height, -1, 100);

        pipeline = cg_pipeline_copy(source->frontend->default_tex2d_pipeline);
        cg_pipeline_set_layer_texture(pipeline, 0, tex2d);

        _c_web_console_warn("scale...\n");

        /* TODO: It could be good to have a fifo of image scaling work
         * to throttle how much image scaling we do per-frame */
        cg_framebuffer_draw_rectangle(fb, pipeline,
                                      0, 0, pot_width, pot_height);

        cg_object_unref(pipeline);
        cg_object_unref(fb);
        cg_object_unref(tex2d);

        source->texture = pot_tex;
    } else
        source->texture = tex2d;

    rut_closure_list_invoke(&source->ready_cb_list,
                            rig_image_source_ready_callback_t, source);
}

#endif /* CG_HAS_WEBGL_SUPPORT */

rig_image_source_t *
rig_image_source_new(rig_frontend_t *frontend,
                     const char *mime,
                     const char *path,
                     const uint8_t *data,
                     int len,
                     int natural_width,
                     int natural_height)
{
    rig_image_source_t *source = rut_object_alloc0(rig_image_source_t,
                                                   &rig_image_source_type,
                                                   _rig_image_source_init_type);
    rut_shell_t *shell = frontend->engine->shell;

    source->frontend = frontend;
    source->default_sample = true;

    c_list_init(&source->changed_cb_list);
    c_list_init(&source->ready_cb_list);

    if (strcmp(mime, "image/jpeg") || strcmp(mime, "image/png")) {

#ifdef CG_HAS_WEBGL_SUPPORT
        char *url = c_strdup_printf("assets/%s", path);
        cg_webgl_image_t *image = cg_webgl_image_new(shell->cg_device, url);

        c_free(url);

        cg_webgl_image_add_onload_callback(image,
                                           on_webgl_image_load_cb,
                                           source,
                                           NULL); /* destroy notify */

        /* Until the image has loaded... */
        source->texture = cg_object_ref(frontend->default_tex2d);

#elif defined(USE_GDK_PIXBUF)
        rut_exception_t *e = NULL;
        GdkPixbuf *pixbuf = create_gdk_pixbuf_for_data(data, len, &e);
        bool allocated;
        cg_bitmap_t *bitmap;
        cg_texture_2d_t *tex2d;
        cg_error_t *cg_error = NULL;

        if (!pixbuf) {
            source->texture = cg_object_ref(frontend->default_tex2d);
            c_warning("%s", e->message);
            rut_exception_free(e);
            return source;
        }

        bitmap = bitmap_new_from_pixbuf(shell->cg_device, pixbuf);
        tex2d = cg_texture_2d_new_from_bitmap(bitmap);

        /* Allocate now so we can simply free the data
         * TODO: allow asynchronous upload. */
        allocated = cg_texture_allocate(tex2d, &cg_error);

        cg_object_unref(bitmap);
        g_object_unref(pixbuf);

        if (!allocated) {
            source->texture = cg_object_ref(frontend->default_tex2d);
            c_warning("Failed to load Cogl texture: %s", cg_error->message);
            cg_error_free(cg_error);
            return source;
        }

        source->texture = tex2d;
#else
        source->texture = cg_object_ref(frontend->default_tex2d);
        c_warning("FIXME: Rig is missing platform support for loading image %s",
                  path);
#endif

    } else if (strncmp(mime, "video/", 6) == 0) {
#ifdef USE_GSTREAMER
        _rig_image_source_video_play(source,
                                     engine,
                                     path,
                                     data,
                                     len):
        g_signal_connect(source->sink,
                         "pipeline_ready",
                         (GCallback)pipeline_ready_cb,
                         source);
        g_signal_connect(
            source->sink, "new_frame", (GCallback)new_frame_cb, source);
#else
        c_warning("FIXME: Rig is missing video support on this platform");
        source->texture = cg_object_ref(frontend->default_tex2d);
#endif
    }

    return source;
}

rut_closure_t *
rig_image_source_add_ready_callback(rig_image_source_t *source,
                                    rig_image_source_ready_callback_t callback,
                                    void *user_data,
                                    rut_closure_destroy_callback_t destroy_cb)
{
    if (source->texture) {
        callback(source, user_data);
        return NULL;
    } else
        return rut_closure_list_add(
            &source->ready_cb_list, callback, user_data, destroy_cb);
}

cg_texture_t *
rig_image_source_get_texture(rig_image_source_t *source)
{
    return source->texture;
}

#ifdef USE_GSTREAMER
CgGstVideoSink *
rig_image_source_get_sink(rig_image_source_t *source)
{
    return source->sink;
}
#endif

bool
rig_image_source_get_is_video(rig_image_source_t *source)
{
    return source->is_video;
}

rut_closure_t *
rig_image_source_add_on_changed_callback(
    rig_image_source_t *source,
    rig_image_source_changed_callback_t callback,
    void *user_data,
    rut_closure_destroy_callback_t destroy_cb)
{
    return rut_closure_list_add(
        &source->changed_cb_list, callback, user_data, destroy_cb);
}

void
rig_image_source_set_first_layer(rig_image_source_t *source,
                                 int first_layer)
{
    source->first_layer = first_layer;
}

void
rig_image_source_set_default_sample(rig_image_source_t *source,
                                    bool default_sample)
{
    source->default_sample = default_sample;
}

void
rig_image_source_setup_pipeline(rig_image_source_t *source,
                                cg_pipeline_t *pipeline)
{
    cg_snippet_t *vertex_snippet;
    cg_snippet_t *fragment_snippet;
    image_source_wrappers_t *wrappers =
        get_image_source_wrappers(source->frontend, source->first_layer);

    if (!rig_image_source_get_is_video(source)) {
        cg_texture_t *texture = rig_image_source_get_texture(source);

        cg_pipeline_set_layer_texture(pipeline, source->first_layer, texture);

        if (!source->default_sample) {
            cg_snippet_t *snippet =
                cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
            cg_snippet_set_replace(snippet, "");
            cg_pipeline_add_layer_snippet(pipeline, source->first_layer, snippet);
            cg_object_unref(snippet);
        }

        vertex_snippet = wrappers->image_source_vertex_wrapper;
        fragment_snippet = wrappers->image_source_fragment_wrapper;
    } else {
#ifdef USE_GSTREAMER
        CgGstVideoSink *sink = rig_image_source_get_sink(source);

        cg_gst_video_sink_set_first_layer(sink, source->first_layer);
        cg_gst_video_sink_set_default_sample(sink, true);
        cg_gst_video_sink_setup_pipeline(sink, pipeline);

        vertex_snippet = wrappers->video_source_vertex_wrapper;
        fragment_snippet = wrappers->video_source_fragment_wrapper;
#else
        c_error("FIXME: missing video support for this platform");
#endif
    }

    cg_pipeline_add_snippet(pipeline, vertex_snippet);
    cg_pipeline_add_snippet(pipeline, fragment_snippet);
}

void
rig_image_source_attach_frame(rig_image_source_t *source,
                              cg_pipeline_t *pipeline)
{
    /* NB: For non-video sources we always attach the texture
     * during rig_image_source_setup_pipeline() so we don't
     * have to do anything here.
     */

    if (rig_image_source_get_is_video(source)) {
#ifdef USE_GSTREAMER
        cg_gst_video_sink_attach_frame(rig_image_source_get_sink(source),
                                       pipeline);
#else
        c_error("FIXME: missing video support for this platform");
#endif
    }
}

void
rig_image_source_get_natural_size(rig_image_source_t *source,
                                  float *width,
                                  float *height)
{
    if (rig_image_source_get_is_video(source)) {
#ifdef USE_GSTREAMER
        cg_gst_video_sink_get_natural_size(source->sink, width, height);
#else
        c_error("FIXME: missing video support for this platform");
#endif
    } else {
        cg_texture_t *texture = rig_image_source_get_texture(source);
        *width = cg_texture_get_width(texture);
        *height = cg_texture_get_height(texture);
    }
}
