/*
 * Rig
 *
 * Copyright (C) 2013 Intel Corporation.
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

#include <math.h>

#include "rut.h"
#include "rut-bin.h"
#include "rut-shim.h"

#include "rut-drag-bin.h"

struct _RutDragBin
{
  RutObjectProps _parent;

  RutContext *ctx;

  int ref_count;

  RutObject *child;
  RutObject *payload;

  RutStack *stack;
  //RutInputRegion *input_region;
  RutBin *bin;
  RutRectangle *drag_overlay;

  RutTransform *transform;
  RutNineSlice *drag_icon;

  bool in_drag;

  RutGraphableProps graphable;
  RutInputableProps inputable;
};

RutType rut_drag_bin_type;

static void
_rut_drag_bin_free (void *object)
{
  RutDragBin *bin = object;

  rut_drag_bin_set_child (bin, NULL);
  rut_refable_unref (bin->payload);

  if (!bin->in_drag)
    {
      rut_refable_unref (bin->drag_overlay);
      rut_refable_unref (bin->transform);
    }

  rut_shell_remove_pre_paint_callback (bin->ctx->shell, bin);

  rut_graphable_destroy (bin);

  rut_refable_unref (bin->inputable.input_region);

  g_slice_free (RutDragBin, bin);
}

#if 1
static void
_rut_drag_bin_set_size (RutObject *object, float width, float height)
{
  RutDragBin *bin = object;

  rut_sizable_set_size (bin->inputable.input_region,
                        width, height);
  rut_composite_sizable_set_size (bin, width, height);
}
#endif

static void
_rut_drag_bin_init_type (void)
{
  static RutRefableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_drag_bin_free
  };
  static RutGraphableVTable graphable_vtable = {
      NULL, /* child removed */
      NULL, /* child addded */
      NULL /* parent changed */
  };
  static RutSizableVTable sizable_vtable = {
      _rut_drag_bin_set_size,
      //rut_composite_sizable_set_size,
      rut_composite_sizable_get_size,
      rut_composite_sizable_get_preferred_width,
      rut_composite_sizable_get_preferred_height,
      rut_composite_sizable_add_preferred_size_callback
  };

  RutType *type = &rut_drag_bin_type;
#define TYPE RutDragBin

  rut_type_init (&rut_drag_bin_type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutDragBin, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutDragBin, graphable),
                          &graphable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_SIZABLE,
                          0, /* no implied properties */
                          &sizable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_COMPOSITE_SIZABLE,
                          offsetof (TYPE, stack),
                          NULL); /* no vtable */
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_INPUTABLE,
                          offsetof (TYPE, inputable),
                          NULL); /* no implied vtable */

#undef TYPE
}

static void
start_drag (RutDragBin *bin)
{
  RutObject *root;

  if (bin->in_drag)
    return;

  rut_stack_add (bin->stack, bin->drag_overlay);

  root = rut_graphable_get_root (bin);
  rut_stack_add (root, bin->transform);

  rut_shell_start_drag (bin->ctx->shell, bin->payload);
  rut_shell_queue_redraw (bin->ctx->shell);
  bin->in_drag = TRUE;
}

static void
cancel_drag (RutDragBin *bin)
{
  if (!bin->in_drag)
    return;

  rut_graphable_remove_child (bin->drag_overlay);
  rut_graphable_remove_child (bin->transform);

  rut_shell_cancel_drag (bin->ctx->shell);
  rut_shell_queue_redraw (bin->ctx->shell);
  bin->in_drag = FALSE;
}

typedef struct _DragState
{
  RutCamera *camera;
  RutDragBin *bin;
  float press_x;
  float press_y;
} DragState;

