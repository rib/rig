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
#include "rut-paintable.h"
#include "components/rut-camera.h"
#include "rut-transform.h"
#include "rut-nine-slice.h"
#include "rut-button.h"

#define BUTTON_HPAD 10
#define BUTTON_VPAD 23

typedef enum _ButtonState
{
  BUTTON_STATE_NORMAL,
  BUTTON_STATE_HOVER,
  BUTTON_STATE_ACTIVE,
  BUTTON_STATE_ACTIVE_CANCEL,
  BUTTON_STATE_DISABLED
} ButtonState;

struct _RutButton
{
  RutObjectProps _parent;
  int ref_count;

  RutContext *ctx;

  ButtonState state;

  RutTransform *text_transform;
  RutText *text;

  float width;
  float height;

  CoglTexture *normal_texture;
  CoglTexture *hover_texture;
  CoglTexture *active_texture;
  CoglTexture *disabled_texture;

  RutNineSlice *background_normal;
  RutNineSlice *background_hover;
  RutNineSlice *background_active;
  RutNineSlice *background_disabled;

  CoglColor text_color;

  RutInputRegion *input_region;

  RutList on_click_cb_list;

  RutGraphableProps graphable;
  RutPaintableProps paintable;
};


static void
destroy_button_slices (RutButton *button)
{
  if (button->background_normal)
    {
      rut_refable_unref (button->background_normal);
      button->background_normal = NULL;
    }

  if (button->background_hover)
    {
      rut_refable_unref (button->background_hover);
      button->background_hover = NULL;
    }

  if (button->background_active)
    {
      rut_refable_unref (button->background_active);
      button->background_active = NULL;
    }

  if (button->background_disabled)
    {
      rut_refable_unref (button->background_disabled);
      button->background_disabled = NULL;
    }
}

static void
_rut_button_free (void *object)
{
  RutButton *button = object;

  rut_closure_list_disconnect_all (&button->on_click_cb_list);

  destroy_button_slices (button);

  if (button->normal_texture)
    {
      rut_refable_unref (button->normal_texture);
      button->normal_texture = NULL;
    }

  if (button->hover_texture)
    {
      rut_refable_unref (button->hover_texture);
      button->hover_texture = NULL;
    }

  if (button->active_texture)
    {
      rut_refable_unref (button->active_texture);
      button->active_texture = NULL;
    }

  if (button->disabled_texture)
    {
      rut_refable_unref (button->disabled_texture);
      button->disabled_texture = NULL;
    }

  rut_graphable_remove_child (button->text);
  rut_refable_unref (button->text);

  rut_graphable_remove_child (button->text_transform);
  rut_refable_unref (button->text_transform);

  rut_graphable_destroy (button);

  g_slice_free (RutButton, object);
}

static void
_rut_button_paint (RutObject *object,
                   RutPaintContext *paint_ctx)
{
  RutButton *button = object;

  switch (button->state)
    {
    case BUTTON_STATE_NORMAL:
      rut_nine_slice_set_size (button->background_normal,
                               button->width,
                               button->height);
      rut_paintable_paint (button->background_normal, paint_ctx);
      break;
    case BUTTON_STATE_HOVER:
      rut_nine_slice_set_size (button->background_hover,
                               button->width,
                               button->height);
      rut_paintable_paint (button->background_hover, paint_ctx);
      break;
    case BUTTON_STATE_ACTIVE:
      rut_nine_slice_set_size (button->background_active,
                               button->width,
                               button->height);
      rut_paintable_paint (button->background_active, paint_ctx);
      break;
    case BUTTON_STATE_ACTIVE_CANCEL:
      rut_nine_slice_set_size (button->background_active,
                               button->width,
                               button->height);
      rut_paintable_paint (button->background_active, paint_ctx);
      break;
    case BUTTON_STATE_DISABLED:
      rut_nine_slice_set_size (button->background_disabled,
                               button->width,
                               button->height);
      rut_paintable_paint (button->background_disabled, paint_ctx);
      break;
    }
}

static void
rut_button_get_preferred_width (void *object,
                                float for_height,
                                float *min_width_p,
                                float *natural_width_p)
{
  RutButton *button = object;

  rut_sizable_get_preferred_width (button->text,
                                   for_height,
                                   min_width_p,
                                   natural_width_p);

  if (min_width_p)
    *min_width_p += BUTTON_HPAD;
  if (natural_width_p)
    *natural_width_p += BUTTON_HPAD;
}

