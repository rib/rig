/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011,2013 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-winsys-egl-x11-private.h"
#include "cg-winsys-egl-private.h"
#include "cg-xlib-renderer-private.h"
#include "cg-xlib-renderer.h"
#include "cg-framebuffer-private.h"
#include "cg-onscreen-private.h"
#include "cg-display-private.h"
#include "cg-renderer-private.h"
#include "cg-frame-info-private.h"

#include "cg-texture-pixmap-x11-private.h"
#include "cg-texture-2d-private.h"
#include "cg-error-private.h"
#include "cg-loop-private.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/sync.h>

#define CG_ONSCREEN_X11_EVENT_MASK (StructureNotifyMask | ExposureMask)

static const cg_winsys_egl_vtable_t _cg_winsys_egl_vtable;

typedef struct _cg_display_xlib_t {
    Window dummy_xwin;
} cg_display_xlib_t;

typedef struct _cg_onscreen_xlib_t {
    Window xwin;
    int x, y;
    bool is_foreign_xwin;
    cg_output_t *output;

    XID frame_sync_xcounters[2];
    int64_t frame_sync_counters[2];

    /* If != 0 then we've received a _NET_WM_FRAME_SYNC message
     * setting a new frame sync counter value (extended sync mode) */
    int64_t last_sync_request_value;

} cg_onscreen_xlib_t;

#ifdef EGL_KHR_image_pixmap
typedef struct _cg_texture_pixmap_egl_t {
    EGLImageKHR image;
    cg_texture_t *texture;
} cg_texture_pixmap_egl_t;
#endif

static cg_onscreen_t *
find_onscreen_for_xid(cg_device_t *dev, uint32_t xid)
{
    c_llist_t *l;

    for (l = dev->framebuffers; l; l = l->next) {
        cg_framebuffer_t *framebuffer = l->data;
        cg_onscreen_egl_t *egl_onscreen;
        cg_onscreen_xlib_t *xlib_onscreen;

        if (!framebuffer->type == CG_FRAMEBUFFER_TYPE_ONSCREEN)
            continue;

        egl_onscreen = CG_ONSCREEN(framebuffer)->winsys;
        xlib_onscreen = egl_onscreen->platform;
        if (xlib_onscreen->xwin == (Window)xid)
            return CG_ONSCREEN(framebuffer);
    }

    return NULL;
}

static void
update_output(cg_onscreen_t *onscreen)
{
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = egl_onscreen->platform;
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_output_t *output;
    int width, height;

    width = cg_framebuffer_get_width(framebuffer);
    height = cg_framebuffer_get_height(framebuffer);
    output = _cg_xlib_renderer_output_for_rectangle(display->renderer,
                                                    xlib_onscreen->x,
                                                    xlib_onscreen->y,
                                                    width,
                                                    height);
    if (xlib_onscreen->output != output) {
        if (xlib_onscreen->output)
            cg_object_unref(xlib_onscreen->output);

        xlib_onscreen->output = output;

        if (output)
            cg_object_ref(xlib_onscreen->output);
    }
}

static void
handle_configure_notify(cg_onscreen_t *onscreen,
                        XConfigureEvent *configure_event)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = egl_onscreen->platform;

    _cg_framebuffer_winsys_update_size(framebuffer,
                                       configure_event->width,
                                       configure_event->height);

    if (!xlib_onscreen->is_foreign_xwin) {
        int x, y;

        if (configure_event->send_event) {
            x = configure_event->x;
            y = configure_event->y;
        } else {
            Window child;
            XTranslateCoordinates(configure_event->display,
                                  configure_event->window,
                                  DefaultRootWindow(configure_event->display),
                                  0,
                                  0,
                                  &x,
                                  &y,
                                  &child);
        }

        xlib_onscreen->x = x;
        xlib_onscreen->y = y;

        update_output(onscreen);
    }
    
    _cg_onscreen_queue_resize_notify(onscreen);
}

