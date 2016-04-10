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

#include <libnsgif.h>

#ifdef USE_UV
#include <xdgmime.h>
#endif

#include <cglib/cglib.h>
#include <cglib/cg-webgl.h>

#include <rut.h>

#include "rig-entity.h"
#include "rig-entity-inlines.h"

#include "components/rig-source.h"

#ifdef USE_GDK_PIXBUF
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#ifdef USE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

enum source_type
{
    SOURCE_TYPE_UNLOADED,

    //SOURCE_TYPE_CONST = 1,
#ifdef USE_GSTREAMER
    SOURCE_TYPE_GSTREAMER,
#endif
#ifdef USE_FFMPEG
    SOURCE_TYPE_FFMPEG,
#endif
    SOURCE_TYPE_GIF,
    SOURCE_TYPE_PNG,
    SOURCE_TYPE_JPG,
#ifdef USE_GDK_PIXBUF
    SOURCE_TYPE_PIXBUF,
#endif
#ifdef CG_HAS_WEBGL_SUPPORT
    SOURCE_TYPE_WEBGL,
#endif
};

enum {
    RIG_SOURCE_PROP_URL,
    RIG_SOURCE_PROP_RUNNING,

    RIG_SOURCE_N_PROPS,
};

enum load_status {
    LOAD_STATE_NONE,
    LOAD_STATE_MIME_QUERY,
    LOAD_STATE_READING,
    LOAD_STATE_LOADED,
    LOAD_STATE_ERROR,
};

typedef struct load_state
{
    enum load_status status;

    rut_closure_t load_idle;

    char *filename;

    xdgmime_request_t mime_req;
    uv_work_t read_req;

    char *error;
} load_state_t;

struct _rig_source_t {
    rut_object_base_t _base;

    rut_componentable_props_t component;

    enum source_type type;

    char *mime;
    char *url;

    float natural_width;
    float natural_height;

    uint8_t *data;
    int data_len;

    load_state_t load_state;

    bool changed;
    cg_texture_t *texture;

#ifdef USE_FFMPEG
    AVFormatContext *ff_fmt_ctx;
    uint8_t *ff_avio_buf;
    AVIOContext *ff_avio_ctx;
    int64_t ff_read_pos;
    int ff_video_stream;
    AVCodecContext *ff_video_codec_ctx;
    struct SwsContext *sws_ctx;
    uint8_t *ff_dst_frame_buf;
    size_t ff_dst_frame_buf_size;
    AVFrame *ff_dst_frame;
#endif

#ifdef USE_GSTREAMER
    CgGstVideoSink *sink;
    GstElement *pipeline;
    GstElement *bin;
#endif

    gif_animation gif;

    int gif_current_frame;
    double gif_current_elapsed;

    int first_layer;
    bool default_sample;

    rig_timeline_t *timeline;

    c_list_t changed_cb_list;
    c_list_t ready_cb_list;
    c_list_t error_cb_list;

    rig_introspectable_props_t introspectable;
    rig_property_t properties[RIG_SOURCE_N_PROPS];
};

typedef struct _source_wrappers_t {
    cg_snippet_t *source_vertex_wrapper;
    cg_snippet_t *source_fragment_wrapper;

    cg_snippet_t *video_source_vertex_wrapper;
    cg_snippet_t *video_source_fragment_wrapper;
} source_wrappers_t;

static void
_source_load_progress(rig_source_t *source);


static rig_property_spec_t _rig_source_prop_specs[] = {
    { .name = "url",
      .type = RUT_PROPERTY_TYPE_TEXT,
      .getter.text_type = rig_source_get_url,
      .setter.text_type = rig_source_set_url,
      .nick = "URL",
      .blurb = "URL for source data",
      .flags = RUT_PROPERTY_FLAG_READWRITE |
          RUT_PROPERTY_FLAG_EXPORT_FRONTEND,
      .animatable = true },
    { .name = "running",
      .nick = "Running",
      .blurb = "The timeline progressing over time",
      .type = RUT_PROPERTY_TYPE_BOOLEAN,
      .getter.boolean_type = rig_source_get_running,
      .setter.boolean_type = rig_source_set_running,
      .flags = RUT_PROPERTY_FLAG_READWRITE, },
    { 0 }
};

static void
destroy_source_wrapper(void *data)
{
    source_wrappers_t *wrapper = data;

    if (wrapper->source_vertex_wrapper)
        cg_object_unref(wrapper->source_vertex_wrapper);
    if (wrapper->source_vertex_wrapper)
        cg_object_unref(wrapper->source_vertex_wrapper);

    if (wrapper->video_source_vertex_wrapper)
        cg_object_unref(wrapper->video_source_vertex_wrapper);
    if (wrapper->video_source_fragment_wrapper)
        cg_object_unref(wrapper->video_source_fragment_wrapper);

    c_slice_free(source_wrappers_t, wrapper);
}

