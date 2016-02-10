/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include "cg-xlib-renderer.h"
#include "cg-util.h"
#include "cg-object.h"

#include "cg-output-private.h"
#include "cg-renderer-private.h"
#include "cg-xlib-renderer-private.h"
#include "cg-x11-renderer-private.h"
#include "cg-winsys-private.h"
#include "cg-error-private.h"
#include "cg-loop-private.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/sync.h>

#include <stdlib.h>
#include <string.h>

static char *_cg_x11_display_name = NULL;
static c_llist_t *_cg_xlib_renderers = NULL;

static void
destroy_xlib_renderer_data(void *user_data)
{
    c_slice_free(cg_xlib_renderer_t, user_data);
}

cg_xlib_renderer_t *
_cg_xlib_renderer_get_data(cg_renderer_t *renderer)
{
    static cg_user_data_key_t key;
    cg_xlib_renderer_t *data;

    /* Constructs a cg_xlib_renderer_t struct on demand and attaches it to
       the object using user data. It's done this way instead of using a
       subclassing hierarchy in the winsys data because all EGL winsys's
       need the EGL winsys data but only one of them wants the Xlib
       data. */

    data = cg_object_get_user_data(CG_OBJECT(renderer), &key);

    if (data == NULL) {
        data = c_slice_new0(cg_xlib_renderer_t);

        cg_object_set_user_data(
            CG_OBJECT(renderer), &key, data, destroy_xlib_renderer_data);
    }

    return data;
}

static void
register_xlib_renderer(cg_renderer_t *renderer)
{
    c_llist_t *l;

    for (l = _cg_xlib_renderers; l; l = l->next)
        if (l->data == renderer)
            return;

    _cg_xlib_renderers = c_llist_prepend(_cg_xlib_renderers, renderer);
}

static void
unregister_xlib_renderer(cg_renderer_t *renderer)
{
    _cg_xlib_renderers = c_llist_remove(_cg_xlib_renderers, renderer);
}

static cg_renderer_t *
get_renderer_for_xdisplay(Display *xdpy)
{
    c_llist_t *l;

    for (l = _cg_xlib_renderers; l; l = l->next) {
        cg_renderer_t *renderer = l->data;
        cg_xlib_renderer_t *xlib_renderer =
            _cg_xlib_renderer_get_data(renderer);

        if (xlib_renderer->xdpy == xdpy)
            return renderer;
    }

    return NULL;
}

static int
error_handler(Display *xdpy, XErrorEvent *error)
{
    cg_renderer_t *renderer;
    cg_xlib_renderer_t *xlib_renderer;

    renderer = get_renderer_for_xdisplay(xdpy);

    xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    c_assert(xlib_renderer->trap_state);

    xlib_renderer->trap_state->trapped_error_code = error->error_code;

    return 0;
}

void
_cg_xlib_renderer_trap_errors(cg_renderer_t *renderer,
                              cg_xlib_trap_state_t *state)
{
    cg_xlib_renderer_t *xlib_renderer;

    xlib_renderer = _cg_xlib_renderer_get_data(renderer);

    state->trapped_error_code = 0;
    state->old_error_handler = XSetErrorHandler(error_handler);

    state->old_state = xlib_renderer->trap_state;
    xlib_renderer->trap_state = state;
}

int
_cg_xlib_renderer_untrap_errors(cg_renderer_t *renderer,
                                cg_xlib_trap_state_t *state)
{
    cg_xlib_renderer_t *xlib_renderer;

    xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    c_assert(state == xlib_renderer->trap_state);

    XSetErrorHandler(state->old_error_handler);

    xlib_renderer->trap_state = state->old_state;

    return state->trapped_error_code;
}

static Display *
assert_xlib_display(cg_renderer_t *renderer, cg_error_t **error)
{
    Display *xdpy = cg_xlib_renderer_get_foreign_display(renderer);
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);

    /* A foreign display may have already been set... */
    if (xdpy) {
        xlib_renderer->xdpy = xdpy;
        return xdpy;
    }

    xdpy = XOpenDisplay(_cg_x11_display_name);
    if (xdpy == NULL) {
        _cg_set_error(error,
                      CG_RENDERER_ERROR,
                      CG_RENDERER_ERROR_XLIB_DISPLAY_OPEN,
                      "Failed to open X Display %s",
                      _cg_x11_display_name);
        return NULL;
    }

    xlib_renderer->xdpy = xdpy;
    return xdpy;
}

