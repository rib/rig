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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rut-context.h"
#include "rut-interfaces.h"
#include "rut-ui-viewport.h"
#include "rut-scroll-bar.h"

enum {
  RUT_UI_VIEWPORT_PROP_WIDTH,
  RUT_UI_VIEWPORT_PROP_HEIGHT,
  RUT_UI_VIEWPORT_PROP_DOC_WIDTH,
  RUT_UI_VIEWPORT_PROP_DOC_HEIGHT,
  RUT_UI_VIEWPORT_PROP_DOC_X,
  RUT_UI_VIEWPORT_PROP_DOC_Y,
  RUT_UI_VIEWPORT_PROP_SYNC_WIDGET,
  RUT_UI_VIEWPORT_PROP_X_PANNABLE,
  RUT_UI_VIEWPORT_PROP_Y_PANNABLE,
  RUT_UI_VIEWPORT_PROP_X_EXPAND,
  RUT_UI_VIEWPORT_PROP_Y_EXPAND,
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
  float doc_width;
  float doc_height;
  float doc_scale_x;
  float doc_scale_y;

  RutObject *sync_widget;
  RutClosure *sync_widget_preferred_size_closure;

  CoglBool x_pannable;
  CoglBool y_pannable;
  CoglBool x_expand;
  CoglBool y_expand;

  RutTransform *scroll_bar_x_transform;
  RutScrollBar *scroll_bar_x;
  CoglBool scroll_bar_x_visible;
  RutTransform *scroll_bar_y_transform;
  RutScrollBar *scroll_bar_y;
  CoglBool scroll_bar_y_visible;

  RutTransform *doc_transform;

  float grab_x;
  float grab_y;
  float grab_doc_x;
  float grab_doc_y;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_UI_VIEWPORT_N_PROPS];

  RutInputableProps inputable;
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
    .name = "doc-width",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutUIViewport, doc_width),
    .setter = rut_ui_viewport_set_doc_width
  },
  {
    .name = "doc-height",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutUIViewport, doc_height),
    .setter = rut_ui_viewport_set_doc_height
  },
  {
    .name = "doc-x",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutUIViewport, doc_x),
    .setter = rut_ui_viewport_set_doc_x
  },
  {
    .name = "doc-y",
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutUIViewport, doc_y),
    .setter = rut_ui_viewport_set_doc_y
  },
  {
    .name = "sync-widget",
    .type = RUT_PROPERTY_TYPE_OBJECT,
    .data_offset = offsetof (RutUIViewport, sync_widget),
    .setter = rut_ui_viewport_set_sync_widget
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
  {
    .name = "x-expand",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .data_offset = offsetof (RutUIViewport, x_expand),
    .setter = rut_ui_viewport_set_x_expand
  },
  {
    .name = "y-expand",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .data_offset = offsetof (RutUIViewport, y_expand),
    .setter = rut_ui_viewport_set_y_expand
  },

  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_ui_viewport_free (void *object)
{
  RutUIViewport *ui_viewport = object;

  rut_ui_viewport_set_sync_widget (ui_viewport, NULL);

  rut_refable_simple_unref (ui_viewport->inputable.input_region);

  rut_simple_introspectable_destroy (ui_viewport);
  rut_graphable_destroy (ui_viewport);

  g_slice_free (RutUIViewport, object);
}

static RutRefCountableVTable _rut_ui_viewport_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
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
  NULL,
  NULL /* add_preferred_size_callback */
};

static RutIntrospectableVTable _rut_ui_viewport_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

RutType rut_ui_viewport_type;

void
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
  rut_type_add_interface (&rut_ui_viewport_type,
                          RUT_INTERFACE_ID_INPUTABLE,
                          offsetof (RutUIViewport, inputable),
                          NULL); /* no implied vtable */
}

