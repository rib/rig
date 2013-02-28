/*
 * Rut
 *
 * Copyright (C) 2012,2013 Intel Corporation.
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

#include "rut-toggle.h"
#include "rut-interfaces.h"
#include "rut-paintable.h"
#include "rut-camera-private.h"
#include "components/rut-camera.h"

#define RUT_TOGGLE_BOX_WIDTH 15
#define RUT_TOGGLE_BOX_RIGHT_PAD 5
#define RUT_TOGGLE_LABEL_VPAD 23
#define RUT_TOGGLE_MIN_LABEL_WIDTH 30

#define RUT_TOGGLE_MIN_WIDTH ((RUT_TOGGLE_BOX_WIDTH + \
                               RUT_TOGGLE_BOX_RIGHT_PAD + \
                               RUT_TOGGLE_MIN_LABEL_WIDTH)


enum {
  RUT_TOGGLE_PROP_STATE,
  RUT_TOGGLE_PROP_ENABLED,
  RUT_TOGGLE_PROP_TICK,
  RUT_TOGGLE_PROP_TICK_COLOR,
  RUT_TOGGLE_N_PROPS
};

struct _RutToggle
{
  RutObjectProps _parent;
  int ref_count;

  RutContext *ctx;

  CoglBool state;
  CoglBool enabled;

  /* While we have the input grabbed we want to reflect what
   * the state will be when the mouse button is released
   * without actually changing the state... */
  CoglBool tentative_set;

  /* FIXME: we don't need a separate tick for every toggle! */
  PangoLayout *tick;

  CoglTexture *selected_icon;
  CoglTexture *unselected_icon;

  PangoLayout *label;
  int label_width;
  int label_height;

  float width;
  float height;

  /* FIXME: we should be able to share these pipelines
   * between different toggle boxes. */
  CoglPipeline *pipeline_border;
  CoglPipeline *pipeline_box;
  CoglPipeline *pipeline_selected_icon;
  CoglPipeline *pipeline_unselected_icon;

  CoglColor text_color;
  CoglColor tick_color;

  RutInputRegion *input_region;

  RutList on_toggle_cb_list;

  RutGraphableProps graphable;
  RutPaintableProps paintable;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_TOGGLE_N_PROPS];
};

static RutPropertySpec _rut_toggle_prop_specs[] = {
  {
    .name = "state",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .data_offset = offsetof (RutToggle, state),
    .setter.boolean_type = rut_toggle_set_state
  },
  {
    .name = "enabled",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .data_offset = offsetof (RutToggle, state),
    .setter.boolean_type = rut_toggle_set_enabled
  },
  {
    .name = "tick",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_TEXT,
    .setter.text_type = rut_toggle_set_tick,
    .getter.text_type = rut_toggle_get_tick
  },
  {
    .name = "tick_color",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_COLOR,
    .setter.color_type = rut_toggle_set_tick_color,
    .getter.color_type = rut_toggle_get_tick_color
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_toggle_free (void *object)
{
  RutToggle *toggle = object;

  rut_closure_list_disconnect_all (&toggle->on_toggle_cb_list);

  if (toggle->selected_icon)
    {
      cogl_object_unref (toggle->selected_icon);
      cogl_object_unref (toggle->pipeline_selected_icon);
    }
  if (toggle->unselected_icon)
    {
      cogl_object_unref (toggle->unselected_icon);
      cogl_object_unref (toggle->pipeline_unselected_icon);
    }
  if (toggle->tick)
    g_object_unref (toggle->tick);
  g_object_unref (toggle->label);

  cogl_object_unref (toggle->pipeline_border);
  cogl_object_unref (toggle->pipeline_box);

  rut_simple_introspectable_destroy (toggle);
  rut_graphable_destroy (toggle);

  g_slice_free (RutToggle, object);
}

RutRefCountableVTable _rut_toggle_ref_countable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_toggle_free
};

static RutGraphableVTable _rut_toggle_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static void
_rut_toggle_paint (RutObject *object,
                   RutPaintContext *paint_ctx)
{
  RutToggle *toggle = RUT_TOGGLE (object);
  RutCamera *camera = paint_ctx->camera;
  CoglFramebuffer *fb = camera->fb;
  int icon_width;

  if (toggle->selected_icon)
    {
      CoglTexture *icon;
      CoglPipeline *pipeline;
      int icon_y;
      int icon_height;

      if (toggle->state || toggle->tentative_set)
        {
          icon = toggle->selected_icon;
          pipeline = toggle->pipeline_selected_icon;
        }
      else
        {
          icon = toggle->unselected_icon;
          pipeline = toggle->pipeline_unselected_icon;
        }

      icon_y = (toggle->label_height / 2.0) -
        (cogl_texture_get_height (icon) / 2.0);
      icon_width = cogl_texture_get_width (icon);
      icon_height = cogl_texture_get_height (icon);

      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       0, icon_y,
                                       icon_width,
                                       icon_y + icon_height);
    }
  else
    {
      /* FIXME: This is a fairly lame way of drawing a check box! */

      int box_y = (toggle->label_height / 2.0) - (RUT_TOGGLE_BOX_WIDTH / 2.0);


      cogl_framebuffer_draw_rectangle (fb,
                                       toggle->pipeline_border,
                                       0, box_y,
                                       RUT_TOGGLE_BOX_WIDTH,
                                       box_y + RUT_TOGGLE_BOX_WIDTH);

      cogl_framebuffer_draw_rectangle (fb,
                                       toggle->pipeline_box,
                                       1, box_y + 1,
                                       RUT_TOGGLE_BOX_WIDTH - 2,
                                       box_y + RUT_TOGGLE_BOX_WIDTH - 2);

      if (toggle->state || toggle->tentative_set)
        cogl_pango_show_layout (camera->fb,
                                toggle->tick,
                                0, 0,
                                &toggle->tick_color);

      icon_width = RUT_TOGGLE_BOX_WIDTH;
    }

  cogl_pango_show_layout (camera->fb,
                          toggle->label,
                          icon_width + RUT_TOGGLE_BOX_RIGHT_PAD, 0,
                          &toggle->text_color);
}