static cg_filter_return_t
event_filter_cb(XEvent *xevent, void *data)
{
    cg_device_t *dev = data;
    cg_renderer_t *renderer = dev->display->renderer;

    if (xevent->type == ConfigureNotify) {
        XConfigureEvent *xconfig = &xevent->xconfigure;
        cg_onscreen_t *onscreen = find_onscreen_for_xid(dev, xconfig->window);

        if (!onscreen) {
            c_warning("Ignoring spurious ConfigureNotify that couldn't be mapped to an CGLib onscreen window");
            return CG_FILTER_CONTINUE;
        }

        handle_configure_notify(onscreen, xconfig);

        return CG_FILTER_CONTINUE;
    }
    
    if (xevent->type == ClientMessage) {
        XClientMessageEvent *msg = &xevent->xclient;
        cg_onscreen_t *onscreen =
            find_onscreen_for_xid(dev, msg->window);
        cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
        cg_onscreen_xlib_t *xlib_onscreen = egl_onscreen->platform;
        cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
        cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

        if (!onscreen) {
            c_warning("Ignoring spurious client message that couldn't be mapped to an CGLib onscreen window");
            return CG_FILTER_CONTINUE;
        }

        if (msg->message_type == XInternAtom(xlib_renderer->xdpy, "WM_PROTOCOLS", False)) {
            Atom protocol = msg->data.l[0];

            if (protocol == XInternAtom(xlib_renderer->xdpy, "WM_DELETE_WINDOW", False)) {

                /* FIXME: we should eventually support multiple windows and
                 * we should be able close windows individually. */
                c_warning("TODO: add cg_onscreen_add_close_callback() api");
            } else if (protocol == XInternAtom(xlib_renderer->xdpy, "WM_TAKE_FOCUS", False)) {
                XSetInputFocus(xlib_renderer->xdpy,
                               msg->window,
                               RevertToParent,
                               CurrentTime);
            } else if (protocol == XInternAtom(xlib_renderer->xdpy, "_NET_WM_PING", False)) {
                msg->window = DefaultRootWindow(xlib_renderer->xdpy);
                XSendEvent(xlib_renderer->xdpy, DefaultRootWindow(xlib_renderer->xdpy), False,
                           SubstructureRedirectMask | SubstructureNotifyMask, xevent);
            } else {
                char *name = XGetAtomName(xlib_renderer->xdpy, protocol);
                c_warning("Unknown X client WM_PROTOCOLS message recieved (%s)\n",
                          name);
                XFree(name);
            }

            return CG_FILTER_REMOVE;
        }

        if (!x11_renderer->net_wm_frame_drawn_supported)
            return CG_FILTER_CONTINUE;

        if (msg->message_type == XInternAtom(xlib_renderer->xdpy,
                                             "_NET_WM_FRAME_DRAWN", False)) {
            int64_t sync_counter = msg->data.l[0] | ((int64_t)msg->data.l[1] << 32);

            if (sync_counter) { /* Ignore message for initial window Map */
                cg_frame_info_t *info =
                    c_queue_peek_head(&onscreen->pending_frame_infos);

                c_warning("CG: _NET_WM_FRAME_DRAWN");

                if (info->x11_frame_sync_counter != sync_counter)
                    c_warning("INFO=%p frame_sync_counter %"PRIu64" != expected %"PRIu64,
                              info, sync_counter, info->x11_frame_sync_counter);

                c_warn_if_fail(info->composite_end_time == 0);

                info->composite_end_time =
                    msg->data.l[2] | ((int64_t)msg->data.l[3] << 32);
                info->composite_end_time *= 1000; /* micro to nanoseconds */

                _cg_onscreen_queue_event(onscreen, CG_FRAME_EVENT_SYNC, info);
            }

            return CG_FILTER_REMOVE;
        } else if (msg->message_type == XInternAtom(xlib_renderer->xdpy,
                                                    "_NET_WM_FRAME_TIMINGS", False)) {
            int64_t sync_counter = msg->data.l[0] | ((int64_t)msg->data.l[1] << 32);
            int64_t presentation_time;

            if (sync_counter) { /* Ignore message for initial window Map */
                cg_frame_info_t *info =
                    c_queue_pop_head(&onscreen->pending_frame_infos);
                int32_t refresh_interval = 0;

                c_warning("CG: _NET_WM_FRAME_TIMINGS");

                if (info->x11_frame_sync_counter != sync_counter)
                    c_warning("INFO=%p frame_sync_counter %"PRIu64" != expected %"PRIu64,
                              info, sync_counter, info->x11_frame_sync_counter);
                //c_warn_if_fail(info->x11_frame_sync_counter != sync_counter);
                c_warn_if_fail(info->composite_end_time != 0);

                presentation_time =
                    info->composite_end_time + (msg->data.l[2] * 1000);

                /* If this is the first time we've seen a presentation
                 * timestamp then determine if it comes from the same
                 * clock as c_get_monotonic_time() */
                if (!dev->presentation_time_seen) {
                    int64_t now = c_get_monotonic_time();

                    /* If it's less than a second old with respect to
                     * c_get_monotonic_time() that's close enough to
                     * assume it's from the same clock.
                     */
                    if (info->presentation_time < now &&
                        info->presentation_time >= (now - 1000000000))
                    {
                        dev->presentation_clock_is_monotonic = true;
                    }
                    dev->presentation_time_seen = true;
                }

                if (dev->presentation_clock_is_monotonic)
                    info->presentation_time = presentation_time;

                refresh_interval = msg->data.l[3];
                /* NB: interval is in microseconds... */
                info->refresh_rate = 1000000.0f/(float)refresh_interval;

                if (!(msg->data.l[4] & 0x80000000))
                    info->next_refresh_deadline =
                        info->composite_end_time + (msg->data.l[4] * 1000);

                _cg_onscreen_queue_event(onscreen, CG_FRAME_EVENT_COMPLETE, info);

                cg_object_unref(info);
            }

            return CG_FILTER_REMOVE;
        } else if (msg->message_type  == XInternAtom(xlib_renderer->xdpy,
                                                     "_NET_WM_SYNC_REQUEST", False)) {

            /* only care about extended sync support */
            if (x11_renderer->net_wm_frame_drawn_supported &&
                msg->data.l[4] == 0)
            {
                int64_t value = msg->data.l[2] | ((int64_t)msg->data.l[3] << 32);

                if (value % 2 != 0) {
                    /* An even number would be odd here :-D .. badum *tish*! */
                    c_warning("CG: Spurious odd _NET_WM_SYNC_REQUEST value");
                    value++;
                }

                xlib_onscreen->last_sync_request_value = value;
            }

            return CG_FILTER_REMOVE;
        }

        return CG_FILTER_CONTINUE;
    }


    if (xevent->type == Expose) {
        cg_onscreen_t *onscreen =
            find_onscreen_for_xid(dev, xevent->xexpose.window);

        if (onscreen) {
            cg_onscreen_dirty_info_t info;

            info.x = xevent->xexpose.x;
            info.y = xevent->xexpose.y;
            info.width = xevent->xexpose.width;
            info.height = xevent->xexpose.height;

            _cg_onscreen_queue_dirty(onscreen, &info);
        }
    }

    return CG_FILTER_CONTINUE;
}

