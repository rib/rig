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

#include "rut-shell.h"
#include "rut-interfaces.h"
#include "rut-text.h"
#include "rut-transform.h"
#include "rut-stack.h"
#include "rut-box-layout.h"
#include "rut-icon-button.h"
#include "rut-bin.h"
#include "rut-icon.h"
#include "rut-composite-sizable.h"
#include "rut-input-region.h"

#include "rut-camera.h"

typedef enum _IconButtonState
{
  ICON_BUTTON_STATE_NORMAL,
  ICON_BUTTON_STATE_HOVER,
  ICON_BUTTON_STATE_ACTIVE,
  ICON_BUTTON_STATE_ACTIVE_CANCEL,
  ICON_BUTTON_STATE_DISABLED
} IconButtonState;

struct _RutIconButton
{
  RutObjectBase _base;

  RutContext *ctx;

  IconButtonState state;

  RutStack *stack;
  RutBoxLayout *layout;
  RutBin *bin;

  RutIcon *icon_normal;
  RutIcon *icon_hover;
  RutIcon *icon_active;
  RutIcon *icon_disabled;

  RutIcon *current_icon;

  RutText *label;
  RutIconButtonPosition label_position;

  RutInputRegion *input_region;

  RutList on_click_cb_list;

  RutGraphableProps graphable;
};

static void
destroy_icons (RutIconButton *button)
{
  if (button->icon_normal)
    {
      rut_object_unref (button->icon_normal);
      button->icon_normal = NULL;
    }

  if (button->icon_hover)
    {
      rut_object_unref (button->icon_hover);
      button->icon_hover = NULL;
    }

  if (button->icon_active)
    {
      rut_object_unref (button->icon_active);
      button->icon_active = NULL;
    }

  if (button->icon_disabled)
    {
      rut_object_unref (button->icon_disabled);
      button->icon_disabled = NULL;
    }
}

static void
_rut_icon_button_free (void *object)
{
  RutIconButton *button = object;

  rut_closure_list_disconnect_all (&button->on_click_cb_list);

  destroy_icons (button);

  /* NB: This will destroy the stack, layout, label and input_region
   * which we don't hold extra references for... */
  rut_graphable_destroy (button);

  rut_object_free (RutIconButton, object);
}

RutType rut_icon_button_type;

static void
_rut_icon_button_init_type (void)
{
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutSizableVTable sizable_vtable = {
      rut_composite_sizable_set_size,
      rut_composite_sizable_get_size,
      rut_composite_sizable_get_preferred_width,
      rut_composite_sizable_get_preferred_height,
      rut_composite_sizable_add_preferred_size_callback
  };

  RutType *type = &rut_icon_button_type;
#define TYPE RutIconButton

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_icon_button_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                      offsetof (TYPE, stack),
                      NULL); /* no vtable */

#undef TYPE
}

typedef struct _IconButtonGrabState
{
  RutObject *camera;
  RutIconButton *button;
  CoglMatrix transform;
  CoglMatrix inverse_transform;
} IconButtonGrabState;

static void
update_current_icon (RutIconButton *button)
{
  RutIcon *current;

  switch (button->state)
    {
      case ICON_BUTTON_STATE_NORMAL:
        current = button->icon_normal;
        break;
      case ICON_BUTTON_STATE_HOVER:
        current = button->icon_hover;
        break;
      case ICON_BUTTON_STATE_ACTIVE:
        current = button->icon_active;
        break;
      case ICON_BUTTON_STATE_ACTIVE_CANCEL:
        current = button->icon_normal;
        break;
      case ICON_BUTTON_STATE_DISABLED:
        current = button->icon_disabled;
        break;
    }

  if (button->current_icon != current)
    {
      if (button->current_icon)
        rut_bin_set_child (button->bin, NULL);
      rut_bin_set_child (button->bin, current);
      button->current_icon = current;
    }
}

static void
set_state (RutIconButton *button,
           IconButtonState state)
{
  if (button->state == state)
    return;

  button->state = state;
  update_current_icon (button);
}

