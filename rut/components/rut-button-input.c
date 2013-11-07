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
 */

#include <config.h>

#include <glib.h>

#include <cogl/cogl.h>

#include "rut-interfaces.h"
#include "rut-camera-private.h"
#include "rut-button-input.h"
#include "rut-closure.h"
#include "rut-inputable.h"

#include "components/rut-camera.h"

enum {
  RUT_BUTTON_INPUT_PROP_PRESS_COUNT,
  RUT_BUTTON_INPUT_PROP_NORMAL,
  RUT_BUTTON_INPUT_PROP_HOVER,
  RUT_BUTTON_INPUT_PROP_ACTIVE,
  RUT_BUTTON_INPUT_PROP_ACTIVE_CANCEL,
  RUT_BUTTON_INPUT_PROP_DISABLED,
  RUT_BUTTON_INPUT_N_PROPS
};

typedef enum _ButtonState
{
  BUTTON_STATE_NORMAL,
  BUTTON_STATE_HOVER,
  BUTTON_STATE_ACTIVE,
  BUTTON_STATE_ACTIVE_CANCEL,
  BUTTON_STATE_DISABLED
} ButtonState;

struct _RutButtonInput
{
  RutObjectProps _parent;
  int ref_count;

  RutContext *ctx;

  RutComponentableProps component;

  int press_counter;
  ButtonState state;

  RutSimpleIntrospectableProps introspectable;
  RutProperty properties[RUT_BUTTON_INPUT_N_PROPS];
};

static bool
_rut_button_input_get_normal_state (RutObject *object)
{
  RutButtonInput *input = object;
  return input->state == BUTTON_STATE_NORMAL;
}

static bool
_rut_button_input_get_hover_state (RutObject *object)
{
  RutButtonInput *input = object;
  return input->state == BUTTON_STATE_HOVER;
}

static bool
_rut_button_input_get_active_state (RutObject *object)
{
  RutButtonInput *input = object;
  return input->state == BUTTON_STATE_ACTIVE;
}

static bool
_rut_button_input_get_active_cancel_state (RutObject *object)
{
  RutButtonInput *input = object;
  return input->state == BUTTON_STATE_ACTIVE_CANCEL;
}

static bool
_rut_button_input_get_disabled_state (RutObject *object)
{
  RutButtonInput *input = object;
  return input->state == BUTTON_STATE_DISABLED;
}

static RutPropertySpec _rut_button_input_prop_specs[] = {
  {
    .name = "press_counter",
    .nick = "Press Counter",
    .type = RUT_PROPERTY_TYPE_INTEGER,
    .data_offset = G_STRUCT_OFFSET (RutButtonInput, press_counter),
    .flags = RUT_PROPERTY_FLAG_READABLE,
  },
  {
    .name = "normal",
    .nick = "Normal",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = _rut_button_input_get_normal_state,
    .flags = RUT_PROPERTY_FLAG_READABLE,
  },
  {
    .name = "hover",
    .nick = "Hover",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = _rut_button_input_get_hover_state,
    .flags = RUT_PROPERTY_FLAG_READABLE,
  },
  {
    .name = "active",
    .nick = "Active",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = _rut_button_input_get_active_state,
    .flags = RUT_PROPERTY_FLAG_READABLE,
  },
  {
    .name = "active_cancel",
    .nick = "Cancelling Activate",
    .blurb = "Cancelling an activation",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = _rut_button_input_get_active_cancel_state,
    .flags = RUT_PROPERTY_FLAG_READABLE,
  },
  {
    .name = "disabled",
    .nick = "Disabled",
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .getter.boolean_type = _rut_button_input_get_disabled_state,
    .flags = RUT_PROPERTY_FLAG_READABLE,
  },
  { NULL }
};

static void
_rut_button_input_free (void *object)
{
  RutButtonInput *button_input = object;

  rut_simple_introspectable_destroy (button_input);

  g_slice_free (RutButtonInput, object);
}

static RutObject *
_rut_button_input_copy (RutObject *object)
{
  RutButtonInput *button_input = object;

  return rut_button_input_new (button_input->ctx);
}

static int
get_prop_for_state (ButtonState state)
{
  switch (state)
    {
    case BUTTON_STATE_NORMAL:
      return RUT_BUTTON_INPUT_PROP_NORMAL;
    case BUTTON_STATE_HOVER:
      return RUT_BUTTON_INPUT_PROP_HOVER;
    case BUTTON_STATE_ACTIVE:
      return RUT_BUTTON_INPUT_PROP_ACTIVE;
    case BUTTON_STATE_ACTIVE_CANCEL:
      return RUT_BUTTON_INPUT_PROP_ACTIVE_CANCEL;
    case BUTTON_STATE_DISABLED:
      return RUT_BUTTON_INPUT_PROP_DISABLED;
    }

  g_return_val_if_fail (0, 0);
}

