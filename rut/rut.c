/*
 * Rut
 *
 * A tiny toolkit
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>

#ifdef USE_GTK
#include <gdk/gdkx.h>
#include <cogl/cogl-xlib.h>
#endif

/* FIXME: we should have a config.h where things like USE_SDL would
 * be defined instead of defining that in rut.h */
#include "rut.h"

#include "rut-bitmask.h"
#include "rut-global.h"
#include "rut-context.h"
#include "rut-camera-private.h"
#include "rut-transform-private.h"
#include "rut-text.h"
#include "rut-timeline.h"
#include "rut-text-buffer.h"
#include "rut-entity.h"
#include "rut-components.h"
#include "rut-geometry.h"
#include "rut-scroll-bar.h"

/*
 * Overall issues to keep in mind for the UI scenegraph support in Rut:
 * (in no particular order)
 *
 * How does it handle batching geometry?
 * - How does it handle tiny primitives that can't be efficiently handled using
 *   the GPU.
 * - How does it reorder primitives to avoid state changes?
 * - How does it avoid redundant overdraw?
 * How does it handle culling?
 * How does it track damage regions?
 * How does it handle incremental screen updates?
 * How does it handle anti-aliasing?
 * How does it handle filter effects; blur, desaturate etc?
 * How does it integrate video efficiently?
 * How does it handle animations?
 * How does it ensure the GL driver can't block the application?
 * How does it ensure the application can't block animations?
 * How flexible is the rendering model?
 *  - Is cloning nodes a core part of the scene graph design?
 *  - Is the graph acyclic, or does it allow recursion?
 *
 * Note: Rut doesn't actually tackle any of these particularly well currently
 * and may never since it's currently just wanted as a minimal toolkit to
 * implement the UI of Rig and isn't intended to run on a real device. Never the
 * less they are things to keep in mind when shaping the code on the off-chance
 * that something interesting comes out of it.
 *
 * One quite nice thing about this code is the simple approach to interface
 * oriented programming:
 *
 * - Interfaces are defined by the combination of a struct typedef of function
 *   pointers (vtable) and a struct typedef of per-instance properties that are
 *   both optional.
 *
 * - Types are variables that have a bitmask of supported interfaces and an
 *   array indexable up to the highest offset bit in the bitmask. Each entry
 *   contains a pointer to an interface vtable and byte-offset that can be used
 *   to access interface properties associated with an instance.
 *
 * - The base object just contains a single "type" pointer (which could
 *   potentially be changed dynamically at runtime to add/remove interfaces).
 *
 * - Checking if an object implements an interface as well as calling through
 *   the interface vtable or accessing interface properties can be done in O(1)
 *   time.
 *
 * Some of the interfaces defined currently for Rut are:
 * "RefCountable"
 *  - implies an int ref_count property and ref, unref, free methods
 * "Graphable"
 *  - implies parent and children properties but no methods
 * "Paintable"
 *  - no properties implied, just a paint() method to draw with Cogl
 *
 */

struct _RutGraph
{
  RutObjectProps _parent;
  int ref_count;

  RutGraphableProps graphable;
};

struct _RutNineSlice
{
  RutObjectProps _parent;
  int ref_count;

  CoglTexture *texture;

  float left;
  float right;
  float top;
  float bottom;

  float width;
  float height;

  CoglPipeline *pipeline;

  RutGraphableProps graphable;
  RutPaintableProps paintable;
};

struct _RutRectangle
{
  RutObjectProps _parent;
  int ref_count;

  float width;
  float height;

  RutSimpleWidgetProps simple_widget;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  CoglPipeline *pipeline;

};

typedef struct _RutTextureCacheEntry
{
  RutContext *ctx;
  char *filename;
  CoglTexture *texture;
} RutTextureCacheEntry;
#define RUT_TEXTURE_CACHE_ENTRY(X) ((RutTextureCacheEntry *)X)

CoglContext *rut_cogl_context;

static CoglUserDataKey texture_cache_key;

uint8_t _rut_nine_slice_indices_data[] = {
    0,4,5,   0,5,1,  1,5,6,   1,6,2,   2,6,7,    2,7,3,
    4,8,9,   4,9,5,  5,9,10,  5,10,6,  6,10,11,  6,11,7,
    8,12,13, 8,13,9, 9,13,14, 9,14,10, 10,14,15, 10,15,11
};

