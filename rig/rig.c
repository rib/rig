/*
 * Rig
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
#include "rig-bitmask.h"
#include "rig-camera-private.h"
#include "rig-transform-private.h"

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
 * Note: Rig doesn't actually tackle any of these particularly well currently
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
 * Interfaces defined currently for Rig are:
 * "RefCountable"
 *  - implies an int ref_count property and ref, unref, free methods
 * "Graphable"
 *  - implies parent and children properties but no methods
 * "PaintBatchable"
 *  - no properties implied but adds set_insert_point and update methods.
 *
 * The Rig rendering model was designed so objects retain drawing primitives
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
 * a rig_display_list api which would allow us to internally queue list
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

#include "rig.h"

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
 *    rig_batchable_insert_batch (RigObject *object,
 *                                RigBatchContext *paint_ctx,
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

struct _RigGraph
{
  RigObjectProps _parent;
  int ref_count;

  RigGraphableProps graphable;
};

typedef struct _RigNineSlice
{
  RigObjectProps _parent;
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

  RigGraphableProps graphable;
  RigPaintableProps paintable;

} RigNineSlice;

struct _RigButton
{
  RigObjectProps _parent;
  int ref_count;

  PangoLayout *label;
  int label_width;
  int label_height;

  RigNineSlice *background;
  CoglColor text_color;

  RigSimpleWidgetProps simple_widget;

  RigGraphableProps graphable;
  RigPaintableProps paintable;
};

struct _RigRectangle
{
  RigObjectProps _parent;
  int ref_count;

  float width;
  float height;

  RigSimpleWidgetProps simple_widget;

  RigGraphableProps graphable;
  RigPaintableProps paintable;

  CoglPipeline *pipeline;

};

typedef struct _RigTextureCacheEntry
{
  RigContext *ctx;
  char *filename;
  CoglTexture *texture;
} RigTextureCacheEntry;
#define RIG_TEXTURE_CACHE_ENTRY(X) ((RigTextureCacheEntry *)X)

static CoglUserDataKey texture_cache_key;

static uint8_t _rig_nine_slice_indices_data[] = {
    0,4,5,   0,5,1,  1,5,6,   1,6,2,   2,6,7,    2,7,3,
    4,8,9,   4,9,5,  5,9,10,  5,10,6,  6,10,11,  6,11,7,
    8,12,13, 8,13,9, 9,13,14, 9,14,10, 10,14,15, 10,15,11
};

static void
_rig_context_free (void *object)
{
  RigContext *ctx = object;

  rig_property_context_destroy (&ctx->property_ctx);

  g_object_unref (ctx->pango_context);
  g_object_unref (ctx->pango_font_map);
  pango_font_description_free (ctx->pango_font_desc);

  g_hash_table_destroy (ctx->texture_cache);

  if (rig_cogl_context == ctx->cogl_context)
    {
      cogl_object_unref (rig_cogl_context);
      rig_cogl_context = NULL;
    }

  cogl_object_unref (ctx->cogl_context);

  g_slice_free (RigContext, ctx);
}

static RigRefCountableVTable _rig_context_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_context_free
};

RigType rig_context_type;

static void
_rig_context_init_type (void)
{
  rig_type_init (&rig_context_type);
  rig_type_add_interface (&rig_context_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigContext, ref_count),
                          &_rig_context_ref_countable_vtable);
}

static void
_rig_texture_cache_entry_free (RigTextureCacheEntry *entry)
{
  g_free (entry->filename);
  g_slice_free (RigTextureCacheEntry, entry);
}

static void
_rig_texture_cache_entry_destroy_cb (void *entry)
{
  _rig_texture_cache_entry_free (entry);
}

static void
texture_destroyed_cb (void *user_data)
{
  RigTextureCacheEntry *entry = user_data;

  g_hash_table_remove (entry->ctx->texture_cache, entry->filename);
  _rig_texture_cache_entry_free (entry);
}

CoglTexture *
rig_load_texture (RigContext *ctx, const char *filename, GError **error)
{
  RigTextureCacheEntry *entry =
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

  entry = g_slice_new0 (RigTextureCacheEntry);
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

RigContext *
rig_context_new (RigShell *shell)
{
  RigContext *context = g_new0 (RigContext, 1);
  GError *error = NULL;

  _rig_init ();

  rig_object_init (&context->_parent, &rig_context_type);

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

  /* We set up the first created RigContext as a global default context */
  if (rig_cogl_context == NULL)
    rig_cogl_context = cogl_object_ref (context->cogl_context);

  context->texture_cache =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           NULL,
                           _rig_texture_cache_entry_destroy_cb);

  context->nine_slice_indices =
    cogl_indices_new (context->cogl_context,
                      COGL_INDICES_TYPE_UNSIGNED_BYTE,
                      _rig_nine_slice_indices_data,
                      sizeof (_rig_nine_slice_indices_data) /
                      sizeof (_rig_nine_slice_indices_data[0]));

  context->pango_font_map =
    COGL_PANGO_FONT_MAP (cogl_pango_font_map_new (context->cogl_context));

  cogl_pango_font_map_set_use_mipmapping (context->pango_font_map, TRUE);

  context->pango_context =
    pango_font_map_create_context (PANGO_FONT_MAP (context->pango_font_map));

  context->pango_font_desc = pango_font_description_new ();
  pango_font_description_set_family (context->pango_font_desc, "Sans");
  pango_font_description_set_size (context->pango_font_desc, 14 * PANGO_SCALE);

  rig_property_context_init (&context->property_ctx);

  if (shell)
    {
      context->shell = rig_ref_countable_ref (shell);

      _rig_shell_associate_context (shell, context);
    }

  return context;
}

