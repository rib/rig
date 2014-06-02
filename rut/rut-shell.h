/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
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

#ifndef _RUT_SHELL_H_
#define _RUT_SHELL_H_

#include <stdbool.h>

#if defined (USE_SDL)
#include "rut-sdl-keysyms.h"
#include "SDL_syswm.h"
#endif

#ifdef USE_UV
#include <uv.h>
#endif

#include <cogl/cogl.h>

#include "rut-keysyms.h"
#include "rut-object.h"
#include "rut-types.h"
#include "rut-closure.h"
#include "rut-poll.h"

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
  RutObject *camera;
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

typedef struct
{
  RutList list_node;
  RutInputCallback callback;
  RutObject *camera;
  void *user_data;
} RutShellGrab;

typedef void
(* RutPrePaintCallback) (RutObject *graphable,
                         void *user_data);

typedef struct
{
  RutList list_node;

  int depth;
  RutObject *graphable;

  RutPrePaintCallback callback;
  void *user_data;
} RutShellPrePaintEntry;

#ifdef USE_SDL
typedef struct _RutSDLEvent
{
  SDL_Event sdl_event;

  /* SDL uses global state to report keyboard modifier and button states
   * which is a pain if events are being batched before processing them
   * on a per-frame basis since we want to be able to track how this
   * state changes relative to events. */
  SDL_Keymod mod_state;

  /* It could be nice if SDL_MouseButtonEvents had a buttons member
   * that had the full state of buttons as returned by
   * SDL_GetMouseState () */
  uint32_t buttons;
} RutSDLEvent;

typedef void (*RutSDLEventHandler) (RutShell *shell,
                                    SDL_Event *event,
                                    void *user_data);
#endif

typedef struct _RutInputQueue
{
  RutShell *shell;
  RutList events;
  int n_events;
} RutInputQueue;

struct _RutShell
{
  RutObjectBase _base;


  /* If true then this process does not handle input events directly
   * or output graphics directly. */
  bool headless;
#ifdef USE_SDL
  SDL_SYSWM_TYPE sdl_subsystem;
  SDL_Keymod sdl_keymod;
  uint32_t sdl_buttons;
  bool x11_grabbed;

  /* Note we can't use SDL_WaitEvent() to block for events given
   * that we also care about non-SDL based events.
   *
   * This lets us to use poll() to block until an SDL event
   * is recieved instead of busy waiting...
   */
  SDL_mutex *event_pipe_mutex;
  int event_pipe_read;
  int event_pipe_write;
  bool wake_queued;

  CArray *cogl_poll_fds;
  int cogl_poll_fds_age;
#endif

  int poll_sources_age;
  RutList poll_sources;

  RutList idle_closures;

#if 0
  int signal_read_fd;
  RutList signal_cb_list;
#endif

#ifdef USE_GLIB
  GMainLoop *main_loop;
#endif

#ifdef USE_UV
  uv_loop_t *uv_loop;
  uv_idle_t uv_idle;
  uv_prepare_t cogl_prepare;
  uv_timer_t cogl_timer;
  uv_check_t cogl_check;
#ifdef USE_GLIB
  GMainContext *glib_main_ctx;
  uv_prepare_t glib_uv_prepare;
  uv_check_t glib_uv_check;
  uv_timer_t glib_uv_timer;
  uv_check_t glib_uv_timer_check;
  GArray *pollfds;
  GArray *glib_polls;
  int n_pollfds;
#endif
#endif

  RutClosure *paint_idle;

  RutInputQueue *input_queue;
  int input_queue_len;

  RutContext *rut_ctx;

  RutShellInitCallback init_cb;
  RutShellFiniCallback fini_cb;
  RutShellPaintCallback paint_cb;
  void *user_data;

  RutList input_cb_list;
  CList *input_cameras;

  /* Used to handle input events in window coordinates */
  RutObject *window_camera;

  /* Last known position of the mouse */
  float mouse_x;
  float mouse_y;

  RutObject *drag_payload;
  RutObject *drop_offer_taker;

  /* List of grabs that are currently in place. This are in order from
   * highest to lowest priority. */
  RutList grabs;
  /* A pointer to the next grab to process. This is only used while
   * invoking the grab callbacks so that we can cope with multiple
   * grabs being removed from the list while one is being processed */
  RutShellGrab *next_grab;

