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
#include "rut-icon-toggle.h"
#include "rut-bin.h"
#include "rut-icon.h"
#include "rut-composite-sizable.h"
#include "rut-input-region.h"

#include "components/rut-camera.h"

struct _RutIconToggle
{
  RutObjectBase _base;

  RutContext *ctx;

  bool visual_state;
  bool real_state;

  RutStack *stack;
  RutBin *bin;

  RutIcon *icon_set;
  RutIcon *icon_unset;

  RutIcon *current_icon;

  RutInputRegion *input_region;
  bool in_grab;

  bool interactive_unset_enabled;

  RutList on_toggle_cb_list;

  RutGraphableProps graphable;
};

static void
destroy_icons (RutIconToggle *toggle)
{
  if (toggle->icon_set)
    {
      rut_object_unref (toggle->icon_set);
      toggle->icon_set = NULL;
    }

  if (toggle->icon_unset)
    {
      rut_object_unref (toggle->icon_unset);
      toggle->icon_unset = NULL;
    }
}

static void
_rut_icon_toggle_free (void *object)
{
  RutIconToggle *toggle = object;

  rut_closure_list_disconnect_all (&toggle->on_toggle_cb_list);

  destroy_icons (toggle);

  /* NB: This will destroy the stack, layout, label and input_region
   * which we don't hold extra references for... */
  rut_graphable_destroy (toggle);

  rut_object_free (RutIconToggle, object);
}

#if 0
static RutPropertySpec
_rut_icon_toggle_prop_specs[] =
{
  {
    .name = "state",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_BOOLEAN,
    .data_offset = offsetof (RutIcnToggle, state),
    .setter.boolean_type = rut_icon_toggle_set_state
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};
#endif

RutType rut_icon_toggle_type;

static void
_rut_icon_toggle_init_type (void)
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

  RutType *type = &rut_icon_toggle_type;
#define TYPE RutIconToggle

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_icon_toggle_free);
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

typedef struct _IconToggleGrabState
{
  RutCamera *camera;
  RutIconToggle *toggle;
  CoglMatrix transform;
  CoglMatrix inverse_transform;
} IconToggleGrabState;

static void
update_current_icon (RutIconToggle *toggle)
{
  RutIcon *current;

  if (toggle->visual_state)
    current = toggle->icon_set;
  else
    current = toggle->icon_unset;

  if (toggle->current_icon != current)
    {
      if (toggle->current_icon)
        rut_bin_set_child (toggle->bin, NULL);
      rut_bin_set_child (toggle->bin, current);
      toggle->current_icon = current;
    }
}

static void
set_visual_state (RutIconToggle *toggle, bool state)
{
  if (toggle->visual_state == state)
    return;

  toggle->visual_state = state;

  update_current_icon (toggle);
}