RutUIEnum _rut_projection_ui_enum = {
  .nick = "Projection",
  .values = {
    { RUT_PROJECTION_PERSPECTIVE, "Perspective", "Perspective Projection" },
    { RUT_PROJECTION_ORTHOGRAPHIC, "Orthographic", "Orthographic Projection" },
    { 0 }
  }
};

typedef struct _SettingsChangedCallbackState
{
  RutSettingsChangedCallback callback;
  GDestroyNotify destroy_notify;
  void *user_data;
} SettingsChangedCallbackState;

struct _RutSettings
{
  GList *changed_callbacks;
};

static void
_rut_settings_free (RutSettings *settings)
{
  GList *l;

  for (l = settings->changed_callbacks; l; l = l->next)
    g_slice_free (SettingsChangedCallbackState, l->data);
  g_list_free (settings->changed_callbacks);
}

RutSettings *
rut_settings_new (void)
{
  return g_slice_new0 (RutSettings);
}

void
rut_settings_add_changed_callback (RutSettings *settings,
                                   RutSettingsChangedCallback callback,
                                   GDestroyNotify destroy_notify,
                                   void *user_data)
{
  GList *l;
  SettingsChangedCallbackState *state;

  for (l = settings->changed_callbacks; l; l = l->next)
    {
      state = l->data;

      if (state->callback == callback)
        {
          state->user_data = user_data;
          state->destroy_notify = destroy_notify;
          return;
        }
    }

  state = g_slice_new (SettingsChangedCallbackState);
  state->callback = callback;
  state->destroy_notify = destroy_notify;
  state->user_data = user_data;

  settings->changed_callbacks = g_list_prepend (settings->changed_callbacks,
                                                state);
}

void
rut_settings_remove_changed_callback (RutSettings *settings,
                                      RutSettingsChangedCallback callback)
{
  GList *l;

  for (l = settings->changed_callbacks; l; l = l->next)
    {
      SettingsChangedCallbackState *state = l->data;

      if (state->callback == callback)
        {
          settings->changed_callbacks =
            g_list_delete_link (settings->changed_callbacks, l);
          g_slice_free (SettingsChangedCallbackState, state);
          return;
        }
    }
}

/* FIXME HACK */
unsigned int
rut_settings_get_password_hint_time (RutSettings *settings)
{
  return 10;
}

char *
rut_settings_get_font_name (RutSettings *settings)
{
  return g_strdup ("Sans 12");
}

static void
_rut_context_free (void *object)
{
  RutContext *ctx = object;

  rut_property_context_destroy (&ctx->property_ctx);

  g_object_unref (ctx->pango_context);
  g_object_unref (ctx->pango_font_map);
  pango_font_description_free (ctx->pango_font_desc);

  g_hash_table_destroy (ctx->texture_cache);

  if (rut_cogl_context == ctx->cogl_context)
    {
      cogl_object_unref (rut_cogl_context);
      rut_cogl_context = NULL;
    }

  cogl_object_unref (ctx->cogl_context);

  _rut_settings_free (ctx->settings);

  g_slice_free (RutContext, ctx);
}

static RutRefCountableVTable _rut_context_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_context_free
};

RutType rut_context_type;

static void
_rut_context_init_type (void)
{
  rut_type_init (&rut_context_type, "RigContext");
  rut_type_add_interface (&rut_context_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutContext, ref_count),
                          &_rut_context_ref_countable_vtable);
}

static void
_rut_texture_cache_entry_free (RutTextureCacheEntry *entry)
{
  g_free (entry->filename);
  g_slice_free (RutTextureCacheEntry, entry);
}

static void
_rut_texture_cache_entry_destroy_cb (void *entry)
{
  _rut_texture_cache_entry_free (entry);
}

static void
texture_destroyed_cb (void *user_data)
{
  RutTextureCacheEntry *entry = user_data;

  g_hash_table_remove (entry->ctx->texture_cache, entry->filename);
  _rut_texture_cache_entry_free (entry);
}

