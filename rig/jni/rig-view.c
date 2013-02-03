/*
 * Rig
 *
 * Copyright (C) 2013 Intel Corporation.
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

#include <rut.h>

#include "rig-data.h"
#include "rig-view.h"

struct _RigView
{
  RutObjectProps _parent;

  RutContext *context;

  RutList preferred_size_cb_list;

  RutBoxLayout *vbox;
  RutBoxLayout *hbox;
  RutClosure *vbox_preferred_size_closure;

  float width, height;

  RutGraphableProps graphable;

  int ref_count;
};

RutType rig_view_type;

static void
_rig_view_free (void *object)
{
  RigView *view = object;

  rut_closure_list_disconnect_all (&view->preferred_size_cb_list);

  //rig_view_set_child (view, NULL);

  rut_shell_remove_pre_paint_callback (view->context->shell, view);

  rut_refable_unref (view->context);

  rut_graphable_destroy (view);

  rut_graphable_remove_child (view->hbox);
  rut_graphable_remove_child (view->vbox);

  g_slice_free (RigView, view);
}

RutRefCountableVTable _rig_view_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rig_view_free
};

static RutGraphableVTable _rig_view_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

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

static RutSizableVTable _rig_view_sizable_vtable = {
  rig_view_set_size,
  rig_view_get_size,
  rig_view_get_preferred_width,
  rig_view_get_preferred_height,
  rig_view_add_preferred_size_callback
};

static void
_rig_view_init_type (void)
{
  rut_type_init (&rig_view_type, "RigView");
  rut_type_add_interface (&rig_view_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigView, ref_count),
                          &_rig_view_ref_countable_vtable);
  rut_type_add_interface (&rig_view_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigView, graphable),
                          &_rig_view_graphable_vtable);
  rut_type_add_interface (&rig_view_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_view_sizable_vtable);
}

RigView *
rig_view_new (RigData *data)
{
  RutContext *ctx = data->ctx;
  RigView *view = g_slice_new0 (RigView);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_view_init_type ();

      initialized = TRUE;
    }

  view->ref_count = 1;
  view->context = rut_refable_ref (ctx);

  rut_list_init (&view->preferred_size_cb_list);

  rut_object_init (&view->_parent, &rig_view_type);

  rut_graphable_init (RUT_OBJECT (view));

  view->vbox =
    rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM, NULL);
  rut_graphable_add_child (view, view->vbox);
  rut_refable_unref (view->vbox);
  view->hbox =
    rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT, NULL);
  rut_graphable_add_child (view->vbox, view->hbox);
  rut_refable_unref (view->hbox);

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
    rut_refable_ref (child_widget);

  if (view->child)
    {
      rut_graphable_remove_child (view->child);
      rut_closure_disconnect (view->vbox_preferred_size_closure);
      view->vbox_preferred_size_closure = NULL;
      rut_refable_unref (view->child);
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