void
rig_context_init (RigContext *context)
{
  if (context->shell)
    _rig_shell_init (context->shell);
}

static void
_rig_camera_free (void *object)
{
  RigCamera *camera = object;
  GList *l;

  cogl_object_unref (camera->fb);

  for (l = camera->input_regions; l; l = l->next)
    rig_ref_countable_unref (l->data);
  g_list_free (camera->input_regions);

  g_slice_free (RigCamera, object);
}

RigRefCountableVTable _rig_camera_ref_countable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_camera_free
};

#if 0
static RigTraverseVisitFlags
_rig_widget_set_camera_cb (RigObject *object,
                           int depth,
                           void *user_data)
{
  if (rig_object_is (object, RIG_INTERFACE_ID_SIMPLE_WIDGET))
    {
      RigSimpleWidgetVTable *widget_vtable =
        rig_object_get_vtable (object, RIG_INTERFACE_ID_SIMPLE_WIDGET);
      if (widget_vtable->set_camera)
        widget_vtable->set_camera (object, user_data);
    }

  return RIG_TRAVERSE_VISIT_CONTINUE;
}
#endif

static void
_rig_camera_graphable_child_removed (RigObject *self,
                                    RigObject *child)
{
#if 0
  rig_graphable_traverse (child,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_widget_set_camera_cb,
                          NULL, /* after callback */
                          NULL);
#endif
}

static void
_rig_camera_graphable_child_added (RigObject *self,
                                  RigObject *child)

{
#if 0
  RigCamera *camera = RIG_CAMERA (self);
  rig_graphable_traverse (child,
                          RIG_TRAVERSE_DEPTH_FIRST,
                          _rig_widget_set_camera_cb,
                          NULL, /* after callback */
                          camera);
#endif
}

static void
_rig_camera_graphable_parent_changed (RigObject *self,
                                      RigObject *old_parent,
                                      RigObject *new_parent)
{
 /* nop */
}

static RigGraphableVTable _rig_camera_graphable_vtable = {
  _rig_camera_graphable_child_removed,
  _rig_camera_graphable_child_added,
  _rig_camera_graphable_parent_changed
};

RigType rig_camera_type;

static void
_rig_camera_init_type (void)
{
  rig_type_init (&rig_camera_type);
  rig_type_add_interface (&rig_camera_type,
                           RIG_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RigCamera, ref_count),
                           &_rig_camera_ref_countable);
  rig_type_add_interface (&rig_camera_type,
                           RIG_INTERFACE_ID_GRAPHABLE,
                           offsetof (RigCamera, graphable),
                           &_rig_camera_graphable_vtable);
}

