/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2015  Intel Corporation
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

#include <rut-config.h>

#include <emscripten.h>
#include <html5.h>

#include <cglib/cglib.h>
#include <cogl/cogl-webgl.h>

#include "rut-shell.h"
#include "rut-emscripten-shell.h"

static int32_t
rut_emscripten_key_event_get_keysym(rut_input_event_t *event)
{
    return 0;
}

static rut_key_event_action_t
rut_emscripten_key_event_get_action(rut_input_event_t *event)
{
    rut_emscripten_event_t *rut_em_event = event->native;

    switch (rut_em_event->em_type) {
    case EMSCRIPTEN_EVENT_KEYDOWN:
        return RUT_KEY_EVENT_ACTION_DOWN;
    case EMSCRIPTEN_EVENT_KEYUP:
        return RUT_KEY_EVENT_ACTION_UP;
    default:
        c_warn_if_reached();
    }

    return 0;
}

static rut_modifier_state_t
rut_emscripten_key_event_get_modifier_state(rut_input_event_t *event)
{
    rut_emscripten_event_t *rut_em_event = event->native;

    return 0;
}

static rut_motion_event_action_t
rut_emscripten_motion_event_get_action(rut_input_event_t *event)
{
    rut_emscripten_event_t *rut_em_event = event->native;

    switch (rut_em_event->em_type) {
    case EMSCRIPTEN_EVENT_MOUSEMOVE:
        return RUT_MOTION_EVENT_ACTION_MOVE;
    case EMSCRIPTEN_EVENT_MOUSEUP:
        return RUT_MOTION_EVENT_ACTION_UP;
    case EMSCRIPTEN_EVENT_MOUSEDOWN:
        return RUT_MOTION_EVENT_ACTION_DOWN;
    default:
        c_warn_if_reached();
    }

    return 0;
}

static rut_button_state_t
rut_emscripten_motion_event_get_button(rut_input_event_t *event)
{
    rut_emscripten_event_t *rut_em_event = event->native;

    return 0;
}

static rut_button_state_t
rut_emscripten_motion_event_get_button_state(rut_input_event_t *event)
{
    rut_emscripten_event_t *rut_em_event = event->native;

    return 0;
}

static rut_modifier_state_t
rut_emscripten_motion_event_get_modifier_state(rut_input_event_t *event)
{
    rut_emscripten_event_t *rut_em_event = event->native;

    return 0;
}

static void
rut_emscripten_motion_event_get_transformed_xy(rut_input_event_t *event,
                                               float *x,
                                               float *y)
{
    rut_emscripten_event_t *rut_em_event = event->native;

    *x = *y = 0;
}

static const char *
rut_emscripten_text_event_get_text(rut_input_event_t *event)
{
    rut_emscripten_event_t *rut_em_event = event->native;

    return rut_em_event->text;
}

static void
append_text_event(rut_shell_t *shell,
                  rut_shell_onscreen_t *onscreen,
                  const char *text)
{
    rut_input_event_t *event = c_slice_alloc0(sizeof(rut_input_event_t) +
                                              sizeof(rut_emscripten_event_t));
    rut_emscripten_event_t *rut_em_event;

    event->type = RUT_INPUT_EVENT_TYPE_TEXT;
    event->onscreen = onscreen;
    event->native = event->data;

    rut_em_event = (void *)event->data;
    rut_em_event->text = text;

    rut_input_queue_append(shell->input_queue, event);
}

static void
rut_emscripten_free_input_event(rut_input_event_t *event)
{
    c_slice_free1(sizeof(rut_input_event_t) + sizeof(rut_emscripten_event_t),
                  event);
}

static EM_BOOL
em_key_callback(int type, const EmscriptenKeyboardEvent *em_event,
                void *user_data)
{
    rut_shell_onscreen_t *onscreen = user_data;
    rut_shell_t *shell = onscreen->shell;
    rut_input_event_t *event = c_slice_alloc(sizeof(rut_input_event_t) +
                                             sizeof(rut_emscripten_event_t));
    rut_emscripten_event_t *rut_em_event;

    event->type = RUT_INPUT_EVENT_TYPE_KEY;
    event->onscreen = onscreen;
    event->native = event->data;

    rut_em_event = (void *)event->data;
    rut_em_event->em_type = type;
    rut_em_event->key = *em_event;

    rut_input_queue_append(shell->input_queue, event);

    if (type == EMSCRIPTEN_EVENT_KEYDOWN && em_event->key[0] != '\0')
        append_text_event(shell, onscreen, em_event->key);

    /* FIXME: we need a separate status so we can trigger a new
     * frame, but if the input doesn't affect anything then we
     * want to avoid any actual rendering. */
    rut_shell_queue_redraw(shell);

    return true;
}