static int
compare_outputs(cg_output_t *a, cg_output_t *b)
{
    return strcmp(a->name, b->name);
}

#define CSO(X) CG_SUBPIXEL_ORDER_##X
static cg_subpixel_order_t subpixel_map[6][6] = {
    { CSO(UNKNOWN),        CSO(NONE),         CSO(HORIZONTAL_RGB),
      CSO(HORIZONTAL_BGR), CSO(VERTICAL_RGB), CSO(VERTICAL_BGR) }, /* 0 */
    { CSO(UNKNOWN),      CSO(NONE),           CSO(VERTICAL_RGB),
      CSO(VERTICAL_BGR), CSO(HORIZONTAL_BGR), CSO(HORIZONTAL_RGB) }, /* 90 */
    { CSO(UNKNOWN),        CSO(NONE),         CSO(HORIZONTAL_BGR),
      CSO(HORIZONTAL_RGB), CSO(VERTICAL_BGR), CSO(VERTICAL_RGB) }, /* 180 */
    { CSO(UNKNOWN),      CSO(NONE),           CSO(VERTICAL_BGR),
      CSO(VERTICAL_RGB), CSO(HORIZONTAL_RGB), CSO(HORIZONTAL_BGR) }, /* 270 */
    {
        CSO(UNKNOWN),        CSO(NONE),         CSO(HORIZONTAL_BGR),
        CSO(HORIZONTAL_RGB), CSO(VERTICAL_RGB), CSO(VERTICAL_BGR)
    }, /* Reflect_X */
    {
        CSO(UNKNOWN),        CSO(NONE),         CSO(HORIZONTAL_RGB),
        CSO(HORIZONTAL_BGR), CSO(VERTICAL_BGR), CSO(VERTICAL_RGB)
    }, /* Reflect_Y */
};
#undef CSO

