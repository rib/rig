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

#include "rut-sdl-keysyms.h"

#include <clib.h>

int32_t
_rut_keysym_from_sdl_keysym(RutSDLKeycode sdl_keysym)
{
    switch (sdl_keysym) {
    /* ASCII mapped keysyms */
    case SDLK_BACKSPACE:
        return RUT_KEY_BackSpace;
    case SDLK_TAB:
        return RUT_KEY_Tab;
    case SDLK_CLEAR:
        return RUT_KEY_Clear;
    case SDLK_RETURN:
        return RUT_KEY_Return;
    case SDLK_PAUSE:
        return RUT_KEY_Pause;
    case SDLK_ESCAPE:
        return RUT_KEY_Escape;
    case SDLK_SPACE:
        return RUT_KEY_space;
    case SDLK_EXCLAIM:
        return RUT_KEY_exclam;
    case SDLK_QUOTEDBL:
        return RUT_KEY_quotedbl;
    case SDLK_HASH:
        return RUT_KEY_numbersign;
    case SDLK_DOLLAR:
        return RUT_KEY_dollar;
    case SDLK_AMPERSAND:
        return RUT_KEY_ampersand;
    case SDLK_QUOTE:
        return RUT_KEY_apostrophe;
    case SDLK_LEFTPAREN:
        return RUT_KEY_parenleft;
    case SDLK_RIGHTPAREN:
        return RUT_KEY_parenright;
    case SDLK_ASTERISK:
        return RUT_KEY_asterisk;
    case SDLK_PLUS:
        return RUT_KEY_plus;
    case SDLK_COMMA:
        return RUT_KEY_comma;
    case SDLK_MINUS:
        return RUT_KEY_minus;
    case SDLK_PERIOD:
        return RUT_KEY_period;
    case SDLK_SLASH:
        return RUT_KEY_slash;
    case SDLK_0:
        return RUT_KEY_0;
    case SDLK_1:
        return RUT_KEY_1;
    case SDLK_2:
        return RUT_KEY_2;
    case SDLK_3:
        return RUT_KEY_3;
    case SDLK_4:
        return RUT_KEY_4;
    case SDLK_5:
        return RUT_KEY_5;
    case SDLK_6:
        return RUT_KEY_6;
    case SDLK_7:
        return RUT_KEY_7;
    case SDLK_8:
        return RUT_KEY_8;
    case SDLK_9:
        return RUT_KEY_9;
    case SDLK_COLON:
        return RUT_KEY_colon;
    case SDLK_SEMICOLON:
        return RUT_KEY_semicolon;
    case SDLK_LESS:
        return RUT_KEY_less;
    case SDLK_EQUALS:
        return RUT_KEY_equal;
    case SDLK_GREATER:
        return RUT_KEY_greater;
    case SDLK_QUESTION:
        return RUT_KEY_question;
    case SDLK_AT:
        return RUT_KEY_at;

    /*
     * Skip uppercase letters
     */

    case SDLK_LEFTBRACKET:
        return RUT_KEY_bracketleft;
    case SDLK_BACKSLASH:
        return RUT_KEY_backslash;
    case SDLK_RIGHTBRACKET:
        return RUT_KEY_bracketright;
    case SDLK_CARET:
        return RUT_KEY_caret;
    case SDLK_UNDERSCORE:
        return RUT_KEY_underscore;
    case SDLK_BACKQUOTE:
        return RUT_KEY_quoteleft;
    case SDLK_a:
        return RUT_KEY_a;
    case SDLK_b:
        return RUT_KEY_b;
    case SDLK_c:
        return RUT_KEY_c;
    case SDLK_d:
        return RUT_KEY_d;
    case SDLK_e:
        return RUT_KEY_e;
    case SDLK_f:
        return RUT_KEY_f;
    case SDLK_g:
        return RUT_KEY_g;
    case SDLK_h:
        return RUT_KEY_h;
    case SDLK_i:
        return RUT_KEY_i;
    case SDLK_j:
        return RUT_KEY_j;
    case SDLK_k:
        return RUT_KEY_k;
    case SDLK_l:
        return RUT_KEY_l;
    case SDLK_m:
        return RUT_KEY_m;
    case SDLK_n:
        return RUT_KEY_n;
    case SDLK_o:
        return RUT_KEY_o;
    case SDLK_p:
        return RUT_KEY_p;
    case SDLK_q:
        return RUT_KEY_q;
    case SDLK_r:
        return RUT_KEY_r;
    case SDLK_s:
        return RUT_KEY_s;
    case SDLK_t:
        return RUT_KEY_t;
    case SDLK_u:
        return RUT_KEY_u;
    case SDLK_v:
        return RUT_KEY_v;
    case SDLK_w:
        return RUT_KEY_w;
    case SDLK_x:
        return RUT_KEY_x;
    case SDLK_y:
        return RUT_KEY_y;
    case SDLK_z:
        return RUT_KEY_z;
    case SDLK_DELETE:
        return RUT_KEY_Delete;

/* International keyboard keysyms */
#if SDL_MAJOR_VERSION < 2
    case SDLK_WORLD_0:
    case SDLK_WORLD_1:
    case SDLK_WORLD_2:
    case SDLK_WORLD_3:
    case SDLK_WORLD_4:
    case SDLK_WORLD_5:
    case SDLK_WORLD_6:
    case SDLK_WORLD_7:
    case SDLK_WORLD_8:
    case SDLK_WORLD_9:
    case SDLK_WORLD_10:
    case SDLK_WORLD_11:
    case SDLK_WORLD_12:
    case SDLK_WORLD_13:
    case SDLK_WORLD_14:
    case SDLK_WORLD_15:
    case SDLK_WORLD_16:
    case SDLK_WORLD_17:
    case SDLK_WORLD_18:
    case SDLK_WORLD_19:
    case SDLK_WORLD_20:
    case SDLK_WORLD_21:
    case SDLK_WORLD_22:
    case SDLK_WORLD_23:
    case SDLK_WORLD_24:
    case SDLK_WORLD_25:
    case SDLK_WORLD_26:
    case SDLK_WORLD_27:
    case SDLK_WORLD_28:
    case SDLK_WORLD_29:
    case SDLK_WORLD_30:
    case SDLK_WORLD_31:
    case SDLK_WORLD_32:
    case SDLK_WORLD_33:
    case SDLK_WORLD_34:
    case SDLK_WORLD_35:
    case SDLK_WORLD_36:
    case SDLK_WORLD_37:
    case SDLK_WORLD_38:
    case SDLK_WORLD_39:
    case SDLK_WORLD_40:
    case SDLK_WORLD_41:
    case SDLK_WORLD_42:
    case SDLK_WORLD_43:
    case SDLK_WORLD_44:
    case SDLK_WORLD_45:
    case SDLK_WORLD_46:
    case SDLK_WORLD_47:
    case SDLK_WORLD_48:
    case SDLK_WORLD_49:
    case SDLK_WORLD_50:
    case SDLK_WORLD_51:
    case SDLK_WORLD_52:
    case SDLK_WORLD_53:
    case SDLK_WORLD_54:
    case SDLK_WORLD_55:
    case SDLK_WORLD_56:
    case SDLK_WORLD_57:
    case SDLK_WORLD_58:
    case SDLK_WORLD_59:
    case SDLK_WORLD_60:
    case SDLK_WORLD_61:
    case SDLK_WORLD_62:
    case SDLK_WORLD_63:
    case SDLK_WORLD_64:
    case SDLK_WORLD_65:
    case SDLK_WORLD_66:
    case SDLK_WORLD_67:
    case SDLK_WORLD_68:
    case SDLK_WORLD_69:
    case SDLK_WORLD_70:
    case SDLK_WORLD_71:
    case SDLK_WORLD_72:
    case SDLK_WORLD_73:
    case SDLK_WORLD_74:
    case SDLK_WORLD_75:
    case SDLK_WORLD_76:
    case SDLK_WORLD_77:
    case SDLK_WORLD_78:
    case SDLK_WORLD_79:
    case SDLK_WORLD_80:
    case SDLK_WORLD_81:
    case SDLK_WORLD_82:
    case SDLK_WORLD_83:
    case SDLK_WORLD_84:
    case SDLK_WORLD_85:
    case SDLK_WORLD_86:
    case SDLK_WORLD_87:
    case SDLK_WORLD_88:
    case SDLK_WORLD_89:
    case SDLK_WORLD_90:
    case SDLK_WORLD_91:
    case SDLK_WORLD_92:
    case SDLK_WORLD_93:
    case SDLK_WORLD_94:
    case SDLK_WORLD_95:
        return sdl_keysym;
#endif /* SDL_MAJOR_VERSION < 2 */

/* Number Pad keysyms */
#if SDL_MAJOR_VERSION >= 2
#define KEYPAD_KEYSYM(digit) SDLK_KP_##digit
#else
#define KEYPAD_KEYSYM(digit) SDLK_KP##digit
#endif
    case KEYPAD_KEYSYM(0):
        return RUT_KEY_KP_0;
    case KEYPAD_KEYSYM(1):
        return RUT_KEY_KP_1;
    case KEYPAD_KEYSYM(2):
        return RUT_KEY_KP_2;
    case KEYPAD_KEYSYM(3):
        return RUT_KEY_KP_3;
    case KEYPAD_KEYSYM(4):
        return RUT_KEY_KP_4;
    case KEYPAD_KEYSYM(5):
        return RUT_KEY_KP_5;
    case KEYPAD_KEYSYM(6):
        return RUT_KEY_KP_6;
    case KEYPAD_KEYSYM(7):
        return RUT_KEY_KP_7;
    case KEYPAD_KEYSYM(8):
        return RUT_KEY_KP_8;
    case KEYPAD_KEYSYM(9):
        return RUT_KEY_KP_9;
#undef KEYPAD_KEYSYM
    case SDLK_KP_PERIOD:
        return RUT_KEY_KP_Decimal;
    case SDLK_KP_DIVIDE:
        return RUT_KEY_KP_Divide;
    case SDLK_KP_MULTIPLY:
        return RUT_KEY_KP_Multiply;
    case SDLK_KP_MINUS:
        return RUT_KEY_KP_Subtract;
    case SDLK_KP_PLUS:
        return RUT_KEY_KP_Add;
    case SDLK_KP_ENTER:
        return RUT_KEY_KP_Enter;
    case SDLK_KP_EQUALS:
        return RUT_KEY_KP_Equal;

    /* Arrows + Home/End keysyms */
    case SDLK_UP:
        return RUT_KEY_Up;
    case SDLK_DOWN:
        return RUT_KEY_Down;
    case SDLK_RIGHT:
        return RUT_KEY_Right;
    case SDLK_LEFT:
        return RUT_KEY_Left;
    case SDLK_INSERT:
        return RUT_KEY_Insert;
    case SDLK_HOME:
        return RUT_KEY_Home;
    case SDLK_END:
        return RUT_KEY_End;
    case SDLK_PAGEUP:
        return RUT_KEY_Page_Up;
    case SDLK_PAGEDOWN:
        return RUT_KEY_Page_Down;

    /* Function key keysyms */
    case SDLK_F1:
        return RUT_KEY_F1;
    case SDLK_F2:
        return RUT_KEY_F2;
    case SDLK_F3:
        return RUT_KEY_F3;
    case SDLK_F4:
        return RUT_KEY_F4;
    case SDLK_F5:
        return RUT_KEY_F5;
    case SDLK_F6:
        return RUT_KEY_F6;
    case SDLK_F7:
        return RUT_KEY_F7;
    case SDLK_F8:
        return RUT_KEY_F8;
    case SDLK_F9:
        return RUT_KEY_F9;
    case SDLK_F10:
        return RUT_KEY_F10;
    case SDLK_F11:
        return RUT_KEY_F11;
    case SDLK_F12:
        return RUT_KEY_F12;
    case SDLK_F13:
        return RUT_KEY_F13;
    case SDLK_F14:
        return RUT_KEY_F14;
    case SDLK_F15:
        return RUT_KEY_F15;

    /* Modifier keysyms */
    case SDLK_CAPSLOCK:
        return RUT_KEY_Caps_Lock;
    case SDLK_RSHIFT:
        return RUT_KEY_Shift_R;
    case SDLK_LSHIFT:
        return RUT_KEY_Shift_L;
    case SDLK_RCTRL:
        return RUT_KEY_Control_R;
    case SDLK_LCTRL:
        return RUT_KEY_Control_L;
    case SDLK_RALT:
        return RUT_KEY_Alt_R;
    case SDLK_LALT:
        return RUT_KEY_Alt_L;
    case SDLK_MODE:
        return RUT_KEY_Mode_switch;
#if SDL_MAJOR_VERSION < 2
    case SDLK_NUMLOCK:
        return RUT_KEY_Num_Lock;
    case SDLK_RMETA:
        return RUT_KEY_Meta_R;
    case SDLK_LMETA:
        return RUT_KEY_Meta_L;
    case SDLK_LSUPER:
        return RUT_KEY_Super_L;
    case SDLK_RSUPER:
        return RUT_KEY_Super_R;
    case SDLK_COMPOSE:
        return RUT_KEY_Multi_key;
    case SDLK_SCROLLOCK:
        return RUT_KEY_Scroll_Lock;
#else /* SDL_MAJOR_VERSION < 2 */
    case SDLK_SCROLLLOCK:
        return RUT_KEY_Scroll_Lock;
#endif /* SDL_MAJOR_VERSION < 2 */

    /* Miscellaneous keysyms */
    case SDLK_HELP:
        return RUT_KEY_Help;
    case SDLK_SYSREQ:
        return RUT_KEY_Sys_Req;
    case SDLK_MENU:
        return RUT_KEY_Menu;
    case SDLK_POWER:
        return RUT_KEY_PowerOff;
    case SDLK_UNDO:
        return RUT_KEY_Undo;
#if SDL_MAJOR_VERSION < 2
    case SDLK_PRINT:
        return RUT_KEY_Print;
    case SDLK_BREAK:
        return RUT_KEY_Break;
    case SDLK_EURO:
        return RUT_KEY_EuroSign;
#endif /* SDL_MAJOR_VERSION < 2 */
    }

    c_warn_if_reached();
    return RUT_KEY_VoidSymbol;
}