static void
rut_button_get_preferred_height (void *object,
                                 float for_width,
                                 float *min_height_p,
                                 float *natural_height_p)
{
  RutButton *button = object;

  rut_sizable_get_preferred_height (button->text,
                                    for_width,
                                    min_height_p,
                                    natural_height_p);

  if (min_height_p)
    *min_height_p += BUTTON_VPAD;
  if (natural_height_p)
    *natural_height_p += BUTTON_VPAD;
}

RutType rut_button_type;

static void
_rut_button_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_button_free
  };

  static RutGraphableVTable graphable_vtable = {
      NULL, /* child remove */
      NULL, /* child add */
      NULL /* parent changed */
  };

  static RutPaintableVTable paintable_vtable = {
      _rut_button_paint
  };

  static RutSizableVTable sizable_vtable = {
      rut_button_set_size,
      rut_button_get_size,
      rut_button_get_preferred_width,
      rut_button_get_preferred_height,
      NULL /* add_preferred_size_callback */
  };

  RutType *type = &rut_button_type;
#define TYPE RutButton

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (TYPE, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (TYPE, paintable),
                          &paintable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);

#undef TYPE
}

typedef struct _ButtonGrabState
{
  RutCamera *camera;
  RutButton *button;
  CoglMatrix transform;
  CoglMatrix inverse_transform;
} ButtonGrabState;

static RutInputEventStatus
_rut_button_grab_input_cb (RutInputEvent *event,
                           void *user_data)
{
  ButtonGrabState *state = user_data;
  RutButton *button = state->button;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutShell *shell = button->ctx->shell;
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell, _rut_button_grab_input_cb, user_data);

          rut_closure_list_invoke (&button->on_click_cb_list,
                                   RutButtonClickCallback,
                                   button);

          g_slice_free (ButtonGrabState, state);

          button->state = BUTTON_STATE_NORMAL;
          rut_shell_queue_redraw (button->ctx->shell);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) ==
               RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float x = rut_motion_event_get_x (event);
          float y = rut_motion_event_get_y (event);
          RutCamera *camera = state->camera;

          rut_camera_unproject_coord (camera,
                                      &state->transform,
                                      &state->inverse_transform,
                                      0,
                                      &x,
                                      &y);

          if (x < 0 || x > button->width ||
              y < 0 || y > button->height)
            button->state = BUTTON_STATE_ACTIVE_CANCEL;
          else
            button->state = BUTTON_STATE_ACTIVE;

          rut_shell_queue_redraw (button->ctx->shell);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_button_input_cb (RutInputRegion *region,
                      RutInputEvent *event,
                      void *user_data)
{
  RutButton *button = user_data;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
     rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = button->ctx->shell;
      ButtonGrabState *state = g_slice_new (ButtonGrabState);
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
          g_slice_free (ButtonGrabState, state);
          return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }

      rut_shell_grab_input (shell,
                            state->camera,
                            _rut_button_grab_input_cb,
                            state);
      //button->grab_x = rut_motion_event_get_x (event);
      //button->grab_y = rut_motion_event_get_y (event);

      button->state = BUTTON_STATE_ACTIVE;
      rut_shell_queue_redraw (button->ctx->shell);

      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
_rut_button_allocate_cb (RutObject *graphable,
                         void *user_data)
{
  RutButton *button = graphable;
  float text_min_width, text_natural_width;
  float text_min_height, text_natural_height;
  int text_width, text_height;
  int text_x, text_y;

  rut_sizable_get_preferred_width (button->text,
                                   -1,
                                   &text_min_width,
                                   &text_natural_width);

  rut_sizable_get_preferred_height (button->text,
                                    -1,
                                    &text_min_height,
                                    &text_natural_height);

  if (button->width > (BUTTON_HPAD + text_natural_width))
    text_width = text_natural_width;
  else
    text_width = MAX (0, button->width - BUTTON_HPAD);

  if (button->height > (BUTTON_VPAD + text_natural_height))
    text_height = text_natural_height;
  else
    text_height = MAX (0, button->height - BUTTON_VPAD);

  rut_sizable_set_size (button->text, text_width, text_height);

  rut_transform_init_identity (button->text_transform);

  text_x = (button->width / 2) - (text_width / 2.0);
  text_y = (button->height / 2) - (text_height / 2.0);
  rut_transform_translate (button->text_transform, text_x, text_y, 0);
}