static RutPaintableVTable _rut_toggle_paintable_vtable = {
  _rut_toggle_paint
};

static RutIntrospectableVTable _rut_toggle_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

static void
_rut_toggle_set_size (RutObject *object,
                      float width,
                      float height)
{
  /* FIXME: we could elipsize the label if smaller than our preferred size */
}

static void
_rut_toggle_get_size (RutObject *object,
                      float *width,
                      float *height)
{
  RutToggle *toggle = RUT_TOGGLE (object);

  *width = toggle->width;
  *height = toggle->height;
}

static void
_rut_toggle_get_preferred_width (RutObject *object,
                                 float for_height,
                                 float *min_width_p,
                                 float *natural_width_p)
{
  RutToggle *toggle = RUT_TOGGLE (object);
  PangoRectangle logical_rect;
  float width;
  float right_pad;

  pango_layout_get_pixel_extents (toggle->label, NULL, &logical_rect);

  /* Don't bother padding the right of the toggle button if the label
   * is empty */
  if (logical_rect.width > 0)
    right_pad = RUT_TOGGLE_BOX_RIGHT_PAD;
  else
    right_pad = 0.0f;

  if (toggle->selected_icon)
    {
      width = logical_rect.width +
        cogl_texture_get_width (toggle->selected_icon) +
        right_pad;
    }
  else
    width = logical_rect.width + RUT_TOGGLE_BOX_WIDTH + right_pad;

  if (min_width_p)
    *min_width_p = width;
  if (natural_width_p)
    *natural_width_p = width;
}

static void
_rut_toggle_get_preferred_height (RutObject *object,
                                  float for_width,
                                  float *min_height_p,
                                  float *natural_height_p)
{
  RutToggle *toggle = RUT_TOGGLE (object);
  PangoRectangle logical_rect;
  float height;

  pango_layout_get_pixel_extents (toggle->label, NULL, &logical_rect);

  if (toggle->selected_icon)
    height = MAX (logical_rect.height,
                  cogl_texture_get_height (toggle->selected_icon));
  else
    height = MAX (logical_rect.height, RUT_TOGGLE_BOX_WIDTH);

  if (min_height_p)
    *min_height_p = height;
  if (natural_height_p)
    *natural_height_p = height;
}

static RutSizableVTable _rut_toggle_sizable_vtable = {
  _rut_toggle_set_size,
  _rut_toggle_get_size,
  _rut_toggle_get_preferred_width,
  _rut_toggle_get_preferred_height,
  NULL /* add_preferred_size_callback (the preferred size never changes) */
};

RutType rut_toggle_type;

static void
_rut_toggle_init_type (void)
{
  rut_type_init (&rut_toggle_type, "RigToggle");
  rut_type_add_interface (&rut_toggle_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutToggle, ref_count),
                          &_rut_toggle_ref_countable_vtable);
  rut_type_add_interface (&rut_toggle_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutToggle, graphable),
                          &_rut_toggle_graphable_vtable);
  rut_type_add_interface (&rut_toggle_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutToggle, paintable),
                          &_rut_toggle_paintable_vtable);
  rut_type_add_interface (&rut_toggle_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_toggle_introspectable_vtable);
  rut_type_add_interface (&rut_toggle_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutToggle, introspectable),
                          NULL); /* no implied vtable */
  rut_type_add_interface (&rut_toggle_type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &_rut_toggle_sizable_vtable);
}

