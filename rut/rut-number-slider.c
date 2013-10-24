/*
 * Rut
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

#include <cogl/cogl.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>

#include "rut.h"
#include "rut-number-slider.h"
#include "rut-text.h"

enum {
  RUT_NUMBER_SLIDER_PROP_VALUE,
  RUT_NUMBER_SLIDER_N_PROPS
};

struct _RutNumberSlider
{
  RutObjectProps _parent;

  RutContext *context;

  RutGraphableProps graphable;

  char *markup_label;

  int width, height;

  int decimal_places;

  int ref_count;

  float min_value, max_value;
  float value;
  float step;

  RutText *text;

  RutInputRegion *input_region;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_NUMBER_SLIDER_N_PROPS];
};

static RutInputEventStatus
rut_number_slider_text_grab_cb (RutInputEvent *event,
                                void *user_data);

static RutInputEventStatus
rut_number_slider_grab_input_cb (RutInputEvent *event,
                                 void *user_data);


RutType rut_number_slider_type;

static RutPropertySpec
_rut_number_slider_prop_specs[] =
  {
    {
      .name = "value",
      .flags = RUT_PROPERTY_FLAG_READWRITE,
      .type = RUT_PROPERTY_TYPE_FLOAT,
      .data_offset = offsetof (RutNumberSlider, value),
      .setter.float_type = rut_number_slider_set_value
    },
    { 0 } /* XXX: Needed for runtime counting of the number of properties */
  };

static void
_rut_number_slider_free (void *object)
{
  RutNumberSlider *slider = object;

  rut_graphable_remove_child (slider->input_region);
  rut_refable_unref (slider->input_region);

  rut_simple_introspectable_destroy (slider);
  rut_graphable_destroy (slider);

  if (slider->markup_label)
    g_free (slider->markup_label);

  g_slice_free (RutNumberSlider, slider);
}

typedef struct _EditState
{
  RutNumberSlider *slider;

  RutCamera *camera;

  RutClosure *activate_closure;

  /* This is set to true after we get a motion event with the down
   * action regardless of where was clicked */
  CoglBool button_down;
  /* This is set to TRUE if cursor has moved more than a couple of
   * pixels since the button was pressed. Once this happens the press
   * is no longer considered a click but is instead interpreted as a
   * drag to change the value */
  CoglBool button_drag;
  /* Where within the widget the cursor was when the button was
   * originally pressed */
  float button_x, button_y;

  /* The original value when the button was pressed */
  float button_value;

} EditState;

static void
update_text (RutNumberSlider *slider)
{
  char *label = slider->markup_label ? slider->markup_label : "";
  char *text = g_strdup_printf ("%s%.*f",
                                label,
                                slider->decimal_places,
                                slider->value);
  rut_text_set_markup (slider->text, text);
  g_free (text);
}

static void
end_text_edit (EditState *state)
{
  RutNumberSlider *slider = state->slider;

  if (state->activate_closure)
    rut_closure_disconnect (state->activate_closure);

  rut_selectable_cancel (slider->text);
  rut_text_set_editable (slider->text, false);

  update_text (slider);

  rut_shell_ungrab_input (slider->context->shell,
                          rut_number_slider_text_grab_cb,
                          state);

  rut_shell_ungrab_input (slider->context->shell,
                          rut_number_slider_grab_input_cb,
                          state);

  g_slice_free (EditState, state);
}

static void
rut_number_slider_commit_text (RutNumberSlider *slider)
{
  const char *text = rut_text_get_text (slider->text);
  double value;

  errno = 0;
  value = strtod (text, NULL);

  if (errno == 0)
    rut_number_slider_set_value (slider, value);
}