static XVisualInfo *
get_visual_info(cg_display_t *display,
                EGLConfig egl_config,
                cg_error_t **error)
{
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(display->renderer);
    cg_renderer_egl_t *egl_renderer = display->renderer->winsys;
    XVisualInfo visinfo_template;
    int template_mask = 0;
    XVisualInfo *visinfo = NULL;
    int visinfos_count;
    EGLint visualid, red_size, green_size, blue_size, alpha_size;

    eglGetConfigAttrib(
        egl_renderer->edpy, egl_config, EGL_NATIVE_VISUAL_ID, &visualid);

    if (visualid != 0) {
        visinfo_template.visualid = visualid;
        template_mask |= VisualIDMask;
    } else {
        /* some EGL drivers don't implement the EGL_NATIVE_VISUAL_ID
         * attribute, so attempt to find the closest match. */

        eglGetConfigAttrib(
            egl_renderer->edpy, egl_config, EGL_RED_SIZE, &red_size);
        eglGetConfigAttrib(
            egl_renderer->edpy, egl_config, EGL_GREEN_SIZE, &green_size);
        eglGetConfigAttrib(
            egl_renderer->edpy, egl_config, EGL_BLUE_SIZE, &blue_size);
        eglGetConfigAttrib(
            egl_renderer->edpy, egl_config, EGL_ALPHA_SIZE, &alpha_size);

        visinfo_template.depth = red_size + green_size + blue_size + alpha_size;
        template_mask |= VisualDepthMask;

        visinfo_template.screen = DefaultScreen(xlib_renderer->xdpy);
        template_mask |= VisualScreenMask;
    }

    visinfo = XGetVisualInfo(
        xlib_renderer->xdpy, template_mask, &visinfo_template, &visinfos_count);

    if (visinfo == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "Unable to retrieve the X11 visual of egl config");
    }

    return visinfo;
}

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    cg_renderer_egl_t *egl_renderer = renderer->winsys;

    _cg_xlib_renderer_disconnect(renderer);

    eglTerminate(egl_renderer->edpy);

    c_slice_free(cg_renderer_egl_t, egl_renderer);
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
    cg_renderer_egl_t *egl_renderer;
    cg_xlib_renderer_t *xlib_renderer;

    renderer->winsys = c_slice_new0(cg_renderer_egl_t);
    egl_renderer = renderer->winsys;
    xlib_renderer = _cg_xlib_renderer_get_data(renderer);

    egl_renderer->platform_vtable = &_cg_winsys_egl_vtable;

    if (!_cg_xlib_renderer_connect(renderer, error))
        goto error;

    egl_renderer->edpy = eglGetDisplay((NativeDisplayType)xlib_renderer->xdpy);

    if (!_cg_winsys_egl_renderer_connect_common(renderer, error))
        goto error;

    return true;