char *
rut_find_data_file (const char *base_filename)
{
  const gchar *const *dirs = g_get_system_data_dirs ();
  const gchar *const *dir;

  for (dir = dirs; *dir; dir++)
    {
      char *full_path =
        g_build_filename (*dir, "rig", base_filename, NULL);

      if (g_file_test (full_path, G_FILE_TEST_EXISTS))
        return full_path;

      g_free (full_path);
    }

  return NULL;
}

CoglTexture *
rut_load_texture (RutContext *ctx, const char *filename, CoglError **error)
{
  RutTextureCacheEntry *entry =
    g_hash_table_lookup (ctx->texture_cache, filename);
  CoglTexture *texture;

  if (entry)
    return cogl_object_ref (entry->texture);

  texture = cogl_texture_new_from_file (ctx->cogl_context,
                                        filename,
                                        COGL_TEXTURE_NO_SLICING,
                                        COGL_PIXEL_FORMAT_ANY,
                                        error);
  if (!texture)
    return NULL;

  entry = g_slice_new0 (RutTextureCacheEntry);
  entry->ctx = ctx;
  entry->filename = g_strdup (filename);
  /* Note: we don't take a reference on the texture. The aim of this
   * cache is simply to avoid multiple loads of the same file and
   * doesn't affect the lifetime of the tracked textures. */
  entry->texture = texture;

  /* Track when the texture is freed... */
  cogl_object_set_user_data (COGL_OBJECT (texture),
                             &texture_cache_key,
                             entry,
                             texture_destroyed_cb);

  return texture;
}

CoglTexture *
rut_load_texture_from_data_file (RutContext *ctx,
                                 const char *filename,
                                 GError **error)
{
  char *full_path = rut_find_data_file (filename);
  CoglTexture *tex;

  if (full_path == NULL)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   G_FILE_ERROR_NOENT,
                   "File not found");
      return NULL;
    }

#ifndef COGL_HAS_GLIB_SUPPORT
#warning "Rig relies on Cogl being built with glib support, assuming CoglError == GError"
#endif
  tex = rut_load_texture (ctx, full_path, (CoglError **) error);

  g_free (full_path);

  return tex;
}

RutContext *
rut_context_new (RutShell *shell)
{
  RutContext *context = g_new0 (RutContext, 1);
  CoglError *error = NULL;

  _rut_init ();

  rut_object_init (&context->_parent, &rut_context_type);

  context->ref_count = 1;

#ifdef USE_SDL
  context->cogl_context = cogl_sdl_context_new (SDL_USEREVENT, &error);
#elif defined (USE_GTK)
  {
    /* We need to force Cogl to use xlib if we're using GTK */
    CoglRenderer *renderer;
    GdkDisplay *gdk_display;
    Display *xlib_display;
    CoglDisplay *display;

    gdk_display = gdk_display_get_default ();
    xlib_display = gdk_x11_display_get_xdisplay (gdk_display);

    renderer = cogl_renderer_new ();
    cogl_renderer_add_constraint (renderer, COGL_RENDERER_CONSTRAINT_USES_XLIB);
    cogl_xlib_renderer_set_foreign_display (renderer,
                                            xlib_display);

    display = cogl_display_new (renderer, NULL /* no onscreen template */);

    context->cogl_context = cogl_context_new (display, &error);

    cogl_object_unref (display);
    cogl_object_unref (renderer);
  }
#else
  context->cogl_context = cogl_context_new (NULL, &error);
#endif
  if (!context->cogl_context)
    {
      g_warning ("Failed to create Cogl Context: %s", error->message);
      g_free (context);
      return NULL;
    }

  /* We set up the first created RutContext as a global default context */
  if (rut_cogl_context == NULL)
    rut_cogl_context = cogl_object_ref (context->cogl_context);

  context->settings = rut_settings_new ();

  context->texture_cache =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           NULL,
                           _rut_texture_cache_entry_destroy_cb);

  context->nine_slice_indices =
    cogl_indices_new (context->cogl_context,
                      COGL_INDICES_TYPE_UNSIGNED_BYTE,
                      _rut_nine_slice_indices_data,
                      sizeof (_rut_nine_slice_indices_data) /
                      sizeof (_rut_nine_slice_indices_data[0]));

  context->single_texture_2d_template =
    cogl_pipeline_new (context->cogl_context);
  cogl_pipeline_set_layer_null_texture (context->single_texture_2d_template,
                                        0, COGL_TEXTURE_TYPE_2D);

  context->circle_texture =
    rut_create_circle_texture (context,
                               CIRCLE_TEX_RADIUS /* radius */,
                               CIRCLE_TEX_PADDING /* padding */);

  cogl_matrix_init_identity (&context->identity_matrix);

  context->pango_font_map =
    COGL_PANGO_FONT_MAP (cogl_pango_font_map_new (context->cogl_context));

  cogl_pango_font_map_set_use_mipmapping (context->pango_font_map, TRUE);

  context->pango_context =
    pango_font_map_create_context (PANGO_FONT_MAP (context->pango_font_map));

  context->pango_font_desc = pango_font_description_new ();
  pango_font_description_set_family (context->pango_font_desc, "Sans");
  pango_font_description_set_size (context->pango_font_desc, 14 * PANGO_SCALE);

  rut_property_context_init (&context->property_ctx);

  if (shell)
    {
      context->shell = rut_refable_ref (shell);

      _rut_shell_associate_context (shell, context);
    }

  return context;
}

