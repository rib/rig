/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2012,2013  Intel Corporation
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
 */
/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <config.h>

#if 0
#include <signal.h>
#endif

#include <glib.h>
#include <gio/gio.h>

#include <cogl/cogl.h>
#include <cogl/cogl-sdl.h>

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
#include "rut-introspectable.h"
#include "rut-nine-slice.h"
#include "rut-camera.h"
#include "rut-poll.h"
#include "rut-glib-source.h"

#if defined (USE_SDL)
#include "rut-sdl-keysyms.h"
#include "SDL_syswm.h"
#endif

#ifdef USE_GSTREAMER
#include "gstmemsrc.h"
#endif

#ifdef USE_XLIB
#include <X11/Xlib.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

typedef struct
{
  RutList link;

  CoglOnscreen *onscreen;

  RutCursor current_cursor;
  /* This is used to record whether anything set a cursor while
   * handling a mouse motion event. If nothing sets one then the shell
   * will put the cursor back to the default pointer. */
  CoglBool cursor_set;

#if defined(USE_SDL)
  SDL_SysWMinfo sdl_info;
  SDL_Window *sdl_window;
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
  rut_object_unref (shell->rut_ctx);
}

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
  RutObject *camera;
  RutObject *scenegraph;
} InputCamera;

void
rut_shell_add_input_camera (RutShell *shell,
                            RutObject *camera,
                            RutObject *scenegraph)
{
  InputCamera *input_camera = g_slice_new (InputCamera);

  input_camera->camera = rut_object_ref (camera);

  if (scenegraph)
    input_camera->scenegraph = rut_object_ref (scenegraph);
  else
    input_camera->scenegraph = NULL;

  shell->input_cameras = g_list_prepend (shell->input_cameras, input_camera);
}

static void
input_camera_free (InputCamera *input_camera)
{
  rut_object_unref (input_camera->camera);
  if (input_camera->scenegraph)
    rut_object_unref (input_camera->scenegraph);
  g_slice_free (InputCamera, input_camera);
}

