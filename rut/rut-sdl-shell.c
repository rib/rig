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


#include <rut-config.h>

#include <cglib/cglib.h>
#include <cglib/cg-sdl.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include "rut-shell.h"
#include "rut-sdl-keysyms.h"
#include "rut-sdl-shell.h"
#include "rut-sdl-keysyms.h"

static int32_t
rut_sdl_key_event_get_keysym(rut_input_event_t *event)
{
    rut_sdl_event_t *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;

    return _rut_keysym_from_sdl_keysym(sdl_event->key.keysym.sym);
}

static rut_key_event_action_t
rut_sdl_key_event_get_action(rut_input_event_t *event)
{
    rut_sdl_event_t *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;
    switch (sdl_event->type) {
    case SDL_KEYUP:
        return RUT_KEY_EVENT_ACTION_UP;
    case SDL_KEYDOWN:
        return RUT_KEY_EVENT_ACTION_DOWN;
    default:
        c_warn_if_reached();
        return RUT_KEY_EVENT_ACTION_UP;
    }
}

static rut_modifier_state_t
modifier_state_for_sdl_state(SDL_Keymod mod)
{
    rut_modifier_state_t rut_state = 0;

    if (mod & KMOD_LSHIFT || mod & KMOD_RSHIFT)
        rut_state |= RUT_MODIFIER_SHIFT_ON;
    if (mod & KMOD_LCTRL || mod & KMOD_RCTL)
        rut_state |= RUT_MODIFIER_CTRL_ON;
    if (mod & KMOD_LALT || mod & KMOD_RALT)
        rut_state |= RUT_MODIFIER_ALT_ON;
    if (mod & KMOD_NUM)
        rut_state |= RUT_MODIFIER_NUM_LOCK_ON;
    if (mod & KMOD_CAPS)
        rut_state |= RUT_MODIFIER_CAPS_LOCK_ON;

    return rut_state;
}

static rut_modifier_state_t
rut_sdl_key_event_get_modifier_state(rut_input_event_t *event)
{
    rut_sdl_event_t *rut_sdl_event = event->native;
    return modifier_state_for_sdl_state(rut_sdl_event->mod_state);
}

static rut_motion_event_action_t
rut_sdl_motion_event_get_action(rut_input_event_t *event)
{
    rut_sdl_event_t *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;
    switch (sdl_event->type) {
    case SDL_MOUSEBUTTONDOWN:
        return RUT_MOTION_EVENT_ACTION_DOWN;
    case SDL_MOUSEBUTTONUP:
        return RUT_MOTION_EVENT_ACTION_UP;
    case SDL_MOUSEMOTION:
        return RUT_MOTION_EVENT_ACTION_MOVE;
    default:
        c_warn_if_reached(); /* Not a motion event */
        return RUT_MOTION_EVENT_ACTION_MOVE;
    }
}

static rut_button_state_t
rut_sdl_motion_event_get_button(rut_input_event_t *event)
{
    rut_sdl_event_t *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;

    c_return_val_if_fail((sdl_event->type == SDL_MOUSEBUTTONUP ||
                          sdl_event->type == SDL_MOUSEBUTTONDOWN),
                         RUT_BUTTON_STATE_1);

    switch (sdl_event->button.button) {
    case SDL_BUTTON_LEFT:
        return RUT_BUTTON_STATE_1;
    case SDL_BUTTON_MIDDLE:
        return RUT_BUTTON_STATE_2;
    case SDL_BUTTON_RIGHT:
        return RUT_BUTTON_STATE_3;
    default:
        c_warn_if_reached();
        return RUT_BUTTON_STATE_1;
    }
}

static rut_button_state_t
button_state_for_sdl_state(SDL_Event *event,
                           uint8_t sdl_state)
{
    rut_button_state_t rut_state = 0;
    if (sdl_state & SDL_BUTTON(1))
        rut_state |= RUT_BUTTON_STATE_1;
    if (sdl_state & SDL_BUTTON(2))
        rut_state |= RUT_BUTTON_STATE_2;
    if (sdl_state & SDL_BUTTON(3))
        rut_state |= RUT_BUTTON_STATE_3;

    return rut_state;
}

static rut_button_state_t
rut_sdl_motion_event_get_button_state(rut_input_event_t *event)
{
    rut_sdl_event_t *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;

    return button_state_for_sdl_state(sdl_event,
                                          rut_sdl_event->buttons);
#if 0
    /* FIXME: we need access to the rut_shell_t here so that
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
            c_warning ("Out of range SDL button number");
            return 0;
        }
    case SDL_MOUSEMOTION:
        return button_state_for_sdl_state (sdl_event->button.state);
    default:
        c_warn_if_reached (); /* Not a motion event */
        return 0;
    }
#endif
}

