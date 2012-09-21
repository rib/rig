/*
 * Rut
 *
 * A tiny toolkit
 *
 * Copyright (C) 2012 Intel Corporation.
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

#include <glib.h>

#include <cogl/cogl.h>
#include <cogl-pango/cogl-pango.h>

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

/*
 * Overall issues to keep in mind for a useful and efficient UI scenegraph:
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
 * and may never since it's currently just wanted as a minimal toolkit in Cogl
 * for debugging purposes and examples. Never the less they are things to keep
 * in mind when shaping the code on the off-chance that something interesting
 * comes out of it.
 *
 * One quite nice thing about this code is the simple approach to interface
 * oriented programming:
 *
 * - Interfaces are a vtable struct typedef of function pointers that must be
 *   implemented and a struct typedef of per-instance properties that must be
 *   available. (Both are optional)
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
 * Interfaces defined currently for Rut are:
 * "RefCountable"
 *  - implies an int ref_count property and ref, unref, free methods
 * "Graphable"
 *  - implies parent and children properties but no methods
 * "PaintBatchable"
 *  - no properties implied but adds set_insert_point and update methods.
 *
 * The Rut rendering model was designed so objects retain drawing primitives
 * and drawing state instead of using immediate mode drawing.
 *
 * The objects in the scenegraph have a very tight integration with the linear
 * "display list" structure used to actually paint. A display list is just
 * a linked list of rendering commands including transformation and primitive
 * drawing commands. Each object that wants to render is expected to maintain
 * a linked list of drawing commands.
 *
 * Objects in the scenegraph wanting to render implement the "PaintBatchable"
 * interface which has two methods "set_insert_point" and "update_batch".  The
 * set_insert_point method gives the object a display list link node which
 * tells it where it can insert its own linked list of commands. The
 * "update_batch" method (not used currently) will be used if an object queues
 * an update and it allows the object to change the commands it has linked into
 * the display list.
 *
 * This design will mean that individual objects may be update completely in
 * isolation without any graph traversal.
 *
 * The main disadvantage is that with no indirection at all then it would be
 * difficult to add a thread boundary for rendering without copying the display
 * list. Later instead of literally manipulating a GList though we could use
 * a rut_display_list api which would allow us to internally queue list
 * manipulations instead of allowing direct access.
 *
 * Transforms around children in the scenegraph will have corresponding "push"
 * "pop" commands in the display list and the "pop" commands will contain a
 * back link to the "push". This means that when dealing with the display list
 * data structure we can walk backwards from any primitive to recover all the
 * transformations apply to the primitive jumping over redundant commands.
 *
 * Another problem with this design compared to having a simple
 * imperative paint method like clutter is that it may be more awkward
 * to support nodes belonging to multiple camera graphs which would
 * each need separate display lists. The interface would need some
 * further work to allow nodes to be associated with multiple cameras.
 *
 * Something else to consider is the very tight coupling between nodes
 * in the graph and the code that paints what they represent.
 * Something I'm keen on experimenting with is having a globally aware
 * scene compositor that owns the whole screen, but can derive the
 * structure of a UI from a scene graph, and input regions may be
 * associated with the graph too.
 *
 */

