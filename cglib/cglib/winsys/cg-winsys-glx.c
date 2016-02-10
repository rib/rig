/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010,2011,2013 Intel Corporation.
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
 */

#include <cglib-config.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <GL/glx.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/sync.h>

#include "cg-i18n-private.h"
#include "cg-util.h"
#include "cg-winsys-private.h"
#include "cg-feature-private.h"
#include "cg-device-private.h"
#include "cg-framebuffer.h"
#include "cg-renderer-private.h"
#include "cg-glx-renderer-private.h"
#include "cg-onscreen-template-private.h"
#include "cg-glx-display-private.h"
#include "cg-private.h"
#include "cg-texture-2d-private.h"
#include "cg-pipeline-opengl-private.h"
#include "cg-frame-info-private.h"
#include "cg-framebuffer-private.h"
#include "cg-onscreen-private.h"
#include "cg-xlib-renderer.h"
#include "cg-util.h"
#include "cg-winsys-glx-private.h"
#include "cg-error-private.h"
#include "cg-loop-private.h"
#include "cg-version.h"

#define CG_ONSCREEN_X11_EVENT_MASK (StructureNotifyMask | ExposureMask)
#define MAX_GLX_CONFIG_ATTRIBS 30

typedef struct _cg_device_glx_t {
    GLXDrawable current_drawable;
} cg_device_glx_t;

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

typedef struct _cg_onscreen_glx_t {
    cg_onscreen_xlib_t _parent;
    GLXDrawable glxwin;
    uint32_t last_swap_vsync_counter;
} cg_onscreen_glx_t;

typedef struct _cg_texture_pixmap_glx_t {
    GLXPixmap glx_pixmap;
    bool has_mipmap_space;
    bool can_mipmap;

    cg_texture_t *glx_tex;

    bool bind_tex_image_queued;
    bool pixmap_bound;
} cg_texture_pixmap_glx_t;

/* Define a set of arrays containing the functions required from GL
   for each winsys feature */
#define CG_WINSYS_FEATURE_BEGIN(major_version,                                 \
                                minor_version,                                 \
                                name,                                          \
                                namespaces,                                    \
                                extension_names,                               \
                                winsys_feature)                                \
    static const cg_feature_function_t cg_glx_feature_##name##_funcs[] = {
#define CG_WINSYS_FEATURE_FUNCTION(ret, name, args)                            \
    {                                                                          \
        C_STRINGIFY(name), C_STRUCT_OFFSET(cg_glx_renderer_t, name)            \
    }                                                                          \
    ,
#define CG_WINSYS_FEATURE_END()                                                \
    {                                                                          \
        NULL, 0                                                                \
    }                                                                          \
    ,                                                                          \
    }                                                                          \
    ;
#include "cg-winsys-glx-feature-functions.h"

/* Define an array of features */
#undef CG_WINSYS_FEATURE_BEGIN
#define CG_WINSYS_FEATURE_BEGIN(major_version,                                 \
                                minor_version,                                 \
                                name,                                          \
                                namespaces,                                    \
                                extension_names,                               \
                                winsys_feature)                                \
    {                                                                          \
        major_version, minor_version, 0, namespaces, extension_names, 0,       \
        winsys_feature, cg_glx_feature_##name##_funcs                      \
    }                                                                          \
    ,
#undef CG_WINSYS_FEATURE_FUNCTION
#define CG_WINSYS_FEATURE_FUNCTION(ret, name, args)
#undef CG_WINSYS_FEATURE_END
#define CG_WINSYS_FEATURE_END()

static const cg_feature_data_t winsys_feature_data[] = {
#include "cg-winsys-glx-feature-functions.h"
};

static cg_func_ptr_t
_cg_winsys_renderer_get_proc_address(
    cg_renderer_t *renderer, const char *name, bool in_core)
{
    cg_glx_renderer_t *glx_renderer = renderer->winsys;

    /* The GLX_ARB_get_proc_address extension documents that this should
     * work for core functions too so we don't need to do anything
     * special with in_core */

    return glx_renderer->glXGetProcAddress((const GLubyte *)name);
}

static cg_onscreen_t *
find_onscreen_for_xid(cg_device_t *dev, uint32_t xid)
{
    c_llist_t *l;

    for (l = dev->framebuffers; l; l = l->next) {
        cg_framebuffer_t *framebuffer = l->data;
        cg_onscreen_xlib_t *xlib_onscreen;

        if (framebuffer->type != CG_FRAMEBUFFER_TYPE_ONSCREEN)
            continue;

        /* Does the GLXEvent have the GLXDrawable or the X Window? */
        xlib_onscreen = CG_ONSCREEN(framebuffer)->winsys;
        if (xlib_onscreen->xwin == (Window)xid)
            return CG_ONSCREEN(framebuffer);
    }

    return NULL;
}

static void
check_ust_is_monotonic(cg_device_t *dev, GLXDrawable drawable)
{
    cg_renderer_t *renderer = dev->renderer;
    cg_glx_renderer_t *glx_renderer = renderer->winsys;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    int64_t ust;
    int64_t msc;
    int64_t sbc;
    int64_t now_nanoseconds;
    int64_t now_microseconds;

    dev->presentation_time_seen = true;

    if (glx_renderer->glXGetSyncValues == NULL)
        return;

    if (!glx_renderer->glXGetSyncValues(xlib_renderer->xdpy,
                                        drawable, &ust, &msc, &sbc))
        return;

    now_nanoseconds = c_get_monotonic_time();
    now_microseconds = now_nanoseconds / 1000;

    if (ust < now_nanoseconds && ust > (now_nanoseconds - 1000000000)) {
        glx_renderer->ust_scale = 1;
        dev->presentation_time_seen = true;
        dev->presentation_clock_is_monotonic = true;
    } else if (ust < now_microseconds && ust > (now_microseconds - 1000000)) {
        glx_renderer->ust_scale = 1000;
        dev->presentation_clock_is_monotonic = true;
    }
}

static int64_t
ust_to_nanoseconds(cg_device_t *dev, int64_t ust)
{
    cg_renderer_t *renderer = dev->renderer;
    cg_glx_renderer_t *glx_renderer = renderer->winsys;

    c_warn_if_fail(dev->presentation_clock_is_monotonic);

    return ust * glx_renderer->ust_scale;
}

static void
notify_swap_buffers(cg_device_t *dev,
                    GLXBufferSwapComplete *swap_event)
{
    cg_onscreen_t *onscreen =
        find_onscreen_for_xid(dev, (uint32_t)swap_event->drawable);
    cg_onscreen_glx_t *glx_onscreen;
    cg_frame_info_t *info;

    if (!onscreen)
        return;

    glx_onscreen = onscreen->winsys;

    info = c_queue_pop_head(&onscreen->pending_frame_infos);
    c_warning("CG: pop tail frame info = %p (notify_swap_buffers)", info);

    if (swap_event->ust != 0) {
        if (!dev->presentation_time_seen)
            check_ust_is_monotonic(dev, glx_onscreen->glxwin);

        if (dev->presentation_clock_is_monotonic)
            info->presentation_time = ust_to_nanoseconds(dev, swap_event->ust);
    }

    _cg_onscreen_queue_event(onscreen, CG_FRAME_EVENT_SYNC, info);
    _cg_onscreen_queue_event(onscreen, CG_FRAME_EVENT_COMPLETE, info);

    cg_object_unref(info);
}

static void
update_output(cg_onscreen_t *onscreen)
{
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_output_t *output;
    int width, height;

    width = cg_framebuffer_get_width(framebuffer);
    height = cg_framebuffer_get_height(framebuffer);
    output = _cg_xlib_renderer_output_for_rectangle(
        display->renderer, xlib_onscreen->x, xlib_onscreen->y, width, height);
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
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;

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
glx_event_filter_cb(XEvent *xevent, void *data)
{
    cg_device_t *dev = data;
    cg_renderer_t *renderer = dev->display->renderer;
    cg_glx_renderer_t *glx_renderer = renderer->winsys;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

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
        cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
        //cg_onscreen_glx_t *glx_onscreen = onscreen->winsys;

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

                c_warning("CG: _NET_WM_FRAME_DRAWN frame sync = %"PRIi64, sync_counter);

                if (info->x11_frame_sync_counter != sync_counter)
                    c_warning("_NET_WM_FRAME_DRAWN: INFO=%p frame_sync_counter %"PRIu64" != expected %"PRIu64,
                              info, sync_counter, info->x11_frame_sync_counter);

                c_warn_if_fail(info->composite_end_time == 0);

                info->composite_end_time =
                    msg->data.l[2] | ((int64_t)msg->data.l[3] << 32);
                info->composite_end_time *= 1000; /* micro to nanoseconds */
                c_warning("%u: _NET_WM_FRAME_DRAWN: composite end time = %"PRIu64,
                          (unsigned)info->frame_counter, info->composite_end_time);

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

                c_warning("CG: pop head frame info = %p (_NET_WM_FRAME_TIMINGS)", info);

                c_warning("CG: _NET_WM_FRAME_TIMINGS frame sync = %"PRIi64, sync_counter);

                if (info->x11_frame_sync_counter != sync_counter)
                    c_warning("_NET_WM_FRAME_TIMINGS: INFO=%p frame_sync_counter %"PRIu64" != expected %"PRIu64,
                              info, sync_counter, info->x11_frame_sync_counter);
                //c_warn_if_fail(info->x11_frame_sync_counter != sync_counter);
                c_warn_if_fail(info->composite_end_time != 0);

                presentation_time =
                    info->composite_end_time + (msg->data.l[2] * 1000);
                c_warning("%u: _NET_WM_FRAME_TIMINGS: presentation time = %"PRIu64,
                          (unsigned)info->frame_counter, presentation_time);

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
                c_warning("_NET_WM_FRAME_TIMINGS: refresh = %f", info->refresh_rate);

                if (!(msg->data.l[4] & 0x80000000))
                    info->next_refresh_deadline =
                        info->composite_end_time + (msg->data.l[4] * 1000);

                c_warning("_NET_WM_FRAME_TIMINGS: next deadline = %"PRIu64,
                          info->next_refresh_deadline);

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

#ifdef GLX_INTEL_swap_event
    /* NB: If _NET_WM_FRAME_DRAWN is available then we are running
     * under a compositor and throttling is handled via X client
     * messages instead */
    if (!x11_renderer->net_wm_frame_drawn_supported &&
        xevent->type == (glx_renderer->glx_event_base + GLX_BufferSwapComplete))
    {
        GLXBufferSwapComplete *swap_event = (GLXBufferSwapComplete *)xevent;

        notify_swap_buffers(dev, swap_event);

        /* remove SwapComplete events from the queue */
        return CG_FILTER_REMOVE;
    }
#endif /* GLX_INTEL_swap_event */

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

        return CG_FILTER_CONTINUE;
    }

    return CG_FILTER_CONTINUE;
}

