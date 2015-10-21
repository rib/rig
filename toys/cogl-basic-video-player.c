#include <config.h>

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <cglib/cglib.h>
#include <cogl-gst/cogl-gst.h>

typedef struct _Data {
    cg_framebuffer_t *fb;
    cg_pipeline_t *border_pipeline;
    cg_pipeline_t *video_pipeline;
    CgGstVideoSink *sink;
    int onscreen_width;
    int onscreen_height;
    CgGstRectangle video_output;
    bool draw_ready;
    bool frame_ready;
    GMainLoop *main_loop;
} Data;

static gboolean
_bus_watch(GstBus *bus, GstMessage *msg, void *user_data)
{
    Data *data = (Data *)user_data;
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS: {
        g_main_loop_quit(data->main_loop);
        break;
    }
    case GST_MESSAGE_ERROR: {
        char *debug;
        GError *error = NULL;

        gst_message_parse_error(msg, &error, &debug);
        g_free(debug);

        if (error != NULL) {
            g_error("Playback error: %s\n", error->message);
            g_error_free(error);
        }
        g_main_loop_quit(data->main_loop);
        break;
    }
    default:
        break;
    }

    return TRUE;
}

static void
_draw(Data *data)
{
    /*
       The cogl pipeline needs to be retrieved from the sink before every draw.
       This is due to the cogl-gst sink creating a new cogl pipeline for each
       frame
       by copying the previous one and attaching the new frame to it.
     */
    cg_pipeline_t *current = cg_gst_video_sink_get_pipeline(data->sink);

    data->video_pipeline = current;

    if (data->video_output.x) {
        int x = data->video_output.x;

        /* Letterboxed with vertical borders */
        cg_framebuffer_draw_rectangle(
            data->fb, data->border_pipeline, 0, 0, x, data->onscreen_height);
        cg_framebuffer_draw_rectangle(data->fb,
                                      data->border_pipeline,
                                      data->onscreen_width - x,
                                      0,
                                      data->onscreen_width,
                                      data->onscreen_height);
        cg_framebuffer_draw_rectangle(data->fb,
                                      data->video_pipeline,
                                      x,
                                      0,
                                      x + data->video_output.width,
                                      data->onscreen_height);
    } else if (data->video_output.y) {
        int y = data->video_output.y;

        /* Letterboxed with horizontal borders */
        cg_framebuffer_draw_rectangle(
            data->fb, data->border_pipeline, 0, 0, data->onscreen_width, y);
        cg_framebuffer_draw_rectangle(data->fb,
                                      data->border_pipeline,
                                      0,
                                      data->onscreen_height - y,
                                      data->onscreen_width,
                                      data->onscreen_height);
        cg_framebuffer_draw_rectangle(data->fb,
                                      data->video_pipeline,
                                      0,
                                      y,
                                      data->onscreen_width,
                                      y + data->video_output.height);

    } else {
        cg_framebuffer_draw_rectangle(data->fb,
                                      data->video_pipeline,
                                      0,
                                      0,
                                      data->onscreen_width,
                                      data->onscreen_height);
    }

    cg_onscreen_swap_buffers(CG_ONSCREEN(data->fb));
}

static void
_check_draw(Data *data)
{
    /* The frame is only drawn once we know that a new buffer is ready
     * from GStreamer and that Cogl is ready to accept some new
     * rendering */
    if (data->draw_ready && data->frame_ready) {
        _draw(data);
        data->draw_ready = FALSE;
        data->frame_ready = FALSE;
    }
}

static void
_frame_callback(cg_onscreen_t *onscreen,
                cg_frame_event_t event,
                cg_frame_info_t *info,
                void *user_data)
{
    Data *data = user_data;

    if (event == CG_FRAME_EVENT_SYNC) {
        data->draw_ready = TRUE;
        _check_draw(data);
    }
}

static void
_new_frame_cb(CgGstVideoSink *sink, Data *data)
{
    data->frame_ready = TRUE;
    _check_draw(data);
}

static void
_resize_callback(cg_onscreen_t *onscreen,
                 int width,
                 int height,
                 void *user_data)
{
    Data *data = user_data;
    CgGstRectangle available;

    data->onscreen_width = width;
    data->onscreen_height = height;

    cg_framebuffer_orthographic(data->fb, 0, 0, width, height, -1, 100);

    if (!data->video_pipeline)
        return;

    available.x = 0;
    available.y = 0;
    available.width = width;
    available.height = height;
    cg_gst_video_sink_fit_size(data->sink, &available, &data->video_output);
}

/*
   A callback like this should be attached to the cogl-pipeline-ready
   signal. This way requesting the cogl pipeline before its creation
   by the sink is avoided. At this point, user textures and snippets can
   be added to the cogl pipeline.
 */

static void
_set_up_pipeline(gpointer instance, gpointer user_data)
{
    Data *data = (Data *)user_data;

    data->video_pipeline = cg_gst_video_sink_get_pipeline(data->sink);

    /* disable blending... */
    cg_pipeline_set_blend(
        data->video_pipeline, "RGBA = ADD (SRC_COLOR, 0)", NULL);

    /* Now that we know the video size we can perform letterboxing */
    _resize_callback(CG_ONSCREEN(data->fb),
                     data->onscreen_width,
                     data->onscreen_height,
                     data);

    cg_onscreen_add_frame_callback(
        CG_ONSCREEN(data->fb), _frame_callback, data, NULL);

    /*
       The cogl-gst-new-frame signal is emitted when the cogl-gst sink has
       retrieved a new frame and attached it to the cogl pipeline. This can be
       used to make sure cogl doesn't do any unnecessary drawing i.e. keeps to
       the
       frame-rate of the video.
     */

    g_signal_connect(data->sink, "new-frame", G_CALLBACK(_new_frame_cb), data);
}