/* Requirements for batching:
 * - Want to be able to insert commands around children. E.g. for a transform
 *   we want to insert a "push" command, then a transform, then a child's
 *   commands and then a "pop" command. Also we want a parent to be able to
 *   draw things before and after drawing children.
 * - Want to be able to "re-batch" individual objects in isolation so the
 *   cost of batching a new frame scales according to how many changes
 *   there are not by the total scene complexity.
 * - Want to be able to hide an object by removing from a display list
 *   ("un-batching")
 * - We need to think about how transform information is exposed so that it's
 *   possible to efficiently determine the transformation of any batched
 *   primitive. In Clutter we have an "apply_transform" method because
 *   transforms are dealt with in the paint method which is a black box but
 *   the apply_transform approach isn't very extensible (e.g. it can't handle
 *   projection matrix changes.)
 *
 * Can we use a clutter-style imperative paint method?
 *  - The advantage is that it's a natural way for an implementation to
 *    pass control to children and directly pass the display list insert
 *    point too:
 *
 *    void
 *    rut_batchable_insert_batch (RutObject *object,
 *                                RutBatchContext *paint_ctx,
 *                                GList *insert_point);
 *
 *    Having a wrapper function as above like clutter also allows us to
 *    play tricks and not necessarily *actually* paint the child; we
 *    might just change the child's insert point if we know the child
 *    itself hasn't change.
 *
 * Why do we have the apply transform in clutter?
 * - To determine the matrix used for input transformation because the transforms
 *   are dealt with as part of the imperative paint functions which are a black
 *   box.
 * Do we need that here?
 * - If the transforms were handled as nodes in the scenegraph then no we can
 *   just walk up the ancestors of the scenegraph.
 *   - The disadvantage is that some of these transforms are essentially just
 *     implementation details for a particular drawable and there is a question
 *     of who owns the scenegraph so it might not make sense to let objects
 *     expose private transforms in the scenegraph.
 *   - recovering the transforms from the display list would be possible but
 *   that could be quite inefficient if we don't find a way to avoid walking
 *   over redundant branches in the scenegraph since it would be very
 *   expensive for objects inserted near the end of the display list.
 *   - Could we have some sideband linking in the display list to be able to
 *   only walk through commands relating to ancestors?
 *      - If transform pops had a back link to transform pushes then it would
 *        be possible to efficiently skip over commands relating to redundant
 *        branches of the display list.
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
  CoglPrimitive *primitive;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

};

typedef enum _ButtonState
{
  BUTTON_STATE_NORMAL,
  BUTTON_STATE_HOVER,
  BUTTON_STATE_ACTIVE,
  BUTTON_STATE_ACTIVE_CANCEL,
  BUTTON_STATE_DISABLED
} ButtonState;

struct _RutButton
{
  RutObjectProps _parent;
  int ref_count;

  RutContext *ctx;

  ButtonState state;

  PangoLayout *label;
  int label_width;
  int label_height;

  float width;
  float height;

  RutNineSlice *background_normal;
  RutNineSlice *background_hover;
  RutNineSlice *background_active;
  RutNineSlice *background_disabled;

  CoglColor text_color;

  RutInputRegion *input_region;

  RutList on_click_cb_list;

  RutSimpleWidgetProps simple_widget;

  RutGraphableProps graphable;
  RutPaintableProps paintable;
};

struct _RutText
{
  RutObjectProps _parent;
  int ref_count;

  RutContext *ctx;

  CoglBool enabled;
  CoglBool editable;

  PangoLayout *text;
  int text_width;
  int text_height;

  float width;
  float height;

  CoglColor text_color;

  RutInputRegion *input_region;

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
  rut_ref_countable_simple_ref,
  rut_ref_countable_simple_unref,
  _rut_context_free
};

RutType rut_context_type;

static void
_rut_context_init_type (void)
{
  rut_type_init (&rut_context_type);
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

CoglTexture *
rut_load_texture (RutContext *ctx, const char *filename, CoglError **error)
{
  RutTextureCacheEntry *entry =
    g_hash_table_lookup (ctx->texture_cache, filename);
  CoglTexture *texture;

  if (entry)
    return cogl_object_ref (entry->texture);

  texture = cogl_texture_new_from_file (filename,
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
      context->shell = rut_ref_countable_ref (shell);

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
  cogl_object_unref (nine_slice->primitive);

  g_slice_free (RutNineSlice, object);
}

RutRefCountableVTable _rut_nine_slice_ref_countable_vtable = {
  rut_ref_countable_simple_ref,
  rut_ref_countable_simple_unref,
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

  cogl_framebuffer_draw_primitive (camera->fb,
                                   nine_slice->pipeline,
                                   nine_slice->primitive);
}

static RutPaintableVTable _rut_nine_slice_paintable_vtable = {
  _rut_nine_slice_paint
};

RutType rut_nine_slice_type;

static void
_rut_nine_slice_init_type (void)
{
  rut_type_init (&rut_nine_slice_type);
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
}

static CoglPrimitive *
primitive_new_textured_rectangle (RutContext *ctx,
                                  float x0,
                                  float y0,
                                  float x1,
                                  float y1,
                                  float s0,
                                  float t0,
                                  float s1,
                                  float t1)
{
  CoglVertexP2T2 vertices[8] = {
        {x0, y0, s0, t0},
        {x0, y1, s0, t1},
        {x1, y1, s1, t1},
        {x1, y0, s1, t0}
  };

  return cogl_primitive_new_p2t2 (ctx->cogl_context,
                                  COGL_VERTICES_MODE_TRIANGLE_STRIP,
                                  8,
                                  vertices);
}

static RutNineSlice *
_rut_nine_slice_new_full (RutContext *ctx,
                          CoglTexture *texture,
                          float top,
                          float right,
                          float bottom,
                          float left,
                          float width,
                          float height,
                          CoglPrimitive *shared_prim)
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

  nine_slice->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_layer_texture (nine_slice->pipeline, 0, texture);

  /* simple stretch */
  if (left == 0 && right == 0 && top == 0 && bottom == 0)
    {
      nine_slice->primitive =
        primitive_new_textured_rectangle (ctx,
                                          0, 0, width, height,
                                          0, 0, 1, 1);
    }
  else if (shared_prim)
    {
      nine_slice->primitive = cogl_object_ref (shared_prim);
    }
  else
    {
      float tex_width = cogl_texture_get_width (texture);
      float tex_height = cogl_texture_get_height (texture);

      /* x0,y0,x1,y1 and s0,t0,s1,t1 define the postion and texture
       * coordinates for the center rectangle... */
      float x0 = left;
      float y0 = top;
      float x1 = width - right;
      float y1 = height - bottom;

      float s0 = left / tex_width;
      float t0 = top / tex_height;
      float s1 = (tex_width - right) / tex_width;
      float t1 = (tex_height - bottom) / tex_height;

      /*
       * 0,0      x0,0      x1,0      width,0
       * 0,0      s0,0      s1,0      1,0
       * 0        1         2         3
       *
       * 0,y0     x0,y0     x1,y0     width,y0
       * 0,t0     s0,t0     s1,t0     1,t0
       * 4        5         6         7
       *
       * 0,y1     x0,y1     x1,y1     width,y1
       * 0,t1     s0,t1     s1,t1     1,t1
       * 8        9         10        11
       *
       * 0,height x0,height x1,height width,height
       * 0,1      s0,1      s1,1      1,1
       * 12       13        14        15
       */

      CoglVertexP2T2 vertices[] =
        {
            { 0, 0, 0, 0 },
            { x0, 0, s0, 0},
            { x1, 0, s1, 0},
            { width, 0, 1, 0},

            { 0, y0, 0, t0},
            { x0, y0, s0, t0},
            { x1, y0, s1, t0},
            { width, y0, 1, t0},

            { 0, y1, 0, t1},
            { x0, y1, s0, t1},
            { x1, y1, s1, t1},
            { width, y1, 1, t1},

            { 0, height, 0, 1},
            { x0, height, s0, 1},
            { x1, height, s1, 1},
            { width, height, 1, 1},
        };

      nine_slice->primitive =
        cogl_primitive_new_p2t2 (ctx->cogl_context,
                                 COGL_VERTICES_MODE_TRIANGLES,
                                 sizeof (vertices) / sizeof (CoglVertexP2T2),
                                 vertices);

      /* The vertices uploaded only map to the key intersection points of the
       * 9-slice grid which isn't a topology that GPUs can handle directly so
       * this specifies an array of indices that allow the GPU to interpret the
       * vertices as a list of triangles... */
      cogl_primitive_set_indices (nine_slice->primitive,
                                  ctx->nine_slice_indices,
                                  sizeof (_rut_nine_slice_indices_data) /
                                  sizeof (_rut_nine_slice_indices_data[0]));
    }

  return nine_slice;
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
  return _rut_nine_slice_new_full (ctx, texture,
                                   top, right, bottom, left,
                                   width, height,
                                   NULL);
}

