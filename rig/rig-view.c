/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013 Intel Corporation.
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
 *
 */

#include <config.h>

#include <rut.h>

#include "rig-engine.h"
#include "rig-view.h"

struct _RigView
{
  RutObjectBase _base;

  RutContext *context;

  RutList preferred_size_cb_list;

  RutBoxLayout *vbox;
  RutBoxLayout *hbox;
  RutClosure *vbox_preferred_size_closure;

  float width, height;

  RutGraphableProps graphable;

};

static void
_rig_view_free (void *object)
{
  RigView *view = object;

  rut_closure_list_disconnect_all (&view->preferred_size_cb_list);

  //rig_view_set_child (view, NULL);

  rut_shell_remove_pre_paint_callback_by_graphable (view->context->shell, view);

  rut_object_unref (view->context);

  rut_graphable_destroy (view);

  rut_graphable_remove_child (view->hbox);
  rut_graphable_remove_child (view->vbox);

  c_slice_free (RigView, view);
}

static void
allocate_cb (RutObject *graphable,
             void *user_data)
{
  RigView *view = RIG_VIEW (graphable);
  rut_sizable_set_size (view->vbox, view->width, view->height);
}

static void
queue_allocation (RigView *view)
{
  rut_shell_add_pre_paint_callback (view->context->shell,
                                    view,
                                    allocate_cb,
                                    NULL /* user_data */);
}

static void
rig_view_set_size (void *object,
                  float width,
                  float height)
{
  RigView *view = object;

  if (width == view->width && height == view->height)
    return;

  view->width = width;
  view->height = height;

  queue_allocation (view);
}

static void
rig_view_get_preferred_width (void *sizable,
                              float for_height,
                              float *min_width_p,
                              float *natural_width_p)
{
  RigView *view = RIG_VIEW (sizable);

  rut_sizable_get_preferred_width (view->vbox,
                                   for_height,
                                   min_width_p,
                                   natural_width_p);
}

static void
rig_view_get_preferred_height (void *sizable,
                               float for_width,
                               float *min_height_p,
                               float *natural_height_p)
{
  RigView *view = RIG_VIEW (sizable);

  rut_sizable_get_preferred_height (view->vbox,
                                    for_width,
                                    min_height_p,
                                    natural_height_p);
}

static RutClosure *
rig_view_add_preferred_size_callback (void *object,
                                      RutSizablePreferredSizeCallback cb,
                                      void *user_data,
                                      RutClosureDestroyCallback destroy)
{
  RigView *view = object;

  return rut_closure_list_add (&view->preferred_size_cb_list,
                               cb,
                               user_data,
                               destroy);
}

static void
rig_view_get_size (void *object,
                   float *width,
                   float *height)
{
  RigView *view = object;

  *width = view->width;
  *height = view->height;
}

RutType rig_view_type;

static void
_rig_view_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
    NULL, /* child removed */
    NULL, /* child addded */
    NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
    rig_view_set_size,
    rig_view_get_size,
    rig_view_get_preferred_width,
    rig_view_get_preferred_height,
    rig_view_add_preferred_size_callback
  };

  RutType *type = &rig_view_type;
#define TYPE RigView

  rut_type_init (type, C_STRINGIFY (TYPE), _rig_view_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);

#undef TYPE
}

RigView *
rig_view_new (RigEngine *engine)
{
  RutContext *ctx = engine->ctx;
  RigView *view =
    rut_object_alloc0 (RigView, &rig_view_type, _rig_view_init_type);

  view->context = rut_object_ref (ctx);

  rut_list_init (&view->preferred_size_cb_list);


  rut_graphable_init (view);

  view->vbox = rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_graphable_add_child (view, view->vbox);
  rut_object_unref (view->vbox);
  view->hbox = rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT);
  rut_graphable_add_child (view->vbox, view->hbox);
  rut_object_unref (view->hbox);

  return view;
}

#if 0
static void
preferred_size_changed (RigView *view)
{
  rut_closure_list_invoke (&view->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           view);
}

static void
child_preferred_size_cb (RutObject *sizable,
                         void *user_data)
{
  RigView *view = user_data;

  preferred_size_changed (view);
  queue_allocation (view);
}

void
rig_view_set_child (RigView *view,
                    RutObject *child_widget)
{
  if (child_widget)
    rut_object_ref (child_widget);

  if (view->child)
    {
      rut_graphable_remove_child (view->child);
      rut_closure_disconnect (view->vbox_preferred_size_closure);
      view->vbox_preferred_size_closure = NULL;
      rut_object_unref (view->child);
    }

  view->child = child_widget;
  rut_graphable_add_child (view->child_transform, child_widget);

  if (child_widget)
    {
      view->vbox_preferred_size_closure =
        rut_sizable_add_preferred_size_callback (child_widget,
                                                 child_preferred_size_cb,
                                                 view,
                                                 NULL /* destroy */);
    }

  preferred_size_changed (view);
  queue_allocation (view);
}
#endif
