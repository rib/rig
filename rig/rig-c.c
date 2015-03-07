/*
 * Rig C
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2015  Intel Corporation.
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

#include <config.h>

#include "rig-c.h"

#include <rut.h>

RInputEventType
r_input_event_get_type(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    switch (rut_input_event_get_type(rut_event)) {
    case RUT_INPUT_EVENT_TYPE_MOTION:
        return R_INPUT_EVENT_TYPE_MOTION;
    case RUT_INPUT_EVENT_TYPE_KEY:
        return R_INPUT_EVENT_TYPE_KEY;
    case RUT_INPUT_EVENT_TYPE_TEXT:
        return R_INPUT_EVENT_TYPE_TEXT;
    default:
        c_return_val_if_reached(0);
    }
}

RKeyEventAction
r_key_event_get_action(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    switch (rut_key_event_get_action(rut_event)) {
    case RUT_KEY_EVENT_ACTION_UP:
        return RUT_KEY_EVENT_ACTION_UP;
    case RUT_KEY_EVENT_ACTION_DOWN:
        return RUT_KEY_EVENT_ACTION_DOWN;
    default:
        c_return_val_if_reached(0);
    }
}

int32_t
r_key_event_get_keysym(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return rut_key_event_get_keysym(rut_event);
}

RModifierState
r_key_event_get_modifier_state(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return (RModifierState)rut_key_event_get_modifier_state(rut_event);
}

RMotionEventAction
r_motion_event_get_action(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    switch (rut_motion_event_get_action(rut_event)) {
    case R_MOTION_EVENT_ACTION_UP:
        return RUT_MOTION_EVENT_ACTION_UP;
    case R_MOTION_EVENT_ACTION_DOWN:
        return RUT_MOTION_EVENT_ACTION_DOWN;
    case R_MOTION_EVENT_ACTION_MOVE:
        return RUT_MOTION_EVENT_ACTION_MOVE;
    default:
        c_return_val_if_reached(0);
    }
}

RButtonState
r_motion_event_get_button(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    switch (rut_motion_event_get_button(rut_event)) {
    case RUT_BUTTON_STATE_1:
        return R_BUTTON_STATE_1;
    case RUT_BUTTON_STATE_2:
        return R_BUTTON_STATE_2;
    case RUT_BUTTON_STATE_3:
        return R_BUTTON_STATE_3;
    default:
        c_return_val_if_reached(0);
    }
}

RButtonState
r_motion_event_get_button_state(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return (RButtonState)rut_motion_event_get_button_state(rut_event);
}

RModifierState
r_motion_event_get_modifier_state(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return (RModifierState)rut_motion_event_get_modifier_state(rut_event);
}

float
r_motion_event_get_x(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return rut_motion_event_get_x(rut_event);
}

float
r_motion_event_get_y(RInputEvent *event)
{
    rut_input_event_t *rut_event = (rut_input_event_t *)event;

    return rut_motion_event_get_y(rut_event);
}

static void
_r_debugv(RModule *module,
          const char *format,
          va_list args)
{
    c_logv(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_DEBUG, format, args);
}

void
r_debug(RModule *module,
        const char *format,
        ...)
{
    va_list args;

    va_start(args, format);
    _r_debugv(module, format, args);
    va_end(args);
}

typedef struct _RObject RObject;

RObject *
r_find(RModule *module, const char *name)
{
    return NULL;
}
