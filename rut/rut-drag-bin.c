/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <config.h>

#include <math.h>

#include "rut-bin.h"
#include "rut-shim.h"
#include "rut-stack.h"
#include "rut-rectangle.h"
#include "rut-transform.h"
#include "rut-drag-bin.h"
#include "rut-interfaces.h"
#include "rut-composite-sizable.h"
#include "rut-inputable.h"
#include "rut-pickable.h"
#include "rut-input-region.h"

#include "rut-nine-slice.h"
#include "rut-camera.h"

struct _RutDragBin
{
  RutObjectBase _base;

  RutContext *ctx;


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

  RutInputRegion *input_region;
};

RutType rut_drag_bin_type;

static void
_rut_drag_bin_free (void *object)
{
  RutDragBin *bin = object;

  rut_drag_bin_set_child (bin, NULL);
  rut_object_unref (bin->payload);

  if (!bin->in_drag)
    {
      rut_object_unref (bin->drag_overlay);
      rut_object_unref (bin->transform);
    }

  rut_shell_remove_pre_paint_callback_by_graphable (bin->ctx->shell, bin);

  rut_graphable_destroy (bin);

  rut_object_unref (bin->input_region);

  rut_object_free (RutDragBin, bin);
}

#if 1
static void
_rut_drag_bin_set_size (RutObject *object, float width, float height)
{
  RutDragBin *bin = object;

  rut_sizable_set_size (bin->input_region, width, height);
  rut_composite_sizable_set_size (bin, width, height);
}
#endif

static bool
_rut_drag_bin_pick (RutObject *inputable,
                    RutObject *camera,
                    const CoglMatrix *modelview,
                    float x,
                    float y)
{
  RutDragBin *bin = inputable;
  CoglMatrix matrix;

  if (!modelview)
    {
      matrix = *rut_camera_get_view_transform (camera);
      rut_graphable_apply_transform (inputable, &matrix);
      modelview = &matrix;
    }

  return rut_pickable_pick (bin->input_region,
                             camera, modelview, x, y);
}

static RutInputEventStatus
_rut_drag_bin_handle_event (RutObject *inputable,
                            RutInputEvent *event)
{
  RutDragBin *bin = inputable;
  return rut_inputable_handle_event (bin->input_region, event);
}

static void
_rut_drag_bin_init_type (void)
{
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
  static RutPickableVTable pickable_vtable = {
      _rut_drag_bin_pick,
  };
  static RutInputableVTable inputable_vtable = {
      _rut_drag_bin_handle_event
  };

  RutType *type = &rut_drag_bin_type;
#define TYPE RutDragBin

  rut_type_init (&rut_drag_bin_type, C_STRINGIFY (TYPE), _rut_drag_bin_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (RutDragBin, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_SIZABLE,
                      0, /* no implied properties */
                      &sizable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_COMPOSITE_SIZABLE,
                      offsetof (TYPE, stack),
                      NULL); /* no vtable */
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_PICKABLE,
                      0, /* no implied properties */
                      &pickable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INPUTABLE,
                      0, /* no implied properties */
                      &inputable_vtable);

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
  bin->in_drag = true;
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
  bin->in_drag = false;
}

typedef struct _DragState
{
  RutObject *camera;
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

          c_slice_free (DragState, state);

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

          return RUT_INPUT_EVENT_STATUS_HANDLED;
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
          DragState *state = c_slice_new (DragState);

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

  bin->ctx = ctx;

  rut_graphable_init (bin);

  bin->in_drag = false;

  bin->stack = rut_stack_new (ctx, 1, 1);
  rut_graphable_add_child (bin, bin->stack);
  rut_object_unref (bin->stack);

  bin->input_region =
    rut_input_region_new_rectangle (0, 0, 1, 1,
                                    _rut_drag_bin_input_cb,
                                    bin);

  bin->bin = rut_bin_new (ctx);
  rut_stack_add (bin->stack, bin->bin);
  rut_object_unref (bin->bin);

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
  rut_object_unref (bin->drag_icon);

  return bin;
}

void
rut_drag_bin_set_child (RutDragBin *bin,
                        RutObject *child_widget)
{
  c_return_if_fail (rut_object_get_type (bin) == &rut_drag_bin_type);
  c_return_if_fail (bin->in_drag == false);

  if (bin->child == child_widget)
    return;

  if (bin->child)
    {
      rut_graphable_remove_child (bin->child);
      rut_object_unref (bin->child);
    }

  bin->child = child_widget;

  if (child_widget)
    {
      rut_bin_set_child (bin->bin, child_widget);
      rut_object_ref (child_widget);
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
    rut_object_unref (bin->payload);

  bin->payload = payload;
  if (payload)
    rut_object_ref (payload);
}
