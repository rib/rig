#ifndef _RUT_SDL_KEYSYMS_H_
#define _RUT_SDL_KEYSYMS_H_

#include "rut-keysyms.h"

#include <SDL.h>

#include <stdint.h>

#if SDL_MAJOR_VERSION >= 2
typedef SDL_Keycode RutSDLKeycode;
#else
typedef SDLKey RutSDLKeycode;
#endif

int32_t
_rut_keysym_from_sdl_keysym (RutSDLKeycode sdl_keysym);

#endif /* _RUT_SDL_KEYSYMS_H_ */
