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

#ifndef _RIG_C_
#define _RIG_C_

#include <stdint.h>

typedef struct _RModule RModule;

typedef struct _RInputEvent RInputEvent;

typedef enum {
    R_INPUT_EVENT_TYPE_MOTION = 1,
    R_INPUT_EVENT_TYPE_KEY,
    R_INPUT_EVENT_TYPE_TEXT,
} RInputEventType;

typedef enum {
    R_KEY_EVENT_ACTION_UP = 1,
    R_KEY_EVENT_ACTION_DOWN
} RKeyEventAction;

typedef enum _r_motion_event_action_t {
    R_MOTION_EVENT_ACTION_UP = 1,
    R_MOTION_EVENT_ACTION_DOWN,
    R_MOTION_EVENT_ACTION_MOVE,
} RMotionEventAction;

typedef enum {
    R_BUTTON_STATE_1 = 1 << 1,
    R_BUTTON_STATE_2 = 1 << 2,
    R_BUTTON_STATE_3 = 1 << 3,
} RButtonState;

typedef enum {
    R_MODIFIER_SHIFT_ON = 1 << 0,
    R_MODIFIER_CTRL_ON = 1 << 1,
    R_MODIFIER_ALT_ON = 1 << 2,
    R_MODIFIER_NUM_LOCK_ON = 1 << 3,
    R_MODIFIER_CAPS_LOCK_ON = 1 << 4
} RModifierState;

typedef enum {
    R_INPUT_EVENT_STATUS_UNHANDLED,
    R_INPUT_EVENT_STATUS_HANDLED,
} RInputEventStatus;

RInputEventType r_input_event_get_type(RInputEvent *event);

RKeyEventAction r_key_event_get_action(RInputEvent *event);

int32_t r_key_event_get_keysym(RInputEvent *event);

RModifierState r_key_event_get_modifier_state(RInputEvent *event);

RMotionEventAction r_motion_event_get_action(RInputEvent *event);

RButtonState r_motion_event_get_button(RInputEvent *event);

RButtonState r_motion_event_get_button_state(RInputEvent *event);

RModifierState r_motion_event_get_modifier_state(RInputEvent *event);

float r_motion_event_get_x(RInputEvent *event);
float r_motion_event_get_y(RInputEvent *event);

void r_debug(RModule *module,
             const char *format,
             ...);

#endif /* _RIG_C_ */
