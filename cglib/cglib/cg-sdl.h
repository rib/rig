/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
 *
 *
 */

#ifndef __CG_SDL_H__
#define __CG_SDL_H__

/* NB: this is a top-level header that can be included directly but we
 * want to be careful not to define __CG_H_INSIDE__ when this is
 * included internally while building CGlib itself since
 * __CG_H_INSIDE__ is used in headers to guard public vs private api
 * definitions
 */
#ifndef CG_COMPILATION
#define __CG_H_INSIDE__
#endif

#include <cglib/cg-device.h>
#include <cglib/cg-onscreen.h>
#include <SDL.h>

#ifdef _MSC_VER
/* We need to link to SDL.lib/SDLmain.lib
 * if we are using CGlib
 * that uses the SDL winsys
 */
#pragma comment(lib, "SDL.lib")
#pragma comment(lib, "SDLmain.lib")
#endif

CG_BEGIN_DECLS

/**
 * SECTION:cg-sdl
 * @short_description: Integration api for the Simple DirectMedia
 *                     Layer library.
 *
 * CGlib is a portable graphics api that can either be used standalone
 * or alternatively integrated with certain existing frameworks. This
 * api enables CGlib to be used in conjunction with the Simple
 * DirectMedia Layer library.
 *
 * Using this API a typical SDL application would look something like
 * this:
 * |[
 * MyAppData data;
 * cg_error_t *error = NULL;
 *
 * data.ctx = cg_sdl_device_new (SDL_USEREVENT, &error);
 * if (!data.ctx)
 *   {
 *     fprintf (stderr, "Failed to create context: %s\n",
 *              error->message);
 *     return 1;
 *   }
 *
 * my_application_setup (&data);
 *
 * data.redraw_queued = true;
 * while (!data.quit)
 *   {
 *     while (!data.quit)
 *       {
 *         if (!SDL_PollEvent (&event))
 *           {
 *             if (data.redraw_queued)
 *               break;
 *
 *             cg_sdl_idle (ctx);
 *             if (!SDL_WaitEvent (&event))
 *               {
 *                 fprintf (stderr, "Error waiting for SDL events");
 *                 return 1;
 *               }
 *           }
 *
 *          handle_event (&data, &event);
 *          cg_sdl_handle_event (ctx, &event);
 *        }
 *
 *     data.redraw_queued = redraw (&data);
 *   }
 * ]|
 */

/**
 * cg_sdl_device_new:
 * @type: An SDL user event type between <constant>SDL_USEREVENT</constant> and
 *        <constant>SDL_NUMEVENTS</constant> - 1
 * @error: A cg_error_t return location.
 *
 * This is a convenience function for creating a new #cg_device_t for
 * use with SDL and specifying what SDL user event type CGlib can use
 * as a way to interrupt SDL_WaitEvent().
 *
 * This function is equivalent to the following code:
 * |[
 * cg_renderer_t *renderer = cg_renderer_new ();
 * cg_device_t *dev;
 *
 * cg_renderer_set_winsys_id (renderer, CG_WINSYS_ID_SDL);
 *
 * cg_sdl_renderer_set_event_type (renderer, type);
 *
 * if (!cg_renderer_connect (renderer, error))
 *   return NULL;
 *
 * dev = cg_device_new ();
 * cg_device_set_renderer(dev, renderer);
 *
 * return dev;
 * ]|
 *
 * <note>SDL applications are required to either use this API or
 * to manually create a #cg_renderer_t and call
 * cg_sdl_renderer_set_event_type().</note>
 *
 * Stability: unstable
 */
cg_device_t *cg_sdl_device_new(int type, cg_error_t **error);

/**
 * cg_sdl_renderer_set_event_type:
 * @renderer: A #cg_renderer_t
 * @type: An SDL user event type between <constant>SDL_USEREVENT</constant> and
 *        <constant>SDL_NUMEVENTS</constant> - 1
 *
 * Tells CGlib what SDL user event type it can use as a way to
 * interrupt SDL_WaitEvent() to ensure that cg_sdl_handle_event()
 * will be called in a finite amount of time.
 *
 * <note>This should only be called on an un-connected
 * @renderer.</note>
 *
 * <note>For convenience most simple applications can use
 * cg_sdl_device_new() if they don't want to manually create
 * #cg_renderer_t and #cg_display_t objects during
 * initialization.</note>
 *
 * By default CGlib will assume it can use %SDL_USEREVENT
 *
 * Stability: unstable
 */
void cg_sdl_renderer_set_event_type(cg_renderer_t *renderer, int type);

/**
 * cg_sdl_renderer_get_event_type:
 * @renderer: A #cg_renderer_t
 *
 * Queries what SDL user event type CGlib is using as a way to
 * interrupt SDL_WaitEvent(). This is set either using
 * cg_sdl_device_new or by using
 * cg_sdl_renderer_set_event_type().
 *
 * By default CGlib will assume it can use %SDL_USEREVENT
 *
 * Stability: unstable
 */
int cg_sdl_renderer_get_event_type(cg_renderer_t *renderer);

/**
 * cg_sdl_handle_event:
 * @dev: A #cg_device_t
 * @event: An SDL event
 *
 * Passes control to CGlib so that it may dispatch any internal event
 * callbacks in response to the given SDL @event. This function must
 * be called for every SDL event.
 *
 * Stability: unstable
 */
void cg_sdl_handle_event(cg_device_t *dev, SDL_Event *event);

/**
 * cg_sdl_idle:
 * @dev: A #cg_device_t
 *
 * Notifies CGlib that the application is idle and about to call
 * SDL_WaitEvent(). CGlib may use this to run low priority book keeping
 * tasks.
 *
 * Stability: unstable
 */
void cg_sdl_idle(cg_device_t *dev);

#if SDL_MAJOR_VERSION >= 2

/**
 * cg_sdl_onscreen_get_window:
 * @onscreen: A #cg_onscreen_t
 *
 * Returns: the underlying SDL_Window associated with an onscreen framebuffer.
 *
 * Stability: unstable
 */
SDL_Window *cg_sdl_onscreen_get_window(cg_onscreen_t *onscreen);

#endif /* SDL_MAJOR_VERSION */

CG_END_DECLS

#ifndef CG_COMPILATION
#undef __CG_H_INSIDE__
#endif

#endif /* __CG_SDL_H__ */
