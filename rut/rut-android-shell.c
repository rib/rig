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

#include <rut-config.h>

#include <android_native_app_glue.h>

#include <cglib/cglib.h>

#include "rut-shell.h"

static void
android_handle_cmd (struct android_app *app,
                    int32_t cmd)
{
    rut_shell_t *shell = (rut_shell_t *)app->userData;

    switch (cmd)
    {
    case APP_CMD_SAVE_STATE:
        c_message("command: SAVE_STATE");
        break;
    case APP_CMD_INIT_WINDOW:
        /* The window is being shown, get it ready */
        c_message("command: INIT_WINDOW");
        if (shell->android_application->window != NULL) {
            cg_android_set_native_window (shell->android_application->window);

            if (shell->on_run_cb)
                shell->on_run_cb(shell, shell->on_run_data);

            rut_shell_queue_redraw(shell);
        }
        break;

    case APP_CMD_TERM_WINDOW:
        /* The window is being hidden or closed, clean it up */
        c_message("command: TERM_WINDOW");
        shell->quit = true;
        break;

    case APP_CMD_GAINED_FOCUS:
        c_message("command: GAINED_FOCUS");
        break;

    case APP_CMD_LOST_FOCUS:
        c_message("command: LOST_FOCUS");
        break;
    }
}

/**
 * Process the next input event.
 */
void
rut_android_shell_handle_input(rut_shell_t *shell,
                               AInputEvent *android_event)
{
    int32_t type = AInputEvent_getType(android_event);
    rut_input_event_t *event = NULL;

    switch (type) {
    case AINPUT_EVENT_TYPE_MOTION:
    case AINPUT_EVENT_TYPE_KEY:

        /* We queue input events to be handled on a per-frame
         * basis instead of dispatching them immediately...
         */

        event = c_slice_alloc(sizeof(rut_input_event_t));

        event->native = android_event;
        event->shell = shell;
        event->input_transform = NULL;

        /* We assume there's only one onscreen... */
        event->onscreen =
            rut_container_of(shell->onscreens.next, event->onscreen, link);
    default:
        break;
    }

    if (event) {
        switch (type) {
        case AINPUT_EVENT_TYPE_MOTION:
            event->type = RUT_INPUT_EVENT_TYPE_MOTION;
            break;

        case AINPUT_EVENT_TYPE_KEY:
            event->type = RUT_INPUT_EVENT_TYPE_KEY;
            break;
        }

        rut_input_queue_append(shell->input_queue, event);

        /* FIXME: we need a separate status so we can trigger a new
         * frame, but if the input doesn't affect anything then we
         * want to avoid any actual rendering. */
        rut_shell_queue_redraw(shell);
    }
}

