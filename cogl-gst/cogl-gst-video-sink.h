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

#ifndef __CG_GST_VIDEO_SINK_H__
#define __CG_GST_VIDEO_SINK_H__
#include <glib-object.h>
#include <gst/base/gstbasesink.h>

/* We just need the public Cogl api for cogl-gst but we first need to
 * undef CG_COMPILATION to avoid getting an error that normally
 * checks cogl.h isn't used internally. */
#ifdef CG_COMPILATION
#undef CG_COMPILATION
#endif

#include <cogl/cogl.h>

#include <cogl/cogl.h>

/**
 * SECTION:cogl-gst-video-sink
 * @short_description: A video sink for integrating a GStreamer
 *   pipeline with a Cogl pipeline.
 *
 * #CgGstVideoSink is a subclass of #GstBaseSink which can be used to
 * create a #cg_pipeline_t for rendering the frames of the video.
 *
 * To create a basic video player, an application can create a
 * #GstPipeline as normal using gst_pipeline_new() and set the
 * sink on it to one created with cg_gst_video_sink_new(). The
 * application can then listen for the #CgGstVideoSink::new-frame
 * signal which will be emitted whenever there are new textures ready
 * for rendering. For simple rendering, the application can just call
 * cg_gst_video_sink_get_pipeline() in the signal handler and use
 * the returned pipeline to paint the new frame.
 *
 * An application is also free to do more advanced rendering by
 * customizing the pipeline. In that case it should listen for the
 * #CgGstVideoSink::pipeline-ready signal which will be emitted as
 * soon as the sink has determined enough information about the video
 * to know how it should be rendered. In the handler for this signal,
 * the application can either make modifications to a copy of the
 * pipeline returned by cg_gst_video_sink_get_pipeline() or it can
 * create its own pipeline from scratch and ask the sink to configure
 * it with cg_gst_video_sink_setup_pipeline(). If a custom pipeline
 * is created using one of these methods then the application should
 * call cg_gst_video_sink_attach_frame() on the pipeline before
 * rendering in order to update the textures on the pipeline's layers.
 *
 * If the %CG_FEATURE_ID_GLSL feature is available then the pipeline
 * used by the sink will have a shader snippet with a function in it
 * called cg_gst_sample_video0 which takes a single vec2 argument.
 * This can be used by custom snippets set the by the application to
 * sample from the video. The vec2 argument represents the normalised
 * coordinates within the video. The function returns a vec4
 * containing a pre-multiplied RGBA color of the pixel within the
 * video.
 *
 */

G_BEGIN_DECLS

#define CG_GST_TYPE_VIDEO_SINK cg_gst_video_sink_get_type()

#define CG_GST_VIDEO_SINK(obj)                                                 \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), CG_GST_TYPE_VIDEO_SINK, CgGstVideoSink))

#define CG_GST_VIDEO_SINK_CLASS(klass)                                         \
    (G_TYPE_CHECK_CLASS_CAST(                                                  \
         (klass), CG_GST_TYPE_VIDEO_SINK, CgGstVideoSinkClass))

#define CG_GST_IS_VIDEO_SINK(obj)                                              \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), CG_GST_TYPE_VIDEO_SINK))

#define CG_GST_IS_VIDEO_SINK_CLASS(klass)                                      \
    (G_TYPE_CHECK_CLASS_TYPE((klass), CG_GST_TYPE_VIDEO_SINK))

#define CG_GST_VIDEO_SINK_GET_CLASS(obj)                                       \
    (G_TYPE_INSTANCE_GET_CLASS(                                                \
         (obj), CG_GST_TYPE_VIDEO_SINK, CgGstVideoSinkClass))

typedef struct _CgGstVideoSink CgGstVideoSink;
typedef struct _CgGstVideoSinkClass CgGstVideoSinkClass;
typedef struct _CgGstVideoSinkPrivate CgGstVideoSinkPrivate;

/**
 * CgGstVideoSink:
 *
 * The #CgGstVideoSink structure contains only private data and
 * should be accessed using the provided API.
 *
 */