void
rut_context_init (RutContext *context)
{
  if (context->shell)
    _rut_shell_init (context->shell);
}

RutTextDirection
rut_get_text_direction (RutContext *context)
{
  return RUT_TEXT_DIRECTION_LEFT_TO_RIGHT;
}

void
rut_set_assets_location (RutContext *context,
                         const char *assets_location)
{
  context->assets_location = g_strdup (assets_location);
}

static void
_rut_nine_slice_free (void *object)
{
  RutNineSlice *nine_slice = object;

  cogl_object_unref (nine_slice->texture);

  cogl_object_unref (nine_slice->pipeline);

  rut_graphable_destroy (nine_slice);

  g_slice_free (RutNineSlice, object);
}

RutRefCountableVTable _rut_nine_slice_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_nine_slice_free
};

static RutGraphableVTable _rut_nine_slice_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static void
_rut_nine_slice_paint (RutObject *object,
                       RutPaintContext *paint_ctx)
{
  RutNineSlice *nine_slice = RUT_NINE_SLICE (object);
  RutCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = camera->fb;

  float left = nine_slice->left;
  float right = nine_slice->right;
  float top = nine_slice->top;
  float bottom = nine_slice->bottom;

  /* simple stretch */
  if (left == 0 && right == 0 && top == 0 && bottom == 0)
    {
      cogl_framebuffer_draw_rectangle (fb,
                                       nine_slice->pipeline,
                                       0, 0,
                                       nine_slice->width,
                                       nine_slice->height);
    }
  else
    {
      float width = nine_slice->width;
      float height = nine_slice->height;
      CoglTexture *texture = nine_slice->texture;
      float tex_width = cogl_texture_get_width (texture);
      float tex_height = cogl_texture_get_height (texture);

      /* s0,t0,s1,t1 define the texture coordinates for the center
       * rectangle... */
      float s0 = left / tex_width;
      float t0 = top / tex_height;
      float s1 = (tex_width - right) / tex_width;
      float t1 = (tex_height - bottom) / tex_height;

      float ex;
      float ey;

      ex = width - right;
      if (ex < left)
        ex = left;

      ey = height - bottom;
      if (ey < top)
        ey = top;

      {
        float rectangles[] =
          {
            /* top left corner */
            0, 0,
            left, top,
            0.0, 0.0,
            s0, t0,

            /* top middle */
            left, 0,
            MAX (left, ex), top,
            s0, 0.0,
            s1, t0,

            /* top right */
            ex, 0,
            MAX (ex + right, width), top,
            s1, 0.0,
            1.0, t0,

            /* mid left */
            0, top,
            left,  ey,
            0.0, t0,
            s0, t1,

            /* center */
            left, top,
            ex, ey,
            s0, t0,
            s1, t1,

            /* mid right */
            ex, top,
            MAX (ex + right, width), ey,
            s1, t0,
            1.0, t1,

            /* bottom left */
            0, ey,
            left, MAX (ey + bottom, height),
            0.0, t1,
            s0, 1.0,

            /* bottom center */
            left, ey,
            ex, MAX (ey + bottom, height),
            s0, t1,
            s1, 1.0,

            /* bottom right */
            ex, ey,
            MAX (ex + right, width), MAX (ey + bottom, height),
            s1, t1,
            1.0, 1.0
          };

          cogl_framebuffer_draw_textured_rectangles (fb,
                                                     nine_slice->pipeline,
                                                     rectangles,
                                                     9);
      }
    }
}