static int32_t
rut_android_key_event_get_keysym(rut_input_event_t *event)
{
    static int android_key_code_map[] = {
        RUT_KEY_VoidSymbol, /* AKEYCODE_VoidSymbol */
        RUT_KEY_VoidSymbol, /* AKEYCODE_SOFT_LEFT */
        RUT_KEY_VoidSymbol, /* AKEYCODE_SOFT_RIGHT */
        RUT_KEY_Home, /* AKEYCODE_HOME */
        RUT_KEY_Back, /* AKEYCODE_BACK */
        RUT_KEY_VoidSymbol, /* AKEYCODE_CALL */
        RUT_KEY_VoidSymbol, /* AKEYCODE_ENDCALL */
        RUT_KEY_0, /* AKEYCODE_0 */
        RUT_KEY_1, /* AKEYCODE_1 */
        RUT_KEY_2, /* AKEYCODE_2 */
        RUT_KEY_3, /* AKEYCODE_3 */
        RUT_KEY_4, /* AKEYCODE_4 */
        RUT_KEY_5, /* AKEYCODE_5 */
        RUT_KEY_6, /* AKEYCODE_6 */
        RUT_KEY_7, /* AKEYCODE_7 */
        RUT_KEY_8, /* AKEYCODE_8 */
        RUT_KEY_9, /* AKEYCODE_9 */
        RUT_KEY_asterisk, /* AKEYCODE_STAR */
        RUT_KEY_numbersign, /* AKEYCODE_POUND */
        RUT_KEY_Up, /* AKEYCODE_DPAD_UP */
        RUT_KEY_Down, /* AKEYCODE_DPAD_DOWN */
        RUT_KEY_Left, /* AKEYCODE_DPAD_LEFT */
        RUT_KEY_Right, /* AKEYCODE_DPAD_RIGHT */
        RUT_KEY_Select, /* AKEYCODE_DPAD_CENTER */
        RUT_KEY_AudioRaiseVolume, /* AKEYCODE_VOLUME_UP */
        RUT_KEY_AudioLowerVolume, /* AKEYCODE_VOLUME_DOWN */
        RUT_KEY_PowerOff, /* AKEYCODE_POWER */
        RUT_KEY_VoidSymbol, /* AKEYCODE_CAMERA */
        RUT_KEY_Clear, /* AKEYCODE_CLEAR */
        RUT_KEY_A, /* AKEYCODE_A */
        RUT_KEY_B, /* AKEYCODE_B */
        RUT_KEY_C, /* AKEYCODE_C */
        RUT_KEY_D, /* AKEYCODE_D */
        RUT_KEY_E, /* AKEYCODE_E */
        RUT_KEY_F, /* AKEYCODE_F */
        RUT_KEY_G, /* AKEYCODE_G */
        RUT_KEY_H, /* AKEYCODE_H */
        RUT_KEY_I, /* AKEYCODE_I */
        RUT_KEY_J, /* AKEYCODE_J */
        RUT_KEY_K, /* AKEYCODE_K */
        RUT_KEY_L, /* AKEYCODE_L */
        RUT_KEY_M, /* AKEYCODE_M */
        RUT_KEY_N, /* AKEYCODE_N */
        RUT_KEY_O, /* AKEYCODE_O */
        RUT_KEY_P, /* AKEYCODE_P */
        RUT_KEY_Q, /* AKEYCODE_Q */
        RUT_KEY_R, /* AKEYCODE_R */
        RUT_KEY_S, /* AKEYCODE_S */
        RUT_KEY_T, /* AKEYCODE_T */
        RUT_KEY_U, /* AKEYCODE_U */
        RUT_KEY_V, /* AKEYCODE_V */
        RUT_KEY_W, /* AKEYCODE_W */
        RUT_KEY_X, /* AKEYCODE_X */
        RUT_KEY_Y, /* AKEYCODE_Y */
        RUT_KEY_Z, /* AKEYCODE_Z */
        RUT_KEY_comma, /* AKEYCODE_COMMA */
        RUT_KEY_period, /* AKEYCODE_PERIOD */
        RUT_KEY_Alt_L, /* AKEYCODE_ALT_LEFT */
        RUT_KEY_Alt_R, /* AKEYCODE_ALT_RIGHT */
        RUT_KEY_Shift_L, /* AKEYCODE_SHIFT_LEFT */
        RUT_KEY_Shift_R, /* AKEYCODE_SHIFT_RIGHT */
        RUT_KEY_Tab, /* AKEYCODE_TAB */
        RUT_KEY_space, /* AKEYCODE_SPACE */
        RUT_KEY_VoidSymbol, /* AKEYCODE_SYM */
        RUT_KEY_WWW, /* AKEYCODE_EXPLORER */
        RUT_KEY_Mail, /* AKEYCODE_ENVELOPE */
        RUT_KEY_Return, /* AKEYCODE_ENTER */
        RUT_KEY_BackSpace, /* AKEYCODE_DEL */
        RUT_KEY_grave, /* AKEYCODE_GRAVE */
        RUT_KEY_minus, /* AKEYCODE_MINUS */
        RUT_KEY_equal, /* AKEYCODE_EQUALS */
        RUT_KEY_bracketleft, /* AKEYCODE_LEFT_BRACKET */
        RUT_KEY_bracketright, /* AKEYCODE_RIGHT_BRACKET */
        RUT_KEY_backslash, /* AKEYCODE_BACKSLASH */
        RUT_KEY_semicolon, /* AKEYCODE_SEMICOLON */
        RUT_KEY_apostrophe, /* AKEYCODE_APOSTROPHE */
        RUT_KEY_slash, /* AKEYCODE_SLASH */
        RUT_KEY_at, /* AKEYCODE_AT */
        RUT_KEY_Alt_L, /* AKEYCODE_NUM */
        RUT_KEY_VoidSymbol, /* AKEYCODE_HEADSETHOOK */
        RUT_KEY_VoidSymbol, /* AKEYCODE_FOCUS */
        RUT_KEY_plus, /* AKEYCODE_PLUS */
        RUT_KEY_Menu, /* AKEYCODE_MENU */
        RUT_KEY_VoidSymbol, /* AKEYCODE_NOTIFICATION */
        RUT_KEY_Search, /* AKEYCODE_SEARCH */
        RUT_KEY_AudioPlay, /* AKEYCODE_MEDIA_PLAY_PAUSE */
        RUT_KEY_AudioStop, /* AKEYCODE_MEDIA_STOP */
        RUT_KEY_AudioNext, /* AKEYCODE_MEDIA_NEXT */
        RUT_KEY_AudioPrev, /* AKEYCODE_MEDIA_PREVIOUS */
        RUT_KEY_AudioRewind, /* AKEYCODE_MEDIA_REWIND */
        RUT_KEY_AudioForward, /* AKEYCODE_MEDIA_FAST_FORWARD */
        RUT_KEY_AudioMute, /* AKEYCODE_MUTE */
        RUT_KEY_Page_Up, /* AKEYCODE_PAGE_UP */
        RUT_KEY_Page_Down, /* AKEYCODE_PAGE_DOWN */
        RUT_KEY_VoidSymbol, /* AKEYCODE_PICTSYMBOLS */
        RUT_KEY_VoidSymbol, /* AKEYCODE_SWITCH_CHARSET */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_A */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_B */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_C */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_X */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_Y */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_Z */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_L1 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_R1 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_L2 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_R2 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_THUMBL */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_THUMBR */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_START */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_SELECT */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_MODE */
        RUT_KEY_Escape, /* AKEYCODE_ESCAPE */
        RUT_KEY_Delete, /* AKEYCODE_FORWARD_DEL */
        RUT_KEY_Control_L, /* AKEYCODE_CTRL_LEFT */
        RUT_KEY_Control_R, /* AKEYCODE_CTRL_RIGHT */
        RUT_KEY_Caps_Lock, /* AKEYCODE_CAPS_LOCK */
        RUT_KEY_Scroll_Lock, /* AKEYCODE_SCROLL_LOCK */
        RUT_KEY_Meta_L, /* AKEYCODE_META_LEFT */
        RUT_KEY_Meta_R, /* AKEYCODE_META_RIGHT */
        RUT_KEY_VoidSymbol, /* AKEYCODE_FUNCTION */
        RUT_KEY_Sys_Req, /* AKEYCODE_SYSRQ */
        RUT_KEY_Pause, /* AKEYCODE_BREAK */
        RUT_KEY_Home, /* AKEYCODE_MOVE_HOME */
        RUT_KEY_End, /* AKEYCODE_MOVE_END */
        RUT_KEY_Insert, /* AKEYCODE_INSERT */
        RUT_KEY_Forward, /* AKEYCODE_FORWARD */
        RUT_KEY_AudioPlay, /* AKEYCODE_MEDIA_PLAY */
        RUT_KEY_AudioPause, /* AKEYCODE_MEDIA_PAUSE */
        RUT_KEY_Close, /* AKEYCODE_MEDIA_CLOSE */
        RUT_KEY_Eject, /* AKEYCODE_MEDIA_EJECT */
        RUT_KEY_AudioRecord, /* AKEYCODE_MEDIA_RECORD */
        RUT_KEY_F1, /* AKEYCODE_F1 */
        RUT_KEY_F2, /* AKEYCODE_F2 */
        RUT_KEY_F3, /* AKEYCODE_F3 */
        RUT_KEY_F4, /* AKEYCODE_F4 */
        RUT_KEY_F5, /* AKEYCODE_F5 */
        RUT_KEY_F6, /* AKEYCODE_F6 */
        RUT_KEY_F7, /* AKEYCODE_F7 */
        RUT_KEY_F8, /* AKEYCODE_F8 */
        RUT_KEY_F9, /* AKEYCODE_F9 */
        RUT_KEY_F10, /* AKEYCODE_F10 */
        RUT_KEY_F11, /* AKEYCODE_F11 */
        RUT_KEY_F12, /* AKEYCODE_F12 */
        RUT_KEY_Num_Lock, /* AKEYCODE_NUM_LOCK */
        RUT_KEY_KP_0, /* AKEYCODE_NUMPAD_0 */
        RUT_KEY_KP_1, /* AKEYCODE_NUMPAD_1 */
        RUT_KEY_KP_2, /* AKEYCODE_NUMPAD_2 */
        RUT_KEY_KP_3, /* AKEYCODE_NUMPAD_3 */
        RUT_KEY_KP_4, /* AKEYCODE_NUMPAD_4 */
        RUT_KEY_KP_5, /* AKEYCODE_NUMPAD_5 */
        RUT_KEY_KP_6, /* AKEYCODE_NUMPAD_6 */
        RUT_KEY_KP_7, /* AKEYCODE_NUMPAD_7 */
        RUT_KEY_KP_8, /* AKEYCODE_NUMPAD_8 */
        RUT_KEY_KP_9, /* AKEYCODE_NUMPAD_9 */
        RUT_KEY_KP_Divide, /* AKEYCODE_NUMPAD_DIVIDE */
        RUT_KEY_KP_Multiply, /* AKEYCODE_NUMPAD_MULTIPLY */
        RUT_KEY_KP_Subtract, /* AKEYCODE_NUMPAD_SUBTRACT */
        RUT_KEY_KP_Add, /* AKEYCODE_NUMPAD_ADD */
        RUT_KEY_KP_Decimal, /* AKEYCODE_NUMPAD_DOT */
        RUT_KEY_KP_Separator, /* AKEYCODE_NUMPAD_COMMA */
        RUT_KEY_KP_Enter, /* AKEYCODE_NUMPAD_ENTER */
        RUT_KEY_KP_Equal, /* AKEYCODE_NUMPAD_EQUALS */
        RUT_KEY_parenleft, /* AKEYCODE_NUMPAD_LEFT_PAREN */
        RUT_KEY_parenright, /* AKEYCODE_NUMPAD_RIGHT_PAREN */
        RUT_KEY_AudioMute, /* AKEYCODE_VOLUME_MUTE */
        RUT_KEY_VoidSymbol, /* AKEYCODE_INFO */
        RUT_KEY_VoidSymbol, /* AKEYCODE_CHANNEL_UP */
        RUT_KEY_VoidSymbol, /* AKEYCODE_CHANNEL_DOWN */
        RUT_KEY_ZoomIn, /* AKEYCODE_ZOOM_IN */
        RUT_KEY_ZoomOut, /* AKEYCODE_ZOOM_OUT */
        RUT_KEY_VoidSymbol, /* AKEYCODE_TV */
        RUT_KEY_VoidSymbol, /* AKEYCODE_WINDOW */
        RUT_KEY_VoidSymbol, /* AKEYCODE_GUIDE */
        RUT_KEY_VoidSymbol, /* AKEYCODE_DVR */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BOOKMARK */
        RUT_KEY_VoidSymbol, /* AKEYCODE_CAPTIONS */
        RUT_KEY_VoidSymbol, /* AKEYCODE_SETTINGS */
        RUT_KEY_VoidSymbol, /* AKEYCODE_TV_POWER */
        RUT_KEY_VoidSymbol, /* AKEYCODE_TV_INPUT */
        RUT_KEY_VoidSymbol, /* AKEYCODE_STB_POWER */
        RUT_KEY_VoidSymbol, /* AKEYCODE_STB_INPUT */
        RUT_KEY_VoidSymbol, /* AKEYCODE_AVR_POWER */
        RUT_KEY_VoidSymbol, /* AKEYCODE_AVR_INPUT */
        RUT_KEY_VoidSymbol, /* AKEYCODE_PROG_RED */
        RUT_KEY_VoidSymbol, /* AKEYCODE_PROG_GREEN */
        RUT_KEY_VoidSymbol, /* AKEYCODE_PROG_YELLOW */
        RUT_KEY_VoidSymbol, /* AKEYCODE_PROG_BLUE */
        RUT_KEY_VoidSymbol, /* AKEYCODE_APP_SWITCH */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_1 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_2 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_3 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_4 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_5 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_6 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_7 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_8 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_9 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_10 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_11 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_12 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_13 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_14 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_15 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_BUTTON_16 */
        RUT_KEY_VoidSymbol, /* AKEYCODE_LANGUAGE_SWITCH */
        RUT_KEY_VoidSymbol, /* AKEYCODE_MANNER_MODE */
        RUT_KEY_VoidSymbol, /* AKEYCODE_3D_MODE */
        RUT_KEY_VoidSymbol, /* AKEYCODE_CONTACTS */
        RUT_KEY_Calendar, /* AKEYCODE_CALENDAR */
        RUT_KEY_Music, /* AKEYCODE_MUSIC */
        RUT_KEY_Calculator, /* AKEYCODE_CALCULATOR */
        RUT_KEY_Zenkaku_Hankaku, /* AKEYCODE_ZENKAKU_HANKAKU */
        RUT_KEY_Eisu_Shift, /* AKEYCODE_EISU */
        RUT_KEY_Muhenkan, /* AKEYCODE_MUHENKAN */
        RUT_KEY_Henkan, /* AKEYCODE_HENKAN */
        RUT_KEY_Hiragana_Katakana, /* AKEYCODE_KATAKANA_HIRAGANA */
        RUT_KEY_yen, /* AKEYCODE_YEN */
        RUT_KEY_Romaji, /* AKEYCODE_RO */
        RUT_KEY_Kana_Shift, /* AKEYCODE_KANA */
        RUT_KEY_VoidSymbol, /* AKEYCODE_ASSIST */
        RUT_KEY_MonBrightnessDown, /* AKEYCODE_BRIGHTNESS_DOWN */
        RUT_KEY_MonBrightnessUp, /* AKEYCODE_BRIGHTNESS_UP */
        RUT_KEY_AudioCycleTrack, /* AKEYCODE_MEDIA_AUDIO_TRACK */
    };

    int32_t keycode = AKeyEvent_getKeyCode(event->native);

    if (keycode < C_N_ELEMENTS(android_key_code_map))
        return android_key_code_map[keycode];
    else
        return RUT_KEY_VoidSymbol;
}

