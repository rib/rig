/*
 * Rut
 *
 * Copyright (C) 2012,2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib.h>

#include <cogl/cogl.h>
#include <cogl/cogl-sdl.h>

#include "rut-camera-private.h"
#include "rut-transform-private.h"
#include "rut-shell.h"
#include "rut-util.h"
#include "rut-ui-viewport.h"
#include "rut-inputable.h"
#include "rut-pickable.h"
#include "rut-timeline.h"
#include "rut-global.h"
#include "rut-paintable.h"
#include "rut-transform.h"
#include "rut-input-region.h"
#include "rut-mimable.h"

#include "components/rut-nine-slice.h"
#include "components/rut-camera.h"

#ifdef __ANDROID__
#include <android_native_app_glue.h>
#include <glib-android/glib-android.h>
#elif defined (USE_SDL)
#include "rut-sdl-keysyms.h"
#endif

typedef struct
{
  RutList list_node;
  RutInputCallback callback;
  RutCamera *camera;
  void *user_data;
} RutShellGrab;

typedef struct
{
  RutList list_node;

  int depth;
  RutObject *graphable;

  RutPrePaintCallback callback;
  void *user_data;
} RutShellPrePaintEntry;

#ifdef USE_SDL
typedef void (*RutSDLEventHandler) (RutShell *shell,
                                    SDL_Event *event,
                                    void *user_data);
#endif

struct _RutShell
{
  RutObjectProps _parent;

  int ref_count;

#ifdef __ANDROID__
  CoglBool quit;
#else
  GMainLoop *main_loop;
#endif

#ifdef __ANDROID__
  struct android_app* app;
#endif

  RutContext *rut_ctx;

  RutShellInitCallback init_cb;
  RutShellFiniCallback fini_cb;
  RutShellPaintCallback paint_cb;
  void *user_data;

  RutList input_cb_list;
  GList *input_cameras;

  /* Use to handle input events in window coordinates */
  RutCamera *window_camera;

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

  int glib_paint_idle;
  CoglBool redraw_queued;

  /* Queue of callbacks to be invoked before painting. If
   * ‘flushing_pre_paints‘ is TRUE then this will be maintained in
   * sorted order. Otherwise it is kept in no particular order and it
   * will be sorted once prepaint flushing starts. That way it doesn't
   * need to keep track of hierarchy changes that occur after the
   * pre-paint was queued. This assumes that the depths won't change
   * will the queue is being flushed */
  RutList pre_paint_callbacks;
  CoglBool flushing_pre_paints;

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

typedef struct
{
  RutList link;

  CoglOnscreen *onscreen;

  RutCursor current_cursor;
  /* This is used to record whether anything set a cursor while
   * handling a mouse motion event. If nothing sets one then the shell
   * will put the cursor back to the default pointer. */
  CoglBool cursor_set;

#if defined(USE_SDL) && SDL_MAJOR_VERSION >= 2
  SDL_Cursor *cursor_image;
#endif
} RutShellOnscreen;

static void
_rut_slider_init_type (void);

RutContext *
rut_shell_get_context (RutShell *shell)
{
  return shell->rut_ctx;
}

static void
_rut_shell_fini (RutShell *shell)
{
  rut_refable_unref (shell->rut_ctx);
}

struct _RutInputEvent
{
  RutInputEventType type;
  RutShell *shell;
  void *native;
  RutCamera *camera;
  const CoglMatrix *input_transform;
};

RutClosure *
rut_shell_add_input_callback (RutShell *shell,
                              RutInputCallback callback,
                              void *user_data,
                              RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&shell->input_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}

typedef struct _InputCamera
{
  RutCamera *camera;
  RutObject *scenegraph;
} InputCamera;

void
rut_shell_add_input_camera (RutShell *shell,
                            RutCamera *camera,
                            RutObject *scenegraph)
{
  InputCamera *input_camera = g_slice_new (InputCamera);

  input_camera->camera = rut_refable_ref (camera);

  if (scenegraph)
    input_camera->scenegraph = rut_refable_ref (scenegraph);
  else
    input_camera->scenegraph = NULL;

  shell->input_cameras = g_list_prepend (shell->input_cameras, input_camera);
}

static void
input_camera_free (InputCamera *input_camera)
{
  rut_refable_unref (input_camera->camera);
  if (input_camera->scenegraph)
    rut_refable_unref (input_camera->scenegraph);
  g_slice_free (InputCamera, input_camera);
}

void
rut_shell_remove_input_camera (RutShell *shell,
                               RutCamera *camera,
                               RutObject *scenegraph)
{
  GList *l;

  for (l = shell->input_cameras; l; l = l->next)
    {
      InputCamera *input_camera = l->data;
      if (input_camera->camera == camera &&
          input_camera->scenegraph == scenegraph)
        {
          input_camera_free (input_camera);
          shell->input_cameras = g_list_delete_link (shell->input_cameras, l);
          return;
        }
    }

  g_warning ("Failed to find input camera to remove from shell");
}

static void
_rut_shell_remove_all_input_cameras (RutShell *shell)
{
  GList *l;

  for (l = shell->input_cameras; l; l = l->next)
    input_camera_free (l->data);
  g_list_free (shell->input_cameras);
  shell->input_cameras = NULL;
}

RutCamera *
rut_input_event_get_camera (RutInputEvent *event)
{
  return event->camera;
}

RutInputEventType
rut_input_event_get_type (RutInputEvent *event)
{
  return event->type;
}

CoglOnscreen *
rut_input_event_get_onscreen (RutInputEvent *event)
{
  RutShell *shell = event->shell;

#if defined(USE_SDL) && SDL_MAJOR_VERSION >= 2

  {
    RutShellOnscreen *shell_onscreen;
    SDL_Event *sdl_event = event->native;
    Uint32 window_id;

    switch ((SDL_EventType) sdl_event->type)
      {
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        window_id = sdl_event->key.windowID;
        break;

      case SDL_TEXTEDITING:
        window_id = sdl_event->edit.windowID;
        break;

      case SDL_TEXTINPUT:
        window_id = sdl_event->text.windowID;
        break;

      case SDL_MOUSEMOTION:
        window_id = sdl_event->motion.windowID;
        break;

      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        window_id = sdl_event->button.windowID;
        break;

      case SDL_MOUSEWHEEL:
        window_id = sdl_event->wheel.windowID;
        break;

      default:
        return NULL;
      }

    rut_list_for_each (shell_onscreen, &shell->onscreens, link)
      {
        SDL_Window *sdl_window =
          cogl_sdl_onscreen_get_window (shell_onscreen->onscreen);

        if (SDL_GetWindowID (sdl_window) == window_id)
          return shell_onscreen->onscreen;
      }

    return NULL;
  }

#else

  /* If there is only onscreen then we'll assume that all events are
   * related to that. This will be the case when using Android or
   * SDL1 */
  if (shell->onscreens.next != &shell->onscreens &&
      shell->onscreens.next->next == &shell->onscreens)
    {
      RutShellOnscreen *shell_onscreen =
        rut_container_of (shell->onscreens.next, shell_onscreen, link);
      return shell_onscreen->onscreen;
    }
  else
    return NULL;

#endif
}

int32_t
rut_key_event_get_keysym (RutInputEvent *event)
{
#ifdef __ANDROID__
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;

  return _rut_keysym_from_sdl_keysym (sdl_event->key.keysym.sym);
#else
#error "Unknown input system"
#endif
}

RutKeyEventAction
rut_key_event_get_action (RutInputEvent *event)
{
#ifdef __ANDROID__
  switch (AKeyEvent_getAction (event->native))
    {
    case AKEY_EVENT_ACTION_DOWN:
      return RUT_KEY_EVENT_ACTION_DOWN;
    case AKEY_EVENT_ACTION_UP:
      return RUT_KEY_EVENT_ACTION_UP;
    case AKEY_EVENT_ACTION_MULTIPLE:
      g_warn_if_reached ();
      /* TODO: Expand these out in android_handle_event into multiple distinct events,
       * it seems odd to require app developers to have to have special code for this
       * and key events are surely always low frequency enough that we don't need
       * this for optimization purposes.
       */
      return RUT_KEY_EVENT_ACTION_UP;
    }
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;
  switch (sdl_event->type)
    {
    case SDL_KEYUP:
      return RUT_KEY_EVENT_ACTION_UP;
    case SDL_KEYDOWN:
      return RUT_KEY_EVENT_ACTION_DOWN;
    default:
      g_warn_if_reached ();
      return RUT_KEY_EVENT_ACTION_UP;
    }
#else
#error "Unknown input system"
#endif
}