void
_rig_init_source_wrappers_cache(rig_frontend_t *frontend)
{
    frontend->source_wrappers =
        c_hash_table_new_full(c_direct_hash,
                              c_direct_equal,
                              NULL, /* key destroy */
                              destroy_source_wrapper);
}

void
_rig_destroy_source_wrappers(rig_frontend_t *frontend)
{
    c_hash_table_destroy(frontend->source_wrappers);
}

#ifdef USE_GSTREAMER
static gboolean
gst_source_loop_cb(GstBus *bus, GstMessage *msg, void *data)
{
    rig_source_t *source = (rig_source_t *)data;
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
gst_source_stop(rig_source_t *source)
{
    if (source->sink) {
        gst_element_set_state(source->pipeline, GST_STATE_NULL);
        gst_object_unref(source->sink);
    }
}

static void
gst_source_start(rig_source_t *source)
{
    rig_engine_t *engine = rig_component_props_get_engine(&source->component);
    GstBus *bus;

    gst_source_stop(source);

    source->sink = cg_gst_video_sink_new(engine->shell->cg_device);
    source->pipeline = gst_pipeline_new("renderer");
    source->bin = gst_element_factory_make("playbin", NULL);

    g_object_set(G_OBJECT(source->bin), "video-sink",
                 GST_ELEMENT(source->sink), NULL);

    if (!source->url && source->data && source->data_len) {
        char *url = c_strdup_printf("mem://%p:%lu",
                                    source->data,
                                    (unsigned long)source->data_len);
        g_object_set(G_OBJECT(source->bin), "uri", url, NULL);
    } else {
        g_object_set(G_OBJECT(source->bin), "uri", source->url, NULL);
    }

    gst_bin_add(GST_BIN(source->pipeline), source->bin);

    bus = gst_pipeline_get_bus(GST_PIPELINE(source->pipeline));

    gst_element_set_state(source->pipeline, GST_STATE_PLAYING);
    gst_bus_add_watch(bus, gst_source_loop_cb, source);

    gst_object_unref(bus);
}

static CgGstVideoSink *
gst_source_get_sink(rig_source_t *source)
{
    return source->sink;
}

static void
gst_pipeline_ready_cb(gpointer instance, gpointer user_data)
{
    rig_source_t *source = (rig_source_t *)user_data;

    source->state.status = LOAD_STATE_LOADED;
    source->changed = true;

    rut_closure_list_invoke(
        &source->ready_cb_list, rig_source_ready_callback_t, source);
}

static void
gst_new_frame_cb(gpointer instance, gpointer user_data)
{
    rig_source_t *source = (rig_source_t *)user_data;

    source->changed = true;

    rut_closure_list_invoke(
        &source->changed_cb_list, rig_source_changed_callback_t, source);
}
#endif /* USE_GSTREAMER */

static void
destroy_load_state(rig_source_t *source)
{
    load_state_t *state = &source->load_state;

    switch (state->status) {
    case LOAD_STATE_NONE:
        break;
    case LOAD_STATE_READING:
        uv_cancel((uv_req_t *)&state->read_req);
        break;
    case LOAD_STATE_MIME_QUERY:
        xdgmime_request_cancel(&state->mime_req);
        break;
    case LOAD_STATE_ERROR:
        c_free(state->error);
        state->error = NULL;
        break;
    case LOAD_STATE_LOADED:
        break;
    }

    if (state->filename) {
        c_free(state->filename);
        state->filename = NULL;
    }
}

static void
_rig_source_free(void *object)
{
    rig_source_t *source = object;

    rut_closure_list_remove_all(&source->ready_cb_list);
    rut_closure_list_remove_all(&source->changed_cb_list);
    rut_closure_list_remove_all(&source->error_cb_list);

    destroy_load_state(source);

#ifdef USE_GSTREAMER
    if (source->type == SOURCE_TYPE_GSTREAMER)
        gst_source_stop(source);
#endif

    if (source->texture) {
        cg_object_unref(source->texture);
        source->texture = NULL;
    }

    if (source->data)
        c_free(source->data);

    if (source->url)
        c_free(source->url);

    if (source->mime)
        c_free(source->mime);

    rig_introspectable_destroy(source);

    rut_object_free(rig_source_t, source);
}

static rut_object_t *
_rig_source_copy(rut_object_t *object)
{
    rig_source_t *source = object;
    rig_engine_t *engine = rig_component_props_get_engine(&source->component);
    rig_source_t *copy = rig_source_new(engine,
                                        source->mime,
                                        source->url,
                                        source->data,
                                        source->data_len,
                                        source->natural_width,
                                        source->natural_height);

    return copy;
}

rut_type_t rig_source_type;

static void
_rig_source_init_type(void)
{
    static rut_componentable_vtable_t componentable_vtable = {
        .copy = _rig_source_copy
    };

    rut_type_t *type = &rig_source_type;
#define TYPE rig_source_t

    rut_type_init(type, C_STRINGIFY(TYPE), _rig_source_free);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_COMPONENTABLE,
                       offsetof(TYPE, component),
                       &componentable_vtable);
    rut_type_add_trait(type,
                       RUT_TRAIT_ID_INTROSPECTABLE,
                       offsetof(TYPE, introspectable),
                       NULL); /* no implied vtable */

#undef TYPE

}