static void
update_outputs(cg_renderer_t *renderer, bool notify)
{
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    XRRScreenResources *resources;
    cg_xlib_trap_state_t state;
    bool error = false;
    c_llist_t *new_outputs = NULL;
    c_llist_t *l, *m;
    bool changed = false;
    int i;

    xlib_renderer->outputs_update_serial = XNextRequest(xlib_renderer->xdpy);

    resources = XRRGetScreenResources(xlib_renderer->xdpy,
                                      DefaultRootWindow(xlib_renderer->xdpy));

    _cg_xlib_renderer_trap_errors(renderer, &state);

    for (i = 0; resources && i < resources->ncrtc && !error; i++) {
        XRRCrtcInfo *crtc_info = NULL;
        XRROutputInfo *output_info = NULL;
        cg_output_t *output;
        float refresh_rate = 0;
        int j;

        crtc_info =
            XRRGetCrtcInfo(xlib_renderer->xdpy, resources, resources->crtcs[i]);
        if (crtc_info == NULL) {
            error = true;
            goto next;
        }

        if (crtc_info->mode == None)
            goto next;

        for (j = 0; j < resources->nmode; j++) {
            if (resources->modes[j].id == crtc_info->mode)
                refresh_rate = (resources->modes[j].dotClock /
                                ((float)resources->modes[j].hTotal *
                                 resources->modes[j].vTotal));
        }

        output_info = XRRGetOutputInfo(
            xlib_renderer->xdpy, resources, crtc_info->outputs[0]);
        if (output_info == NULL) {
            error = true;
            goto next;
        }

        output = _cg_output_new(output_info->name);
        output->x = crtc_info->x;
        output->y = crtc_info->y;
        output->width = crtc_info->width;
        output->height = crtc_info->height;
        if ((crtc_info->rotation & (RR_Rotate_90 | RR_Rotate_270)) != 0) {
            output->mm_width = output_info->mm_height;
            output->mm_height = output_info->mm_width;
        } else {
            output->mm_width = output_info->mm_width;
            output->mm_height = output_info->mm_height;
        }

        output->refresh_rate = refresh_rate;

        switch (output_info->subpixel_order) {
        case SubPixelUnknown:
        default:
            output->subpixel_order = CG_SUBPIXEL_ORDER_UNKNOWN;
            break;
        case SubPixelNone:
            output->subpixel_order = CG_SUBPIXEL_ORDER_NONE;
            break;
        case SubPixelHorizontalRGB:
            output->subpixel_order = CG_SUBPIXEL_ORDER_HORIZONTAL_RGB;
            break;
        case SubPixelHorizontalBGR:
            output->subpixel_order = CG_SUBPIXEL_ORDER_HORIZONTAL_BGR;
            break;
        case SubPixelVerticalRGB:
            output->subpixel_order = CG_SUBPIXEL_ORDER_VERTICAL_RGB;
            break;
        case SubPixelVerticalBGR:
            output->subpixel_order = CG_SUBPIXEL_ORDER_VERTICAL_BGR;
            break;
        }

        output->subpixel_order = CG_SUBPIXEL_ORDER_HORIZONTAL_RGB;

        /* Handle the effect of rotation and reflection on subpixel order (ugh)
         */
        for (j = 0; j < 6; j++) {
            if ((crtc_info->rotation & (1 << j)) != 0)
                output->subpixel_order =
                    subpixel_map[j][output->subpixel_order];
        }

        new_outputs = c_llist_prepend(new_outputs, output);

next:
        if (crtc_info != NULL)
            XFree(crtc_info);

        if (output_info != NULL)
            XFree(output_info);
    }

    XFree(resources);

    if (!error) {
        new_outputs = c_llist_sort(new_outputs,
                                  (c_compare_func_t)compare_outputs);

        l = new_outputs;
        m = renderer->outputs;

        while (l || m) {
            int cmp;
            cg_output_t *output_l = l ? (cg_output_t *)l->data : NULL;
            cg_output_t *output_m = m ? (cg_output_t *)m->data : NULL;

            if (l && m)
                cmp = compare_outputs(output_l, output_m);
            else if (l)
                cmp = -1;
            else
                cmp = 1;

            if (cmp == 0) {
                c_llist_t *m_next = m->next;

                if (!_cg_output_values_equal(output_l, output_m)) {
                    renderer->outputs =
                        c_llist_remove_link(renderer->outputs, m);
                    renderer->outputs = c_llist_insert_before(
                        renderer->outputs, m_next, output_l);
                    cg_object_ref(output_l);

                    changed = true;
                }

                l = l->next;
                m = m_next;
            } else if (cmp < 0) {
                renderer->outputs =
                    c_llist_insert_before(renderer->outputs, m, output_l);
                cg_object_ref(output_l);
                changed = true;
                l = l->next;
            } else {
                c_llist_t *m_next = m->next;
                renderer->outputs = c_llist_remove_link(renderer->outputs, m);
                changed = true;
                m = m_next;
            }
        }
    }

    c_llist_free_full(new_outputs, (c_destroy_func_t)cg_object_unref);
    _cg_xlib_renderer_untrap_errors(renderer, &state);

    if (changed) {
        const cg_winsys_vtable_t *winsys = renderer->winsys_vtable;

        if (notify)
            CG_NOTE(WINSYS, "Outputs changed:");
        else
            CG_NOTE(WINSYS, "Outputs:");

        for (l = renderer->outputs; l; l = l->next) {
            cg_output_t *output = l->data;
            const char *subpixel_string;

            switch (output->subpixel_order) {
            case CG_SUBPIXEL_ORDER_UNKNOWN:
            default:
                subpixel_string = "unknown";
                break;
            case CG_SUBPIXEL_ORDER_NONE:
                subpixel_string = "none";
                break;
            case CG_SUBPIXEL_ORDER_HORIZONTAL_RGB:
                subpixel_string = "horizontal_rgb";
                break;
            case CG_SUBPIXEL_ORDER_HORIZONTAL_BGR:
                subpixel_string = "horizontal_bgr";
                break;
            case CG_SUBPIXEL_ORDER_VERTICAL_RGB:
                subpixel_string = "vertical_rgb";
                break;
            case CG_SUBPIXEL_ORDER_VERTICAL_BGR:
                subpixel_string = "vertical_bgr";
                break;
            }

            CG_NOTE(WINSYS,
                    " %10s: +%d+%dx%dx%d mm=%dx%d dpi=%.1fx%.1f "
                    "subpixel_order=%s refresh_rate=%.3f",
                    output->name,
                    output->x,
                    output->y,
                    output->width,
                    output->height,
                    output->mm_width,
                    output->mm_height,
                    output->width / (output->mm_width / 25.4),
                    output->height / (output->mm_height / 25.4),
                    subpixel_string,
                    output->refresh_rate);
        }

        if (notify && winsys->renderer_outputs_changed != NULL)
            winsys->renderer_outputs_changed(renderer);
    }
}