RutMotionEventAction
rut_motion_event_get_action (RutInputEvent *event)
{
#ifdef __ANDROID__
  switch (AMotionEvent_getAction (event->native))
    {
    case AMOTION_EVENT_ACTION_DOWN:
      return RUT_MOTION_EVENT_ACTION_DOWN;
    case AMOTION_EVENT_ACTION_UP:
      return RUT_MOTION_EVENT_ACTION_UP;
    case AMOTION_EVENT_ACTION_MOVE:
      return RUT_MOTION_EVENT_ACTION_MOVE;
    }
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;
  switch (sdl_event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
      return RUT_MOTION_EVENT_ACTION_DOWN;
    case SDL_MOUSEBUTTONUP:
      return RUT_MOTION_EVENT_ACTION_UP;
    case SDL_MOUSEMOTION:
      return RUT_MOTION_EVENT_ACTION_MOVE;
    default:
      g_warn_if_reached (); /* Not a motion event */
      return RUT_MOTION_EVENT_ACTION_MOVE;
    }
#else
#error "Unknown input system"
#endif
}

#ifdef USE_SDL
static RutButtonState
rut_button_state_for_sdl_state (SDL_Event *event,
                                uint8_t    sdl_state)
{
  RutButtonState rut_state = 0;
  if (sdl_state & SDL_BUTTON(1))
    rut_state |= RUT_BUTTON_STATE_1;
  if (sdl_state & SDL_BUTTON(2))
    rut_state |= RUT_BUTTON_STATE_2;
  if (sdl_state & SDL_BUTTON(3))
    rut_state |= RUT_BUTTON_STATE_3;

#if SDL_MAJOR_VERSION < 2

  if ((event->type == SDL_MOUSEBUTTONUP ||
       event->type == SDL_MOUSEBUTTONDOWN) &&
      event->button.button == SDL_BUTTON_WHEELUP)
    rut_state |= RUT_BUTTON_STATE_WHEELUP;
  if ((event->type == SDL_MOUSEBUTTONUP ||
       event->type == SDL_MOUSEBUTTONDOWN) &&
      event->button.button == SDL_BUTTON_WHEELDOWN)
    rut_state |= RUT_BUTTON_STATE_WHEELDOWN;

#else /* SDL_MAJOR_VERSION < 2 */

  if (event->type == SDL_MOUSEWHEEL)
    {
      if (event->wheel.y < 0)
        rut_state |= RUT_BUTTON_STATE_WHEELUP;
      else if (event->wheel.y > 0)
        rut_state |= RUT_BUTTON_STATE_WHEELDOWN;
    }

#endif /* SDL_MAJOR_VERSION < 2 */

  return rut_state;
}
#endif /* USE_SDL */

RutButtonState
rut_motion_event_get_button_state (RutInputEvent *event)
{
#ifdef __ANDROID__
  return 0;
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;

  return rut_button_state_for_sdl_state (sdl_event,
                                         SDL_GetMouseState (NULL, NULL));
#if 0
  /* FIXME: we need access to the RutContext here so that
   * we can statefully track the changes to the button
   * mask because the button up and down events don't
   * tell you what other buttons are currently down they
   * only tell you what button changed.
   */

  switch (sdl_event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      switch (sdl_event->motion.button)
        {
        case 1:
          return RUT_BUTTON_STATE_1;
        case 2:
          return RUT_BUTTON_STATE_2;
        case 3:
          return RUT_BUTTON_STATE_3;
        case 4:
          return RUT_BUTTON_STATE_WHEELUP;
        case 5:
          return RUT_BUTTON_STATE_WHEELDOWN;
        default:
          g_warning ("Out of range SDL button number");
          return 0;
        }
    case SDL_MOUSEMOTION:
      return rut_button_state_for_sdl_state (sdl_event->button.state);
    default:
      g_warn_if_reached (); /* Not a motion event */
      return 0;
    }
#endif
#else
#error "Unknown input system"
#endif
}

#ifdef __ANDROID__
RutModifierState
rut_modifier_state_for_android_meta (int32_t meta)
{
  RutModifierState rut_state = 0;

  if (meta & AMETA_ALT_LEFT_ON)
    rut_state |= RUT_MODIFIER_LEFT_ALT_ON;
  if (meta & AMETA_ALT_RIGHT_ON)
    rut_state |= RUT_MODIFIER_RIGHT_ALT_ON;

  if (meta & AMETA_SHIFT_LEFT_ON)
    rut_state |= RUT_MODIFIER_LEFT_SHIFT_ON;
  if (meta & AMETA_SHIFT_RIGHT_ON)
    rut_state |= RUT_MODIFIER_RIGHT_SHIFT_ON;

  return rut_state;
}
#endif

#ifdef USE_SDL
static RutModifierState
rut_sdl_get_modifier_state (void)
{
  RutModifierState rut_state = 0;
#if SDL_MAJOR_VERSION < 2
  SDLMod mod;
#else
  SDL_Keymod mod;
#endif

  mod = SDL_GetModState ();

  if (mod & KMOD_LSHIFT)
    rut_state |= RUT_MODIFIER_LEFT_SHIFT_ON;
  if (mod & KMOD_RSHIFT)
    rut_state |= RUT_MODIFIER_RIGHT_SHIFT_ON;
  if (mod & KMOD_LCTRL)
    rut_state |= RUT_MODIFIER_LEFT_CTRL_ON;
  if (mod & KMOD_RCTRL)
    rut_state |= RUT_MODIFIER_RIGHT_CTRL_ON;
  if (mod & KMOD_LALT)
    rut_state |= RUT_MODIFIER_LEFT_ALT_ON;
  if (mod & KMOD_RALT)
    rut_state |= RUT_MODIFIER_RIGHT_ALT_ON;
  if (mod & KMOD_NUM)
    rut_state |= RUT_MODIFIER_NUM_LOCK_ON;
  if (mod & KMOD_CAPS)
    rut_state |= RUT_MODIFIER_CAPS_LOCK_ON;
#if SDL_MAJOR_VERSION < 2
  if (mod & KMOD_LMETA)
    rut_state |= RUT_MODIFIER_LEFT_META_ON;
  if (mod & KMOD_RMETA)
    rut_state |= RUT_MODIFIER_RIGHT_META_ON;
#endif /* SDL_MAJOR_VERSION < 2 */

  return rut_state;
}
#endif

RutModifierState
rut_key_event_get_modifier_state (RutInputEvent *event)
{
 #ifdef __ANDROID__
  int32_t meta = AKeyEvent_getMetaState (event->native);
  return rut_modifier_state_for_android_meta (meta);
#elif defined (USE_SDL)
  return rut_sdl_get_modifier_state ();
#else
#error "Unknown input system"
  return 0;
#endif
}

RutModifierState
rut_motion_event_get_modifier_state (RutInputEvent *event)
{
#ifdef __ANDROID__
  int32_t meta = AMotionEvent_getMetaState (event->native);
  return rut_modifier_state_for_android_meta (meta);
#elif defined (USE_SDL)
  return rut_sdl_get_modifier_state ();
#else
#error "Unknown input system"
  return 0;
#endif
}

static void
rut_motion_event_get_transformed_xy (RutInputEvent *event,
                                     float *x,
                                     float *y)
{
  const CoglMatrix *transform = event->input_transform;

#ifdef __ANDROID__
  *x = AMotionEvent_getX (event->native, 0);
  *y = AMotionEvent_getY (event->native, 0);
#elif defined (USE_SDL)
  SDL_Event *sdl_event = event->native;
  switch (sdl_event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      *x = sdl_event->button.x;
      *y = sdl_event->button.y;
      break;
    case SDL_MOUSEMOTION:
      *x = sdl_event->motion.x;
      *y = sdl_event->motion.y;
      break;
    default:
      g_warn_if_reached (); /* Not a motion event */
      return;
    }
#else
#error "Unknown input system"
#endif

  if (transform)
    {
      *x = transform->xx * *x + transform->xy * *y + transform->xw;
      *y = transform->yx * *x + transform->yy * *y + transform->yw;
    }
}

float
rut_motion_event_get_x (RutInputEvent *event)
{
  float x, y;

  rut_motion_event_get_transformed_xy (event, &x, &y);

  return x;
}

float
rut_motion_event_get_y (RutInputEvent *event)
{
  float x, y;

  rut_motion_event_get_transformed_xy (event, &x, &y);

  return y;
}