static void
_source_set_texture(rig_source_t *source, cg_texture_2d_t *texture)
{
    c_return_if_fail(texture != NULL);

    if (source->texture)
        cg_object_unref(source->texture);

    source->texture = cg_object_ref(texture);
    source->changed = true;
}

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
    rig_source_t *source = user_data;
    load_state_t *state = &source->load_state;
    rig_engine_t *engine = rig_component_props_get_engine(&source->component);
    rig_frontend_t *frontend = engine->frontend;
    rut_shell_t *shell = engine->shell;
    cg_texture_2d_t *tex2d =
        cg_webgl_texture_2d_new_from_image(shell->cg_device, image);
    int width = cg_webgl_image_get_width(image);
    int height = cg_webgl_image_get_height(image);
    int pot_width;
    int pot_height;
    cg_error_t *error = NULL;

    if (!cg_texture_allocate(tex2d, &error)) {
        state->status = LOAD_STATE_ERROR;
        state->error = c_strdup_printf("Failed allocate texture: %s", error->message);
        cg_error_free(error);
        return;
    }

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

        _c_web_console_warn("fallback to scaling image to nearest power of two...\n");

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

            state->status = LOAD_STATE_ERROR;
            state->error = c_strdup_printf("Failed alloc framebuffer to "
                                           "re-scale image source "
                                           "texture to nearest power-of-two "
                                           "size: %s",
                                           error->message);
            cg_error_free(error);
            cg_object_unref(tex2d);
            return;
        }

        cg_framebuffer_orthographic(fb, 0, 0, pot_width, pot_height, -1, 100);

        pipeline = cg_pipeline_copy(frontend->default_tex2d_pipeline);
        cg_pipeline_set_layer_texture(pipeline, 0, tex2d);

        _c_web_console_warn("scale...\n");

        /* TODO: It could be good to have a fifo of image scaling work
         * to throttle how much image scaling we do per-frame */
        cg_framebuffer_draw_rectangle(fb, pipeline,
                                      0, 0, pot_width, pot_height);

        cg_object_unref(pipeline);
        cg_object_unref(fb);
        cg_object_unref(tex2d);

        _source_set_texture(source, pot_tex);
    } else
        _source_set_texture(source, tex2d);

    state->status = LOAD_STATE_LOADED;

    rut_closure_list_invoke(&source->ready_cb_list,
                            rig_source_ready_callback_t, source);
}

#endif /* CG_HAS_WEBGL_SUPPORT */

struct gif_bitmap
{
    int width;
    int height;
    bool opaque;
    uint8_t *buf;
};

static void *
bitmap_create(gif_animation *gif, int width, int height)
{
    struct gif_bitmap *bmp = c_new0(struct gif_bitmap, 1);

    bmp->width = width;
    bmp->height = height;
    bmp->opaque = false;
    bmp->buf = c_malloc(width * height * 4);

    return bmp;
}

static void
bitmap_set_opaque(gif_animation *gif, void *bitmap, bool opaque)
{
    struct gif_bitmap *bmp = bitmap;

    c_return_if_fail(bitmap);

    bmp->opaque = opaque;
}

static bool
bitmap_test_opaque(gif_animation *gif, void *bitmap)
{
    c_return_val_if_fail(bitmap, false);

    return false;
}

static unsigned char *
bitmap_get_buffer(gif_animation *gif, void *bitmap)
{
    c_return_val_if_fail(bitmap, NULL);

    return ((struct gif_bitmap *)bitmap)->buf;
}

static void
bitmap_destroy(gif_animation *gif, void *bitmap)
{
    struct gif_bitmap *bmp = bitmap;

    c_return_if_fail(bitmap);

    c_free(bmp->buf);
    c_free(bmp);
}

static void
bitmap_modified(gif_animation *gif, void *bitmap)
{
	(void) bitmap;  /* unused */
	assert(bitmap);
	return;
}

static gif_bitmap_callback_vt bitmap_callbacks = {
    bitmap_create,
    bitmap_destroy,
    bitmap_get_buffer,
    bitmap_set_opaque,
    bitmap_test_opaque,
    bitmap_modified
};

static void
_source_idle_load_cb(rig_source_t *source)
{
    rig_engine_t *engine = rig_component_props_get_engine(&source->component);
    rut_shell_t *shell = engine->shell;

    rut_poll_shell_remove_idle(shell, &source->load_state.load_idle);
    _source_load_progress(source);
}

