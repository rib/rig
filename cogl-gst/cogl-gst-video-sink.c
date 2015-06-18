/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007, 2008 OpenedHand
 * Copyright (C) 2009, 2010, 2013 Intel Corporation
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

#include "config.h"

#include <gst/gst.h>
#include <gst/gstvalue.h>
#include <gst/video/video.h>
#include <gst/riff/riff-ids.h>
#include <string.h>

#include <clib.h>

/* We just need the public Cogl api for cogl-gst but we first need to
 * undef CG_COMPILATION to avoid getting an error that normally
 * checks cogl.h isn't used internally. */
#undef CG_COMPILATION
#include <cogl/cogl.h>

#include "cogl-gst-video-sink.h"

#define CG_GST_DEFAULT_PRIORITY G_PRIORITY_HIGH_IDLE

#define BASE_SINK_CAPS                                                         \
    "{ AYUV,"                                                                  \
    "YV12,"                                                                    \
    "I420,"                                                                    \
    "RGBA,"                                                                    \
    "BGRA,"                                                                    \
    "RGB,"                                                                     \
    "BGR,"                                                                     \
    "NV12 }"

#define SINK_CAPS GST_VIDEO_CAPS_MAKE(BASE_SINK_CAPS)

#define CG_GST_PARAM_STATIC                                                    \
    (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

#define CG_GST_PARAM_READABLE (G_PARAM_READABLE | CG_GST_PARAM_STATIC)

#define CG_GST_PARAM_WRITABLE (G_PARAM_WRITABLE | CG_GST_PARAM_STATIC)

#define CG_GST_PARAM_READWRITE                                                 \
    (G_PARAM_READABLE | G_PARAM_WRITABLE | CG_GST_PARAM_STATIC)

static GstStaticPadTemplate sinktemplate_all = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(SINK_CAPS));

G_DEFINE_TYPE(CgGstVideoSink, cg_gst_video_sink, GST_TYPE_BASE_SINK);

enum {
    PROP_0,
    PROP_UPDATE_PRIORITY
};

enum {
    PIPELINE_READY_SIGNAL,
    NEW_FRAME_SIGNAL,
    LAST_SIGNAL
};

static guint video_sink_signals[LAST_SIGNAL] = { 0, };

typedef enum {
    CG_GST_NOFORMAT,
    CG_GST_RGB32,
    CG_GST_RGB24,
    CG_GST_AYUV,
    CG_GST_YV12,
    CG_GST_SURFACE,
    CG_GST_I420,
    CG_GST_NV12
} CgGstVideoFormat;

typedef enum {
    CG_GST_RENDERER_NEEDS_GLSL = (1 << 0),
    CG_GST_RENDERER_NEEDS_TEXTURE_RG = (1 << 1)
} CgGstRendererFlag;

/* We want to cache the snippets instead of recreating a new one every
 * time we initialise a pipeline so that if we end up recreating the
 * same pipeline again then Cogl will be able to use the pipeline
 * cache to avoid linking a redundant identical shader program */
typedef struct {
    cg_snippet_t *vertex_snippet;
    cg_snippet_t *fragment_snippet;
    cg_snippet_t *default_sample_snippet;
    int start_position;
} SnippetCacheEntry;

typedef struct {
    GQueue entries;
} SnippetCache;

typedef struct _CgGstSource {
    GSource source;
    CgGstVideoSink *sink;
    GMutex buffer_lock;
    GstBuffer *buffer;
    bool has_new_caps;
} CgGstSource;

typedef void (CgGstRendererPaint)(CgGstVideoSink *);
typedef void (CgGstRendererPostPaint)(CgGstVideoSink *);

typedef struct _CgGstRenderer {
    const char *name;
    CgGstVideoFormat format;
    int flags;
    GstStaticCaps caps;
    int n_layers;
    void (*setup_pipeline)(CgGstVideoSink *sink, cg_pipeline_t *pipeline);
    bool (*upload)(CgGstVideoSink *sink, GstBuffer *buffer);
} CgGstRenderer;

struct _CgGstVideoSinkPrivate {
    cg_device_t *dev;
    cg_pipeline_t *pipeline;
    cg_texture_t *frame[3];
    bool frame_dirty;
    CgGstVideoFormat format;
    bool bgr;
    CgGstSource *source;
    GSList *renderers;
    GstCaps *caps;
    CgGstRenderer *renderer;
    GstFlowReturn flow_return;
    cg_snippet_t *layer_skip_snippet;
    int custom_start;
    int free_layer;
    bool default_sample;
    GstVideoInfo info;
};

static void
cg_gst_source_finalize(GSource *source)
{
    CgGstSource *gst_source = (CgGstSource *)source;

    g_mutex_lock(&gst_source->buffer_lock);
    if (gst_source->buffer)
        gst_buffer_unref(gst_source->buffer);
    gst_source->buffer = NULL;
    g_mutex_unlock(&gst_source->buffer_lock);
    g_mutex_clear(&gst_source->buffer_lock);
}

int
cg_gst_video_sink_get_free_layer(CgGstVideoSink *sink)
{
    return sink->priv->free_layer;
}

void
cg_gst_video_sink_attach_frame(CgGstVideoSink *sink, cg_pipeline_t *pln)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    int i;

    for (i = 0; i < C_N_ELEMENTS(priv->frame); i++)
        if (priv->frame[i] != NULL)
            cg_pipeline_set_layer_texture(
                pln, i + priv->custom_start, priv->frame[i]);
}

static gboolean
cg_gst_source_prepare(GSource *source, int *timeout)
{
    CgGstSource *gst_source = (CgGstSource *)source;

    *timeout = -1;

    return gst_source->buffer != NULL;
}

static gboolean
cg_gst_source_check(GSource *source)
{
    CgGstSource *gst_source = (CgGstSource *)source;

    return gst_source->buffer != NULL;
}

static void
cg_gst_video_sink_set_priority(CgGstVideoSink *sink, int priority)
{
    if (sink->priv->source)
        g_source_set_priority((GSource *)sink->priv->source, priority);
}