static void
_rut_ui_viewport_update_doc_matrix (RutUIViewport *ui_viewport)
{
  rut_transform_init_identity (ui_viewport->doc_transform);
  rut_transform_translate (ui_viewport->doc_transform,
                           -ui_viewport->doc_x,
                           -ui_viewport->doc_y,
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
_rut_ui_viewport_input_cb (RutInputEvent *event,
                           void *user_data)
{
  RutUIViewport *ui_viewport = user_data;

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
  else if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY &&
           rut_key_event_get_action (event) == RUT_KEY_EVENT_ACTION_DOWN)
    {
      switch (rut_key_event_get_keysym (event))
        {
        case RUT_KEY_Page_Up:
          if (ui_viewport->y_pannable)
            {
              rut_ui_viewport_set_doc_y (ui_viewport,
                                         ui_viewport->doc_y - ui_viewport->height);
              rut_shell_queue_redraw (ui_viewport->ctx->shell);
              g_print ("Page Up %f %f\n", ui_viewport->height, ui_viewport->doc_y);
            }
          break;
        case RUT_KEY_Page_Down:
          if (ui_viewport->y_pannable)
            {
              rut_ui_viewport_set_doc_y (ui_viewport,
                                         ui_viewport->doc_y + ui_viewport->height);
              rut_shell_queue_redraw (ui_viewport->ctx->shell);
              g_print ("Page Down %f %f\n", ui_viewport->height, ui_viewport->doc_y);
            }
          break;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_ui_viewport_input_region_cb (RutInputRegion *region,
                                  RutInputEvent *event,
                                  void *user_data)
{
  g_print ("viewport input\n");
  return _rut_ui_viewport_input_cb (event, user_data);
}

static void
pre_paint_cb (RutObject *graphable,
              void *user_data)
{
  RutUIViewport *ui_viewport = RUT_UI_VIEWPORT (graphable);

  if (ui_viewport->sync_widget)
    {
      float width, height;

      rut_sizable_get_preferred_width (ui_viewport->sync_widget,
                                       -1, /* for_height */
                                       NULL, /* min_width */
                                       &width);

      rut_sizable_get_preferred_height (ui_viewport->sync_widget,
                                        width, /* for_width */
                                        NULL, /* min_height */
                                        &height);

      if (ui_viewport->x_expand && width < ui_viewport->width)
        width = ui_viewport->width;
      if (ui_viewport->y_expand && height < ui_viewport->height)
        height = ui_viewport->height;

      rut_sizable_set_size (ui_viewport->sync_widget, width, height);
      rut_ui_viewport_set_doc_width (ui_viewport, width);
      rut_ui_viewport_set_doc_height (ui_viewport, height);
    }
}

static void
queue_allocation (RutUIViewport *ui_viewport)
{
  rut_shell_add_pre_paint_callback (ui_viewport->ctx->shell,
                                    ui_viewport,
                                    pre_paint_cb,
                                    NULL /* user_data */);
}

static void
maybe_clamp_position (RutUIViewport *ui_viewport)
{
   if (ui_viewport->doc_width > 0)
    {
      float doc_view_x = ui_viewport->width / ui_viewport->doc_scale_x;
      if (ui_viewport->doc_x < 0)
        ui_viewport->doc_x = 0;
      else if ((ui_viewport->doc_x + doc_view_x) > ui_viewport->doc_width)
        ui_viewport->doc_x = ui_viewport->doc_width - doc_view_x;
    }

  if (ui_viewport->doc_height > 0)
    {
      float doc_view_y = ui_viewport->height / ui_viewport->doc_scale_y;
      if (ui_viewport->doc_y < 0)
        ui_viewport->doc_y = 0;
      else if ((ui_viewport->doc_y + doc_view_y) > ui_viewport->doc_height)
        ui_viewport->doc_y = ui_viewport->doc_height - doc_view_y;
    }
}

static void
update_scroll_bars (RutUIViewport *ui_viewport)
{
  float doc_view_x = ui_viewport->width * (1.0 / ui_viewport->doc_scale_x);
  float doc_view_y = ui_viewport->height * (1.0 / ui_viewport->doc_scale_y);

  if (ui_viewport->doc_width > 0)
    {
      if (ui_viewport->doc_x < 0)
        rut_ui_viewport_set_doc_x (ui_viewport, 0);
      else if ((ui_viewport->doc_x + doc_view_x) > ui_viewport->doc_width)
        rut_ui_viewport_set_doc_x (ui_viewport,
                                   ui_viewport->doc_x + doc_view_x);

      rut_graphable_add_child (ui_viewport, ui_viewport->scroll_bar_x);
    }
  else
    rut_graphable_remove_child (ui_viewport->scroll_bar_x);

  if (ui_viewport->doc_height > 0)
    {
      if (ui_viewport->doc_y < 0)
        rut_ui_viewport_set_doc_y (ui_viewport, 0);
      else if ((ui_viewport->doc_y + doc_view_y) > ui_viewport->doc_height)
        rut_ui_viewport_set_doc_y (ui_viewport,
                                   ui_viewport->doc_y + doc_view_y);

      rut_graphable_add_child (ui_viewport, ui_viewport->scroll_bar_y_transform);
    }
  else
    rut_graphable_remove_child (ui_viewport->scroll_bar_y_transform);
}

static void
position_scroll_bars (RutUIViewport *ui_viewport)
{
  CoglBool need_scroll_bar_x, need_scroll_bar_y;

  rut_transform_init_identity (ui_viewport->scroll_bar_x_transform);
  rut_transform_init_identity (ui_viewport->scroll_bar_y_transform);

  rut_transform_translate (ui_viewport->scroll_bar_x_transform,
                           0,
                           ui_viewport->height -
                           rut_scroll_bar_get_thickness (ui_viewport->scroll_bar_x),
                           0);

  rut_transform_translate (ui_viewport->scroll_bar_y_transform,
                           ui_viewport->width -
                           rut_scroll_bar_get_thickness (ui_viewport->scroll_bar_y),
                           0,
                           0);

  if (ui_viewport->doc_width)
    {
      if (ui_viewport->width >=
          (ui_viewport->doc_width * ui_viewport->doc_scale_x))
        need_scroll_bar_x = FALSE;
      else
        need_scroll_bar_x = TRUE;
    }
  else
    need_scroll_bar_x = FALSE;

  if (ui_viewport->scroll_bar_x_visible != need_scroll_bar_x)
    {
      if (need_scroll_bar_x)
        rut_graphable_add_child (ui_viewport,
                                 ui_viewport->scroll_bar_x_transform);
      else
        rut_graphable_remove_child (ui_viewport->scroll_bar_x_transform);
    }

  if (ui_viewport->doc_height)
    {
      if (ui_viewport->height >=
          (ui_viewport->doc_height * ui_viewport->doc_scale_y))
        need_scroll_bar_y = FALSE;
      else
        need_scroll_bar_y = TRUE;
    }
  else
    need_scroll_bar_y = FALSE;

  if (ui_viewport->scroll_bar_y_visible != need_scroll_bar_y)
    {
      if (need_scroll_bar_y)
        rut_graphable_add_child (ui_viewport,
                                 ui_viewport->scroll_bar_y_transform);
      else
        rut_graphable_remove_child (ui_viewport->scroll_bar_y_transform);
    }

}

static void
update_scroll_bar_x_cb (RutProperty *target_property,
                        RutProperty *source_property,
                        void *user_data)
{
  RutUIViewport *ui_viewport = user_data;

  /* Update the scrollbar virtual_length, viewport_length and offset */

  rut_scroll_bar_set_virtual_length (ui_viewport->scroll_bar_x,
                                     ui_viewport->doc_width * ui_viewport->doc_scale_x);

  rut_scroll_bar_set_virtual_viewport (ui_viewport->scroll_bar_x,
                                       ui_viewport->width / ui_viewport->doc_scale_x);

  rut_scroll_bar_set_virtual_offset (ui_viewport->scroll_bar_x,
                                     ui_viewport->doc_x * ui_viewport->doc_scale_x);
}

static void
update_scroll_bar_y_cb (RutProperty *target_property,
                        RutProperty *source_property,
                        void *user_data)
{
  RutUIViewport *ui_viewport = user_data;

  /* Update the scrollbar virtual_length, viewport_length and offset */

  rut_scroll_bar_set_virtual_length (ui_viewport->scroll_bar_y,
                                     ui_viewport->doc_height * ui_viewport->doc_scale_y);

  rut_scroll_bar_set_virtual_viewport (ui_viewport->scroll_bar_y,
                                       ui_viewport->height / ui_viewport->doc_scale_y);

  rut_scroll_bar_set_virtual_offset (ui_viewport->scroll_bar_y,
                                     ui_viewport->doc_y * ui_viewport->doc_scale_y);
}

static void
update_doc_x_cb (RutProperty *target_property,
                 RutProperty *source_property,
                 void *user_data)
{
  RutUIViewport *ui_viewport = user_data;
  RutScrollBar *scroll_bar = ui_viewport->scroll_bar_x;

  rut_ui_viewport_set_doc_x (ui_viewport,
                             rut_scroll_bar_get_virtual_offset (scroll_bar) /
                             ui_viewport->doc_scale_x);
}

static void
update_doc_y_cb (RutProperty *target_property,
                 RutProperty *source_property,
                 void *user_data)
{
  RutUIViewport *ui_viewport = user_data;
  RutScrollBar *scroll_bar = ui_viewport->scroll_bar_y;

  rut_ui_viewport_set_doc_y (ui_viewport,
                             rut_scroll_bar_get_virtual_offset (scroll_bar) /
                             ui_viewport->doc_scale_y);
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
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_init ();
      _rut_ui_viewport_init_type ();

      initialized = TRUE;
    }

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
  ui_viewport->doc_width = 0;
  ui_viewport->doc_height = 0;
  ui_viewport->doc_scale_x = 1;
  ui_viewport->doc_scale_y = 1;

  ui_viewport->x_pannable = TRUE;
  ui_viewport->y_pannable = TRUE;

  ui_viewport->scroll_bar_x_transform =
    rut_transform_new (ctx,
                       (ui_viewport->scroll_bar_x =
                        rut_scroll_bar_new (ctx,
                                            RUT_AXIS_X,
                                            width, /* len */
                                            width * 2, /* virtual len */
                                            width)), /* viewport len */
                       NULL);

  ui_viewport->scroll_bar_y_transform =
    rut_transform_new (ctx,
                       (ui_viewport->scroll_bar_y =
                        rut_scroll_bar_new (ctx,
                                            RUT_AXIS_Y,
                                            height, /* len */
                                            height * 2, /* virtual len */
                                            height)), /* viewport len */
                       NULL);

  position_scroll_bars (ui_viewport);

  rut_property_set_binding_by_name (ui_viewport->scroll_bar_x,
                                    "virtual_length",
                                    update_scroll_bar_x_cb,
                                    ui_viewport,
                                    &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_WIDTH],
                                    NULL);
  rut_property_set_binding_by_name (ui_viewport->scroll_bar_x,
                                    "virtual_offset",
                                    update_scroll_bar_x_cb,
                                    ui_viewport,
                                    &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_X],
                                    NULL);
  rut_property_set_binding_by_name (ui_viewport->scroll_bar_x,
                                    "virtual_viewport",
                                    update_scroll_bar_x_cb,
                                    ui_viewport,
                                    &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_WIDTH],
                                    NULL);

  rut_property_set_binding_by_name (ui_viewport->scroll_bar_y,
                                    "virtual_length",
                                    update_scroll_bar_y_cb,
                                    ui_viewport,
                                    &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_HEIGHT],
                                    NULL);
  rut_property_set_binding_by_name (ui_viewport->scroll_bar_y,
                                    "virtual_offset",
                                    update_scroll_bar_y_cb,
                                    ui_viewport,
                                    &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_Y],
                                    NULL);
  rut_property_set_binding_by_name (ui_viewport->scroll_bar_y,
                                    "virtual_viewport",
                                    update_scroll_bar_y_cb,
                                    ui_viewport,
                                    &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_HEIGHT],
                                    NULL);

  rut_property_set_binding (&ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_X],
                            update_doc_x_cb,
                            ui_viewport,
                            rut_introspectable_lookup_property (ui_viewport->scroll_bar_x,
                                                                "virtual_offset"),
                            NULL);
  rut_property_set_binding (&ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_Y],
                            update_doc_y_cb,
                            ui_viewport,
                            rut_introspectable_lookup_property (ui_viewport->scroll_bar_y,
                                                                "virtual_offset"),
                            NULL);


  ui_viewport->doc_transform = rut_transform_new (ctx, NULL);
  rut_graphable_add_child (ui_viewport, ui_viewport->doc_transform);

  _rut_ui_viewport_update_doc_matrix (ui_viewport);

  ui_viewport->inputable.input_region =
    rut_input_region_new_rectangle (0, 0,
                                    ui_viewport->width,
                                    ui_viewport->height,
                                    _rut_ui_viewport_input_region_cb,
                                    ui_viewport);

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
  float spacing;

  ui_viewport->width = width;
  ui_viewport->height = height;

  rut_input_region_set_rectangle (ui_viewport->inputable.input_region,
                                  0, 0,
                                  width,
                                  height);

  position_scroll_bars (ui_viewport);

  /* If we might need to show both scroll bars at some point then leave a space
   * in the corner so we don't have to deal with the chicken and egg situation
   * of one scroll bar affecting whether the other scrollbar should be visible
   * or not.
   */
  if (ui_viewport->x_pannable && ui_viewport->y_pannable)
    spacing = rut_scroll_bar_get_thickness (ui_viewport->scroll_bar_x);
  else
    spacing = 0;

  rut_scroll_bar_set_length (ui_viewport->scroll_bar_x, width - spacing);
  rut_scroll_bar_set_length (ui_viewport->scroll_bar_y, height - spacing);

  if (ui_viewport->sync_widget)
    queue_allocation (ui_viewport);

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
  maybe_clamp_position (ui_viewport);

  _rut_ui_viewport_update_doc_matrix (ui_viewport);

  rut_property_dirty (&ui_viewport->ctx->property_ctx,
                      &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_X]);
}