static void
_cg_winsys_renderer_disconnect(cg_renderer_t *renderer)
{
    cg_glx_renderer_t *glx_renderer = renderer->winsys;

    _cg_xlib_renderer_disconnect(renderer);

    if (glx_renderer->libgl_module)
        c_module_close(glx_renderer->libgl_module);

    c_slice_free(cg_glx_renderer_t, renderer->winsys);
}

static bool
update_all_outputs(cg_renderer_t *renderer)
{
    c_llist_t *l;

    _CG_GET_DEVICE(dev, false);

    if (dev->display == NULL) /* during connection */
        return false;

    if (dev->display->renderer != renderer)
        return false;

    for (l = dev->framebuffers; l; l = l->next) {
        cg_framebuffer_t *framebuffer = l->data;

        if (framebuffer->type != CG_FRAMEBUFFER_TYPE_ONSCREEN)
            continue;

        update_output(CG_ONSCREEN(framebuffer));
    }

    return true;
}

static void
_cg_winsys_renderer_outputs_changed(cg_renderer_t *renderer)
{
    update_all_outputs(renderer);
}

static bool
resolve_core_glx_functions(cg_renderer_t *renderer,
                           cg_error_t **error)
{
    cg_glx_renderer_t *glx_renderer;

    glx_renderer = renderer->winsys;

    if (!c_module_symbol(glx_renderer->libgl_module,
                         "glXQueryExtension",
                         (void **)&glx_renderer->glXQueryExtension) ||
        !c_module_symbol(glx_renderer->libgl_module,
                         "glXQueryVersion",
                         (void **)&glx_renderer->glXQueryVersion) ||
        !c_module_symbol(glx_renderer->libgl_module,
                         "glXQueryExtensionsString",
                         (void **)&glx_renderer->glXQueryExtensionsString) ||
        (!c_module_symbol(glx_renderer->libgl_module,
                          "glXGetProcAddress",
                          (void **)&glx_renderer->glXGetProcAddress) &&
         !c_module_symbol(glx_renderer->libgl_module,
                          "glXGetProcAddressARB",
                          (void **)&glx_renderer->glXGetProcAddress)) ||
        !c_module_symbol(glx_renderer->libgl_module,
                         "glXQueryDrawable",
                         (void **)&glx_renderer->glXQueryDrawable)) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "Failed to resolve required GLX symbol");
        return false;
    }

    return true;
}

static void
update_base_winsys_features(cg_renderer_t *renderer)
{
    cg_glx_renderer_t *glx_renderer = renderer->winsys;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    const char *glx_extensions;
    int default_screen;
    char **split_extensions;

    default_screen = DefaultScreen(xlib_renderer->xdpy);
    glx_extensions = glx_renderer->glXQueryExtensionsString(xlib_renderer->xdpy,
                                                            default_screen);

    CG_NOTE(WINSYS, "  GLX Extensions: %s", glx_extensions);

    split_extensions = c_strsplit(glx_extensions, " ", 0 /* max_tokens */);

    for (unsigned i = 0; i < C_N_ELEMENTS(winsys_feature_data); i++)
        if (_cg_feature_check(renderer,
                              "GLX",
                              winsys_feature_data + i,
                              glx_renderer->glx_major,
                              glx_renderer->glx_minor,
                              CG_DRIVER_GL, /* the driver isn't used */
                              split_extensions,
                              glx_renderer)) {
            if (winsys_feature_data[i].winsys_feature)
                CG_FLAGS_SET(glx_renderer->base_winsys_features,
                             winsys_feature_data[i].winsys_feature,
                             true);
        }

    c_strfreev(split_extensions);

    /* Note: the GLX_SGI_video_sync spec explicitly states this extension
     * only works for direct contexts. */
    if (!glx_renderer->is_direct) {
        glx_renderer->glXGetVideoSync = NULL;
        glx_renderer->glXWaitVideoSync = NULL;
        CG_FLAGS_SET(glx_renderer->base_winsys_features,
                     CG_WINSYS_FEATURE_VBLANK_COUNTER,
                     false);
    }

    CG_FLAGS_SET(glx_renderer->base_winsys_features,
                 CG_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                 true);

    if (glx_renderer->glXWaitVideoSync || glx_renderer->glXWaitForMsc)
        CG_FLAGS_SET(glx_renderer->base_winsys_features,
                     CG_WINSYS_FEATURE_VBLANK_WAIT,
                     true);
}

static bool
_cg_winsys_renderer_connect(cg_renderer_t *renderer,
                            cg_error_t **error)
{
    cg_glx_renderer_t *glx_renderer;
    cg_xlib_renderer_t *xlib_renderer;

    renderer->winsys = c_slice_new0(cg_glx_renderer_t);

    glx_renderer = renderer->winsys;
    xlib_renderer = _cg_xlib_renderer_get_data(renderer);

    if (!_cg_xlib_renderer_connect(renderer, error))
        goto error;

    if (renderer->driver != CG_DRIVER_GL && renderer->driver != CG_DRIVER_GL3) {
        _cg_set_error(
            error,
            CG_WINSYS_ERROR,
            CG_WINSYS_ERROR_INIT,
            "GLX Backend can only be used in conjunction with OpenGL");
        goto error;
    }

    glx_renderer->libgl_module =
        c_module_open(CG_GL_LIBNAME);

    if (glx_renderer->libgl_module == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "Failed to dynamically open the OpenGL library");
        goto error;
    }

    if (!resolve_core_glx_functions(renderer, error))
        goto error;

    if (!glx_renderer->glXQueryExtension(xlib_renderer->xdpy,
                                         &glx_renderer->glx_error_base,
                                         &glx_renderer->glx_event_base)) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "XServer appears to lack required GLX support");
        goto error;
    }

    /* XXX: Note: For a long time Mesa exported a hybrid GLX, exporting
     * extensions specified to require GLX 1.3, but still reporting 1.2
     * via glXQueryVersion. */
    if (!glx_renderer->glXQueryVersion(xlib_renderer->xdpy,
                                       &glx_renderer->glx_major,
                                       &glx_renderer->glx_minor) ||
        !(glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 2)) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_INIT,
                      "XServer appears to lack required GLX 1.2 support");
        goto error;
    }

    update_base_winsys_features(renderer);

    return true;

error:
    _cg_winsys_renderer_disconnect(renderer);
    return false;
}

static bool
update_winsys_features(cg_device_t *dev, cg_error_t **error)
{
    cg_glx_display_t *glx_display = dev->display->winsys;
    cg_renderer_t *renderer = dev->renderer;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;
    cg_glx_renderer_t *glx_renderer = renderer->winsys;

    c_return_val_if_fail(glx_display, false);
    c_return_val_if_fail(glx_display->glx_context, false);

    if (!_cg_device_update_features(dev, error))
        return false;

    memcpy(dev->winsys_features,
           glx_renderer->base_winsys_features,
           sizeof(dev->winsys_features));

    CG_FLAGS_SET(dev->features, CG_FEATURE_ID_ONSCREEN_MULTIPLE, true);

    if (glx_renderer->glXCopySubBuffer || dev->glBlitFramebuffer) {
        cg_gpu_info_t *info = &dev->gpu;
        cg_gpu_info_architecture_t arch = info->architecture;

        CG_FLAGS_SET(dev->winsys_features, CG_WINSYS_FEATURE_SWAP_REGION,
                     true);

        /*
         * "The "drisw" binding in Mesa for loading sofware renderers is
         * broken, and neither glBlitFramebuffer nor glXCopySubBuffer
         * work correctly."
         * - ajax
         * - https://bugzilla.gnome.org/show_bug.cgi?id=674208
         *
         * This is broken in software Mesa at least as of 7.10 and got
         * fixed in Mesa 10.1
         */

        if (info->driver_package == CG_GPU_INFO_DRIVER_PACKAGE_MESA &&
            info->driver_package_version < CG_VERSION_ENCODE(10, 1, 0) &&
            (arch == CG_GPU_INFO_ARCHITECTURE_LLVMPIPE ||
             arch == CG_GPU_INFO_ARCHITECTURE_SOFTPIPE ||
             arch == CG_GPU_INFO_ARCHITECTURE_SWRAST)) {
            CG_FLAGS_SET(dev->winsys_features, CG_WINSYS_FEATURE_SWAP_REGION,
                         false);
        }
    }

    /* Note: glXCopySubBuffer and glBlitFramebuffer won't be throttled
     * by the SwapInterval so we have to throttle swap_region requests
     * manually... */
    if (_cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_SWAP_REGION) &&
        _cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_VBLANK_WAIT))
        CG_FLAGS_SET(dev->winsys_features,
                     CG_WINSYS_FEATURE_SWAP_REGION_THROTTLE,
                     true);

    if (x11_renderer->net_wm_frame_drawn_supported) {
        CG_FLAGS_SET(dev->winsys_features,
                     CG_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT,
                     true);
    }

    if (_cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT)) {
        /* Either via GLX_INTEL_swap_event or _NET_WM_FRAME_DRAWN */
        CG_FLAGS_SET(dev->features, CG_FEATURE_ID_PRESENTATION_TIME, true);
    }

    /* We'll manually handle queueing dirty events in response to
     * Expose events from X */
    CG_FLAGS_SET(dev->private_features, CG_PRIVATE_FEATURE_DIRTY_EVENTS,
                 true);

    return true;
}