rig_source_t *
rig_source_new(rig_engine_t *engine,
               const char *mime,
               const char *url,
               const uint8_t *data,
               int len,
               int natural_width,
               int natural_height)
{
    rig_source_t *source = rut_object_alloc0(rig_source_t,
                                             &rig_source_type,
                                             _rig_source_init_type);

    source->component.type = RUT_COMPONENT_TYPE_SOURCE;
    source->component.parented = false;
    source->component.engine = engine;

    rig_introspectable_init(source, _rig_source_prop_specs, source->properties);

    source->default_sample = true;

    if (mime)
        source->mime = strdup(mime);

    if (url)
        source->url = strdup(url);

    if (data) {
        source->data = c_memdup(data, len);
        source->data_len = len;
    }

    rut_closure_init(&source->load_state.load_idle,
                     _source_idle_load_cb, source);

    source->natural_width = natural_width;
    source->natural_height = natural_height;

    c_list_init(&source->changed_cb_list);
    c_list_init(&source->ready_cb_list);
    c_list_init(&source->error_cb_list);

    if (engine->frontend) {
        rut_shell_t *shell = engine->shell;

        /* Until something else is loaded... */
        _source_set_texture(source, engine->frontend->default_tex2d);
        rut_poll_shell_add_idle(shell, &source->load_state.load_idle);
    }

    return source;
}

static void
mime_request_cb(xdgmime_request_t *req, const char *mime_type)
{
    rig_source_t *source = req->data;
    load_state_t *state = &source->load_state;

    source->mime = c_strdup(mime_type);

    xdgmime_request_cancel(&state->mime_req);

    _source_load_progress(source);
}

static void
read_file_contents_cb(uv_work_t *req)
{
    rig_source_t *source = req->data;
    load_state_t *state = &source->load_state;
    c_error_t *error = NULL;
    char *data;
    unsigned long len;

    if (!c_file_get_contents(state->filename,
                             &data,
                             &len,
                             &error))
    {
        state->error = c_strdup(error->message);
        c_error_free(error);
    }

    source->data = (uint8_t *)data;
    source->data_len = len;
}

static void
finished_read_file_contents_cb(uv_work_t *req, int status)
{
    _source_load_progress(req->data);
}

static char *
get_url_filename(rut_shell_t *shell, const char *url)
{
    if (strncmp(url, "file://", 7) == 0)
        return c_strdup(url + 7);
    else if (strncmp(url, "asset://", 8) == 0)
        return c_build_filename(shell->assets_location, url + 8, NULL);
    else
        return NULL;
}

#ifdef USE_FFMPEG
static int
source_avio_read_packet_cb(void *user_data,
                           uint8_t *buf,
                           int buf_size)
{
    rig_source_t *source = user_data;
    size_t rem = source->data_len - source->ff_read_pos;
    size_t size = MIN(rem, buf_size);

    memcpy(buf, source->data + source->ff_read_pos, size);
    source->ff_read_pos += size;

    return size;
}

static int64_t
source_avio_seek_cb(void *user_data, int64_t offset, int whence)
{
    rig_source_t *source = user_data;

    whence &= ~AVSEEK_FORCE;

    switch (whence) {
    case AVSEEK_SIZE:
        return source->data_len;
    case SEEK_END:
        source->ff_read_pos = source->data_len - 1;
        break;
    case SEEK_CUR:
        source->ff_read_pos += offset;
        break;
    case SEEK_SET:
        source->ff_read_pos = offset;
        break;
    default:
        return -1;
    }

    if (source->ff_read_pos < 0)
        source->ff_read_pos = 0;
    else if (source->ff_read_pos >= source->data_len)
        source->ff_read_pos = source->data_len - 1;

    return source->ff_read_pos;
}

static void
cleanup_ffmpeg_state(rig_source_t *source)
{
    if (source->ff_fmt_ctx)
        avformat_close_input(&source->ff_fmt_ctx);

    if (source->ff_avio_ctx) {
        av_freep(&source->ff_avio_ctx->buffer);
        av_freep(&source->ff_avio_ctx);
    }

    /* apparently it's possible that ff_avio_ctx->buffer might
     * get replaced so don't asume we've freed the buffer
     * we allocated yet...*/
    av_freep(&source->ff_avio_buf);

    source->ff_read_pos = 0;
}
#endif