RigCamera *
rig_camera_new (RigContext *ctx, CoglFramebuffer *framebuffer)
{
  RigCamera *camera = g_slice_new0 (RigCamera);
  int width = cogl_framebuffer_get_width (framebuffer);
  int height = cogl_framebuffer_get_height (framebuffer);

  camera->ctx = rig_ref_countable_ref (ctx);

  rig_object_init (&camera->_parent, &rig_camera_type);

  camera->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (camera));

  cogl_matrix_init_identity (&camera->projection);
  cogl_matrix_orthographic (&camera->projection,
                            0, 0, width, height,
                            -1, 100);
  cogl_framebuffer_set_projection_matrix (framebuffer,
                                          &camera->projection);

  cogl_matrix_init_identity (&camera->view);
  camera->viewport[2] = width;
  camera->viewport[3] = height;
  cogl_framebuffer_set_viewport (framebuffer,
                                 camera->viewport[0],
                                 camera->viewport[1],
                                 camera->viewport[2],
                                 camera->viewport[3]);


  cogl_matrix_init_identity (&camera->input_transform);

  camera->fb = cogl_object_ref (framebuffer);
  camera->age = 0;

  camera->frame = 0;
  camera->timer = g_timer_new ();
  g_timer_start (camera->timer);

  return camera;
}

CoglFramebuffer *
rig_camera_get_framebuffer (RigCamera *camera)
{
  return camera->fb;
}

void
rig_camera_set_viewport (RigCamera *camera,
                         float x,
                         float y,
                         float width,
                         float height)
{
  camera->viewport[0] = x;
  camera->viewport[1] = y;
  camera->viewport[2] = width;
  camera->viewport[3] = height;
}

const float *
rig_camera_get_viewport (RigCamera *camera)
{
  return camera->viewport;
}

void
rig_camera_set_projection (RigCamera *camera,
                           const CoglMatrix *projection)
{
  camera->projection = *projection;
  cogl_framebuffer_set_projection_matrix (camera->fb,
                                          &camera->projection);
  camera->inverse_cached = FALSE;
}

const CoglMatrix *
rig_camera_get_projection (RigCamera *camera)
{
  return &camera->projection;
}

const CoglMatrix *
rig_camera_get_inverse_projection (RigCamera *camera)
{
  if (camera->inverse_cached)
    return &camera->inverse_projection;

  if (!cogl_matrix_get_inverse (&camera->projection,
                                &camera->inverse_projection))
    return NULL;

  camera->inverse_cached = TRUE;
  return &camera->inverse_projection;
}

void
rig_camera_set_view_transform (RigCamera *camera,
                               const CoglMatrix *view)
{
  camera->view = *view;

  /* XXX: we have no way to assert that we are at the bottom of the
   * matrix stack at this point, so this might do bad things...
   */
  cogl_framebuffer_set_modelview_matrix (camera->fb,
                                         &camera->view);
}

const CoglMatrix *
rig_camera_get_view_transform (RigCamera *camera)
{
  return &camera->view;
}

void
rig_camera_set_input_transform (RigCamera *camera,
                                const CoglMatrix *input_transform)
{
  camera->input_transform = *input_transform;
}

void
rig_camera_add_input_region (RigCamera *camera,
                             RigInputRegion *region)
{
  g_print ("add input region %p, %p\n", camera, region);
  camera->input_regions = g_list_prepend (camera->input_regions, region);
}

void
rig_camera_remove_input_region (RigCamera *camera,
                                RigInputRegion *region)
{
  camera->input_regions = g_list_remove (camera->input_regions, region);
}

#if 0
typedef struct _RigCameraInputCallbackState
{
  RigInputCallback callback;
  void *user_data;
} RigCameraInputCallbackState;

RigInputEventStatus
_rig_camera_input_callback_wrapper (RigCameraInputCallbackState *state,
                                    RigInputEvent *event)
{
  RigCamera *camera = state->camera;
  float *viewport = camera->viewport;

  cogl_matrix_translate (event->input_transform,
                         viewport[0], viewport[1], 0);
  cogl_matrix_scale (event->input_transform,
                     cogl_framebuffer_get_width (camera->fb) / viewport[2],
                     cogl_framebuffer_get_height (camera->fb) / viewport[3]);

  return state->callback (event, state->user_data);
}

void
rig_camera_add_input_callback (RigCamera *camera,
                               RigInputCallback callback,
                               void *user_data)
{
  RigCameraInputCallbackState *state = g_slice_new (RigCameraInputCallbackState);
  state->camera = camera;
  state->callback = callback;
  state->user_data = user_data;
  camera->input_callbacks = g_list_prepend (camera->input_callbacks, state);
}
#endif