struct _CgGstVideoSink {
    /*< private >*/
    GstBaseSink parent;
    CgGstVideoSinkPrivate *priv;
};

/**
 * CgGstVideoSinkClass:
 * @new_frame: handler for the #CgGstVideoSink::new-frame signal
 * @pipeline_ready: handler for the #CgGstVideoSink::pipeline-ready signal
 *
 */

/**
 * CgGstVideoSink::new-frame:
 * @sink: the #CgGstVideoSink
 *
 * The sink will emit this signal whenever there are new textures
 * available for a new frame of the video. After this signal is
 * emitted, an application can call cg_gst_video_sink_get_pipeline()
 * to get a pipeline suitable for rendering the frame. If the
 * application is using a custom pipeline it can alternatively call
 * cg_gst_video_sink_attach_frame() to attach the textures.
 *
 */

/**
 * CgGstVideoSink::pipeline-ready:
 * @sink: the #CgGstVideoSink
 *
 * The sink will emit this signal as soon as it has gathered enough
 * information from the video to configure a pipeline. If the
 * application wants to do some customized rendering, it can setup its
 * pipeline after this signal is emitted. The application's pipeline
 * will typically either be a copy of the one returned by
 * cg_gst_video_sink_get_pipeline() or it can be a completely custom
 * pipeline which is setup using cg_gst_video_sink_setup_pipeline().
 *
 * Note that it is an error to call either of those functions before
 * this signal is emitted. The #CgGstVideoSink::new-frame signal
 * will only be emitted after the pipeline is ready so the application
 * could also create its pipeline in the handler for that.
 *
 */

struct _CgGstVideoSinkClass {
    /*< private >*/
    GstBaseSinkClass parent_class;

    /*< public >*/
    void (*new_frame)(CgGstVideoSink *sink);
    void (*pipeline_ready)(CgGstVideoSink *sink);

    /*< private >*/
    void *_padding_dummy[8];
};

GType cg_gst_video_sink_get_type(void) G_GNUC_CONST;

/**
 * cg_gst_video_sink_new:
 * @dev: The #cg_device_t
 *
 * Creates a new #CgGstVideoSink which will create resources for use
 * with the given context.
 *
 * Return value: (transfer full): a new #CgGstVideoSink
 */
CgGstVideoSink *cg_gst_video_sink_new(cg_device_t *dev);

/**
 * cg_gst_video_sink_is_ready:
 * @sink: The #CgGstVideoSink
 *
 * Returns whether the pipeline is ready and so
 * cg_gst_video_sink_get_pipeline() and
 * cg_gst_video_sink_setup_pipeline() can be called without causing error.
 *
 * Note: Normally an application will wait until the
 * #CgGstVideoSink::pipeline-ready signal is emitted instead of
 * polling the ready status with this api, but sometimes when a sink
 * is passed between components that didn't have an opportunity to
 * connect a signal handler this can be useful.
 *
 * Return value: %true if the sink is ready, else %false
 */
bool cg_gst_video_sink_is_ready(CgGstVideoSink *sink);

/**
 * cg_gst_video_sink_get_pipeline:
 * @vt: The #CgGstVideoSink
 *
 * Returns a pipeline suitable for rendering the current frame of the
 * given video sink. The pipeline will already have the textures for
 * the frame attached. For simple rendering, an application will
 * typically call this function immediately before it paints the
 * video. It can then just paint a rectangle using the returned
 * pipeline.
 *
 * An application is free to make a copy of this
 * pipeline and modify it for custom rendering.
 *
 * Note: it is considered an error to call this function before the
 * #CgGstVideoSink::pipeline-ready signal is emitted.
 *
 * Return value: (transfer none): the pipeline for rendering the
 *   current frame
 */
cg_pipeline_t *cg_gst_video_sink_get_pipeline(CgGstVideoSink *vt);

/**
 * cg_gst_video_sink_set_device:
 * @vt: The #CgGstVideoSink
 * @dev: The #cg_device_t for the sink to use
 *
 * Sets the #cg_device_t that the video sink should use for creating
 * any resources. This function would normally only be used if the
 * sink was constructed via gst_element_factory_make() instead of
 * cg_gst_video_sink_new().
 *
 */
