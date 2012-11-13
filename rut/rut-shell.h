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

#ifndef _RUT_SHELL_H_
#define _RUT_SHELL_H_

#include "rut-keysyms.h"
#include "rut-object.h"
#include "rut-types.h"
#include "rut-closure.h"

#ifdef __ANDROID__
#include <android_native_app_glue.h>
#endif

typedef void (*RutShellInitCallback) (RutShell *shell, void *user_data);
typedef void (*RutShellFiniCallback) (RutShell *shell, void *user_data);
typedef CoglBool (*RutShellPaintCallback) (RutShell *shell, void *user_data);

RutShell *
rut_shell_new (RutShellInitCallback init,
               RutShellFiniCallback fini,
               RutShellPaintCallback paint,
               void *user_data);

/* XXX: Basically just a hack for now to effectively relate input events to
 * a CoglFramebuffer and so we have a way to consistently associate a
 * RutCamera with all input events.
 *
 * The camera should provide an orthographic projection into input device
 * coordinates and it's assume to be automatically updated according to
 * window resizes.
 */
void
rut_shell_set_window_camera (RutShell *shell, RutCamera *window_camera);

#ifdef __ANDROID__
RutShell *
rut_android_shell_new (struct android_app* application,
                       RutShellInitCallback init,
                       RutShellFiniCallback fini,
                       RutShellPaintCallback paint,
                       void *user_data);
#endif

/* PRIVATE */
void
_rut_shell_associate_context (RutShell *shell,
                              RutContext *context);

void
_rut_shell_init (RutShell *shell);

RutContext *
rut_shell_get_context (RutShell *shell);

void
rut_shell_main (RutShell *shell);

void
rut_shell_add_input_camera (RutShell *shell,
                            RutCamera *camera,
                            RutObject *scenegraph);

void
rut_shell_remove_input_camera (RutShell *shell,
                               RutCamera *camera,
                               RutObject *scenegraph);

typedef enum _RutInputEventType
{
  RUT_INPUT_EVENT_TYPE_MOTION = 1,
  RUT_INPUT_EVENT_TYPE_KEY
} RutInputEventType;

typedef enum _RutKeyEventAction
{
  RUT_KEY_EVENT_ACTION_UP = 1,
  RUT_KEY_EVENT_ACTION_DOWN
} RutKeyEventAction;

typedef enum _RutMotionEventAction
{
  RUT_MOTION_EVENT_ACTION_UP = 1,
  RUT_MOTION_EVENT_ACTION_DOWN,
  RUT_MOTION_EVENT_ACTION_MOVE,
} RutMotionEventAction;

typedef enum _RutButtonState
{
  RUT_BUTTON_STATE_1         = 1<<0,
  RUT_BUTTON_STATE_2         = 1<<1,
  RUT_BUTTON_STATE_3         = 1<<2,
  RUT_BUTTON_STATE_WHEELUP   = 1<<3,
  RUT_BUTTON_STATE_WHEELDOWN = 1<<4
} RutButtonState;

typedef enum _RutModifierState
{
  RUT_MODIFIER_LEFT_ALT_ON = 1<<0,
  RUT_MODIFIER_RIGHT_ALT_ON = 1<<1,

  RUT_MODIFIER_LEFT_SHIFT_ON = 1<<2,
  RUT_MODIFIER_RIGHT_SHIFT_ON = 1<<3,

  RUT_MODIFIER_LEFT_CTRL_ON = 1<<4,
  RUT_MODIFIER_RIGHT_CTRL_ON = 1<<5,

  RUT_MODIFIER_LEFT_META_ON = 1<<6,
  RUT_MODIFIER_RIGHT_META_ON = 1<<7,

  RUT_MODIFIER_NUM_LOCK_ON = 1<<8,
  RUT_MODIFIER_CAPS_LOCK_ON = 1<<9

} RutModifierState;

