/*
 * Rig
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rut.h"
#include "rut-box-layout.h"

typedef struct
{
  RutList link;
  RutObject *transform;
  RutObject *widget;
  RutClosure *preferred_size_closure;
} RutBoxLayoutChild;

struct _RutBoxLayout
{
  RutObjectProps _parent;

  RutContext *context;

  RutList preferred_size_cb_list;
  RutList children;

  RutBoxLayoutOrientation orientation;

  float width, height;

  RutGraphableProps graphable;

  int ref_count;
};

RutType rut_box_layout_type;

static void
_rut_box_layout_free (void *object)
{
  RutBoxLayout *box = object;

  rut_closure_list_disconnect_all (&box->preferred_size_cb_list);

  while (!rut_list_empty (&box->children))
    {
      RutBoxLayoutChild *child =
        rut_container_of (box->children.next, child, link);

      rut_box_layout_remove (box, child->widget);
    }

  rut_shell_remove_pre_paint_callback (box->context->shell, box);

  rut_refable_unref (box->context);

  rut_graphable_destroy (box);

  g_slice_free (RutBoxLayout, box);
}

RutRefCountableVTable _rut_box_layout_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_box_layout_free
};

static RutGraphableVTable _rut_box_layout_graphable_vtable = {
  NULL, /* child removed */
  NULL, /* child addded */
  NULL /* parent changed */
};

static void
allocate_cb (RutObject *graphable,
             void *user_data)
{
  RutBoxLayout *box = RUT_BOX_LAYOUT (graphable);
  RutBoxLayoutChild *child;
  /* The position along whichever axis is the orientation */
  float pos = 0.0f;

  /* FIXME: This is currently just ignoring its allocated size. It
   * should probably squish the children to fix or expand them as
   * necessary. */

  /* FIXME: It should be configurable whether it expands its children
   * on the axis that isn't the orientation */

  /* Allocate the children using the calculated max size */
  rut_list_for_each (child, &box->children, link)
    {
      float size;

      rut_transform_init_identity (child->transform);

      switch (box->orientation)
        {
        case RUT_BOX_LAYOUT_ORIENTATION_HORIZONTAL:
          rut_sizable_get_preferred_width (child->widget,
                                           box->height, /* for_height */
                                           NULL, /* min_width_p */
                                           &size);
          rut_sizable_set_size (child->widget, size, box->height);
          rut_transform_translate (child->transform, pos, 0.0f, 0.0f);
          break;

        case RUT_BOX_LAYOUT_ORIENTATION_VERTICAL:
          rut_sizable_get_preferred_height (child->widget,
                                            box->width, /* for_width */
                                            NULL, /* min_heighth_p */
                                            &size);
          rut_sizable_set_size (child->widget, box->width, size);
          rut_transform_translate (child->transform, 0.0f, pos, 0.0f);
          break;
        }

      pos += size;
    }
}

static void
queue_allocation (RutBoxLayout *box)
{
  rut_shell_add_pre_paint_callback (box->context->shell,
                                    box,
                                    allocate_cb,
                                    NULL /* user_data */);
}

static void
preferred_size_changed (RutBoxLayout *box)
{
  rut_closure_list_invoke (&box->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           box);
}

static void
rut_box_layout_set_size (void *object,
                         float width,
                         float height)
{
  RutBoxLayout *box = object;

  if (width == box->width && height == box->height)
    return;

  box->width = width;
  box->height = height;

  queue_allocation (box);
}

static void
get_main_preferred_size (RutBoxLayout *box,
                         float for_size,
                         float *min_size_p,
                         float *natural_size_p)
{
  float total_min_size = 0.0f;
  float total_natural_size = 0.0f;
  RutBoxLayoutChild *child;

  rut_list_for_each (child, &box->children, link)
    {
      float min_size = 0.0f, natural_size = 0.0f;

      switch (box->orientation)
        {
        case RUT_BOX_LAYOUT_ORIENTATION_HORIZONTAL:
          rut_sizable_get_preferred_width (child->widget,
                                           for_size, /* for_height */
                                           min_size_p ?
                                           &min_size :
                                           NULL,
                                           natural_size_p ?
                                           &natural_size :
                                           NULL);
          break;

        case RUT_BOX_LAYOUT_ORIENTATION_VERTICAL:
          rut_sizable_get_preferred_height (child->widget,
                                            for_size, /* for_width */
                                            min_size_p ?
                                            &min_size :
                                            NULL,
                                            natural_size_p ?
                                            &natural_size :
                                            NULL);
          break;
        }

      total_min_size += min_size;
      total_natural_size += natural_size;
    }

  if (min_size_p)
    *min_size_p = total_min_size;
  if (natural_size_p)
    *natural_size_p = total_natural_size;
}

static void
get_other_preferred_size (RutBoxLayout *box,
                          float for_size,
                          float *min_size_p,
                          float *natural_size_p)
{
  float max_min_size = 0.0f;
  float max_natural_size = 0.0f;
  RutBoxLayoutChild *child;

  rut_list_for_each (child, &box->children, link)
    {
      float min_size = 0.0f, natural_size = 0.0f;

      switch (box->orientation)
        {
        case RUT_BOX_LAYOUT_ORIENTATION_HORIZONTAL:
          rut_sizable_get_preferred_width (child->widget,
                                           -1, /* for_height */
                                           min_size_p ?
                                           &min_size :
                                           NULL,
                                           natural_size_p ?
                                           &natural_size :
                                           NULL);
          break;

        case RUT_BOX_LAYOUT_ORIENTATION_VERTICAL:
          rut_sizable_get_preferred_height (child->widget,
                                            -1, /* for_width */
                                            min_size_p ?
                                            &min_size :
                                            NULL,
                                            natural_size_p ?
                                            &natural_size :
                                            NULL);
          break;
        }

      if (min_size > max_min_size)
        max_min_size = min_size;
      if (natural_size > max_natural_size)
        max_natural_size = natural_size;
    }

  if (min_size_p)
    *min_size_p = max_min_size;
  if (natural_size_p)
    *natural_size_p = max_natural_size;
}