static rut_modifier_state_t
rut_sdl_motion_event_get_modifier_state(rut_input_event_t *event)
{
    rut_sdl_event_t *rut_sdl_event = event->native;
    return modifier_state_for_sdl_state(rut_sdl_event->mod_state);
}

static void
rut_sdl_motion_event_get_transformed_xy(rut_input_event_t *event,
                                        float *x,
                                        float *y)
{
    rut_sdl_event_t *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;
    switch (sdl_event->type) {
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
        c_warn_if_reached(); /* Not a motion event */
        return;
    }
}

static const char *
rut_sdl_text_event_get_text(rut_input_event_t *event)
{
    rut_sdl_event_t *rut_sdl_event = event->native;
    SDL_Event *sdl_event = &rut_sdl_event->sdl_event;

    return sdl_event->text.text;
}

static rut_shell_onscreen_t *
get_onscreen_for_sdl_event(rut_shell_t *shell, SDL_Event *sdl_event)
{
    rut_shell_onscreen_t *onscreen;
    rut_sdl_event_t *rut_sdl_event = event->native;
    Uint32 window_id;

    switch ((SDL_EventType)sdl_event->type) {
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
        c_warn_if_reached();
        return NULL;
    }

    c_list_for_each(onscreen, &shell->onscreens, link) {
        SDL_Window *sdl_window =
            cg_sdl_onscreen_get_window(onscreen->onscreen);

        if (SDL_GetWindowID(sdl_window) == window_id)
            return onscreen;
    }

    c_warn_if_reached();
    return NULL;
}