static void
dirty_default_pipeline(CgGstVideoSink *sink)
{
    CgGstVideoSinkPrivate *priv = sink->priv;

    if (priv->pipeline) {
        cg_object_unref(priv->pipeline);
        priv->pipeline = NULL;
    }
}

void
cg_gst_video_sink_set_first_layer(CgGstVideoSink *sink, int first_layer)
{
    g_return_if_fail(CG_GST_IS_VIDEO_SINK(sink));

    if (first_layer != sink->priv->custom_start) {
        sink->priv->custom_start = first_layer;
        dirty_default_pipeline(sink);

        if (sink->priv->renderer)
            sink->priv->free_layer =
                (sink->priv->custom_start + sink->priv->renderer->n_layers);
    }
}

void
cg_gst_video_sink_set_default_sample(CgGstVideoSink *sink,
                                     bool default_sample)
{
    g_return_if_fail(CG_GST_IS_VIDEO_SINK(sink));

    if (default_sample != sink->priv->default_sample) {
        sink->priv->default_sample = default_sample;
        dirty_default_pipeline(sink);
    }
}

void
cg_gst_video_sink_setup_pipeline(CgGstVideoSink *sink,
                                 cg_pipeline_t *pipeline)
{
    g_return_if_fail(CG_GST_IS_VIDEO_SINK(sink));

    if (sink->priv->renderer)
        sink->priv->renderer->setup_pipeline(sink, pipeline);
}

static SnippetCacheEntry *
get_cache_entry(CgGstVideoSink *sink,
                SnippetCache *cache)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    GList *l;

    for (l = cache->entries.head; l; l = l->next) {
        SnippetCacheEntry *entry = l->data;

        if (entry->start_position == priv->custom_start)
            return entry;
    }

    return NULL;
}

static SnippetCacheEntry *
add_cache_entry(CgGstVideoSink *sink, SnippetCache *cache, const char *decl)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    SnippetCacheEntry *entry = g_slice_new(SnippetCacheEntry);
    char *default_source;

    entry->start_position = priv->custom_start;

    entry->vertex_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_VERTEX_GLOBALS, decl, NULL /* post */);
    entry->fragment_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_FRAGMENT_GLOBALS, decl, NULL /* post */);

    default_source = g_strdup_printf("  frag *= cg_gst_sample_video%i "
                                     "(cg_tex_coord%i_in.st);\n",
                                     priv->custom_start,
                                     priv->custom_start);
    entry->default_sample_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT,
                       NULL, /* declarations */
                       default_source);
    g_free(default_source);

    g_queue_push_head(&cache->entries, entry);

    return entry;
}

static void
setup_pipeline_from_cache_entry(CgGstVideoSink *sink,
                                cg_pipeline_t *pipeline,
                                SnippetCacheEntry *cache_entry,
                                int n_layers)
{
    CgGstVideoSinkPrivate *priv = sink->priv;

    if (cache_entry) {
        int i;

        /* The global sampling function gets added to both the fragment
         * and vertex stages. The hope is that the GLSL compiler will
         * easily remove the dead code if it's not actually used */
        cg_pipeline_add_snippet(pipeline, cache_entry->vertex_snippet);
        cg_pipeline_add_snippet(pipeline, cache_entry->fragment_snippet);

        /* Set all of the layers to just directly copy from the previous
         * layer so that it won't redundantly generate code to sample
         * the intermediate textures */
        for (i = 0; i < n_layers; i++)
            cg_pipeline_add_layer_snippet(pipeline,
                                          priv->custom_start + i,
                                          priv->layer_skip_snippet);

        if (priv->default_sample)
            cg_pipeline_add_layer_snippet(pipeline,
                                          priv->custom_start + n_layers - 1,
                                          cache_entry->default_sample_snippet);
    }

    priv->frame_dirty = true;
}

cg_pipeline_t *
cg_gst_video_sink_get_pipeline(CgGstVideoSink *vt)
{
    CgGstVideoSinkPrivate *priv;

    g_return_val_if_fail(CG_GST_IS_VIDEO_SINK(vt), NULL);

    priv = vt->priv;

    if (priv->pipeline == NULL) {
        priv->pipeline = cg_pipeline_new(priv->dev);
        cg_gst_video_sink_setup_pipeline(vt, priv->pipeline);
        cg_gst_video_sink_attach_frame(vt, priv->pipeline);
        priv->frame_dirty = false;
    } else if (priv->frame_dirty) {
        cg_pipeline_t *pipeline = cg_pipeline_copy(priv->pipeline);
        cg_object_unref(priv->pipeline);
        priv->pipeline = pipeline;

        cg_gst_video_sink_attach_frame(vt, pipeline);
        priv->frame_dirty = false;
    }

    return priv->pipeline;
}

static void
clear_frame_textures(CgGstVideoSink *sink)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    int i;

    for (i = 0; i < C_N_ELEMENTS(priv->frame); i++) {
        if (priv->frame[i] == NULL)
            break;
        else
            cg_object_unref(priv->frame[i]);
    }

    memset(priv->frame, 0, sizeof(priv->frame));

    priv->frame_dirty = true;
}

static inline bool
is_pot(unsigned int number)
{
    /* Make sure there is only one bit set */
    return (number & (number - 1)) == 0;
}

/* This first tries to upload the texture to a cg_texture_2d_t, but
 * if that's not possible it falls back to a cg_texture_2d_sliced_t.
 *
 * Auto-mipmapping of any uploaded texture is disabled
 */
static cg_texture_t *
video_texture_new_from_data(cg_device_t *dev,
                            int width,
                            int height,
                            cg_pixel_format_t format,
                            int rowstride,
                            const uint8_t *data)
{
    cg_bitmap_t *bitmap;
    cg_texture_t *tex;
    cg_error_t *internal_error = NULL;

    bitmap = cg_bitmap_new_for_data(dev, width, height, format, rowstride,
                                    (uint8_t *)data);

    if ((is_pot(cg_bitmap_get_width(bitmap)) &&
         is_pot(cg_bitmap_get_height(bitmap))) ||
        cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_NPOT_BASIC)) {
        tex = cg_texture_2d_new_from_bitmap(bitmap);

        cg_texture_set_premultiplied(tex, false);

        if (!cg_texture_allocate(tex, &internal_error)) {
            cg_error_free(internal_error);
            internal_error = NULL;
            cg_object_unref(tex);
            tex = NULL;
        }
    } else
        tex = NULL;

    if (!tex) {
        /* Otherwise create a sliced texture */
        tex = cg_texture_2d_sliced_new_from_bitmap(bitmap,
                                                   -1); /* no maximum waste */

        cg_texture_set_premultiplied(tex, false);

        cg_texture_allocate(tex, NULL);
    }

    cg_object_unref(bitmap);

    return tex;
}

