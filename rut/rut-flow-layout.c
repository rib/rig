/*
 * Rut.
 *
 * Copyright (C) 2013  Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */


#include <config.h>

#include "rut-list.h"
#include "rut-closure.h"
#include "rut-context.h"
#include "rut-interfaces.h"
#include "rut-flow-layout.h"

#include <math.h>

enum
{
  RUT_FLOW_LAYOUT_PROP_PACKING,

  RUT_FLOW_LAYOUT_PROP_X_PADDING,
  RUT_FLOW_LAYOUT_PROP_Y_PADDING,

  RUT_FLOW_LAYOUT_PROP_MIN_CHILD_WIDTH,
  RUT_FLOW_LAYOUT_PROP_MAX_CHILD_WIDTH,
  RUT_FLOW_LAYOUT_PROP_MIN_CHILD_HEIGHT,
  RUT_FLOW_LAYOUT_PROP_MAX_CHILD_HEIGHT,

  RUT_FLOW_LAYOUT_N_PROPS,
};


typedef struct
{
  RutList link;
  RutObject *transform;
  RutObject *widget;
  RutClosure *preferred_size_closure;

  /* re-flowing is done on a line-by-line basis and so this is used
   * during re-flowing to link the child into the current line being
   * handled...
   */
  RutList line_link;

  /* During re-flowing we track the allocation in normalized
   * coordinates here. 'normalized' means that instead of using x, y,
   * width, height, we instead track coordinates that relate to the
   * packing direction of the layout. 'a' corresponds to the axis
   * in-line with the direction of the layout. */
  int a_pos;
  int b_pos;
  int a_size;

  /* We re-flow on a line-by-line basis, and once we get to the end of
   * a line we then iterate all the children of the line and map from
   * the normalized re-flow coordinates (see above) into final
   * coordinates that can later be used for allocation.
   */
  int flow_x;
  int flow_y;
  int flow_width;
  int flow_height;

} RutFlowLayoutChild;

struct _RutFlowLayout
{
  RutObjectProps _parent;

  RutContext *ctx;

  int ref_count;

  float width, height;

  RutGraphableProps graphable;

  RutList preferred_size_cb_list;
  RutList children;
  int n_children;

  RutFlowLayoutPacking packing;

  int x_padding;
  int y_padding;

  int min_child_width;
  int max_child_width;
  int min_child_height;
  int max_child_height;

  int last_flow_line_length;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_FLOW_LAYOUT_N_PROPS];

  unsigned int needs_reflow: 1;
};

static RutPropertySpec _rut_flow_layout_prop_specs[] = {

  /**
   * RutFlowLayout:packing:
   *
   * The packing direction of the #RutFlowLayout. The children
   * of the layout will be layed out following this direction.
   *
   * This property also controls the overflowing directions
   */
  {
    .name = "packing",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = (void *)rut_flow_layout_get_packing,
    .setter.integer_type = (void *)rut_flow_layout_set_packing,
    .nick = "Packing",
    .blurb = "The flow packing direction",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .default_value = { .integer = RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT }
  },

  /**
   * RutFlowLayout:x-padding:
   *
   * The x-axis padding between children, in pixels.
   */
  {
    .name = "x-padding",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = (void *)rut_flow_layout_get_x_padding,
    .setter.integer_type = (void *)rut_flow_layout_set_x_padding,
    .nick = "X Axis Padding",
    .blurb = "The x-axis padding between children",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
  },

  /**
   * RutFlowLayout:y-padding:
   *
   * The y-axis padding between children, in pixels.
   */
  {
    .name = "y-padding",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = (void *)rut_flow_layout_get_y_padding,
    .setter.integer_type = (void *)rut_flow_layout_set_y_padding,
    .nick = "Y Axis Padding",
    .blurb = "The y-axis padding between children",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
  },

  /**
   * RutFlowLayout:min-box-width:
   *
   * Minimum width for each box in the layout, in pixels
   */
  {
    .name = "min_child_width",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = (void *)rut_flow_layout_get_min_child_width,
    .setter.integer_type = (void *)rut_flow_layout_set_min_child_width,
    .nick = "Minimum Child Width",
    .blurb = "The minimum width for children",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
  },

  /**
   * RutFlowLayout:max-child-width:
   *
   * Maximum width for each child in the layout, in pixels. If
   * set to -1 that means there is no specific limit
   */
  {
    .name = "max_child_width",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = (void *)rut_flow_layout_get_max_child_width,
    .setter.integer_type = (void *)rut_flow_layout_set_max_child_width,
    .nick = "Maximum Child Width",
    .blurb = "The maximum width for children",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
  },

  /**
   * RutFlowLayout:min-child-height:
   *
   * Minimum height for each child in the layout, in pixels
   */
  {
    .name = "min_child_height",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = (void *)rut_flow_layout_get_min_child_height,
    .setter.integer_type = (void *)rut_flow_layout_set_min_child_height,
    .nick = "Minimum Child Height",
    .blurb = "The minimum height for children",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
  },

  /**
   * RutFlowLayout:max-child-height:
   *
   * Maximum height for each child in the layout, in pixels. If
   * set to -1 that means there is no specific limit
   */
  {
    .name = "max_child_height",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .getter.integer_type = (void *)rut_flow_layout_get_max_child_height,
    .setter.integer_type = (void *)rut_flow_layout_set_max_child_height,
    .nick = "Maximum Child Height",
    .blurb = "The maximum height for children",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
  },

  { 0 }
};