CoglBool
rig_camera_transform_window_coordinate (RigCamera *camera,
                                        float *x,
                                        float *y)
{
  float *viewport = camera->viewport;
  *x -= viewport[0];
  *y -= viewport[1];

  if (*x < 0 || *x >= viewport[2] || *y < 0 || *y >= viewport[3])
    return FALSE;
  else
    return TRUE;
}

typedef struct _CameraFlushState
{
  RigCamera *current_camera;
} CameraFlushState;

static void
free_camera_flush_state (void *user_data)
{
  CameraFlushState *state = user_data;
  g_slice_free (CameraFlushState, state);
}

void
rig_camera_flush (RigCamera *camera)
{
  static CoglUserDataKey fb_camera_key;
  CameraFlushState *state;
  CoglFramebuffer *framebuffer = camera->fb;

  state = cogl_object_get_user_data (COGL_OBJECT (framebuffer),
                                     &fb_camera_key);
  if (!state)
    {
      state = g_slice_new (CameraFlushState);
      state->current_camera = camera;
      cogl_object_set_user_data (COGL_OBJECT (framebuffer),
                                 &fb_camera_key,
                                 state,
                                 free_camera_flush_state);
    }
  else if (state->current_camera == camera)
    return;

  cogl_framebuffer_set_viewport (framebuffer,
                                 camera->viewport[0],
                                 camera->viewport[1],
                                 camera->viewport[2],
                                 camera->viewport[3]);
  cogl_framebuffer_set_projection_matrix (framebuffer, &camera->projection);
  cogl_framebuffer_push_matrix (framebuffer);
  cogl_framebuffer_set_modelview_matrix (framebuffer, &camera->view);

  state->current_camera = camera;
}

void
rig_camera_end_frame (RigCamera *camera)
{
  double elapsed;

  camera->frame++;

  elapsed = g_timer_elapsed (camera->timer, NULL);
  if (elapsed > 1.0)
    {
      g_print ("fps = %f (camera = %p)\n",
               camera->frame / elapsed,
               camera);
      g_timer_start (camera->timer);
      camera->frame = 0;
    }
}

void
rig_paintable_init (RigObject *object)
{
#if 0
  RigPaintableProps *props =
    rig_object_get_properties (object, RIG_INTERFACE_ID_PAINTABLE);
#endif
}

static void
_rig_nine_slice_free (void *object)
{
  RigNineSlice *nine_slice = object;

  cogl_object_unref (nine_slice->texture);

  cogl_object_unref (nine_slice->pipeline);
  cogl_object_unref (nine_slice->primitive);

  g_slice_free (RigNineSlice, object);
}

RigRefCountableVTable _rig_nine_slice_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_nine_slice_free
};

static void
_rig_nine_slice_graphable_child_removed (RigObject *self,
                                         RigObject *child)
{
  /* You can't add children to a button currently */
  g_warn_if_reached ();
}

static void
_rig_nine_slice_graphable_child_added (RigObject *self,
                                       RigObject *child)
{
  /* You can't add children to a button currently */
  g_warn_if_reached ();
}

static void
_rig_nine_slice_graphable_parent_changed (RigObject *self,
                                          RigObject *old_parent,
                                          RigObject *new_parent)
{
  /* nop */
}

static RigGraphableVTable _rig_nine_slice_graphable_vtable = {
  _rig_nine_slice_graphable_child_removed,
  _rig_nine_slice_graphable_child_added,
  _rig_nine_slice_graphable_parent_changed
};

static void
_rig_nine_slice_paint (RigObject *object,
                       RigPaintContext *paint_ctx)
{
  RigNineSlice *nine_slice = RIG_NINE_SLICE (object);
  RigCamera *camera = paint_ctx->camera;

  cogl_framebuffer_draw_primitive (camera->fb,
                                   nine_slice->pipeline,
                                   nine_slice->primitive);
}

static RigPaintableVTable _rig_nine_slice_paintable_vtable = {
  _rig_nine_slice_paint
};

RigType rig_nine_slice_type;