static void
cg_gst_rgb24_glsl_setup_pipeline(CgGstVideoSink *sink,
                                 cg_pipeline_t *pipeline)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    static SnippetCache snippet_cache;
    SnippetCacheEntry *entry = get_cache_entry(sink, &snippet_cache);

    if (entry == NULL) {
        char *source;

        source = g_strdup_printf("vec4\n"
                                 "cg_gst_sample_video%i (vec2 UV)\n"
                                 "{\n"
                                 "  return texture2D (cg_sampler%i, UV);\n"
                                 "}\n",
                                 priv->custom_start,
                                 priv->custom_start);

        entry = add_cache_entry(sink, &snippet_cache, source);
        g_free(source);
    }

    setup_pipeline_from_cache_entry(sink, pipeline, entry, 1);
}

static void
cg_gst_rgb24_setup_pipeline(CgGstVideoSink *sink,
                            cg_pipeline_t *pipeline)
{
    setup_pipeline_from_cache_entry(sink, pipeline, NULL, 1);
}

static bool
cg_gst_rgb24_upload(CgGstVideoSink *sink, GstBuffer *buffer)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    cg_pixel_format_t format;
    GstVideoFrame frame;

    if (priv->bgr)
        format = CG_PIXEL_FORMAT_BGR_888;
    else
        format = CG_PIXEL_FORMAT_RGB_888;

    if (!gst_video_frame_map(&frame, &priv->info, buffer, GST_MAP_READ))
        goto map_fail;

    clear_frame_textures(sink);

    priv->frame[0] = video_texture_new_from_data(priv->dev,
                                                 priv->info.width,
                                                 priv->info.height,
                                                 format,
                                                 priv->info.stride[0],
                                                 frame.data[0]);

    gst_video_frame_unmap(&frame);

    return true;

map_fail: {
        GST_ERROR_OBJECT(sink, "Could not map incoming video frame");
        return false;
    }
}

static CgGstRenderer rgb24_glsl_renderer = {
    "RGB 24",
    CG_GST_RGB24,
    CG_GST_RENDERER_NEEDS_GLSL,
    GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("{ RGB, BGR }")),
    1, /* n_layers */
    cg_gst_rgb24_glsl_setup_pipeline,
    cg_gst_rgb24_upload,
};

static CgGstRenderer rgb24_renderer = { "RGB 24", CG_GST_RGB24, 0,
                                        GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE(
                                                            "{ RGB, BGR }")),
                                        1, /* n_layers */
                                        cg_gst_rgb24_setup_pipeline,
                                        cg_gst_rgb24_upload, };

static void
cg_gst_rgb32_glsl_setup_pipeline(CgGstVideoSink *sink,
                                 cg_pipeline_t *pipeline)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    static SnippetCache snippet_cache;
    SnippetCacheEntry *entry = get_cache_entry(sink, &snippet_cache);

    if (entry == NULL) {
        char *source;

        source =
            g_strdup_printf("vec4\n"
                            "cg_gst_sample_video%i (vec2 UV)\n"
                            "{\n"
                            "  vec4 color = texture2D (cg_sampler%i, UV);\n"
                            /* Premultiply the color */
                            "  color.rgb *= color.a;\n"
                            "  return color;\n"
                            "}\n",
                            priv->custom_start,
                            priv->custom_start);

        entry = add_cache_entry(sink, &snippet_cache, source);
        g_free(source);
    }

    setup_pipeline_from_cache_entry(sink, pipeline, entry, 1);
}

static void
cg_gst_rgb32_setup_pipeline(CgGstVideoSink *sink,
                            cg_pipeline_t *pipeline)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    cg_snippet_t *snippet;
    char *code;

    setup_pipeline_from_cache_entry(sink, pipeline, NULL, 1);

    /* Premultiply the texture using a special layer hook */

    snippet = cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);

    code = g_strdup_printf("frag.rgb *= cg_texel%i.a;\n", priv->custom_start);
    cg_snippet_set_replace(snippet, code);
    g_free(code);

    cg_pipeline_add_layer_snippet(pipeline, priv->custom_start + 1, snippet);
    cg_object_unref(snippet);
}

static bool
cg_gst_rgb32_upload(CgGstVideoSink *sink, GstBuffer *buffer)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    cg_pixel_format_t format;
    GstVideoFrame frame;

    if (priv->bgr)
        format = CG_PIXEL_FORMAT_BGRA_8888;
    else
        format = CG_PIXEL_FORMAT_RGBA_8888;

    if (!gst_video_frame_map(&frame, &priv->info, buffer, GST_MAP_READ))
        goto map_fail;

    clear_frame_textures(sink);

    priv->frame[0] = video_texture_new_from_data(priv->dev,
                                                 priv->info.width,
                                                 priv->info.height,
                                                 format,
                                                 priv->info.stride[0],
                                                 frame.data[0]);

    gst_video_frame_unmap(&frame);

    return true;

map_fail: {
        GST_ERROR_OBJECT(sink, "Could not map incoming video frame");
        return false;
    }
}

static CgGstRenderer rgb32_glsl_renderer = {
    "RGB 32",
    CG_GST_RGB32,
    CG_GST_RENDERER_NEEDS_GLSL,
    GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("{ RGBA, BGRA }")),
    1, /* n_layers */
    cg_gst_rgb32_glsl_setup_pipeline,
    cg_gst_rgb32_upload,
};

static CgGstRenderer rgb32_renderer = { "RGB 32", CG_GST_RGB32, 0,
                                        GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE(
                                                            "{ RGBA, BGRA }")),
                                        2, /* n_layers */
                                        cg_gst_rgb32_setup_pipeline,
                                        cg_gst_rgb32_upload, };