error:
    _cg_winsys_renderer_disconnect(renderer);
    return false;
}

static bool
_cg_winsys_egl_display_setup(cg_display_t *display,
                             cg_error_t **error)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_xlib_t *xlib_display;

    xlib_display = c_slice_new0(cg_display_xlib_t);
    egl_display->platform = xlib_display;

    return true;
}

static void
_cg_winsys_egl_display_destroy(cg_display_t *display)
{
    cg_display_egl_t *egl_display = display->winsys;

    c_slice_free(cg_display_xlib_t, egl_display->platform);
}

static bool
_cg_winsys_egl_device_init(cg_device_t *dev,
                            cg_error_t **error)
{
    cg_renderer_t *renderer = dev->renderer;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

    cg_xlib_renderer_add_filter(dev->display->renderer, event_filter_cb, dev);

    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_ONSCREEN_MULTIPLE, true);
    CG_FLAGS_SET(dev->winsys_features, CG_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                 true);

    /* We'll manually handle queueing dirty events in response to
     * Expose events from X */
    CG_FLAGS_SET(dev->private_features, CG_PRIVATE_FEATURE_DIRTY_EVENTS,
                 true);

    if (x11_renderer->net_wm_frame_drawn_supported) {
        CG_FLAGS_SET(dev->winsys_features,
                     CG_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
                     true);
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_PRESENTATION_TIME, true);
    }

    return true;
}

static void
_cg_winsys_egl_device_deinit(cg_device_t *dev)
{
    cg_xlib_renderer_remove_filter(dev->display->renderer, event_filter_cb,
                                   dev);
}

