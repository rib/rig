#include "rig-sdl-keysyms.h"

#include <glib.h>

int32_t
_rig_keysym_from_sdl_keysym (RigSDLKeycode sdl_keysym)
{
  switch (sdl_keysym)
    {
     /* ASCII mapped keysyms */
    case SDLK_BACKSPACE: return RIG_KEY_BackSpace;
    case SDLK_TAB: return RIG_KEY_Tab;
    case SDLK_CLEAR: return RIG_KEY_Clear;
    case SDLK_RETURN: return RIG_KEY_Return;
    case SDLK_PAUSE: return RIG_KEY_Pause;
    case SDLK_ESCAPE: return RIG_KEY_Escape;
    case SDLK_SPACE: return RIG_KEY_space;
    case SDLK_EXCLAIM: return RIG_KEY_exclam;
    case SDLK_QUOTEDBL: return RIG_KEY_quotedbl;
    case SDLK_HASH: return RIG_KEY_numbersign;
    case SDLK_DOLLAR: return RIG_KEY_dollar;
    case SDLK_AMPERSAND: return RIG_KEY_ampersand;
    case SDLK_QUOTE: return RIG_KEY_apostrophe;
    case SDLK_LEFTPAREN: return RIG_KEY_parenleft;
    case SDLK_RIGHTPAREN: return RIG_KEY_parenright;
    case SDLK_ASTERISK: return RIG_KEY_asterisk;
    case SDLK_PLUS: return RIG_KEY_plus;
    case SDLK_COMMA: return RIG_KEY_comma;
    case SDLK_MINUS: return RIG_KEY_minus;
    case SDLK_PERIOD: return RIG_KEY_period;
    case SDLK_SLASH: return RIG_KEY_slash;
    case SDLK_0: return RIG_KEY_0;
    case SDLK_1: return RIG_KEY_1;
    case SDLK_2: return RIG_KEY_2;
    case SDLK_3: return RIG_KEY_3;
    case SDLK_4: return RIG_KEY_4;
    case SDLK_5: return RIG_KEY_5;
    case SDLK_6: return RIG_KEY_6;
    case SDLK_7: return RIG_KEY_7;
    case SDLK_8: return RIG_KEY_8;
    case SDLK_9: return RIG_KEY_9;
    case SDLK_COLON: return RIG_KEY_colon;
    case SDLK_SEMICOLON: return RIG_KEY_semicolon;
    case SDLK_LESS: return RIG_KEY_less;
    case SDLK_EQUALS: return RIG_KEY_equal;
    case SDLK_GREATER: return RIG_KEY_greater;
    case SDLK_QUESTION: return RIG_KEY_question;
    case SDLK_AT: return RIG_KEY_at;

    /*
     * Skip uppercase letters
     */

    case SDLK_LEFTBRACKET: return RIG_KEY_bracketleft;
    case SDLK_BACKSLASH: return RIG_KEY_backslash;
    case SDLK_RIGHTBRACKET: return RIG_KEY_bracketright;
    case SDLK_CARET: return RIG_KEY_caret;
    case SDLK_UNDERSCORE: return RIG_KEY_underscore;
    case SDLK_BACKQUOTE: return RIG_KEY_quoteleft;
    case SDLK_a: return RIG_KEY_a;
    case SDLK_b: return RIG_KEY_b;
    case SDLK_c: return RIG_KEY_c;
    case SDLK_d: return RIG_KEY_d;
    case SDLK_e: return RIG_KEY_e;
    case SDLK_f: return RIG_KEY_f;
    case SDLK_g: return RIG_KEY_g;
    case SDLK_h: return RIG_KEY_h;
    case SDLK_i: return RIG_KEY_i;
    case SDLK_j: return RIG_KEY_j;
    case SDLK_k: return RIG_KEY_k;
    case SDLK_l: return RIG_KEY_l;
    case SDLK_m: return RIG_KEY_m;
    case SDLK_n: return RIG_KEY_n;
    case SDLK_o: return RIG_KEY_o;
    case SDLK_p: return RIG_KEY_p;
    case SDLK_q: return RIG_KEY_q;
    case SDLK_r: return RIG_KEY_r;
    case SDLK_s: return RIG_KEY_s;
    case SDLK_t: return RIG_KEY_t;
    case SDLK_u: return RIG_KEY_u;
    case SDLK_v: return RIG_KEY_v;
    case SDLK_w: return RIG_KEY_w;
    case SDLK_x: return RIG_KEY_x;
    case SDLK_y: return RIG_KEY_y;
    case SDLK_z: return RIG_KEY_z;
    case SDLK_DELETE: return RIG_KEY_Delete;

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
#define KEYPAD_KEYSYM(digit) SDLK_KP_ ## digit
#else
#define KEYPAD_KEYSYM(digit) SDLK_KP ## digit
#endif
    case KEYPAD_KEYSYM(0): return RIG_KEY_KP_0;
    case KEYPAD_KEYSYM(1): return RIG_KEY_KP_1;
    case KEYPAD_KEYSYM(2): return RIG_KEY_KP_2;
    case KEYPAD_KEYSYM(3): return RIG_KEY_KP_3;
    case KEYPAD_KEYSYM(4): return RIG_KEY_KP_4;
    case KEYPAD_KEYSYM(5): return RIG_KEY_KP_5;
    case KEYPAD_KEYSYM(6): return RIG_KEY_KP_6;
    case KEYPAD_KEYSYM(7): return RIG_KEY_KP_7;
    case KEYPAD_KEYSYM(8): return RIG_KEY_KP_8;
    case KEYPAD_KEYSYM(9): return RIG_KEY_KP_9;
#undef KEYPAD_KEYSYM
    case SDLK_KP_PERIOD: return RIG_KEY_KP_Decimal;
    case SDLK_KP_DIVIDE: return RIG_KEY_KP_Divide;
    case SDLK_KP_MULTIPLY: return RIG_KEY_KP_Multiply;
    case SDLK_KP_MINUS: return RIG_KEY_KP_Subtract;
    case SDLK_KP_PLUS: return RIG_KEY_KP_Add;
    case SDLK_KP_ENTER: return RIG_KEY_KP_Enter;
    case SDLK_KP_EQUALS: return RIG_KEY_KP_Equal;

    /* Arrows + Home/End keysyms */
    case SDLK_UP: return RIG_KEY_Up;
    case SDLK_DOWN: return RIG_KEY_Down;
    case SDLK_RIGHT: return RIG_KEY_Right;
    case SDLK_LEFT: return RIG_KEY_Left;
    case SDLK_INSERT: return RIG_KEY_Insert;
    case SDLK_HOME: return RIG_KEY_Home;
    case SDLK_END: return RIG_KEY_End;
    case SDLK_PAGEUP: return RIG_KEY_Page_Up;
    case SDLK_PAGEDOWN: return RIG_KEY_Page_Down;

    /* Function key keysyms */
    case SDLK_F1: return RIG_KEY_F1;
    case SDLK_F2: return RIG_KEY_F2;
    case SDLK_F3: return RIG_KEY_F3;
    case SDLK_F4: return RIG_KEY_F4;
    case SDLK_F5: return RIG_KEY_F5;
    case SDLK_F6: return RIG_KEY_F6;
    case SDLK_F7: return RIG_KEY_F7;
    case SDLK_F8: return RIG_KEY_F8;
    case SDLK_F9: return RIG_KEY_F9;
    case SDLK_F10: return RIG_KEY_F10;
    case SDLK_F11: return RIG_KEY_F11;
    case SDLK_F12: return RIG_KEY_F12;
    case SDLK_F13: return RIG_KEY_F13;
    case SDLK_F14: return RIG_KEY_F14;
    case SDLK_F15: return RIG_KEY_F15;

    /* Modifier keysyms */
    case SDLK_CAPSLOCK: return RIG_KEY_Caps_Lock;
    case SDLK_RSHIFT: return RIG_KEY_Shift_R;
    case SDLK_LSHIFT: return RIG_KEY_Shift_L;
    case SDLK_RCTRL: return RIG_KEY_Control_R;
    case SDLK_LCTRL: return RIG_KEY_Control_L;
    case SDLK_RALT: return RIG_KEY_Alt_R;
    case SDLK_LALT: return RIG_KEY_Alt_L;
    case SDLK_MODE: return RIG_KEY_Mode_switch;
#if SDL_MAJOR_VERSION < 2
    case SDLK_NUMLOCK: return RIG_KEY_Num_Lock;
    case SDLK_RMETA: return RIG_KEY_Meta_R;
    case SDLK_LMETA: return RIG_KEY_Meta_L;
    case SDLK_LSUPER: return RIG_KEY_Super_L;
    case SDLK_RSUPER: return RIG_KEY_Super_R;
    case SDLK_COMPOSE: return RIG_KEY_Multi_key;
    case SDLK_SCROLLOCK: return RIG_KEY_Scroll_Lock;
#else /* SDL_MAJOR_VERSION < 2 */
    case SDLK_SCROLLLOCK: return RIG_KEY_Scroll_Lock;
#endif /* SDL_MAJOR_VERSION < 2 */

    /* Miscellaneous keysyms */
    case SDLK_HELP: return RIG_KEY_Help;
    case SDLK_SYSREQ: return RIG_KEY_Sys_Req;
    case SDLK_MENU: return RIG_KEY_Menu;
    case SDLK_POWER: return RIG_KEY_PowerOff;
    case SDLK_UNDO: return RIG_KEY_Undo;
#if SDL_MAJOR_VERSION < 2
    case SDLK_PRINT: return RIG_KEY_Print;
    case SDLK_BREAK: return RIG_KEY_Break;
    case SDLK_EURO: return RIG_KEY_EuroSign;
#endif /* SDL_MAJOR_VERSION < 2 */
    }

  g_warn_if_reached ();
  return RIG_KEY_VoidSymbol;
}