typedef struct _ToggleGrabState
{
  RutCamera *camera;
  RutInputRegion *region;
  RutToggle *toggle;
} ToggleGrabState;

static RutInputEventStatus
_rut_toggle_grab_input_cb (RutInputEvent *event,
                           void *user_data)
{
  ToggleGrabState *state = user_data;
  RutToggle *toggle = state->toggle;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutShell *shell = toggle->ctx->shell;
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);

          rut_shell_ungrab_input (shell, _rut_toggle_grab_input_cb, user_data);

          if (rut_camera_pick_inputable (state->camera,
                                         state->region,
                                         x, y))
            {
              rut_toggle_set_state (toggle, !toggle->state);

              rut_closure_list_invoke (&toggle->on_toggle_cb_list,
                                       RutToggleCallback,
                                       toggle,
                                       toggle->state);

              g_print ("Toggle click\n");

              g_slice_free (ToggleGrabState, state);

              rut_shell_queue_redraw (toggle->ctx->shell);

              toggle->tentative_set = FALSE;
            }

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);

         if (rut_camera_pick_inputable (state->camera,
                                        state->region,
                                        x, y))
           toggle->tentative_set = TRUE;
         else
           toggle->tentative_set = FALSE;

          rut_shell_queue_redraw (toggle->ctx->shell);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_toggle_input_cb (RutInputRegion *region,
                      RutInputEvent *event,
                      void *user_data)
{
  RutToggle *toggle = user_data;

  g_print ("Toggle input\n");

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
     rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = toggle->ctx->shell;
      ToggleGrabState *state = g_slice_new (ToggleGrabState);

      state->toggle = toggle;
      state->camera = rut_input_event_get_camera (event);
      state->region = region;

      rut_shell_grab_input (shell,
                            state->camera,
                            _rut_toggle_grab_input_cb,
                            state);

      toggle->tentative_set = TRUE;

      rut_shell_queue_redraw (toggle->ctx->shell);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
_rut_toggle_update_colours (RutToggle *toggle)
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
                             RUT_UINT32_RED_AS_FLOAT (border),
                             RUT_UINT32_GREEN_AS_FLOAT (border),
                             RUT_UINT32_BLUE_AS_FLOAT (border),
                             RUT_UINT32_ALPHA_AS_FLOAT (border));
  cogl_pipeline_set_color4f (toggle->pipeline_box,
                             RUT_UINT32_RED_AS_FLOAT (box),
                             RUT_UINT32_GREEN_AS_FLOAT (box),
                             RUT_UINT32_BLUE_AS_FLOAT (box),
                             RUT_UINT32_ALPHA_AS_FLOAT (box));
  cogl_color_init_from_4f (&toggle->text_color,
                           RUT_UINT32_RED_AS_FLOAT (text),
                           RUT_UINT32_GREEN_AS_FLOAT (text),
                           RUT_UINT32_BLUE_AS_FLOAT (text),
                           RUT_UINT32_ALPHA_AS_FLOAT (text));
  cogl_color_init_from_4f (&toggle->tick_color,
                           RUT_UINT32_RED_AS_FLOAT (text),
                           RUT_UINT32_GREEN_AS_FLOAT (text),
                           RUT_UINT32_BLUE_AS_FLOAT (text),
                           RUT_UINT32_ALPHA_AS_FLOAT (text));
}