static bool
_cg_winsys_egl_onscreen_init(cg_onscreen_t *onscreen,
                             EGLConfig egl_config,
                             cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_renderer_t *renderer = display->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_onscreen_xlib_t *xlib_onscreen;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    Window xwin;

    /* FIXME: We need to explicitly Select for ConfigureNotify events.
     * For foreign windows we need to be careful not to mess up any
     * existing event mask.
     * We need to document that for windows we create then toolkits
     * must be careful not to clear event mask bits that we select.
     */

    /* XXX: Note we ignore the user's original width/height when
     * given a foreign X window. */
    if (onscreen->foreign_xid) {
        Status status;
        cg_xlib_trap_state_t state;
        XWindowAttributes attr;
        int xerror;

        xwin = onscreen->foreign_xid;

        _cg_xlib_renderer_trap_errors(display->renderer, &state);

        status = XGetWindowAttributes(xlib_renderer->xdpy, xwin, &attr);
        xerror = _cg_xlib_renderer_untrap_errors(display->renderer, &state);
        if (status == 0 || xerror) {
            char message[1000];
            XGetErrorText(
                xlib_renderer->xdpy, xerror, message, sizeof(message));
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_ONSCREEN,
                          "Unable to query geometry of foreign "
                          "xid 0x%08lX: %s",
                          xwin,
                          message);
            return false;
        }

        _cg_framebuffer_winsys_update_size(
            framebuffer, attr.width, attr.height);

        /* Make sure the app selects for the events we require... */
        onscreen->foreign_update_mask_callback(
            onscreen,
            CG_ONSCREEN_X11_EVENT_MASK,
            onscreen->foreign_update_mask_data);
    } else {
        int width;
        int height;
        cg_xlib_trap_state_t state;
        XVisualInfo *xvisinfo;
        XSetWindowAttributes xattr;
        unsigned long mask;
        int xerror;

        width = cg_framebuffer_get_width(framebuffer);
        height = cg_framebuffer_get_height(framebuffer);

        _cg_xlib_renderer_trap_errors(display->renderer, &state);

        xvisinfo = get_visual_info(display, egl_config, error);
        if (!xvisinfo)
            return false;

        /* window attributes */
        xattr.background_pixel =
            WhitePixel(xlib_renderer->xdpy, DefaultScreen(xlib_renderer->xdpy));
        xattr.border_pixel = 0;
        /* XXX: is this an X resource that we are leaking‽... */
        xattr.colormap = XCreateColormap(xlib_renderer->xdpy,
                                         DefaultRootWindow(xlib_renderer->xdpy),
                                         xvisinfo->visual,
                                         AllocNone);
        xattr.event_mask = CG_ONSCREEN_X11_EVENT_MASK;

        mask = CWBorderPixel | CWColormap | CWEventMask;

        xwin = XCreateWindow(xlib_renderer->xdpy,
                             DefaultRootWindow(xlib_renderer->xdpy),
                             0,
                             0,
                             width,
                             height,
                             0,
                             xvisinfo->depth,
                             InputOutput,
                             xvisinfo->visual,
                             mask,
                             &xattr);

        XFree(xvisinfo);

        XSync(xlib_renderer->xdpy, False);
        xerror = _cg_xlib_renderer_untrap_errors(display->renderer, &state);
        if (xerror) {
            char message[1000];
            XGetErrorText(
                xlib_renderer->xdpy, xerror, message, sizeof(message));
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_ONSCREEN,
                          "X error while creating Window for cg_onscreen_t: %s",
                          message);
            return false;
        }
    }

    xlib_onscreen = c_slice_new(cg_onscreen_xlib_t);
    egl_onscreen->platform = xlib_onscreen;

    xlib_onscreen->xwin = xwin;
    xlib_onscreen->is_foreign_xwin = onscreen->foreign_xid ? true : false;

    egl_onscreen->egl_surface =
        eglCreateWindowSurface(egl_renderer->edpy,
                               egl_config,
                               (NativeWindowType)xlib_onscreen->xwin,
                               NULL);

    return true;
}

static void
_cg_winsys_egl_onscreen_deinit(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_xlib_trap_state_t old_state;
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = egl_onscreen->platform;

    _cg_xlib_renderer_trap_errors(renderer, &old_state);

    if (!xlib_onscreen->is_foreign_xwin && xlib_onscreen->xwin != None) {
        XDestroyWindow(xlib_renderer->xdpy, xlib_onscreen->xwin);
        xlib_onscreen->xwin = None;
    } else
        xlib_onscreen->xwin = None;

    XSync(xlib_renderer->xdpy, False);

    if (_cg_xlib_renderer_untrap_errors(renderer, &old_state) != Success)
        c_warning("X Error while destroying X window");

    c_slice_free(cg_onscreen_xlib_t, xlib_onscreen);
}

static void
_cg_winsys_onscreen_set_visibility(cg_onscreen_t *onscreen,
                                   bool visibility)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_onscreen_egl_t *onscreen_egl = onscreen->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = onscreen_egl->platform;

    if (visibility)
        XMapWindow(xlib_renderer->xdpy, xlib_onscreen->xwin);
    else
        XUnmapWindow(xlib_renderer->xdpy, xlib_onscreen->xwin);
}

static void
_cg_winsys_onscreen_set_resizable(cg_onscreen_t *onscreen,
                                  bool resizable)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = egl_onscreen->platform;

    XSizeHints *size_hints = XAllocSizeHints();

    if (resizable) {
        /* TODO: Add cg_onscreen_request_minimum_size () */
        size_hints->min_width = 1;
        size_hints->min_height = 1;

        size_hints->max_width = INT_MAX;
        size_hints->max_height = INT_MAX;
    } else {
        int width = cg_framebuffer_get_width(framebuffer);
        int height = cg_framebuffer_get_height(framebuffer);

        size_hints->min_width = width;
        size_hints->min_height = height;

        size_hints->max_width = width;
        size_hints->max_height = height;
    }

    XSetWMNormalHints(xlib_renderer->xdpy, xlib_onscreen->xwin, size_hints);

    XFree(size_hints);
}

