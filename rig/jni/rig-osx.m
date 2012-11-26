/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL.h>
#include <SDL_syswm.h>
#include <cogl/cogl.h>
#include <Cocoa/Cocoa.h>

#include "rig-osx.h"

void
rig_osx_init_onscreen (CoglOnscreen *onscreen)
{
  SDL_Window *window = cogl_sdl_onscreen_get_window (onscreen);
  SDL_SysWMinfo info;

  SDL_VERSION (&info.version);

  if (!SDL_GetWindowWMInfo (window, &info))
    return;

  [info.info.cocoa.window setShowsResizeIndicator:YES];
}