static bool
cg_gst_yv12_upload(CgGstVideoSink *sink, GstBuffer *buffer)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    cg_pixel_format_t format = CG_PIXEL_FORMAT_A_8;
    GstVideoFrame frame;

    if (!gst_video_frame_map(&frame, &priv->info, buffer, GST_MAP_READ))
        goto map_fail;

    clear_frame_textures(sink);

    priv->frame[0] =
        video_texture_new_from_data(priv->dev,
                                    GST_VIDEO_INFO_COMP_WIDTH(&priv->info, 0),
                                    GST_VIDEO_INFO_COMP_HEIGHT(&priv->info, 0),
                                    format,
                                    priv->info.stride[0],
                                    frame.data[0]);

    priv->frame[2] =
        video_texture_new_from_data(priv->dev,
                                    GST_VIDEO_INFO_COMP_WIDTH(&priv->info, 1),
                                    GST_VIDEO_INFO_COMP_HEIGHT(&priv->info, 1),
                                    format,
                                    priv->info.stride[1],
                                    frame.data[1]);

    priv->frame[1] =
        video_texture_new_from_data(priv->dev,
                                    GST_VIDEO_INFO_COMP_WIDTH(&priv->info, 2),
                                    GST_VIDEO_INFO_COMP_HEIGHT(&priv->info, 2),
                                    format,
                                    priv->info.stride[2],
                                    frame.data[2]);

    gst_video_frame_unmap(&frame);

    return true;

map_fail: {
        GST_ERROR_OBJECT(sink, "Could not map incoming video frame");
        return false;
    }
}

static bool
cg_gst_i420_upload(CgGstVideoSink *sink, GstBuffer *buffer)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    cg_pixel_format_t format = CG_PIXEL_FORMAT_A_8;
    GstVideoFrame frame;

    if (!gst_video_frame_map(&frame, &priv->info, buffer, GST_MAP_READ))
        goto map_fail;

    clear_frame_textures(sink);

    priv->frame[0] =
        video_texture_new_from_data(priv->dev,
                                    GST_VIDEO_INFO_COMP_WIDTH(&priv->info, 0),
                                    GST_VIDEO_INFO_COMP_HEIGHT(&priv->info, 0),
                                    format,
                                    priv->info.stride[0],
                                    frame.data[0]);

    priv->frame[1] =
        video_texture_new_from_data(priv->dev,
                                    GST_VIDEO_INFO_COMP_WIDTH(&priv->info, 1),
                                    GST_VIDEO_INFO_COMP_HEIGHT(&priv->info, 1),
                                    format,
                                    priv->info.stride[1],
                                    frame.data[1]);

    priv->frame[2] =
        video_texture_new_from_data(priv->dev,
                                    GST_VIDEO_INFO_COMP_WIDTH(&priv->info, 2),
                                    GST_VIDEO_INFO_COMP_HEIGHT(&priv->info, 2),
                                    format,
                                    priv->info.stride[2],
                                    frame.data[2]);

    gst_video_frame_unmap(&frame);

    return true;

map_fail: {
        GST_ERROR_OBJECT(sink, "Could not map incoming video frame");
        return false;
    }
}

static void
cg_gst_yv12_glsl_setup_pipeline(CgGstVideoSink *sink,
                                cg_pipeline_t *pipeline)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    static SnippetCache snippet_cache;
    SnippetCacheEntry *entry;

    entry = get_cache_entry(sink, &snippet_cache);

    if (entry == NULL) {
        char *source;

        source = g_strdup_printf(
            "vec4\n"
            "cg_gst_sample_video%i (vec2 UV)\n"
            "{\n"
            "  float y = 1.1640625 * "
            "(texture2D (cg_sampler%i, UV).a - 0.0625);\n"
            "  float u = texture2D (cg_sampler%i, UV).a - 0.5;\n"
            "  float v = texture2D (cg_sampler%i, UV).a - 0.5;\n"
            "  vec4 color;\n"
            "  color.r = y + 1.59765625 * v;\n"
            "  color.g = y - 0.390625 * u - 0.8125 * v;\n"
            "  color.b = y + 2.015625 * u;\n"
            "  color.a = 1.0;\n"
            "  return color;\n"
            "}\n",
            priv->custom_start,
            priv->custom_start,
            priv->custom_start + 1,
            priv->custom_start + 2);

        entry = add_cache_entry(sink, &snippet_cache, source);
        g_free(source);
    }

    setup_pipeline_from_cache_entry(sink, pipeline, entry, 3);
}

static CgGstRenderer yv12_glsl_renderer = { "YV12 glsl", CG_GST_YV12,
                                            CG_GST_RENDERER_NEEDS_GLSL,
                                            GST_STATIC_CAPS(
                                                GST_VIDEO_CAPS_MAKE("YV12")),
                                            3, /* n_layers */
                                            cg_gst_yv12_glsl_setup_pipeline,
                                            cg_gst_yv12_upload, };

static CgGstRenderer i420_glsl_renderer = { "I420 glsl", CG_GST_I420,
                                            CG_GST_RENDERER_NEEDS_GLSL,
                                            GST_STATIC_CAPS(
                                                GST_VIDEO_CAPS_MAKE("I420")),
                                            3, /* n_layers */
                                            cg_gst_yv12_glsl_setup_pipeline,
                                            cg_gst_i420_upload, };

static void
cg_gst_ayuv_glsl_setup_pipeline(CgGstVideoSink *sink,
                                cg_pipeline_t *pipeline)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    static SnippetCache snippet_cache;
    SnippetCacheEntry *entry;

    entry = get_cache_entry(sink, &snippet_cache);

    if (entry == NULL) {
        char *source;

        source =
            g_strdup_printf("vec4\n"
                            "cg_gst_sample_video%i (vec2 UV)\n"
                            "{\n"
                            "  vec4 color = texture2D (cg_sampler%i, UV);\n"
                            "  float y = 1.1640625 * (color.g - 0.0625);\n"
                            "  float u = color.b - 0.5;\n"
                            "  float v = color.a - 0.5;\n"
                            "  color.a = color.r;\n"
                            "  color.r = y + 1.59765625 * v;\n"
                            "  color.g = y - 0.390625 * u - 0.8125 * v;\n"
                            "  color.b = y + 2.015625 * u;\n"
                            /* Premultiply the color */
                            "  color.rgb *= color.a;\n"
                            "  return color;\n"
                            "}\n",
                            priv->custom_start,
                            priv->custom_start);

        entry = add_cache_entry(sink, &snippet_cache, source);
        g_free(source);
    }

    setup_pipeline_from_cache_entry(sink, pipeline, entry, 1);
}