static uint32_t
_cg_winsys_onscreen_x11_get_window_xid(cg_onscreen_t *onscreen)
{
    cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = egl_onscreen->platform;

    return xlib_onscreen->xwin;
}

static void
_cg_winsys_egl_swap_interval(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *fb = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = fb->dev;
    cg_renderer_t *renderer = dev->renderer;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

    /* NB: If _NET_WM_FRAME_DRAWN is available then we are running
     * under a compositor and throttling is handled via X client
     * messages instead */
    if (fb->config.swap_throttled &&
        !x11_renderer->net_wm_frame_drawn_supported)
        eglSwapInterval(egl_renderer->edpy, 1);
    else
        eglSwapInterval(egl_renderer->edpy, 0);
}

static void
xsync_int64_to_value_counter(XSyncValue *xvalue,
                             int64_t value)
{
    unsigned int low = value & 0xffffffff;
    int high = value >> 32;

    XSyncIntsToValue(xvalue, low, high);
}

static void
_cg_winsys_egl_start_swap(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

    if (x11_renderer->net_wm_frame_drawn_supported) {
        cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
        cg_onscreen_xlib_t *xlib_onscreen = egl_onscreen->platform;
        XSyncValue xvalue;
        int64_t frame_sync_counter;

        /* Check incase the WM explicitly gave us a new
         * frame sync counter value via a _NET_WM_SYNC_REQUEST
         * message... */
        if (xlib_onscreen->last_sync_request_value) {
            xlib_onscreen->frame_sync_counters[1] = xlib_onscreen->last_sync_request_value;
            xlib_onscreen->last_sync_request_value = 0;
        }

        c_warn_if_fail(xlib_onscreen->frame_sync_counters[1] % 2 == 0);


        frame_sync_counter = ++xlib_onscreen->frame_sync_counters[1];
        c_debug("CG: update frame sync counter to %"PRIi64" (START FRAME)",
                frame_sync_counter);

        xsync_int64_to_value_counter(&xvalue, frame_sync_counter);
        XSyncSetCounter(xlib_renderer->xdpy,
                        xlib_onscreen->frame_sync_xcounters[1],
                        xvalue);
    }
}

static void
_cg_winsys_egl_end_swap(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

    if (x11_renderer->net_wm_frame_drawn_supported) {
        cg_onscreen_egl_t *egl_onscreen = onscreen->winsys;
        cg_onscreen_xlib_t *xlib_onscreen = egl_onscreen->platform;
        cg_frame_info_t *info = c_queue_peek_tail(&onscreen->pending_frame_infos);
        XSyncValue xvalue;

        c_warn_if_fail(xlib_onscreen->frame_sync_counters[1] % 2 == 1);

        info->x11_frame_sync_counter = ++xlib_onscreen->frame_sync_counters[1];
        c_debug("CG: INFO=%p update frame sync counter to %"PRIi64" (END FRAME)",
                info, info->x11_frame_sync_counter);

        xsync_int64_to_value_counter(&xvalue, info->x11_frame_sync_counter);
        XSyncSetCounter(xlib_renderer->xdpy,
                        xlib_onscreen->frame_sync_xcounters[1],
                        xvalue);
    }

}