  RutObject *keyboard_focus_object;
  GDestroyNotify keyboard_ungrab_cb;

  RutObject *clipboard;

  void (*queue_redraw_callback) (RutShell *shell,
                                 void *user_data);
  void *queue_redraw_data;

  /* Queue of callbacks to be invoked before painting. If
   * ‘flushing_pre_paints‘ is TRUE then this will be maintained in
   * sorted order. Otherwise it is kept in no particular order and it
   * will be sorted once prepaint flushing starts. That way it doesn't
   * need to keep track of hierarchy changes that occur after the
   * pre-paint was queued. This assumes that the depths won't change
   * will the queue is being flushed */
  RutList pre_paint_callbacks;
  CoglBool flushing_pre_paints;

  RutList start_paint_callbacks;
  RutList post_paint_callbacks;

  int frame;
  RutList frame_infos;

  /* A list of onscreen windows that the shell is manipulating */
  RutList onscreens;

  RutObject *selection;
};

typedef enum _RutInputTransformType
{
  RUT_INPUT_TRANSFORM_TYPE_NONE,
  RUT_INPUT_TRANSFORM_TYPE_MATRIX,
  RUT_INPUT_TRANSFORM_TYPE_GRAPHABLE
} RutInputTransformType;

typedef struct _RutInputTransformAny
{
  RutInputTransformType type;
} RutInputTransformAny;

typedef struct _RutInputTransformMatrix
{
  RutInputTransformType type;
  CoglMatrix *matrix;
} RutInputTransformMatrix;

typedef struct _RutInputTransformGraphable
{
  RutInputTransformType type;
} RutInputTransformGraphable;

typedef union _RutInputTransform
{
  RutInputTransformAny any;
  RutInputTransformMatrix matrix;
  RutInputTransformGraphable graphable;
} RutInputTransform;


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
 * camera with all input events.
 *
 * The camera should provide an orthographic projection into input device
 * coordinates and it's assume to be automatically updated according to
 * window resizes.
 */
void
rut_shell_set_window_camera (RutShell *shell, RutObject *window_camera);

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
                            RutObject *camera,
                            RutObject *scenegraph);

void
rut_shell_remove_input_camera (RutShell *shell,
                               RutObject *camera,
                               RutObject *scenegraph);


/**
 * rut_shell_grab_input:
 * @shell: The #RutShell
 * @camera: An optional camera to set on grabbed events
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
                      RutObject *camera,
                      RutInputEventStatus (*callback) (RutInputEvent *event,
                                                       void *user_data),
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
                        RutInputEventStatus (*callback) (RutInputEvent *event,
                                                         void *user_data),
                        void *user_data);

/*
 * Use this API for the common case of grabbing input for a pointer
 * when we receive a button press and want to grab until a
 * corresponding button release.
 */
void
rut_shell_grab_pointer (RutShell *shell,
                        RutObject *camera,
                        RutInputEventStatus (*callback) (RutInputEvent *event,
                                                         void *user_data),
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

void
rut_shell_set_queue_redraw_callback (RutShell *shell,
                                     void (*callback) (RutShell *shell,
                                                       void *user_data),
                                     void *user_data);

/* You can hook into rut_shell_queue_redraw() via
 * rut_shell_set_queue_redraw_callback() but then it if you really
 * want to queue a redraw with the platform mainloop it is your
 * responsibility to call rut_shell_queue_redraw_real() */
void
rut_shell_queue_redraw_real (RutShell *shell);

RutObject *
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

RutClosure *
rut_shell_add_input_callback (RutShell *shell,
                              RutInputCallback callback,
                              void *user_data,
                              RutClosureDestroyCallback destroy_cb);

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

#if 0
typedef void (*RutShellSignalCallback) (RutShell *shell,
                                        int signal,
                                        void *user_data);

RutClosure *
rut_shell_add_signal_callback (RutShell *shell,
                               RutShellSignalCallback callback,
                               void *user_data,
                               RutClosureDestroyCallback destroy_cb);
#endif

#ifdef USE_SDL
void
rut_shell_handle_sdl_event (RutShell *shell, SDL_Event *sdl_event);
#endif

#endif /* _RUT_SHELL_H_ */
