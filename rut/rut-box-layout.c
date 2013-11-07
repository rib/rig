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

#include <config.h>

#include "rut-box-layout.h"
#include "rut-transform.h"
#include "rut-util.h"

enum {
  RUT_BOX_LAYOUT_PROP_PACKING,
  RUT_BOX_LAYOUT_PROP_HOMOGENEOUS,
  RUT_BOX_LAYOUT_PROP_SPACING,
  RUT_BOX_LAYOUT_N_PROPS
};

typedef struct
{
  RutList link;
  RutObject *transform;
  RutObject *widget;
  RutClosure *preferred_size_closure;
  bool expand;
} RutBoxLayoutChild;

struct _RutBoxLayout
{
  RutObjectProps _parent;

  RutContext *ctx;

  RutList preferred_size_cb_list;
  RutList children;
  int n_children;

  RutBoxLayoutPacking packing;
  int spacing;
  bool homogeneous;

  float width, height;

  RutGraphableProps graphable;

  int ref_count;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_BOX_LAYOUT_N_PROPS];
};

static RutPropertySpec _rut_box_layout_prop_specs[] = {

  {
    .name = "packing",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = (void *)rut_box_layout_get_packing,
    .setter.integer_type = (void *)rut_box_layout_set_packing,
    .nick = "Packing",
    .blurb = "The packing direction",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .default_value = { .integer = RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT }
  },

  {
    .name = "homogeneous",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = rut_box_layout_get_homogeneous,
    .setter.boolean_type = rut_box_layout_set_homogeneous,
    .nick = "Homogeneous",
    .blurb = "Pack children with the same size",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .default_value = { .boolean = FALSE }
  },

  {
    .name = "spacing",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = rut_box_layout_get_spacing,
    .setter.integer_type = rut_box_layout_set_spacing,
    .nick = "Spacing",
    .blurb = "The spacing between children",
    .flags = RUT_PROPERTY_FLAG_READWRITE
  },

  { 0 }
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

  rut_shell_remove_pre_paint_callback (box->ctx->shell, box);

  rut_refable_unref (box->ctx);

  rut_graphable_destroy (box);

  g_slice_free (RutBoxLayout, box);
}

static void
allocate_cb (RutObject *graphable,
             void *user_data)
{
  RutBoxLayout *box = graphable;
  RutBoxLayoutChild *child;
  RutBoxLayoutPacking packing = box->packing;
  bool horizontal;
  int n_children = box->n_children;
  int n_expand_children;

  int child_x, child_y, child_width, child_height;
  int child_size;
  RutPreferredSize *sizes;

  int size;
  int extra;
  int n_extra_px_widgets;
  int pos;
  int i;

  if (!n_children)
    return;

  sizes = g_newa (RutPreferredSize, n_children);

  switch (packing)
    {
    case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
    case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
      size = box->width - (n_children - 1) * box->spacing;
      horizontal = TRUE;
      child_y = 0;
      child_height = box->height;
      if (rut_get_text_direction (box->ctx) == RUT_TEXT_DIRECTION_RIGHT_TO_LEFT)
        {
          if (packing == RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT)
            packing = RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT;
          else
            packing = RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT;
        }
      break;
    case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
    case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
      size = box->height - (n_children - 1) * box->spacing;
      horizontal = FALSE;
      child_x = 0;
      child_width = box->width;
      break;
    }

  /* Allocate the children using the calculated max size */
  i = 0;
  n_expand_children = 0;
  rut_list_for_each (child, &box->children, link)
    {
      rut_transform_init_identity (child->transform);

      if (horizontal)
        {
          rut_sizable_get_preferred_width (child->widget,
                                           box->height, /* for_height */
                                           &sizes[i].minimum_size,
                                           &sizes[i].natural_size);
        }
      else
        {
          rut_sizable_get_preferred_height (child->widget,
                                            box->width, /* for_width */
                                            &sizes[i].minimum_size,
                                            &sizes[i].natural_size);
        }

      if (child->expand)
        n_expand_children++;

      size -= sizes[i].minimum_size;
      i++;
    }

  if (box->homogeneous)
    {
      /* If were homogenous we still need to run the above loop to get the
       * minimum sizes for children that are not going to fill
       */
      if (horizontal)
        size = box->width - (n_children - 1) * box->spacing;
      else
        size = box->height - (n_children - 1) * box->spacing;

      extra = size / n_children;
      n_extra_px_widgets = size % n_children;
    }
  else
    {
      /* Bring children up to size first */
      size = rut_util_distribute_natural_allocation (MAX (0, size),
                                                     n_children, sizes);

      /* Calculate space which hasn't distributed yet,
       * and is available for expanding children.
       */
      if (n_expand_children > 0)
	{
	  extra = size / n_expand_children;
	  n_extra_px_widgets = size % n_expand_children;
	}
      else
        {
	  extra = 0;
          n_extra_px_widgets = 0;
        }
    }

  /* Allocate child positions. */

  i = pos = 0;
  rut_list_for_each (child, &box->children, link)
    {
      /* Assign the child's size. */
      if (box->homogeneous)
        {
          child_size = extra;

          if (n_extra_px_widgets > 0)
            {
              child_size++;
              n_extra_px_widgets--;
            }
        }
      else
        {
          child_size = sizes[i].minimum_size;

          if (child->expand)
            {
              child_size += extra;

              if (n_extra_px_widgets > 0)
                {
                  child_size++;
                  n_extra_px_widgets--;
                }
            }
        }

      /* Assign the child's position. */

      child_size = MAX (1, child_size);

      switch (packing)
        {
        case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
          child_x = pos;
          child_width = child_size;
          break;
        case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
          child_x = box->width - pos - child_size;
          child_width = child_size;
          break;
        case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
          child_y = pos;
          child_height = child_size;
          break;
        case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
          child_y = box->height - pos - child_size;
          child_height = child_size;
          break;
        }

      pos += child_size + box->spacing;

      rut_sizable_set_size (child->widget, child_width, child_height);
      rut_transform_init_identity (child->transform);
      rut_transform_translate (child->transform, child_x, child_y, 0.0f);

      i++;
    }
}