static void
rut_flow_layout_remove_child (RutFlowLayout *flow,
                              RutFlowLayoutChild *child)
{
  rut_closure_disconnect (child->preferred_size_closure);

  rut_graphable_remove_child (child->widget);
  rut_refable_unref (child->widget);

  rut_graphable_remove_child (child->transform);
  rut_refable_unref (child->transform);

  rut_list_remove (&child->link);
  g_slice_free (RutFlowLayoutChild, child);

  flow->n_children--;
}

static void
_rut_flow_layout_free (void *object)
{
  RutFlowLayout *flow = object;

  rut_closure_list_disconnect_all (&flow->preferred_size_cb_list);

  while (!rut_list_empty (&flow->children))
    {
      RutFlowLayoutChild *child =
        rut_container_of (flow->children.next, child, link);

      rut_flow_layout_remove_child (flow, child);
    }

  rut_shell_remove_pre_paint_callback (flow->ctx->shell, flow);

  rut_refable_unref (flow->ctx);

  rut_simple_introspectable_destroy (flow);

  g_slice_free (RutFlowLayout, flow);
}

typedef void (* PreferredSizeCallback) (void *sizable,
                                        float for_b,
                                        float *min_size_p,
                                        float *natural_size_p);

typedef void (* FlowLineCallback) (RutList *line_list,
                                   float line_length,
                                   float height);

static void
flow_horizontal_line_ltr (RutList *line_list,
                          float line_length,
                          float height)
{
  RutFlowLayoutChild *tmp, *child;

  rut_list_for_each_safe (child, tmp, line_list, line_link)
    {
      child->flow_x = child->a_pos;
      child->flow_y = child->b_pos;
      child->flow_width = child->a_size;
      child->flow_height = height;

      rut_list_remove (&child->line_link);
    }
}

static void
flow_horizontal_line_rtl (RutList *line_list,
                          float line_length,
                          float height)
{
  RutFlowLayoutChild *tmp, *child;

  rut_list_for_each_safe (child, tmp, line_list, line_link)
    {
      child->flow_x = line_length - child->a_size - child->a_pos;
      child->flow_y = child->b_pos;
      child->flow_width = child->a_size;
      child->flow_height = height;

      rut_list_remove (&child->line_link);
    }
}

static void
flow_vertical_line_ttb (RutList *line_list,
                        float line_length,
                        float width)
{
  RutFlowLayoutChild *tmp, *child;

  rut_list_for_each_safe (child, tmp, line_list, line_link)
    {
      child->flow_x = child->b_pos;
      child->flow_y = child->a_pos;
      child->flow_width = width;
      child->flow_height = child->a_size;

      rut_list_remove (&child->line_link);
    }
}