static void
_source_load_progress(rig_source_t *source)
{
    load_state_t *state = &source->load_state;
    rig_engine_t *engine = rig_component_props_get_engine(&source->component);
    rut_shell_t *shell = engine->shell;
    rig_frontend_t *frontend = engine->frontend;

    c_return_if_fail(frontend);

    if (state->status == LOAD_STATE_ERROR)
        return;

    if (!source->mime) {
#ifdef USE_UV
        char *filename = NULL;

        state->status = LOAD_STATE_MIME_QUERY;

        if (source->url)
            filename = get_url_filename(shell, source->url);

        /* TODO: support mime type queries just based on a data
         * pointer + len */
        if (filename) {
            state->mime_req.data = source;
            xdgmime_request_init(shell->uv_loop, &state->mime_req);
            xdgmime_request_start(&state->mime_req,
                                  filename,
                                  mime_request_cb);

            c_free(filename);
            return;
        } else
#endif
        {
            state->error = c_strdup_printf("Can't determine source mime type");
            state->status = LOAD_STATE_ERROR;

            rut_closure_list_invoke(&source->error_cb_list,
                                    rig_source_error_callback_t,
                                    source,
                                    state->error);
            return;
        }
    }

    if (!source->data) {
        state->status = LOAD_STATE_READING;

        state->filename = get_url_filename(shell, source->url);

        if (!state->filename) {
            state->status = LOAD_STATE_ERROR;
            state->error = c_strdup("No file to read source data from");

            rut_closure_list_invoke(&source->error_cb_list,
                                    rig_source_error_callback_t,
                                    source,
                                    state->error);
        }

        state->read_req.data = source;
        uv_queue_work(shell->uv_loop,
                      &state->read_req,
                      read_file_contents_cb,
                      finished_read_file_contents_cb);
        return;
    }

#ifdef CG_HAS_WEBGL_SUPPORT
    if (strncmp(mime, "image/", 6) == 0) {
        char *filename = get_url_filename(source->url);
        cg_webgl_image_t *image;
        
        if (filename) {
            char *remote_path = c_strdup_printf("assets/%s", filename);

            image = cg_webgl_image_new(shell->cg_device, remote_path);
            c_free(remote_path);
        } else
            image = cg_webgl_image_new(shell->cg_device, source->url);

        cg_webgl_image_add_onload_callback(image,
                                           on_webgl_image_load_cb,
                                           source,
                                           NULL); /* destroy notify */

        source->type = SOURCE_TYPE_WEBGL;
    } else
#elif defined(USE_GDK_PIXBUF)
    if (strncmp(mime, "image/", 6) == 0) {
        rut_exception_t *e = NULL;
        GdkPixbuf *pixbuf = create_gdk_pixbuf_for_data(source->data,
                                                       source->data_len,
                                                       &e);
        bool allocated;
        cg_bitmap_t *bitmap;
        cg_texture_2d_t *tex2d;
        cg_error_t *cg_error = NULL;

        if (!pixbuf) {
            source->texture = cg_object_ref(frontend->default_tex2d);
            c_warning("%s", e->message);

            state->status = LOAD_STATED_ERROR;
            state->error = c_strdup(e->message);

            rut_exception_free(e);
            return;
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
            c_warning("Failed to load source texture: %s", cg_error->message);

            state->status = LOAD_STATED_ERROR;
            state->error = c_strdup(cg_error->message);

            cg_error_free(cg_error);
            return;
        }

        _source_set_texture(source, tex2d);
        source->type = SOURCE_TYPE_PIXBUF;
        state->status = LOAD_STATE_LOADED
    } else
#else
    if (strcmp(source->mime, "image/gif") == 0) {
        gif_result code;

	gif_create(&source->gif, &bitmap_callbacks);

        source->gif.priv = shell;

        /* FIXME: load the GIF asynchronously */
	do {
            code = gif_initialise(&source->gif, source->data_len, source->data);
            if (code != GIF_OK && code != GIF_WORKING) {
                c_warning("failed to load GIF");

                state->status = LOAD_STATE_ERROR;
                state->error = c_strdup("failed to load GIF");
                return;
            }
        } while (code != GIF_OK);

        source->timeline = rig_timeline_new(engine, FLT_MAX);

        source->type = SOURCE_TYPE_GIF;
        state->status = LOAD_STATE_LOADED;
    } else
#endif
#ifdef USE_FFMPEG
    if (strncmp(source->mime, "video/", 6) == 0) {
        int ret;
        AVCodec *codec = NULL;
        AVDictionary *opts = NULL;

        //Ref: https://www.ffmpeg.org/doxygen/2.3/avio_reading_8c-example.html

        source->ff_fmt_ctx = avformat_alloc_context();
        if (!source->ff_fmt_ctx) {
            state->status = LOAD_STATE_ERROR;
            state->error = c_strdup("failed to create ffmpeg avformat context");
            return;
        }

        source->ff_avio_buf = av_malloc(4096);
        if (!source->ff_avio_buf) {
            state->status = LOAD_STATE_ERROR;
            state->error = c_strdup("failed to allocate avio buffer");

            cleanup_ffmpeg_state(source);
            return;
        }

        source->ff_avio_ctx = avio_alloc_context(source->ff_avio_buf,
                                                 4096,
                                                 0, /* reading only */
                                                 source, /* user data */
                                                 source_avio_read_packet_cb,
                                                 NULL, /* no write callback */
                                                 source_avio_seek_cb);
        if (!source->ff_avio_ctx) {
            state->status = LOAD_STATE_ERROR;
            state->error = c_strdup("failed to create ffmpeg avio context");

            cleanup_ffmpeg_state(source);
            return;
        }

        source->ff_fmt_ctx->pb = source->ff_avio_ctx;

        ret = avformat_open_input(&source->ff_fmt_ctx,
                                  NULL, /* filename */
                                  NULL, /* format (autodetect) */
                                  NULL); /* fmt_ctx + demuxer options */
        if (ret < 0) {
            state->status = LOAD_STATE_ERROR;
            state->error = c_strdup("failed to create ffmpeg avio context");

            cleanup_ffmpeg_state(source);
            return;
        }

        ret = avformat_find_stream_info(source->ff_fmt_ctx,
                                        NULL); /* options */
        if (ret < 0) {
            state->status = LOAD_STATE_ERROR;
            state->error = c_strdup("failed to find ffmpeg stream info");

            cleanup_ffmpeg_state(source);
            return;
        }

        av_dump_format(source->ff_fmt_ctx,
                       0, /* ndex */
                       source->url,
                       0); /* fmt_ctx is an input */


        source->ff_video_stream = av_find_best_stream(source->ff_fmt_ctx,
                                                      AVMEDIA_TYPE_VIDEO,
                                                      -1, /* auto select stream */
                                                      -1, /* related stream */
                                                      &codec, /* decode codec */
                                                      0); /* flags */
        if (source->ff_video_stream < 0) {
            state->status = LOAD_STATE_ERROR;
            state->error = c_strdup("failed to find video stream or codec");

            cleanup_ffmpeg_state(source);
            return;
        }

        source->ff_video_codec_ctx =
            source->ff_fmt_ctx->streams[source->ff_video_stream]->codec;
        av_dict_set(&opts, "refcounted_frames", "1", 0);

        ret = avcodec_open2(source->ff_video_codec_ctx, codec, &opts);
        if (ret < 0) {
            state->status = LOAD_STATE_ERROR;
            state->error = c_strdup("failed to find video stream or codec");

            cleanup_ffmpeg_state(source);
            return;
        }

        source->ff_dst_frame = av_frame_alloc();

#warning "fixme: missing some ffmpeg cleanup"

        source->timeline = rig_timeline_new(engine, FLT_MAX);

        source->type = SOURCE_TYPE_FFMPEG;
        state->status = LOAD_STATE_LOADED;
    } else
#endif
#ifdef USE_GSTREAMER
    if (strncmp(source->mime, "video/", 6) == 0) {
        gst_source_start(source);
        g_signal_connect(source->sink,
                         "pipeline_ready",
                         (GCallback)gst_pipeline_ready_cb,
                         source);
        g_signal_connect(source->sink, "new_frame",
                         (GCallback)gst_new_frame_cb, source);

        source->type = SOURCE_TYPE_GSTREAMER;
    } else
#endif
    {
        c_warning("FIXME: Rig is missing support for '%s' on this platform",
                  source->mime);
        source->texture = cg_object_ref(frontend->default_tex2d);
    }
}