static RutInputEventStatus
_rut_drag_bin_grab_input_cb (RutInputEvent *event,
                             void *user_data)
{
  DragState *state = user_data;
  RutDragBin *bin = state->bin;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (bin->ctx->shell,
                                  _rut_drag_bin_grab_input_cb,
                                  state);

          g_slice_free (DragState, state);

          if (bin->in_drag)
            {
              rut_shell_drop (bin->ctx->shell);
              cancel_drag (bin);
              return RUT_INPUT_EVENT_STATUS_HANDLED;
            }
          else
            return RUT_INPUT_EVENT_STATUS_UNHANDLED;
        }
      else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float dx = rut_motion_event_get_x (event) - state->press_x;
          float dy = rut_motion_event_get_y (event) - state->press_y;
          float dist = sqrtf (dx * dx + dy * dy);

          if (dist > 20)
            {
              CoglMatrix transform;

              start_drag (bin);

              rut_graphable_get_transform (bin, &transform);

              rut_transform_init_identity (bin->transform);
              rut_transform_transform (bin->transform, &transform);

              rut_transform_translate (bin->transform, dx, dy, 0);
            }
          else
            cancel_drag (bin);

          rut_shell_queue_redraw (bin->ctx->shell);
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_drag_bin_input_cb (RutInputRegion *region,
                        RutInputEvent *event,
                        void *user_data)
{
  RutDragBin *bin = user_data;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN &&
          rut_motion_event_get_button_state (event) == RUT_BUTTON_STATE_1)
        {
          DragState *state = g_slice_new (DragState);

          state->bin = bin;
          state->camera = rut_input_event_get_camera (event);
          state->press_x = rut_motion_event_get_x (event);
          state->press_y = rut_motion_event_get_y (event);

          rut_shell_grab_input (bin->ctx->shell,
                                rut_input_event_get_camera (event),
                                _rut_drag_bin_grab_input_cb,
                                state);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutDragBin *
rut_drag_bin_new (RutContext *ctx)
{
  RutDragBin *bin = rut_object_alloc0 (RutDragBin,
                                       &rut_drag_bin_type,
                                       _rut_drag_bin_init_type);

  bin->ref_count = 1;
  bin->ctx = ctx;

  rut_graphable_init (bin);

  bin->in_drag = FALSE;

  bin->stack = rut_stack_new (ctx, 1, 1);
  rut_graphable_add_child (bin, bin->stack);
  rut_refable_unref (bin->stack);

  bin->inputable.input_region =
    rut_input_region_new_rectangle (0, 0, 1, 1,
                                    _rut_drag_bin_input_cb,
                                    bin);

  bin->bin = rut_bin_new (ctx);
  rut_stack_add (bin->stack, bin->bin);
  rut_refable_unref (bin->bin);

  bin->drag_overlay = rut_rectangle_new4f (ctx, 1, 1, 0.5, 0.5, 0.5, 0.5);

  bin->transform = rut_transform_new (ctx);
  bin->drag_icon =
    rut_nine_slice_new (ctx,
                        rut_load_texture_from_data_file (ctx,
                                                         "transparency-grid.png",
                                                         NULL),
                        0, 0, 0, 0,
                        100, 100);
  rut_graphable_add_child (bin->transform, bin->drag_icon);
  rut_refable_unref (bin->drag_icon);

  return bin;
}

void
rut_drag_bin_set_child (RutDragBin *bin,
                        RutObject *child_widget)
{
  g_return_if_fail (rut_object_get_type (bin) == &rut_drag_bin_type);
  g_return_if_fail (bin->in_drag == FALSE);

  if (bin->child == child_widget)
    return;

  if (bin->child)
    {
      rut_graphable_remove_child (bin->child);
      rut_refable_unref (bin->child);
    }

  bin->child = child_widget;

  if (child_widget)
    {
      rut_bin_set_child (bin->bin, child_widget);
      rut_refable_ref (child_widget);
    }

  rut_shell_queue_redraw (bin->ctx->shell);
}

void
rut_drag_bin_set_payload (RutDragBin *bin,
                          RutObject *payload)
{
  if (bin->payload == payload)
    return;

  if (bin->payload)
    rut_refable_unref (bin->payload);

  bin->payload = payload;
  if (payload)
    rut_refable_ref (payload);
}