static void
flow_vertical_line_btt (RutList *line_list,
                        float line_length,
                        float width)
{
  RutFlowLayoutChild *tmp, *child;

  rut_list_for_each_safe (child, tmp, line_list, line_link)
    {
      child->flow_x = child->b_pos;
      child->flow_y = line_length - child->a_size - child->a_pos;
      child->flow_width = width;
      child->flow_height = child->a_size;

      rut_list_remove (&child->line_link);
    }
}

typedef struct _ReFlowState
{
  float min_child_a_size;
  float max_child_a_size;
  PreferredSizeCallback get_a_size;
  float a_pad;
  float min_child_b_size;
  float max_child_b_size;
  PreferredSizeCallback get_b_size;
  float b_pad;
  float line_length;
  FlowLineCallback flow_line;
} ReFlowState;

static void
init_reflow_state (ReFlowState *state,
                   RutFlowLayout *flow,
                   float for_width,
                   float for_height)
{
  switch (flow->packing)
    {
    case RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT:
    case RUT_FLOW_LAYOUT_PACKING_RIGHT_TO_LEFT:

      state->max_child_a_size = flow->max_child_width;
      state->max_child_b_size = flow->max_child_height;

      /* NB: max_child_width/height and for_width/height may == -1 */

      if (for_width >= 0)
        {
          if (state->max_child_a_size < 0)
            state->max_child_a_size = for_width;
          else
            state->max_child_a_size = MIN (state->max_child_a_size, for_width);
        }

      if (for_height >= 0)
        {
          if (state->max_child_b_size < 0)
            state->max_child_b_size = for_height;
          else
            state->max_child_b_size = MIN (state->max_child_b_size, for_height);
        }

      state->min_child_a_size = flow->min_child_width;
      state->min_child_b_size = flow->min_child_height;

      state->get_a_size = rut_sizable_get_preferred_width;
      state->get_b_size = rut_sizable_get_preferred_height;

      state->a_pad = flow->x_padding;
      state->b_pad = flow->y_padding;

      state->line_length = for_width;

      break;

    case RUT_FLOW_LAYOUT_PACKING_TOP_TO_BOTTOM:
    case RUT_FLOW_LAYOUT_PACKING_BOTTOM_TO_TOP:

      state->max_child_a_size = flow->max_child_height;
      state->max_child_b_size = flow->max_child_width;

      /* NB: max_child_width/height and for_width/height may == -1 */

      if (for_width >= 0)
        {
          if (state->max_child_b_size < 0)
            state->max_child_b_size = for_width;
          else
            state->max_child_b_size = MIN (state->max_child_b_size, for_width);
        }

      if (for_height >= 0)
        {
          if (state->max_child_a_size < 0)
            state->max_child_a_size = for_height;
          else
            state->max_child_a_size = MIN (state->max_child_b_size, for_height);
        }

      state->min_child_a_size = flow->min_child_height;
      state->min_child_b_size = flow->min_child_width;

      state->get_a_size = rut_sizable_get_preferred_height;
      state->get_b_size = rut_sizable_get_preferred_width;

      state->a_pad = flow->y_padding;
      state->b_pad = flow->x_padding;

      state->line_length = for_height;

      break;
    }

  switch (flow->packing)
    {
    case RUT_FLOW_LAYOUT_PACKING_LEFT_TO_RIGHT:
      state->flow_line = flow_horizontal_line_ltr;
      break;
    case RUT_FLOW_LAYOUT_PACKING_RIGHT_TO_LEFT:
      state->flow_line = flow_horizontal_line_rtl;
      break;
    case RUT_FLOW_LAYOUT_PACKING_TOP_TO_BOTTOM:
      state->flow_line = flow_vertical_line_ttb;
      break;
    case RUT_FLOW_LAYOUT_PACKING_BOTTOM_TO_TOP:
      state->flow_line = flow_vertical_line_btt;
      break;
    }
}