static void
_rig_nine_slice_init_type (void)
{
  rig_type_init (&rig_nine_slice_type);
  rig_type_add_interface (&rig_nine_slice_type,
                           RIG_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RigNineSlice, ref_count),
                           &_rig_nine_slice_ref_countable_vtable);
  rig_type_add_interface (&rig_nine_slice_type,
                           RIG_INTERFACE_ID_GRAPHABLE,
                           offsetof (RigNineSlice, graphable),
                           &_rig_nine_slice_graphable_vtable);
  rig_type_add_interface (&rig_nine_slice_type,
                           RIG_INTERFACE_ID_PAINTABLE,
                           offsetof (RigNineSlice, paintable),
                           &_rig_nine_slice_paintable_vtable);
}

static CoglPrimitive *
primitive_new_textured_rectangle (RigContext *ctx,
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

RigNineSlice *
rig_nine_slice_new (RigContext *ctx,
                    CoglTexture *texture,
                    float top,
                    float right,
                    float bottom,
                    float left,
                    float width,
                    float height)
{
  RigNineSlice *nine_slice = g_slice_new (RigNineSlice);

  rig_object_init (&nine_slice->_parent, &rig_nine_slice_type);

  nine_slice->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (nine_slice));

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
                                  sizeof (_rig_nine_slice_indices_data) /
                                  sizeof (_rig_nine_slice_indices_data[0]));
    }

  return nine_slice;
}

static void
_rig_graph_free (void *object)
{
  RigGraph *graph = RIG_GRAPH (object);
  GList *l;

  for (l = graph->graphable.children.head; l; l = l->next)
    rig_graphable_remove_child (object, l->data);

  g_slice_free (RigGraph, object);
}

RigRefCountableVTable _rig_graph_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_graph_free
};

static void
_rig_graph_graphable_child_removed (RigObject *self,
                                    RigObject *child)
{
  /* nop */
}

static void
_rig_graph_graphable_child_added (RigObject *self,
                                  RigObject *child)
{
  /* nop */
}

static void
_rig_graph_graphable_parent_changed (RigObject *self,
                                     RigObject *old_parent,
                                     RigObject *new_parent)
{
  /* nop */
}

RigGraphableVTable _rig_graph_graphable_vtable = {
  _rig_graph_graphable_child_removed,
  _rig_graph_graphable_child_added,
  _rig_graph_graphable_parent_changed
};

RigType rig_graph_type;

static void
_rig_graph_init_type (void)
{
  rig_type_init (&rig_graph_type);
  rig_type_add_interface (&rig_graph_type,
                           RIG_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RigButton, ref_count),
                           &_rig_graph_ref_countable_vtable);
  rig_type_add_interface (&rig_graph_type,
                           RIG_INTERFACE_ID_GRAPHABLE,
                           offsetof (RigGraph, graphable),
                           &_rig_graph_graphable_vtable);
}

RigGraph *
rig_graph_new (RigContext *ctx, ...)
{
  RigGraph *graph = g_slice_new (RigGraph);
  RigObject *object;
  va_list ap;

  rig_object_init (&graph->_parent, &rig_graph_type);

  graph->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (graph));

  va_start (ap, ctx);
  while ((object = va_arg (ap, RigObject *)))
    rig_graphable_add_child (RIG_OBJECT (graph), object);
  va_end (ap);

  return graph;
}

static void
_rig_transform_free (void *object)
{
  RigTransform *transform = RIG_TRANSFORM (object);
  GList *l;

  for (l = transform->graphable.children.head; l; l = l->next)
    rig_graphable_remove_child (object, l->data);

  g_slice_free (RigTransform, object);
}

RigRefCountableVTable _rig_transform_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_transform_free
};

static void
_rig_transform_graphable_child_removed (RigObject *self,
                                    RigObject *child)
{
  /* nop */
}

static void
_rig_transform_graphable_child_added (RigObject *self,
                                  RigObject *child)
{
  /* nop */
}

static void
_rig_transform_graphable_parent_changed (RigObject *self,
                                     RigObject *old_parent,
                                     RigObject *new_parent)
{
  /* nop */
}

static RigGraphableVTable _rig_transform_graphable_vtable = {
  _rig_transform_graphable_child_removed,
  _rig_transform_graphable_child_added,
  _rig_transform_graphable_parent_changed
};

RigType rig_transform_type;