CoglBool
rut_motion_event_unproject (RutInputEvent *event,
                            RutObject *graphable,
                            float *x,
                            float *y)
{
  CoglMatrix transform;
  CoglMatrix inverse_transform;
  RutCamera *camera = rut_input_event_get_camera (event);

  rut_graphable_get_modelview (graphable, camera, &transform);

  if (!cogl_matrix_get_inverse (&transform, &inverse_transform))
    return FALSE;

  *x = rut_motion_event_get_x (event);
  *y = rut_motion_event_get_y (event);
  rut_camera_unproject_coord (camera,
                              &transform,
                              &inverse_transform,
                              0, /* object_coord_z */
                              x,
                              y);

  return TRUE;
}

RutObject *
rut_drop_offer_event_get_payload (RutInputEvent *event)
{
  return event->shell->drag_payload;
}

const char *
rut_text_event_get_text (RutInputEvent *event)
{
#ifdef __ANDROID__

  return "";

#elif defined (USE_SDL)

  SDL_Event *sdl_event = event->native;

#if SDL_MAJOR_VERSION < 2

  /* The UTF-8 representation of the single unicode character will
   * have been stuffed into the space for the keysym */
  return (char *) &sdl_event->key.keysym;

#else /* SDL_MAJOR_VERSION */

  return sdl_event->text.text;

#endif /* SDL_MAJOR_VERSION */

#else
#error "Unknown input system"
#endif
}

static void
poly_init_from_rectangle (float *poly,
                          float x0,
                          float y0,
                          float x1,
                          float y1)
{
  poly[0] = x0;
  poly[1] = y0;
  poly[2] = 0;
  poly[3] = 1;

  poly[4] = x0;
  poly[5] = y1;
  poly[6] = 0;
  poly[7] = 1;

  poly[8] = x1;
  poly[9] = y1;
  poly[10] = 0;
  poly[11] = 1;

  poly[12] = x1;
  poly[13] = y0;
  poly[14] = 0;
  poly[15] = 1;
}

/* Given an (x0,y0) (x1,y1) rectangle this transforms it into
 * a polygon in window coordinates that can be intersected
 * with input coordinates for picking.
 */
static void
rect_to_screen_polygon (float x0,
                        float y0,
                        float x1,
                        float y1,
                        const CoglMatrix *modelview,
                        const CoglMatrix *projection,
                        const float *viewport,
                        float *poly)
{
  poly_init_from_rectangle (poly, x0, y0, x1, y1);

  rut_util_fully_transform_points (modelview,
                                   projection,
                                   viewport,
                                   poly,
                                   4);
}

typedef struct _CameraPickState
{
  RutCamera *camera;
  RutInputEvent *event;
  float x, y;
  RutObject *picked_object;
} CameraPickState;

static RutTraverseVisitFlags
camera_pre_pick_region_cb (RutObject *object,
                           int depth,
                           void *user_data)
{
  CameraPickState *state = user_data;

  if (rut_object_get_type (object) == &rut_ui_viewport_type)
    {
      RutUIViewport *ui_viewport = RUT_UI_VIEWPORT (object);
      CoglMatrix transform;
      float poly[16];
      RutObject *parent = rut_graphable_get_parent (object);
      const CoglMatrix *view = rut_camera_get_view_transform (state->camera);

      transform = *view;
      rut_graphable_apply_transform (parent, &transform);

      rect_to_screen_polygon (0, 0,
                              rut_ui_viewport_get_width (ui_viewport),
                              rut_ui_viewport_get_height (ui_viewport),
                              &transform,
                              &state->camera->projection,
                              state->camera->viewport,
                              poly);

      if (!rut_util_point_in_screen_poly (state->x, state->y,
                                          poly, sizeof (float) * 4, 4))
        return RUT_TRAVERSE_VISIT_SKIP_CHILDREN;
    }

  if (rut_object_is (object, RUT_INTERFACE_ID_PICKABLE) &&
      rut_pickable_pick (object,
                          state->camera,
                          NULL, /* pre-computed modelview */
                          state->x, state->y))
    {
      state->picked_object = object;
    }

  return RUT_TRAVERSE_VISIT_CONTINUE;
}

static RutObject *
_rut_shell_get_scenegraph_event_target (RutShell *shell,
                                        RutInputEvent *event)
{
  RutObject *picked_object = NULL;
  RutCamera *picked_camera = NULL;
  GList *l;

  /* Key events by default go to the object that has key focus. If
   * there is no object with key focus then we will let them go to
   * whichever object the pointer is over to implement a kind of
   * sloppy focus as a fallback */
  if (shell->keyboard_focus_object &&
      (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY ||
       rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_TEXT))
    return shell->keyboard_focus_object;

  for (l = shell->input_cameras; l; l = l->next)
    {
      InputCamera *input_camera = l->data;
      RutCamera *camera = input_camera->camera;
      RutObject *scenegraph = input_camera->scenegraph;
      float x = shell->mouse_x;
      float y = shell->mouse_y;
      CameraPickState state;

      event->camera = camera;
      event->input_transform = &camera->input_transform;

      if (scenegraph)
        {
          state.camera = camera;
          state.event = event;
          state.x = x;
          state.y = y;
          state.picked_object = NULL;

          rut_graphable_traverse (scenegraph,
                                  RUT_TRAVERSE_DEPTH_FIRST,
                                  camera_pre_pick_region_cb,
                                  NULL, /* post_children_cb */
                                  &state);

          if (state.picked_object)
            {
              picked_object = state.picked_object;
              picked_camera = camera;
            }
        }
    }

  if (picked_object)
    {
      event->camera = picked_camera;
      event->input_transform = &picked_camera->input_transform;
    }

  return picked_object;
}

static void
cancel_current_drop_offer_taker (RutShell *shell)
{
  RutInputEvent drop_cancel;
  RutInputEventStatus status;

  if (!shell->drop_offer_taker)
    return;

  drop_cancel.type = RUT_INPUT_EVENT_TYPE_DROP_CANCEL;
  drop_cancel.shell = shell;
  drop_cancel.native = NULL;
  drop_cancel.camera = NULL;
  drop_cancel.input_transform = NULL;

  status = rut_inputable_handle_event (shell->drop_offer_taker, &drop_cancel);

  g_warn_if_fail (status == RUT_INPUT_EVENT_STATUS_HANDLED);

  rut_refable_unref (shell->drop_offer_taker);
  shell->drop_offer_taker = NULL;
}

static RutShellOnscreen *
get_shell_onscreen (RutShell *shell,
                    CoglOnscreen *onscreen)
{
  RutShellOnscreen *shell_onscreen;

  rut_list_for_each (shell_onscreen, &shell->onscreens, link)
    if (shell_onscreen->onscreen == onscreen)
      return shell_onscreen;

  return NULL;
}