static void
set_state (RutButtonInput *button_input, ButtonState state)
{
  ButtonState prev_state = button_input->state;
  RutContext *ctx;
  int prev_prop;

  if (prev_state == state)
    return;

  button_input->state = state;

  prev_prop = get_prop_for_state (prev_state);
  ctx = button_input->ctx;
  rut_property_dirty (&ctx->property_ctx,
                      &button_input->properties[prev_prop]);
  rut_property_dirty (&ctx->property_ctx,
                      &button_input->properties[get_prop_for_state (state)]);
}

typedef struct _ButtonGrabState
{
  RutCamera *camera;
  RutButtonInput *button_input;
  CoglMatrix transform;
  CoglMatrix inverse_transform;
} ButtonGrabState;

static RutInputEventStatus
_rut_button_input_grab_input_cb (RutInputEvent *event,
                                 void *user_data)
{
  ButtonGrabState *state = user_data;
  RutButtonInput *button_input = state->button_input;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutContext *ctx = button_input->ctx;
      RutShell *shell = ctx->shell;
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell, _rut_button_input_grab_input_cb,
                                  user_data);
          g_slice_free (ButtonGrabState, state);

          button_input->press_counter++;
          rut_property_dirty (&ctx->property_ctx,
                              &button_input->properties[RUT_BUTTON_INPUT_PROP_PRESS_COUNT]);

          set_state (button_input, BUTTON_STATE_NORMAL);
          rut_shell_queue_redraw (button_input->ctx->shell);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) ==
               RUT_MOTION_EVENT_ACTION_MOVE)
        {
#warning "fixme: pick during button input grab to handle ACTIVE_CANCEL state"
          g_print ("TODO: RutButtonInput - pick during motion to handle ACTIVE_CANCEL state\n");
#if 0
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);
          RutCamera *camera = state->camera;

          rut_camera_unproject_coord (camera,
                                      &state->transform,
                                      &state->inverse_transform,
                                      0,
                                      &x,
                                      &y);

          if (x < 0 || x > button_input->width ||
              y < 0 || y > button_input->height)
            button_input->state = BUTTON_STATE_ACTIVE_CANCEL;
          else
            button_input->state = BUTTON_STATE_ACTIVE;

          rut_shell_queue_redraw (button_input->ctx->shell);
#endif
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_button_input_handle_event (RutObject *inputable,
                                RutInputEvent *event)
{
  RutButtonInput *button_input = inputable;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = button_input->ctx->shell;
      ButtonGrabState *state = g_slice_new (ButtonGrabState);
      const CoglMatrix *view;

      state->button_input = button_input;
      state->camera = rut_input_event_get_camera (event);
      view = rut_camera_get_view_transform (state->camera);
      state->transform = *view;

#if 0
      rut_graphable_apply_transform (button, &state->transform);
      if (!cogl_matrix_get_inverse (&state->transform,
                                    &state->inverse_transform))
        {
          g_warning ("Failed to calculate inverse of button transform\n");
          g_slice_free (ButtonGrabState, state);
          return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }
#endif

      rut_shell_grab_input (shell,
                            state->camera,
                            _rut_button_input_grab_input_cb,
                            state);
      //button_input->grab_x = rut_motion_event_get_x (event);
      //button_input->grab_y = rut_motion_event_get_y (event);

      set_state (button_input, BUTTON_STATE_ACTIVE);

      rut_shell_queue_redraw (button_input->ctx->shell);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutType rut_button_input_type;

static void
_rut_button_input_init_type (void)
{
  static RutComponentableVTable componentable_vtable = {
      .copy = _rut_button_input_copy
  };

  static RutIntrospectableVTable introspectable_vtable = {
      rut_simple_introspectable_lookup_property,
      rut_simple_introspectable_foreach_property
  };

  static RutInputableVTable inputable_vtable = {
      .handle_event = _rut_button_input_handle_event
  };

  RutType *type = &rut_button_input_type;
#define TYPE RutButtonInput

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_refable (type, ref_count, _rut_button_input_free);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_COMPONENTABLE,
                          offsetof (TYPE, component),
                          &componentable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &introspectable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (TYPE, introspectable),
                          NULL); /* no implied vtable */
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_INPUTABLE,
                          0, /* no implied properties */
                          &inputable_vtable);

#undef TYPE
}

RutButtonInput *
rut_button_input_new (RutContext *ctx)
{
  RutButtonInput *button_input =
    rut_object_alloc0 (RutButtonInput,
                       &rut_button_input_type,
                       _rut_button_input_init_type);

  button_input->ctx = ctx;

  button_input->ref_count = 1;

  button_input->component.type = RUT_COMPONENT_TYPE_INPUT;

  button_input->state = BUTTON_STATE_NORMAL;

  rut_simple_introspectable_init (button_input,
                                  _rut_button_input_prop_specs,
                                  button_input->properties);

  return button_input;
}