static void
_rut_graph_free (void *object)
{
  RutGraph *graph = RUT_GRAPH (object);

  rut_graphable_remove_all_children (graph);

  g_slice_free (RutGraph, object);
}

RutRefCountableVTable _rut_graph_ref_countable_vtable = {
  rut_ref_countable_simple_ref,
  rut_ref_countable_simple_unref,
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
  rut_type_init (&rut_graph_type);
  rut_type_add_interface (&rut_graph_type,
                           RUT_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RutButton, ref_count),
                           &_rut_graph_ref_countable_vtable);
  rut_type_add_interface (&rut_graph_type,
                           RUT_INTERFACE_ID_GRAPHABLE,
                           offsetof (RutGraph, graphable),
                           &_rut_graph_graphable_vtable);
}

RutGraph *
rut_graph_new (RutContext *ctx, ...)
{
  RutGraph *graph = g_slice_new (RutGraph);
  RutObject *object;
  va_list ap;

  rut_object_init (&graph->_parent, &rut_graph_type);

  graph->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (graph));

  va_start (ap, ctx);
  while ((object = va_arg (ap, RutObject *)))
    rut_graphable_add_child (RUT_OBJECT (graph), object);
  va_end (ap);

  return graph;
}

static void
_rut_transform_free (void *object)
{
  RutTransform *transform = RUT_TRANSFORM (object);

  rut_graphable_remove_all_children (transform);

  g_slice_free (RutTransform, object);
}

RutRefCountableVTable _rut_transform_ref_countable_vtable = {
  rut_ref_countable_simple_ref,
  rut_ref_countable_simple_unref,
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
  rut_type_init (&rut_transform_type);
  rut_type_add_interface (&rut_transform_type,
                           RUT_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RutButton, ref_count),
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
rut_transform_new (RutContext *ctx,
                   ...)
{
  RutTransform *transform = g_slice_new (RutTransform);
  RutObject *object;
  va_list ap;

  rut_object_init (&transform->_parent, &rut_transform_type);

  transform->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (transform));

  cogl_matrix_init_identity (&transform->matrix);

  va_start (ap, ctx);
  while ((object = va_arg (ap, RutObject *)))
    rut_graphable_add_child (RUT_OBJECT (transform), object);
  va_end (ap);

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
rut_transform_get_matrix (RutTransform *transform)
{
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

  g_slice_free (RutRectangle, object);
}

static RutRefCountableVTable _rut_rectangle_ref_countable_vtable = {
  rut_ref_countable_simple_ref,
  rut_ref_countable_simple_unref,
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
  NULL,
  NULL
};

RutType rut_rectangle_type;

static void
_rut_rectangle_init_type (void)
{
  rut_type_init (&rut_rectangle_type);
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
rut_rectangle_set_size (RutRectangle *rectangle,
                        float width,
                        float height)
{
  rectangle->width = width;
  rectangle->height = height;
}

void
rut_rectangle_get_size (RutRectangle *rectangle,
                        float *width,
                        float *height)
{
  *width = rectangle->width;
  *height = rectangle->height;
}

enum {
  RUT_UI_VIEWPORT_PROP_WIDTH,
  RUT_UI_VIEWPORT_PROP_HEIGHT,
  RUT_UI_VIEWPORT_PROP_X_PANNABLE,
  RUT_UI_VIEWPORT_PROP_Y_PANNABLE,
  RUT_UI_VIEWPORT_N_PROPS
};

struct _RutUIViewport
{
  RutObjectProps _parent;

  RutContext *ctx;

  int ref_count;

  RutGraphableProps graphable;

  float width;
  float height;

  float doc_x;
  float doc_y;
  float doc_scale_x;
  float doc_scale_y;

  CoglBool x_pannable;
  CoglBool y_pannable;

  RutTransform *doc_transform;

  RutInputRegion *input_region;
  float grab_x;
  float grab_y;
  float grab_doc_x;
  float grab_doc_y;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_UI_VIEWPORT_N_PROPS];
};

static RutPropertySpec _rut_ui_viewport_prop_specs[] = {
  {
    .name = "width",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutUIViewport, width),
    .setter = rut_ui_viewport_set_width
  },
  {
    .name = "height",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutUIViewport, height),
    .setter = rut_ui_viewport_set_height
  },
  {
    .name = "x-pannable",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .data_offset = offsetof (RutUIViewport, x_pannable),
    .getter = rut_ui_viewport_get_x_pannable,
    .setter = rut_ui_viewport_set_x_pannable
  },
  {
    .name = "y-pannable",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .data_offset = offsetof (RutUIViewport, y_pannable),
    .getter = rut_ui_viewport_get_y_pannable,
    .setter = rut_ui_viewport_set_y_pannable
  },

  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_ui_viewport_free (void *object)
{
  RutUIViewport *ui_viewport = object;

  rut_ref_countable_simple_unref (ui_viewport->input_region);

  rut_simple_introspectable_destroy (ui_viewport);

  g_slice_free (RutUIViewport, object);
}