static RutInputEventStatus
_rut_shell_handle_input (RutShell *shell, RutInputEvent *event)
{
  RutInputEventStatus status = RUT_INPUT_EVENT_STATUS_UNHANDLED;
  GList *l;
  RutClosure *c, *tmp;
  RutObject *target;
  RutShellGrab *grab;
  CoglOnscreen *onscreen = NULL;
  RutShellOnscreen *shell_onscreen = NULL;

  onscreen = rut_input_event_get_onscreen (event);

  if (onscreen)
    shell_onscreen = get_shell_onscreen (shell, onscreen);

  /* Keep track of the last known position of the mouse so that we can
   * send key events to whatever is under the mouse when there is no
   * key focus */
  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      shell->mouse_x = rut_motion_event_get_x (event);
      shell->mouse_y = rut_motion_event_get_y (event);

      /* Keep track of whether any handlers set a cursor in response to
       * the motion event */
      if (shell_onscreen)
        shell_onscreen->cursor_set = FALSE;

      if (shell->drag_payload)
        {
          event->type = RUT_INPUT_EVENT_TYPE_DROP_OFFER;
          _rut_shell_handle_input (shell, event);
          event->type = RUT_INPUT_EVENT_TYPE_MOTION;
        }
    }
  else if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_DROP_OFFER)
    cancel_current_drop_offer_taker (shell);
  else if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_DROP)
    {
      if (shell->drop_offer_taker)
        {
          RutInputEventStatus status =
            rut_inputable_handle_event (shell->drop_offer_taker, event);

          if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
            goto handled;
        }
    }

  event->camera = shell->window_camera;

  rut_list_for_each_safe (c, tmp, &shell->input_cb_list, list_node)
    {
      RutInputCallback cb = c->function;

      status = cb (event, c->user_data);
      if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
        goto handled;
    }

  rut_list_for_each_safe (grab, shell->next_grab, &shell->grabs, list_node)
    {
      RutCamera *old_camera = event->camera;
      RutInputEventStatus grab_status;

      if (grab->camera)
        event->camera = grab->camera;

      grab_status = grab->callback (event, grab->user_data);

      event->camera = old_camera;

      if (grab_status == RUT_INPUT_EVENT_STATUS_HANDLED)
        {
          status = RUT_INPUT_EVENT_STATUS_HANDLED;
          goto handled;
        }
    }

  for (l = shell->input_cameras; l; l = l->next)
    {
      InputCamera *input_camera = l->data;
      RutCamera *camera = input_camera->camera;
      GList *l2;

      event->camera = camera;
      event->input_transform = &camera->input_transform;

      for (l2 = camera->input_regions; l2; l2 = l2->next)
        {
          RutInputRegion *region = l2->data;

          if (rut_pickable_pick (region,
                                 camera,
                                 NULL, /* pre-computed modelview */
                                 shell->mouse_x,
                                 shell->mouse_y))
            {
              status = rut_inputable_handle_event (region, event);

              if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
                goto handled;
            }
        }
    }

  /* If nothing has handled the event by now we'll try to pick a
   * single object from the scenegraphs attached to the cameras that
   * will handle the event */
  target = _rut_shell_get_scenegraph_event_target (shell, event);

  /* Send the event to the object we found. If it doesn't handle it
   * then we'll also bubble the event up to any inputable parents of
   * the object until one of them handles it */
  while (target)
    {
      if (rut_object_is (target, RUT_INTERFACE_ID_INPUTABLE))
        {
          status = rut_inputable_handle_event (target, event);

          if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
            break;
        }

      if (!rut_object_is (target, RUT_INTERFACE_ID_GRAPHABLE))
        break;

      target = rut_graphable_get_parent (target);
    }

 handled:

  /* If nothing set a cursor in response to the motion event then
   * we'll reset it back to the default pointer */
  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
      shell_onscreen &&
      !shell_onscreen->cursor_set)
    rut_shell_set_cursor (shell, onscreen, RUT_CURSOR_ARROW);

  return status;
}


#ifdef __ANDROID__

/**
 * Process the next input event.
 */
static int32_t
android_handle_input (struct android_app* app, AInputEvent *event)
{
  RutInputEvent rut_event;
  RutShell *shell = (RutShell *)app->userData;

  rut_event.native = event;
  rut_event.shell = shell;
  rut_event.input_transform = NULL;

  switch (AInputEvent_getType (event))
    {
    case AINPUT_EVENT_TYPE_MOTION:
      rut_event.type = RUT_INPUT_EVENT_TYPE_MOTION;
      break;

    case AINPUT_EVENT_TYPE_KEY:
      rut_event.type = RUT_INPUT_EVENT_TYPE_KEY;
      break;

    default:
      return 0;
    }

  if (_rut_shell_handle_input (shell, &rut_event) ==
      RUT_INPUT_EVENT_STATUS_HANDLED)
    return 1;

  return 0;
}

static int
android_init (RutShell *shell)
{
  cogl_android_set_native_window (shell->app->window);

  shell->init_cb (shell, shell->user_data);
  return 0;
}

/**
 * Process the next main command.
 */
static void
android_handle_cmd (struct android_app *app,
                    int32_t cmd)
{
  RutShell *shell = (RutShell *) app->userData;

  switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
      /* The window is being shown, get it ready */
      g_message ("command: INIT_WINDOW");
      if (shell->app->window != NULL)
        {
          android_init (shell);
          _rut_shell_paint (shell);
        }
      break;

    case APP_CMD_TERM_WINDOW:
      /* The window is being hidden or closed, clean it up */
      g_message ("command: TERM_WINDOW");
      _rut_shell_fini (shell);
      break;

    case APP_CMD_GAINED_FOCUS:
      g_message ("command: GAINED_FOCUS");
      break;

    case APP_CMD_LOST_FOCUS:
      g_message ("command: LOST_FOCUS");
      _rut_shell_paint (shell);
      break;
    }
}
#endif

static void
_rut_shell_remove_grab_link (RutShell *shell,
                             RutShellGrab *grab)
{
  if (grab->camera)
    rut_refable_unref (grab->camera);

  /* If we are in the middle of iterating the grab callbacks then this
   * will make it cope with removing arbritrary nodes from the list
   * while iterating it */
  if (shell->next_grab == grab)
    shell->next_grab = rut_container_of (grab->list_node.next,
                                         grab,
                                         list_node);

  rut_list_remove (&grab->list_node);

  g_slice_free (RutShellGrab, grab);
}

RutType rut_shell_type;

static void
_rut_shell_free (void *object)
{
  RutShell *shell = object;

  rut_closure_list_disconnect_all (&shell->input_cb_list);

  while (!rut_list_empty (&shell->grabs))
    {
      RutShellGrab *first_grab =
        rut_container_of (shell->grabs.next, first_grab, list_node);

      _rut_shell_remove_grab_link (shell, first_grab);
    }

  _rut_shell_remove_all_input_cameras (shell);

  _rut_shell_fini (shell);

  g_free (shell);
}

RutRefableVTable _rut_shell_refable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_shell_free
};


static void
_rut_shell_init_types (void)
{
  rut_type_init (&rut_shell_type, "RigShell");
  rut_type_add_interface (&rut_shell_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutShell, ref_count),
                          &_rut_shell_refable_vtable);

  _rut_slider_init_type ();
}

RutShell *
rut_shell_new (RutShellInitCallback init,
               RutShellFiniCallback fini,
               RutShellPaintCallback paint,
               void *user_data)
{
  static CoglBool initialized = FALSE;
  RutShell *shell;

  /* Make sure core types are registered */
  _rut_init ();

  shell = g_new0 (RutShell, 1);

  if (G_UNLIKELY (initialized == FALSE))
    _rut_shell_init_types ();

  shell->ref_count = 1;

  rut_object_init (&shell->_parent, &rut_shell_type);

  rut_list_init (&shell->input_cb_list);
  rut_list_init (&shell->grabs);
  rut_list_init (&shell->onscreens);

  shell->init_cb = init;
  shell->fini_cb = fini;
  shell->paint_cb = paint;
  shell->user_data = user_data;

  rut_list_init (&shell->pre_paint_callbacks);
  shell->flushing_pre_paints = FALSE;

  return shell;
}

/* Note: we don't take a reference on the context so we don't
 * introduce a circular reference. */
void
_rut_shell_associate_context (RutShell *shell,
                              RutContext *context)
{
  shell->rut_ctx = context;
}

void
_rut_shell_init (RutShell *shell)
{
#if defined(USE_SDL) && SDL_MAJOR_VERSION < 2
  SDL_EnableUNICODE (1);
#endif
}

void
rut_shell_set_window_camera (RutShell *shell, RutCamera *window_camera)
{
  shell->window_camera = window_camera;
}

#ifdef __ANDROID__

RutShell *
rut_android_shell_new (struct android_app* application,
                       RutShellInitCallback init,
                       RutShellFiniCallback fini,
                       RutShellPaintCallback paint,
                       void *user_data)
{
  RutShell *shell = rut_shell_new (init, fini, paint, user_data);

  shell->app = application;
  application->userData = shell;
  application->onAppCmd = android_handle_cmd;
  application->onInputEvent = android_handle_input;
}

#endif

void
rut_shell_grab_key_focus (RutShell *shell,
                          RutObject *inputable,
                          GDestroyNotify ungrab_callback)
{
  g_return_if_fail (rut_object_is (inputable, RUT_INTERFACE_ID_INPUTABLE));

  /* If something tries to set the keyboard focus to the same object
   * then we probably do still want to call the keyboard ungrab
   * callback for the last object that set it. The code may be
   * assuming that when this function is called it definetely has the
   * keyboard focus and that the callback will definetely called at
   * some point. Otherwise this function is more like a request and it
   * should have a way of reporting whether the request succeeded */

  if (rut_object_is (inputable, RUT_INTERFACE_ID_REF_COUNTABLE))
    rut_refable_ref (inputable);

  rut_shell_ungrab_key_focus (shell);

  shell->keyboard_focus_object = inputable;
  shell->keyboard_ungrab_cb = ungrab_callback;
}