static void
reflow (RutFlowLayout *flow,
        ReFlowState *reflow_state,
        float *length_p)
{
  ReFlowState state = *reflow_state;
  RutFlowLayoutChild *child;
  float a_pos = 0;
  float b_pos = 0;
  RutList line_list;
  float line_max_b_size = 0;

  rut_list_init (&line_list);

  rut_list_for_each (child, &flow->children, link)
    {
      float a_size, b_size;

      /* First we want to know how long the child would prefer to be
       * along the a axis...
       */
      state.get_a_size (child->widget, state.max_child_b_size, NULL, &a_size);

      /* Apply the min/max_child_a_size constraints... */
      a_size = MAX (a_size, state.min_child_a_size);
      if (state.max_child_a_size >= 0)
        a_size = MIN (a_size, state.max_child_a_size);

      /* Check if we need to wrap because we've run out of space for
       * the current line... */

      if (state.line_length >= 0 &&
          !rut_list_empty (&line_list) &&
          a_size > (state.line_length - a_pos))
        {
          state.flow_line (&line_list, state.line_length, line_max_b_size);

          a_pos = 0;
          b_pos += line_max_b_size + state.b_pad;

          line_max_b_size = 0;
        }

      /* Now find out what size the child would like to be along the b
       * axis, given the constrained a_size we have calculated...
       */
      state.get_b_size (child->widget, a_size, NULL, &b_size);

      /* Apply the min/max_child_b_size constraints... */
      b_size = MAX (b_size, state.min_child_b_size);
      if (state.max_child_b_size >= 0)
        b_size = MIN (b_size, state.max_child_b_size);

      child->a_pos = a_pos;
      child->b_pos = b_pos;
      child->a_size = a_size;

      rut_list_insert (&line_list, &child->line_link);

      a_pos += a_size + state.a_pad;
      line_max_b_size = MAX (line_max_b_size, b_size);
    }

  if (!rut_list_empty (&line_list))
    {
      float line_length =
        state.line_length >= 0 ? state.line_length : a_pos - state.a_pad;

      state.flow_line (&line_list, line_length, line_max_b_size);
      *length_p = b_pos + line_max_b_size;
    }
  else
    *length_p = 0;

  flow->needs_reflow = FALSE;
  flow->last_flow_line_length = state.line_length;
}

static void
flush_allocations (RutFlowLayout *flow)
{
  RutFlowLayoutChild *child;

  rut_list_for_each (child, &flow->children, link)
    {
      rut_transform_init_identity (child->transform);
      rut_transform_translate (child->transform,
                               child->flow_x,
                               child->flow_y,
                               0);
      rut_sizable_set_size (child->widget,
                            child->flow_width,
                            child->flow_height);
    }
}

static void
allocate_cb (RutObject *graphable,
             void *user_data)
{
  RutFlowLayout *flow = graphable;
  ReFlowState state;
  float length_ignore;


  if (flow->n_children == 0)
    return;

  init_reflow_state (&state, flow, flow->width, flow->height);

  /* Since it's quite likely we will be allocated according to a
   * previous get_preferred_width/height request which will have had
   * to reflow the children we can sometimes avoid needing another
   * reflow here... */
  if (flow->needs_reflow || state.line_length != flow->last_flow_line_length)
    reflow (flow, &state, &length_ignore);

  flush_allocations (flow);
}

static void
queue_allocation (RutFlowLayout *flow)
{
  rut_shell_add_pre_paint_callback (flow->ctx->shell,
                                    flow,
                                    allocate_cb,
                                    NULL /* user_data */);
}

static void
rut_flow_layout_set_size (void *object,
                          float width,
                          float height)
{
  RutFlowLayout *flow = object;

  if (width == flow->width && height == flow->height)
    return;

  flow->width = width;
  flow->height = height;

  queue_allocation (flow);
}

static void
rut_flow_layout_get_size (void *object,
                          float *width,
                          float *height)
{
  RutFlowLayout *flow = object;

  *width = flow->width;
  *height = flow->height;
}

static void
rut_flow_layout_get_preferred_height (void *sizable,
                                      float for_width,
                                      float *min_height_p,
                                      float *natural_height_p)
{
  RutFlowLayout *flow = sizable;
  ReFlowState state;
  float length;

  init_reflow_state (&state, flow, for_width, -1);
  reflow (flow, &state, &length);

  length = floorf (length + 0.5f);

  if (min_height_p)
    *min_height_p = length;
  if (natural_height_p)
    *natural_height_p = length;
}