static RutRefCountableVTable _rut_ui_viewport_ref_countable_vtable = {
  rut_ref_countable_simple_ref,
  rut_ref_countable_simple_unref,
  _rut_ui_viewport_free
};

static RutGraphableVTable _rut_ui_viewport_graphable_vtable = {
  NULL, /* child_removed */
  NULL, /* child_added */
  NULL, /* parent_changed */
};

static RutSizableVTable _rut_ui_viewport_sizable_vtable = {
  rut_ui_viewport_set_size,
  rut_ui_viewport_get_size,
  NULL,
  NULL
};

static RutIntrospectableVTable _rut_ui_viewport_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

RutType rut_ui_viewport_type;

static void
_rut_ui_viewport_init_type (void)
{
  rut_type_init (&rut_ui_viewport_type);
  rut_type_add_interface (&rut_ui_viewport_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutUIViewport, ref_count),
                          &_rut_ui_viewport_ref_countable_vtable);
  rut_type_add_interface (&rut_ui_viewport_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutUIViewport, graphable),
                          &_rut_ui_viewport_graphable_vtable);
  rut_type_add_interface (&rut_ui_viewport_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_ui_viewport_sizable_vtable);
  rut_type_add_interface (&rut_ui_viewport_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_ui_viewport_introspectable_vtable);
  rut_type_add_interface (&rut_ui_viewport_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutUIViewport, introspectable),
                          NULL); /* no implied vtable */
}

static void
_rut_ui_viewport_update_doc_matrix (RutUIViewport *ui_viewport)
{
  rut_transform_init_identity (ui_viewport->doc_transform);
  rut_transform_translate (ui_viewport->doc_transform,
                           ui_viewport->doc_x,
                           ui_viewport->doc_y,
                           0);
  rut_transform_scale (ui_viewport->doc_transform,
                       ui_viewport->doc_scale_x,
                       ui_viewport->doc_scale_y,
                       1);
}

static RutInputEventStatus
ui_viewport_grab_input_cb (RutInputEvent *event, void *user_data)
{
  RutUIViewport *ui_viewport = user_data;

  if (rut_input_event_get_type (event) != RUT_INPUT_EVENT_TYPE_MOTION)
    return RUT_INPUT_EVENT_STATUS_UNHANDLED;

  if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
    {
      RutButtonState state = rut_motion_event_get_button_state (event);

      if (state & RUT_BUTTON_STATE_2)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);
          float dx = x - ui_viewport->grab_x;
          float dy = y - ui_viewport->grab_y;
          float x_scale = rut_ui_viewport_get_doc_scale_x (ui_viewport);
          float y_scale = rut_ui_viewport_get_doc_scale_y (ui_viewport);
          float inv_x_scale = 1.0 / x_scale;
          float inv_y_scale = 1.0 / y_scale;

          if (ui_viewport->x_pannable)
            rut_ui_viewport_set_doc_x (ui_viewport,
                                       ui_viewport->grab_doc_x +
                                       (dx * inv_x_scale));

          if (ui_viewport->y_pannable)
            rut_ui_viewport_set_doc_y (ui_viewport,
                                       ui_viewport->grab_doc_y +
                                       (dy * inv_y_scale));

          rut_shell_queue_redraw (ui_viewport->ctx->shell);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }
  else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
    {
      rut_shell_ungrab_input (ui_viewport->ctx->shell,
                              ui_viewport_grab_input_cb,
                              user_data);
      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}