void
rut_shell_ungrab_key_focus (RutShell *shell)
{
  if (shell->keyboard_focus_object)
    {
      if (shell->keyboard_ungrab_cb)
        shell->keyboard_ungrab_cb (shell->keyboard_focus_object);

      if (rut_object_is (shell->keyboard_focus_object,
                         RUT_INTERFACE_ID_REF_COUNTABLE))
        rut_refable_unref (shell->keyboard_focus_object);

      shell->keyboard_focus_object = NULL;
      shell->keyboard_ungrab_cb = NULL;
    }
}

void
rut_shell_set_cursor (RutShell *shell,
                      CoglOnscreen *onscreen,
                      RutCursor cursor)
{
  RutShellOnscreen *shell_onscreen;

  shell_onscreen = get_shell_onscreen (shell, onscreen);

  if (shell_onscreen == NULL)
    return;

  if (shell_onscreen->current_cursor != cursor)
    {
#if defined(USE_SDL) && SDL_MAJOR_VERSION >= 2
      SDL_Cursor *cursor_image;
      SDL_SystemCursor system_cursor;

      switch (cursor)
        {
        case RUT_CURSOR_ARROW:
          system_cursor = SDL_SYSTEM_CURSOR_ARROW;
          break;
        case RUT_CURSOR_IBEAM:
          system_cursor = SDL_SYSTEM_CURSOR_IBEAM;
          break;
        case RUT_CURSOR_WAIT:
          system_cursor = SDL_SYSTEM_CURSOR_WAIT;
          break;
        case RUT_CURSOR_CROSSHAIR:
          system_cursor = SDL_SYSTEM_CURSOR_CROSSHAIR;
          break;
        case RUT_CURSOR_SIZE_WE:
          system_cursor = SDL_SYSTEM_CURSOR_SIZEWE;
          break;
        case RUT_CURSOR_SIZE_NS:
          system_cursor = SDL_SYSTEM_CURSOR_SIZENS;
          break;
        }

      cursor_image = SDL_CreateSystemCursor (system_cursor);
      SDL_SetCursor (cursor_image);

      if (shell_onscreen->cursor_image)
        SDL_FreeCursor (shell_onscreen->cursor_image);
      shell_onscreen->cursor_image = cursor_image;
#endif

      shell_onscreen->current_cursor = cursor;
    }

  shell_onscreen->cursor_set = TRUE;
}

void
rut_shell_set_title (RutShell *shell,
                     CoglOnscreen *onscreen,
                     const char *title)
{
#if defined(USE_SDL)
#if SDL_MAJOR_VERSION < 2
  SDL_WM_SetCaption (title, title);
#else /* SDL_MAJOR_VERSION */
  SDL_Window *window = cogl_sdl_onscreen_get_window (onscreen);
  SDL_SetWindowTitle (window, title);
#endif /* SDL_MAJOR_VERSION */
#endif /* USE_SDL */
}

static void
update_pre_paint_entry_depth (RutShellPrePaintEntry *entry)
{
  RutObject *parent;

  entry->depth = 0;

  if (!entry->graphable)
    return;

  for (parent = rut_graphable_get_parent (entry->graphable);
       parent;
       parent = rut_graphable_get_parent (parent))
    entry->depth++;
}

static int
compare_entry_depth_cb (const void *a,
                        const void *b)
{
  RutShellPrePaintEntry *entry_a = *(RutShellPrePaintEntry **) a;
  RutShellPrePaintEntry *entry_b = *(RutShellPrePaintEntry **) b;

  return entry_a->depth - entry_b->depth;
}

static void
sort_pre_paint_callbacks (RutShell *shell)
{
  RutShellPrePaintEntry **entry_ptrs;
  RutShellPrePaintEntry *entry;
  int i = 0, n_entries = 0;

  rut_list_for_each (entry, &shell->pre_paint_callbacks, list_node)
    {
      update_pre_paint_entry_depth (entry);
      n_entries++;
    }

  entry_ptrs = g_alloca (sizeof (RutShellPrePaintEntry *) * n_entries);

  rut_list_for_each (entry, &shell->pre_paint_callbacks, list_node)
    entry_ptrs[i++] = entry;

  qsort (entry_ptrs,
         n_entries,
         sizeof (RutShellPrePaintEntry *),
         compare_entry_depth_cb);

  /* Reconstruct the list from the sorted array */
  rut_list_init (&shell->pre_paint_callbacks);
  for (i = 0; i < n_entries; i++)
    rut_list_insert (shell->pre_paint_callbacks.prev,
                     &entry_ptrs[i]->list_node);
}

static void
flush_pre_paint_callbacks (RutShell *shell)
{
  /* This doesn't support recursive flushing */
  g_return_if_fail (!shell->flushing_pre_paints);

  sort_pre_paint_callbacks (shell);

  /* Mark that we're in the middle of flushing so that subsequent adds
   * will keep the list sorted by depth */
  shell->flushing_pre_paints = TRUE;

  while (!rut_list_empty (&shell->pre_paint_callbacks))
    {
      RutShellPrePaintEntry *entry =
        rut_container_of (shell->pre_paint_callbacks.next,
                          entry,
                          list_node);

      rut_list_remove (&entry->list_node);

      entry->callback (entry->graphable, entry->user_data);

      g_slice_free (RutShellPrePaintEntry, entry);
    }

  shell->flushing_pre_paints = FALSE;
}

static void
_rut_shell_paint (RutShell *shell)
{
  GSList *l;

  g_return_if_fail (shell->redraw_queued == TRUE);

  shell->redraw_queued = FALSE;
#ifndef __ANDROID__
  g_source_remove (shell->glib_paint_idle);
  shell->glib_paint_idle = 0;
#endif

  for (l = shell->rut_ctx->timelines; l; l = l->next)
    _rut_timeline_update (l->data);

  flush_pre_paint_callbacks (shell);

#warning "fixme: input should be handled once per frame after pre-paint callbacks"

  if (shell->paint_cb (shell, shell->user_data))
    goto queue_redraw;

  for (l = shell->rut_ctx->timelines; l; l = l->next)
    if (rut_timeline_is_running (l->data))
      goto queue_redraw;

  return;

queue_redraw:
  rut_shell_queue_redraw (shell);
}

#ifdef USE_SDL

static void
sdl_handle_event (RutShell *shell, SDL_Event *event)
{
  RutInputEvent rut_event;

  rut_event.native = event;
  rut_event.shell = shell;
  rut_event.input_transform = NULL;

  switch (event->type)
    {
#if SDL_MAJOR_VERSION < 2
    case SDL_VIDEOEXPOSE:
      rut_shell_queue_redraw (shell);
      break;
#else /* SDL_MAJOR_VERSION < 2 */
    case SDL_WINDOWEVENT:
      switch (event->window.event)
        {
        case SDL_WINDOWEVENT_EXPOSED:
          rut_shell_queue_redraw (shell);
          break;

        case SDL_WINDOWEVENT_CLOSE:
          rut_shell_quit (shell);
          break;
        }
      break;
#endif /* SDL_MAJOR_VERSION < 2 */

    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      rut_event.type = RUT_INPUT_EVENT_TYPE_MOTION;
      _rut_shell_handle_input (shell, &rut_event);
      break;

    case SDL_KEYUP:
#if SDL_MAJOR_VERSION >= 2
    case SDL_KEYDOWN:
#endif
      rut_event.type = RUT_INPUT_EVENT_TYPE_KEY;
      _rut_shell_handle_input (shell, &rut_event);
      break;

#if SDL_MAJOR_VERSION >= 2

    case SDL_TEXTINPUT:
      rut_event.type = RUT_INPUT_EVENT_TYPE_TEXT;
      _rut_shell_handle_input (shell, &rut_event);
      break;

#else /* SDL_MAJOR_VERSION */

    case SDL_KEYDOWN:
      {
        RutInputEventStatus status;

        rut_event.type = RUT_INPUT_EVENT_TYPE_KEY;
        status = _rut_shell_handle_input (shell, &rut_event);

        if (status == RUT_INPUT_EVENT_STATUS_UNHANDLED &&
            event->key.keysym.unicode > 0)
          {
            gunichar unicode;
            char *utf8_data;
            int utf8_len;

            /* Also treat the event as a text event. We'll stuff the
             * UTF-8 representation of the unicode character over the
             * top of the keysym field. For this to work, the size of
             * the event union needs to have at least 7 extra bytes
             * after the key sym data */
            G_STATIC_ASSERT (sizeof (SDL_Event) -
                             (offsetof (SDL_Event, key.keysym) >= 7));

            unicode = event->key.keysym.unicode;
            utf8_data = (char *) &event->key.keysym;
            utf8_len = g_unichar_to_utf8 (unicode, utf8_data);
            utf8_data[utf8_len] = '\0';

            rut_event.type = RUT_INPUT_EVENT_TYPE_TEXT;
            rut_event.input_transform = NULL;
            _rut_shell_handle_input (shell, &rut_event);
          }
      }
      break;

#endif /* SDL_MAJOR_VERSION */

    case SDL_QUIT:
      rut_shell_quit (shell);
      break;
    }
}