static RutInputEventStatus
_rut_icon_button_grab_input_cb (RutInputEvent *event,
                                void *user_data)
{
  IconButtonGrabState *state = user_data;
  RutIconButton *button = state->button;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutShell *shell = button->ctx->shell;
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell, _rut_icon_button_grab_input_cb, user_data);

          /* NB: It's possible the click callbacks could result in the
           * button's last reference being released... */
          rut_object_ref (button);

          rut_closure_list_invoke (&button->on_click_cb_list,
                                   RutIconButtonClickCallback,
                                   button);

          g_slice_free (IconButtonGrabState, state);

          set_state (button, ICON_BUTTON_STATE_NORMAL);

          rut_object_unref (button);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) ==
               RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);
          float width, height;
          RutObject *camera = state->camera;

          rut_camera_unproject_coord (camera,
                                      &state->transform,
                                      &state->inverse_transform,
                                      0,
                                      &x,
                                      &y);

          rut_sizable_get_size (button, &width, &height);
          if (x < 0 || x > width || y < 0 || y > height)
            set_state (button, ICON_BUTTON_STATE_ACTIVE_CANCEL);
          else
            set_state (button, ICON_BUTTON_STATE_ACTIVE);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_icon_button_input_cb (RutInputRegion *region,
                           RutInputEvent *event,
                           void *user_data)
{
  RutIconButton *button = user_data;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
     rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = button->ctx->shell;
      IconButtonGrabState *state = g_slice_new (IconButtonGrabState);
      const CoglMatrix *view;

      state->button = button;
      state->camera = rut_input_event_get_camera (event);
      view = rut_camera_get_view_transform (state->camera);
      state->transform = *view;
      rut_graphable_apply_transform (button, &state->transform);
      if (!cogl_matrix_get_inverse (&state->transform,
                                    &state->inverse_transform))
        {
          g_warning ("Failed to calculate inverse of button transform\n");
          g_slice_free (IconButtonGrabState, state);
          return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

      rut_shell_grab_input (shell,
                            state->camera,
                            _rut_icon_button_grab_input_cb,
                            state);

      set_state (button, ICON_BUTTON_STATE_ACTIVE);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
update_layout (RutIconButton *button)
{
  RutBoxLayoutPacking packing;

  switch (button->label_position)
    {
    case RUT_ICON_BUTTON_POSITION_ABOVE:
      packing = RUT_BOX_LAYOUT_PACKING_BOTTOM_TO_TOP;
      break;
    case RUT_ICON_BUTTON_POSITION_BELOW:
      packing = RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM;
      break;
    case RUT_ICON_BUTTON_POSITION_SIDE:
      {
        RutTextDirection dir = rut_get_text_direction (button->ctx);
        if (dir == RUT_TEXT_DIRECTION_LEFT_TO_RIGHT)
          packing = RUT_BOX_LAYOUT_PACKING_LEFT_TO_RIGHT;
        else
          packing = RUT_BOX_LAYOUT_PACKING_RIGHT_TO_LEFT;
        break;
      }
    }

  rut_box_layout_set_packing (button->layout, packing);
}

RutIconButton *
rut_icon_button_new (RutContext *ctx,
                     const char *label,
                     RutIconButtonPosition label_position,
                     const char *normal_icon,
                     const char *hover_icon,
                     const char *active_icon,
                     const char *disabled_icon)
{
  RutIconButton *button = rut_object_alloc0 (RutIconButton,
                                             &rut_icon_button_type,
                                             _rut_icon_button_init_type);
  float natural_width, natural_height;


  rut_list_init (&button->on_click_cb_list);

  rut_graphable_init (button);

  button->ctx = ctx;

  button->state = ICON_BUTTON_STATE_NORMAL;

  button->stack = rut_stack_new (ctx, 100, 100);
  rut_graphable_add_child (button, button->stack);
  rut_object_unref (button->stack);

  button->layout = rut_box_layout_new (ctx, RUT_BOX_LAYOUT_PACKING_TOP_TO_BOTTOM);
  rut_stack_add (button->stack, button->layout);
  rut_object_unref (button->layout);

  button->bin = rut_bin_new (ctx);
  rut_box_layout_add (button->layout, TRUE, button->bin);
  rut_object_unref (button->bin);

  button->label_position = label_position;

  if (label)
    {
      RutBin *bin = rut_bin_new (ctx);

      rut_bin_set_x_position (bin, RUT_BIN_POSITION_CENTER);

      button->label = rut_text_new_with_text (ctx, NULL, label);
      rut_bin_set_child (bin, button->label);
      rut_object_unref (button->label);

      rut_box_layout_add (button->layout, FALSE, bin);
      rut_object_unref (bin);

      update_layout (button);
    }

  rut_icon_button_set_normal (button, normal_icon);
  rut_icon_button_set_hover (button, hover_icon);
  rut_icon_button_set_active (button, active_icon);
  rut_icon_button_set_disabled (button, disabled_icon);

  button->input_region =
    rut_input_region_new_rectangle (0, 0, 100, 100,
                                    _rut_icon_button_input_cb,
                                    button);
  rut_stack_add (button->stack, button->input_region);
  rut_object_unref (button->input_region);

  rut_sizable_get_preferred_width (button->stack, -1, NULL,
                                   &natural_width);
  rut_sizable_get_preferred_height (button->stack, natural_width, NULL,
                                    &natural_height);
  rut_sizable_set_size (button->stack, natural_width, natural_height);

  return button;
}

RutClosure *
rut_icon_button_add_on_click_callback (RutIconButton *button,
                                  RutIconButtonClickCallback callback,
                                  void *user_data,
                                  RutClosureDestroyCallback destroy_cb)
{
  g_return_val_if_fail (callback != NULL, NULL);

  return rut_closure_list_add (&button->on_click_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

static void
set_icon (RutIconButton *button,
          RutIcon **icon,
          const char *icon_name)
{
  if (*icon)
    {
      rut_object_unref (*icon);
      if (button->current_icon == *icon)
        {
          rut_bin_set_child (button->bin, NULL);
          button->current_icon = NULL;
        }
    }

  *icon = rut_icon_new (button->ctx, icon_name);
  update_current_icon (button);
}

void
rut_icon_button_set_normal (RutIconButton *button,
                            const char *icon)
{
  set_icon (button, &button->icon_normal, icon);
}

void
rut_icon_button_set_hover (RutIconButton *button,
                           const char *icon)
{
  set_icon (button, &button->icon_hover, icon);
}

void
rut_icon_button_set_active (RutIconButton *button,
                            const char *icon)
{
  set_icon (button, &button->icon_active, icon);
}

void
rut_icon_button_set_disabled (RutIconButton *button,
                              const char *icon)
{
  set_icon (button, &button->icon_disabled, icon);
}

void
rut_icon_button_set_label_color (RutIconButton *button,
                                 const CoglColor *color)
{
  if (button->label)
    rut_text_set_color (button->label, color);
}
