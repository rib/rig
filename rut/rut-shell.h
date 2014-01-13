/*
 * Rut
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
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

#include <stdbool.h>

#include <cogl/cogl.h>

#include "rut-keysyms.h"
#include "rut-object.h"
#include "rut-types.h"
#include "rut-closure.h"

#ifdef __ANDROID__
#include <android_native_app_glue.h>
#endif

typedef void (*RutShellInitCallback) (RutShell *shell, void *user_data);
typedef void (*RutShellFiniCallback) (RutShell *shell, void *user_data);
typedef void (*RutShellPaintCallback) (RutShell *shell, void *user_data);

typedef enum _RutInputEventType
{
  RUT_INPUT_EVENT_TYPE_MOTION = 1,
  RUT_INPUT_EVENT_TYPE_KEY,
  RUT_INPUT_EVENT_TYPE_TEXT,
  RUT_INPUT_EVENT_TYPE_DROP_OFFER,
  RUT_INPUT_EVENT_TYPE_DROP_CANCEL,
  RUT_INPUT_EVENT_TYPE_DROP
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

typedef enum
{
  RUT_CURSOR_ARROW,
  RUT_CURSOR_IBEAM,
  RUT_CURSOR_WAIT,
  RUT_CURSOR_CROSSHAIR,
  RUT_CURSOR_SIZE_WE,
  RUT_CURSOR_SIZE_NS
} RutCursor;

#define RUT_MODIFIER_ALT_ON (RUT_MODIFIER_LEFT_ALT_ON|RUT_MODIFIER_RIGHT_ALT_ON)
#define RUT_MODIFIER_SHIFT_ON (RUT_MODIFIER_LEFT_SHIFT_ON|RUT_MODIFIER_RIGHT_SHIFT_ON)
#define RUT_MODIFIER_CTRL_ON (RUT_MODIFIER_LEFT_CTRL_ON|RUT_MODIFIER_RIGHT_CTRL_ON)
#define RUT_MODIFIER_META_ON (RUT_MODIFIER_LEFT_META_ON|RUT_MODIFIER_RIGHT_META_ON)

typedef enum _RutInputEventStatus
{
  RUT_INPUT_EVENT_STATUS_UNHANDLED,
  RUT_INPUT_EVENT_STATUS_HANDLED,
} RutInputEventStatus;

typedef struct _RutInputEvent
{
  RutList list_node;
  RutInputEventType type;
  RutShell *shell;
  RutCamera *camera;
  const CoglMatrix *input_transform;

  void *native;

  uint8_t data[];
} RutInputEvent;

/* Stream events are optimized for IPC based on the assumption that
 * the remote process does some state tracking to know the current
 * state of pointer buttons for example.
 */
typedef enum _RutStreamEventType
{
  RUT_STREAM_EVENT_POINTER_MOVE = 1,
  RUT_STREAM_EVENT_POINTER_DOWN,
  RUT_STREAM_EVENT_POINTER_UP,
  RUT_STREAM_EVENT_KEY_DOWN,
  RUT_STREAM_EVENT_KEY_UP
} RutStreamEventType;

typedef struct _RutStreamEvent
{
  RutStreamEventType type;
  uint64_t timestamp;

  union {
    struct {
      RutButtonState state;
      float x;
      float y;
    } pointer_move;
    struct {
      RutButtonState state;
      RutButtonState button;
      float x;
      float y;
    } pointer_button;
    struct {
      int keysym;
      RutModifierState mod_state;
    } key;
  };
} RutStreamEvent;

typedef RutInputEventStatus (*RutInputCallback) (RutInputEvent *event,
                                                 void *user_data);

RutShell *
rut_shell_new (bool headless,
               RutShellInitCallback init,
               RutShellFiniCallback fini,
               RutShellPaintCallback paint,
               void *user_data);

bool
rut_shell_get_headless (RutShell *shell);

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

/*
 * Whatever paint function is given when creating a RutShell
 * is responsible for handling each redraw cycle but should
 * pass control back to the shell for progressing timlines,
 * running pre-paint callbacks and finally checking whether
 * to queue another redraw if there are any timelines
 * running...
 *
 * The folling apis can be used to implement a
 * RutShellPaintCallback...
 */

/* Should be the first thing called for each redraw... */
void
rut_shell_start_redraw (RutShell *shell);

/* Progress timelines */
void
rut_shell_update_timelines (RutShell *shell);

void
rut_shell_dispatch_input_events (RutShell *shell);

/* Dispatch a single event as rut_shell_dispatch_input_events would */
RutInputEventStatus
rut_shell_dispatch_input_event (RutShell *shell, RutInputEvent *event);

typedef struct _RutInputQueue
{
  RutShell *shell;
  RutList events;
  int n_events;
} RutInputQueue;

RutInputQueue *
rut_input_queue_new (RutShell *shell);

void
rut_input_queue_append (RutInputQueue *queue,
                        RutInputEvent *event);

void
rut_input_queue_remove (RutInputQueue *queue,
                        RutInputEvent *event);

void
rut_input_queue_clear (RutInputQueue *queue);

RutInputQueue *
rut_shell_get_input_queue (RutShell *shell);

void
rut_shell_run_pre_paint_callbacks (RutShell *shell);

/* Determines whether any timelines are running */
bool
rut_shell_check_timelines (RutShell *shell);

void
rut_shell_handle_stream_event (RutShell *shell,
                               RutStreamEvent *event);

void
rut_shell_run_start_paint_callbacks (RutShell *shell);

void
rut_shell_run_post_paint_callbacks (RutShell *shell);