static RutPaintableVTable _rut_nine_slice_paintable_vtable = {
  _rut_nine_slice_paint
};

static RutSizableVTable _rut_nine_slice_sizable_vtable = {
  rut_nine_slice_set_size,
  rut_nine_slice_get_size,
  rut_simple_sizable_get_preferred_width,
  rut_simple_sizable_get_preferred_height,
  NULL /* add_preferred_size_callback */
};


RutType rut_nine_slice_type;

static void
_rut_nine_slice_init_type (void)
{
  rut_type_init (&rut_nine_slice_type, "RigNineSlice");
  rut_type_add_interface (&rut_nine_slice_type,
                           RUT_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RutNineSlice, ref_count),
                           &_rut_nine_slice_ref_countable_vtable);
  rut_type_add_interface (&rut_nine_slice_type,
                           RUT_INTERFACE_ID_GRAPHABLE,
                           offsetof (RutNineSlice, graphable),
                           &_rut_nine_slice_graphable_vtable);
  rut_type_add_interface (&rut_nine_slice_type,
                           RUT_INTERFACE_ID_PAINTABLE,
                           offsetof (RutNineSlice, paintable),
                           &_rut_nine_slice_paintable_vtable);
  rut_type_add_interface (&rut_nine_slice_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_nine_slice_sizable_vtable);
}

RutNineSlice *
rut_nine_slice_new (RutContext *ctx,
                    CoglTexture *texture,
                    float top,
                    float right,
                    float bottom,
                    float left,
                    float width,
                    float height)
{
  RutNineSlice *nine_slice = g_slice_new (RutNineSlice);

  rut_object_init (&nine_slice->_parent, &rut_nine_slice_type);

  nine_slice->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (nine_slice));

  nine_slice->texture = cogl_object_ref (texture);

  nine_slice->left = left;
  nine_slice->right = right;
  nine_slice->top = top;
  nine_slice->bottom = bottom;

  nine_slice->width = width;
  nine_slice->height = height;

  nine_slice->pipeline = cogl_pipeline_copy (ctx->single_texture_2d_template);
  cogl_pipeline_set_layer_texture (nine_slice->pipeline, 0, texture);

  return nine_slice;
}

CoglTexture *
rut_nine_slice_get_texture (RutNineSlice *nine_slice)
{
  return nine_slice->texture;
}

void
rut_nine_slice_set_size (RutObject *self,
                         float width,
                         float height)
{
  RutNineSlice *nine_slice = self;
  nine_slice->width = width;
  nine_slice->height = height;
}

void
rut_nine_slice_get_size (RutObject *self,
                         float *width,
                         float *height)
{
  RutNineSlice *nine_slice = self;
  *width = nine_slice->width;
  *height = nine_slice->height;
}

static void
_rut_graph_free (void *object)
{
  RutGraph *graph = RUT_GRAPH (object);

  rut_graphable_destroy (graph);

  g_slice_free (RutGraph, object);
}

RutRefCountableVTable _rut_graph_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_graph_free
};

RutGraphableVTable _rut_graph_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

RutType rut_graph_type;

static void
_rut_graph_init_type (void)
{
  rut_type_init (&rut_graph_type, "RigGraph");
  rut_type_add_interface (&rut_graph_type,
                           RUT_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RutGraph, ref_count),
                           &_rut_graph_ref_countable_vtable);
  rut_type_add_interface (&rut_graph_type,
                           RUT_INTERFACE_ID_GRAPHABLE,
                           offsetof (RutGraph, graphable),
                           &_rut_graph_graphable_vtable);
}

