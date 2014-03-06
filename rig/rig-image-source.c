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

#include "rig-image-source.h"

struct _RigImageSource
{
  RutObjectBase _base;

  RigEngine *engine;

  CoglTexture *texture;

  CoglGstVideoSink *sink;
  GstElement *pipeline;
  GstElement *bin;
  CoglBool is_video;

  int first_layer;
  bool default_sample;

  RutList changed_cb_list;
  RutList ready_cb_list;
};

typedef struct _ImageSourceWrappers
{
  CoglSnippet *image_source_vertex_wrapper;
  CoglSnippet *image_source_fragment_wrapper;

  CoglSnippet *video_source_vertex_wrapper;
  CoglSnippet *video_source_fragment_wrapper;
} ImageSourceWrappers;

static void
destroy_source_wrapper (gpointer data)
{
  ImageSourceWrappers *wrapper = data;

  if (wrapper->image_source_vertex_wrapper)
    cogl_object_unref (wrapper->image_source_vertex_wrapper);
  if (wrapper->image_source_vertex_wrapper)
    cogl_object_unref (wrapper->image_source_vertex_wrapper);

  if (wrapper->video_source_vertex_wrapper)
    cogl_object_unref (wrapper->video_source_vertex_wrapper);
  if (wrapper->video_source_fragment_wrapper)
    cogl_object_unref (wrapper->video_source_fragment_wrapper);

  g_slice_free (ImageSourceWrappers, wrapper);
}

void
_rig_init_image_source_wrappers_cache (RigEngine *engine)
{
  engine->image_source_wrappers =
    g_hash_table_new_full (g_direct_hash,
                           g_direct_equal,
                           NULL, /* key destroy */
                           destroy_source_wrapper);
}

void
_rig_destroy_image_source_wrappers (RigEngine *engine)
{
  g_hash_table_destroy (engine->image_source_wrappers);
}

static ImageSourceWrappers *
get_image_source_wrappers (RigEngine *engine, int layer_index)
{
  ImageSourceWrappers *wrappers =
    g_hash_table_lookup (engine->image_source_wrappers,
                         GUINT_TO_POINTER (layer_index));
  char *wrapper;

  if (wrappers)
    return wrappers;

  wrappers = g_slice_new0 (ImageSourceWrappers);

  /* XXX: Note: we use texture2D() instead of the
   * cogl_texture_lookup%i wrapper because the _GLOBALS hook comes
   * before the _lookup functions are emitted by Cogl */
  wrapper = g_strdup_printf ("vec4\n"
                             "rig_image_source_sample%d (vec2 UV)\n"
                             "{\n"
                             "  return texture2D (cogl_sampler%d, UV);\n"
                             "}\n",
                             layer_index,
                             layer_index);

  wrappers->image_source_vertex_wrapper =
    cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX_GLOBALS, wrapper, NULL);
  wrappers->image_source_fragment_wrapper =
    cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT_GLOBALS, wrapper, NULL);

  g_free (wrapper);

  wrapper = g_strdup_printf ("vec4\n"
                             "rig_image_source_sample%d (vec2 UV)\n"
                             "{\n"
                             "  return cogl_gst_sample_video%d (UV);\n"
                             "}\n",
                             layer_index,
                             layer_index);

  wrappers->video_source_vertex_wrapper =
    cogl_snippet_new (COGL_SNIPPET_HOOK_VERTEX_GLOBALS, wrapper, NULL);
  wrappers->video_source_fragment_wrapper =
    cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT_GLOBALS, wrapper, NULL);

  g_free (wrapper);

  g_hash_table_insert (engine->image_source_wrappers,
                       GUINT_TO_POINTER (layer_index),
                       wrappers);

  return wrappers;
}

static CoglBool
_rig_image_source_video_loop (GstBus *bus,
                              GstMessage *msg,
                              void *data)
{
  RigImageSource *source = (RigImageSource*) data;
  switch (GST_MESSAGE_TYPE(msg))
    {
      case GST_MESSAGE_EOS:
        gst_element_seek (source->pipeline, 1.0, GST_FORMAT_TIME,
                          GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 0,
                          GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
        break;
      default:
        break;
    }
  return TRUE;
}

static void
_rig_image_source_video_stop (RigImageSource *source)
{
  if (source->sink)
    {
      gst_element_set_state (source->pipeline, GST_STATE_NULL);
      gst_object_unref (source->sink);
    }
}

static void
_rig_image_source_video_play (RigImageSource *source,
                              RigEngine *engine,
                              const char *path,
                              const uint8_t *data,
                              size_t len)
{
  GstBus* bus;
  char *uri;
  char *filename = NULL;

  _rig_image_source_video_stop (source);

  source->sink = cogl_gst_video_sink_new (engine->ctx->cogl_context);
  source->pipeline = gst_pipeline_new ("renderer");
  source->bin = gst_element_factory_make ("playbin", NULL);

  if (data && len)
    uri = g_strdup_printf ("mem://%p:%lu", data, (unsigned long)len);
  else
    {
      filename = g_build_filename (engine->ctx->assets_location, path, NULL);
      uri = gst_filename_to_uri (filename, NULL);
    }

  g_object_set (G_OBJECT (source->bin), "video-sink",
                GST_ELEMENT (source->sink), NULL);
  g_object_set (G_OBJECT (source->bin), "uri", uri, NULL);
  gst_bin_add (GST_BIN (source->pipeline), source->bin);

  bus = gst_pipeline_get_bus (GST_PIPELINE (source->pipeline));

  gst_element_set_state (source->pipeline, GST_STATE_PLAYING);
  gst_bus_add_watch (bus, _rig_image_source_video_loop, source);

  g_free (uri);
  if (filename)
    g_free (filename);
  gst_object_unref (bus);
}

static void
_rig_image_source_free (void *object)
{
  RigImageSource *source = object;

  _rig_image_source_video_stop (source);
}

RutType rig_image_source_type;

void
_rig_image_source_init_type (void)
{

  RutType *type = &rig_image_source_type;
#define TYPE RigImageSource

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_image_source_free);

#undef TYPE
}