cg_texture_t *
rig_source_get_texture(rig_source_t *source)
{
    return source->texture;
}

void
rig_source_add_ready_callback(rig_source_t *source,
                              rut_closure_t *closure)
{
    rut_closure_list_add(&source->ready_cb_list, closure);

    if (source->texture)
        rut_closure_invoke(closure, rig_source_ready_callback_t, source);
}

void
rig_source_add_on_changed_callback(rig_source_t *source,
                                   rut_closure_t *closure)
{
    return rut_closure_list_add(&source->changed_cb_list, closure);
}

void
rig_source_add_on_error_callback(rig_source_t *source,
                                 rut_closure_t *closure)
{
    return rut_closure_list_add(&source->error_cb_list, closure);
}

void
rig_source_set_first_layer(rig_source_t *source,
                                 int first_layer)
{
    source->first_layer = first_layer;
}

void
rig_source_set_default_sample(rig_source_t *source,
                                    bool default_sample)
{
    source->default_sample = default_sample;
}

static source_wrappers_t *
get_source_wrappers(rig_frontend_t *frontend,
                    int layer_index)
{
    source_wrappers_t *wrappers = c_hash_table_lookup(
        frontend->source_wrappers, C_UINT_TO_POINTER(layer_index));
    char *wrapper;

    if (wrappers)
        return wrappers;

    wrappers = c_slice_new0(source_wrappers_t);

    /* XXX: Note: we use texture2D() instead of the
     * cg_texture_lookup%i wrapper because the _GLOBALS hook comes
     * before the _lookup functions are emitted by Cogl */
    wrapper = c_strdup_printf("vec4\n"
                              "rig_source_sample%d(vec2 UV)\n"
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

    wrappers->source_vertex_wrapper =
        cg_snippet_new(CG_SNIPPET_HOOK_VERTEX_GLOBALS, wrapper, NULL);
    wrappers->source_fragment_wrapper =
        cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT_GLOBALS, wrapper, NULL);

    c_free(wrapper);

    wrapper = c_strdup_printf("vec4\n"
                              "rig_source_sample%d (vec2 UV)\n"
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

    c_hash_table_insert(frontend->source_wrappers,
                        C_UINT_TO_POINTER(layer_index), wrappers);

    return wrappers;
}

void
rig_source_setup_pipeline(rig_source_t *source,
                          cg_pipeline_t *pipeline)
{
    rig_engine_t *engine = rig_component_props_get_engine(&source->component);
    rig_frontend_t *frontend = engine->frontend;
    cg_snippet_t *vertex_snippet;
    cg_snippet_t *fragment_snippet;
    source_wrappers_t *wrappers =
        get_source_wrappers(frontend, source->first_layer);

    switch (source->type) {
#ifdef USE_GSTREAMER
    case SOURCE_TYPE_GSTREAMER: {
        CgGstVideoSink *sink = gst_source_get_sink(source);

        cg_gst_video_sink_set_first_layer(sink, source->first_layer);
        cg_gst_video_sink_set_default_sample(sink, true);
        cg_gst_video_sink_setup_pipeline(sink, pipeline);

        vertex_snippet = wrappers->video_source_vertex_wrapper;
        fragment_snippet = wrappers->video_source_fragment_wrapper;
        break;
    }
#endif

    case SOURCE_TYPE_UNLOADED:
#ifdef USE_FFMPEG
    case SOURCE_TYPE_FFMPEG:
#endif
#ifdef USE_GDK_PIXBUF
    case SOURCE_TYPE_PIXBUF: 
#endif
    case SOURCE_TYPE_GIF:
    case SOURCE_TYPE_PNG:
    case SOURCE_TYPE_JPG: {
        cg_texture_t *texture = source->texture;

        //cg_pipeline_set_color4f(pipeline, 0, 1, 0, 1);
//#warning "debug - source pipeline override"
#if 1
        cg_pipeline_set_layer_texture(pipeline, source->first_layer, texture);

        if (!source->default_sample) {
            cg_snippet_t *snippet =
                cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
            cg_snippet_set_replace(snippet, "");
            cg_pipeline_add_layer_snippet(pipeline, source->first_layer, snippet);
            cg_object_unref(snippet);
        }

        vertex_snippet = wrappers->source_vertex_wrapper;
        fragment_snippet = wrappers->source_fragment_wrapper;
#endif
        break;
    }
    }

    //cg_pipeline_add_snippet(pipeline, vertex_snippet);
    //cg_pipeline_add_snippet(pipeline, fragment_snippet);
#warning "debug - source pipeline override"
}

void
rig_source_attach_frame(rig_source_t *source,
                        cg_pipeline_t *pipeline)
{
    /* NB: For non-video sources we always attach the texture
     * during rig_source_setup_pipeline() so we don't
     * have to do anything here.
     */

    if (!source->changed)
        return;

    switch (source->type) {
#ifdef USE_GSTREAMER
    case SOURCE_TYPE_GSTREAMER:
        cg_gst_video_sink_attach_frame(gst_source_get_sink(source),
                                       pipeline);
        break;
#endif
    case SOURCE_TYPE_GIF: {
        rig_engine_t *engine = rig_component_props_get_engine(&source->component);
        cg_bitmap_t *bmp;
        cg_texture_2d_t *tex;
        gif_result code;
        cg_error_t *error = NULL;
        uint8_t *buf;
        double elapsed = rig_timeline_get_elapsed(source->timeline);
        int current_frame = source->gif_current_frame;
        double current_elapsed = source->gif_current_elapsed;
        gif_animation *gif = &source->gif;
        int frame_count = gif->frame_count;

        if (elapsed != current_elapsed) {
            double e = elapsed;

            if (elapsed > current_elapsed) {
                for (int i = current_frame + 1;
                     e <= elapsed && i < (frame_count - 1);
                     i++)
                {
                    gif_frame *f = &gif->frames[i];

                    e += f->frame_delay;
                    current_frame = i;
                }
            } else {
                for (int i = current_frame - 1; e > elapsed && i > 1; i--) {
                    gif_frame *f = &gif->frames[i];

                    e -= f->frame_delay;
                    current_frame = i;
                }
            }

            source->gif_current_frame = current_frame;
            source->gif_current_elapsed = e;
        }

        code = gif_decode_frame(&source->gif, current_frame);
        if (code != GIF_OK) {
            c_warning("failed to load GIF frame %d", current_frame);
            break;
        }

        buf = bitmap_get_buffer(&source->gif, source->gif.frame_image);
        bmp = cg_bitmap_new_for_data(engine->shell->cg_device,
                                     source->gif.width,
                                     source->gif.height,
                                     CG_PIXEL_FORMAT_RGBA_8888,
                                     0,
                                     buf);
        tex = cg_texture_2d_new_from_bitmap(bmp);
        cg_object_unref(bmp);

        if (cg_texture_allocate(tex, &error))
            _source_set_texture(source, tex);
        else
            _source_set_texture(source, engine->frontend->default_tex2d);

        cg_pipeline_set_layer_texture(pipeline,
                                      //0,
                                      source->first_layer,
                                      source->texture);
        break;
    }
    case SOURCE_TYPE_FFMPEG: {
        rig_engine_t *engine = rig_component_props_get_engine(&source->component);
        AVPacket packet;
        int frame_ready = false;
        int dst_size;
        cg_bitmap_t *bmp;
        cg_texture_2d_t *tex;
        cg_error_t *error = NULL;

        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            c_warning("failed to allocate ffmpeg frame");
            break;
        }

        while(!frame_ready) {
            int ret = av_read_frame(source->ff_fmt_ctx, &packet);
            if (ret < 0) {
                if (ret == AVERROR_EOF)
                    c_warning("video EOF");
                else
                    c_warning("video ");
                break;
            }

            if (packet.stream_index != source->ff_video_stream)
                continue;

            avcodec_decode_video2(source->ff_video_codec_ctx,
                                  frame,
                                  &frame_ready,
                                  &packet);

            av_packet_unref(&packet);
        }

        source->sws_ctx = sws_getCachedContext(source->sws_ctx,
                                               frame->width,
                                               frame->height,
                                               frame->format,
                                               frame->width,
                                               frame->height,
                                               AV_PIX_FMT_RGBA,
                                               SWS_BICUBIC, /* flags */
                                               NULL, /* src filter */
                                               NULL, /* dst filter */
                                               NULL); /* param */

        dst_size = frame->width * frame->height * 4;
        if (dst_size > source->ff_dst_frame_buf_size) {
            av_freep(&source->ff_dst_frame_buf);
            source->ff_dst_frame_buf = av_malloc(dst_size);
            source->ff_dst_frame_buf_size = dst_size;
        }

        if (source->ff_dst_frame->width != frame->width ||
            source->ff_dst_frame->height != frame->height)
        {
            if (source->ff_dst_frame->data)
                av_frame_unref(source->ff_dst_frame);

            avpicture_fill((AVPicture *)source->ff_dst_frame,
                           source->ff_dst_frame_buf,
                           AV_PIX_FMT_RGBA,
                           frame->width,
                           frame->height);
        }

        sws_scale(source->sws_ctx,
                  frame->data, /* src slice */
                  frame->linesize, /* src stride */
                  0, /* src slice Y */
                  frame->height, /* src slice H */
                  source->ff_dst_frame->data, /* dest */
                  source->ff_dst_frame->linesize); /* dest stride */

        bmp = cg_bitmap_new_for_data(engine->shell->cg_device,
                                     frame->width,
                                     frame->height,
                                     CG_PIXEL_FORMAT_RGBA_8888,
                                     0,
                                     source->ff_dst_frame_buf);

        tex = cg_texture_2d_new_from_bitmap(bmp);
        cg_object_unref(bmp);

        if (cg_texture_allocate(tex, &error))
            _source_set_texture(source, tex);
        else
            _source_set_texture(source, engine->frontend->default_tex2d);

        cg_pipeline_set_layer_texture(pipeline,
                                      //0,
                                      source->first_layer,
                                      source->texture);

        cg_object_unref(tex); /* ref taken by pipeline */

        av_frame_free(&frame);

        break;
    }
    default:
        cg_pipeline_set_layer_texture(pipeline,
                                      source->first_layer,
                                      source->texture);
        break;
    }
}

