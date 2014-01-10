/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "rut-image-source.h"

struct _RutImageSource
{
  RutObjectBase _base;

  RutContext *ctx;

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
_rut_init_image_source_wrappers_cache (RutContext *ctx)
{
  ctx->image_source_wrappers = g_hash_table_new_full (g_direct_hash,
                                                      g_direct_equal,
                                                      NULL, /* key destroy */
                                                      destroy_source_wrapper);
}

void
_rut_destroy_image_source_wrappers (RutContext *ctx)
{
  g_hash_table_destroy (ctx->image_source_wrappers);
}

static ImageSourceWrappers *
get_image_source_wrappers (RutContext *ctx, int layer_index)
{
  ImageSourceWrappers *wrappers =
    g_hash_table_lookup (ctx->image_source_wrappers,
                         GUINT_TO_POINTER (layer_index));
  char *wrapper;

  if (wrappers)
    return wrappers;

  wrappers = g_slice_new0 (ImageSourceWrappers);

  /* XXX: Note: we use texture2D() instead of the
   * cogl_texture_lookup%i wrapper because the _GLOBALS hook comes
   * before the _lookup functions are emitted by Cogl */
  wrapper = g_strdup_printf ("vec4\n"
                             "rut_image_source_sample%d (vec2 UV)\n"
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
                             "rut_image_source_sample%d (vec2 UV)\n"
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

  g_hash_table_insert (ctx->image_source_wrappers,
                       GUINT_TO_POINTER (layer_index),
                       wrappers);

  return wrappers;
}

static CoglBool
_rut_image_source_video_loop (GstBus *bus,
                              GstMessage *msg,
                              void *data)
{
  RutImageSource *source = (RutImageSource*) data;
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
_rut_image_source_video_stop (RutImageSource *source)
{
  if (source->sink)
    {
      gst_element_set_state (source->pipeline, GST_STATE_NULL);
      gst_object_unref (source->sink);
    }
}

static void
_rut_image_source_video_play (RutImageSource *source,
                              RutContext *ctx,
                              const char *path,
                              const uint8_t *data,
                              size_t len)
{
  GstBus* bus;
  char *uri;
  char *filename = NULL;

  _rut_image_source_video_stop (source);

  source->sink = cogl_gst_video_sink_new (ctx->cogl_context);
  source->pipeline = gst_pipeline_new ("renderer");
  source->bin = gst_element_factory_make ("playbin", NULL);

  if (data && len)
    uri = g_strdup_printf ("mem://%p:%lu", data, (unsigned long)len);
  else
    {
      filename = g_build_filename (ctx->assets_location, path, NULL);
      uri = gst_filename_to_uri (filename, NULL);
    }

  g_object_set (G_OBJECT (source->bin), "video-sink",
                GST_ELEMENT (source->sink), NULL);
  g_object_set (G_OBJECT (source->bin), "uri", uri, NULL);
  gst_bin_add (GST_BIN (source->pipeline), source->bin);

  bus = gst_pipeline_get_bus (GST_PIPELINE (source->pipeline));

  gst_element_set_state (source->pipeline, GST_STATE_PLAYING);
  gst_bus_add_watch (bus, _rut_image_source_video_loop, source);

  g_free (uri);
  if (filename)
    g_free (filename);
  gst_object_unref (bus);
}

static void
_rut_image_source_free (void *object)
{
  RutImageSource *source = object;

  _rut_image_source_video_stop (source);
}

RutType rut_image_source_type;

void
_rut_image_source_init_type (void)
{

  RutType *type = &rut_image_source_type;
#define TYPE RutImageSource

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_image_source_free);

#undef TYPE
}

static void
pipeline_ready_cb (gpointer instance,
                   gpointer user_data)
{
  RutImageSource *source = (RutImageSource*) user_data;

  source->is_video = TRUE;

  rut_closure_list_invoke (&source->ready_cb_list,
                           RutImageSourceReadyCallback,
                           source);
}

static void
new_frame_cb (gpointer instance,
              gpointer user_data)
{
  RutImageSource *source = (RutImageSource*) user_data;

  rut_closure_list_invoke (&source->changed_cb_list,
                           RutImageSourceChangedCallback,
                           source);
}

RutImageSource*
rut_image_source_new (RutContext *ctx,
                      RutAsset *asset)
{
  RutImageSource *source = rut_object_alloc0 (RutImageSource,
                                              &rut_image_source_type,
                                              _rut_image_source_init_type);

  source->ctx = ctx;
  source->sink = NULL;
  source->texture = NULL;
  source->is_video = FALSE;
  source->first_layer = 0;
  source->default_sample = TRUE;

  rut_list_init (&source->changed_cb_list);
  rut_list_init (&source->ready_cb_list);

  if (rut_asset_get_is_video (asset))
    {
      _rut_image_source_video_play (source, ctx,
                                    rut_asset_get_path (asset),
                                    rut_asset_get_data (asset),
                                    rut_asset_get_data_len (asset));
       g_signal_connect (source->sink, "pipeline_ready",
                         (GCallback) pipeline_ready_cb,
                         source);
       g_signal_connect (source->sink, "new_frame",
                         (GCallback) new_frame_cb,
                         source);
    }
  else if (rut_asset_get_texture (asset))
    source->texture = rut_asset_get_texture (asset);

  return source;
}

RutClosure *
rut_image_source_add_ready_callback (RutImageSource *source,
                                     RutImageSourceReadyCallback callback,
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
rut_image_source_get_texture (RutImageSource *source)
{
  return source->texture;
}

CoglGstVideoSink*
rut_image_source_get_sink (RutImageSource *source)
{
  return source->sink;
}

CoglBool
rut_image_source_get_is_video (RutImageSource *source)
{
  return source->is_video;
}

RutClosure *
rut_image_source_add_on_changed_callback (RutImageSource *source,
                                          RutImageSourceChangedCallback callback,
                                          void *user_data,
                                          RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&source->changed_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

void
rut_image_source_set_first_layer (RutImageSource *source,
                                  int first_layer)
{
  source->first_layer = first_layer;
}

void
rut_image_source_set_default_sample (RutImageSource *source,
                                     bool default_sample)
{
  source->default_sample = default_sample;
}

void
rut_image_source_setup_pipeline (RutImageSource *source,
                                 CoglPipeline *pipeline)
{
  CoglSnippet *vertex_snippet;
  CoglSnippet *fragment_snippet;
  ImageSourceWrappers *wrappers =
    get_image_source_wrappers (source->ctx, source->first_layer);

  if (!rut_image_source_get_is_video (source))
    {
      CoglTexture *texture = rut_image_source_get_texture (source);

      cogl_pipeline_set_layer_texture (pipeline, source->first_layer, texture);

      if (!source->default_sample)
        cogl_pipeline_set_layer_combine (pipeline, source->first_layer,
                                         "RGBA=REPLACE(PREVIOUS)", NULL);

      vertex_snippet = wrappers->image_source_vertex_wrapper;
      fragment_snippet = wrappers->image_source_fragment_wrapper;
    }
  else
    {
      CoglGstVideoSink *sink = rut_image_source_get_sink (source);

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
rut_image_source_attach_frame (RutImageSource *source,
                               CoglPipeline *pipeline)
{
  /* NB: For non-video sources we always attach the texture
   * during rut_image_source_setup_pipeline() so we don't
   * have to do anything here.
   */

  if (rut_image_source_get_is_video (source))
    {
      cogl_gst_video_sink_attach_frame (
        rut_image_source_get_sink (source), pipeline);
    }
}