typedef struct _SDLSource
{
  GSource source;

  RutShell *shell;

} SDLSource;

static gboolean
sdl_glib_source_prepare (GSource *source, int *timeout)
{
  if (SDL_PollEvent (NULL))
    return TRUE;

  *timeout = 8;

  return FALSE;
}

static gboolean
sdl_glib_source_check (GSource *source)
{
  if (SDL_PollEvent (NULL))
    return TRUE;

  return FALSE;
}

static gboolean
sdl_glib_source_dispatch (GSource *source,
                           GSourceFunc callback,
                           void *user_data)
{
  SDLSource *sdl_source = (SDLSource *) source;
  SDL_Event event;

  while (SDL_PollEvent (&event))
    {
      cogl_sdl_handle_event (sdl_source->shell->rut_ctx->cogl_context,
                             &event);

      sdl_handle_event (sdl_source->shell, &event);
    }

  return TRUE;
}

static GSourceFuncs
sdl_glib_source_funcs =
  {
    sdl_glib_source_prepare,
    sdl_glib_source_check,
    sdl_glib_source_dispatch,
    NULL
  };

static GSource *
sdl_glib_source_new (RutShell *shell, int priority)
{
  GSource *source = g_source_new (&sdl_glib_source_funcs, sizeof (SDLSource));
  SDLSource *sdl_source = (SDLSource *)source;

  sdl_source->shell = shell;

  return source;
}

static GPollFunc rut_sdl_original_poll;

int
sdl_poll_wrapper (GPollFD *ufds,
                  guint nfsd,
                  gint timeout_)
{
  cogl_sdl_idle (rut_cogl_context);

  return rut_sdl_original_poll (ufds, nfsd, timeout_);
}
#endif

static CoglBool
glib_paint_cb (void *user_data)
{
  _rut_shell_paint (user_data);
  return FALSE;
}

static void
destroy_onscreen_cb (void *user_data)
{
  RutShellOnscreen *shell_onscreen = user_data;

  rut_list_remove (&shell_onscreen->link);
}

void
rut_shell_add_onscreen (RutShell *shell,
                        CoglOnscreen *onscreen)
{
  RutShellOnscreen *shell_onscreen = g_slice_new0 (RutShellOnscreen);
  static CoglUserDataKey data_key;

  shell_onscreen->onscreen = onscreen;
  cogl_object_set_user_data (COGL_OBJECT (onscreen),
                             &data_key,
                             shell_onscreen,
                             destroy_onscreen_cb);
  rut_list_insert (&shell->onscreens, &shell_onscreen->link);
}

void
rut_shell_main (RutShell *shell)
{
#ifdef __ANDROID__
  while (!shell->quit)
    {
      int events;
      struct android_poll_source* source;

      while (!shell->quit)
        {
          int status;

          status = ALooper_pollAll (0, NULL, &events, (void**)&source);
          if (status == ALOOPER_POLL_TIMEOUT)
            {
              if (shell->redraw_queued)
                break;

              /* Idle now
               * FIXME: cogl_android_idle (shell->ctx)
               */

              status = ALooper_pollAll (-1, NULL, &events, (void**)&source);
            }

          if (status == ALOOPER_POLL_ERROR)
            {
              g_error ("Error waiting for polling for events");
              return;
            }

          if (shell->app->destroyRequested != 0)
            {
              shell->fini_cb (shell, shell->user_data);
              return;
            }

          if (source != NULL)
            source->process (shell->app, source);
        }

      _rut_shell_paint (shell);
    }

#else

  GSource *cogl_source;

  shell->init_cb (shell, shell->user_data);

  cogl_source = cogl_glib_source_new (shell->rut_ctx->cogl_context,
                                      G_PRIORITY_DEFAULT);
  g_source_attach (cogl_source, NULL);

#ifdef USE_SDL

  {
    GSource *sdl_source;

    rut_sdl_original_poll =
      g_main_context_get_poll_func (g_main_context_default ());
    g_main_context_set_poll_func (g_main_context_default (),
                                  sdl_poll_wrapper);
    sdl_source = sdl_glib_source_new (shell, G_PRIORITY_DEFAULT);
    g_source_attach (sdl_source, NULL);
  }
#endif

  {
    GApplication *application = g_application_get_default ();

    /* If the application has created a GApplication then we'll run
     * that instead of running our own mainloop directly */
    if (application)
      g_application_run (application, 0, NULL);
    else
      {
        shell->main_loop = g_main_loop_new (NULL, TRUE);
        g_main_loop_run (shell->main_loop);
        g_main_loop_unref (shell->main_loop);
      }
  }

  shell->fini_cb (shell, shell->user_data);

#ifdef USE_SDL
  g_main_context_set_poll_func (g_main_context_default (),
                                rut_sdl_original_poll);
#endif /* USE_SDL */

#endif
}

void
rut_shell_grab_input (RutShell *shell,
                      RutCamera *camera,
                      RutInputCallback callback,
                      void *user_data)
{
  RutShellGrab *grab = g_slice_new (RutShellGrab);

  grab->callback = callback;
  grab->user_data = user_data;

  if (camera)
    grab->camera = rut_refable_ref (camera);
  else
    grab->camera = NULL;

  rut_list_insert (&shell->grabs, &grab->list_node);
}

void
rut_shell_ungrab_input (RutShell *shell,
                        RutInputCallback callback,
                        void *user_data)
{
  RutShellGrab *grab;

  rut_list_for_each (grab, &shell->grabs, list_node)
    if (grab->callback == callback && grab->user_data == user_data)
      {
        _rut_shell_remove_grab_link (shell, grab);
        break;
      }
}

void
rut_shell_queue_redraw (RutShell *shell)
{
  shell->redraw_queued = TRUE;

#ifndef __ANDROID__
  if (shell->glib_paint_idle <= 0)
    shell->glib_paint_idle = g_idle_add (glib_paint_cb, shell);
#endif
}

enum {
  RUT_SLIDER_PROP_PROGRESS,
  RUT_SLIDER_N_PROPS
};

struct _RutSlider
{
  RutObjectProps _parent;
  int ref_count;

  /* FIXME: It doesn't seem right that we should have to save a
   * pointer to the context for input here... */
  RutContext *ctx;

  /* FIXME: It also doesn't seem right to have to save a pointer
   * to the camera here so we can queue a redraw */
  //RutCamera *camera;

  RutGraphableProps graphable;
  RutPaintableProps paintable;
  RutSimpleIntrospectableProps introspectable;

  RutNineSlice *background;
  RutNineSlice *handle;
  RutTransform *handle_transform;

  RutInputRegion *input_region;
  float grab_x;
  float grab_y;
  float grab_progress;

  RutAxis axis;
  float range_min;
  float range_max;
  float length;
  float progress;

  RutProperty properties[RUT_SLIDER_N_PROPS];
};

static RutPropertySpec _rut_slider_prop_specs[] = {
  {
    .name = "progress",
    .flags = RUT_PROPERTY_FLAG_READWRITE,
    .type = RUT_PROPERTY_TYPE_FLOAT,
    .data_offset = offsetof (RutSlider, progress),
    .setter.float_type = rut_slider_set_progress
  },
  { 0 } /* XXX: Needed for runtime counting of the number of properties */
};

static void
_rut_slider_free (void *object)
{
  RutSlider *slider = object;

  rut_refable_unref (slider->input_region);

  rut_graphable_remove_child (slider->handle_transform);

  rut_refable_unref (slider->handle_transform);
  rut_refable_unref (slider->handle);
  rut_refable_unref (slider->background);

  rut_simple_introspectable_destroy (slider);

  rut_graphable_destroy (slider);

  g_slice_free (RutSlider, object);
}

RutRefableVTable _rut_slider_refable_vtable = {
  rut_refable_simple_ref,
  rut_refable_simple_unref,
  _rut_slider_free
};

static RutGraphableVTable _rut_slider_graphable_vtable = {
  NULL, /* child remove */
  NULL, /* child add */
  NULL /* parent changed */
};