static RutInputEventStatus
_rut_ui_viewport_input_cb (RutInputRegion *region,
                           RutInputEvent *event,
                           void *user_data)
{
  RutUIViewport *ui_viewport = user_data;

  g_print ("viewport input\n");
  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      switch (rut_motion_event_get_action (event))
        {
        case RUT_MOTION_EVENT_ACTION_DOWN:
          {
            RutButtonState state = rut_motion_event_get_button_state (event);

            if (state & RUT_BUTTON_STATE_2)
              {
                ui_viewport->grab_x = rut_motion_event_get_x (event);
                ui_viewport->grab_y = rut_motion_event_get_y (event);

                ui_viewport->grab_doc_x = rut_ui_viewport_get_doc_x (ui_viewport);
                ui_viewport->grab_doc_y = rut_ui_viewport_get_doc_y (ui_viewport);

                /* TODO: Add rut_shell_implicit_grab_input() that handles releasing
                 * the grab for you */
                g_print ("viewport input grab\n");
                rut_shell_grab_input (ui_viewport->ctx->shell,
                                      rut_input_event_get_camera (event),
                                      ui_viewport_grab_input_cb,
                                      ui_viewport);
                return RUT_INPUT_EVENT_STATUS_HANDLED;
              }
          }
	default:
	  break;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutUIViewport *
rut_ui_viewport_new (RutContext *ctx,
                     float width,
                     float height,
                     ...)
{
  RutUIViewport *ui_viewport = g_slice_new0 (RutUIViewport);
  va_list ap;
  RutObject *object;

  rut_object_init (RUT_OBJECT (ui_viewport), &rut_ui_viewport_type);

  ui_viewport->ctx = ctx;

  ui_viewport->ref_count = 1;

  rut_simple_introspectable_init (ui_viewport,
                                  _rut_ui_viewport_prop_specs,
                                  ui_viewport->properties);

  rut_graphable_init (RUT_OBJECT (ui_viewport));

  ui_viewport->width = width;
  ui_viewport->height = height;
  ui_viewport->doc_x = 0;
  ui_viewport->doc_y = 0;
  ui_viewport->doc_scale_x = 1;
  ui_viewport->doc_scale_y = 1;

  ui_viewport->x_pannable = TRUE;
  ui_viewport->y_pannable = TRUE;

  ui_viewport->doc_transform = rut_transform_new (ctx, NULL);
  rut_graphable_add_child (ui_viewport, ui_viewport->doc_transform);

  _rut_ui_viewport_update_doc_matrix (ui_viewport);

  ui_viewport->input_region =
    rut_input_region_new_rectangle (0, 0,
                                    ui_viewport->width,
                                    ui_viewport->height,
                                    _rut_ui_viewport_input_cb,
                                    ui_viewport);

  //rut_input_region_set_graphable (ui_viewport->input_region, ui_viewport);
  rut_graphable_add_child (ui_viewport, ui_viewport->input_region);

  va_start (ap, height);
  while ((object = va_arg (ap, RutObject *)))
    rut_graphable_add_child (RUT_OBJECT (ui_viewport), object);
  va_end (ap);

  return ui_viewport;
}

void
rut_ui_viewport_set_size (RutUIViewport *ui_viewport,
                          float width,
                          float height)
{
  ui_viewport->width = width;
  ui_viewport->height = height;

  rut_input_region_set_rectangle (ui_viewport->input_region,
                                  0, 0,
                                  width,
                                  height);

  rut_property_dirty (&ui_viewport->ctx->property_ctx,
                      &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_WIDTH]);
  rut_property_dirty (&ui_viewport->ctx->property_ctx,
                      &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_HEIGHT]);
}

void
rut_ui_viewport_get_size (RutUIViewport *ui_viewport,
                          float *width,
                          float *height)
{
  *width = ui_viewport->width;
  *height = ui_viewport->height;
}

void
rut_ui_viewport_set_width (RutUIViewport *ui_viewport, float width)
{
  rut_ui_viewport_set_size (ui_viewport, width, ui_viewport->height);
}

void
rut_ui_viewport_set_height (RutUIViewport *ui_viewport, float height)
{
  rut_ui_viewport_set_size (ui_viewport, ui_viewport->width, height);
}

void
rut_ui_viewport_set_doc_x (RutUIViewport *ui_viewport, float doc_x)
{
  ui_viewport->doc_x = doc_x;
  _rut_ui_viewport_update_doc_matrix (ui_viewport);
}

void
rut_ui_viewport_set_doc_y (RutUIViewport *ui_viewport, float doc_y)
{
  g_print ("ui_viewport doc_y = %f\n", doc_y);
  ui_viewport->doc_y = doc_y;
  _rut_ui_viewport_update_doc_matrix (ui_viewport);
}

void
rut_ui_viewport_set_doc_scale_x (RutUIViewport *ui_viewport, float doc_scale_x)
{
  ui_viewport->doc_scale_x = doc_scale_x;
  _rut_ui_viewport_update_doc_matrix (ui_viewport);
}

void
rut_ui_viewport_set_doc_scale_y (RutUIViewport *ui_viewport, float doc_scale_y)
{
  ui_viewport->doc_scale_y = doc_scale_y;
  _rut_ui_viewport_update_doc_matrix (ui_viewport);
}

float
rut_ui_viewport_get_width (RutUIViewport *ui_viewport)
{
  return ui_viewport->width;
}

float
rut_ui_viewport_get_height (RutUIViewport *ui_viewport)
{
  return ui_viewport->height;
}

float
rut_ui_viewport_get_doc_x (RutUIViewport *ui_viewport)
{
  return ui_viewport->doc_x;
}

