#ifndef _RIG_SDL_KEYSYMS_H_
#define _RIG_SDL_KEYSYMS_H_

#include "rig-keysyms.h"

#include <SDL.h>

#include <stdint.h>

#if SDL_MAJOR_VERSION >= 2
typedef SDL_Keycode RigSDLKeycode;
#else
typedef SDLKey RigSDLKeycode;
#endif

int32_t
_rig_keysym_from_sdl_keysym (RigSDLKeycode sdl_keysym);

#endif /* _RIG_SDL_KEYSYMS_H_ */