static void
_rut_slider_paint (RutObject *object, RutPaintContext *paint_ctx)
{
  RutSlider *slider = RUT_SLIDER (object);
  RutPaintableVTable *bg_paintable =
    rut_object_get_vtable (slider->background, RUT_INTERFACE_ID_PAINTABLE);

  bg_paintable->paint (RUT_OBJECT (slider->background), paint_ctx);
}

static RutPaintableVTable _rut_slider_paintable_vtable = {
  _rut_slider_paint
};

static RutIntrospectableVTable _rut_slider_introspectable_vtable = {
  rut_simple_introspectable_lookup_property,
  rut_simple_introspectable_foreach_property
};

#if 0
static void
_rut_slider_set_camera (RutObject *object,
                        RutCamera *camera)
{
  RutSlider *slider = RUT_SLIDER (object);

  if (camera)
    rut_camera_add_input_region (camera, slider->input_region);

  slider->camera = camera;
}
#endif

RutType rut_slider_type;

static void
_rut_slider_init_type (void)
{
  rut_type_init (&rut_slider_type, "RigSlider");
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (RutSlider, ref_count),
                          &_rut_slider_refable_vtable);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_GRAPHABLE,
                          offsetof (RutSlider, graphable),
                          &_rut_slider_graphable_vtable);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_PAINTABLE,
                          offsetof (RutSlider, paintable),
                          &_rut_slider_paintable_vtable);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_INTROSPECTABLE,
                          0, /* no implied properties */
                          &_rut_slider_introspectable_vtable);
  rut_type_add_interface (&rut_slider_type,
                          RUT_INTERFACE_ID_SIMPLE_INTROSPECTABLE,
                          offsetof (RutSlider, introspectable),
                          NULL); /* no implied vtable */
}

static RutInputEventStatus
_rut_slider_grab_input_cb (RutInputEvent *event,
                           void *user_data)
{
  RutSlider *slider = user_data;

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutShell *shell = slider->ctx->shell;
      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP)
        {
          rut_shell_ungrab_input (shell,
                                  _rut_slider_grab_input_cb,
                                  user_data);
          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_MOVE)
        {
          float diff;
          float progress;

          if (slider->axis == RUT_AXIS_X)
            diff = rut_motion_event_get_x (event) - slider->grab_x;
          else
            diff = rut_motion_event_get_y (event) - slider->grab_y;

          progress = CLAMP (slider->grab_progress + diff / slider->length, 0, 1);

          rut_slider_set_progress (slider, progress);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static RutInputEventStatus
_rut_slider_input_cb (RutInputRegion *region,
                      RutInputEvent *event,
                      void *user_data)
{
  RutSlider *slider = user_data;

  g_print ("Slider input\n");

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION &&
     rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_DOWN)
    {
      RutShell *shell = slider->ctx->shell;
      rut_shell_grab_input (shell,
                            rut_input_event_get_camera (event),
                            _rut_slider_grab_input_cb,
                            slider);
      slider->grab_x = rut_motion_event_get_x (event);
      slider->grab_y = rut_motion_event_get_y (event);
      slider->grab_progress = slider->progress;
      return RUT_INPUT_EVENT_STATUS_HANDLED;
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutSlider *
rut_slider_new (RutContext *ctx,
                RutAxis axis,
                float min,
                float max,
                float length)
{
  RutSlider *slider = g_slice_new0 (RutSlider);
  CoglTexture *bg_texture;
  CoglTexture *handle_texture;
  GError *error = NULL;
  //PangoRectangle label_size;
  float width;
  float height;

  rut_object_init (&slider->_parent, &rut_slider_type);

  slider->ref_count = 1;

  rut_graphable_init (RUT_OBJECT (slider));
  rut_paintable_init (RUT_OBJECT (slider));

  slider->ctx = ctx;

  slider->axis = axis;
  slider->range_min = min;
  slider->range_max = max;
  slider->length = length;
  slider->progress = 0;

  bg_texture =
    rut_load_texture_from_data_file (ctx, "slider-background.png", &error);
  if (!bg_texture)
    {
      g_warning ("Failed to load slider-background.png: %s", error->message);
      g_error_free (error);
    }

  handle_texture =
    rut_load_texture_from_data_file (ctx, "slider-handle.png", &error);
  if (!handle_texture)
    {
      g_warning ("Failed to load slider-handle.png: %s", error->message);
      g_error_free (error);
    }

  if (axis == RUT_AXIS_X)
    {
      width = length;
      height = 20;
    }
  else
    {
      width = 20;
      height = length;
    }

  slider->background = rut_nine_slice_new (ctx, bg_texture, 2, 3, 3, 3,
                                           width, height);

  if (axis == RUT_AXIS_X)
    width = 20;
  else
    height = 20;

  slider->handle_transform = rut_transform_new (ctx);
  slider->handle = rut_nine_slice_new (ctx,
                                       handle_texture,
                                       4, 5, 6, 5,
                                       width, height);
  rut_graphable_add_child (slider->handle_transform, slider->handle);
  rut_graphable_add_child (slider, slider->handle_transform);

  slider->input_region =
    rut_input_region_new_rectangle (0, 0, width, height,
                                    _rut_slider_input_cb,
                                    slider);

  //rut_input_region_set_graphable (slider->input_region, slider->handle);
  rut_graphable_add_child (slider, slider->input_region);

  rut_simple_introspectable_init (slider,
                                  _rut_slider_prop_specs,
                                  slider->properties);

  return slider;
}

void
rut_slider_set_range (RutSlider *slider,
                      float min, float max)
{
  slider->range_min = min;
  slider->range_max = max;
}

void
rut_slider_set_length (RutSlider *slider,
                       float length)
{
  slider->length = length;
}

void
rut_slider_set_progress (RutObject *obj,
                         float progress)
{
  RutSlider *slider = RUT_SLIDER (obj);
  float translation;

  if (slider->progress == progress)
    return;

  slider->progress = progress;
  rut_property_dirty (&slider->ctx->property_ctx,
                      &slider->properties[RUT_SLIDER_PROP_PROGRESS]);

  translation = (slider->length - 20) * slider->progress;

  rut_transform_init_identity (slider->handle_transform);

  if (slider->axis == RUT_AXIS_X)
    rut_transform_translate (slider->handle_transform, translation, 0, 0);
  else
    rut_transform_translate (slider->handle_transform, 0, translation, 0);

  rut_shell_queue_redraw (slider->ctx->shell);

  g_print ("progress = %f\n", slider->progress);
}

void
rut_shell_add_pre_paint_callback (RutShell *shell,
                                  RutObject *graphable,
                                  RutPrePaintCallback callback,
                                  void *user_data)
{
  RutShellPrePaintEntry *entry;
  RutList *insert_point;

  if (graphable)
    {
      /* Don't do anything if the graphable is already queued */
      rut_list_for_each (entry, &shell->pre_paint_callbacks, list_node)
        {
          if (entry->graphable == graphable)
            {
              g_warn_if_fail (entry->callback == callback);
              g_warn_if_fail (entry->user_data == user_data);
              return;
            }
        }
    }

  entry = g_slice_new (RutShellPrePaintEntry);
  entry->graphable = graphable;
  entry->callback = callback;
  entry->user_data = user_data;

  insert_point = &shell->pre_paint_callbacks;

  /* If we are in the middle of flushing the queue then we'll keep the
   * list in order sorted by depth. Otherwise we'll delay sorting it
   * until the flushing starts so that the hierarchy is free to
   * change in the meantime. */

  if (shell->flushing_pre_paints)
    {
      RutShellPrePaintEntry *next_entry;

      update_pre_paint_entry_depth (entry);

      rut_list_for_each (next_entry, &shell->pre_paint_callbacks, list_node)
        {
          if (next_entry->depth >= entry->depth)
            {
              insert_point = &next_entry->list_node;
              break;
            }
        }
    }

  rut_list_insert (insert_point->prev, &entry->list_node);
}

void
rut_shell_remove_pre_paint_callback_by_graphable (RutShell *shell,
                                                  RutObject *graphable)
{
  RutShellPrePaintEntry *entry;

  rut_list_for_each (entry, &shell->pre_paint_callbacks, list_node)
    {
      if (entry->graphable == graphable)
        {
          rut_list_remove (&entry->list_node);
          g_slice_free (RutShellPrePaintEntry, entry);
          break;
        }
    }
}

void
rut_shell_remove_pre_paint_callback (RutShell *shell,
                                     RutPrePaintCallback callback,
                                     void *user_data)
{
  RutShellPrePaintEntry *entry;

  rut_list_for_each (entry, &shell->pre_paint_callbacks, list_node)
    {
      if (entry->callback == callback &&
          entry->user_data == user_data)
        {
          rut_list_remove (&entry->list_node);
          g_slice_free (RutShellPrePaintEntry, entry);
          break;
        }
    }
}

void
rut_shell_quit (RutShell *shell)
{
#ifdef __ANDROID__
  shell->quit = TRUE;
#else
  {
    GApplication *application = g_application_get_default ();

    /* If the application has created a GApplication then we'll quit
     * that instead of the mainloop */
    if (application)
      g_application_quit (application);
    else
      g_main_loop_quit (shell->main_loop);
  }
#endif
}

static void
_rut_shell_paste (RutShell *shell,
                  RutObject *data)
{
  RutInputEvent drop_event;

  drop_event.type = RUT_INPUT_EVENT_TYPE_DROP;
  drop_event.shell = shell;
  drop_event.native = data;
  drop_event.camera = NULL;
  drop_event.input_transform = NULL;

  /* Note: This assumes input handlers are re-entrant and hopefully
   * that's ok. */
  _rut_shell_handle_input (shell, &drop_event);
}

void
rut_shell_drop (RutShell *shell)
{
  RutInputEvent drop_event;
  RutInputEventStatus status;

  if (!shell->drop_offer_taker)
    return;

  drop_event.type = RUT_INPUT_EVENT_TYPE_DROP;
  drop_event.shell = shell;
  drop_event.native = shell->drag_payload;
  drop_event.camera = NULL;
  drop_event.input_transform = NULL;

  status = rut_inputable_handle_event (shell->drop_offer_taker, &drop_event);

  g_warn_if_fail (status == RUT_INPUT_EVENT_STATUS_HANDLED);

  rut_shell_cancel_drag (shell);
}

RutObject *
rut_drop_event_get_data (RutInputEvent *drop_event)
{
  return drop_event->native;
}

struct _RutTextBlob
{
  RutObjectProps _parent;
  int ref_count;

  char *text;
};

static RutObject *
_rut_text_blob_copy (RutObject *object)
{
  RutTextBlob *blob = object;

  return rut_text_blob_new (blob->text);
}

static bool
_rut_text_blob_has (RutObject *object, RutMimableType type)
{
  if (type == RUT_MIMABLE_TYPE_TEXT)
    return TRUE;

  return FALSE;
}

static void *
_rut_text_blob_get (RutObject *object, RutMimableType type)
{
  RutTextBlob *blob = object;

  if (type == RUT_MIMABLE_TYPE_TEXT)
    return blob->text;
  else
    return NULL;
}

static void
_rut_text_blob_free (void *object)
{
  RutTextBlob *blob = object;

  g_free (blob->text);

  g_slice_free (RutTextBlob, object);
}

RutType rut_text_blob_type;

static void
_rut_text_blob_init_type (void)
{
  static RutRefableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rut_text_blob_free
  };
  static RutMimableVTable mimable_vtable = {
      .copy = _rut_text_blob_copy,
      .has = _rut_text_blob_has,
      .get = _rut_text_blob_get,
  };

  RutType *type = &rut_text_blob_type;
#define TYPE RutTextBlob

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_MIMABLE,
                          0, /* no associated properties */
                          &mimable_vtable);

#undef TYPE
}