static rut_key_event_action_t
rut_android_key_event_get_action(rut_input_event_t *event)
{
    switch (AKeyEvent_getAction (event->native)) {
    case AKEY_EVENT_ACTION_DOWN:
        return RUT_KEY_EVENT_ACTION_DOWN;
    case AKEY_EVENT_ACTION_UP:
        return RUT_KEY_EVENT_ACTION_UP;
    case AKEY_EVENT_ACTION_MULTIPLE:
        c_warn_if_reached ();

        /* TODO: Expand these out in android_handle_event into
         * multiple distinct events, it seems odd to require app
         * developers to have to have special code for this and key
         * events are surely always low frequency enough that we don't
         * need this for optimization purposes.
         */
        return RUT_KEY_EVENT_ACTION_UP;
    }
}

static rut_modifier_state_t
modifier_state_for_android_meta (int32_t meta)
{
  rut_modifier_state_t rut_state = 0;

  if (meta & AMETA_SHIFT_ON)
    rut_state |= RUT_MODIFIER_SHIFT_ON;

  if (meta & AMETA_CTRL_ON)
    rut_state |= RUT_MODIFIER_CTRL_ON;

  if (meta & AMETA_ALT_ON)
    rut_state |= RUT_MODIFIER_ALT_ON;

  if (meta & AMETA_CAPS_LOCK_ON)
    rut_state |= RUT_MODIFIER_CAPS_LOCK_ON;

  if (meta & AMETA_NUM_LOCK_ON)
    rut_state |= RUT_MODIFIER_NUM_LOCK_ON;

  return rut_state;
}