RutToggle *
rut_toggle_new_with_icons (RutContext *ctx,
                           const char *unselected_icon,
                           const char *selected_icon,
                           const char *label)
{
  RutToggle *toggle = g_slice_new0 (RutToggle);
  PangoRectangle label_size;
  char *font_name;
  PangoFontDescription *font_desc;
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_init ();
      _rut_toggle_init_type ();

      initialized = TRUE;
    }

  rut_object_init (RUT_OBJECT (toggle), &rut_toggle_type);

  toggle->ref_count = 1;

  rut_list_init (&toggle->on_toggle_cb_list);

  rut_graphable_init (toggle);
  rut_paintable_init (toggle);

  rut_simple_introspectable_init (toggle,
                                  _rut_toggle_prop_specs,
                                  toggle->properties);

  toggle->ctx = ctx;

  toggle->state = TRUE;
  toggle->enabled = TRUE;

  if (selected_icon)
    {
      toggle->selected_icon = rut_load_texture (ctx, selected_icon, NULL);
      if (toggle->selected_icon && unselected_icon)
        toggle->unselected_icon = rut_load_texture (ctx, unselected_icon, NULL);
      if (toggle->unselected_icon)
        {
          toggle->pipeline_selected_icon = cogl_pipeline_new (ctx->cogl_context);
          cogl_pipeline_set_layer_texture (toggle->pipeline_selected_icon, 0,
                                           toggle->selected_icon);
          toggle->pipeline_unselected_icon = cogl_pipeline_copy (toggle->pipeline_selected_icon);
          cogl_pipeline_set_layer_texture (toggle->pipeline_unselected_icon, 0,
                                           toggle->unselected_icon);
        }
      else
        {
          g_warning ("Failed to load toggle icons %s and %s",
                     selected_icon, unselected_icon);
          if (toggle->selected_icon)
            {
              cogl_object_unref (toggle->selected_icon);
              toggle->selected_icon = NULL;
            }
        }
    }

  if (!toggle->selected_icon)
    {
      toggle->tick = pango_layout_new (ctx->pango_context);
      pango_layout_set_font_description (toggle->tick, ctx->pango_font_desc);
      pango_layout_set_text (toggle->tick, "✔", -1);
    }

  font_name = rut_settings_get_font_name (ctx->settings); /* font_name is allocated */
  font_desc = pango_font_description_from_string (font_name);
  g_free (font_name);

  toggle->label = pango_layout_new (ctx->pango_context);
  pango_layout_set_font_description (toggle->label, font_desc);
  pango_layout_set_text (toggle->label, label, -1);

  pango_font_description_free (font_desc);

  pango_layout_get_extents (toggle->label, NULL, &label_size);
  toggle->label_width = PANGO_PIXELS (label_size.width);
  toggle->label_height = PANGO_PIXELS (label_size.height);

  toggle->width = toggle->label_width + RUT_TOGGLE_BOX_RIGHT_PAD + RUT_TOGGLE_BOX_WIDTH;
  toggle->height = toggle->label_height + RUT_TOGGLE_LABEL_VPAD;

  toggle->pipeline_border = cogl_pipeline_new (ctx->cogl_context);
  toggle->pipeline_box = cogl_pipeline_new (ctx->cogl_context);

  _rut_toggle_update_colours (toggle);

  toggle->input_region =
    rut_input_region_new_rectangle (0, 0,
                                    RUT_TOGGLE_BOX_WIDTH,
                                    RUT_TOGGLE_BOX_WIDTH,
                                    _rut_toggle_input_cb,
                                    toggle);

  //rut_input_region_set_graphable (toggle->input_region, toggle);
  rut_graphable_add_child (toggle, toggle->input_region);

  return toggle;
}

RutToggle *
rut_toggle_new (RutContext *ctx,
                const char *label)
{
  return rut_toggle_new_with_icons (ctx, NULL, NULL, label);
}

RutClosure *
rut_toggle_add_on_toggle_callback (RutToggle *toggle,
                                   RutToggleCallback callback,
                                   void *user_data,
                                   RutClosureDestroyCallback destroy_cb)
{
  g_return_val_if_fail (callback != NULL, NULL);

  return rut_closure_list_add (&toggle->on_toggle_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

void
rut_toggle_set_enabled (RutObject *obj,
                        CoglBool enabled)
{
  RutToggle *toggle = RUT_TOGGLE (obj);

  if (toggle->enabled == enabled)
    return;

  toggle->enabled = enabled;
  rut_property_dirty (&toggle->ctx->property_ctx,
                      &toggle->properties[RUT_TOGGLE_PROP_ENABLED]);
  rut_shell_queue_redraw (toggle->ctx->shell);
}

void
rut_toggle_set_state (RutObject *obj,
                      CoglBool state)
{
  RutToggle *toggle = RUT_TOGGLE (obj);

  if (toggle->state == state)
    return;

  toggle->state = state;
  rut_property_dirty (&toggle->ctx->property_ctx,
                      &toggle->properties[RUT_TOGGLE_PROP_STATE]);
  rut_shell_queue_redraw (toggle->ctx->shell);
}

RutProperty *
rut_toggle_get_enabled_property (RutToggle *toggle)
{
  return &toggle->properties[RUT_TOGGLE_PROP_STATE];
}

void
rut_toggle_set_tick (RutObject *obj,
                     const char *tick)
{
  RutToggle *toggle = RUT_TOGGLE (obj);

  pango_layout_set_text (toggle->tick, tick, -1);
  rut_shell_queue_redraw (toggle->ctx->shell);
}

const char *
rut_toggle_get_tick (RutObject *obj)
{
  RutToggle *toggle = RUT_TOGGLE (obj);

  return pango_layout_get_text (toggle->tick);
}

void
rut_toggle_set_tick_color (RutObject *obj,
                           const CoglColor *color)
{
  RutToggle *toggle = RUT_TOGGLE (obj);

  toggle->tick_color = *color;
  rut_shell_queue_redraw (toggle->ctx->shell);
}

const CoglColor *
rut_toggle_get_tick_color (RutObject *obj)
{
  RutToggle *toggle = RUT_TOGGLE (obj);

  return &toggle->tick_color;
}
