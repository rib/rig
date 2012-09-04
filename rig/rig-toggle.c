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

#include "rig-toggle.h"
#include "rig-interfaces.h"
#include "rig-paintable.h"
#include "rig-camera-private.h"
#include "components/rig-camera.h"

#define RIG_TOGGLE_BOX_WIDTH 15
#define RIG_TOGGLE_BOX_RIGHT_PAD 5
#define RIG_TOGGLE_LABEL_VPAD 23
#define RIG_TOGGLE_MIN_LABEL_WIDTH 30

#define RIG_TOGGLE_MIN_WIDTH ((RIG_TOGGLE_BOX_WIDTH + \
                               RIG_TOGGLE_BOX_RIGHT_PAD + \
                               RIG_TOGGLE_MIN_LABEL_WIDTH)


enum {
  RIG_TOGGLE_PROP_STATE,
  RIG_TOGGLE_PROP_ENABLED,
  RIG_TOGGLE_PROP_TICK,
  RIG_TOGGLE_PROP_TICK_COLOR,
  RIG_TOGGLE_N_PROPS
};

struct _RigToggle
{
  RigObjectProps _parent;
  int ref_count;

  RigContext *ctx;

  CoglBool state;
  CoglBool enabled;

  /* While we have the input grabbed we want to reflect what
   * the state will be when the mouse button is released
   * without actually changing the state... */
  CoglBool tentative_set;

  /* FIXME: we don't need a separate tick for every toggle! */
  PangoLayout *tick;

  PangoLayout *label;
  int label_width;
  int label_height;

  float width;
  float height;

  /* FIXME: we should be able to share border/box pipelines
   * between different toggle boxes. */
  CoglPipeline *pipeline_border;
  CoglPipeline *pipeline_box;

  CoglColor text_color;
  CoglColor tick_color;

  RigInputRegion *input_region;

  RigList on_toggle_cb_list;

  RigGraphableProps graphable;
  RigPaintableProps paintable;

  RigSimpleIntrospectableProps introspectable;
  RigProperty properties[RIG_TOGGLE_N_PROPS];
};