void
rut_shell_remove_input_camera (RutShell *shell,
                               RutObject *camera,
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

RutObject *
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

  if (shell->headless)
    return NULL;

#if defined(USE_SDL)

  {
    RutShellOnscreen *shell_onscreen;
    RutSDLEvent *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;
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
  RutShell *shell = event->shell;

  if (shell->headless)
    {
      RutStreamEvent *stream_event = event->native;
      switch (stream_event->type)
        {
        case RUT_STREAM_EVENT_KEY_UP:
        case RUT_STREAM_EVENT_KEY_DOWN:
          return stream_event->key.keysym;
        default:
          g_return_val_if_fail (0, RUT_KEY_Escape);
        }
    }

#if defined (USE_SDL)
  {
    RutSDLEvent *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;

    return _rut_keysym_from_sdl_keysym (sdl_event->key.keysym.sym);
  }
#else
#error "Unknown input system"
#endif
}

RutKeyEventAction
rut_key_event_get_action (RutInputEvent *event)
{
  RutShell *shell = event->shell;

  if (shell->headless)
    {
      RutStreamEvent  *stream_event = event->native;
      switch (stream_event->type)
        {
        case RUT_STREAM_EVENT_KEY_DOWN:
          return RUT_KEY_EVENT_ACTION_DOWN;
        case RUT_STREAM_EVENT_KEY_UP:
          return RUT_KEY_EVENT_ACTION_UP;
        default:
          g_return_val_if_fail (0, RUT_KEY_EVENT_ACTION_DOWN);
        }
    }

#if defined (USE_SDL)
  {
    RutSDLEvent *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;
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
  }
#else
#error "Unknown input system"
#endif
}

RutMotionEventAction
rut_motion_event_get_action (RutInputEvent *event)
{
  RutShell *shell = event->shell;

  if (shell->headless)
    {
      RutStreamEvent *stream_event = event->native;
      switch (stream_event->type)
        {
        case RUT_STREAM_EVENT_POINTER_DOWN:
          return RUT_MOTION_EVENT_ACTION_DOWN;
        case RUT_STREAM_EVENT_POINTER_UP:
          return RUT_MOTION_EVENT_ACTION_UP;
        case RUT_STREAM_EVENT_POINTER_MOVE:
          return RUT_MOTION_EVENT_ACTION_MOVE;
        default:
          g_warn_if_reached ();
          return RUT_KEY_EVENT_ACTION_UP;
        }
    }

#if defined (USE_SDL)
  {
    RutSDLEvent *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;
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
  }
#else
#error "Unknown input system"
#endif
}

RutButtonState
rut_motion_event_get_button (RutInputEvent *event)
{
  RutShell *shell = event->shell;

  if (shell->headless)
    {
      RutStreamEvent *stream_event = event->native;
      switch (stream_event->type)
        {
        case RUT_STREAM_EVENT_POINTER_DOWN:
          return stream_event->pointer_button.button;
        case RUT_STREAM_EVENT_POINTER_UP:
          return stream_event->pointer_button.button;
        default:
          g_warn_if_reached ();
          return RUT_BUTTON_STATE_1;
        }
    }

#if defined(USE_SDL)
  {
    RutSDLEvent *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;

    g_return_val_if_fail ((sdl_event->type == SDL_MOUSEBUTTONUP ||
                           sdl_event->type == SDL_MOUSEBUTTONDOWN),
                          RUT_BUTTON_STATE_1);

    switch (sdl_event->button.button)
      {
      case SDL_BUTTON_LEFT:
        return RUT_BUTTON_STATE_1;
      case SDL_BUTTON_MIDDLE:
        return RUT_BUTTON_STATE_2;
      case SDL_BUTTON_RIGHT:
        return RUT_BUTTON_STATE_3;
      default:
        g_warn_if_reached ();
        return RUT_BUTTON_STATE_1;
      }
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

  return rut_state;
}
#endif /* USE_SDL */

RutButtonState
rut_motion_event_get_button_state (RutInputEvent *event)
{
  RutShell *shell = event->shell;

  if (shell->headless)
    {
      RutStreamEvent *stream_event = event->native;

      switch (stream_event->type)
        {
        case RUT_STREAM_EVENT_POINTER_MOVE:
          return stream_event->pointer_move.state;
        case RUT_STREAM_EVENT_POINTER_DOWN:
        case RUT_STREAM_EVENT_POINTER_UP:
          return stream_event->pointer_button.state;
        default:
          g_warn_if_reached ();
          return 0;
        }
    }

#if defined (USE_SDL)
  {
    RutSDLEvent *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;

    return rut_button_state_for_sdl_state (sdl_event, rut_sdl_event->buttons);
  }
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

#ifdef USE_SDL
static RutModifierState
rut_modifier_state_for_sdl_state (SDL_Keymod mod)
{
  RutModifierState rut_state = 0;

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

  return rut_state;
}
#endif

RutModifierState
rut_key_event_get_modifier_state (RutInputEvent *event)
{
  RutShell *shell = event->shell;

  if (shell->headless)
    {
      RutStreamEvent *stream_event = event->native;
      switch (stream_event->type)
        {
        case RUT_STREAM_EVENT_KEY_DOWN:
        case RUT_STREAM_EVENT_KEY_UP:
          return stream_event->key.mod_state;
        default:
          g_warn_if_reached ();
          return 0;
        }
    }

#if defined (USE_SDL)
  {
    RutSDLEvent *rut_sdl_event = event->native;
    return rut_modifier_state_for_sdl_state (rut_sdl_event->mod_state);
  }
#else
#error "Unknown input system"
  return 0;
#endif
}

RutModifierState
rut_motion_event_get_modifier_state (RutInputEvent *event)
{
#if defined (USE_SDL)
  {
    RutSDLEvent *rut_sdl_event = event->native;
    return rut_modifier_state_for_sdl_state (rut_sdl_event->mod_state);
  }
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
  RutShell *shell = event->shell;

  if (shell->headless)
    {
      RutStreamEvent *stream_event = event->native;
      switch (stream_event->type)
        {
        case RUT_STREAM_EVENT_POINTER_MOVE:
          *x = stream_event->pointer_move.x;
          *y = stream_event->pointer_move.y;
          break;
        case RUT_STREAM_EVENT_POINTER_DOWN:
        case RUT_STREAM_EVENT_POINTER_UP:
          *x = stream_event->pointer_button.x;
          *y = stream_event->pointer_button.y;
          break;
        default:
          g_warn_if_reached ();
        }
    }
  else
    {
#if defined (USE_SDL)
      {
        RutSDLEvent *rut_sdl_event = event->native;
        SDL_Event *sdl_event = &rut_sdl_event->sdl_event;
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
      }
#else
#error "Unknown input system"
#endif
    }

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
  RutObject *camera = rut_input_event_get_camera (event);

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
#if defined (USE_SDL)

  RutSDLEvent *rut_sdl_event = event->native;
  SDL_Event *sdl_event = &rut_sdl_event->sdl_event;

  return sdl_event->text.text;

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
  RutObject *camera;
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
      RutUIViewport *ui_viewport = object;
      RutObject *camera = state->camera;
      const CoglMatrix *view = rut_camera_get_view_transform (camera);
      const CoglMatrix *projection = rut_camera_get_projection (camera);
      const float *viewport = rut_camera_get_viewport (camera);
      RutObject *parent = rut_graphable_get_parent (object);
      CoglMatrix transform;
      float poly[16];

      transform = *view;
      rut_graphable_apply_transform (parent, &transform);

      rect_to_screen_polygon (0, 0,
                              rut_ui_viewport_get_width (ui_viewport),
                              rut_ui_viewport_get_height (ui_viewport),
                              &transform,
                              projection,
                              viewport,
                              poly);

      if (!rut_util_point_in_screen_poly (state->x, state->y,
                                          poly, sizeof (float) * 4, 4))
        return RUT_TRAVERSE_VISIT_SKIP_CHILDREN;
    }

  if (rut_object_is (object, RUT_TRAIT_ID_PICKABLE) &&
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
  RutObject *picked_camera = NULL;
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
      RutObject *camera = input_camera->camera;
      RutObject *scenegraph = input_camera->scenegraph;
      float x = shell->mouse_x;
      float y = shell->mouse_y;
      CameraPickState state;

      event->camera = camera;
      event->input_transform = rut_camera_get_input_transform (camera);

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
      event->input_transform = rut_camera_get_input_transform (picked_camera);
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

  rut_object_unref (shell->drop_offer_taker);
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

RutInputEventStatus
rut_shell_dispatch_input_event (RutShell *shell, RutInputEvent *event)
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
          rut_shell_dispatch_input_event (shell, event);
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
      RutObject *old_camera = event->camera;
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
      RutObject *camera = input_camera->camera;
      GList *input_regions = rut_camera_get_input_regions (camera);
      GList *l2;

      event->camera = camera;
      event->input_transform = rut_camera_get_input_transform (camera);

      for (l2 = input_regions; l2; l2 = l2->next)
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
      if (rut_object_is (target, RUT_TRAIT_ID_INPUTABLE))
        {
          status = rut_inputable_handle_event (target, event);

          if (status == RUT_INPUT_EVENT_STATUS_HANDLED)
            break;
        }

      if (!rut_object_is (target, RUT_TRAIT_ID_GRAPHABLE))
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

static void
_rut_shell_remove_grab_link (RutShell *shell,
                             RutShellGrab *grab)
{
  if (grab->camera)
    rut_object_unref (grab->camera);

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

  rut_closure_list_disconnect_all (&shell->start_paint_callbacks);
  rut_closure_list_disconnect_all (&shell->post_paint_callbacks);

  _rut_shell_fini (shell);

  rut_object_free (RutShell, shell);
}

RutType rut_shell_type;

static void
_rut_shell_init_type (void)
{
  rut_type_init (&rut_shell_type, "RutShell", _rut_shell_free);
}

RutShell *
rut_shell_new (bool headless,
               RutShellInitCallback init,
               RutShellFiniCallback fini,
               RutShellPaintCallback paint,
               void *user_data)
{
  static bool initialized = false;
  RutShell *shell =
    rut_object_alloc0 (RutShell, &rut_shell_type, _rut_shell_init_type);
  RutFrameInfo *frame_info;

  if (G_UNLIKELY (initialized == false))
    {
#ifdef USE_GSTREAMER
      if (!headless)
        {
          gst_element_register (NULL,
                                "memsrc",
                                0,
                                gst_mem_src_get_type());
        }
#endif
      initialized = true;
    }

  shell->headless = headless;

  shell->input_queue = rut_input_queue_new (shell);

  rut_list_init (&shell->input_cb_list);
  rut_list_init (&shell->grabs);
  rut_list_init (&shell->onscreens);

  shell->init_cb = init;
  shell->fini_cb = fini;
  shell->paint_cb = paint;
  shell->user_data = user_data;

  rut_list_init (&shell->pre_paint_callbacks);
  rut_list_init (&shell->start_paint_callbacks);
  rut_list_init (&shell->post_paint_callbacks);
  shell->flushing_pre_paints = FALSE;


  rut_list_init (&shell->frame_infos);

  frame_info = g_slice_new0 (RutFrameInfo);
  rut_list_init (&frame_info->frame_callbacks);
  rut_list_insert (shell->frame_infos.prev, &frame_info->list_node);

  return shell;
}

RutFrameInfo *
rut_shell_get_frame_info (RutShell *shell)
{
  RutFrameInfo *head =
    rut_container_of (shell->frame_infos.prev, head, list_node);
  return head;
}

void
rut_shell_end_redraw (RutShell *shell)
{
  RutFrameInfo *frame_info = g_slice_new0 (RutFrameInfo);

  shell->frame++;

  frame_info->frame = shell->frame;
  rut_list_init (&frame_info->frame_callbacks);
  rut_list_insert (shell->frame_infos.prev, &frame_info->list_node);
}

void
rut_shell_finish_frame (RutShell *shell)
{
  RutFrameInfo *info =
    rut_container_of (shell->frame_infos.next, info, list_node);

  rut_list_remove (&info->list_node);

  rut_closure_list_invoke (&info->frame_callbacks,
                           RutShellFrameCallback,
                           shell,
                           info);

  rut_closure_list_disconnect_all (&info->frame_callbacks);

  g_slice_free (RutFrameInfo, info);
}

bool
rut_shell_get_headless (RutShell *shell)
{
  return shell->headless;
}

/* Note: we don't take a reference on the context so we don't
 * introduce a circular reference. */
void
_rut_shell_associate_context (RutShell *shell,
                              RutContext *context)
{
  shell->rut_ctx = context;
}

#if 0
static int signal_write_fd;

void
signal_handler (int sig)
{
  switch (sig)
    {
    case SIGCHLD:
      write (signal_write_fd, &sig, sizeof (sig));
      break;
    }
}

gboolean
dispatch_signal_source (GSource *source,
                        GSourceFunc callback,
                        void *user_data)
{
  RutShell *shell = user_data;

  do {
    int sig;
    int ret = read (shell->signal_read_fd, &sig, sizeof (sig));
    if (ret != sizeof (sig))
      {
        if (ret == 0)
          return TRUE;
        else if (ret < 0 && errno == EINTR)
          continue;
        else
          {
            g_warning ("Error reading signal fd: %s", strerror (errno));
            return FALSE;
          }
      }

    g_print ("Signal received: %d\n", sig);

    rut_closure_list_invoke (&shell->signal_cb_list,
                             RutShellSignalCallback,
                             shell,
                             sig);
  } while (1);

  return TRUE;
}
#endif

#ifdef __ANDROID__
static android_LogPriority
glib_log_level_to_android_priority (GLogLevelFlags flags)
{
  guint i;
  static long levels[8] =
    {
      0,                      /* (unused) ANDROID_LOG_UNKNOWN */
      0,                      /* (unused) ANDROID_LOG_DEFAULT */
      G_LOG_LEVEL_MESSAGE,    /* ANDROID_LOG_VERBOSE */
      G_LOG_LEVEL_DEBUG,      /* ANDROID_LOG_DEBUG */
      G_LOG_LEVEL_INFO,       /* ANDROID_LOG_INFO */
      G_LOG_LEVEL_WARNING,    /* ANDROID_LOG_WARN */
      G_LOG_LEVEL_CRITICAL,   /* ANDROID_LOG_ERROR */
      G_LOG_LEVEL_ERROR       /* ANDROID_LOG_FATAL */
    };

  for (i = 3; i < G_N_ELEMENTS (levels); i++)
    if (flags & levels [i])
      break;

  if (i >= G_N_ELEMENTS (levels))
    return ANDROID_LOG_INFO;

  return i;
}

static void
android_glib_log_handler (const gchar *log_domain,
                          GLogLevelFlags log_level,
                          const gchar *message,
                          gpointer user_data)
{
  gboolean is_fatal = (log_level & G_LOG_FLAG_FATAL);
  android_LogPriority android_level;

  if (is_fatal)
    android_level = ANDROID_LOG_FATAL;
  else
    android_level = glib_log_level_to_android_priority (log_level);

  __android_log_write (android_level, log_domain, message);
}
#endif /* __ANDROID__ */

void
_rut_shell_init (RutShell *shell)
{
#ifdef USE_SDL
  shell->sdl_keymod = SDL_GetModState ();
  shell->sdl_buttons = SDL_GetMouseState (NULL, NULL);
#endif

#ifdef __ANDROID__
  g_log_set_default_handler (android_glib_log_handler, NULL);
#endif

  /* XXX: for some reason handling SGICHLD like this interferes
   * with GApplication... */
#if 0
  {
    int fds[2];
    if (pipe (fds) == 0)
      {
        struct sigaction act = {
            .sa_handler = signal_handler,
            .sa_flags = SA_NOCLDSTOP,
        };
        GSourceFuncs funcs = {
            .prepare = NULL,
            .check = NULL,
            .dispatch = dispatch_signal_source,
            .finalize = NULL
        };
        GSource *source;

        shell->signal_read_fd = fds[0];
        signal_write_fd = fds[1];

        sigemptyset (&act.sa_mask);
        sigaction (SIGCHLD, &act, NULL);

        source = g_source_new (&funcs, sizeof (GSource));
        g_source_add_unix_fd (source, fds[0], G_IO_IN);
        /* Actually we don't care about the callback, we just want to
         * pass some data to the dispatch callback... */
        g_source_set_callback (source, NULL, shell, NULL);
        g_source_attach (source, NULL);

        rut_list_init (&shell->signal_cb_list);
      }
  }
#endif

  rut_poll_init (shell);
}

void
rut_shell_set_window_camera (RutShell *shell, RutObject *window_camera)
{
  shell->window_camera = window_camera;
}

void
rut_shell_grab_key_focus (RutShell *shell,
                          RutObject *inputable,
                          GDestroyNotify ungrab_callback)
{
  g_return_if_fail (rut_object_is (inputable, RUT_TRAIT_ID_INPUTABLE));

  /* If something tries to set the keyboard focus to the same object
   * then we probably do still want to call the keyboard ungrab
   * callback for the last object that set it. The code may be
   * assuming that when this function is called it definetely has the
   * keyboard focus and that the callback will definetely called at
   * some point. Otherwise this function is more like a request and it
   * should have a way of reporting whether the request succeeded */

  rut_object_ref (inputable);

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

      rut_object_unref (shell->keyboard_focus_object);

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
#if defined(USE_SDL)
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
  SDL_Window *window = cogl_sdl_onscreen_get_window (onscreen);
  SDL_SetWindowTitle (window, title);
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


void
rut_shell_start_redraw (RutShell *shell)
{
  g_return_if_fail (shell->redraw_queued == true);

  shell->redraw_queued = false;
  g_source_remove (shell->glib_paint_idle);
  shell->glib_paint_idle = 0;
}

void
rut_shell_update_timelines (RutShell *shell)
{
  GSList *l;

  for (l = shell->rut_ctx->timelines; l; l = l->next)
    _rut_timeline_update (l->data);
}

static void
free_input_event (RutShell *shell, RutInputEvent *event)
{
  if (shell->headless)
    {
      g_slice_free (RutStreamEvent, event->native);
      g_slice_free (RutInputEvent, event);
    }
  else
    {
#ifdef USE_SDL
      g_slice_free1 (sizeof (RutInputEvent) + sizeof (RutSDLEvent), event);
#else
      g_slice_free (RutInputEvent, event);
#endif
    }
}

void
rut_shell_dispatch_input_events (RutShell *shell)
{
  RutInputQueue *input_queue = shell->input_queue;
  RutInputEvent *event, *tmp;

  rut_list_for_each_safe (event, tmp, &input_queue->events, list_node)
    {
      /* XXX: we remove the event from the queue before dispatching it
       * so that it can potentially be deferred to another input queue
       * during the dispatch. For example the Rig editor will re-queue
       * events received for a RutObjectView to be forwarded to the
       * simulator process. */
      rut_input_queue_remove (shell->input_queue, event);

      rut_shell_dispatch_input_event (shell, event);

      /* Only free the event if it hasn't been re-queued
       * TODO: perhaps make RutInputEvent into a ref-counted object
       */
      if (event->list_node.prev == NULL && event->list_node.next == NULL)
        {
          free_input_event (shell, event);
        }
    }

  g_warn_if_fail (input_queue->n_events == 0);
}

RutInputQueue *
rut_shell_get_input_queue (RutShell *shell)
{
  return shell->input_queue;
}

RutInputQueue *
rut_input_queue_new (RutShell *shell)
{
  RutInputQueue *queue = g_slice_new0 (RutInputQueue);

  queue->shell = shell;
  rut_list_init (&queue->events);
  queue->n_events = 0;

  return queue;
}

void
rut_input_queue_append (RutInputQueue *queue,
                        RutInputEvent *event)
{
  if (queue->shell->headless)
    {
#if 0
      g_print ("input_queue_append %p %d\n", event, event->type);
      if (event->type == RUT_INPUT_EVENT_TYPE_KEY)
        g_print ("> is key\n");
      else
        g_print ("> not key\n");
#endif
    }
  rut_list_insert (queue->events.prev, &event->list_node);
  queue->n_events++;
}

void
rut_input_queue_remove (RutInputQueue *queue,
                        RutInputEvent *event)
{
  rut_list_remove (&event->list_node);
  queue->n_events--;
}

void
rut_input_queue_destroy (RutInputQueue *queue)
{
  rut_input_queue_clear (queue);

  g_slice_free (RutInputQueue, queue);
}

void
rut_input_queue_clear (RutInputQueue *input_queue)
{
  RutShell *shell = input_queue->shell;
  RutInputEvent *event, *tmp;

  rut_list_for_each_safe (event, tmp, &input_queue->events, list_node)
    {
      rut_list_remove (&event->list_node);
      free_input_event (shell, event);
    }

  input_queue->n_events = 0;
}

void
rut_shell_handle_stream_event (RutShell *shell,
                               RutStreamEvent *stream_event)
{
  /* XXX: it's assumed that any process that's handling stream events
   * is not handling any other native events. I.e stream events
   * are effectively the native events.
   */

  RutInputEvent *event = g_slice_alloc (sizeof (RutInputEvent));
  event->native = stream_event;

  event->type = 0;
  event->shell = shell;
  event->input_transform = NULL;

  switch (stream_event->type)
    {
    case RUT_STREAM_EVENT_POINTER_MOVE:
    case RUT_STREAM_EVENT_POINTER_DOWN:
    case RUT_STREAM_EVENT_POINTER_UP:
      event->type = RUT_INPUT_EVENT_TYPE_MOTION;
      break;
    case RUT_STREAM_EVENT_KEY_DOWN:
    case RUT_STREAM_EVENT_KEY_UP:
      event->type = RUT_INPUT_EVENT_TYPE_KEY;
      break;
    }

  /* Note: we don't use a default: case since we want the
   * compiler to warn us when we forget to handle a new
   * enum */
  if (!event->type)
    {
      g_warning ("Shell: Spurious stream event type %d\n",
                 stream_event->type);
      g_slice_free (RutInputEvent, event);
      return;
    }

  rut_input_queue_append (shell->input_queue, event);

  /* FIXME: we need a separate status so we can trigger a new
   * frame, but if the input doesn't affect anything then we
   * want to avoid any actual rendering. */
  rut_shell_queue_redraw (shell);
}

void
rut_shell_run_pre_paint_callbacks (RutShell *shell)
{
  flush_pre_paint_callbacks (shell);
}

void
rut_shell_run_start_paint_callbacks (RutShell *shell)
{
  rut_closure_list_invoke (&shell->start_paint_callbacks,
                           RutShellPaintCallback,
                           shell);
}

void
rut_shell_run_post_paint_callbacks (RutShell *shell)
{
  rut_closure_list_invoke (&shell->post_paint_callbacks,
                           RutShellPaintCallback,
                           shell);
}

bool
rut_shell_check_timelines (RutShell *shell)
{
  GSList *l;

  for (l = shell->rut_ctx->timelines; l; l = l->next)
    if (rut_timeline_is_running (l->data))
      return true;

  return false;
}

static void
_rut_shell_paint (RutShell *shell)
{
  shell->paint_cb (shell, shell->user_data);
}

#ifdef USE_SDL

void
rut_shell_handle_sdl_event (RutShell *shell, SDL_Event *sdl_event)
{
  RutInputEvent *event = NULL;
  RutSDLEvent *rut_sdl_event;

  switch (sdl_event->type)
    {
    case SDL_WINDOWEVENT:
      switch (sdl_event->window.event)
        {
        case SDL_WINDOWEVENT_EXPOSED:
          rut_shell_queue_redraw (shell);
          break;

        case SDL_WINDOWEVENT_CLOSE:
          rut_shell_quit (shell);
          break;
        }
      return;

    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_KEYUP:
    case SDL_KEYDOWN:
    case SDL_TEXTINPUT:

      /* We queue input events to be handled on a per-frame
       * basis instead of dispatching them immediately...
       */

      event = g_slice_alloc (sizeof (RutInputEvent) + sizeof (RutSDLEvent));
      rut_sdl_event = (void *)event->data;
      rut_sdl_event->sdl_event = *sdl_event;

      memcpy (event->data, sdl_event, sizeof (SDL_Event));
      event->native = event->data;

      event->shell = shell;
      event->input_transform = NULL;
      break;
    default:
      break;
    }

  if (event)
    {
      switch (sdl_event->type)
        {

        case SDL_MOUSEMOTION:
          event->type = RUT_INPUT_EVENT_TYPE_MOTION;
          shell->sdl_buttons = sdl_event->motion.state;
          break;
        case SDL_MOUSEBUTTONDOWN:
          event->type = RUT_INPUT_EVENT_TYPE_MOTION;
          shell->sdl_buttons |= sdl_event->button.button;
          break;
        case SDL_MOUSEBUTTONUP:
          event->type = RUT_INPUT_EVENT_TYPE_MOTION;
          shell->sdl_buttons &= ~sdl_event->button.button;
          break;

        case SDL_KEYDOWN:

          event->type = RUT_INPUT_EVENT_TYPE_KEY;
          shell->sdl_keymod = sdl_event->key.keysym.mod;

          /* Copied from from SDL_keyboard.c: SDL_SendKeyboardKey()
           * since we want to track the modifier state in relation to
           * events instead of globally and we can't simply use
           * sdl_event->key.keysym.mod because if the button being
           * pressed is itself a modifier then SDL doesn't reflect
           * that in the modifier state for the event.
           */
          switch (sdl_event->key.keysym.scancode)
            {
            case SDL_SCANCODE_NUMLOCKCLEAR:
              shell->sdl_keymod ^= KMOD_NUM;
              break;
            case SDL_SCANCODE_CAPSLOCK:
              shell->sdl_keymod ^= KMOD_CAPS;
              break;
            case SDL_SCANCODE_LCTRL:
              shell->sdl_keymod |= KMOD_LCTRL;
              break;
            case SDL_SCANCODE_RCTRL:
              shell->sdl_keymod |= KMOD_RCTRL;
              break;
            case SDL_SCANCODE_LSHIFT:
              shell->sdl_keymod |= KMOD_LSHIFT;
              break;
            case SDL_SCANCODE_RSHIFT:
              shell->sdl_keymod |= KMOD_RSHIFT;
              break;
            case SDL_SCANCODE_LALT:
              shell->sdl_keymod |= KMOD_LALT;
              break;
            case SDL_SCANCODE_RALT:
              shell->sdl_keymod |= KMOD_RALT;
              break;
            case SDL_SCANCODE_LGUI:
              shell->sdl_keymod |= KMOD_LGUI;
              break;
            case SDL_SCANCODE_RGUI:
              shell->sdl_keymod |= KMOD_RGUI;
              break;
            case SDL_SCANCODE_MODE:
              shell->sdl_keymod |= KMOD_MODE;
              break;
            default:
              break;
            }
          break;

        case SDL_KEYUP:

          event->type = RUT_INPUT_EVENT_TYPE_KEY;
          shell->sdl_keymod = sdl_event->key.keysym.mod;
          break;

        case SDL_TEXTINPUT:
          event->type = RUT_INPUT_EVENT_TYPE_TEXT;
          break;

        case SDL_QUIT:
          rut_shell_quit (shell);
          break;
        }

      rut_sdl_event->buttons = shell->sdl_buttons;
      rut_sdl_event->mod_state = shell->sdl_keymod;

      rut_input_queue_append (shell->input_queue, event);

      /* FIXME: we need a separate status so we can trigger a new
       * frame, but if the input doesn't affect anything then we
       * want to avoid any actual rendering. */
      rut_shell_queue_redraw (shell);
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

  /* XXX: SDL doesn't give us a portable way of blocking for events
   * that is compatible with us polling for other file descriptor
   * events outside of SDL which means we normally resort to busily
   * polling SDL for events.
   *
   * Luckily on Android though we know that events are delivered to
   * SDL in a separate thread which we can monitor and using a pipe we
   * are able to wake up our own polling mainloop. This means we can
   * allow the Glib mainloop to block on Android...
   *
   * TODO: On X11 use XConnectionNumber(sdl_info.info.x11.display)
   * so we can also poll for events on X. One caveat would probably
   * be that we'd subvert SDL being able to specify a timeout for
   * polling.
   */
#ifdef __ANDROID__
  *timeout = -1;
#else
  *timeout = 8;
#endif

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

      rut_shell_handle_sdl_event (sdl_source->shell, &event);
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

static gboolean
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

#ifdef USE_SDL
  {
    SDL_Window *sdl_window =
      cogl_sdl_onscreen_get_window (shell_onscreen->onscreen);

    SDL_VERSION (&shell_onscreen->sdl_info.version);
    SDL_GetWindowWMInfo(sdl_window, &shell_onscreen->sdl_info);

    shell->sdl_subsystem = shell_onscreen->sdl_info.subsystem;
  }
#endif
}

void
rut_shell_main (RutShell *shell)
{
  GSource *rut_source;

  if (shell->init_cb)
    shell->init_cb (shell, shell->user_data);

  rut_source = rut_glib_shell_source_new (shell, G_PRIORITY_DEFAULT);
  g_source_attach (rut_source, NULL);

#ifdef USE_SDL
  if (!shell->headless)
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
    GApplication *application = NULL;

    if (!shell->headless)
      application = g_application_get_default ();

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

  if (shell->fini_cb)
    shell->fini_cb (shell, shell->user_data);

#ifdef USE_SDL
  g_main_context_set_poll_func (g_main_context_default (),
                                rut_sdl_original_poll);
#endif /* USE_SDL */
}

void
rut_shell_grab_input (RutShell *shell,
                      RutObject *camera,
                      RutInputCallback callback,
                      void *user_data)
{
  RutShellGrab *grab = g_slice_new (RutShellGrab);

  grab->callback = callback;
  grab->user_data = user_data;

  if (camera)
    grab->camera = rut_object_ref (camera);
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

typedef struct _PointerGrab
{
  RutShell *shell;
  RutInputEventStatus (*callback) (RutInputEvent *event,
                                   void *user_data);
  void *user_data;
  RutButtonState button_mask;
  bool x11_grabbed;
} PointerGrab;

static RutInputEventStatus
pointer_grab_cb (RutInputEvent *event, void *user_data)
{
  PointerGrab *grab = user_data;
  RutInputEventStatus status = grab->callback (event, grab->user_data);

  if(rut_input_event_get_type (event) == RUT_INPUT_EVENT_TYPE_MOTION)
    {
      RutButtonState current = rut_motion_event_get_button_state (event);
      RutButtonState last_button = 1<<(ffs (current) - 1);

      if (rut_motion_event_get_action (event) == RUT_MOTION_EVENT_ACTION_UP &&
          ((rut_motion_event_get_button_state (event) & last_button) == 0))
        {
          RutShell *shell = grab->shell;
          g_slice_free (PointerGrab, grab);
          rut_shell_ungrab_input (shell,
                                  pointer_grab_cb,
                                  user_data);

#ifdef USE_XLIB
          /* X11 doesn't implicitly grab the mouse on pointer-down events
           * so we have to do it explicitly... */
          if (grab->x11_grabbed)
            {
              RutShellOnscreen *shell_onscreen =
                rut_container_of (shell->onscreens.next, shell_onscreen, link);
              Display *dpy = shell_onscreen->sdl_info.info.x11.display;

              if (shell->x11_grabbed)
                {
                  XUngrabPointer (dpy, CurrentTime);
                  XUngrabKeyboard (dpy, CurrentTime);
                  shell->x11_grabbed = false;
                }
            }
#endif
        }
    }

  return status;
}

void
rut_shell_grab_pointer (RutShell *shell,
                        RutObject *camera,
                        RutInputEventStatus (*callback) (RutInputEvent *event,
                                                         void *user_data),
                        void *user_data)
{

  PointerGrab *grab = g_slice_new0 (PointerGrab);

  grab->shell = shell;
  grab->callback = callback;
  grab->user_data = user_data;

  rut_shell_grab_input (shell,
                        camera,
                        pointer_grab_cb,
                        grab);

#ifdef USE_XLIB
  /* X11 doesn't implicitly grab the mouse on pointer-down events
   * so we have to do it explicitly... */
  if (shell->sdl_subsystem == SDL_SYSWM_X11)
    {
      RutShellOnscreen *shell_onscreen =
        rut_container_of (shell->onscreens.next, shell_onscreen, link);
      Display *dpy = shell_onscreen->sdl_info.info.x11.display;
      Window win = shell_onscreen->sdl_info.info.x11.window;

      g_warn_if_fail (shell->x11_grabbed == false);

      if (XGrabPointer (dpy, win, False,
                        PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
                        GrabModeAsync, /* pointer mode */
                        GrabModeAsync, /* keyboard mode */
                        None, /* confine to */
                        None, /* cursor */
                        CurrentTime) == GrabSuccess)
        {
          grab->x11_grabbed = true;
          shell->x11_grabbed = true;
        }

      XGrabKeyboard(dpy, win, False,
                    GrabModeAsync,
                    GrabModeAsync,
                    CurrentTime);
    }
#endif
}

void
rut_shell_queue_redraw_real (RutShell *shell)
{
  shell->redraw_queued = true;

  if (shell->glib_paint_idle <= 0)
    shell->glib_paint_idle = g_idle_add (glib_paint_cb, shell);
}

void
rut_shell_queue_redraw (RutShell *shell)
{
  if (shell->queue_redraw_callback)
    shell->queue_redraw_callback (shell, shell->queue_redraw_data);
  else
    rut_shell_queue_redraw_real (shell);
}

void
rut_shell_set_queue_redraw_callback (RutShell *shell,
                                     void (*callback) (RutShell *shell,
                                                       void *user_data),
                                     void *user_data)
{
  shell->queue_redraw_callback = callback;
  shell->queue_redraw_data = user_data;
}

enum {
  RUT_SLIDER_PROP_PROGRESS,
  RUT_SLIDER_N_PROPS
};

struct _RutSlider
{
  RutObjectBase _base;

  /* FIXME: It doesn't seem right that we should have to save a
   * pointer to the context for input here... */
  RutContext *ctx;

  /* FIXME: It also doesn't seem right to have to save a pointer
   * to the camera here so we can queue a redraw */
  //RutObject *camera;

  RutGraphableProps graphable;
  RutPaintableProps paintable;
  RutIntrospectableProps introspectable;

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

  rut_object_unref (slider->input_region);

  rut_graphable_remove_child (slider->handle_transform);

  rut_object_unref (slider->handle_transform);
  rut_object_unref (slider->handle);
  rut_object_unref (slider->background);

  rut_introspectable_destroy (slider);

  rut_graphable_destroy (slider);

  rut_object_free (RutSlider, object);
}

static void
_rut_slider_paint (RutObject *object, RutPaintContext *paint_ctx)
{
  RutSlider *slider = RUT_SLIDER (object);
  RutPaintableVTable *bg_paintable =
    rut_object_get_vtable (slider->background, RUT_TRAIT_ID_PAINTABLE);

  bg_paintable->paint (slider->background, paint_ctx);
}

#if 0
static void
_rut_slider_set_camera (RutObject *object,
                        RutObject *camera)
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
  static RutGraphableVTable graphable_vtable = {
    NULL, /* child remove */
    NULL, /* child add */
    NULL /* parent changed */
  };

  static RutPaintableVTable paintable_vtable = {
    _rut_slider_paint
  };



  RutType *type = &rut_slider_type;
#define TYPE RutSlider

  rut_type_init (&rut_slider_type, G_STRINGIFY (TYPE), _rut_slider_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_GRAPHABLE,
                      offsetof (TYPE, graphable),
                      &graphable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_PAINTABLE,
                      offsetof (TYPE, paintable),
                      &paintable_vtable);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_INTROSPECTABLE,
                      offsetof (TYPE, introspectable),
                      NULL); /* no implied vtable */

#undef TYPE
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

  //g_print ("Slider input\n");

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
  RutSlider *slider =
    rut_object_alloc0 (RutSlider, &rut_slider_type, _rut_slider_init_type);
  CoglTexture *bg_texture;
  CoglTexture *handle_texture;
  GError *error = NULL;
  //PangoRectangle label_size;
  float width;
  float height;



  rut_graphable_init (slider);
  rut_paintable_init (slider);

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

  rut_introspectable_init (slider,
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

  //g_print ("progress = %f\n", slider->progress);
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

RutClosure *
rut_shell_add_start_paint_callback (RutShell *shell,
                                    RutShellPaintCallback callback,
                                    void *user_data,
                                    RutClosureDestroyCallback destroy)
{
  return rut_closure_list_add (&shell->start_paint_callbacks,
                               callback,
                               user_data,
                               destroy);
}


RutClosure *
rut_shell_add_post_paint_callback (RutShell *shell,
                                   RutShellPaintCallback callback,
                                   void *user_data,
                                   RutClosureDestroyCallback destroy)
{
  return rut_closure_list_add (&shell->post_paint_callbacks,
                               callback,
                               user_data,
                               destroy);
}

RutClosure *
rut_shell_add_frame_callback (RutShell *shell,
                              RutShellFrameCallback callback,
                              void *user_data,
                              RutClosureDestroyCallback destroy)
{
  RutFrameInfo *info = rut_shell_get_frame_info (shell);
  return rut_closure_list_add (&info->frame_callbacks,
                               callback,
                               user_data,
                               destroy);
}

void
rut_shell_quit (RutShell *shell)
{
  GApplication *application = g_application_get_default ();

  /* If the application has created a GApplication then we'll quit
   * that instead of the mainloop */
  if (application)
    g_application_quit (application);
  else
    g_main_loop_quit (shell->main_loop);
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
  rut_shell_dispatch_input_event (shell, &drop_event);
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
  RutObjectBase _base;

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

  rut_object_free (RutTextBlob, object);
}

RutType rut_text_blob_type;

static void
_rut_text_blob_init_type (void)
{
  static RutMimableVTable mimable_vtable = {
      .copy = _rut_text_blob_copy,
      .has = _rut_text_blob_has,
      .get = _rut_text_blob_get,
  };

  RutType *type = &rut_text_blob_type;
#define TYPE RutTextBlob

  rut_type_init (type, G_STRINGIFY (TYPE), _rut_text_blob_free);
  rut_type_add_trait (type,
                      RUT_TRAIT_ID_MIMABLE,
                      0, /* no associated properties */
                      &mimable_vtable);

#undef TYPE
}

RutTextBlob *
rut_text_blob_new (const char *text)
{
  RutTextBlob *blob =
    rut_object_alloc0 (RutTextBlob, &rut_text_blob_type, _rut_text_blob_init_type);



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
            rut_object_get_vtable (data, RUT_TRAIT_ID_MIMABLE);
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
      rut_object_unref (shell->clipboard);

      rut_shell_ungrab_input (shell,
                              clipboard_input_grab_cb,
                              shell);
    }

  if (data)
    {
      shell->clipboard = rut_object_ref (data);

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
            rut_object_get_vtable (selection, RUT_TRAIT_ID_SELECTABLE);
          RutObject *copy = selectable->copy (selection);

          set_clipboard (shell, copy);

          rut_object_unref (copy);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_key_event_get_keysym (event) == RUT_KEY_x &&
               (rut_key_event_get_modifier_state (event) & RUT_MODIFIER_CTRL_ON))
        {
          RutObject *selection = shell->selection;
          RutSelectableVTable *selectable =
            rut_object_get_vtable (selection, RUT_TRAIT_ID_SELECTABLE);
          RutObject *copy = selectable->copy (selection);

          selectable->del (selection);

          set_clipboard (shell, copy);

          rut_object_unref (copy);

          rut_shell_set_selection (shell, NULL);

          return RUT_INPUT_EVENT_STATUS_HANDLED;
        }
      else if (rut_key_event_get_keysym (event) == RUT_KEY_Delete ||
               rut_key_event_get_keysym (event) == RUT_KEY_BackSpace)
        {
          RutObject *selection = shell->selection;
          RutSelectableVTable *selectable =
            rut_object_get_vtable (selection, RUT_TRAIT_ID_SELECTABLE);

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

      rut_object_unref (shell->selection);

      rut_shell_ungrab_input (shell,
                              selection_input_grab_cb,
                              shell);
    }

  if (selection)
    {
      shell->selection = rut_object_ref (selection);

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
    shell->drag_payload = rut_object_ref (payload);
}

void
rut_shell_cancel_drag (RutShell *shell)
{
  if (shell->drag_payload)
    {
      cancel_current_drop_offer_taker (shell);
      rut_object_unref (shell->drag_payload);
      shell->drag_payload = NULL;
    }
}

void
rut_shell_take_drop_offer (RutShell *shell, RutObject *taker)
{
  g_return_if_fail (rut_object_is (taker, RUT_TRAIT_ID_INPUTABLE));

  /* shell->drop_offer_taker is always canceled at the start of
   * _rut_shell_handle_input() so it should always be NULL at
   * this point. */
  g_return_if_fail (shell->drop_offer_taker == NULL);

  g_return_if_fail (taker);

  shell->drop_offer_taker = rut_object_ref (taker);
}

#if 0
RutClosure *
rut_shell_add_signal_callback (RutShell *shell,
                               RutShellSignalCallback callback,
                               void *user_data,
                               RutClosureDestroyCallback destroy_cb)
{
  return rut_closure_list_add (&shell->signal_cb_list,
                               callback,
                               user_data,
                               destroy_cb);
}
#endif