static void
rut_flow_layout_get_preferred_width (void *sizable,
                                     float for_height,
                                     float *min_width_p,
                                     float *natural_width_p)
{
  RutFlowLayout *flow = sizable;
  ReFlowState state;
  float length;

  init_reflow_state (&state, flow, -1, for_height);
  reflow (flow, &state, &length);

  if (min_width_p)
    *min_width_p = length;
  if (natural_width_p)
    *natural_width_p = length;
}

static RutClosure *
rut_flow_layout_add_preferred_size_callback (void *object,
                                             RutSizablePreferredSizeCallback cb,
                                             void *user_data,
                                             RutClosureDestroyCallback destroy)
{
  RutFlowLayout *flow = object;

  return rut_closure_list_add (&flow->preferred_size_cb_list,
                               cb,
                               user_data,
                               destroy);
}

RutType rut_flow_layout_type;

static void
_rut_flow_layout_init_type (void)
{
  static RutRefCountableVTable ref_countable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_flow_layout_free
  };
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };
  static RutSizableVTable sizable_vtable = {
      rut_flow_layout_set_size,
      rut_flow_layout_get_size,
      rut_flow_layout_get_preferred_width,
      rut_flow_layout_get_preferred_height,
      rut_flow_layout_add_preferred_size_callback
  };
  static RutIntrospectableVTable introspectable_vtable = {
      rut_simple_introspectable_lookup_property,
      rut_simple_introspectable_foreach_property
  };

  RutType *type = &rut_flow_layout_type;
#define TYPE RutFlowLayout

  rut_type_init (type, G_STRINGIFY (TYPE));

  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &ref_countable_vtable);
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

RutFlowLayout *
rut_flow_layout_new (RutContext *ctx,
                     RutFlowLayoutPacking packing)
{
  RutFlowLayout *flow = g_slice_new0 (RutFlowLayout);
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_flow_layout_init_type ();
      initialized = TRUE;
    }

  flow->ref_count = 1;

  rut_object_init (&flow->_parent, &rut_flow_layout_type);

  rut_list_init (&flow->preferred_size_cb_list);
  rut_list_init (&flow->children);

  rut_graphable_init (flow);

  rut_simple_introspectable_init (flow,
                                  _rut_flow_layout_prop_specs,
                                  flow->properties);

  flow->ctx = rut_refable_ref (ctx);
  flow->packing = packing;

  flow->x_padding = 0;
  flow->y_padding = 0;

  flow->min_child_width = flow->min_child_height = 0;
  flow->max_child_width = flow->max_child_height = -1;

  flow->needs_reflow = TRUE;
  queue_allocation (flow);

  return flow;
}

static void
preferred_size_changed (RutFlowLayout *flow)
{
  flow->needs_reflow = TRUE;

  rut_closure_list_invoke (&flow->preferred_size_cb_list,
                           RutSizablePreferredSizeCallback,
                           flow);
}

static void
child_preferred_size_cb (RutObject *sizable,
                         void *user_data)
{
  RutFlowLayout *flow = user_data;

  preferred_size_changed (flow);
  queue_allocation (flow);
}

void
rut_flow_layout_add (RutFlowLayout *flow,
                     RutObject *child_widget)
{
  RutFlowLayoutChild *child = g_slice_new (RutFlowLayoutChild);

  child->widget = rut_refable_ref (child_widget);

  child->transform = rut_transform_new (flow->ctx);
  rut_graphable_add_child (child->transform, child_widget);

  rut_graphable_add_child (flow, child->transform);
  flow->n_children++;

  child->preferred_size_closure =
    rut_sizable_add_preferred_size_callback (child_widget,
                                             child_preferred_size_cb,
                                             flow,
                                             NULL /* destroy */);

  rut_list_insert (flow->children.prev, &child->link);

  preferred_size_changed (flow);
  queue_allocation (flow);
}