static bool
cg_gst_ayuv_upload(CgGstVideoSink *sink, GstBuffer *buffer)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    cg_pixel_format_t format = CG_PIXEL_FORMAT_RGBA_8888;
    GstVideoFrame frame;

    if (!gst_video_frame_map(&frame, &priv->info, buffer, GST_MAP_READ))
        goto map_fail;

    clear_frame_textures(sink);

    priv->frame[0] = video_texture_new_from_data(priv->dev,
                                                 priv->info.width,
                                                 priv->info.height,
                                                 format,
                                                 priv->info.stride[0],
                                                 frame.data[0]);

    gst_video_frame_unmap(&frame);

    return true;

map_fail: {
        GST_ERROR_OBJECT(sink, "Could not map incoming video frame");
        return false;
    }
}

static CgGstRenderer ayuv_glsl_renderer = { "AYUV glsl", CG_GST_AYUV,
                                            CG_GST_RENDERER_NEEDS_GLSL,
                                            GST_STATIC_CAPS(
                                                GST_VIDEO_CAPS_MAKE("AYUV")),
                                            1, /* n_layers */
                                            cg_gst_ayuv_glsl_setup_pipeline,
                                            cg_gst_ayuv_upload, };

static void
cg_gst_nv12_glsl_setup_pipeline(CgGstVideoSink *sink,
                                cg_pipeline_t *pipeline)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    static SnippetCache snippet_cache;
    SnippetCacheEntry *entry;

    entry = get_cache_entry(sink, &snippet_cache);

    if (entry == NULL) {
        char *source;

        source =
            g_strdup_printf("vec4\n"
                            "cg_gst_sample_video%i (vec2 UV)\n"
                            "{\n"
                            "  vec4 color;\n"
                            "  float y = 1.1640625 *\n"
                            "            (texture2D (cg_sampler%i, UV).a -\n"
                            "             0.0625);\n"
                            "  vec2 uv = texture2D (cg_sampler%i, UV).rg;\n"
                            "  uv -= 0.5;\n"
                            "  float u = uv.x;\n"
                            "  float v = uv.y;\n"
                            "  color.r = y + 1.59765625 * v;\n"
                            "  color.g = y - 0.390625 * u - 0.8125 * v;\n"
                            "  color.b = y + 2.015625 * u;\n"
                            "  color.a = 1.0;\n"
                            "  return color;\n"
                            "}\n",
                            priv->custom_start,
                            priv->custom_start,
                            priv->custom_start + 1);

        entry = add_cache_entry(sink, &snippet_cache, source);
        g_free(source);
    }

    setup_pipeline_from_cache_entry(sink, pipeline, entry, 2);
}

static bool
cg_gst_nv12_upload(CgGstVideoSink *sink, GstBuffer *buffer)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    GstVideoFrame frame;

    if (!gst_video_frame_map(&frame, &priv->info, buffer, GST_MAP_READ))
        goto map_fail;

    clear_frame_textures(sink);

    priv->frame[0] =
        video_texture_new_from_data(priv->dev,
                                    GST_VIDEO_INFO_COMP_WIDTH(&priv->info, 0),
                                    GST_VIDEO_INFO_COMP_HEIGHT(&priv->info, 0),
                                    CG_PIXEL_FORMAT_A_8,
                                    priv->info.stride[0],
                                    frame.data[0]);

    priv->frame[1] =
        video_texture_new_from_data(priv->dev,
                                    GST_VIDEO_INFO_COMP_WIDTH(&priv->info, 1),
                                    GST_VIDEO_INFO_COMP_HEIGHT(&priv->info, 1),
                                    CG_PIXEL_FORMAT_RG_88,
                                    priv->info.stride[1],
                                    frame.data[1]);

    gst_video_frame_unmap(&frame);

    return true;

map_fail: {
        GST_ERROR_OBJECT(sink, "Could not map incoming video frame");
        return false;
    }
}

static CgGstRenderer nv12_glsl_renderer = {
    "NV12 glsl", CG_GST_NV12,
    CG_GST_RENDERER_NEEDS_GLSL | CG_GST_RENDERER_NEEDS_TEXTURE_RG,
    GST_STATIC_CAPS(
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:SystemMemory", "NV12")),
    2, /* n_layers */
    cg_gst_nv12_glsl_setup_pipeline, cg_gst_nv12_upload,
};

static GSList *
cg_gst_build_renderers_list(cg_device_t *dev)
{
    GSList *list = NULL;
    CgGstRendererFlag flags = 0;
    int i;
    static CgGstRenderer *const renderers[] = {
        /* These are in increasing order of priority so that the
         * priv->renderers will be in decreasing order. That way the GLSL
         * renderers will be preferred if they are available */
        &rgb24_renderer,      &rgb32_renderer,     &rgb24_glsl_renderer,
        &rgb32_glsl_renderer, &yv12_glsl_renderer, &i420_glsl_renderer,
        &ayuv_glsl_renderer,  &nv12_glsl_renderer, NULL
    };

    if (cg_has_feature(dev, CG_FEATURE_ID_GLSL))
        flags |= CG_GST_RENDERER_NEEDS_GLSL;

    if (cg_has_feature(dev, CG_FEATURE_ID_TEXTURE_RG))
        flags |= CG_GST_RENDERER_NEEDS_TEXTURE_RG;

    for (i = 0; renderers[i]; i++)
        if ((renderers[i]->flags & flags) == renderers[i]->flags)
            list = g_slist_prepend(list, renderers[i]);

    return list;
}

static void
append_cap(gpointer data, gpointer user_data)
{
    CgGstRenderer *renderer = (CgGstRenderer *)data;
    GstCaps *caps = (GstCaps *)user_data;
    GstCaps *writable_caps;
    writable_caps =
        gst_caps_make_writable(gst_static_caps_get(&renderer->caps));
    gst_caps_append(caps, writable_caps);
}

static GstCaps *
cg_gst_build_caps(GSList *renderers)
{
    GstCaps *caps;

    caps = gst_caps_new_empty();

    g_slist_foreach(renderers, append_cap, caps);

    return caps;
}

void
cg_gst_video_sink_set_device(CgGstVideoSink *vt, cg_device_t *dev)
{
    CgGstVideoSinkPrivate *priv = vt->priv;

    if (dev)
        dev = cg_object_ref(dev);

    if (priv->dev) {
        cg_object_unref(priv->dev);
        g_slist_free(priv->renderers);
        priv->renderers = NULL;
        if (priv->caps) {
            gst_caps_unref(priv->caps);
            priv->caps = NULL;
        }
    }

    if (dev) {
        priv->dev = dev;
        priv->renderers = cg_gst_build_renderers_list(priv->dev);
        priv->caps = cg_gst_build_caps(priv->renderers);
    }
}

static CgGstRenderer *
cg_gst_find_renderer_by_format(CgGstVideoSink *sink,
                               CgGstVideoFormat format)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    CgGstRenderer *renderer = NULL;
    GSList *element;

    /* The renderers list is in decreasing order of priority so we'll
     * pick the first one that matches */
    for (element = priv->renderers; element; element = g_slist_next(element)) {
        CgGstRenderer *candidate = (CgGstRenderer *)element->data;
        if (candidate->format == format) {
            renderer = candidate;
            break;
        }
    }

    return renderer;
}