static EM_BOOL
em_mouse_callback(int type, const EmscriptenMouseEvent *em_event,
                  void *user_data)
{
    rut_shell_onscreen_t *onscreen = user_data;
    rut_shell_t *shell = onscreen->shell;
    rut_input_event_t *event = c_slice_alloc(sizeof(rut_input_event_t) +
                                             sizeof(rut_emscripten_event_t));
    rut_emscripten_event_t *rut_em_event;

    event->type = RUT_INPUT_EVENT_TYPE_MOTION;
    event->onscreen = onscreen;
    event->native = event->data;

    rut_em_event = (void *)event->data;
    rut_em_event->em_type = type;
    rut_em_event->mouse = *em_event;

    rut_input_queue_append(shell->input_queue, event);

    /* FIXME: we need a separate status so we can trigger a new
     * frame, but if the input doesn't affect anything then we
     * want to avoid any actual rendering. */
    rut_shell_queue_redraw(shell);

    return true;
}

static cg_onscreen_t *
rut_emscripten_allocate_onscreen(rut_shell_onscreen_t *onscreen)
{
    rut_shell_t *shell = onscreen->shell;
    cg_onscreen_t *cg_onscreen;
    cg_error_t *ignore = NULL;
    const char *id;

    cg_onscreen = cg_onscreen_new(shell->cg_device,
                                  onscreen->width,
                                  onscreen->height);

    if (!cg_framebuffer_allocate(cg_onscreen, &ignore)) {
        cg_error_free(ignore);
        return NULL;
    }

    id = cg_webgl_onscreen_get_id(cg_onscreen);

    emscripten_set_keyup_callback(id, onscreen, true, /* capture */
                                     em_key_callback);
    emscripten_set_keydown_callback(id, onscreen, true, /* capture */
                                     em_key_callback);
    //emscripten_set_keypress_callback(id, onscreen, true, /* capture */
    //                                 em_key_callback);

    emscripten_set_mousemove_callback(id, onscreen, true, /* capture */
                                      em_mouse_callback);
    emscripten_set_mousedown_callback(id, onscreen, true, /* capture */
                                      em_mouse_callback);
    emscripten_set_mouseup_callback(id, onscreen, true, /* capture */
                                    em_mouse_callback);
    return cg_onscreen;
}

void
rut_emscripten_onscreen_resize(rut_shell_onscreen_t *onscreen,
                               int width,
                               int height)
{
    cg_webgl_onscreen_resize(onscreen->cg_onscreen, width, height);
}

static void
rut_emscripten_onscreen_set_title(rut_shell_onscreen_t *onscreen,
                                  const char *title)
{
}

static void
rut_emscripten_onscreen_set_cursor(rut_shell_onscreen_t *onscreen,
                                   rut_cursor_t cursor)
{
}

void
rut_emscripten_onscreen_set_fullscreen(rut_shell_onscreen_t *onscreen,
                                       bool fullscreen)
{
}

bool
rut_emscripten_shell_init(rut_shell_t *shell)
{
    cg_error_t *error = NULL;

    shell->cg_renderer = cg_renderer_new();

    shell->cg_device = cg_device_new();

    cg_renderer_set_winsys_id(shell->cg_renderer, CG_WINSYS_ID_WEBGL);
    if (cg_renderer_connect(shell->cg_renderer, &error))
        cg_device_set_renderer(shell->cg_device, shell->cg_renderer);
    else {
        c_warning("Failed to setup emscripten renderer: ", error->message);
        cg_error_free(error);
        goto error;
    }

    cg_device_connect(shell->cg_device, &error);
    if (!shell->cg_device) {
        c_warning("Failed to create Cogl Context: %s", error->message);
        cg_error_free(error);
        goto error;
    }

    shell->platform.type = RUT_SHELL_WEB_PLATFORM;

    shell->platform.allocate_onscreen = rut_emscripten_allocate_onscreen;
    shell->platform.onscreen_resize = rut_emscripten_onscreen_resize;
    shell->platform.onscreen_set_title = rut_emscripten_onscreen_set_title;
    shell->platform.onscreen_set_cursor = rut_emscripten_onscreen_set_cursor;
    shell->platform.onscreen_set_fullscreen = rut_emscripten_onscreen_set_fullscreen;

    shell->platform.key_event_get_keysym = rut_emscripten_key_event_get_keysym;
    shell->platform.key_event_get_action = rut_emscripten_key_event_get_action;
    shell->platform.key_event_get_modifier_state = rut_emscripten_key_event_get_modifier_state;

    shell->platform.motion_event_get_action = rut_emscripten_motion_event_get_action;
    shell->platform.motion_event_get_button = rut_emscripten_motion_event_get_button;
    shell->platform.motion_event_get_button_state = rut_emscripten_motion_event_get_button_state;
    shell->platform.motion_event_get_modifier_state = rut_emscripten_motion_event_get_modifier_state;
    shell->platform.motion_event_get_transformed_xy = rut_emscripten_motion_event_get_transformed_xy;

    shell->platform.text_event_get_text = rut_emscripten_text_event_get_text;

    shell->platform.free_input_event = rut_emscripten_free_input_event;

    return true;

error:

    if (shell->cg_device) {
        cg_object_unref(shell->cg_device);
        shell->cg_device = NULL;
    }
    if (shell->cg_renderer) {
        cg_object_unref(shell->cg_renderer);
        shell->cg_renderer = NULL;
    }

    c_free(shell);

    return false;
}