static void
glx_attributes_from_framebuffer_config(
    cg_display_t *display, cg_framebuffer_config_t *config, int *attributes)
{
    cg_glx_renderer_t *glx_renderer = display->renderer->winsys;
    int i = 0;

    attributes[i++] = GLX_DRAWABLE_TYPE;
    attributes[i++] = GLX_WINDOW_BIT;

    attributes[i++] = GLX_RENDER_TYPE;
    attributes[i++] = GLX_RGBA_BIT;

    attributes[i++] = GLX_DOUBLEBUFFER;
    attributes[i++] = GL_TRUE;

    attributes[i++] = GLX_RED_SIZE;
    attributes[i++] = 1;
    attributes[i++] = GLX_GREEN_SIZE;
    attributes[i++] = 1;
    attributes[i++] = GLX_BLUE_SIZE;
    attributes[i++] = 1;
    attributes[i++] = GLX_ALPHA_SIZE;
    attributes[i++] = config->has_alpha ? 1 : GLX_DONT_CARE;
    attributes[i++] = GLX_DEPTH_SIZE;
    attributes[i++] = 1;
    attributes[i++] = GLX_STENCIL_SIZE;
    attributes[i++] = config->need_stencil ? 1 : GLX_DONT_CARE;

    if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 4 &&
        config->samples_per_pixel) {
        attributes[i++] = GLX_SAMPLE_BUFFERS;
        attributes[i++] = 1;
        attributes[i++] = GLX_SAMPLES;
        attributes[i++] = config->samples_per_pixel;
    }

    attributes[i++] = None;

    c_assert(i < MAX_GLX_CONFIG_ATTRIBS);
}

/* It seems the GLX spec never defined an invalid GLXFBConfig that
 * we could overload as an indication of error, so we have to return
 * an explicit boolean status. */
static bool
find_fbconfig(cg_display_t *display,
              cg_framebuffer_config_t *config,
              GLXFBConfig *config_ret,
              cg_error_t **error)
{
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(display->renderer);
    cg_glx_renderer_t *glx_renderer = display->renderer->winsys;
    GLXFBConfig *configs = NULL;
    int n_configs;
    static int attributes[MAX_GLX_CONFIG_ATTRIBS];
    bool ret = true;
    int xscreen_num = DefaultScreen(xlib_renderer->xdpy);

    glx_attributes_from_framebuffer_config(display, config, attributes);

    configs = glx_renderer->glXChooseFBConfig(
        xlib_renderer->xdpy, xscreen_num, attributes, &n_configs);
    if (!configs || n_configs == 0) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "Failed to find any compatible fbconfigs");
        ret = false;
        goto done;
    }

    if (config->has_alpha) {
        int i;

        for (i = 0; i < n_configs; i++) {
            XVisualInfo *vinfo;

            vinfo = glx_renderer->glXGetVisualFromFBConfig(xlib_renderer->xdpy,
                                                           configs[i]);
            if (vinfo == NULL)
                continue;

            if (vinfo->depth == 32 && (vinfo->red_mask | vinfo->green_mask |
                                       vinfo->blue_mask) != 0xffffffff) {
                CG_NOTE(WINSYS, "Found an ARGB FBConfig [index:%d]", i);
                *config_ret = configs[i];
                goto done;
            }
        }

        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "Unable to find fbconfig with rgba visual");
        ret = false;
        goto done;
    } else {
        CG_NOTE(WINSYS, "Using the first available FBConfig");
        *config_ret = configs[0];
    }

done:
    XFree(configs);
    return ret;
}

static GLXContext
create_gl3_context(cg_display_t *display,
                   GLXFBConfig fb_config)
{
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(display->renderer);
    cg_glx_renderer_t *glx_renderer = display->renderer->winsys;

    /* We want a core profile 3.1 context with no deprecated features */
    static const int attrib_list[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 1,
        GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        GLX_CONTEXT_FLAGS_ARB,         GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
    };

    /* Make sure that the display supports the GLX_ARB_create_context
       extension */
    if (glx_renderer->glXCreateContextAttribs == NULL)
        return NULL;

    return glx_renderer->glXCreateContextAttribs(xlib_renderer->xdpy,
                                                 fb_config,
                                                 NULL /* share_context */,
                                                 True, /* direct */
                                                 attrib_list);
}

static bool
create_context(cg_display_t *display, cg_error_t **error)
{
    cg_glx_display_t *glx_display = display->winsys;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(display->renderer);
    cg_glx_renderer_t *glx_renderer = display->renderer->winsys;
    bool support_transparent_windows =
        display->onscreen_template->config.has_alpha;
    GLXFBConfig config;
    cg_error_t *fbconfig_error = NULL;
    XSetWindowAttributes attrs;
    XVisualInfo *xvisinfo;
    GLXDrawable dummy_drawable;
    cg_xlib_trap_state_t old_state;

    c_return_val_if_fail(glx_display->glx_context == NULL, true);

    glx_display->found_fbconfig = find_fbconfig(
        display, &display->onscreen_template->config, &config, &fbconfig_error);
    if (!glx_display->found_fbconfig) {
        _cg_set_error(
            error,
            CG_WINSYS_ERROR,
            CG_WINSYS_ERROR_CREATE_CONTEXT,
            "Unable to find suitable fbconfig for the GLX context: %s",
            fbconfig_error->message);
        cg_error_free(fbconfig_error);
        return false;
    }

    glx_display->fbconfig = config;
    glx_display->fbconfig_has_rgba_visual = support_transparent_windows;

    CG_NOTE(WINSYS, "Creating GLX Context (display: %p)", xlib_renderer->xdpy);

    if (display->renderer->driver == CG_DRIVER_GL3)
        glx_display->glx_context = create_gl3_context(display, config);
    else
        glx_display->glx_context = glx_renderer->glXCreateNewContext(
            xlib_renderer->xdpy, config, GLX_RGBA_TYPE, NULL, True);

    if (glx_display->glx_context == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "Unable to create suitable GL context");
        return false;
    }

    glx_renderer->is_direct = glx_renderer->glXIsDirect(
        xlib_renderer->xdpy, glx_display->glx_context);

    CG_NOTE(WINSYS,
            "Setting %s context",
            glx_renderer->is_direct ? "direct" : "indirect");

    /* XXX: GLX doesn't let us make a context current without a window
     * so we create a dummy window that we can use while no cg_onscreen_t
     * framebuffer is in use.
     */

    xvisinfo =
        glx_renderer->glXGetVisualFromFBConfig(xlib_renderer->xdpy, config);
    if (xvisinfo == NULL) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "Unable to retrieve the X11 visual");
        return false;
    }

    _cg_xlib_renderer_trap_errors(display->renderer, &old_state);

    attrs.override_redirect = True;
    attrs.colormap = XCreateColormap(xlib_renderer->xdpy,
                                     DefaultRootWindow(xlib_renderer->xdpy),
                                     xvisinfo->visual,
                                     AllocNone);
    attrs.border_pixel = 0;

    glx_display->dummy_xwin =
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

    /* Try and create a GLXWindow to use with extensions dependent on
     * GLX versions >= 1.3 that don't accept regular X Windows as GLX
     * drawables. */
    if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 3) {
        glx_display->dummy_glxwin = glx_renderer->glXCreateWindow(
            xlib_renderer->xdpy, config, glx_display->dummy_xwin, NULL);
    }

    if (glx_display->dummy_glxwin)
        dummy_drawable = glx_display->dummy_glxwin;
    else
        dummy_drawable = glx_display->dummy_xwin;

    CG_NOTE(WINSYS,
            "Selecting dummy 0x%x for the GLX context",
            (unsigned int)dummy_drawable);

    glx_renderer->glXMakeContextCurrent(xlib_renderer->xdpy,
                                        dummy_drawable,
                                        dummy_drawable,
                                        glx_display->glx_context);

    XFree(xvisinfo);

    if (_cg_xlib_renderer_untrap_errors(display->renderer, &old_state)) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_CONTEXT,
                      "Unable to select the newly created GLX context");
        return false;
    }

    return true;
}

static void
_cg_winsys_display_destroy(cg_display_t *display)
{
    cg_glx_display_t *glx_display = display->winsys;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(display->renderer);
    cg_glx_renderer_t *glx_renderer = display->renderer->winsys;

    c_return_if_fail(glx_display != NULL);

    if (glx_display->glx_context) {
        glx_renderer->glXMakeContextCurrent(
            xlib_renderer->xdpy, None, None, NULL);
        glx_renderer->glXDestroyContext(xlib_renderer->xdpy,
                                        glx_display->glx_context);
        glx_display->glx_context = NULL;
    }

    if (glx_display->dummy_glxwin) {
        glx_renderer->glXDestroyWindow(xlib_renderer->xdpy,
                                       glx_display->dummy_glxwin);
        glx_display->dummy_glxwin = None;
    }

    if (glx_display->dummy_xwin) {
        XDestroyWindow(xlib_renderer->xdpy, glx_display->dummy_xwin);
        glx_display->dummy_xwin = None;
    }

    c_slice_free(cg_glx_display_t, display->winsys);
    display->winsys = NULL;
}