static void
_rig_transform_init_type (void)
{
  rig_type_init (&rig_transform_type);
  rig_type_add_interface (&rig_transform_type,
                           RIG_INTERFACE_ID_REF_COUNTABLE,
                           offsetof (RigButton, ref_count),
                           &_rig_transform_ref_countable_vtable);
  rig_type_add_interface (&rig_transform_type,
                           RIG_INTERFACE_ID_GRAPHABLE,
                           offsetof (RigTransform, graphable),
                           &_rig_transform_graphable_vtable);
}

RigTransform *
rig_transform_new (RigContext *ctx,
                   ...)
{
  RigTransform *transform = g_slice_new (RigTransform);
  RigObject *object;
  va_list ap;

  rig_object_init (&transform->_parent, &rig_transform_type);

  transform->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (transform));

  cogl_matrix_init_identity (&transform->matrix);

  va_start (ap, ctx);
  while ((object = va_arg (ap, RigObject *)))
    rig_graphable_add_child (RIG_OBJECT (transform), object);
  va_end (ap);

  return transform;
}

void
rig_transform_translate (RigTransform *transform,
                         float x,
                         float y,
                         float z)
{
  cogl_matrix_translate (&transform->matrix, x, y, z);
}

void
rig_transform_scale (RigTransform *transform,
                     float x,
                     float y,
                     float z)
{
  cogl_matrix_scale (&transform->matrix, x, y, z);
}

void
rig_transform_init_identity (RigTransform *transform)
{
  cogl_matrix_init_identity (&transform->matrix);
}

const CoglMatrix *
rig_transform_get_matrix (RigTransform *transform)
{
  return &transform->matrix;
}

void
rig_simple_widget_graphable_parent_changed (RigObject *self,
                                            RigObject *old_parent,
                                            RigObject *new_parent)
{
  /* nop */
}

void
rig_simple_widget_graphable_child_removed_warn (RigObject *self,
                                                RigObject *child)
{
  /* You can't add children to a button currently */
  g_warn_if_reached ();
}

void
rig_simple_widget_graphable_child_added_warn (RigObject *self,
                                              RigObject *child)
{
  /* You can't add children to a button currently */
  g_warn_if_reached ();
}

static void
_rig_rectangle_free (void *object)
{
  RigRectangle *rectangle = object;

  cogl_object_unref (rectangle->pipeline);

  g_slice_free (RigRectangle, object);
}

static RigRefCountableVTable _rig_rectangle_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_rectangle_free
};

static RigGraphableVTable _rig_rectangle_graphable_vtable = {
  rig_simple_widget_graphable_child_removed_warn,
  rig_simple_widget_graphable_child_added_warn,
  rig_simple_widget_graphable_parent_changed
};

static void
_rig_rectangle_paint (RigObject *object,
                      RigPaintContext *paint_ctx)
{
  RigRectangle *rectangle = RIG_RECTANGLE (object);
  RigCamera *camera = paint_ctx->camera;

  cogl_framebuffer_draw_rectangle (camera->fb,
                                   rectangle->pipeline,
                                   0, 0,
                                   rectangle->width,
                                   rectangle->height);
}

static RigPaintableVTable _rig_rectangle_paintable_vtable = {
  _rig_rectangle_paint
};

static RigSimpleWidgetVTable _rig_rectangle_simple_widget_vtable = {
 0
};

RigType rig_rectangle_type;

static void
_rig_rectangle_init_type (void)
{
  rig_type_init (&rig_rectangle_type);
  rig_type_add_interface (&rig_rectangle_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigRectangle, ref_count),
                          &_rig_rectangle_ref_countable_vtable);
  rig_type_add_interface (&rig_rectangle_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigRectangle, graphable),
                          &_rig_rectangle_graphable_vtable);
  rig_type_add_interface (&rig_rectangle_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigRectangle, paintable),
                          &_rig_rectangle_paintable_vtable);
  rig_type_add_interface (&rig_rectangle_type,
                          RIG_INTERFACE_ID_SIMPLE_WIDGET,
                          offsetof (RigRectangle, simple_widget),
                          &_rig_rectangle_simple_widget_vtable);
}

RigRectangle *
rig_rectangle_new4f (RigContext *ctx,
                     float width,
                     float height,
                     float red,
                     float green,
                     float blue,
                     float alpha)
{
  RigRectangle *rectangle = g_slice_new0 (RigRectangle);

  rig_object_init (RIG_OBJECT (rectangle), &rig_rectangle_type);

  rectangle->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (rectangle));
  rig_paintable_init (RIG_OBJECT (rectangle));

  rectangle->width = width;
  rectangle->height = height;

  rectangle->pipeline = cogl_pipeline_new (ctx->cogl_context);
  cogl_pipeline_set_color4f (rectangle->pipeline,
                             red, green, blue, alpha);

  return rectangle;
}