static bool
is_uri(const char *str)
{
    const char *p = str;

    while (g_ascii_isalpha(*p))
        p++;

    return p > str && g_str_has_prefix(p, "://");
}

static CgGstVideoSink *
find_cg_gst_video_sink(GstElement *element)
{
    GstElement *sink_element = NULL;
    GstIterator *iterator;
    GstElement *iterator_value;
    GValue value;

    if (!GST_IS_BIN(element))
        return NULL;

    iterator = gst_bin_iterate_recurse(GST_BIN(element));

    g_value_init(&value, GST_TYPE_ELEMENT);

    while (gst_iterator_next(iterator, &value) == GST_ITERATOR_OK) {
        iterator_value = g_value_get_object(&value);

        g_value_reset(&value);

        if (CG_GST_IS_VIDEO_SINK(iterator_value)) {
            sink_element = iterator_value;
            break;
        }
    }

    g_value_unset(&value);

    gst_iterator_free(iterator);

    return CG_GST_VIDEO_SINK(sink_element);
}

static bool
make_pipeline_for_uri(cg_device_t *dev,
                      const char *uri,
                      GstElement **pipeline_out,
                      CgGstVideoSink **sink_out,
                      GError **error)
{
    GstElement *pipeline;
    GstElement *bin;
    CgGstVideoSink *sink;
    GError *tmp_error = NULL;

    if (is_uri(uri)) {
        pipeline = gst_pipeline_new("gst-player");
        bin = gst_element_factory_make("playbin", "bin");

        sink = cg_gst_video_sink_new(dev);

        g_object_set(G_OBJECT(bin), "video-sink", GST_ELEMENT(sink), NULL);

        gst_bin_add(GST_BIN(pipeline), bin);

        g_object_set(G_OBJECT(bin), "uri", uri, NULL);
    } else {
        pipeline = gst_parse_launch(uri, &tmp_error);

        if (tmp_error) {
            if (pipeline)
                g_object_unref(pipeline);

            g_propagate_error(error, tmp_error);

            return FALSE;
        }

        sink = find_cg_gst_video_sink(pipeline);

        if (sink == NULL) {
            g_set_error(error,
                        GST_STREAM_ERROR,
                        GST_STREAM_ERROR_FAILED,
                        "The pipeline does not contain a CgGstVideoSink. "
                        "Make sure you add a 'coglsink' element somewhere in "
                        "the pipeline");
            g_object_unref(pipeline);
            return FALSE;
        }

        g_object_ref(sink);

        cg_gst_video_sink_set_device(sink, dev);
    }

    *pipeline_out = pipeline;
    *sink_out = sink;

    return TRUE;
}

int
main(int argc, char **argv)
{
    Data data;
    cg_device_t *dev;
    cg_onscreen_t *onscreen;
    GstElement *pipeline;
    GSource *cg_source;
    GstBus *bus;
    char *uri;
    GError *error = NULL;

    memset(&data, 0, sizeof(Data));

    /* Set the necessary cogl elements */

    dev = cg_device_new();

    onscreen = cg_onscreen_new(dev, 640, 480);
    cg_onscreen_set_resizable(onscreen, TRUE);
    cg_onscreen_add_resize_callback(onscreen, _resize_callback, &data, NULL);
    cg_onscreen_show(onscreen);

    data.fb = onscreen;
    cg_framebuffer_orthographic(data.fb, 0, 0, 640, 480, -1, 100);

    data.border_pipeline = cg_pipeline_new(dev);
    cg_pipeline_set_color4f(data.border_pipeline, 0, 0, 0, 1);
    /* disable blending */
    cg_pipeline_set_blend(
        data.border_pipeline, "RGBA = ADD (SRC_COLOR, 0)", NULL);

    /* Intialize GStreamer */

    gst_init(&argc, &argv);

    /*
       Create the cogl-gst video sink by calling the cg_gst_video_sink_new
       function and passing it a cg_device_t (this is used to create the
       cg_pipeline_t and the texures for each frame). Alternatively you can use
       gst_element_factory_make ("coglsink", "some_name") and then set the
       context with cg_gst_video_sink_set_context.
     */

    if (argc < 2)
        uri = "http://docs.gstreamer.com/media/sintel_trailer-480p.webm";
    else
        uri = argv[1];

    if (!make_pipeline_for_uri(dev, uri, &pipeline, &data.sink, &error)) {
        g_print("Error creating pipeline: %s\n", error->message);
        g_clear_error(&error);
        return EXIT_FAILURE;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_watch(bus, _bus_watch, &data);

    data.main_loop = g_main_loop_new(NULL, FALSE);

    cg_source = cg_glib_source_new(dev, G_PRIORITY_DEFAULT);
    g_source_attach(cg_source, NULL);

    /*
       The cogl-pipeline-ready signal tells you when the cogl pipeline is
       initialized i.e. when cogl-gst has figured out the video format and
       is prepared to retrieve and attach the first frame of the video.
     */

    g_signal_connect(
        data.sink, "pipeline-ready", G_CALLBACK(_set_up_pipeline), &data);

    data.draw_ready = TRUE;
    data.frame_ready = FALSE;

    g_main_loop_run(data.main_loop);

    g_source_destroy(cg_source);
    g_source_unref(cg_source);

    g_main_loop_unref(data.main_loop);

    return 0;
}