static bool
_cg_winsys_display_setup(cg_display_t *display, cg_error_t **error)
{
    cg_glx_display_t *glx_display;
    int i;

    c_return_val_if_fail(display->winsys == NULL, false);

    glx_display = c_slice_new0(cg_glx_display_t);
    display->winsys = glx_display;

    if (!create_context(display, error))
        goto error;

    for (i = 0; i < CG_GLX_N_CACHED_CONFIGS; i++)
        glx_display->glx_cached_configs[i].depth = -1;

    return true;

error:
    _cg_winsys_display_destroy(display);
    return false;
}

static bool
_cg_winsys_device_init(cg_device_t *dev, cg_error_t **error)
{
    dev->winsys = c_new0(cg_device_glx_t, 1);

    cg_xlib_renderer_add_filter(dev->display->renderer, glx_event_filter_cb,
                                dev);
    return update_winsys_features(dev, error);
}

static void
_cg_winsys_device_deinit(cg_device_t *dev)
{
    cg_xlib_renderer_remove_filter(dev->display->renderer,
                                   glx_event_filter_cb, dev);
    c_free(dev->winsys);
}

static bool
_cg_winsys_onscreen_init(cg_onscreen_t *onscreen,
                         cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_glx_display_t *glx_display = display->winsys;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(display->renderer);
    cg_glx_renderer_t *glx_renderer = display->renderer->winsys;
    Window xwin;
    cg_onscreen_xlib_t *xlib_onscreen;
    cg_onscreen_glx_t *glx_onscreen;
    GLXFBConfig fbconfig;
    cg_error_t *fbconfig_error = NULL;

    c_return_val_if_fail(glx_display->glx_context, false);

    if (!find_fbconfig(
            display, &framebuffer->config, &fbconfig, &fbconfig_error)) {
        _cg_set_error(
            error,
            CG_WINSYS_ERROR,
            CG_WINSYS_ERROR_CREATE_CONTEXT,
            "Unable to find suitable fbconfig for the GLX context: %s",
            fbconfig_error->message);
        cg_error_free(fbconfig_error);
        return false;
    }

    /* Update the real number of samples_per_pixel now that we have
     * found an fbconfig... */
    if (framebuffer->config.samples_per_pixel) {
        int samples;
        int status = glx_renderer->glXGetFBConfigAttrib(
            xlib_renderer->xdpy, fbconfig, GLX_SAMPLES, &samples);
        c_return_val_if_fail(status == Success, true);
        framebuffer->samples_per_pixel = samples;
    }

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
        XSync(xlib_renderer->xdpy, False);
        xerror = _cg_xlib_renderer_untrap_errors(display->renderer, &state);
        if (status == 0 || xerror) {
            char message[1000];
            XGetErrorText(
                xlib_renderer->xdpy, xerror, message, sizeof(message));
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_ONSCREEN,
                          "Unable to query geometry of foreign xid 0x%08lX: %s",
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

        xvisinfo = glx_renderer->glXGetVisualFromFBConfig(xlib_renderer->xdpy,
                                                          fbconfig);
        if (xvisinfo == NULL) {
            _cg_set_error(error,
                          CG_WINSYS_ERROR,
                          CG_WINSYS_ERROR_CREATE_ONSCREEN,
                          "Unable to retrieve the X11 visual of context's "
                          "fbconfig");
            return false;
        }

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

    onscreen->winsys = c_slice_new0(cg_onscreen_glx_t);
    xlib_onscreen = onscreen->winsys;
    glx_onscreen = onscreen->winsys;

    xlib_onscreen->xwin = xwin;
    xlib_onscreen->is_foreign_xwin = onscreen->foreign_xid ? true : false;

    {
        Atom window_type_atom = XInternAtom(xlib_renderer->xdpy, "_NET_WM_WINDOW_TYPE", False);
        Atom normal_atom = XInternAtom(xlib_renderer->xdpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
        Atom net_wm_pid_atom = XInternAtom(xlib_renderer->xdpy, "_NET_WM_PID", False);
        Atom net_wm_sync_req_counter_atom = XInternAtom(xlib_renderer->xdpy, "_NET_WM_SYNC_REQUEST_COUNTER", False);
        Atom wm_protocols[] = {
            XInternAtom(xlib_renderer->xdpy, "WM_DELETE_WINDOW", False),
            XInternAtom(xlib_renderer->xdpy, "WM_TAKE_FOCUS", False),
            XInternAtom(xlib_renderer->xdpy, "_NET_WM_PING", False),
            XInternAtom(xlib_renderer->xdpy, "_NET_WM_SYNC_REQUEST", False),
        };
        uint32_t pid;
        XSyncValue xzero;

        XSetWMProtocols(xlib_renderer->xdpy, xwin, wm_protocols,
                        sizeof(wm_protocols) / sizeof(Atom));

        pid = getpid();
        XChangeProperty(xlib_renderer->xdpy,
                        xwin,
                        net_wm_pid_atom,
                        XA_CARDINAL,
                        32, /* format */
                        PropModeReplace,
                        (unsigned char *)&pid,
                        1); /* n elements */

        XChangeProperty(xlib_renderer->xdpy,
                        xwin,
                        window_type_atom,
                        XA_ATOM,
                        32, /* format */
                        PropModeReplace,
                        (unsigned char *)&normal_atom,
                        1); /* n elements */


        XSyncIntToValue(&xzero, 0);
        xlib_onscreen->frame_sync_xcounters[0] = XSyncCreateCounter(xlib_renderer->xdpy,
                                                                   xzero);
        xlib_onscreen->frame_sync_counters[0] = 0;
        xlib_onscreen->frame_sync_xcounters[1] = XSyncCreateCounter(xlib_renderer->xdpy,
                                                                   xzero);
        xlib_onscreen->frame_sync_counters[1] = 0;

        XChangeProperty(xlib_renderer->xdpy,
                        xwin,
                        net_wm_sync_req_counter_atom,
                        XA_CARDINAL,
                        32, /* format */
                        PropModeReplace,
                        (unsigned char *)xlib_onscreen->frame_sync_xcounters,
                        2); /* n elements */
    }

    /* Try and create a GLXWindow to use with extensions dependent on
     * GLX versions >= 1.3 that don't accept regular X Windows as GLX
     * drawables. */
    if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 3) {
        glx_onscreen->glxwin = glx_renderer->glXCreateWindow(
            xlib_renderer->xdpy, fbconfig, xlib_onscreen->xwin, NULL);
    }

#ifdef GLX_INTEL_swap_event
    if (_cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT)) {
        GLXDrawable drawable =
            glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

        /* similarly to above, we unconditionally select this event
         * because we rely on it to advance the master clock, and
         * drive redraw/relayout, animations and event handling.
         */
        glx_renderer->glXSelectEvent(
            xlib_renderer->xdpy, drawable, GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
    }
#endif /* GLX_INTEL_swap_event */

    return true;
}

static void
_cg_winsys_onscreen_deinit(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_device_glx_t *glx_context = dev->winsys;
    cg_glx_display_t *glx_display = dev->display->winsys;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_glx_renderer_t *glx_renderer = dev->display->renderer->winsys;
    cg_xlib_trap_state_t old_state;
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
    cg_onscreen_glx_t *glx_onscreen = onscreen->winsys;
    GLXDrawable drawable;

    /* If we never successfully allocated then there's nothing to do */
    if (glx_onscreen == NULL)
        return;

    if (xlib_onscreen->output != NULL) {
        cg_object_unref(xlib_onscreen->output);
        xlib_onscreen->output = NULL;
    }

    _cg_xlib_renderer_trap_errors(dev->display->renderer, &old_state);

    drawable = glx_onscreen->glxwin == None ? xlib_onscreen->xwin
               : glx_onscreen->glxwin;

    /* CGlib always needs a valid context bound to something so if we are
     * destroying the onscreen that is currently bound we'll switch back
     * to the dummy drawable. Although the documentation for
     * glXDestroyWindow states that a currently bound window won't
     * actually be destroyed until it is unbound, it looks like this
     * doesn't work if the X window itself is destroyed */
    if (drawable == glx_context->current_drawable) {
        GLXDrawable dummy_drawable =
            (glx_display->dummy_glxwin == None ? glx_display->dummy_xwin
             : glx_display->dummy_glxwin);

        glx_renderer->glXMakeContextCurrent(xlib_renderer->xdpy,
                                            dummy_drawable,
                                            dummy_drawable,
                                            glx_display->glx_context);
        glx_context->current_drawable = dummy_drawable;
    }

    if (glx_onscreen->glxwin != None) {
        glx_renderer->glXDestroyWindow(xlib_renderer->xdpy,
                                       glx_onscreen->glxwin);
        glx_onscreen->glxwin = None;
    }

    if (!xlib_onscreen->is_foreign_xwin && xlib_onscreen->xwin != None) {
        XDestroyWindow(xlib_renderer->xdpy, xlib_onscreen->xwin);
        xlib_onscreen->xwin = None;
    } else
        xlib_onscreen->xwin = None;

    XSync(xlib_renderer->xdpy, False);

    _cg_xlib_renderer_untrap_errors(dev->display->renderer, &old_state);

    c_slice_free(cg_onscreen_glx_t, onscreen->winsys);
    onscreen->winsys = NULL;
}

static void
_cg_winsys_onscreen_bind(cg_onscreen_t *onscreen)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_device_glx_t *glx_context = dev->winsys;
    cg_glx_display_t *glx_display = dev->display->winsys;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_glx_renderer_t *glx_renderer = dev->display->renderer->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
    cg_onscreen_glx_t *glx_onscreen = onscreen->winsys;
    cg_xlib_trap_state_t old_state;
    GLXDrawable drawable;

    drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

    if (glx_context->current_drawable == drawable)
        return;

    _cg_xlib_renderer_trap_errors(dev->display->renderer, &old_state);

    CG_NOTE(WINSYS,
            "MakeContextCurrent dpy: %p, window: 0x%x (%s), context: %p",
            xlib_renderer->xdpy,
            (unsigned int)drawable,
            xlib_onscreen->is_foreign_xwin ? "foreign" : "native",
            glx_display->glx_context);

    glx_renderer->glXMakeContextCurrent(
        xlib_renderer->xdpy, drawable, drawable, glx_display->glx_context);

    /* In case we are using GLX_SGI_swap_control for vblank syncing
     * we need call glXSwapIntervalSGI here to make sure that it
     * affects the current drawable.
     *
     * Note: we explicitly set to 0 when we aren't using the swap
     * interval to synchronize since some drivers have a default
     * swap interval of 1. Sadly some drivers even ignore requests
     * to disable the swap interval.
     *
     * NB: glXSwapIntervalSGI applies to the context not the
     * drawable which is why we can't just do this once when the
     * framebuffer is allocated.
     *
     * FIXME: We should check for GLX_EXT_swap_control which allows
     * per framebuffer swap intervals. GLX_MESA_swap_control also
     * allows per-framebuffer swap intervals but the semantics tend
     * to be more muddled since Mesa drivers tend to expose both the
     * MESA and SGI extensions which should technically be mutually
     * exclusive.
     */
    if (glx_renderer->glXSwapInterval) {
        cg_framebuffer_t *fb = CG_FRAMEBUFFER(onscreen);
        cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

        /* NB: If _NET_WM_FRAME_DRAWN is available then we are running
         * under a compositor and throttling is handled via X client
         * messages instead */
        if (fb->config.swap_throttled &&
            !x11_renderer->net_wm_frame_drawn_supported)
        {
            glx_renderer->glXSwapInterval(1);
        } else
            glx_renderer->glXSwapInterval(0);
    }

    XSync(xlib_renderer->xdpy, False);

    /* FIXME: We should be reporting a cg_error_t here
     */
    if (_cg_xlib_renderer_untrap_errors(dev->display->renderer,
                                        &old_state)) {
        c_warning("X Error received while making drawable 0x%08lX current",
                  drawable);
        return;
    }

    glx_context->current_drawable = drawable;
}