static bool
_cg_winsys_egl_device_created(cg_display_t *display,
                               cg_error_t **error)
{
    cg_renderer_t *renderer = display->renderer;
    cg_display_egl_t *egl_display = display->winsys;
    cg_renderer_egl_t *egl_renderer = renderer->winsys;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_display_xlib_t *xlib_display = egl_display->platform;
    XVisualInfo *xvisinfo;
    XSetWindowAttributes attrs;
    const char *error_message;

    xvisinfo = get_visual_info(display, egl_display->egl_config, error);
    if (xvisinfo == NULL)
        return false;

    attrs.override_redirect = True;
    attrs.colormap = XCreateColormap(xlib_renderer->xdpy,
                                     DefaultRootWindow(xlib_renderer->xdpy),
                                     xvisinfo->visual,
                                     AllocNone);
    attrs.border_pixel = 0;

    if ((egl_renderer->private_features &
         CG_EGL_WINSYS_FEATURE_SURFACELESS_CONTEXT) == 0) {
        xlib_display->dummy_xwin =
            XCreateWindow(xlib_renderer->xdpy,
                          DefaultRootWindow(xlib_renderer->xdpy),
                          -100,
                          -100,
                          1,
                          1,
                          0,
                          xvisinfo->depth,
                          CopyFromParent,
                          xvisinfo->visual,
                          CWOverrideRedirect | CWColormap | CWBorderPixel,
                          &attrs);

        egl_display->dummy_surface =
            eglCreateWindowSurface(egl_renderer->edpy,
                                   egl_display->egl_config,
                                   (NativeWindowType)xlib_display->dummy_xwin,
                                   NULL);

        if (egl_display->dummy_surface == EGL_NO_SURFACE) {
            error_message = "Unable to create an EGL surface";
            XFree(xvisinfo);
            goto fail;
        }
    }

    XFree(xvisinfo);

    if (!_cg_winsys_egl_make_current(display,
                                     egl_display->dummy_surface,
                                     egl_display->dummy_surface,
                                     egl_display->egl_context)) {
        if (egl_display->dummy_surface == EGL_NO_SURFACE)
            error_message = "Unable to eglMakeCurrent with no surface";
        else
            error_message = "Unable to eglMakeCurrent with dummy surface";
        goto fail;
    }

    return true;

fail:
    _cg_set_error(error,
                  CG_WINSYS_ERROR,
                  CG_WINSYS_ERROR_CREATE_CONTEXT,
                  "%s",
                  error_message);
    return false;
}

static void
_cg_winsys_egl_cleanup_device(cg_display_t *display)
{
    cg_display_egl_t *egl_display = display->winsys;
    cg_display_xlib_t *xlib_display = egl_display->platform;
    cg_renderer_t *renderer = display->renderer;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_renderer_egl_t *egl_renderer = renderer->winsys;

    if (egl_display->dummy_surface != EGL_NO_SURFACE) {
        eglDestroySurface(egl_renderer->edpy, egl_display->dummy_surface);
        egl_display->dummy_surface = EGL_NO_SURFACE;
    }

    if (xlib_display->dummy_xwin) {
        XDestroyWindow(xlib_renderer->xdpy, xlib_display->dummy_xwin);
        xlib_display->dummy_xwin = None;
    }
}

static XVisualInfo *
_cg_winsys_xlib_get_visual_info(cg_onscreen_t *onscreen,
                                cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    EGLConfig egl_config;

    c_return_val_if_fail(dev->display->winsys, false);

    if (!_cg_egl_find_config(onscreen, &egl_config, error))
        return NULL;

    return get_visual_info(dev->display, egl_config, error);
}

#ifdef EGL_KHR_image_pixmap

static bool
_cg_winsys_texture_pixmap_x11_create(cg_texture_pixmap_x11_t *tex_pixmap)
{
    cg_texture_t *tex = CG_TEXTURE(tex_pixmap);
    cg_device_t *dev = tex->dev;
    cg_texture_pixmap_egl_t *egl_tex_pixmap;
    EGLint attribs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE };
    cg_pixel_format_t texture_format;
    cg_renderer_egl_t *egl_renderer;

    egl_renderer = dev->display->renderer->winsys;

    if (!(egl_renderer->private_features &
          CG_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_X11_PIXMAP) ||
        !_cg_has_private_feature(dev, CG_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE)) {
        tex_pixmap->winsys = NULL;
        return false;
    }

    egl_tex_pixmap = c_new0(cg_texture_pixmap_egl_t, 1);

    egl_tex_pixmap->image =
        _cg_egl_create_image(dev,
                             EGL_NATIVE_PIXMAP_KHR,
                             (EGLClientBuffer)tex_pixmap->pixmap,
                             attribs);
    if (egl_tex_pixmap->image == EGL_NO_IMAGE_KHR) {
        c_free(egl_tex_pixmap);
        return false;
    }

    texture_format = (tex_pixmap->depth >= 32 ? CG_PIXEL_FORMAT_RGBA_8888_PRE
                      : CG_PIXEL_FORMAT_RGB_888);

    egl_tex_pixmap->texture =
        CG_TEXTURE(_cg_egl_texture_2d_new_from_image(dev,
                                                     tex->width,
                                                     tex->height,
                                                     texture_format,
                                                     egl_tex_pixmap->image,
                                                     NULL));

    tex_pixmap->winsys = egl_tex_pixmap;

    return true;
}