static cg_filter_return_t
randr_filter(XEvent *event, void *data)
{
    cg_renderer_t *renderer = data;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;

    if (x11_renderer->randr_base != -1 &&
        (event->xany.type == x11_renderer->randr_base + RRScreenChangeNotify ||
         event->xany.type == x11_renderer->randr_base + RRNotify) &&
        event->xany.serial >= xlib_renderer->outputs_update_serial)
        update_outputs(renderer, true);

    return CG_FILTER_CONTINUE;
}

static int64_t
prepare_xlib_events_timeout(void *user_data)
{
    cg_renderer_t *renderer = user_data;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);

    return XPending(xlib_renderer->xdpy) ? 0 : -1;
}

static void
dispatch_xlib_events(void *user_data, int revents)
{
    cg_renderer_t *renderer = user_data;
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);

    if (renderer->xlib_enable_event_retrieval)
        while (XPending(xlib_renderer->xdpy)) {
            XEvent xevent;

            XNextEvent(xlib_renderer->xdpy, &xevent);

            cg_xlib_renderer_handle_event(renderer, &xevent);
        }
}

/* What features does the window manager support? */
static void
query_net_supported(cg_renderer_t *renderer)
{
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;
    Atom actual_type;
    int actual_format;
    unsigned long n_atoms;
    unsigned long remaining;
    unsigned char *data;
    Atom *atoms;
    int status = XGetWindowProperty(xlib_renderer->xdpy,
                                    DefaultRootWindow(xlib_renderer->xdpy),
                                    XInternAtom(xlib_renderer->xdpy,
                                                "_NET_SUPPORTED", False),
                                    0, /* start */
                                    LONG_MAX, /* length to retrieve (all) */
                                    False, /* don't delete */
                                    XA_ATOM, /* expect an array of atoms */
                                    &actual_type, /* actual type */
                                    &actual_format, /* actual format */
                                    &n_atoms,
                                    &remaining,
                                    &data);
    if (status == Success) {
        Atom net_wm_frame_drawn =
            XInternAtom(xlib_renderer->xdpy, "_NET_WM_FRAME_DRAWN", False);

        if (remaining != 0) {
            c_warning("Failed to read _NET_SUPPORTED property");
            return;
        }

        if (actual_type != XA_ATOM) {
            c_warning("Spurious type for _NET_SUPPORTED property");
            return;
        }

        if (actual_format != 32) {
            c_warning("Spurious format for _NET_SUPPORTED property");
            return;
        }

        atoms = (Atom *)data;
        for (unsigned long i = 0; i < n_atoms; i++) {
            if (atoms[i] == net_wm_frame_drawn) {
                x11_renderer->net_wm_frame_drawn_supported = true;
            }
        }

        XFree(data);
    }
}