static void
_cg_winsys_wait_for_gpu(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;

    dev->glFinish();
}

static void
_cg_winsys_wait_for_vblank(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_glx_renderer_t *glx_renderer;
    cg_xlib_renderer_t *xlib_renderer;

    glx_renderer = dev->display->renderer->winsys;
    xlib_renderer = _cg_xlib_renderer_get_data(dev->display->renderer);

    if (glx_renderer->glXWaitForMsc || glx_renderer->glXGetVideoSync) {
        cg_frame_info_t *info =
            c_queue_peek_tail(&onscreen->pending_frame_infos);

        if (glx_renderer->glXWaitForMsc) {
            cg_onscreen_glx_t *glx_onscreen = onscreen->winsys;
            Drawable drawable = glx_onscreen->glxwin;
            int64_t ust;
            int64_t msc;
            int64_t sbc;

            glx_renderer->glXWaitForMsc(xlib_renderer->xdpy,
                                        drawable,
                                        0, 1, 0,
                                        &ust, &msc, &sbc);

            if (!dev->presentation_time_seen)
                check_ust_is_monotonic(dev, drawable);

            if (dev->presentation_clock_is_monotonic)
                info->presentation_time = ust_to_nanoseconds(dev, ust);
        } else {
            uint32_t current_count;

            glx_renderer->glXGetVideoSync(&current_count);
            glx_renderer->glXWaitVideoSync(
                2, (current_count + 1) % 2, &current_count);

            info->presentation_time = c_get_monotonic_time();
        }
    }
}

static uint32_t
_cg_winsys_get_vsync_counter(cg_device_t *dev)
{
    uint32_t video_sync_count;
    cg_glx_renderer_t *glx_renderer;

    glx_renderer = dev->display->renderer->winsys;

    glx_renderer->glXGetVideoSync(&video_sync_count);

    return video_sync_count;
}

#ifndef GLX_BACK_BUFFER_AGE_EXT
#define GLX_BACK_BUFFER_AGE_EXT 0x20F4
#endif

static int
_cg_winsys_onscreen_get_buffer_age(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_glx_renderer_t *glx_renderer = dev->display->renderer->winsys;
    cg_onscreen_glx_t *glx_onscreen = onscreen->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
    GLXDrawable drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;
    unsigned int age;

    if (!_cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_BUFFER_AGE))
        return 0;

    glx_renderer->glXQueryDrawable(
        xlib_renderer->xdpy, drawable, GLX_BACK_BUFFER_AGE_EXT, &age);

    return age;
}

static void
set_frame_info_output(cg_onscreen_t *onscreen, cg_output_t *output)
{
    cg_frame_info_t *info = c_queue_peek_tail(&onscreen->pending_frame_infos);

    info->output = output;

    if (output) {
        float refresh_rate = cg_output_get_refresh_rate(output);
        if (refresh_rate != 0.0)
            info->refresh_rate = refresh_rate;
    }
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
_x11_frame_sync_start_frame(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

    if (x11_renderer->net_wm_frame_drawn_supported) {
        cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
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
_x11_frame_sync_end_frame(cg_onscreen_t *onscreen)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

    if (x11_renderer->net_wm_frame_drawn_supported) {
        cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
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

static void
_cg_winsys_onscreen_swap_region(cg_onscreen_t *onscreen,
                                const int *user_rectangles,
                                int n_rectangles)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;
    cg_glx_renderer_t *glx_renderer = dev->display->renderer->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
    cg_onscreen_glx_t *glx_onscreen = onscreen->winsys;
    GLXDrawable drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;
    uint32_t end_frame_vsync_counter = 0;
    bool have_counter = false;
    bool can_wait = false;
    int x_min = 0, x_max = 0, y_min = 0, y_max = 0;

    /*
     * We assume that glXCopySubBuffer is synchronized which means it
     * won't prevent multiple blits per retrace if they can all be
     * performed in the blanking period. If that's the case then we
     * still want to use the vblank sync menchanism but we only need
     * it to throttle redraws.
     */
    bool blit_sub_buffer_is_synchronized =
        _cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_SWAP_REGION_SYNCHRONIZED);

    int framebuffer_width = cg_framebuffer_get_width(framebuffer);
    int framebuffer_height = cg_framebuffer_get_height(framebuffer);
    int *rectangles = c_alloca(sizeof(int) * n_rectangles * 4);
    int i;

    _x11_frame_sync_start_frame(onscreen);

    /* glXCopySubBuffer expects rectangles relative to the bottom left
     * corner but we are given rectangles relative to the top left so
     * we need to flip them... */
    memcpy(rectangles, user_rectangles, sizeof(int) * n_rectangles * 4);
    for (i = 0; i < n_rectangles; i++) {
        int *rect = &rectangles[4 * i];

        if (i == 0) {
            x_min = rect[0];
            x_max = rect[0] + rect[2];
            y_min = rect[1];
            y_max = rect[1] + rect[3];
        } else {
            x_min = MIN(x_min, rect[0]);
            x_max = MAX(x_max, rect[0] + rect[2]);
            y_min = MIN(y_min, rect[1]);
            y_max = MAX(y_max, rect[1] + rect[3]);
        }

        rect[1] = framebuffer_height - rect[1] - rect[3];
    }

    _cg_framebuffer_flush_state(
        framebuffer, framebuffer, CG_FRAMEBUFFER_STATE_BIND);

    /* We need to ensure that all the rendering is done, otherwise
     * redraw operations that are slower than the framerate can
     * queue up in the pipeline during a heavy animation, causing a
     * larger and larger backlog of rendering visible as lag to the
     * user.
     *
     * For an exaggerated example consider rendering at 60fps (so 16ms
     * per frame) and you have a really slow frame that takes 160ms to
     * render, even though painting the scene and issuing the commands
     * to the GPU takes no time at all. If all we did was use the
     * video_sync extension to throttle the painting done by the CPU
     * then every 16ms we would have another frame queued up even though
     * the GPU has only rendered one tenth of the current frame. By the
     * time the GPU would get to the 2nd frame there would be 9 frames
     * waiting to be rendered.
     *
     * The problem is that we don't currently have a good way to throttle
     * the GPU, only the CPU so we have to resort to synchronizing the
     * GPU with the CPU to throttle it.
     *
     * Note: since calling glFinish() and synchronizing the CPU with
     * the GPU is far from ideal, we hope that this is only a short
     * term solution.
     * - One idea is to using sync objects to track render
     *   completion so we can throttle the backlog (ideally with an
     *   additional extension that lets us get notifications in our
     *   mainloop instead of having to busy wait for the
     *   completion.)
     * - Another option is to support clipped redraws by reusing the
     *   contents of old back buffers such that we can flip instead
     *   of using a blit and then we can use GLX_INTEL_swap_events
     *   to throttle. For this though we would still probably want an
     *   additional extension so we can report the limited region of
     *   the window damage to X/compositors.
     */
    _cg_winsys_wait_for_gpu(onscreen);

    /* Throttle the frame rate...
     *
     * If _NET_WM_FRAME_DRAWN is available then we are composited and
     * sync and complete events are driven by _NET_WM_FRAME_DRAWN and
     * _FRAME_TIMINGS messages as a result of updating the
     * _FRAME_SYNC_COUNTERS, independent of the GLX presentation
     * details.
     */
    if (framebuffer->config.swap_throttled &&
        !x11_renderer->net_wm_frame_drawn_supported)
    {
        have_counter = _cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_VBLANK_COUNTER);
        can_wait = _cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_VBLANK_WAIT);

        if (blit_sub_buffer_is_synchronized && have_counter && can_wait) {
            end_frame_vsync_counter = _cg_winsys_get_vsync_counter(dev);

            /* If we have the GLX_SGI_video_sync extension then we can
             * be a bit smarter about how we throttle blits by avoiding
             * any waits if we can see that the video sync count has
             * already progressed. */
            if (glx_onscreen->last_swap_vsync_counter == end_frame_vsync_counter)
                _cg_winsys_wait_for_vblank(onscreen);
        } else if (can_wait)
            _cg_winsys_wait_for_vblank(onscreen);
    }

    if (glx_renderer->glXCopySubBuffer) {
        Display *xdpy = xlib_renderer->xdpy;
        int i;
        for (i = 0; i < n_rectangles; i++) {
            int *rect = &rectangles[4 * i];
            glx_renderer->glXCopySubBuffer(
                xdpy, drawable, rect[0], rect[1], rect[2], rect[3]);
        }
    } else if (dev->glBlitFramebuffer) {
        int i;
        /* XXX: checkout how this state interacts with the code to use
         * glBlitFramebuffer in Neil's texture atlasing branch */

        /* glBlitFramebuffer is affected by the scissor so we need to
         * ensure we have flushed an empty clip stack to get rid of it.
         * We also mark that the clip state is dirty so that it will be
         * flushed to the correct state the next time something is
         * drawn */
        _cg_clip_stack_flush(NULL, framebuffer);
        dev->current_draw_buffer_changes |= CG_FRAMEBUFFER_STATE_CLIP;

        dev->glDrawBuffer(GL_FRONT);
        for (i = 0; i < n_rectangles; i++) {
            int *rect = &rectangles[4 * i];
            int x2 = rect[0] + rect[2];
            int y2 = rect[1] + rect[3];
            dev->glBlitFramebuffer(rect[0],
                                       rect[1],
                                       x2,
                                       y2,
                                       rect[0],
                                       rect[1],
                                       x2,
                                       y2,
                                       GL_COLOR_BUFFER_BIT,
                                       GL_NEAREST);
        }
        dev->glDrawBuffer(GL_BACK);
    }

    /* NB: unlike glXSwapBuffers, glXCopySubBuffer and
     * glBlitFramebuffer don't issue an implicit glFlush() so we
     * have to flush ourselves if we want the request to complete in
     * a finite amount of time since otherwise the driver can batch
     * the command indefinitely. */
    dev->glFlush();

    /* NB: It's important we save the counter we read before acting on
     * the swap request since if we are mixing and matching different
     * swap methods between frames we don't want to read the timer e.g.
     * after calling glFinish() some times and not for others.
     *
     * In other words; this way we consistently save the time at the end
     * of the applications frame such that the counter isn't muddled by
     * the varying costs of different swap methods.
     */
    if (have_counter)
        glx_onscreen->last_swap_vsync_counter = end_frame_vsync_counter;

    if (!xlib_onscreen->is_foreign_xwin) {
        cg_output_t *output;

        x_min = CLAMP(x_min, 0, framebuffer_width);
        x_max = CLAMP(x_max, 0, framebuffer_width);
        y_min = CLAMP(y_min, 0, framebuffer_width);
        y_max = CLAMP(y_max, 0, framebuffer_height);

        output =
            _cg_xlib_renderer_output_for_rectangle(dev->display->renderer,
                                                   xlib_onscreen->x + x_min,
                                                   xlib_onscreen->y + y_min,
                                                   x_max - x_min,
                                                   y_max - y_min);

        set_frame_info_output(onscreen, output);
    }

    /* XXX: we don't get SwapComplete events based on how we implement
     * the _swap_region() API but if cg-onscreen.c knows we are
     * handling _SYNC and _COMPLETE events in the winsys then we need to
     * send fake events in this case.
     *
     * If _NET_WM_FRAME_DRAWN is available then we are composited and
     * sync and complete events are driven by _NET_WM_FRAME_DRAWN and
     * _FRAME_TIMINGS messages as a result of updating the
     * _FRAME_SYNC_COUNTERS, independent of the GLX presentation
     * details.
     */
    if (!x11_renderer->net_wm_frame_drawn_supported &&
        _cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT))
    {
        /* XXX: if the application is mixing _swap_region and
         * _swap_buffers_with_damage() they might see _SYNC/_COMPLETE
         * events for different frames arrive out of order as we pop
         * from the tail here, but otherwise pop from head! */
        cg_frame_info_t *info = c_queue_pop_tail(&onscreen->pending_frame_infos);
        c_warning("CG: pop tail frame info = %p (swap_region)", info);

        _cg_onscreen_queue_event(onscreen, CG_FRAME_EVENT_SYNC, info);
        _cg_onscreen_queue_event(onscreen, CG_FRAME_EVENT_COMPLETE, info);

        cg_object_unref(info);
    }

    _x11_frame_sync_end_frame(onscreen);
}