void
rut_sdl_shell_handle_sdl_event(rut_shell_t *shell, SDL_Event *sdl_event)
{
    rut_input_event_t *event = NULL;
    rut_sdl_event_t *rut_sdl_event;

    switch (sdl_event->type) {
    case SDL_WINDOWEVENT:
        switch (sdl_event->window.event) {
        case SDL_WINDOWEVENT_EXPOSED:
            rut_shell_queue_redraw(shell);
            break;

        case SDL_WINDOWEVENT_CLOSE:
            rut_shell_quit(shell);
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

        event = c_slice_alloc(sizeof(rut_input_event_t) +
                              sizeof(rut_sdl_event_t));

        event->onscreen = get_onscreen_for_sdl_event(shell, sdl_event);
        event->input_transform = NULL;
        event->native = event->data;

        rut_sdl_event = (void *)event->data;
        rut_sdl_event->sdl_event = *sdl_event;
        break;
    default:
        break;
    }

    if (event) {
        switch (sdl_event->type) {

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
            switch (sdl_event->key.keysym.scancode) {
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
            rut_shell_quit(shell);
            break;
        }

        rut_sdl_event->buttons = shell->sdl_buttons;
        rut_sdl_event->mod_state = shell->sdl_keymod;

        rut_input_queue_append(shell->input_queue, event);

        /* FIXME: we need a separate status so we can trigger a new
         * frame, but if the input doesn't affect anything then we
         * want to avoid any actual rendering. */
        rut_shell_queue_redraw(shell);
    }
}

static void
rut_sdl_free_input_event(rut_input_event_t *event)
{
    c_slice_free1(sizeof(rut_input_event_t) + sizeof(rut_sdl_event_t),
                  event);
}

static cg_onscreen_t *
rut_sdl_allocate_onscreen(rut_shell_onscreen_t *onscreen)
{
    cg_onscreen_t *cg_onscreen;
    SDL_Window *sdl_window;
    cg_error_t *ignore = NULL;

    cg_onscreen = cg_onscreen_new(engine->shell->cg_device,
                                  onscreen->width,
                                  onscreen->height);

    if (!cg_framebuffer_allocate(cg_onscreen, &ignore)) {
        cg_error_free(ignore);
        return NULL;
    }

    sdl_window = cg_sdl_onscreen_get_window(shell_onscreen->onscreen);

    SDL_VERSION(&shell_onscreen->sdl.sdl_info.version);
    SDL_GetWindowWMInfo(sdl_window, &shell_onscreen->sdl.sdl_info);

    shell->sdl_subsystem = onscreen->sdl.sdl_info.subsystem;

    return cg_onscreen;
}

static void
rut_sdl_onscreen_resize(rut_shell_onscreen_t *onscreen,
                        int width,
                        int height)
{
    SDL_Window *sdl_window = cg_sdl_onscreen_get_window(onscreen->cg_onscreen);
    SDL_SetWindowSize(sdl_window, width, height);
}

static void
rut_sdl_onscreen_set_title(rut_shell_onscreen_t *onscreen,
                           const char *title)
{
    SDL_Window *window = cg_sdl_onscreen_get_window(onscreen);
    SDL_SetWindowTitle(window, title);
}

static void
rut_sdl_onscreen_set_cursor(rut_shell_onscreen_t *onscreen,
                            rut_cursor_t cursor)
{
    SDL_Cursor *cursor_image = NULL;

    switch (cursor) {
    case RUT_CURSOR_DEFAULT:
        cursor_image = NULL;
        break;
    case RUT_CURSOR_INVISIBLE:
        SDL_ShowCursor(false);
        return;
    case RUT_CURSOR_ARROW:
        cursor_image = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        break;
    case RUT_CURSOR_IBEAM:
        cursor_image = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
        break;
    case RUT_CURSOR_WAIT:
        cursor_image = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
        break;
    case RUT_CURSOR_CROSSHAIR:
        cursor_image = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        break;
    case RUT_CURSOR_SIZE_WE:
        cursor_image = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
        break;
    case RUT_CURSOR_SIZE_NS:
        cursor_image = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
        break;
    }

    if (!cursor_image)
        cursor_image = SDL_GetDefaultCursor();

    SDL_SetCursor(cursor_image);
    SDL_ShowCursor(true);

    if (onscreen->sdl.sdl_cursor_image)
        SDL_FreeCursor(shell_onscreen->sdl.sdl_cursor_image);
    onscreen->sdl.sdl_cursor_image = cursor_image;
}

static int64_t
prepare_sdl_busy_wait(void *user_data)
{
    return SDL_PollEvent(NULL) ? 0 : 8000;
}

static void
dispatch_sdl_busy_wait(void *user_data, int fd, int revents)
{
    rut_shell_t *shell = user_data;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        cg_sdl_handle_event(shell->cg_device, &event);

        rut_sdl_shell_handle_sdl_event(shell, &event);
    }
}

/* XXX: SDL doesn't give us a portable way of blocking for
 * events that is compatible with us polling for other file
 * descriptor events outside of SDL which means we resort to
 * busily polling SDL for events.
 *
 * TODO: On X11 use
 * XConnectionNumber(sdl_info.info.x11.display) so we can also
 * poll for events on X. One caveat would probably be that
 * we'd subvert SDL being able to specify a timeout for
 * polling.
 */
static void
integrate_sdl_events_via_busy_wait(rut_shell_t *shell)
{
    rut_poll_shell_add_fd(shell,
                          -1, /* fd */
                          0, /* events */
                          prepare_sdl_busy_wait,
                          dispatch_sdl_busy_wait,
                          shell);
}

bool
rut_sdl_shell_init(rut_shell_t *shell)
{
    cg_error_t *error = NULL;

    shell->cg_renderer = cg_renderer_new();
    shell->cg_device = cg_device_new();

    cg_renderer_set_winsys_id(shell->cg_renderer, CG_WINSYS_ID_SDL);
    if (cg_renderer_connect(shell->cg_renderer, &error))
        cg_device_set_renderer(shell->cg_device, shell->cg_renderer);
    else {
        cg_error_free(error);
        c_warning("Failed to setup SDL renderer; "
                  "falling back to default\n");
    }

    cg_device_connect(shell->cg_device, &error);
    if (!shell->cg_device) {
        c_warning("Failed to create Cogl Context: %s", error->message);
        cg_error_free(error);
        c_free(shell);
        return false;
    }

    integrate_sdl_events_via_busy_wait(shell);

    shell->sdl_keymod = SDL_GetModState();
    shell->sdl_buttons = SDL_GetMouseState(NULL, NULL);

    shell->platform.type = RUT_SHELL_SDL_PLATFORM;

    shell->platform.allocate_onscreen = rut_sdl_allocate_onscreen;
    shell->platform.onscreen_resize = rut_sdl_onscreen_resize;
    shell->platform.onscreen_set_title = rut_sdl_onscreen_set_title;
    shell->platform.onscreen_set_cursor = rut_sdl_onscreen_set_cursor;

    shell->platform.key_event_get_keysym = rut_sdl_key_event_get_keysym;
    shell->platform.key_event_get_action = rut_sdl_key_event_get_action;
    shell->platform.key_event_get_modifier_state = rut_sdl_key_event_get_modifier_state;

    shell->platform.motion_event_get_action = rut_sdl_motion_event_get_action;
    shell->platform.motion_event_get_button = rut_sdl_motion_event_get_button;
    shell->platform.motion_event_get_button_state = rut_sdl_motion_event_get_button_state;
    shell->platform.motion_event_get_modifier_state = rut_sdl_motion_event_get_modifier_state;
    shell->platform.motion_event_get_transformed_xy = rut_sdl_motion_event_get_transformed_xy;

    shell->platform.text_event_get_text = rut_sdl_text_event_get_text;

    shell->platform.free_input_event = rut_sdl_free_input_event;

    return true;
}