static void
queue_allocation (RutBoxLayout *box)
{
  rut_shell_add_pre_paint_callback (box->ctx->shell,
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

      switch (box->packing)
        {
        case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
        case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
          rut_sizable_get_preferred_width (child->widget,
                                           for_size, /* for_height */
                                           min_size_p ?
                                           &min_size :
                                           NULL,
                                           natural_size_p ?
                                           &natural_size :
                                           NULL);
          break;

        case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
        case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
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

      switch (box->packing)
        {
        case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
        case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
          rut_sizable_get_preferred_height (child->widget,
                                            -1, /* for_width */
                                            min_size_p ?
                                            &min_size :
                                            NULL,
                                            natural_size_p ?
                                            &natural_size :
                                            NULL);
          break;

        case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
        case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
          rut_sizable_get_preferred_width (child->widget,
                                           -1, /* for_height */
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
  RutBoxLayout *box = sizable;

  switch (box->packing)
    {
    case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
    case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
      get_main_preferred_size (box, for_height, min_width_p, natural_width_p);
      break;

    case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
    case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
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
  RutBoxLayout *box = sizable;

  switch (box->packing)
    {
    case RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT:
    case RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT:
      get_other_preferred_size (box, for_width, min_height_p, natural_height_p);
      break;

    case RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM:
    case RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP:
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


static void
_rut_box_layout_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
      rut_box_layout_set_size,
      rut_box_layout_get_size,
      rut_box_layout_get_preferred_width,
      rut_box_layout_get_preferred_height,
      rut_box_layout_add_preferred_size_callback
  };

  static RutIntrospectableVTable introspectable_vtable = {
      rut_simple_introspectable_lookup_property,
      rut_simple_introspectable_foreach_property
  };

  RutType *type = &rut_box_layout_type;
#define TYPE RutBoxLayout

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_refable (type, ref_count, _rut_box_layout_free);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &introspectable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (TYPE, introspectable),
                          NULL); /* no implied vtable */

#undef TYPE
}

RutBoxLayout *
rut_box_layout_new (RutContext *ctx,
                    RutBoxLayoutPacking packing)
{
  RutBoxLayout *box = rut_object_alloc0 (RutBoxLayout,
                                         &rut_box_layout_type,
                                         _rut_box_layout_init_type);

  box->ref_count = 1;
  box->ctx = rut_refable_ref (ctx);
  box->packing = packing;

  rut_list_init (&box->preferred_size_cb_list);
  rut_list_init (&box->children);

  rut_graphable_init (RUT_OBJECT (box));

  rut_simple_introspectable_init (box,
                                  _rut_box_layout_prop_specs,
                                  box->properties);

  queue_allocation (box);

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
                    bool expand,
                    RutObject *child_widget)
{
  RutBoxLayoutChild *child = g_slice_new (RutBoxLayoutChild);

  g_return_if_fail (rut_object_get_type (box) == &rut_box_layout_type);

  child->transform = rut_transform_new (box->ctx);
  rut_graphable_add_child (box, child->transform);
  rut_refable_unref (child->transform);

  child->widget = child_widget;
  rut_graphable_add_child (child->transform, child_widget);

  child->expand = expand;

  box->n_children++;

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

  g_return_if_fail (box->n_children > 0);

  rut_list_for_each (child, &box->children, link)
    {
      if (child->widget == child_widget)
        {
          rut_closure_disconnect (child->preferred_size_closure);

          rut_graphable_remove_child (child->widget);
          rut_graphable_remove_child (child->transform);

          rut_list_remove (&child->link);
          g_slice_free (RutBoxLayoutChild, child);

          preferred_size_changed (box);
          queue_allocation (box);

          box->n_children--;

          break;
        }
    }
}

bool
rut_box_layout_get_homogeneous (RutObject *obj)
{
  RutBoxLayout *box = obj;
  return box->homogeneous;
}

void
rut_box_layout_set_homogeneous (RutObject *obj,
                                bool homogeneous)
{
  RutBoxLayout *box = obj;

  if (box->homogeneous == homogeneous)
    return;

  box->homogeneous = homogeneous;

  rut_property_dirty (&box->ctx->property_ctx,
                      &box->properties[RUT_BOX_LAYOUT_PROP_HOMOGENEOUS]);

  queue_allocation (box);
}

int
rut_box_layout_get_spacing (RutObject *obj)
{
  RutBoxLayout *box = obj;

  return box->spacing;
}

void
rut_box_layout_set_spacing (RutObject *obj,
                            int spacing)
{
  RutBoxLayout *box = obj;

  if (box->spacing == spacing)
    return;

  box->spacing = spacing;

  rut_property_dirty (&box->ctx->property_ctx,
                      &box->properties[RUT_BOX_LAYOUT_PROP_SPACING]);

  queue_allocation (box);
}

int
rut_box_layout_get_packing (RutObject *obj)
{
  RutBoxLayout *box = obj;
  return box->packing;
}

void
rut_box_layout_set_packing (RutObject *obj,
                            RutBoxLayoutPacking packing)
{
  RutBoxLayout *box = obj;

  if (box->packing == packing)
    return;

  box->packing = packing;

  rut_property_dirty (&box->ctx->property_ctx,
                      &box->properties[RUT_BOX_LAYOUT_PROP_PACKING]);

  queue_allocation (box);
}