/* Delimit the end of a frame, at this point the frame counter is
 * incremented.
 */
void
rut_shell_end_redraw (RutShell *shell);

/* Called when a frame has really finished, e.g. when the Rig
 * simulator has finished responding to a run_frame request, sent its
 * update, the new frame has been rendered and presented to the user.
 */
void
rut_shell_finish_frame (RutShell *shell);

void
rut_shell_add_input_camera (RutShell *shell,
                            RutCamera *camera,
                            RutObject *scenegraph);

void
rut_shell_remove_input_camera (RutShell *shell,
                               RutCamera *camera,
                               RutObject *scenegraph);


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

/**
 * rut_input_event_get_onscreen:
 * @event: A #RutInputEvent
 *
 * Return value: the onscreen window that this event was generated for
 *   or %NULL if the event does not correspond to a window.
 */
CoglOnscreen *
rut_input_event_get_onscreen (RutInputEvent *event);

RutKeyEventAction
rut_key_event_get_action (RutInputEvent *event);

int32_t
rut_key_event_get_keysym (RutInputEvent *event);

RutModifierState
rut_key_event_get_modifier_state (RutInputEvent *event);

RutMotionEventAction
rut_motion_event_get_action (RutInputEvent *event);

/* For a button-up/down event which specific button changed? */
RutButtonState
rut_motion_event_get_button (RutInputEvent *event);

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

RutObject *
rut_drop_offer_event_get_payload (RutInputEvent *event);

/* Returns the text generated by the event as a null-terminated UTF-8
 * encoded string. */
const char *
rut_text_event_get_text (RutInputEvent *event);

RutObject *
rut_drop_event_get_data (RutInputEvent *drop_event);

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

RutClosure *
rut_shell_add_start_paint_callback (RutShell *shell,
                                    RutShellPaintCallback callback,
                                    void *user_data,
                                    RutClosureDestroyCallback destroy);

RutClosure *
rut_shell_add_post_paint_callback (RutShell *shell,
                                   RutShellPaintCallback callback,
                                   void *user_data,
                                   RutClosureDestroyCallback destroy);

typedef struct _RutFrameInfo
{
  RutList list_node;

  int frame;
  RutList frame_callbacks;
} RutFrameInfo;

RutFrameInfo *
rut_shell_get_frame_info (RutShell *shell);

typedef void (*RutShellFrameCallback) (RutShell *shell,
                                       RutFrameInfo *info,
                                       void *user_data);

RutClosure *
rut_shell_add_frame_callback (RutShell *shell,
                              RutShellFrameCallback callback,
                              void *user_data,
                              RutClosureDestroyCallback destroy);

/**
 * rut_shell_remove_pre_paint_callback_by_graphable:
 * @shell: The #RutShell
 * @graphable: A graphable object
 *
 * Removes a pre-paint callback that was previously registered with
 * rut_shell_add_pre_paint_callback(). It is not an error to call this
 * function if no callback has actually been registered.
 */
void
rut_shell_remove_pre_paint_callback_by_graphable (RutShell *shell,
                                                  RutObject *graphable);

void
rut_shell_remove_pre_paint_callback (RutShell *shell,
                                     RutPrePaintCallback callback,
                                     void *user_data);

/**
 * rut_shell_add_onscreen:
 * @shell: The #RutShell
 * @onscreen: A #CoglOnscreen
 *
 * This should be called for everything onscreen that is going to be
 * used by the shell so that Rut can keep track of them.
 */
void
rut_shell_add_onscreen (RutShell *shell,
                        CoglOnscreen *onscreen);

/**
 * rut_shell_set_cursor:
 * @shell: The #RutShell
 * @onscreen: An onscreen window to set the cursor for
 * @cursor: The new cursor
 *
 * Attempts to set the mouse cursor image to @cursor. The shell will
 * automatically reset the cursor back to the default pointer on every
 * mouse motion event if nothing else sets it. The expected usage is
 * that a widget which wants a particular cursor will listen for
 * motion events and always change the cursor when the pointer is over
 * a certain area.
 */
void
rut_shell_set_cursor (RutShell *shell,
                      CoglOnscreen *onscreen,
                      RutCursor cursor);

void
rut_shell_set_title (RutShell *shell,
                     CoglOnscreen *onscreen,
                     const char *title);

/**
 * rut_shell_quit:
 * @shell: The #RutShell
 *
 * Informs the shell that at the next time it returns to the main loop
 * it should quit the loop.
 */
void
rut_shell_quit (RutShell *shell);

/**
 * rut_shell_set_selection:
 * @selection: An object implementing the selectable interface
 *
 * This cancels any existing global selection, calling the ::cancel
 * method of the current selection and make @selection the replacement
 * selection.
 *
 * If Ctrl-C is later pressed then ::copy will be called for the
 * selectable and the returned object will be set on the clipboard.
 * If Ctrl-Z is later pressed then ::cut will be called for the
 * selectable and the returned object will be set on the clipboard.
 */
void
rut_shell_set_selection (RutShell *shell,
                         RutObject *selection);

RutObject *
rut_shell_get_selection (RutShell *shell);

RutObject *
rut_shell_get_clipboard (RutShell *shell);

extern RutType rut_text_blob_type;

typedef struct _RutTextBlob RutTextBlob;

RutTextBlob *
rut_text_blob_new (const char *text);

void
rut_shell_start_drag (RutShell *shell, RutObject *payload);

void
rut_shell_cancel_drag (RutShell *shell);

void
rut_shell_drop (RutShell *shell);

void
rut_shell_take_drop_offer (RutShell *shell, RutObject *taker);

#endif /* _RUT_SHELL_H_ */