static RigPropertySpec _rig_toggle_prop_specs[] = {
  {
    .name = "state",
    .type = RIG_PROPERTY_TYPE_BOOLEAN,
    .data_offset = offsetof (RigToggle, state),
    .setter = rig_toggle_set_state
  },
  {
    .name = "enabled",
    .type = RIG_PROPERTY_TYPE_BOOLEAN,
    .data_offset = offsetof (RigToggle, state),
    .setter = rig_toggle_set_enabled
  },
  {
    .name = "tick",
    .type = RIG_PROPERTY_TYPE_TEXT,
    .setter = rig_toggle_set_tick,
    .getter = rig_toggle_get_tick
  },
  {
    .name = "tick_color",
    .type = RIG_PROPERTY_TYPE_COLOR,
    .setter = rig_toggle_set_tick_color,
    .getter = rig_toggle_get_tick_color
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rig_toggle_free (void *object)
{
  RigToggle *toggle = object;

  rig_closure_list_disconnect_all (&toggle->on_toggle_cb_list);

  g_object_unref (toggle->tick);
  g_object_unref (toggle->label);

  cogl_object_unref (toggle->pipeline_border);
  cogl_object_unref (toggle->pipeline_box);

  rig_simple_introspectable_destroy (toggle);

  g_slice_free (RigToggle, object);
}

RigRefCountableVTable _rig_toggle_ref_countable_vtable = {
  rig_ref_countable_simple_ref,
  rig_ref_countable_simple_unref,
  _rig_toggle_free
};

static RigGraphableVTable _rig_toggle_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static void
_rig_toggle_paint (RigObject *object,
                   RigPaintContext *paint_ctx)
{
  RigToggle *toggle = RIG_TOGGLE (object);
  RigCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = camera->fb;
  int box_y;

  /* FIXME: This is a fairly lame way of drawing a check box! */

  box_y = (toggle->label_height / 2.0) - (RIG_TOGGLE_BOX_WIDTH / 2.0);


  cogl_framebuffer_draw_rectangle (fb,
                                   toggle->pipeline_border,
                                   0, box_y,
                                   RIG_TOGGLE_BOX_WIDTH,
                                   box_y + RIG_TOGGLE_BOX_WIDTH);

  cogl_framebuffer_draw_rectangle (fb,
                                   toggle->pipeline_box,
                                   1, box_y + 1,
                                   RIG_TOGGLE_BOX_WIDTH - 2,
                                   box_y + RIG_TOGGLE_BOX_WIDTH - 2);

  if (toggle->state || toggle->tentative_set)
    cogl_pango_show_layout (camera->fb,
                            toggle->tick,
                            0, 0,
                            &toggle->tick_color);

  cogl_pango_show_layout (camera->fb,
                          toggle->label,
                          RIG_TOGGLE_BOX_WIDTH + RIG_TOGGLE_BOX_RIGHT_PAD, 0,
                          &toggle->text_color);
}

static RigPaintableVTable _rig_toggle_paintable_vtable = {
  _rig_toggle_paint
};

static RigIntrospectableVTable _rig_toggle_introspectable_vtable = {
  rig_simple_introspectable_lookup_property,
  rig_simple_introspectable_foreach_property
};

RigSimpleWidgetVTable _rig_toggle_simple_widget_vtable = {
 0
};

static void
_rig_toggle_set_size (RigObject *object,
                      float width,
                      float height)
{
  /* FIXME: we could elipsize the label if smaller than our preferred size */
}

static void
_rig_toggle_get_size (RigObject *object,
                      float *width,
                      float *height)
{
  RigToggle *toggle = RIG_TOGGLE (object);

  *width = toggle->width;
  *height = toggle->height;
}

static void
_rig_toggle_get_preferred_width (RigObject *object,
                                 float for_height,
                                 float *min_width_p,
                                 float *natural_width_p)
{
  RigToggle *toggle = RIG_TOGGLE (object);
  PangoRectangle logical_rect;
  float width;

  pango_layout_get_pixel_extents (toggle->label, NULL, &logical_rect);
  width = logical_rect.width + RIG_TOGGLE_BOX_WIDTH + RIG_TOGGLE_BOX_RIGHT_PAD;

  if (min_width_p)
    *min_width_p = width;
  if (natural_width_p)
    *natural_width_p = width;
}

static void
_rig_toggle_get_preferred_height (RigObject *object,
                                  float for_width,
                                  float *min_height_p,
                                  float *natural_height_p)
{
  RigToggle *toggle = RIG_TOGGLE (object);
  PangoRectangle logical_rect;
  float height;

  pango_layout_get_pixel_extents (toggle->label, NULL, &logical_rect);
  height = MAX (logical_rect.height, RIG_TOGGLE_BOX_WIDTH);

  if (min_height_p)
    *min_height_p = height;
  if (natural_height_p)
    *natural_height_p = height;
}

static RigSizableVTable _rig_toggle_sizable_vtable = {
  _rig_toggle_set_size,
  _rig_toggle_get_size,
  _rig_toggle_get_preferred_width,
  _rig_toggle_get_preferred_height
};

RigType rig_toggle_type;

static void
_rig_toggle_init_type (void)
{
  rig_type_init (&rig_toggle_type);
  rig_type_add_interface (&rig_toggle_type,
                          RIG_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RigToggle, ref_count),
                          &_rig_toggle_ref_countable_vtable);
  rig_type_add_interface (&rig_toggle_type,
                          RIG_INTERFACE_ID_GRAPHABLE,
                          offsetof (RigToggle, graphable),
                          &_rig_toggle_graphable_vtable);
  rig_type_add_interface (&rig_toggle_type,
                          RIG_INTERFACE_ID_PAINTABLE,
                          offsetof (RigToggle, paintable),
                          &_rig_toggle_paintable_vtable);
  rig_type_add_interface (&rig_toggle_type,
                          RIG_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rig_toggle_introspectable_vtable);
  rig_type_add_interface (&rig_toggle_type,
                          RIG_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RigToggle, introspectable),
                          NULL); /* no implied vtable */
  rig_type_add_interface (&rig_toggle_type,
                          RIG_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rig_toggle_sizable_vtable);
}

typedef struct _ToggleGrabState
{
  RigCamera *camera;
  RigInputRegion *region;
  RigToggle *toggle;
} ToggleGrabState;