float
rut_ui_viewport_get_doc_y (RutUIViewport *ui_viewport)
{
  return ui_viewport->doc_y;
}

float
rut_ui_viewport_get_doc_scale_x (RutUIViewport *ui_viewport)
{
  return ui_viewport->doc_scale_x;
}

float
rut_ui_viewport_get_doc_scale_y (RutUIViewport *ui_viewport)
{
  return ui_viewport->doc_scale_y;
}

const CoglMatrix *
rut_ui_viewport_get_doc_matrix (RutUIViewport *ui_viewport)
{
  return rut_transform_get_matrix (ui_viewport->doc_transform);
}

RutObject *
rut_ui_viewport_get_doc_node (RutUIViewport *ui_viewport)
{
  return ui_viewport->doc_transform;
}

void
rut_ui_viewport_set_x_pannable (RutUIViewport *ui_viewport,
                                CoglBool pannable)
{
  ui_viewport->x_pannable = pannable;
}

CoglBool
rut_ui_viewport_get_x_pannable (RutUIViewport *ui_viewport)
{
  return ui_viewport->x_pannable;
}

void
rut_ui_viewport_set_y_pannable (RutUIViewport *ui_viewport,
                                CoglBool pannable)
{
  ui_viewport->y_pannable = pannable;
}

CoglBool
rut_ui_viewport_get_y_pannable (RutUIViewport *ui_viewport)
{
  return ui_viewport->y_pannable;
}

#if 0
static void
_rut_text_free (void *object)
{
  RutText *text = object;

  g_object_unref (text->text);

  g_slice_free (RutText, object);
}

RutRefCountableVTable _rut_text_ref_countable_vtable = {
  rut_ref_countable_simple_ref,
  rut_ref_countable_simple_unref,
  _rut_text_free
};

static RutGraphableVTable _rut_text_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static void
_rut_text_paint (RutObject *object,
                 RutPaintContext *paint_ctx)
{
  RutText *text = RUT_TEXT (object);
  RutCamera *camera = paint_ctx->camera;

  cogl_pango_show_layout (camera->fb,
                          text->text,
                          0, 0,
                          &text->text_color);
}

static RutPaintableVTable _rut_text_paintable_vtable = {
  _rut_text_paint
};

RutSimpleWidgetVTable _rut_text_simple_widget_vtable = {
 0
};

RutType rut_text_type;

static void
_rut_text_init_type (void)
{
  rut_type_init (&rut_text_type);
  rut_type_add_interface (&rut_text_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutText, ref_count),
                          &_rut_text_ref_countable_vtable);
  rut_type_add_interface (&rut_text_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutText, graphable),
                          &_rut_text_graphable_vtable);
  rut_type_add_interface (&rut_text_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutText, paintable),
                          &_rut_text_paintable_vtable);
}

static RutInputEventStatus
_rut_text_input_cb (RutInputRegion *region,
                      RutInputEvent *event,
                      void *user_data)
{
  //RutText *text = user_data;

  g_print ("Text input\n");

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutText *
rut_text_new (RutContext *ctx,
              const char *text_str)
{
  RutText *text = g_slice_new0 (RutText);
  PangoRectangle text_size;

  rut_object_init (RUT_OBJECT (text), &rut_text_type);

  text->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (text));
  rut_paintable_init (RUT_OBJECT (text));

  text->ctx = ctx;

  text->text = pango_layout_new (ctx->pango_context);
  pango_layout_set_font_description (text->text, ctx->pango_font_desc);
  pango_layout_set_text (text->text, text_str, -1);

  pango_layout_get_extents (text->text, NULL, &text_size);
  text->text_width = PANGO_PIXELS (text_size.width);
  text->text_height = PANGO_PIXELS (text_size.height);

  text->width = text->text_width + 10;
  text->height = text->text_height + 23;

  cogl_color_init_from_4f (&text->text_color, 0, 0, 0, 1);

  text->input_region =
    rut_input_region_new_rectangle (0, 0, text->width, text->height,
                                    _rut_text_input_cb,
                                    text);

  //rut_input_region_set_graphable (text->input_region, text);
  rut_graphable_add_child (text, text->input_region);

  return text;
}
#endif

static void
_rut_button_free (void *object)
{
  RutButton *button = object;

  rut_closure_list_disconnect_all (&button->on_click_cb_list);

  rut_ref_countable_unref (button->background_normal);
  rut_ref_countable_unref (button->background_hover);
  rut_ref_countable_unref (button->background_active);
  rut_ref_countable_unref (button->background_disabled);

  g_object_unref (button->label);

  g_slice_free (RutButton, object);
}

RutRefCountableVTable _rut_button_ref_countable_vtable = {
  rut_ref_countable_simple_ref,
  rut_ref_countable_simple_unref,
  _rut_button_free
};

