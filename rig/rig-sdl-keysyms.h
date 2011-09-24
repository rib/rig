#ifndef _RIG_SDL_KEYSYMS_H_
#define _RIG_SDL_KEYSYMS_H_

#include "rig-keysyms.h"

#include <SDL.h>

#include <stdint.h>

int32_t
_rig_keysym_from_sdl_keysym (SDLKey sdl_keysym);

#endif /* _RIG_SDL_KEYSYMS_H_ */