void cg_gst_video_sink_set_device(CgGstVideoSink *vt, cg_device_t *dev);

/**
 * cg_gst_video_sink_get_free_layer:
 * @sink: The #CgGstVideoSink
 *
 * This can be used when doing specialised rendering of the video by
 * customizing the pipeline. #CgGstVideoSink may use up to three
 * private layers on the pipeline in order to attach the textures of
 * the video frame. This function will return the index of the next
 * available unused layer after the sink's internal layers. This can
 * be used by the application to add additional layers, for example to
 * blend in another color in the fragment processing.
 *
 * Return value: the index of the next available layer after the
 *   sink's internal layers.
 */
int cg_gst_video_sink_get_free_layer(CgGstVideoSink *sink);

/**
 * cg_gst_video_sink_attach_frame:
 * @sink: The #CgGstVideoSink
 * @pln: A #cg_pipeline_t
 *
 * Updates the given pipeline with the textures for the current frame.
 * This can be used if the application wants to customize the
 * rendering using its own pipeline. Typically this would be called in
 * response to the #CgGstVideoSink::new-frame signal which is
 * emitted whenever the new textures are available. The application
 * would then make a copy of its template pipeline and call this to
 * set the textures.
 *
 */
void cg_gst_video_sink_attach_frame(CgGstVideoSink *sink, cg_pipeline_t *pln);

/**
 * cg_gst_video_sink_set_first_layer:
 * @sink: The #CgGstVideoSink
 * @first_layer: The new first layer
 *
 * Sets the index of the first layer that the sink will use for its
 * rendering. This is useful if the application wants to have custom
 * layers that appear before the layers added by the sink. In that
 * case by default the sink's layers will be modulated with the result
 * of the application's layers that come before @first_layer.
 *
 * Note that if this function is called then the name of the function
 * to call in the shader snippets to sample the video will also
 * change. For example, if @first_layer is three then the function
 * will be cg_gst_sample_video3.
 *
 */
void cg_gst_video_sink_set_first_layer(CgGstVideoSink *sink, int first_layer);

/**
 * cg_gst_video_sink_set_default_sample:
 * @sink: The #CgGstVideoSink
 * @default_sample: Whether to add the default sampling
 *
 * By default the pipeline generated by
 * cg_gst_video_sink_setup_pipeline() and
 * cg_gst_video_sink_get_pipeline() will have a layer with a shader
 * snippet that automatically samples the video. If the application
 * wants to sample the video in a completely custom way using its own
 * shader snippet it can set @default_sample to %false to avoid this
 * default snippet being added. In that case the application's snippet
 * can call cg_gst_sample_video0 to sample the texture itself.
 *
 */
void cg_gst_video_sink_set_default_sample(CgGstVideoSink *sink,
                                          bool default_sample);

/**
 * cg_gst_video_sink_setup_pipeline:
 * @sink: The #CgGstVideoSink
 * @pipeline: A #cg_pipeline_t
 *
 * Configures the given pipeline so that will be able to render the
 * video for the @sink. This should only be used if the application
 * wants to perform some custom rendering using its own pipeline.
 * Typically an application will call this in response to the
 * #CgGstVideoSink::pipeline-ready signal.
 *
 * Note: it is considered an error to call this function before the
 * #CgGstVideoSink::pipeline-ready signal is emitted.
 *
 */
void cg_gst_video_sink_setup_pipeline(CgGstVideoSink *sink,
                                      cg_pipeline_t *pipeline);

/**
 * cg_gst_video_sink_get_aspect:
 * @sink: A #CgGstVideoSink
 *
 * Returns a width-for-height aspect ratio that lets you calculate a
 * suitable width for displaying your video based on a given height by
 * multiplying your chosen height by the returned aspect ratio.
 *
 * This aspect ratio is calculated based on the underlying size of the
 * video buffers and the current pixel-aspect-ratio.
 *
 * Return value: a width-for-height aspect ratio
 *
 * Stability: unstable
 */
float cg_gst_video_sink_get_aspect(CgGstVideoSink *sink);