RutTextBlob *
rut_text_blob_new (const char *text)
{
  static bool initialized = FALSE;
  RutTextBlob *blob = g_slice_new0 (RutTextBlob);

  if (initialized == FALSE)
    {
      _rut_text_blob_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&blob->_parent, &rut_text_blob_type);

  blob->ref_count = 1;

  blob->text = g_strdup (text);

  return blob;
}

static RutInputEventStatus
clipboard_input_grab_cb (RutInputEvent *event,
                         void *user_data)
{
  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY &&
      rut_key_event_get_action (event) == RUT_KEY_EVENT_ACTION_UP)
    {
      if (rut_key_event_get_keysym (event) == RUT_KEY_v &&
          (rut_key_event_get_modifier_state (event) & RUT_MODIFIER_CTRL_ON))
        {
          RutShell *shell = event->shell;
          RutObject *data = shell->clipboard;
          RutMimableVTable *mimable =
            rut_object_get_vtable (data, RUT_INTERFACE_ID_MIMABLE);
          RutObject *copy = mimable->copy (data);

          _rut_shell_paste (shell, copy);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

static void
set_clipboard (RutShell *shell, RutObject *data)
{
  if (shell->clipboard == data)
    return;

  if (shell->clipboard)
    {
      rut_refable_unref (shell->clipboard);

      rut_shell_ungrab_input (shell,
                              clipboard_input_grab_cb,
                              shell);
    }

  if (data)
    {
      shell->clipboard = rut_refable_ref (data);

      rut_shell_grab_input (shell,
                            NULL,
                            clipboard_input_grab_cb,
                            shell);
    }
  else
    shell->clipboard = NULL;
}

/* While there is an active selection then we grab input
 * to catch Ctrl-X/Ctrl-C/Ctrl-V for cut, copy and paste
 */
static RutInputEventStatus
selection_input_grab_cb (RutInputEvent *event,
                         void *user_data)
{
  RutShell *shell = user_data;

  if (rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_KEY)
    {
      if (rut_key_event_get_keysym (event) == RUT_KEY_c &&
          (rut_key_event_get_modifier_state (event) & RUT_MODIFIER_CTRL_ON))
        {
          RutObject *selection = shell->selection;
          RutSelectableVTable *selectable =
            rut_object_get_vtable (selection, RUT_INTERFACE_ID_SELECTABLE);
          RutObject *copy = selectable->copy (selection);

          set_clipboard (shell, copy);

          rut_refable_unref (copy);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_key_event_get_keysym (event) == RUT_KEY_x &&
               (rut_key_event_get_modifier_state (event) & RUT_MODIFIER_CTRL_ON))
        {
          RutObject *selection = shell->selection;
          RutSelectableVTable *selectable =
            rut_object_get_vtable (selection, RUT_INTERFACE_ID_SELECTABLE);
          RutObject *copy = selectable->copy (selection);

          selectable->del (selection);

          set_clipboard (shell, copy);

          rut_refable_unref (copy);

          rut_shell_set_selection (shell, NULL);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_key_event_get_keysym (event) == RUT_KEY_Delete ||
               rut_key_event_get_keysym (event) == RUT_KEY_BackSpace)
        {
          RutObject *selection = shell->selection;
          RutSelectableVTable *selectable =
            rut_object_get_vtable (selection, RUT_INTERFACE_ID_SELECTABLE);

          selectable->del (selection);

          rut_shell_set_selection (shell, NULL);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
    }

  return RUT_INPUT_EVENT_STATUS_UNHANDLED;
}

RutObject *
rut_shell_get_clipboard (RutShell *shell)
{
  return shell->clipboard;
}

void
rut_shell_set_selection (RutShell *shell,
                         RutObject *selection)
{
  if (shell->selection == selection)
    return;

  if (shell->selection)
    {
      rut_selectable_cancel (shell->selection);

      rut_refable_unref (shell->selection);

      rut_shell_ungrab_input (shell,
                              selection_input_grab_cb,
                              shell);
    }

  if (selection)
    {
      shell->selection = rut_refable_ref (selection);

      rut_shell_grab_input (shell,
                            NULL,
                            selection_input_grab_cb,
                            shell);
    }
  else
    shell->selection = NULL;
}

RutObject *
rut_shell_get_selection (RutShell *shell)
{
  return shell->selection;
}

void
rut_shell_start_drag (RutShell *shell, RutObject *payload)
{
  g_return_if_fail (shell->drag_payload == NULL);

  if (payload)
    shell->drag_payload = rut_refable_ref (payload);
}

void
rut_shell_cancel_drag (RutShell *shell)
{
  if (shell->drag_payload)
    {
      cancel_current_drop_offer_taker (shell);
      rut_refable_unref (shell->drag_payload);
      shell->drag_payload = NULL;
    }
}

void
rut_shell_take_drop_offer (RutShell *shell, RutObject *taker)
{
  g_return_if_fail (rut_object_is (taker, RUT_INTERFACE_ID_INPUTABLE));

  /* shell->drop_offer_taker is always canceled at the start of
   * _rut_shell_handle_input() so it should always be NULL at
   * this point. */
  g_return_if_fail (shell->drop_offer_taker == NULL);

  g_return_if_fail (taker);

  shell->drop_offer_taker = rut_refable_ref (taker);
}