void
rut_flow_layout_remove (RutFlowLayout *flow,
                        RutObject *child_widget)
{
  RutFlowLayoutChild *child;

  g_return_if_fail (flow->n_children > 0);

  rut_list_for_each (child, &flow->children, link)
    {
      if (child->widget == child_widget)
        {
          rut_flow_layout_remove_child (flow, child);

          preferred_size_changed (flow);
          queue_allocation (flow);
          break;
        }
    }
}

void
rut_flow_layout_set_packing (RutFlowLayout *flow,
                             RutFlowLayoutPacking packing)
{
  if (flow->packing == packing)
    return;

  flow->packing = packing;

  queue_allocation (flow);
  preferred_size_changed (flow);

  rut_property_dirty (&flow->ctx->property_ctx,
                      &flow->properties[RUT_FLOW_LAYOUT_PROP_PACKING]);
}

RutFlowLayoutPacking
rut_flow_layout_get_packing (RutFlowLayout *flow)
{
  return flow->packing;
}

void
rut_flow_layout_set_x_padding (RutFlowLayout *flow,
                               int padding)
{
  if (flow->x_padding == padding)
    return;

  flow->x_padding = padding;

  queue_allocation (flow);
  preferred_size_changed (flow);

  rut_property_dirty (&flow->ctx->property_ctx,
                      &flow->properties[RUT_FLOW_LAYOUT_PROP_X_PADDING]);
}

int
rut_flow_layout_get_x_padding (RutFlowLayout *flow)
{
  return flow->x_padding;
}

void
rut_flow_layout_set_y_padding (RutFlowLayout *flow,
                               int padding)
{
  if (flow->y_padding == padding)
    return;

  flow->y_padding = padding;

  queue_allocation (flow);
  preferred_size_changed (flow);

  rut_property_dirty (&flow->ctx->property_ctx,
                      &flow->properties[RUT_FLOW_LAYOUT_PROP_Y_PADDING]);
}

int
rut_flow_layout_get_y_padding (RutFlowLayout *flow)
{
  return flow->y_padding;
}

void
rut_flow_layout_set_min_child_width (RutFlowLayout *flow,
                                     int min_width)
{
  if (flow->min_child_width == min_width)
    return;

  flow->min_child_width = min_width;

  queue_allocation (flow);
  preferred_size_changed (flow);

  rut_property_dirty (&flow->ctx->property_ctx,
                      &flow->properties[RUT_FLOW_LAYOUT_PROP_MIN_CHILD_WIDTH]);
}

int
rut_flow_layout_get_min_child_width (RutFlowLayout *flow)
{
  return flow->min_child_width;
}

void
rut_flow_layout_set_max_child_width (RutFlowLayout *flow,
                                     int max_width)
{
  if (flow->max_child_width == max_width)
    return;

  flow->max_child_width = max_width;

  queue_allocation (flow);
  preferred_size_changed (flow);

  rut_property_dirty (&flow->ctx->property_ctx,
                      &flow->properties[RUT_FLOW_LAYOUT_PROP_MAX_CHILD_WIDTH]);
}

int
rut_flow_layout_get_max_child_width (RutFlowLayout *flow)
{
  return flow->max_child_width;
}

void
rut_flow_layout_set_min_child_height (RutFlowLayout *flow,
                                      int min_height)
{
  if (flow->min_child_height == min_height)
    return;

  flow->min_child_height = min_height;

  queue_allocation (flow);
  preferred_size_changed (flow);

  rut_property_dirty (&flow->ctx->property_ctx,
                      &flow->properties[RUT_FLOW_LAYOUT_PROP_MIN_CHILD_HEIGHT]);
}

int
rut_flow_layout_get_min_child_height (RutFlowLayout *flow)
{
  return flow->min_child_height;
}

void
rut_flow_layout_set_max_child_height (RutFlowLayout *flow,
                                      int max_height)
{
  if (flow->max_child_height == max_height)
    return;

  flow->max_child_height = max_height;

  queue_allocation (flow);
  preferred_size_changed (flow);

  rut_property_dirty (&flow->ctx->property_ctx,
                      &flow->properties[RUT_FLOW_LAYOUT_PROP_MAX_CHILD_HEIGHT]);
}

int
rut_flow_layout_get_max_child_height (RutFlowLayout *flow)
{
  return flow->max_child_height;
}