/**
 * cg_gst_video_sink_get_width_for_height:
 * @sink: A #CgGstVideoSink
 * @height: A specific output @height
 *
 * Calculates a suitable output width for a specific output @height
 * that will maintain the video's aspect ratio.
 *
 * Return value: An output width for the given output @height.
 *
 * Stability: unstable
 */
float cg_gst_video_sink_get_width_for_height(CgGstVideoSink *sink,
                                             float height);

/**
 * cg_gst_video_sink_get_height_for_width:
 * @sink: A #CgGstVideoSink
 * @width: A specific output @width
 *
 * Calculates a suitable output height for a specific output @width
 * that will maintain the video's aspect ratio.
 *
 * Return value: An output height for the given output @width.
 *
 * Stability: unstable
 */
float cg_gst_video_sink_get_height_for_width(CgGstVideoSink *sink, float width);

/**
 * cg_gst_video_sink_get_natural_size:
 * @sink: A #CgGstVideoSink
 * @width: (out): return location for the video's natural width
 * @height: (out): return location for the video's natural height
 *
 * Considering the real resolution of the video as well as the aspect
 * ratio of pixel data that may need to be stretched when being displayed;
 * this function calculates what the natural size of the underlying
 * video source is.
 *
 * The natural size has the correct aspect ratio for displaying the
 * video and is the minimum size where downscaling is not required.
 *
 * <note>This natural size is calculated assuming that the video will
 * be displayed on square pixels.</note>
 *
 * Stability: unstable
 */
void cg_gst_video_sink_get_natural_size(CgGstVideoSink *sink,
                                        float *width,
                                        float *height);

/**
 * cg_gst_video_sink_get_natural_width:
 * @sink: A #CgGstVideoSink
 *
 * Considering the real resolution of the video as well as the aspect
 * ratio of pixel data that may need to be stretched when being displayed;
 * this function calculates what the natural size of the underlying
 * video source is, and returns its width.
 *
 * The natural size has the correct aspect ratio for displaying the
 * video and is the minimum size where downscaling is not required.
 *
 * <note>This natural size is calculated assuming that the video will
 * be displayed on square pixels.</note>
 *
 * Return value: The video's natural width
 *
 * Stability: unstable
 */
float cg_gst_video_sink_get_natural_width(CgGstVideoSink *sink);

/**
 * cg_gst_video_sink_get_natural_height:
 * @sink: A #CgGstVideoSink
 *
 * Considering the real resolution of the video as well as the aspect
 * ratio of pixel data that may need to be stretched when being displayed;
 * this function calculates what the natural size of the underlying
 * video source is, and returns its height.
 *
 * The natural size has the correct aspect ratio for displaying the
 * video and is the minimum size where downscaling is not required.
 *
 * <note>This natural size is calculated assuming that the video will
 * be displayed on square pixels.</note>
 *
 * Return value: The video's natural height
 *
 * Stability: unstable
 */
float cg_gst_video_sink_get_natural_height(CgGstVideoSink *sink);

/**
 * CgGstRectangle:
 * @x: The X coordinate of the top left of the rectangle
 * @y: The Y coordinate of the top left of the rectangle
 * @width: The width of the rectangle
 * @height: The height of the rectangle
 *
 * Describes a rectangle that can be used for video output.
 */
typedef struct _CgGstRectangle {
    float x;
    float y;
    float width;
    float height;
} CgGstRectangle;

/**
 * cg_gst_video_sink_fit_size:
 * @sink: A #CgGstVideoSink
 * @available: (in): The space available for video output
 * @output: (inout): The return location for the calculated output position
 *
 * Calculates a suitable @output rectangle that can fit inside the
 * @available space while maintaining the aspect ratio of the current
 * video.
 *
 * Applications would typically use this api for "letterboxing" by
 * using this api to position a video inside a fixed screen space and
 * filling the remaining space with black borders.
 *
 * Stability: unstable
 */
void cg_gst_video_sink_fit_size(CgGstVideoSink *sink,
                                const CgGstRectangle *available,
                                CgGstRectangle *output);

G_END_DECLS

#endif