static RutGraphableVTable _rut_button_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static void
_rut_button_paint (RutObject *object,
                   RutPaintContext *paint_ctx)
{
  RutButton *button = RUT_BUTTON (object);
  RutCamera *camera = paint_ctx->camera;
  RutPaintableVTable *bg_paintable =
    rut_object_get_vtable (button->background_normal, RUT_INTERFACE_ID_PAINTABLE);

  switch (button->state)
    {
    case BUTTON_STATE_NORMAL:
      bg_paintable->paint (RUT_OBJECT (button->background_normal), paint_ctx);
      break;
    case BUTTON_STATE_HOVER:
      bg_paintable->paint (RUT_OBJECT (button->background_hover), paint_ctx);
      break;
    case BUTTON_STATE_ACTIVE:
      bg_paintable->paint (RUT_OBJECT (button->background_active), paint_ctx);
      break;
    case BUTTON_STATE_ACTIVE_CANCEL:
      bg_paintable->paint (RUT_OBJECT (button->background_active), paint_ctx);
      break;
    case BUTTON_STATE_DISABLED:
      bg_paintable->paint (RUT_OBJECT (button->background_disabled), paint_ctx);
      break;
    }

  cogl_pango_show_layout (camera->fb,
                          button->label,
                          5, 11,
                          &button->text_color);
}

static RutPaintableVTable _rut_button_paintable_vtable = {
  _rut_button_paint
};

RutSimpleWidgetVTable _rut_button_simple_widget_vtable = {
 0
};

RutType rut_button_type;

static void
_rut_button_init_type (void)
{
  rut_type_init (&rut_button_type);
  rut_type_add_interface (&rut_button_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutButton, ref_count),
                          &_rut_button_ref_countable_vtable);
  rut_type_add_interface (&rut_button_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutButton, graphable),
                          &_rut_button_graphable_vtable);
  rut_type_add_interface (&rut_button_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutButton, paintable),
                          &_rut_button_paintable_vtable);
  rut_type_add_interface (&rut_button_type,
                          RUT_INTERFACE_ID_SIMPLE_WIDGET,
                          offsetof (RutButton, simple_widget),
                          &_rut_button_simple_widget_vtable);
}

typedef struct _ButtonGrabState
{
  RutCamera *camera;
  RutButton *button;
  CoglMatrix transform;
  CoglMatrix inverse_transform;
} ButtonGrabState;