static void
_cg_winsys_onscreen_swap_buffers_with_damage(
    cg_onscreen_t *onscreen, const int *rectangles, int n_rectangles)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;
    cg_glx_renderer_t *glx_renderer = dev->display->renderer->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
    cg_onscreen_glx_t *glx_onscreen = onscreen->winsys;
    bool have_counter = false;
    GLXDrawable drawable;

    _x11_frame_sync_start_frame(onscreen);

    /* XXX: theoretically this shouldn't be necessary but at least with
     * the Intel drivers we have seen that if we don't call
     * glXMakeContextCurrent for the drawable we are swapping then
     * we get a BadDrawable error from the X server. */
    _cg_framebuffer_flush_state(
        framebuffer, framebuffer, CG_FRAMEBUFFER_STATE_BIND);

    drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

    /* Throttling...
     *
     * If _NET_WM_FRAME_DRAWN is available then we are composited and
     * sync and complete events are driven by _NET_WM_FRAME_DRAWN and
     * _FRAME_TIMINGS messages as a result of updating the
     * _FRAME_SYNC_COUNTERS, independent of the GLX presentation
     * details.
     *
     * Although typically throttling is handled for us via
     * glXSwapInterval we have to consider 1) rarely, we might not have
     * glXSwapInterval support 2) the application is mixing use of
     * _swap_buffers_with_damage() and _swap_region() and the
     * _swap_region() code path bypasses glXSwapInterval and we want
     * consistent throttling between the two apis.
     */
    if (framebuffer->config.swap_throttled &&
        !x11_renderer->net_wm_frame_drawn_supported)
    {
        uint32_t end_frame_vsync_counter = 0;

        have_counter = _cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_VBLANK_COUNTER);

        /* If the swap_region API is also being used then we need to track
         * the vsync counter for each swap request so we can manually
         * throttle swap_region requests. */
        if (have_counter)
            end_frame_vsync_counter = _cg_winsys_get_vsync_counter(dev);

        if (!glx_renderer->glXSwapInterval) {
            bool can_wait =
                _cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_VBLANK_WAIT);

            /* If we are going to wait for VBLANK manually, we not only
             * need to flush out pending drawing to the GPU before we
             * sleep, we need to wait for it to finish. Otherwise, we
             * may end up with the situation:
             *
             *        - We finish drawing      - GPU drawing continues
             *        - We go to sleep         - GPU drawing continues
             * VBLANK - We call glXSwapBuffers - GPU drawing continues
             *                                 - GPU drawing continues
             *                                 - Swap buffers happens
             *
             * Producing a tear. Calling glFinish() first will cause us
             * to properly wait for the next VBLANK before we swap. This
             * obviously does not happen when we use _GLX_SWAP and let
             * the driver do the right thing
             */
            _cg_winsys_wait_for_gpu(onscreen);

            if (have_counter && can_wait) {
                if (glx_onscreen->last_swap_vsync_counter ==
                    end_frame_vsync_counter)
                    _cg_winsys_wait_for_vblank(onscreen);
            } else if (can_wait)
                _cg_winsys_wait_for_vblank(onscreen);
        }
    } else
        have_counter = false;

    glx_renderer->glXSwapBuffers(xlib_renderer->xdpy, drawable);

    if (have_counter)
        glx_onscreen->last_swap_vsync_counter =
            _cg_winsys_get_vsync_counter(dev);

    set_frame_info_output(onscreen, xlib_onscreen->output);

    _x11_frame_sync_end_frame(onscreen);
}

static uint32_t
_cg_winsys_onscreen_x11_get_window_xid(cg_onscreen_t *onscreen)
{
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
    return xlib_onscreen->xwin;
}

static void
_cg_winsys_onscreen_update_swap_throttled(cg_onscreen_t *onscreen)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_device_glx_t *glx_context = dev->winsys;
    cg_onscreen_glx_t *glx_onscreen = onscreen->winsys;
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;
    GLXDrawable drawable =
        glx_onscreen->glxwin ? glx_onscreen->glxwin : xlib_onscreen->xwin;

    if (glx_context->current_drawable != drawable)
        return;

    glx_context->current_drawable = 0;
    _cg_winsys_onscreen_bind(onscreen);
}