#define RUT_MODIFIER_ALT_ON (RUT_MODIFIER_LEFT_ALT_ON|RUT_MODIFIER_RIGHT_ALT_ON)
#define RUT_MODIFIER_SHIFT_ON (RUT_MODIFIER_LEFT_SHIFT_ON|RUT_MODIFIER_RIGHT_SHIFT_ON)
#define RUT_MODIFIER_CTRL_ON (RUT_MODIFIER_LEFT_CTRL_ON|RUT_MODIFIER_RIGHT_CTRL_ON)
#define RUT_MODIFIER_META_ON (RUT_MODIFIER_LEFT_META_ON|RUT_MODIFIER_RIGHT_META_ON)

typedef enum _RutInputEventStatus
{
  RUT_INPUT_EVENT_STATUS_UNHANDLED,
  RUT_INPUT_EVENT_STATUS_HANDLED,
} RutInputEventStatus;

typedef struct _RutInputEvent RutInputEvent;

typedef RutInputEventStatus (*RutInputCallback) (RutInputEvent *event,
                                                 void *user_data);

/**
 * rut_shell_grab_input:
 * @shell: The #RutShell
 * @camera: An optional #RutCamera to set on grabbed events
 * @callback: A callback to give all events to
 * @user_data: A pointer to pass to the callback
 *
 * Adds a grab which will get a chance to see all input events before
 * anything else handles them. The callback can return
 * %RUT_INPUT_EVENT_STATUS_HANDLED if no further processing should be
 * done for the event or %RUT_INPUT_EVENT_STATUS_UNHANDLED otherwise.
 *
 * Multiple grabs can be in place at the same time. In this case the
 * events will be given to the grabs in the reverse order that they
 * were added.
 *
 * If a camera is given for the grab then this camera will be set on
 * all input events before passing them to the callback.
 */
void
rut_shell_grab_input (RutShell *shell,
                      RutCamera *camera,
                      RutInputCallback callback,
                      void *user_data);

/**
 * rut_shell_ungrab_input:
 * @shell: The #RutShell
 * @callback: The callback that the grab was set with
 * @user_data: The user data that the grab was set with
 *
 * Removes a grab that was previously set with rut_shell_grab_input().
 * The @callback and @user_data must match the values passed when the
 * grab was taken.
 */
void
rut_shell_ungrab_input (RutShell *shell,
                        RutInputCallback callback,
                        void *user_data);

/**
 * rut_shell_grab_key_focus:
 * @shell: The #RutShell
 * @inputable: A #RutObject that implements the inputable interface
 * @ungrab_callback: A callback to invoke when the grab is lost
 *
 * Sets the shell's key grab to the given object. The object must
 * implement the inputable interface. All key events will be given to
 * the input region of this object until either something else takes
 * the key focus or rut_shell_ungrab_key_focus() is called.
 */
void
rut_shell_grab_key_focus (RutShell *shell,
                          RutObject *inputable,
                          GDestroyNotify ungrab_callback);

void
rut_shell_ungrab_key_focus (RutShell *shell);

void
rut_shell_queue_redraw (RutShell *shell);

RutCamera *
rut_input_event_get_camera (RutInputEvent *event);

RutInputEventType
rut_input_event_get_type (RutInputEvent *event);

RutKeyEventAction
rut_key_event_get_action (RutInputEvent *event);

int32_t
rut_key_event_get_keysym (RutInputEvent *event);

uint32_t
rut_key_event_get_unicode (RutInputEvent *event);

RutModifierState
rut_key_event_get_modifier_state (RutInputEvent *event);

RutMotionEventAction
rut_motion_event_get_action (RutInputEvent *event);

RutButtonState
rut_motion_event_get_button_state (RutInputEvent *event);

RutModifierState
rut_motion_event_get_modifier_state (RutInputEvent *event);

float
rut_motion_event_get_x (RutInputEvent *event);

float
rut_motion_event_get_y (RutInputEvent *event);