RutGraph *
rut_graph_new (RutContext *ctx)
{
  RutGraph *graph = g_slice_new (RutGraph);

  rut_object_init (&graph->_parent, &rut_graph_type);

  graph->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (graph));

  return graph;
}

static void
_rut_transform_free (void *object)
{
  RutTransform *transform = RUT_TRANSFORM (object);

  rut_graphable_destroy (transform);

  g_slice_free (RutTransform, object);
}

RutRefCountableVTable _rut_transform_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_transform_free
};

static RutGraphableVTable _rut_transform_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static RutTransformableVTable _rut_transform_transformable_vtable = {
  rut_transform_get_matrix
};

RutType rut_transform_type;

static void
_rut_transform_init_type (void)
{
  rut_type_init (&rut_transform_type, "RigTransform");
  rut_type_add_interface (&rut_transform_type,
                           RUT_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RutTransform, ref_count),
                           &_rut_transform_ref_countable_vtable);
  rut_type_add_interface (&rut_transform_type,
                           RUT_INTERFACE_ID_GRAPHABLE,
                           offsetof (RutTransform, graphable),
                           &_rut_transform_graphable_vtable);
  rut_type_add_interface (&rut_transform_type,
                           RUT_INTERFACE_ID_TRANSFORMABLE,
                           0,
                           &_rut_transform_transformable_vtable);
}

RutTransform *
rut_transform_new (RutContext *ctx)
{
  RutTransform *transform = g_slice_new (RutTransform);

  rut_object_init (&transform->_parent, &rut_transform_type);

  transform->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (transform));

  cogl_matrix_init_identity (&transform->matrix);

  return transform;
}

void
rut_transform_translate (RutTransform *transform,
                         float x,
                         float y,
                         float z)
{
  cogl_matrix_translate (&transform->matrix, x, y, z);
}

void
rut_transform_quaternion_rotate (RutTransform *transform,
                                 const CoglQuaternion *quaternion)
{
  CoglMatrix rotation;
  cogl_matrix_init_from_quaternion (&rotation, quaternion);
  cogl_matrix_multiply (&transform->matrix, &transform->matrix, &rotation);
}

void
rut_transform_rotate (RutTransform *transform,
                      float angle,
                      float x,
                      float y,
                      float z)
{
  cogl_matrix_rotate (&transform->matrix, angle, x, y, z);
}
void
rut_transform_scale (RutTransform *transform,
                     float x,
                     float y,
                     float z)
{
  cogl_matrix_scale (&transform->matrix, x, y, z);
}

void
rut_transform_init_identity (RutTransform *transform)
{
  cogl_matrix_init_identity (&transform->matrix);
}

const CoglMatrix *
rut_transform_get_matrix (RutObject *self)
{
  RutTransform *transform = self;
  return &transform->matrix;
}

void
rut_simple_widget_graphable_parent_changed (RutObject *self,
                                            RutObject *old_parent,
                                            RutObject *new_parent)
{
  /* nop */
}

void
rut_simple_widget_graphable_child_removed_warn (RutObject *self,
                                                RutObject *child)
{
  /* You can't add children to a button currently */
  g_warn_if_reached ();
}

void
rut_simple_widget_graphable_child_added_warn (RutObject *self,
                                              RutObject *child)
{
  /* You can't add children to a button currently */
  g_warn_if_reached ();
}

static void
_rut_rectangle_free (void *object)
{
  RutRectangle *rectangle = object;

  cogl_object_unref (rectangle->pipeline);

  rut_graphable_destroy (rectangle);

  g_slice_free (RutRectangle, object);
}

static RutRefCountableVTable _rut_rectangle_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_rectangle_free
};

static RutGraphableVTable _rut_rectangle_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static void
_rut_rectangle_paint (RutObject *object,
                      RutPaintContext *paint_ctx)
{
  RutRectangle *rectangle = RUT_RECTANGLE (object);
  RutCamera *camera = paint_ctx->camera;

  cogl_framebuffer_draw_rectangle (camera->fb,
                                   rectangle->pipeline,
                                   0, 0,
                                   rectangle->width,
                                   rectangle->height);
}