void
rut_ui_viewport_set_doc_y (RutUIViewport *ui_viewport, float doc_y)
{
  g_print ("ui_viewport doc_y = %f\n", doc_y);
  ui_viewport->doc_y = doc_y;
  maybe_clamp_position (ui_viewport);

  _rut_ui_viewport_update_doc_matrix (ui_viewport);

  rut_property_dirty (&ui_viewport->ctx->property_ctx,
                      &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_Y]);
}

void
rut_ui_viewport_set_doc_width (RutUIViewport *ui_viewport, float doc_width)
{
  ui_viewport->doc_width = doc_width;
  rut_scroll_bar_set_virtual_length (ui_viewport->scroll_bar_x,
                                     doc_width * (1.0 / ui_viewport->doc_scale_x));

  position_scroll_bars (ui_viewport);

  rut_property_dirty (&ui_viewport->ctx->property_ctx,
                      &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_WIDTH]);
}

void
rut_ui_viewport_set_doc_height (RutUIViewport *ui_viewport, float doc_height)
{
  ui_viewport->doc_height = doc_height;
  rut_scroll_bar_set_virtual_length (ui_viewport->scroll_bar_y,
                                     doc_height * (1.0 / ui_viewport->doc_scale_y));

  position_scroll_bars (ui_viewport);

  rut_property_dirty (&ui_viewport->ctx->property_ctx,
                      &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_DOC_HEIGHT]);
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