static GstCaps *
cg_gst_video_sink_get_caps(GstBaseSink *bsink, GstCaps *filter)
{
    CgGstVideoSink *sink;
    sink = CG_GST_VIDEO_SINK(bsink);

    if (sink->priv->caps == NULL)
        return NULL;
    else
        return gst_caps_ref(sink->priv->caps);
}

static bool
cg_gst_video_sink_parse_caps(GstCaps *caps, CgGstVideoSink *sink, bool save)
{
    CgGstVideoSinkPrivate *priv = sink->priv;
    GstCaps *intersection;
    GstVideoInfo vinfo;
    CgGstVideoFormat format;
    bool bgr = false;
    CgGstRenderer *renderer;

    intersection = gst_caps_intersect(priv->caps, caps);
    if (gst_caps_is_empty(intersection))
        goto no_intersection;

    gst_caps_unref(intersection);

    if (!gst_video_info_from_caps(&vinfo, caps))
        goto unknown_format;

    switch (vinfo.finfo->format) {
    case GST_VIDEO_FORMAT_YV12:
        format = CG_GST_YV12;
        break;
    case GST_VIDEO_FORMAT_I420:
        format = CG_GST_I420;
        break;
    case GST_VIDEO_FORMAT_AYUV:
        format = CG_GST_AYUV;
        bgr = false;
        break;
    case GST_VIDEO_FORMAT_NV12:
        format = CG_GST_NV12;
        break;
    case GST_VIDEO_FORMAT_RGB:
        format = CG_GST_RGB24;
        bgr = false;
        break;
    case GST_VIDEO_FORMAT_BGR:
        format = CG_GST_RGB24;
        bgr = true;
        break;
    case GST_VIDEO_FORMAT_RGBA:
        format = CG_GST_RGB32;
        bgr = false;
        break;
    case GST_VIDEO_FORMAT_BGRA:
        format = CG_GST_RGB32;
        bgr = true;
        break;
    default:
        goto unhandled_format;
    }

    renderer = cg_gst_find_renderer_by_format(sink, format);

    if (G_UNLIKELY(renderer == NULL))
        goto no_suitable_renderer;

    GST_INFO_OBJECT(sink, "found the %s renderer", renderer->name);

    if (save) {
        priv->info = vinfo;

        priv->format = format;
        priv->bgr = bgr;

        priv->renderer = renderer;
    }

    return true;

no_intersection: {
        GST_WARNING_OBJECT(
            sink,
            "Incompatible caps, don't intersect with %" GST_PTR_FORMAT,
            priv->caps);
        return false;
    }

unknown_format: {
        GST_WARNING_OBJECT(sink, "Could not figure format of input caps");
        return false;
    }

unhandled_format: {
        GST_ERROR_OBJECT(sink, "Provided caps aren't supported by cogl-gst");
        return false;
    }

no_suitable_renderer: {
        GST_ERROR_OBJECT(sink, "could not find a suitable renderer");
        return false;
    }
}

static gboolean
cg_gst_video_sink_set_caps(GstBaseSink *bsink, GstCaps *caps)
{
    CgGstVideoSink *sink;
    CgGstVideoSinkPrivate *priv;

    sink = CG_GST_VIDEO_SINK(bsink);
    priv = sink->priv;

    if (!cg_gst_video_sink_parse_caps(caps, sink, false))
        return false;

    g_mutex_lock(&priv->source->buffer_lock);
    priv->source->has_new_caps = true;
    g_mutex_unlock(&priv->source->buffer_lock);

    return true;
}