/**
 * rut_motion_event_unproject:
 * @event: A motion event
 * @graphable: An object that implements #RutGraphable
 * @x: Output location for the unprojected coordinate
 * @y: Output location for the unprojected coordinate
 *
 * Unprojects the position of the motion event so that it will be
 * relative to the coordinate space of the given graphable object.
 *
 * Return value: %FALSE if the coordinate can't be unprojected or
 *   %TRUE otherwise. The coordinate can't be unprojected if the
 *   transform for the graphable object object does not have an inverse.
 */
CoglBool
rut_motion_event_unproject (RutInputEvent *event,
                            RutObject *graphable,
                            float *x,
                            float *y);

typedef RutInputEventStatus (*RutInputRegionCallback) (RutInputRegion *region,
                                                       RutInputEvent *event,
                                                       void *user_data);

RutInputRegion *
rut_input_region_new_rectangle (float x0,
                                float y0,
                                float x1,
                                float y1,
                                RutInputRegionCallback callback,
                                void *user_data);

RutInputRegion *
rut_input_region_new_circle (float x,
                             float y,
                             float radius,
                             RutInputRegionCallback callback,
                             void *user_data);

void
rut_input_region_set_transform (RutInputRegion *region,
                                CoglMatrix *matrix);

void
rut_input_region_set_rectangle (RutInputRegion *region,
                                float x0,
                                float y0,
                                float x1,
                                float y1);

void
rut_input_region_set_circle (RutInputRegion *region,
                             float x0,
                             float y0,
                             float radius);

/* If HUD mode is TRUE then the region isn't transformed by the
 * camera's view transform so the region is in window coordinates.
 */
void
rut_input_region_set_hud_mode (RutInputRegion *region,
                               CoglBool hud_mode);

typedef struct _RutSlider RutSlider;
#define RUT_SLIDER(X) ((RutSlider *)X)

extern RutType rut_slider_type;

RutSlider *
rut_slider_new (RutContext *ctx,
                RutAxis axis,
                float min,
                float max,
                float length);

void
rut_slider_set_range (RutSlider *slider,
                      float min, float max);

void
rut_slider_set_length (RutSlider *slider,
                       float length);

void
rut_slider_set_progress (RutObject *slider,
                         float progress);
RutClosure *
rut_shell_add_input_callback (RutShell *shell,
                              RutInputCallback callback,
                              void *user_data,
                              RutClosureDestroyCallback destroy_cb);

typedef void
(* RutPrePaintCallback) (RutObject *graphable,
                         void *user_data);

/**
 * rut_shell_add_pre_paint_callback:
 * @shell: The #RutShell
 * @graphable: An object implementing the graphable interface
 * @callback: The callback to invoke
 * @user_data: A user data pointer to pass to the callback
 *
 * Adds a callback that will be invoked just before the next frame of
 * the shell is painted. The callback is associated with a graphable
 * object which is used to ensure the callbacks are invoked in
 * increasing order of depth in the hierarchy that the graphable
 * object belongs to. If this function is called a second time for the
 * same graphable object then no extra callback will be added. For
 * that reason, this function should always be called with the same
 * callback and user_data pointers for a particular graphable object.
 *
 * It is safe to call this function in the middle of a pre paint
 * callback. The shell will keep calling callbacks until all of the
 * pending callbacks are complete and no new callbacks were queued.
 *
 * Typically this callback will be registered when an object needs to
 * layout its children before painting. In that case it is expecting
 * that setting the size on the objects children may cause them to
 * also register a pre-paint callback.
 */
void
rut_shell_add_pre_paint_callback (RutShell *shell,
                                  RutObject *graphable,
                                  RutPrePaintCallback callback,
                                  void *user_data);

/**
 * rut_shell_remove_pre_paint_callback:
 * @shell: The #RutShell
 * @graphable: A graphable object
 *
 * Removes a pre-paint callback that was previously registered with
 * rut_shell_add_pre_paint_callback(). It is not an error to call this
 * function if no callback has actually been registered.
 */
void
rut_shell_remove_pre_paint_callback (RutShell *shell,
                                     RutObject *graphable);

#endif /* _RUT_SHELL_H_ */