void
rig_rectangle_set_width (RigRectangle *rectangle, float width)
{
  rectangle->width = width;
}

void
rig_rectangle_set_height (RigRectangle *rectangle, float height)
{
  rectangle->height = height;
}

static void
_rig_button_free (void *object)
{
  RigButton *button = object;

  rig_ref_countable_unref (button->background);

  g_object_unref (button->label);

  g_slice_free (RigButton, object);
}

RigRefCountableVTable _rig_button_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_button_free
};

static RigGraphableVTable _rig_button_graphable_vtable = {
  rig_simple_widget_graphable_child_removed_warn,
  rig_simple_widget_graphable_child_added_warn,
  rig_simple_widget_graphable_parent_changed
};

static void
_rig_button_paint (RigObject *object,
                   RigPaintContext *paint_ctx)
{
  RigButton *button = RIG_BUTTON (object);
  RigCamera *camera = paint_ctx->camera;
  RigPaintableVTable *bg_paintable =
    rig_object_get_vtable (button->background, RIG_INTERFACE_ID_PAINTABLE);

  bg_paintable->paint (RIG_OBJECT (button->background), paint_ctx);

  cogl_pango_show_layout (camera->fb,
                          button->label,
                          5, 11,
                          &button->text_color);
}

static RigPaintableVTable _rig_button_paintable_vtable = {
  _rig_button_paint
};

RigSimpleWidgetVTable _rig_button_simple_widget_vtable = {
 0
};

RigType rig_button_type;

static void
_rig_button_init_type (void)
{
  rig_type_init (&rig_button_type);
  rig_type_add_interface (&rig_button_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigButton, ref_count),
                          &_rig_button_ref_countable_vtable);
  rig_type_add_interface (&rig_button_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigButton, graphable),
                          &_rig_button_graphable_vtable);
  rig_type_add_interface (&rig_button_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigButton, paintable),
                          &_rig_button_paintable_vtable);
  rig_type_add_interface (&rig_button_type,
                          RIG_INTERFACE_ID_SIMPLE_WIDGET,
                          offsetof (RigButton, simple_widget),
                          &_rig_button_simple_widget_vtable);
}

RigButton *
rig_button_new (RigContext *ctx,
                const char *label)
{
  RigButton *button = g_slice_new0 (RigButton);
  CoglTexture *texture;
  GError *error = NULL;
  PangoRectangle label_size;

  rig_object_init (RIG_OBJECT (button), &rig_button_type);

  button->ref_count = 1;

  rig_graphable_init (RIG_OBJECT (button));
  rig_paintable_init (RIG_OBJECT (button));

  texture = rig_load_texture (ctx, RIG_DATA_DIR "button.png", &error);
  if (!texture)
    {
      g_warning ("Failed to load button texture: %s", error->message);
      g_error_free (error);
    }

  button->label = pango_layout_new (ctx->pango_context);
  pango_layout_set_font_description (button->label, ctx->pango_font_desc);
  pango_layout_set_text (button->label, label, -1);

  pango_layout_get_extents (button->label, NULL, &label_size);
  button->label_width = PANGO_PIXELS (label_size.width);
  button->label_height = PANGO_PIXELS (label_size.height);

  button->background = rig_nine_slice_new (ctx, texture, 11, 5, 13, 5,
                                           button->label_width + 10,
                                           button->label_height + 23);

  cogl_color_init_from_4f (&button->text_color, 0, 0, 0, 1);

  return button;
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
_rig_init (void)
{
  static size_t init_status = 0;

  if (g_once_init_enter (&init_status))
    {
      //bindtextdomain (GETTEXT_PACKAGE, RIG_LOCALEDIR);
      //bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

      g_type_init ();

      _rig_context_init_type ();
      _rig_camera_init_type ();
      _rig_nine_slice_init_type ();
      _rig_rectangle_init_type ();
      _rig_button_init_type ();
      _rig_graph_init_type ();
      _rig_transform_init_type ();
      _rig_timeline_init_type ();

      g_once_init_leave (&init_status, 1);
    }
}