static void
_cg_winsys_onscreen_set_visibility(cg_onscreen_t *onscreen,
                                   bool visibility)
{
    cg_device_t *dev = CG_FRAMEBUFFER(onscreen)->dev;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(dev->display->renderer);
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;

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
    cg_onscreen_xlib_t *xlib_onscreen = onscreen->winsys;

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

static XVisualInfo *
_cg_winsys_xlib_get_visual_info(cg_onscreen_t *onscreen,
                                cg_error_t **error)
{
    cg_framebuffer_t *framebuffer = CG_FRAMEBUFFER(onscreen);
    cg_device_t *dev = framebuffer->dev;
    cg_display_t *display = dev->display;
    cg_glx_display_t *glx_display = display->winsys;
    cg_xlib_renderer_t *xlib_renderer =
        _cg_xlib_renderer_get_data(display->renderer);
    cg_glx_renderer_t *glx_renderer = display->renderer->winsys;
    GLXFBConfig fbconfig;
    XVisualInfo *xvisinfo;

    c_return_val_if_fail(glx_display->glx_context, false);

    if (!find_fbconfig(display, &framebuffer->config,
                       &fbconfig, error))
        return NULL;

    xvisinfo = glx_renderer->glXGetVisualFromFBConfig(xlib_renderer->xdpy,
                                                      fbconfig);
    if (!xvisinfo) {
        _cg_set_error(error,
                      CG_WINSYS_ERROR,
                      CG_WINSYS_ERROR_CREATE_ONSCREEN,
                      "Unable to retrieve the X11 visual of fbconfig");
    }

    return xvisinfo;
}

static bool
get_fbconfig_for_depth(cg_device_t *dev,
                       int depth,
                       GLXFBConfig *fbconfig_ret,
                       bool *can_mipmap_ret)
{
    cg_xlib_renderer_t *xlib_renderer;
    cg_glx_renderer_t *glx_renderer;
    cg_glx_display_t *glx_display;
    Display *dpy;
    GLXFBConfig *fbconfigs;
    int n_elements, i;
    int db, stencil, alpha, mipmap, rgba, value;
    int spare_cache_slot = 0;
    bool found = false;

    xlib_renderer = _cg_xlib_renderer_get_data(dev->display->renderer);
    glx_renderer = dev->display->renderer->winsys;
    glx_display = dev->display->winsys;

    /* Check if we've already got a cached config for this depth */
    for (i = 0; i < CG_GLX_N_CACHED_CONFIGS; i++)
        if (glx_display->glx_cached_configs[i].depth == -1)
            spare_cache_slot = i;
        else if (glx_display->glx_cached_configs[i].depth == depth) {
            *fbconfig_ret = glx_display->glx_cached_configs[i].fb_config;
            *can_mipmap_ret = glx_display->glx_cached_configs[i].can_mipmap;
            return glx_display->glx_cached_configs[i].found;
        }

    dpy = xlib_renderer->xdpy;

    fbconfigs =
        glx_renderer->glXGetFBConfigs(dpy, DefaultScreen(dpy), &n_elements);

    db = INT_MAX;
    stencil = INT_MAX;
    mipmap = 0;
    rgba = 0;

    for (i = 0; i < n_elements; i++) {
        XVisualInfo *vi;
        int visual_depth;

        vi = glx_renderer->glXGetVisualFromFBConfig(dpy, fbconfigs[i]);
        if (vi == NULL)
            continue;

        visual_depth = vi->depth;

        XFree(vi);

        if (visual_depth != depth)
            continue;

        glx_renderer->glXGetFBConfigAttrib(
            dpy, fbconfigs[i], GLX_ALPHA_SIZE, &alpha);
        glx_renderer->glXGetFBConfigAttrib(
            dpy, fbconfigs[i], GLX_BUFFER_SIZE, &value);
        if (value != depth && (value - alpha) != depth)
            continue;

        if (glx_renderer->glx_major == 1 && glx_renderer->glx_minor >= 4) {
            glx_renderer->glXGetFBConfigAttrib(
                dpy, fbconfigs[i], GLX_SAMPLES, &value);
            if (value > 1)
                continue;
        }

        value = 0;
        if (depth == 32) {
            glx_renderer->glXGetFBConfigAttrib(
                dpy, fbconfigs[i], GLX_BIND_TO_TEXTURE_RGBA_EXT, &value);
            if (value)
                rgba = 1;
        }

        if (!value) {
            if (rgba)
                continue;

            glx_renderer->glXGetFBConfigAttrib(
                dpy, fbconfigs[i], GLX_BIND_TO_TEXTURE_RGB_EXT, &value);
            if (!value)
                continue;
        }

        glx_renderer->glXGetFBConfigAttrib(
            dpy, fbconfigs[i], GLX_DOUBLEBUFFER, &value);
        if (value > db)
            continue;

        db = value;

        glx_renderer->glXGetFBConfigAttrib(
            dpy, fbconfigs[i], GLX_STENCIL_SIZE, &value);
        if (value > stencil)
            continue;

        stencil = value;

        glx_renderer->glXGetFBConfigAttrib(dpy, fbconfigs[i],
                                           GLX_BIND_TO_MIPMAP_TEXTURE_EXT, &value);
        if (value < mipmap)
            continue;

        mipmap = value;

        *fbconfig_ret = fbconfigs[i];
        *can_mipmap_ret = mipmap;
        found = true;
    }

    if (n_elements)
        XFree(fbconfigs);

    glx_display->glx_cached_configs[spare_cache_slot].depth = depth;
    glx_display->glx_cached_configs[spare_cache_slot].found = found;
    glx_display->glx_cached_configs[spare_cache_slot].fb_config = *fbconfig_ret;
    glx_display->glx_cached_configs[spare_cache_slot].can_mipmap = mipmap;

    return found;
}

static bool
try_create_glx_pixmap(cg_device_t *dev,
                      cg_texture_pixmap_x11_t *tex_pixmap,
                      bool mipmap)
{
    cg_texture_pixmap_glx_t *glx_tex_pixmap = tex_pixmap->winsys;
    cg_renderer_t *renderer;
    cg_xlib_renderer_t *xlib_renderer;
    cg_glx_renderer_t *glx_renderer;
    Display *dpy;
    /* We have to initialize this *opaque* variable because gcc tries to
     * be too smart for its own good and warns that the variable may be
     * used uninitialized otherwise. */
    GLXFBConfig fb_config = (GLXFBConfig)0;
    int attribs[7];
    int i = 0;
    GLenum target;
    cg_xlib_trap_state_t trap_state;

    int depth = tex_pixmap->depth;
    Visual *visual = tex_pixmap->visual;

    renderer = dev->display->renderer;
    xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    glx_renderer = renderer->winsys;
    dpy = xlib_renderer->xdpy;

    if (!get_fbconfig_for_depth(dev, depth, &fb_config, &glx_tex_pixmap->can_mipmap)) {
        CG_NOTE(
            TEXTURE_PIXMAP, "No suitable FBConfig found for depth %i", depth);
        return false;
    }

    target = GLX_TEXTURE_2D_EXT;

    if (!glx_tex_pixmap->can_mipmap)
        mipmap = false;

    attribs[i++] = GLX_TEXTURE_FORMAT_EXT;

    /* Check whether an alpha channel is used by comparing the total
     * number of 1-bits in color masks against the color depth requested
     * by the client.
     */
    if (_cg_util_popcountl(visual->red_mask | visual->green_mask |
                           visual->blue_mask) == depth)
        attribs[i++] = GLX_TEXTURE_FORMAT_RGB_EXT;
    else
        attribs[i++] = GLX_TEXTURE_FORMAT_RGBA_EXT;

    attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
    attribs[i++] = mipmap;

    attribs[i++] = GLX_TEXTURE_TARGET_EXT;
    attribs[i++] = target;

    attribs[i++] = None;

    /* We need to trap errors from glXCreatePixmap because it can
     * sometimes fail during normal usage. For example on NVidia it gets
     * upset if you try to create two GLXPixmaps for the same drawable.
     */

    _cg_xlib_renderer_trap_errors(renderer, &trap_state);

    glx_tex_pixmap->glx_pixmap = glx_renderer->glXCreatePixmap(
        dpy, fb_config, tex_pixmap->pixmap, attribs);
    glx_tex_pixmap->has_mipmap_space = mipmap;

    XSync(dpy, False);

    if (_cg_xlib_renderer_untrap_errors(renderer, &trap_state)) {
        CG_NOTE(TEXTURE_PIXMAP, "Failed to create pixmap for %p", tex_pixmap);
        _cg_xlib_renderer_trap_errors(renderer, &trap_state);
        glx_renderer->glXDestroyPixmap(dpy, glx_tex_pixmap->glx_pixmap);
        XSync(dpy, False);
        _cg_xlib_renderer_untrap_errors(renderer, &trap_state);

        glx_tex_pixmap->glx_pixmap = None;
        return false;
    }

    return true;
}

static bool
_cg_winsys_texture_pixmap_x11_create(cg_texture_pixmap_x11_t *tex_pixmap)
{
    cg_texture_pixmap_glx_t *glx_tex_pixmap;
    cg_device_t *dev = CG_TEXTURE(tex_pixmap)->dev;

    if (!_cg_winsys_has_feature(dev, CG_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP)) {
        tex_pixmap->winsys = NULL;
        return false;
    }

    glx_tex_pixmap = c_new0(cg_texture_pixmap_glx_t, 1);

    glx_tex_pixmap->glx_pixmap = None;
    glx_tex_pixmap->can_mipmap = false;
    glx_tex_pixmap->has_mipmap_space = false;

    glx_tex_pixmap->glx_tex = NULL;

    glx_tex_pixmap->bind_tex_image_queued = true;
    glx_tex_pixmap->pixmap_bound = false;

    tex_pixmap->winsys = glx_tex_pixmap;

    if (!try_create_glx_pixmap(dev, tex_pixmap, false)) {
        tex_pixmap->winsys = NULL;
        c_free(glx_tex_pixmap);
        return false;
    }

    return true;
}

static void
free_glx_pixmap(cg_device_t *dev,
                cg_texture_pixmap_glx_t *glx_tex_pixmap)
{
    cg_xlib_trap_state_t trap_state;
    cg_renderer_t *renderer;
    cg_xlib_renderer_t *xlib_renderer;
    cg_glx_renderer_t *glx_renderer;

    renderer = dev->display->renderer;
    xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    glx_renderer = renderer->winsys;

    if (glx_tex_pixmap->pixmap_bound)
        glx_renderer->glXReleaseTexImage(xlib_renderer->xdpy,
                                         glx_tex_pixmap->glx_pixmap,
                                         GLX_FRONT_LEFT_EXT);

    /* FIXME - we need to trap errors and synchronize here because
     * of ordering issues between the XPixmap destruction and the
     * GLXPixmap destruction.
     *
     * If the X pixmap is destroyed, the GLX pixmap is destroyed as
     * well immediately, and thus, when CGlib calls glXDestroyPixmap()
     * it'll cause a BadDrawable error.
     *
     * this is technically a bug in the X server, which should not
     * destroy either pixmaps until the call to glXDestroyPixmap(); so
     * at some point we should revisit this code and remove the
     * trap+sync after verifying that the destruction is indeed safe.
     *
     * for reference, see:
     *   http://bugzilla.clutter-project.org/show_bug.cgi?id=2324
     */
    _cg_xlib_renderer_trap_errors(renderer, &trap_state);
    glx_renderer->glXDestroyPixmap(xlib_renderer->xdpy,
                                   glx_tex_pixmap->glx_pixmap);
    XSync(xlib_renderer->xdpy, False);
    _cg_xlib_renderer_untrap_errors(renderer, &trap_state);

    glx_tex_pixmap->glx_pixmap = None;
    glx_tex_pixmap->pixmap_bound = false;
}

static void
_cg_winsys_texture_pixmap_x11_free(cg_texture_pixmap_x11_t *tex_pixmap)
{
    cg_texture_pixmap_glx_t *glx_tex_pixmap;

    if (!tex_pixmap->winsys)
        return;

    glx_tex_pixmap = tex_pixmap->winsys;

    free_glx_pixmap(CG_TEXTURE(tex_pixmap)->dev, glx_tex_pixmap);

    if (glx_tex_pixmap->glx_tex)
        cg_object_unref(glx_tex_pixmap->glx_tex);

    tex_pixmap->winsys = NULL;
    c_free(glx_tex_pixmap);
}

static bool
_cg_winsys_texture_pixmap_x11_update(cg_texture_pixmap_x11_t *tex_pixmap,
                                     bool needs_mipmap)
{
    cg_texture_t *tex = CG_TEXTURE(tex_pixmap);
    cg_device_t *dev = CG_TEXTURE(tex_pixmap)->dev;
    cg_texture_pixmap_glx_t *glx_tex_pixmap = tex_pixmap->winsys;
    cg_glx_renderer_t *glx_renderer;

    /* If we don't have a GLX pixmap then fallback */
    if (glx_tex_pixmap->glx_pixmap == None)
        return false;

    glx_renderer = dev->display->renderer->winsys;

    /* Lazily create a texture to hold the pixmap */
    if (glx_tex_pixmap->glx_tex == NULL) {
        cg_pixel_format_t texture_format;
        cg_error_t *error = NULL;

        texture_format =
            (tex_pixmap->depth >= 32 ? CG_PIXEL_FORMAT_RGBA_8888_PRE
             : CG_PIXEL_FORMAT_RGB_888);

        glx_tex_pixmap->glx_tex = CG_TEXTURE(
            cg_texture_2d_new_with_size(dev, tex->width, tex->height));

        _cg_texture_set_internal_format(tex, texture_format);

        if (cg_texture_allocate(glx_tex_pixmap->glx_tex, &error))
            CG_NOTE(TEXTURE_PIXMAP, "Created a texture 2d for %p", tex_pixmap);
        else {
            CG_NOTE(TEXTURE_PIXMAP,
                    "Falling back for %p because a "
                    "texture 2d could not be created: %s",
                    tex_pixmap,
                    error->message);
            cg_error_free(error);
            free_glx_pixmap(dev, glx_tex_pixmap);
            return false;
        }
    }

    if (needs_mipmap) {
        /* If we can't support mipmapping then temporarily fallback */
        if (!glx_tex_pixmap->can_mipmap)
            return false;

        /* Recreate the GLXPixmap if it wasn't previously created with a
         * mipmap tree */
        if (!glx_tex_pixmap->has_mipmap_space) {
            free_glx_pixmap(dev, glx_tex_pixmap);

            CG_NOTE(TEXTURE_PIXMAP,
                    "Recreating GLXPixmap with mipmap "
                    "support for %p",
                    tex_pixmap);
            if (!try_create_glx_pixmap(dev, tex_pixmap, true)) {
                /* If the pixmap failed then we'll permanently fallback
                 * to using XImage. This shouldn't happen. */
                CG_NOTE(TEXTURE_PIXMAP,
                        "Falling back to XGetImage "
                        "updates for %p because creating the GLXPixmap "
                        "with mipmap support failed",
                        tex_pixmap);

                if (glx_tex_pixmap->glx_tex)
                    cg_object_unref(glx_tex_pixmap->glx_tex);
                return false;
            }

            glx_tex_pixmap->bind_tex_image_queued = true;
        }
    }

    if (glx_tex_pixmap->bind_tex_image_queued) {
        GLuint gl_handle, gl_target;
        cg_xlib_renderer_t *xlib_renderer =
            _cg_xlib_renderer_get_data(dev->display->renderer);

        cg_texture_get_gl_texture(
            glx_tex_pixmap->glx_tex, &gl_handle, &gl_target);

        CG_NOTE(TEXTURE_PIXMAP, "Rebinding GLXPixmap for %p", tex_pixmap);

        _cg_bind_gl_texture_transient(gl_target, gl_handle, false);

        if (glx_tex_pixmap->pixmap_bound)
            glx_renderer->glXReleaseTexImage(xlib_renderer->xdpy,
                                             glx_tex_pixmap->glx_pixmap,
                                             GLX_FRONT_LEFT_EXT);

        glx_renderer->glXBindTexImage(xlib_renderer->xdpy,
                                      glx_tex_pixmap->glx_pixmap,
                                      GLX_FRONT_LEFT_EXT,
                                      NULL);

        /* According to the recommended usage in the spec for
         * GLX_EXT_texture_pixmap we should release the texture after
         * we've finished drawing with it and it is undefined what
         * happens if you render to a pixmap that is bound to a texture.
         * However that would require the texture backend to know when
         * CGlib has finished painting and it may be more expensive to
         * keep unbinding the texture. Leaving it bound appears to work
         * on Mesa and NVidia drivers and it is also what Compiz does so
         * it is probably ok */

        glx_tex_pixmap->bind_tex_image_queued = false;
        glx_tex_pixmap->pixmap_bound = true;

        _cg_texture_2d_externally_modified(glx_tex_pixmap->glx_tex);
    }

    return true;
}

static void
_cg_winsys_texture_pixmap_x11_damage_notify(cg_texture_pixmap_x11_t *tex_pixmap)
{
    cg_texture_pixmap_glx_t *glx_tex_pixmap = tex_pixmap->winsys;

    glx_tex_pixmap->bind_tex_image_queued = true;
}

static cg_texture_t *
_cg_winsys_texture_pixmap_x11_get_texture(cg_texture_pixmap_x11_t *tex_pixmap)
{
    cg_texture_pixmap_glx_t *glx_tex_pixmap = tex_pixmap->winsys;

    return glx_tex_pixmap->glx_tex;
}

static cg_winsys_vtable_t _cg_winsys_vtable = {
    .id = CG_WINSYS_ID_GLX,
    .name = "GLX",
    .constraints =
        (CG_RENDERER_CONSTRAINT_USES_X11 | CG_RENDERER_CONSTRAINT_USES_XLIB),
    .renderer_get_proc_address = _cg_winsys_renderer_get_proc_address,
    .renderer_connect = _cg_winsys_renderer_connect,
    .renderer_disconnect = _cg_winsys_renderer_disconnect,
    .renderer_outputs_changed = _cg_winsys_renderer_outputs_changed,
    .display_setup = _cg_winsys_display_setup,
    .display_destroy = _cg_winsys_display_destroy,
    .device_init = _cg_winsys_device_init,
    .device_deinit = _cg_winsys_device_deinit,
    .xlib_get_visual_info = _cg_winsys_xlib_get_visual_info,
    .onscreen_init = _cg_winsys_onscreen_init,
    .onscreen_deinit = _cg_winsys_onscreen_deinit,
    .onscreen_bind = _cg_winsys_onscreen_bind,
    .onscreen_swap_buffers_with_damage =
        _cg_winsys_onscreen_swap_buffers_with_damage,
    .onscreen_swap_region = _cg_winsys_onscreen_swap_region,
    .onscreen_get_buffer_age = _cg_winsys_onscreen_get_buffer_age,
    .onscreen_update_swap_throttled = _cg_winsys_onscreen_update_swap_throttled,
    .onscreen_x11_get_window_xid = _cg_winsys_onscreen_x11_get_window_xid,
    .onscreen_set_visibility = _cg_winsys_onscreen_set_visibility,
    .onscreen_set_resizable = _cg_winsys_onscreen_set_resizable,

    /* X11 tfp support... */
    /* XXX: instead of having a rather monolithic winsys vtable we could
     * perhaps look for a way to separate these... */
    .texture_pixmap_x11_create = _cg_winsys_texture_pixmap_x11_create,
    .texture_pixmap_x11_free = _cg_winsys_texture_pixmap_x11_free,
    .texture_pixmap_x11_update = _cg_winsys_texture_pixmap_x11_update,
    .texture_pixmap_x11_damage_notify =
        _cg_winsys_texture_pixmap_x11_damage_notify,
    .texture_pixmap_x11_get_texture = _cg_winsys_texture_pixmap_x11_get_texture,
};

/* XXX: we use a function because no doubt someone will complain
 * about using c99 member initializers because they aren't portable
 * to windows. We want to avoid having to rigidly follow the real
 * order of members since some members are #ifdefd and we'd have
 * to mirror the #ifdefing to add padding etc. For any winsys that
 * can assume the platform has a sane compiler then we can just use
 * c99 initializers for insane platforms they can initialize
 * the members by name in a function.
 */
const cg_winsys_vtable_t *
_cg_winsys_glx_get_vtable(void)
{
    return &_cg_winsys_vtable;
}