static void
pipeline_ready_cb (gpointer instance,
                   gpointer user_data)
{
  RigImageSource *source = (RigImageSource*) user_data;

  source->is_video = TRUE;

  rut_closure_list_invoke (&source->ready_cb_list,
                           RigImageSourceReadyCallback,
                           source);
}

static void
new_frame_cb (gpointer instance,
              gpointer user_data)
{
  RigImageSource *source = (RigImageSource*) user_data;

  rut_closure_list_invoke (&source->changed_cb_list,
                           RigImageSourceChangedCallback,
                           source);
}

RigImageSource *
rig_image_source_new (RigEngine *engine,
                      RigAsset *asset)
{
  RigImageSource *source = rut_object_alloc0 (RigImageSource,
                                              &rig_image_source_type,
                                              _rig_image_source_init_type);

  source->engine = engine;
  source->sink = NULL;
  source->texture = NULL;
  source->is_video = FALSE;
  source->first_layer = 0;
  source->default_sample = TRUE;

  rut_list_init (&source->changed_cb_list);
  rut_list_init (&source->ready_cb_list);

  if (rig_asset_get_is_video (asset))
    {
      _rig_image_source_video_play (source, engine,
                                    rig_asset_get_path (asset),
                                    rig_asset_get_data (asset),
                                    rig_asset_get_data_len (asset));
       g_signal_connect (source->sink, "pipeline_ready",
                         (GCallback) pipeline_ready_cb,
                         source);
       g_signal_connect (source->sink, "new_frame",
                         (GCallback) new_frame_cb,
                         source);
    }
  else if (rig_asset_get_texture (asset))
    source->texture = rig_asset_get_texture (asset);

  return source;
}

RutClosure *
rig_image_source_add_ready_callback (RigImageSource *source,
                                     RigImageSourceReadyCallback callback,
                                     void *user_data,
                                     RutClosureDestroyCallback destroy_cb)
{
  if (source->texture)
    {
      callback (source, user_data);
      return NULL;
    }
  else
    return rut_closure_list_add (&source->ready_cb_list, callback, user_data,
                                 destroy_cb);
}

CoglTexture*
rig_image_source_get_texture (RigImageSource *source)
{
  return source->texture;
}

CoglGstVideoSink*
rig_image_source_get_sink (RigImageSource *source)
{
  return source->sink;
}

CoglBool
rig_image_source_get_is_video (RigImageSource *source)
{
  return source->is_video;
}

RutClosure *
rig_image_source_add_on_changed_callback (RigImageSource *source,
                                          RigImageSourceChangedCallback callback,
                                          void *user_data,
                                          RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&source->changed_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

void
rig_image_source_set_first_layer (RigImageSource *source,
                                  int first_layer)
{
  source->first_layer = first_layer;
}

void
rig_image_source_set_default_sample (RigImageSource *source,
                                     bool default_sample)
{
  source->default_sample = default_sample;
}

void
rig_image_source_setup_pipeline (RigImageSource *source,
                                 CoglPipeline *pipeline)
{
  CoglSnippet *vertex_snippet;
  CoglSnippet *fragment_snippet;
  ImageSourceWrappers *wrappers =
    get_image_source_wrappers (source->engine, source->first_layer);

  if (!rig_image_source_get_is_video (source))
    {
      CoglTexture *texture = rig_image_source_get_texture (source);

      cogl_pipeline_set_layer_texture (pipeline, source->first_layer, texture);

      if (!source->default_sample)
        cogl_pipeline_set_layer_combine (pipeline, source->first_layer,
                                         "RGBA=REPLACE(PREVIOUS)", NULL);

      vertex_snippet = wrappers->image_source_vertex_wrapper;
      fragment_snippet = wrappers->image_source_fragment_wrapper;
    }
  else
    {
      CoglGstVideoSink *sink = rig_image_source_get_sink (source);

      cogl_gst_video_sink_set_first_layer (sink, source->first_layer);
      cogl_gst_video_sink_set_default_sample (sink, TRUE);
      cogl_gst_video_sink_setup_pipeline (sink, pipeline);

      vertex_snippet = wrappers->video_source_vertex_wrapper;
      fragment_snippet = wrappers->video_source_fragment_wrapper;
    }

  cogl_pipeline_add_snippet (pipeline, vertex_snippet);
  cogl_pipeline_add_snippet (pipeline, fragment_snippet);
}

void
rig_image_source_attach_frame (RigImageSource *source,
                               CoglPipeline *pipeline)
{
  /* NB: For non-video sources we always attach the texture
   * during rig_image_source_setup_pipeline() so we don't
   * have to do anything here.
   */

  if (rig_image_source_get_is_video (source))
    {
      cogl_gst_video_sink_attach_frame (
        rig_image_source_get_sink (source), pipeline);
    }
}