static RutPaintableVTable _rut_rectangle_paintable_vtable = {
  _rut_rectangle_paint
};

static RutSimpleWidgetVTable _rut_rectangle_simple_widget_vtable = {
 0
};

static RutSizableVTable _rut_rectangle_sizable_vtable = {
  rut_rectangle_set_size,
  rut_rectangle_get_size,
  rut_simple_sizable_get_preferred_width,
  rut_simple_sizable_get_preferred_height,
  NULL /* add_preferred_size_callback */
};

RutType rut_rectangle_type;

static void
_rut_rectangle_init_type (void)
{
  rut_type_init (&rut_rectangle_type, "RigRectangle");
  rut_type_add_interface (&rut_rectangle_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutRectangle, ref_count),
                          &_rut_rectangle_ref_countable_vtable);
  rut_type_add_interface (&rut_rectangle_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutRectangle, graphable),
                          &_rut_rectangle_graphable_vtable);
  rut_type_add_interface (&rut_rectangle_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutRectangle, paintable),
                          &_rut_rectangle_paintable_vtable);
  rut_type_add_interface (&rut_rectangle_type,
                          RUT_INTERFACE_ID_SIMPLE_WIDGET,
                          offsetof (RutRectangle, simple_widget),
                          &_rut_rectangle_simple_widget_vtable);
  rut_type_add_interface (&rut_rectangle_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_rectangle_sizable_vtable);
}

RutRectangle *
rut_rectangle_new4f (RutContext *ctx,
                     float width,
                     float height,
                     float red,
                     float green,
                     float blue,
                     float alpha)
{
  RutRectangle *rectangle = g_slice_new0 (RutRectangle);

  rut_object_init (RUT_OBJECT (rectangle), &rut_rectangle_type);

  rectangle->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (rectangle));
  rut_paintable_init (RUT_OBJECT (rectangle));

  rectangle->width = width;
  rectangle->height = height;

  rectangle->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_color4f (rectangle->pipeline,
                             red, green, blue, alpha);

  return rectangle;
}

void
rut_rectangle_set_width (RutRectangle *rectangle, float width)
{
  rectangle->width = width;
}

void
rut_rectangle_set_height (RutRectangle *rectangle, float height)
{
  rectangle->height = height;
}

void
rut_rectangle_set_size (RutObject *self,
                        float width,
                        float height)
{
  RutRectangle *rectangle = self;
  rectangle->width = width;
  rectangle->height = height;
}

void
rut_rectangle_get_size (RutObject *self,
                        float *width,
                        float *height)
{
  RutRectangle *rectangle = self;
  *width = rectangle->width;
  *height = rectangle->height;
}

void
_rut_init (void)
{
  static size_t init_status = 0;

  if (g_once_init_enter (&init_status))
    {
      //bindtextdomain (GETTEXT_PACKAGE, RUT_LOCALEDIR);
      //bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

      g_type_init ();

      _rut_context_init_type ();
      _rut_nine_slice_init_type ();
      _rut_rectangle_init_type ();
      _rut_text_buffer_init_type ();
      _rut_text_init_type ();
      _rut_graph_init_type ();
      _rut_transform_init_type ();
      _rut_timeline_init_type ();
      _rut_entity_init_type ();
      _rut_asset_type_init ();
      _rut_buffer_init_type ();
      _rut_attribute_init_type ();
      _rut_mesh_init_type ();
      _rut_scroll_bar_init_type ();

      /* components */
      _rut_camera_init_type ();
      _rut_light_init_type ();
      _rut_model_init_type ();
      _rut_material_init_type ();
      _rut_diamond_init_type ();
      _rut_diamond_slice_init_type ();
      _rut_shape_init_type ();
      _rut_shape_model_init_type ();

      g_once_init_leave (&init_status, 1);
    }
}

void
rut_color_init_from_uint32 (CoglColor *color, uint32_t value)
{
  color->red = RUT_UINT32_RED_AS_FLOAT (value);
  color->green = RUT_UINT32_GREEN_AS_FLOAT (value);
  color->blue = RUT_UINT32_BLUE_AS_FLOAT (value);
  color->alpha = RUT_UINT32_ALPHA_AS_FLOAT (value);
}