static RutInputEventStatus
rut_number_slider_text_grab_cb (RutInputEvent *event,
                                void *user_data)
{
  EditState *state = user_data;
  RutNumberSlider *slider = state->slider;
  float x, y;

  switch (rut_input_event_get_type (event))
    {
    case RUT_INPUT_EVENT_TYPE_MOTION:
      /* Check if this is a click outside of the text control */
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
          (!rut_motion_event_unproject (event, slider, &x, &y) ||
           x < 0 || x >= slider->width ||
           y < 0 || y >= slider->height))
        {
          rut_number_slider_commit_text (slider);
          end_text_edit (state);
        }
      break;

    case RUT_INPUT_EVENT_TYPE_KEY:
      /* The escape key cancels the text control */
      if (rut_key_event_get_action (event) == RUT_KEY_EVENT_ACTION_DOWN &&
          rut_key_event_get_keysym (event) == RUT_KEY_Escape)
        {
          end_text_edit (state);
        }
      break;

    default:
      break;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
rut_number_slider_text_activate_cb (RutText *text, void *user_data)
{
  EditState *state = user_data;
  RutNumberSlider *slider = state->slider;

  rut_number_slider_commit_text (slider);
  end_text_edit (state);
}

static void
start_text_edit (EditState *state)
{
  RutNumberSlider *slider = state->slider;
  char *text = g_strdup_printf ("%.*f",
                                slider->decimal_places,
                                slider->value);
  rut_text_set_markup (slider->text, text);
  g_free (text);

  rut_text_set_editable (slider->text, true);
  rut_text_set_cursor_position (slider->text, 0);
  rut_text_set_selection_bound (slider->text, -1);
  rut_text_grab_key_focus (slider->text);

  state->activate_closure =
    rut_text_add_activate_callback (slider->text,
                                    rut_number_slider_text_activate_cb,
                                    state,
                                    NULL /* destroy_cb */);

  rut_shell_grab_input (slider->context->shell,
                        state->camera,
                        rut_number_slider_text_grab_cb,
                        state);

  rut_shell_queue_redraw (slider->context->shell);
}

static RutInputEventStatus
rut_number_slider_grab_input_cb (RutInputEvent *event,
                                 void *user_data)
{
  EditState *state = user_data;
  RutNumberSlider *slider = state->slider;
  float x, y;

  if (rut_input_event_get_type (event) != RUT_INPUT_EVENT_TYPE_MOTION)
    return RUT_INPUT_EVENT_STATUS_UNHANDLED;

  x = rut_motion_event_get_x (event);
  y = rut_motion_event_get_y (event);

  /* If the cursor has moved at least a pixel since it was clicked
   * then we will mark the button as a drag event so that we don't
   * intepret it as a click when the button is released */
  if (fabsf (x - state->button_x) >= 1.0f ||
      fabsf (y - state->button_y) >= 1.0f)
    state->button_drag = TRUE;

  /* Update the value based on the position if we're in a drag */
  if (state->button_drag)
    rut_number_slider_set_value (slider,
                                 state->button_value +
                                 (x - state->button_x) * slider->step);

  if ((rut_motion_event_get_button_state (event) & RUT_BUTTON_STATE_1) == 0)
    {
      state->button_down = FALSE;
      rut_shell_ungrab_input (slider->context->shell,
                              rut_number_slider_grab_input_cb,
                              user_data);

      /* If we weren't dragging then this must have been an attempt to
       * click somewhere on the widget */
      if (!state->button_drag)
        start_text_edit (state);
      else
        g_slice_free (EditState, state);

      rut_shell_queue_redraw (slider->context->shell);
    }

  return RUT_INPUT_EVENT_STATUS_HANDLED;
}

static RutInputEventStatus
rut_number_slider_input_region_cb (RutInputRegion *region,
                                   RutInputEvent *event,
                                   void *user_data)
{
  RutNumberSlider *slider = user_data;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
      rut_motion_event_get_button_state (event) & RUT_BUTTON_STATE_1)
    {
      EditState *state = g_slice_new0 (EditState);

      state->slider = slider;
      state->camera = rut_input_event_get_camera (event);
      state->button_down = TRUE;
      state->button_drag = FALSE;
      state->button_value = slider->value;
      state->button_x = rut_motion_event_get_x (event);
      state->button_y = rut_motion_event_get_y (event);

      rut_shell_grab_input (slider->context->shell,
                            state->camera,
                            rut_number_slider_grab_input_cb,
                            state);

      rut_shell_queue_redraw (slider->context->shell);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
_rut_number_slider_set_size (RutObject *object,
                             float width,
                             float height)
{
  RutNumberSlider *slider = object;

  rut_composite_sizable_set_size (object, width, height);

  slider->width = width;
  slider->height = height;

  rut_input_region_set_rectangle (slider->input_region,
                                  0.0f, 0.0f, /* x0 / y0 */
                                  slider->width, slider->height /* x1 / y1 */);
}

static void
_rut_number_slider_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };

  static RutIntrospectableVTable introspectable_vtable = {
      rut_simple_introspectable_lookup_property,
      rut_simple_introspectable_foreach_property
  };

  static RutSizableVTable sizable_vtable = {
      _rut_number_slider_set_size,
      rut_composite_sizable_get_size,
      rut_composite_sizable_get_preferred_width,
      rut_composite_sizable_get_preferred_height,
      rut_composite_sizable_add_preferred_size_callback
  };

  RutType *type = &rut_number_slider_type;
#define TYPE RutNumberSlider

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_refable (type, ref_count, _rut_number_slider_free);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &introspectable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (TYPE, introspectable),
                          NULL); /* no implied vtable */
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_COMPOSITE_SIZABLE,
                          offsetof (TYPE, text),
                          NULL); /* no vtable */