static void
_cg_winsys_texture_pixmap_x11_free(cg_texture_pixmap_x11_t *tex_pixmap)
{
    cg_texture_t *tex = CG_TEXTURE(tex_pixmap);
    cg_device_t *dev = tex->dev;
    cg_texture_pixmap_egl_t *egl_tex_pixmap;

    if (!tex_pixmap->winsys)
        return;

    egl_tex_pixmap = tex_pixmap->winsys;

    if (egl_tex_pixmap->texture)
        cg_object_unref(egl_tex_pixmap->texture);

    if (egl_tex_pixmap->image != EGL_NO_IMAGE_KHR)
        _cg_egl_destroy_image(dev, egl_tex_pixmap->image);

    tex_pixmap->winsys = NULL;
    c_free(egl_tex_pixmap);
}

static bool
_cg_winsys_texture_pixmap_x11_update(cg_texture_pixmap_x11_t *tex_pixmap,
                                     bool needs_mipmap)
{
    if (needs_mipmap)
        return false;

    return true;
}

static void
_cg_winsys_texture_pixmap_x11_damage_notify(cg_texture_pixmap_x11_t *tex_pixmap)
{
}

static cg_texture_t *
_cg_winsys_texture_pixmap_x11_get_texture(cg_texture_pixmap_x11_t *tex_pixmap)
{
    cg_texture_pixmap_egl_t *egl_tex_pixmap = tex_pixmap->winsys;

    return egl_tex_pixmap->texture;
}

#endif /* EGL_KHR_image_pixmap */

static const cg_winsys_egl_vtable_t _cg_winsys_egl_vtable = {
    .display_setup = _cg_winsys_egl_display_setup,
    .display_destroy = _cg_winsys_egl_display_destroy,
    .device_created = _cg_winsys_egl_device_created,
    .cleanup_device = _cg_winsys_egl_cleanup_device,
    .device_init = _cg_winsys_egl_device_init,
    .device_deinit = _cg_winsys_egl_device_deinit,
    .onscreen_init = _cg_winsys_egl_onscreen_init,
    .onscreen_deinit = _cg_winsys_egl_onscreen_deinit,
    .swap_interval = _cg_winsys_egl_swap_interval,
    .start_swap = _cg_winsys_egl_start_swap,
    .end_swap = _cg_winsys_egl_end_swap,
};

const cg_winsys_vtable_t *
_cg_winsys_egl_xlib_get_vtable(void)
{
    static bool vtable_inited = false;
    static cg_winsys_vtable_t vtable;

    if (!vtable_inited) {
        /* The EGL_X11 winsys is a subclass of the EGL winsys so we
           start by copying its vtable */

        vtable = *_cg_winsys_egl_get_vtable();

        vtable.id = CG_WINSYS_ID_EGL_XLIB;
        vtable.name = "EGL_XLIB";
        vtable.constraints |= (CG_RENDERER_CONSTRAINT_USES_X11 |
                               CG_RENDERER_CONSTRAINT_USES_XLIB);

        vtable.renderer_connect = _cg_winsys_renderer_connect;
        vtable.renderer_disconnect = _cg_winsys_renderer_disconnect;

        vtable.onscreen_set_visibility = _cg_winsys_onscreen_set_visibility;
        vtable.onscreen_set_resizable = _cg_winsys_onscreen_set_resizable;

        vtable.onscreen_x11_get_window_xid =
            _cg_winsys_onscreen_x11_get_window_xid;

        vtable.xlib_get_visual_info = _cg_winsys_xlib_get_visual_info;

#ifdef EGL_KHR_image_pixmap
        /* X11 tfp support... */
        /* XXX: instead of having a rather monolithic winsys vtable we could
         * perhaps look for a way to separate these... */
        vtable.texture_pixmap_x11_create = _cg_winsys_texture_pixmap_x11_create;
        vtable.texture_pixmap_x11_free = _cg_winsys_texture_pixmap_x11_free;
        vtable.texture_pixmap_x11_update = _cg_winsys_texture_pixmap_x11_update;
        vtable.texture_pixmap_x11_damage_notify =
            _cg_winsys_texture_pixmap_x11_damage_notify;
        vtable.texture_pixmap_x11_get_texture =
            _cg_winsys_texture_pixmap_x11_get_texture;
#endif /* EGL_KHR_image_pixmap) */

        vtable_inited = true;
    }

    return &vtable;
}