static void
preferred_size_change_cb (RutObject *child,
                          void *user_data)
{
  RutUIViewport *ui_viewport = user_data;

  g_warn_if_fail (ui_viewport->sync_widget == child);

  queue_allocation (ui_viewport);
}

void
rut_ui_viewport_set_sync_widget (RutUIViewport *ui_viewport,
                                 RutObject *widget)
{
  RutClosure *preferred_size_closure = NULL;
  RutProperty *property =
    &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_SYNC_WIDGET];

  if (widget)
    {
      g_return_if_fail (rut_object_is (widget, RUT_INTERFACE_ID_SIZABLE));
      rut_refable_ref (widget);
      queue_allocation (ui_viewport);
      preferred_size_closure =
        rut_sizable_add_preferred_size_callback (widget,
                                                 preferred_size_change_cb,
                                                 ui_viewport,
                                                 NULL /* destroy_cb */);
    }

  if (ui_viewport->sync_widget)
    {
      rut_closure_disconnect (ui_viewport->sync_widget_preferred_size_closure);
      rut_refable_unref (ui_viewport->sync_widget);
    }

  ui_viewport->sync_widget_preferred_size_closure = preferred_size_closure;
  ui_viewport->sync_widget = widget;

  rut_property_dirty (&ui_viewport->ctx->property_ctx, property);
}

void
rut_ui_viewport_set_x_expand (RutUIViewport *ui_viewport,
                              CoglBool expand)
{
  if (ui_viewport->x_expand != expand)
    {
      RutProperty *property =
        &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_X_EXPAND];

      ui_viewport->x_expand = expand;

      if (ui_viewport->sync_widget)
        queue_allocation (ui_viewport);

      rut_property_dirty (&ui_viewport->ctx->property_ctx, property);
    }
}

void
rut_ui_viewport_set_y_expand (RutUIViewport *ui_viewport,
                              CoglBool expand)
{
  if (ui_viewport->y_expand != expand)
    {
      RutProperty *property =
        &ui_viewport->properties[RUT_UI_VIEWPORT_PROP_Y_EXPAND];

      ui_viewport->y_expand = expand;

      if (ui_viewport->sync_widget)
        queue_allocation (ui_viewport);

      rut_property_dirty (&ui_viewport->ctx->property_ctx, property);
    }
}