static RigInputEventStatus
_rig_toggle_grab_input_cb (RigInputEvent *event,
                           void *user_data)
{
  ToggleGrabState *state = user_data;
  RigToggle *toggle = state->toggle;

  if(rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION)
    {
      RigShell *shell = toggle->ctx->shell;
      if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_UP)
        {
          float x = rig_motion_event_get_x (event);
          float y = rig_motion_event_get_y (event);

          rig_shell_ungrab_input (shell, _rig_toggle_grab_input_cb, user_data);

          if (rig_camera_pick_input_region (state->camera,
                                            state->region,
                                            x, y))
            {
              toggle->state = !toggle->state;

              rig_closure_list_invoke (&toggle->on_toggle_cb_list,
                                       RigToggleCallback,
                                       toggle,
                                       toggle->state);

              g_print ("Toggle click\n");

              g_slice_free (ToggleGrabState, state);

              rig_shell_queue_redraw (toggle->ctx->shell);

              toggle->tentative_set = FALSE;
            }

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_MOVE)
        {
          float x = rig_motion_event_get_x (event);
          float y = rig_motion_event_get_y (event);

         if (rig_camera_pick_input_region (state->camera,
                                           state->region,
                                           x, y))
           toggle->tentative_set = TRUE;
         else
           toggle->tentative_set = FALSE;

          rig_shell_queue_redraw (toggle->ctx->shell);

          return RIG_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static RigInputEventStatus
_rig_toggle_input_cb (RigInputRegion *region,
                      RigInputEvent *event,
                      void *user_data)
{
  RigToggle *toggle = user_data;

  g_print ("Toggle input\n");

  if(rig_input_event_get_type (event) == RIG_INPUT_EVENT_TYPE_MOTION &&
     rig_motion_event_get_action (event) == RIG_MOTION_EVENT_ACTION_DOWN)
    {
      RigShell *shell = toggle->ctx->shell;
      ToggleGrabState *state = g_slice_new (ToggleGrabState);

      state->toggle = toggle;
      state->camera = rig_input_event_get_camera (event);
      state->region = region;

      rig_shell_grab_input (shell,
                            state->camera,
                            _rig_toggle_grab_input_cb,
                            state);

      toggle->tentative_set = TRUE;

      rig_shell_queue_redraw (toggle->ctx->shell);

      return RIG_INPUT_EVENT_STATUS_HANDLED;
    }

  return RIG_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
_rig_toggle_update_colours (RigToggle *toggle)
{
  uint32_t colors[2][2][3] =
    {
        /* Disabled */
        {
            /* Unset */
            {
                0x000000ff, /* border */
                0xffffffff, /* box */
                0x000000ff /* text*/
            },
            /* Set */
            {
                0x000000ff, /* border */
                0xffffffff, /* box */
                0x000000ff /* text*/
            },

        },
        /* Enabled */
        {
            /* Unset */
            {
                0x000000ff, /* border */
                0xffffffff, /* box */
                0x000000ff /* text*/
            },
            /* Set */
            {
                0x000000ff, /* border */
                0xffffffff, /* box */
                0x000000ff /* text*/
            },
        }
    };

  uint32_t border, box, text;

  border = colors[toggle->enabled][toggle->state][0];
  box = colors[toggle->enabled][toggle->state][1];
  text = colors[toggle->enabled][toggle->state][2];

  cogl_pipeline_set_color4f (toggle->pipeline_border,
                             RIG_UINT32_RED_AS_FLOAT (border),
                             RIG_UINT32_GREEN_AS_FLOAT (border),
                             RIG_UINT32_BLUE_AS_FLOAT (border),
                             RIG_UINT32_ALPHA_AS_FLOAT (border));
  cogl_pipeline_set_color4f (toggle->pipeline_box,
                             RIG_UINT32_RED_AS_FLOAT (box),
                             RIG_UINT32_GREEN_AS_FLOAT (box),
                             RIG_UINT32_BLUE_AS_FLOAT (box),
                             RIG_UINT32_ALPHA_AS_FLOAT (box));
  cogl_color_init_from_4f (&toggle->text_color,
                           RIG_UINT32_RED_AS_FLOAT (text),
                           RIG_UINT32_GREEN_AS_FLOAT (text),
                           RIG_UINT32_BLUE_AS_FLOAT (text),
                           RIG_UINT32_ALPHA_AS_FLOAT (text));
  cogl_color_init_from_4f (&toggle->tick_color,
                           RIG_UINT32_RED_AS_FLOAT (text),
                           RIG_UINT32_GREEN_AS_FLOAT (text),
                           RIG_UINT32_BLUE_AS_FLOAT (text),
                           RIG_UINT32_ALPHA_AS_FLOAT (text));
}

RigToggle *
rig_toggle_new (RigContext *ctx,
                const char *label)
{
  RigToggle *toggle = g_slice_new0 (RigToggle);
  PangoRectangle label_size;
  char *font_name;
  PangoFontDescription *font_desc;
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_init ();
      _rig_toggle_init_type ();

      initialized = TRUE;
    }

  rig_object_init (RIG_OBJECT (toggle), &rig_toggle_type);

  toggle->ref_count = 1;

  rig_list_init (&toggle->on_toggle_cb_list);

  rig_graphable_init (toggle);
  rig_paintable_init (toggle);

  rig_simple_introspectable_init (toggle,
                                  _rig_toggle_prop_specs,
                                  toggle->properties);

  toggle->ctx = ctx;

  toggle->state = TRUE;
  toggle->enabled = TRUE;

  toggle->tick = pango_layout_new (ctx->pango_context);
  pango_layout_set_font_description (toggle->tick, ctx->pango_font_desc);
  pango_layout_set_text (toggle->tick, "✔", -1);

  font_name = rig_settings_get_font_name (ctx->settings); /* font_name is allocated */
  font_desc = pango_font_description_from_string (font_name);
  g_free (font_name);
  pango_font_description_free (font_desc);

  toggle->label = pango_layout_new (ctx->pango_context);
  pango_layout_set_font_description (toggle->label, font_desc);
  pango_layout_set_text (toggle->label, label, -1);

  pango_layout_get_extents (toggle->label, NULL, &label_size);
  toggle->label_width = PANGO_PIXELS (label_size.width);
  toggle->label_height = PANGO_PIXELS (label_size.height);

  toggle->width = toggle->label_width + RIG_TOGGLE_BOX_RIGHT_PAD + RIG_TOGGLE_BOX_WIDTH;
  toggle->height = toggle->label_height + RIG_TOGGLE_LABEL_VPAD;

  toggle->pipeline_border = cogl_pipeline_new (ctx->cogl_context);
  toggle->pipeline_box = cogl_pipeline_new (ctx->cogl_context);

  _rig_toggle_update_colours (toggle);

  toggle->input_region =
    rig_input_region_new_rectangle (0, 0,
                                    RIG_TOGGLE_BOX_WIDTH,
                                    RIG_TOGGLE_BOX_WIDTH,
                                    _rig_toggle_input_cb,
                                    toggle);

  //rig_input_region_set_graphable (toggle->input_region, toggle);
  rig_graphable_add_child (toggle, toggle->input_region);

  return toggle;
}

RigClosure *
rig_toggle_add_on_toggle_callback (RigToggle *toggle,
                                   RigToggleCallback callback,
                                   void *user_data,
                                   RigClosureDestroyCallback destroy_cb)
{
  g_return_val_if_fail (callback != NULL, NULL);

  return rig_closure_list_add (&toggle->on_toggle_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

void
rig_toggle_set_enabled (RigToggle *toggle,
                        CoglBool enabled)
{
  if (toggle->enabled == enabled)
    return;

  toggle->enabled = enabled;
  rig_property_dirty (&toggle->ctx->property_ctx,
                      &toggle->properties[RIG_TOGGLE_PROP_ENABLED]);
  rig_shell_queue_redraw (toggle->ctx->shell);
}

void
rig_toggle_set_state (RigToggle *toggle,
                      CoglBool state)
{
  if (toggle->state == state)
    return;

  toggle->state = state;
  rig_property_dirty (&toggle->ctx->property_ctx,
                      &toggle->properties[RIG_TOGGLE_PROP_STATE]);
  rig_shell_queue_redraw (toggle->ctx->shell);
}

RigProperty *
rig_toggle_get_enabled_property (RigToggle *toggle)
{
  return &toggle->properties[RIG_TOGGLE_PROP_STATE];
}

void
rig_toggle_set_tick (RigToggle *toggle,
                     const char *tick)
{
  pango_layout_set_text (toggle->tick, tick, -1);
  rig_shell_queue_redraw (toggle->ctx->shell);
}

const char *
rig_toggle_get_tick (RigToggle *toggle)
{
  return pango_layout_get_text (toggle->tick);
}

void
rig_toggle_set_tick_color (RigToggle *toggle,
                           const RigColor *color)
{
  toggle->tick_color.red = color->red;
  toggle->tick_color.green = color->green;
  toggle->tick_color.blue = color->blue;
  toggle->tick_color.alpha = color->alpha;
  rig_shell_queue_redraw (toggle->ctx->shell);
}

void
rig_toggle_get_tick_color (RigToggle *toggle,
                           RigColor *color)
{
  color->red = toggle->tick_color.red;
  color->green = toggle->tick_color.green;
  color->blue = toggle->tick_color.blue;
  color->alpha = toggle->tick_color.alpha;
}