static gboolean
cg_gst_source_dispatch(GSource *source, GSourceFunc callback, void *user_data)
{
    CgGstSource *gst_source = (CgGstSource *)source;
    CgGstVideoSinkPrivate *priv = gst_source->sink->priv;
    GstBuffer *buffer;
    gboolean pipeline_ready = false;

    g_mutex_lock(&gst_source->buffer_lock);

    if (G_UNLIKELY(gst_source->has_new_caps)) {
        GstCaps *caps = gst_pad_get_current_caps(
            GST_BASE_SINK_PAD((GST_BASE_SINK(gst_source->sink))));

        if (!cg_gst_video_sink_parse_caps(caps, gst_source->sink, true))
            goto negotiation_fail;

        gst_source->has_new_caps = false;
        priv->free_layer = priv->custom_start + priv->renderer->n_layers;

        dirty_default_pipeline(gst_source->sink);

        /* We are now in a state where we could generate the pipeline if
         * the application requests it so we can emit the signal.
         * However we'll actually generate the pipeline lazily only if
         * the application actually asks for it. */
        pipeline_ready = true;
    }

    buffer = gst_source->buffer;
    gst_source->buffer = NULL;

    g_mutex_unlock(&gst_source->buffer_lock);

    if (buffer) {
        if (!priv->renderer->upload(gst_source->sink, buffer))
            goto fail_upload;

        gst_buffer_unref(buffer);
    } else
        GST_WARNING_OBJECT(gst_source->sink,
                           "No buffers available for display");

    if (G_UNLIKELY(pipeline_ready))
        g_signal_emit(gst_source->sink,
                      video_sink_signals[PIPELINE_READY_SIGNAL],
                      0 /* detail */);
    g_signal_emit(
        gst_source->sink, video_sink_signals[NEW_FRAME_SIGNAL], 0, NULL);

    return true;

negotiation_fail: {
        GST_WARNING_OBJECT(gst_source->sink,
                           "Failed to handle caps. Stopping GSource");
        priv->flow_return = GST_FLOW_NOT_NEGOTIATED;
        g_mutex_unlock(&gst_source->buffer_lock);

        return false;
    }

fail_upload: {
        GST_WARNING_OBJECT(gst_source->sink, "Failed to upload buffer");
        priv->flow_return = GST_FLOW_ERROR;
        gst_buffer_unref(buffer);
        return false;
    }
}

static GSourceFuncs gst_source_funcs = { cg_gst_source_prepare,
                                         cg_gst_source_check,
                                         cg_gst_source_dispatch,
                                         cg_gst_source_finalize };

static CgGstSource *
cg_gst_source_new(CgGstVideoSink *sink)
{
    GSource *source;
    CgGstSource *gst_source;

    source = g_source_new(&gst_source_funcs, sizeof(CgGstSource));
    gst_source = (CgGstSource *)source;

    g_source_set_can_recurse(source, true);
    g_source_set_priority(source, CG_GST_DEFAULT_PRIORITY);

    gst_source->sink = sink;
    g_mutex_init(&gst_source->buffer_lock);
    gst_source->buffer = NULL;

    return gst_source;
}

static void
cg_gst_video_sink_init(CgGstVideoSink *sink)
{
    CgGstVideoSinkPrivate *priv;

    sink->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE(
                     sink, CG_GST_TYPE_VIDEO_SINK, CgGstVideoSinkPrivate);
    priv->custom_start = 0;
    priv->default_sample = true;
    priv->layer_skip_snippet =
        cg_snippet_new(CG_SNIPPET_HOOK_LAYER_FRAGMENT, NULL, NULL);
    cg_snippet_set_replace(priv->layer_skip_snippet, "");
}

static GstFlowReturn
_cg_gst_video_sink_render(GstBaseSink *bsink,
                          GstBuffer *buffer)
{
    CgGstVideoSink *sink = CG_GST_VIDEO_SINK(bsink);
    CgGstVideoSinkPrivate *priv = sink->priv;
    CgGstSource *gst_source = priv->source;

    g_mutex_lock(&gst_source->buffer_lock);

    if (G_UNLIKELY(priv->flow_return != GST_FLOW_OK))
        goto dispatch_flow_ret;

    if (gst_source->buffer)
        gst_buffer_unref(gst_source->buffer);

    gst_source->buffer = gst_buffer_ref(buffer);
    g_mutex_unlock(&gst_source->buffer_lock);

    g_main_context_wakeup(NULL);

    return GST_FLOW_OK;

dispatch_flow_ret: {
        g_mutex_unlock(&gst_source->buffer_lock);
        return priv->flow_return;
    }
}

static void
cg_gst_video_sink_dispose(GObject *object)
{
    CgGstVideoSink *self;
    CgGstVideoSinkPrivate *priv;

    self = CG_GST_VIDEO_SINK(object);
    priv = self->priv;

    clear_frame_textures(self);

    if (priv->pipeline) {
        cg_object_unref(priv->pipeline);
        priv->pipeline = NULL;
    }

    if (priv->caps) {
        gst_caps_unref(priv->caps);
        priv->caps = NULL;
    }

    if (priv->layer_skip_snippet) {
        cg_object_unref(priv->layer_skip_snippet);
        priv->layer_skip_snippet = NULL;
    }

    G_OBJECT_CLASS(cg_gst_video_sink_parent_class)->dispose(object);
}

static void
cg_gst_video_sink_finalize(GObject *object)
{
    CgGstVideoSink *self = CG_GST_VIDEO_SINK(object);

    cg_gst_video_sink_set_device(self, NULL);

    G_OBJECT_CLASS(cg_gst_video_sink_parent_class)->finalize(object);
}

static gboolean
cg_gst_video_sink_start(GstBaseSink *base_sink)
{
    CgGstVideoSink *sink = CG_GST_VIDEO_SINK(base_sink);
    CgGstVideoSinkPrivate *priv = sink->priv;

    priv->source = cg_gst_source_new(sink);
    g_source_attach((GSource *)priv->source, NULL);
    priv->flow_return = GST_FLOW_OK;
    return true;
}