static void
rut_box_layout_get_preferred_width (void *sizable,
                                    float for_height,
                                    float *min_width_p,
                                    float *natural_width_p)
{
  RutBoxLayout *box = RUT_BOX_LAYOUT (sizable);

  switch (box->orientation)
    {
    case RUT_BOX_LAYOUT_ORIENTATION_HORIZONTAL:
      get_main_preferred_size (box, for_height, min_width_p, natural_width_p);
      break;

    case RUT_BOX_LAYOUT_ORIENTATION_VERTICAL:
      get_other_preferred_size (box, for_height, min_width_p, natural_width_p);
      break;
    }
}

static void
rut_box_layout_get_preferred_height (void *sizable,
                                     float for_width,
                                     float *min_height_p,
                                     float *natural_height_p)
{
  RutBoxLayout *box = RUT_BOX_LAYOUT (sizable);

  switch (box->orientation)
    {
    case RUT_BOX_LAYOUT_ORIENTATION_HORIZONTAL:
      get_other_preferred_size (box, for_width, min_height_p, natural_height_p);
      break;

    case RUT_BOX_LAYOUT_ORIENTATION_VERTICAL:
      get_main_preferred_size (box, for_width, min_height_p, natural_height_p);
      break;
    }
}

static RutClosure *
rut_box_layout_add_preferred_size_callback (void *object,
                                            RutSizablePreferredSizeCallback cb,
                                            void *user_data,
                                            RutClosureDestroyCallback destroy)
{
  RutBoxLayout *box = object;

  return rut_closure_list_add (&box->preferred_size_cb_list,
                               cb,
                               user_data,
                               destroy);
}

static void
rut_box_layout_get_size (void *object,
                         float *width,
                         float *height)
{
  RutBoxLayout *box = object;

  *width = box->width;
  *height = box->height;
}

static RutSizableVTable _rut_box_layout_sizable_vtable = {
  rut_box_layout_set_size,
  rut_box_layout_get_size,
  rut_box_layout_get_preferred_width,
  rut_box_layout_get_preferred_height,
  rut_box_layout_add_preferred_size_callback
};

static void
_rut_box_layout_init_type (void)
{
  rut_type_init (&rut_box_layout_type);
  rut_type_add_interface (&rut_box_layout_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutBoxLayout, ref_count),
                          &_rut_box_layout_ref_countable_vtable);
  rut_type_add_interface (&rut_box_layout_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutBoxLayout, graphable),
                          &_rut_box_layout_graphable_vtable);
  rut_type_add_interface (&rut_box_layout_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_box_layout_sizable_vtable);
}

RutBoxLayout *
rut_box_layout_new (RutContext *ctx,
                    RutBoxLayoutOrientation orientation)
{
  RutBoxLayout *box = g_slice_new0 (RutBoxLayout);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_box_layout_init_type ();

      initialized = TRUE;
    }

  box->ref_count = 1;
  box->context = rut_refable_ref (ctx);
  box->orientation = orientation;

  rut_list_init (&box->preferred_size_cb_list);
  rut_list_init (&box->children);

  rut_object_init (&box->_parent, &rut_box_layout_type);

  rut_graphable_init (RUT_OBJECT (box));

  return box;
}

static void
child_preferred_size_cb (RutObject *sizable,
                         void *user_data)
{
  RutBoxLayout *box = user_data;

  preferred_size_changed (box);
  queue_allocation (box);
}

void
rut_box_layout_add (RutBoxLayout *box,
                    RutObject *child_widget)
{
  RutBoxLayoutChild *child = g_slice_new (RutBoxLayoutChild);

  child->widget = rut_refable_ref (child_widget);

  child->transform = rut_transform_new (box->context,
                                        child_widget,
                                        NULL);

  rut_graphable_add_child (box, child->transform);

  child->preferred_size_closure =
    rut_sizable_add_preferred_size_callback (child_widget,
                                             child_preferred_size_cb,
                                             box,
                                             NULL /* destroy */);

  rut_list_insert (box->children.prev, &child->link);

  preferred_size_changed (box);
  queue_allocation (box);
}

void
rut_box_layout_remove (RutBoxLayout *box,
                       RutObject *child_widget)
{
  RutBoxLayoutChild *child;

  rut_list_for_each (child, &box->children, link)
    {
      if (child->widget == child_widget)
        {
          rut_closure_disconnect (child->preferred_size_closure);

          rut_graphable_remove_child (child->widget);
          rut_refable_unref (child->widget);

          rut_graphable_remove_child (child->transform);
          rut_refable_unref (child->transform);

          rut_list_remove (&child->link);
          g_slice_free (RutBoxLayoutChild, child);

          preferred_size_changed (box);
          queue_allocation (box);

          break;
        }
    }
}