static void
queue_allocation (RutButton *button)
{
  rut_shell_add_pre_paint_callback (button->ctx->shell,
                                    button,
                                    _rut_button_allocate_cb,
                                    NULL /* user_data */);
}

RutButton *
rut_button_new (RutContext *ctx,
                const char *label)
{
  RutButton *button = g_slice_new0 (RutButton);
  GError *error = NULL;
  float text_width, text_height;
  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rut_button_init_type ();
      initialized = TRUE;
    }

  rut_object_init (RUT_OBJECT (button), &rut_button_type);

  button->ref_count = 1;

  rut_list_init (&button->on_click_cb_list);

  rut_graphable_init (RUT_OBJECT (button));
  rut_paintable_init (RUT_OBJECT (button));

  button->ctx = ctx;

  button->state = BUTTON_STATE_NORMAL;

  button->normal_texture =
    rut_load_texture_from_data_file (ctx, "button.png", &error);
  if (button->normal_texture)
    {
      button->background_normal =
        rut_nine_slice_new (ctx, button->normal_texture, 11, 5, 13, 5,
                            button->width,
                            button->height);
    }
  else
    {
      g_warning ("Failed to load button texture: %s", error->message);
      g_error_free (error);
    }

  button->hover_texture =
    rut_load_texture_from_data_file (ctx, "button-hover.png", &error);
  if (button->hover_texture)
    {
      button->background_hover =
        rut_nine_slice_new (ctx, button->hover_texture, 11, 5, 13, 5,
                            button->width,
                            button->height);
    }
  else
    {
      g_warning ("Failed to load button-hover texture: %s", error->message);
      g_error_free (error);
    }

  button->active_texture =
    rut_load_texture_from_data_file (ctx, "button-active.png", &error);
  if (button->active_texture)
    {
      button->background_active =
        rut_nine_slice_new (ctx, button->active_texture, 11, 5, 13, 5,
                            button->width,
                            button->height);
    }
  else
    {
      g_warning ("Failed to load button-active texture: %s", error->message);
      g_error_free (error);
    }

  button->disabled_texture =
    rut_load_texture_from_data_file (ctx, "button-disabled.png", &error);
  if (button->disabled_texture)
    {
      button->background_disabled =
        rut_nine_slice_new (ctx, button->disabled_texture, 11, 5, 13, 5,
                            button->width,
                            button->height);
    }
  else
    {
      g_warning ("Failed to load button-disabled texture: %s", error->message);
      g_error_free (error);
    }

  button->text = rut_text_new_with_text (ctx, NULL, label);
  button->text_transform = rut_transform_new (ctx);
  rut_graphable_add_child (button, button->text_transform);
  rut_graphable_add_child (button->text_transform, button->text);

  rut_sizable_get_size (button->text, &text_width, &text_height);
  button->width = text_width + BUTTON_HPAD;
  button->height = text_height + BUTTON_VPAD;

  cogl_color_init_from_4f (&button->text_color, 0, 0, 0, 1);

  button->input_region =
    rut_input_region_new_rectangle (0, 0, button->width, button->height,
                                    _rut_button_input_cb,
                                    button);

  //rut_input_region_set_graphable (button->input_region, button);
  rut_graphable_add_child (button, button->input_region);

  queue_allocation (button);

  return button;
}

RutClosure *
rut_button_add_on_click_callback (RutButton *button,
                                  RutButtonClickCallback callback,
                                  void *user_data,
                                  RutClosureDestroyCallback destroy_cb)
{
  g_return_val_if_fail (callback != NULL, NULL);

  return rut_closure_list_add (&button->on_click_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

void
rut_button_set_size (RutObject *self,
                     float width,
                     float height)
{
  RutButton *button = self;

  if (button->width == width &&
      button->height == height)
    return;

  button->width = width;
  button->height = height;

  rut_input_region_set_rectangle (button->input_region,
                                  0, 0, button->width, button->height);

  queue_allocation (button);
}

void
rut_button_get_size (RutObject *self,
                     float *width,
                     float *height)
{
  RutButton *button = self;
  *width = button->width;
  *height = button->height;
}