static void
cg_gst_video_sink_set_property(GObject *object,
                               unsigned int prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    CgGstVideoSink *sink = CG_GST_VIDEO_SINK(object);

    switch (prop_id) {
    case PROP_UPDATE_PRIORITY:
        cg_gst_video_sink_set_priority(sink, g_value_get_int(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
cg_gst_video_sink_get_property(GObject *object,
                               unsigned int prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    CgGstVideoSink *sink = CG_GST_VIDEO_SINK(object);
    CgGstVideoSinkPrivate *priv = sink->priv;

    switch (prop_id) {
    case PROP_UPDATE_PRIORITY:
        g_value_set_int(value, g_source_get_priority((GSource *)priv->source));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean
cg_gst_video_sink_stop(GstBaseSink *base_sink)
{
    CgGstVideoSink *sink = CG_GST_VIDEO_SINK(base_sink);
    CgGstVideoSinkPrivate *priv = sink->priv;

    if (priv->source) {
        GSource *source = (GSource *)priv->source;
        g_source_destroy(source);
        g_source_unref(source);
        priv->source = NULL;
    }

    return true;
}

static void
cg_gst_video_sink_class_init(CgGstVideoSinkClass *klass)
{
    GObjectClass *go_class = G_OBJECT_CLASS(klass);
    GstBaseSinkClass *gb_class = GST_BASE_SINK_CLASS(klass);
    GstElementClass *ge_class = GST_ELEMENT_CLASS(klass);
    GstPadTemplate *pad_template;
    GParamSpec *pspec;

    g_type_class_add_private(klass, sizeof(CgGstVideoSinkPrivate));
    go_class->set_property = cg_gst_video_sink_set_property;
    go_class->get_property = cg_gst_video_sink_get_property;
    go_class->dispose = cg_gst_video_sink_dispose;
    go_class->finalize = cg_gst_video_sink_finalize;

    pad_template = gst_static_pad_template_get(&sinktemplate_all);
    gst_element_class_add_pad_template(ge_class, pad_template);

    gst_element_class_set_metadata(
        ge_class,
        "Cogl video sink",
        "Sink/Video",
        "Sends video data from GStreamer to a "
        "Cogl pipeline",
        "Jonathan Matthew <jonathan@kaolin.wh9.net>, "
        "Matthew Allum <mallum@o-hand.com, "
        "Chris Lord <chris@o-hand.com>, "
        "Plamena Manolova "
        "<plamena.n.manolova@intel.com>");

    gb_class->render = _cg_gst_video_sink_render;
    gb_class->preroll = _cg_gst_video_sink_render;
    gb_class->start = cg_gst_video_sink_start;
    gb_class->stop = cg_gst_video_sink_stop;
    gb_class->set_caps = cg_gst_video_sink_set_caps;
    gb_class->get_caps = cg_gst_video_sink_get_caps;

    pspec = g_param_spec_int("update-priority",
                             "Update Priority",
                             "Priority of video updates in the thread",
                             -G_MAXINT,
                             G_MAXINT,
                             CG_GST_DEFAULT_PRIORITY,
                             CG_GST_PARAM_READWRITE);

    g_object_class_install_property(go_class, PROP_UPDATE_PRIORITY, pspec);

    video_sink_signals[PIPELINE_READY_SIGNAL] =
        g_signal_new("pipeline-ready",
                     CG_GST_TYPE_VIDEO_SINK,
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(CgGstVideoSinkClass, pipeline_ready),
                     NULL, /* accumulator */
                     NULL, /* accu_data */
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0 /* n_params */);

    video_sink_signals[NEW_FRAME_SIGNAL] =
        g_signal_new("new-frame",
                     CG_GST_TYPE_VIDEO_SINK,
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(CgGstVideoSinkClass, new_frame),
                     NULL, /* accumulator */
                     NULL, /* accu_data */
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0 /* n_params */);
}

CgGstVideoSink *
cg_gst_video_sink_new(cg_device_t *dev)
{
    CgGstVideoSink *sink = g_object_new(CG_GST_TYPE_VIDEO_SINK, NULL);
    cg_gst_video_sink_set_device(sink, dev);

    return sink;
}

float
cg_gst_video_sink_get_aspect(CgGstVideoSink *vt)
{
    GstVideoInfo *info;

    g_return_val_if_fail(CG_GST_IS_VIDEO_SINK(vt), 0.);

    info = &vt->priv->info;
    return ((float)info->width * (float)info->par_n) /
           ((float)info->height * (float)info->par_d);
}

float
cg_gst_video_sink_get_width_for_height(CgGstVideoSink *vt, float height)
{
    float aspect;

    g_return_val_if_fail(CG_GST_IS_VIDEO_SINK(vt), 0.);

    aspect = cg_gst_video_sink_get_aspect(vt);
    return height * aspect;
}

float
cg_gst_video_sink_get_height_for_width(CgGstVideoSink *vt, float width)
{
    float aspect;

    g_return_val_if_fail(CG_GST_IS_VIDEO_SINK(vt), 0.);

    aspect = cg_gst_video_sink_get_aspect(vt);
    return width / aspect;
}

void
cg_gst_video_sink_fit_size(CgGstVideoSink *vt,
                           const CgGstRectangle *available,
                           CgGstRectangle *output)
{
    g_return_if_fail(CG_GST_IS_VIDEO_SINK(vt));
    g_return_if_fail(available != NULL);
    g_return_if_fail(output != NULL);

    if (available->height == 0.0f) {
        output->x = available->x;
        output->y = available->y;
        output->width = output->height = 0;
    } else {
        float available_aspect = available->width / available->height;
        float video_aspect = cg_gst_video_sink_get_aspect(vt);

        if (video_aspect > available_aspect) {
            output->width = available->width;
            output->height = available->width / video_aspect;
            output->x = available->x;
            output->y = available->y + (available->height - output->height) / 2;
        } else {
            output->width = available->height * video_aspect;
            output->height = available->height;
            output->x = available->x + (available->width - output->width) / 2;
            output->y = available->y;
        }
    }
}

void
cg_gst_video_sink_get_natural_size(CgGstVideoSink *vt,
                                   float *width,
                                   float *height)
{
    GstVideoInfo *info;

    g_return_if_fail(CG_GST_IS_VIDEO_SINK(vt));

    info = &vt->priv->info;

    if (info->par_n > info->par_d) {
        /* To display the video at the right aspect ratio then in this
         * case the pixels need to be stretched horizontally and so we
         * use the unscaled height as our reference.
         */

        if (height)
            *height = info->height;
        if (width)
            *width = cg_gst_video_sink_get_width_for_height(vt, info->height);
    } else {
        if (width)
            *width = info->width;
        if (height)
            *height = cg_gst_video_sink_get_height_for_width(vt, info->width);
    }
}

float
cg_gst_video_sink_get_natural_width(CgGstVideoSink *vt)
{
    float width;
    cg_gst_video_sink_get_natural_size(vt, &width, NULL);
    return width;
}

float
cg_gst_video_sink_get_natural_height(CgGstVideoSink *vt)
{
    float height;
    cg_gst_video_sink_get_natural_size(vt, NULL, &height);
    return height;
}

bool
cg_gst_video_sink_is_ready(CgGstVideoSink *sink)
{
    return !!sink->priv->renderer;
}