static RutInputEventStatus
_rut_button_grab_input_cb (RutInputEvent *event,
                           void *user_data)
{
  ButtonGrabState *state = user_data;
  RutButton *button = state->button;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutShell *shell = button->ctx->shell;
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell, _rut_button_grab_input_cb, user_data);

          rut_closure_list_invoke (&button->on_click_cb_list,
                                   RutButtonClickCallback,
                                   button);

          g_print ("Button click\n");

          g_slice_free (ButtonGrabState, state);

          button->state = BUTTON_STATE_NORMAL;
          rut_shell_queue_redraw (button->ctx->shell);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);
          RutCamera *camera = state->camera;

          rut_camera_unproject_coord (camera,
                                      &state->transform,
                                      &state->inverse_transform,
                                      0,
                                      &x,
                                      &y);

          if (x < 0 || x > button->width ||
              y < 0 || y > button->height)
            button->state = BUTTON_STATE_ACTIVE_CANCEL;
          else
            button->state = BUTTON_STATE_ACTIVE;

          rut_shell_queue_redraw (button->ctx->shell);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_button_input_cb (RutInputRegion *region,
                      RutInputEvent *event,
                      void *user_data)
{
  RutButton *button = user_data;

  g_print ("Button input\n");

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
     rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = button->ctx->shell;
      ButtonGrabState *state = g_slice_new (ButtonGrabState);
      const CoglMatrix *view;

      state->button = button;
      state->camera = rut_input_event_get_camera (event);
      view = rut_camera_get_view_transform (state->camera);
      state->transform = *view;
      rut_graphable_apply_transform (button, &state->transform);
      if (!cogl_matrix_get_inverse (&state->transform,
                                    &state->inverse_transform))
        {
          g_warning ("Failed to calculate inverse of button transform\n");
          g_slice_free (ButtonGrabState, state);
          return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

      rut_shell_grab_input (shell,
                            state->camera,
                            _rut_button_grab_input_cb,
                            state);
      //button->grab_x = rut_motion_event_get_x (event);
      //button->grab_y = rut_motion_event_get_y (event);

      button->state = BUTTON_STATE_ACTIVE;
      rut_shell_queue_redraw (button->ctx->shell);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutButton *
rut_button_new (RutContext *ctx,
                const char *label)
{
  RutButton *button = g_slice_new0 (RutButton);
  CoglTexture *normal_texture;
  CoglTexture *hover_texture;
  CoglTexture *active_texture;
  CoglTexture *disabled_texture;
  CoglError *error = NULL;
  PangoRectangle label_size;

  rut_object_init (RUT_OBJECT (button), &rut_button_type);

  button->ref_count = 1;

  rut_list_init (&button->on_click_cb_list);

  rut_graphable_init (RUT_OBJECT (button));
  rut_paintable_init (RUT_OBJECT (button));

  button->ctx = ctx;

  button->state = BUTTON_STATE_NORMAL;

  normal_texture = rut_load_texture (ctx, RIG_DATA_DIR "button.png", &error);
  if (!normal_texture)
    {
      g_warning ("Failed to load button texture: %s", error->message);
      cogl_error_free (error);
    }

  hover_texture = rut_load_texture (ctx, RIG_DATA_DIR "button-hover.png", &error);
  if (!hover_texture)
    {
      cogl_object_unref (normal_texture);
      g_warning ("Failed to load button-hover texture: %s", error->message);
      cogl_error_free (error);
    }

  active_texture = rut_load_texture (ctx, RIG_DATA_DIR "button-active.png", &error);
  if (!active_texture)
    {
      cogl_object_unref (normal_texture);
      cogl_object_unref (hover_texture);
      g_warning ("Failed to load button-active texture: %s", error->message);
      cogl_error_free (error);
    }

  disabled_texture = rut_load_texture (ctx, RIG_DATA_DIR "button-disabled.png", &error);
  if (!disabled_texture)
    {
      cogl_object_unref (normal_texture);
      cogl_object_unref (hover_texture);
      cogl_object_unref (active_texture);
      g_warning ("Failed to load button-disabled texture: %s", error->message);
      cogl_error_free (error);
    }

  button->label = pango_layout_new (ctx->pango_context);
  pango_layout_set_font_description (button->label, ctx->pango_font_desc);
  pango_layout_set_text (button->label, label, -1);

  pango_layout_get_extents (button->label, NULL, &label_size);
  button->label_width = PANGO_PIXELS (label_size.width);
  button->label_height = PANGO_PIXELS (label_size.height);

  button->width = button->label_width + 10;
  button->height = button->label_height + 23;

  button->background_normal =
    rut_nine_slice_new (ctx, normal_texture, 11, 5, 13, 5,
                        button->width,
                        button->height);

  button->background_hover =
    _rut_nine_slice_new_full (ctx, hover_texture, 11, 5, 13, 5,
                              button->width,
                              button->height,
                              button->background_normal->primitive);

  button->background_active =
    _rut_nine_slice_new_full (ctx, active_texture, 11, 5, 13, 5,
                              button->width,
                              button->height,
                              button->background_normal->primitive);

  button->background_disabled =
    _rut_nine_slice_new_full (ctx, disabled_texture, 11, 5, 13, 5,
                              button->width,
                              button->height,
                              button->background_normal->primitive);

  cogl_color_init_from_4f (&button->text_color, 0, 0, 0, 1);

  button->input_region =
    rut_input_region_new_rectangle (0, 0, button->width, button->height,
                                    _rut_button_input_cb,
                                    button);

  //rut_input_region_set_graphable (button->input_region, button);
  rut_graphable_add_child (button, button->input_region);

  return button;
}

RutClosure *
rut_button_add_on_click_callback (RutButton *button,
                                  RutButtonClickCallback callback,
                                  void *user_data,
                                  RutClosureDestroyCallback destroy_cb)
{
  g_return_val_if_fail (callback != NULL, NULL);

  return rut_closure_list_add (&button->on_click_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}


/*
 * TODO:
 *
 * Should we add a _queue_batch_update() mechanism or should scene
 * changing events just immediately modify the display lists?
 * - An advantage of deferring is that it can avoid potentially
 *   redundant work.
 * - A difficulty with this currently is that there isn't a way
 *   access the camera associated with a node in the graph.
 *
 * Should we add a "Widgetable" interface that implies:
 * - RefCountable
 * - Graphable
 * - PaintBatchable
 *
 * There are probably lots of utility apis we could add too for
 * widgets.
 *
 * Can we figure out a neat way of handling Cloning?
 * Can we figure out a neat way of handling per-camera state for
 * widgets?
 */
/*

When we paint we should paint an ordered list of cameras

Questions:
- Where should logic for picking and tracking if we have a valid pick buffer live?
-

Think about this idea of the "div" graph that feeds into a separate spacial graph which feeds into a render graph

-----

If I wanted a visual tool where I could have prototyped the hairy cube code how might that work:

- Some ui to setup a viewing frustum
- Some ui to add geometry to the scene - a few toy models such as spheres, pyramids cubes would have been fine here
- Some ui to render the noise textures
  - ui to create a texture
  - ui to create a camera around texture
  - DESC
  - ui to describe what to render to the camera
  - ui to add a random number generator
  -
- Some ui to describe N different pipelines - one for each shell
- Some ui to describe what to draw for a frame
  - An ordered list of drawing commands:
    - draw geometry X with pipeline Y
    - draw geometry A with pipeline B
    - ...

*/

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
      _rut_button_init_type ();
      _rut_graph_init_type ();
      _rut_transform_init_type ();
      _rut_timeline_init_type ();
      _rut_ui_viewport_init_type ();
      _rut_entity_init_type ();
      _rut_asset_type_init ();

      /* components */
      _rut_camera_init_type ();
      _rut_animation_clip_init_type ();
      _rut_light_init_type ();
      _rut_mesh_init_type ();
      _rut_material_init_type ();
      _rut_diamond_init_type ();
      _rut_diamond_slice_init_type ();

      g_once_init_leave (&init_status, 1);
    }
}

void
rut_color_init_from_uint32 (RutColor *color, uint32_t value)
{
  color->red = RUT_UINT32_RED_AS_FLOAT (value);
  color->green = RUT_UINT32_GREEN_AS_FLOAT (value);
  color->blue = RUT_UINT32_BLUE_AS_FLOAT (value);
  color->alpha = RUT_UINT32_ALPHA_AS_FLOAT (value);
}