static RutInputEventStatus
_rut_icon_toggle_grab_input_cb (RutInputEvent *event,
                                void *user_data)
{
  IconToggleGrabState *state = user_data;
  RutIconToggle *toggle = state->toggle;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutShell *shell = toggle->ctx->shell;
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell, _rut_icon_toggle_grab_input_cb, user_data);
          toggle->in_grab = false;

          rut_icon_toggle_set_state (toggle, toggle->visual_state);

          rut_closure_list_invoke (&toggle->on_toggle_cb_list,
                                   RutIconToggleCallback,
                                   toggle,
                                   toggle->real_state);

          g_slice_free (IconToggleGrabState, state);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) ==
               RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);
          float width, height;
          RutCamera *camera = state->camera;

          rut_camera_unproject_coord (camera,
                                      &state->transform,
                                      &state->inverse_transform,
                                      0,
                                      &x,
                                      &y);

          rut_sizable_get_size (toggle, &width, &height);
          if (x < 0 || x > width || y < 0 || y > height)
            set_visual_state (toggle, toggle->real_state);
          else
            set_visual_state (toggle, !toggle->real_state);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_icon_toggle_input_cb (RutInputRegion *region,
                           RutInputEvent *event,
                           void *user_data)
{
  RutIconToggle *toggle = user_data;

  if (!toggle->interactive_unset_enabled && toggle->real_state == true)
    return RUT_INPUT_EVENT_STATUS_UNHANDLED;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
     rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = toggle->ctx->shell;
      IconToggleGrabState *state = g_slice_new (IconToggleGrabState);
      const CoglMatrix *view;

      state->toggle = toggle;
      state->camera = rut_input_event_get_camera (event);
      view = rut_camera_get_view_transform (state->camera);
      state->transform = *view;
      rut_graphable_apply_transform (toggle, &state->transform);
      if (!cogl_matrix_get_inverse (&state->transform,
                                    &state->inverse_transform))
        {
          g_warning ("Failed to calculate inverse of toggle transform\n");
          g_slice_free (IconToggleGrabState, state);
          return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

      toggle->in_grab = true;
      rut_shell_grab_input (shell,
                            state->camera,
                            _rut_icon_toggle_grab_input_cb,
                            state);

      set_visual_state (toggle, !toggle->real_state);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutIconToggle *
rut_icon_toggle_new (RutContext *ctx,
                     const char *set_icon,
                     const char *unset_icon)
{
  RutIconToggle *toggle = rut_object_alloc0 (RutIconToggle,
                                             &rut_icon_toggle_type,
                                             _rut_icon_toggle_init_type);
  float natural_width, natural_height;


  rut_list_init (&toggle->on_toggle_cb_list);

  rut_graphable_init (toggle);

  toggle->ctx = ctx;

  toggle->interactive_unset_enabled = true;

  toggle->real_state = false;
  toggle->visual_state = false;

  toggle->stack = rut_stack_new (ctx, 1, 1);
  rut_graphable_add_child (toggle, toggle->stack);
  rut_object_unref (toggle->stack);

  toggle->bin = rut_bin_new (ctx);
  rut_stack_add (toggle->stack, toggle->bin);
  rut_object_unref (toggle->bin);

  rut_icon_toggle_set_set_icon (toggle, set_icon);
  rut_icon_toggle_set_unset_icon (toggle, unset_icon);

  toggle->input_region =
    rut_input_region_new_rectangle (0, 0, 100, 100,
                                    _rut_icon_toggle_input_cb,
                                    toggle);
  rut_stack_add (toggle->stack, toggle->input_region);
  rut_object_unref (toggle->input_region);

  rut_sizable_get_preferred_width (toggle->stack, -1, NULL,
                                   &natural_width);
  rut_sizable_get_preferred_height (toggle->stack, natural_width, NULL,
                                    &natural_height);
  rut_sizable_set_size (toggle->stack, natural_width, natural_height);

  return toggle;
}

RutClosure *
rut_icon_toggle_add_on_toggle_callback (RutIconToggle *toggle,
                                  RutIconToggleCallback callback,
                                  void *user_data,
                                  RutClosureDestroyCallback destroy_cb)
{
  g_return_val_if_fail (callback != NULL, NULL);

  return rut_closure_list_add (&toggle->on_toggle_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

static void
set_icon (RutIconToggle *toggle,
          RutIcon **icon,
          const char *icon_name)
{
  if (*icon)
    {
      rut_object_unref (*icon);
      if (toggle->current_icon == *icon)
        {
          rut_bin_set_child (toggle->bin, NULL);
          toggle->current_icon = NULL;
        }
    }

  *icon = rut_icon_new (toggle->ctx, icon_name);
  update_current_icon (toggle);
}

void
rut_icon_toggle_set_set_icon (RutIconToggle *toggle,
                              const char *icon)
{
  set_icon (toggle, &toggle->icon_set, icon);
}

void
rut_icon_toggle_set_unset_icon (RutIconToggle *toggle,
                                const char *icon)
{
  set_icon (toggle, &toggle->icon_unset, icon);
}

void
rut_icon_toggle_set_state (RutObject *object,
                           bool state)
{
  RutIconToggle *toggle = object;
  if (toggle->real_state == state)
    return;

  toggle->real_state = state;

  if (toggle->in_grab)
    set_visual_state (toggle, !toggle->visual_state);
  else
    set_visual_state (toggle, state);

  update_current_icon (toggle);
}

void
rut_icon_toggle_set_interactive_unset_enable (RutIconToggle *toggle,
                                              bool enabled)
{
  toggle->interactive_unset_enabled = enabled;
}