static rut_modifier_state_t
rut_android_key_event_get_modifier_state(rut_input_event_t *event)
{
    int32_t meta = AKeyEvent_getMetaState (event->native);
    return modifier_state_for_android_meta (meta);
}

static rut_motion_event_action_t
rut_android_motion_event_get_action(rut_input_event_t *event)
{
    switch (AMotionEvent_getAction (event->native) &
            AMOTION_EVENT_ACTION_MASK) {
    case AMOTION_EVENT_ACTION_DOWN:
        return RUT_MOTION_EVENT_ACTION_DOWN;
    case AMOTION_EVENT_ACTION_UP:
        return RUT_MOTION_EVENT_ACTION_UP;
    case AMOTION_EVENT_ACTION_MOVE:
        return RUT_MOTION_EVENT_ACTION_MOVE;
    }
}

static rut_button_state_t
rut_android_motion_event_get_button(rut_input_event_t *event)
{
    int pointer_index;

    /* We currently just assume this api is used for handling
     * mouse input... */
    if (AInputEvent_getSource (event->native) !=
        AINPUT_SOURCE_MOUSE)
        return RUT_BUTTON_STATE_1;

    pointer_index = ((AMotionEvent_getAction (event->native) &
                      AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                     AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

#warning "fixme: figure out how a pointer_index can be mapped to a mouse button"
    return RUT_BUTTON_STATE_1 + pointer_index;
}

static rut_button_state_t
rut_android_motion_event_get_button_state(rut_input_event_t *event)
{
    c_warn_if_reached(); /* TODO */
    return 0;
}

static rut_modifier_state_t
rut_android_motion_event_get_modifier_state(rut_input_event_t *event)
{
    int32_t meta = AMotionEvent_getMetaState (event->native);
    return modifier_state_for_android_meta (meta);
}

static void
rut_android_motion_event_get_transformed_xy(rut_input_event_t *event,
                                            float *x,
                                            float *y)
{
    *x = AMotionEvent_getX(event->native, 0);
    *y = AMotionEvent_getY(event->native, 0);
}

static const char *
rut_android_text_event_get_text(rut_input_event_t *event)
{
    c_warn_if_reached(); /* TODO */
    return "";
}

static void
rut_android_free_input_event(rut_input_event_t *event)
{
    rut_shell_t *shell = event->shell;
    struct android_app *app = shell->android_application;

    AInputQueue_finishEvent(app->inputQueue,
                            event->native,
                            true /* handled */);

    c_slice_free1(sizeof(rut_input_event_t), event);
}

bool
rut_android_shell_init(rut_shell_t *shell)
{
    struct android_app *application = shell->android_application;
    cg_error_t *error = NULL;

    shell->cg_device = cg_device_new();
    cg_device_connect(shell->cg_device, &error);
    if (!shell->cg_device) {
        c_warning("Failed to create Cogl Context: %s", error->message);
        cg_error_free(error);
        return false;
    }

    application->userData = shell;
    application->onAppCmd = android_handle_cmd;

    shell->platform.type = RUT_SHELL_ANDROID_PLATFORM;

    shell->platform.key_event_get_keysym = rut_android_key_event_get_keysym;
    shell->platform.key_event_get_action = rut_android_key_event_get_action;
    shell->platform.key_event_get_modifier_state = rut_android_key_event_get_modifier_state;

    shell->platform.motion_event_get_action = rut_android_motion_event_get_action;
    shell->platform.motion_event_get_button = rut_android_motion_event_get_button;
    shell->platform.motion_event_get_button_state = rut_android_motion_event_get_button_state;
    shell->platform.motion_event_get_modifier_state = rut_android_motion_event_get_modifier_state;
    shell->platform.motion_event_get_transformed_xy = rut_android_motion_event_get_transformed_xy;

    shell->platform.text_event_get_text = rut_android_text_event_get_text;

    shell->platform.free_input_event = rut_android_free_input_event;

    return true;
}