bool
_cg_xlib_renderer_connect(cg_renderer_t *renderer, cg_error_t **error)
{
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);
    cg_x11_renderer_t *x11_renderer = (cg_x11_renderer_t *)xlib_renderer;
    int damage_error;
    int randr_error;

    if (!assert_xlib_display(renderer, error))
        return false;

    if (getenv("CG_X11_SYNC"))
        XSynchronize(xlib_renderer->xdpy, true);

    query_net_supported(renderer);

    if (!XSyncQueryExtension(xlib_renderer->xdpy,
                             &x11_renderer->xsync_event,
                             &x11_renderer->xsync_error))
    {
        c_warning("X11 missing required XSync extension");
    }

    if (!XSyncInitialize(xlib_renderer->xdpy,
                         &x11_renderer->xsync_major,
                         &x11_renderer->xsync_minor))
    {
        c_warning("Missing required XSync support");
    }

    /* Check whether damage events are supported on this display */
    if (!XDamageQueryExtension(
            xlib_renderer->xdpy, &x11_renderer->damage_base, &damage_error))
        x11_renderer->damage_base = -1;

    /* Check whether randr is supported on this display */
    if (!XRRQueryExtension(
            xlib_renderer->xdpy, &x11_renderer->randr_base, &randr_error))
        x11_renderer->randr_base = -1;

    xlib_renderer->trap_state = NULL;

    if (renderer->xlib_enable_event_retrieval) {
        _cg_loop_add_fd(renderer,
                        ConnectionNumber(xlib_renderer->xdpy),
                        CG_POLL_FD_EVENT_IN,
                        prepare_xlib_events_timeout,
                        dispatch_xlib_events,
                        renderer);
    }

    XRRSelectInput(xlib_renderer->xdpy,
                   DefaultRootWindow(xlib_renderer->xdpy),
                   RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask |
                   RROutputPropertyNotifyMask);
    update_outputs(renderer, false);

    register_xlib_renderer(renderer);

    cg_xlib_renderer_add_filter(renderer, randr_filter, renderer);

    return true;
}

void
_cg_xlib_renderer_disconnect(cg_renderer_t *renderer)
{
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);

    c_llist_free_full(renderer->outputs, (c_destroy_func_t)cg_object_unref);
    renderer->outputs = NULL;

    if (!renderer->foreign_xdpy && xlib_renderer->xdpy)
        XCloseDisplay(xlib_renderer->xdpy);

    unregister_xlib_renderer(renderer);
}

Display *
cg_xlib_renderer_get_display(cg_renderer_t *renderer)
{
    cg_xlib_renderer_t *xlib_renderer;

    c_return_val_if_fail(cg_is_renderer(renderer), NULL);

    xlib_renderer = _cg_xlib_renderer_get_data(renderer);

    return xlib_renderer->xdpy;
}

cg_filter_return_t
cg_xlib_renderer_handle_event(cg_renderer_t *renderer,
                              XEvent *event)
{
    return _cg_renderer_handle_native_event(renderer, event);
}

void
cg_xlib_renderer_add_filter(cg_renderer_t *renderer,
                            cg_xlib_filter_func_t func,
                            void *data)
{
    _cg_renderer_add_native_filter(
        renderer, (cg_native_filter_func_t)func, data);
}

void
cg_xlib_renderer_remove_filter(cg_renderer_t *renderer,
                               cg_xlib_filter_func_t func,
                               void *data)
{
    _cg_renderer_remove_native_filter(
        renderer, (cg_native_filter_func_t)func, data);
}

int64_t
_cg_xlib_renderer_get_dispatch_timeout(cg_renderer_t *renderer)
{
    cg_xlib_renderer_t *xlib_renderer = _cg_xlib_renderer_get_data(renderer);

    if (renderer->xlib_enable_event_retrieval) {
        if (XPending(xlib_renderer->xdpy))
            return 0;
        else
            return -1;
    } else
        return -1;
}

cg_output_t *
_cg_xlib_renderer_output_for_rectangle(
    cg_renderer_t *renderer, int x, int y, int width, int height)
{
    int max_overlap = 0;
    cg_output_t *max_overlapped = NULL;
    c_llist_t *l;
    int xa1 = x, xa2 = x + width;
    int ya1 = y, ya2 = y + height;

    for (l = renderer->outputs; l; l = l->next) {
        cg_output_t *output = l->data;
        int xb1 = output->x, xb2 = output->x + output->width;
        int yb1 = output->y, yb2 = output->y + output->height;

        int overlap_x = MIN(xa2, xb2) - MAX(xa1, xb1);
        int overlap_y = MIN(ya2, yb2) - MAX(ya1, yb1);

        if (overlap_x > 0 && overlap_y > 0) {
            int overlap = overlap_x * overlap_y;
            if (overlap > max_overlap) {
                max_overlap = overlap;
                max_overlapped = output;
            }
        }
    }

    return max_overlapped;
}

int
_cg_xlib_renderer_get_damage_base(cg_renderer_t *renderer)
{
    cg_x11_renderer_t *x11_renderer =
        (cg_x11_renderer_t *)_cg_xlib_renderer_get_data(renderer);
    return x11_renderer->damage_base;
}