void
rig_source_get_natural_size(rig_source_t *source,
                            float *width,
                            float *height)
{
    *width = 100;
    *height = 100;

    switch (source->type) {
#ifdef USE_GSTREAMER
    case SOURCE_TYPE_GSTREAMER:
        cg_gst_video_sink_get_natural_size(source->sink, width, height);
        break;
#endif
    default:
        if (source->texture) {
            *width = cg_texture_get_width(source->texture);
            *height = cg_texture_get_height(source->texture);
        }
    }
}

void
rig_source_set_url(rut_object_t *obj, const char *url)
{
    rig_source_t *source = obj;
    rig_property_context_t *prop_ctx;

    if (source->url) {
        c_free(source->url);
#warning "TODO: Free other associated resources, or mark them dirty"
        source->url = NULL;
    }

    if (url) {
        rig_engine_t *engine = rig_component_props_get_engine(&source->component);
        rig_frontend_t *frontend = engine->frontend;

        source->url = strdup(url);

        if (frontend) {
            rut_shell_t *shell = engine->shell;

            rut_poll_shell_add_idle(shell, &source->load_state.load_idle);
        }
    }

    prop_ctx = rig_component_props_get_property_context(&source->component);
    rig_property_dirty(prop_ctx, &source->properties[RIG_SOURCE_PROP_URL]);
}

const char *
rig_source_get_url(rut_object_t *obj)
{
    rig_source_t *source = obj;

    return source->url;
}

bool
rig_source_get_running(rut_object_t *object)
{
    rig_source_t *source = object;
    return source->timeline ? rig_timeline_get_running(source->timeline) : false;
}

void
rig_source_set_running(rut_object_t *object, bool running)
{
    rig_source_t *source = object;
    rig_engine_t *engine;

    if (rig_source_get_running(source) == running)
        return;

    if (source->timeline)
        rig_timeline_set_running(source->timeline, running);

    engine = rig_component_props_get_engine(&source->component);
    rig_property_dirty(engine->property_ctx,
                       &source->properties[RIG_SOURCE_PROP_RUNNING]);
}


