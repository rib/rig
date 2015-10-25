/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2014  Intel Corporation
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

#include "rut-shell.h"
#include "rut-headless-shell.h"

static int32_t
rut_headless_key_event_get_keysym(rut_input_event_t *event)
{
    rut_stream_event_t *stream_event = event->native;

    switch (stream_event->type) {
        case RUT_STREAM_EVENT_KEY_UP:
        case RUT_STREAM_EVENT_KEY_DOWN:
            return stream_event->key.keysym;
        default:
            c_warn_if_reached();
            return 0;
    }
}

static rut_key_event_action_t
rut_headless_key_event_get_action(rut_input_event_t *event)
{
    rut_stream_event_t *stream_event = event->native;

    switch (stream_event->type) {
        case RUT_STREAM_EVENT_KEY_DOWN:
            return RUT_KEY_EVENT_ACTION_DOWN;
        case RUT_STREAM_EVENT_KEY_UP:
            return RUT_KEY_EVENT_ACTION_UP;
        default:
            c_warn_if_reached();
            return 0;
    }
}

static rut_modifier_state_t
rut_headless_key_event_get_modifier_state(rut_input_event_t *event)
{
    rut_stream_event_t *stream_event = event->native;
    switch (stream_event->type) {
        case RUT_STREAM_EVENT_KEY_DOWN:
        case RUT_STREAM_EVENT_KEY_UP:
            return stream_event->key.mod_state;
        default:
            c_warn_if_reached();
            return 0;
    }
}

static rut_motion_event_action_t
rut_headless_motion_event_get_action(rut_input_event_t *event)
{
    rut_stream_event_t *stream_event = event->native;

    switch (stream_event->type) {
        case RUT_STREAM_EVENT_POINTER_DOWN:
            return RUT_MOTION_EVENT_ACTION_DOWN;
        case RUT_STREAM_EVENT_POINTER_UP:
            return RUT_MOTION_EVENT_ACTION_UP;
        case RUT_STREAM_EVENT_POINTER_MOVE:
            return RUT_MOTION_EVENT_ACTION_MOVE;
        default:
            c_warn_if_reached();
            return 0;
    }
}

static rut_button_state_t
rut_headless_motion_event_get_button(rut_input_event_t *event)
{
    rut_stream_event_t *stream_event = event->native;

    switch (stream_event->type) {
        case RUT_STREAM_EVENT_POINTER_DOWN:
            return stream_event->pointer_button.button;
        case RUT_STREAM_EVENT_POINTER_UP:
            return stream_event->pointer_button.button;
        default:
            c_warn_if_reached();
            return 0;
    }
}

static rut_button_state_t
rut_headless_motion_event_get_button_state(rut_input_event_t *event)
{
    rut_stream_event_t *stream_event = event->native;

    switch (stream_event->type) {
        case RUT_STREAM_EVENT_POINTER_MOVE:
            return stream_event->pointer_move.state;
        case RUT_STREAM_EVENT_POINTER_DOWN:
        case RUT_STREAM_EVENT_POINTER_UP:
            return stream_event->pointer_button.state;
        default:
            c_warn_if_reached();
            return 0;
    }
}

static rut_modifier_state_t
rut_headless_motion_event_get_modifier_state(rut_input_event_t *event)
{
    rut_stream_event_t *stream_event = event->native;

    switch (stream_event->type) {
        case RUT_STREAM_EVENT_POINTER_MOVE:
            return stream_event->pointer_move.mod_state;
        case RUT_STREAM_EVENT_POINTER_DOWN:
        case RUT_STREAM_EVENT_POINTER_UP:
            return stream_event->pointer_button.mod_state;
        default:
            c_warn_if_reached();
            return 0;
    }
}

static void
rut_headless_motion_event_get_transformed_xy(rut_input_event_t *event,
                                        float *x,
                                        float *y)
{
    rut_stream_event_t *stream_event = event->native;
    switch (stream_event->type) {
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
            c_warn_if_reached();
    }
}

static const char *
rut_headless_text_event_get_text(rut_input_event_t *event)
{
    c_return_val_if_reached(NULL); /* TODO */
    return NULL;
}

static void
rut_headless_free_input_event(rut_input_event_t *event)
{
    c_slice_free(rut_stream_event_t, event->native);
    c_slice_free(rut_input_event_t, event);
}

void
rut_headless_shell_handle_stream_event(rut_shell_t *shell,
                                       rut_stream_event_t *stream_event)
{
    rut_input_event_t *event = c_slice_alloc0(sizeof(rut_input_event_t));
    event->native = stream_event;

    event->type = 0;

    event->camera_entity = stream_event->camera_entity;
    event->onscreen = shell->headless_onscreen;

    switch (stream_event->type) {
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
    if (!event->type) {
        c_warning("Shell: Spurious stream event type %d\n", stream_event->type);
        c_slice_free(rut_input_event_t, event);
        return;
    }

    rut_input_queue_append(shell->input_queue, event);

    /* FIXME: we need a separate status so we can trigger a new
     * frame, but if the input doesn't affect anything then we
     * want to avoid any actual rendering. */
    rut_shell_queue_redraw(shell);
}

static void
rut_headless_shell_cleanup(rut_shell_t *shell)
{
    if (shell->headless_onscreen) {
        rut_object_unref(shell->headless_onscreen);
        shell->headless_onscreen = NULL;
    }
}

bool
rut_headless_shell_init(rut_shell_t *shell)
{
    /* We make a dummy onscreen to associate with headless events
     * for consistency and so we can always map an event to a
     * shell via event->onscreen->shell. */

    shell->headless_onscreen = rut_shell_onscreen_new(shell, 100, 100);

    shell->platform.type = RUT_SHELL_HEADLESS;

    shell->platform.key_event_get_keysym = rut_headless_key_event_get_keysym;
    shell->platform.key_event_get_action = rut_headless_key_event_get_action;
    shell->platform.key_event_get_modifier_state = rut_headless_key_event_get_modifier_state;

    shell->platform.motion_event_get_action = rut_headless_motion_event_get_action;
    shell->platform.motion_event_get_button = rut_headless_motion_event_get_button;
    shell->platform.motion_event_get_button_state = rut_headless_motion_event_get_button_state;
    shell->platform.motion_event_get_modifier_state = rut_headless_motion_event_get_modifier_state;
    shell->platform.motion_event_get_transformed_xy = rut_headless_motion_event_get_transformed_xy;

    shell->platform.text_event_get_text = rut_headless_text_event_get_text;

    shell->platform.free_input_event = rut_headless_free_input_event;

    shell->platform.cleanup = rut_headless_shell_cleanup;

    return true;
}