#undef TYPE
}

RutNumberSlider *
rut_number_slider_new (RutContext *context)
{
  RutNumberSlider *slider = rut_object_alloc0 (RutNumberSlider,
                                               &rut_number_slider_type,
                                               _rut_number_slider_init_type);

  slider->ref_count = 1;
  slider->context = context;
  slider->step = 1.0f;
  slider->decimal_places = 2;

  slider->max_value = G_MAXFLOAT;

  rut_graphable_init (slider);

  rut_simple_introspectable_init (slider,
                                  _rut_number_slider_prop_specs,
                                  slider->properties);

  slider->text = rut_text_new (context);
  rut_text_set_use_markup (slider->text, true);
  rut_text_set_editable (slider->text, false);
  rut_text_set_activatable (slider->text, true);
  rut_graphable_add_child (slider, slider->text);
  rut_refable_unref (slider->text);

  slider->input_region =
    rut_input_region_new_rectangle (0, 0, 0, 0,
                                    rut_number_slider_input_region_cb,
                                    slider);
  rut_graphable_add_child (slider, slider->input_region);

  update_text (slider);

  rut_sizable_set_size (slider, 60, 30);

  return slider;
}

void
rut_number_slider_set_markup_label (RutNumberSlider *slider,
                                    const char *markup)
{
  g_free (slider->markup_label);
  slider->markup_label = NULL;

  if (markup)
    slider->markup_label = g_strdup (markup);
}

void
rut_number_slider_set_min_value (RutNumberSlider *slider,
                                 float min_value)
{
  slider->min_value = min_value;
  rut_number_slider_set_value (slider, slider->value);
}

void
rut_number_slider_set_max_value (RutNumberSlider *slider,
                                 float max_value)
{
  slider->max_value = max_value;
  rut_number_slider_set_value (slider, slider->value);
}

void
rut_number_slider_set_value (RutObject *obj,
                             float value)
{
  RutNumberSlider *slider = obj;

  value = CLAMP (value, slider->min_value, slider->max_value);

  g_assert (!isnan (value));

  if (value == slider->value)
    return;

  slider->value = value;

  update_text (slider);

  rut_property_dirty (&slider->context->property_ctx,
                      &slider->properties[RUT_NUMBER_SLIDER_PROP_VALUE]);

  rut_shell_queue_redraw (slider->context->shell);
}

float
rut_number_slider_get_value (RutNumberSlider *slider)
{
  return slider->value;
}

void
rut_number_slider_set_step (RutNumberSlider *slider,
                            float step)
{
  slider->step = step;
}

int
rut_number_slider_get_decimal_places (RutNumberSlider *slider)
{
  return slider->decimal_places;
}

void
rut_number_slider_set_decimal_places (RutNumberSlider *slider,
                                      int decimal_places)
{
  rut_shell_queue_redraw (slider->context->shell);

  slider->decimal_places = decimal_places;
  update_text (slider);
}
