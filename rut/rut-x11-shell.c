/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2014  Intel Corporation
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

#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/cursorfont.h>

#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>

#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon-compose.h>

#include <cglib/cglib.h>
#include <cglib/cg-xlib.h>

#include "rut-shell.h"
#include "rut-x11-shell.h"

#include "edid-parse.h"

static int32_t
rut_x11_key_event_get_keysym(rut_input_event_t *event)
{
    rut_x11_event_t *rut_x11_event = event->native;

    return rut_x11_event->keysym;
}

static rut_key_event_action_t
rut_x11_key_event_get_action(rut_input_event_t *event)
{
    rut_x11_event_t *rut_x11_event = event->native;
    XIEvent *xi2event = rut_x11_event->xcookie.data;

    switch (xi2event->evtype) {
    case XI_KeyPress:
        return RUT_KEY_EVENT_ACTION_DOWN;
    case XI_KeyRelease:
        return RUT_KEY_EVENT_ACTION_UP;
    default:
        c_warn_if_reached();
        return 0;
    }
}

static rut_modifier_state_t
rut_x11_key_event_get_modifier_state(rut_input_event_t *event)
{
    rut_x11_event_t *rut_x11_event = event->native;
    return rut_x11_event->mod_state;
}

static rut_motion_event_action_t
rut_x11_motion_event_get_action(rut_input_event_t *event)
{
    rut_x11_event_t *rut_x11_event = event->native;
    XIEvent *xi2event = rut_x11_event->xcookie.data;

    switch (xi2event->evtype) {
        case XI_ButtonPress:
            return RUT_MOTION_EVENT_ACTION_DOWN;
        case XI_ButtonRelease:
            return RUT_MOTION_EVENT_ACTION_UP;
        case XI_Motion:
            return RUT_MOTION_EVENT_ACTION_MOVE;
        case XI_TouchBegin:
        case XI_TouchUpdate:
        case XI_TouchEnd:
            /* FIXME: support touch events */
            c_warn_if_reached();
            return 0;
            break;
    default:
        c_warn_if_reached(); /* Not a motion event */
        return 0;
    }
}

static rut_button_state_t
rut_x11_motion_event_get_button(rut_input_event_t *event)
{
    rut_x11_event_t *rut_x11_event = event->native;
    XIEvent *xi2event = rut_x11_event->xcookie.data;
    XIDeviceEvent *xi2_dev_event = rut_x11_event->xcookie.data;

    c_return_val_if_fail((xi2event->evtype == XI_ButtonPress ||
                          xi2event->evtype == XI_ButtonRelease) &&
                         xi2_dev_event->detail > 0 && xi2_dev_event->detail < 4,
                         0);

    switch(xi2_dev_event->detail) {
    case 1:
        return RUT_BUTTON_STATE_1;
    case 2:
        return RUT_BUTTON_STATE_2;
    case 3:
        return RUT_BUTTON_STATE_3;
    default:
        c_warn_if_reached();
        return 0;
    }
}

static rut_button_state_t
button_state_for_xi2_button_mask(XIButtonState *state)
{
    int max = MIN(state->mask_len * 8, sizeof(rut_button_state_t) * 8) - 1;
    rut_button_state_t rut_state = 0;

    for (int i = 0; i < max; i++) {
        if (XIMaskIsSet(state->mask, i))
            rut_state |= 1<<i;
    }

    return rut_state;
}

static rut_button_state_t
rut_x11_motion_event_get_button_state(rut_input_event_t *event)
{
    rut_x11_event_t *rut_x11_event = event->native;
    XIEvent *xi2event = rut_x11_event->xcookie.data;
    XIDeviceEvent *xi2_dev_event = rut_x11_event->xcookie.data;
    rut_button_state_t rut_state =
        button_state_for_xi2_button_mask(&xi2_dev_event->buttons);

    if (xi2event->evtype == XI_ButtonPress)
        rut_state |= rut_x11_motion_event_get_button(event);
    if (xi2event->evtype == XI_ButtonRelease)
        rut_state &= ~rut_x11_motion_event_get_button(event);

    return rut_state;
}

static rut_modifier_state_t
rut_x11_motion_event_get_modifier_state(rut_input_event_t *event)
{
    rut_x11_event_t *rut_x11_event = event->native;
    return rut_x11_event->mod_state;
}

static void
rut_x11_motion_event_get_transformed_xy(rut_input_event_t *event,
                                        float *x,
                                        float *y)
{
    rut_x11_event_t *rut_x11_event = event->native;
    XIEvent *xi2event = rut_x11_event->xcookie.data;
    XIDeviceEvent *xi2_dev_event = rut_x11_event->xcookie.data;

    switch (xi2event->evtype) {
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_Motion:
    case XI_TouchBegin:
    case XI_TouchUpdate:
    case XI_TouchEnd:
        *x = xi2_dev_event->event_x;
        *y = xi2_dev_event->event_y;
        break;
    default:
        c_warn_if_reached(); /* Not a motion event */
        return;
    }
}

static const char *
rut_x11_text_event_get_text(rut_input_event_t *event)
{
    rut_x11_event_t *rut_x11_event = event->native;
    return rut_x11_event->text;
}

/* XXX: Currently only handles a single, virtual (core) device */
static void
update_keyboard_state(rut_shell_t *shell)
{
    struct xkb_state *xkb_state;
    struct xkb_keymap *xkb_keymap;

    xkb_keymap = xkb_x11_keymap_new_from_device(shell->xkb_ctx,
                                                shell->xcon,
                                                shell->xkb_core_device_id,
                                                0); /* compile options */
    if (!xkb_keymap)
        goto keymap_error;

    xkb_state = xkb_x11_state_new_from_device(xkb_keymap,
                                              shell->xcon,
                                              shell->xkb_core_device_id);
    if (!xkb_state)
        goto state_error;

    xkb_state_unref(shell->xkb_state);
    xkb_keymap_unref(shell->xkb_keymap);

    shell->xkb_state = xkb_state;
    shell->xkb_keymap = xkb_keymap;

    shell->xkb_mod_index_map[0].mod = RUT_MODIFIER_SHIFT_ON;
    shell->xkb_mod_index_map[0].mod_index =
        xkb_keymap_mod_get_index(xkb_keymap, XKB_MOD_NAME_SHIFT);

    shell->xkb_mod_index_map[1].mod = RUT_MODIFIER_CTRL_ON;
    shell->xkb_mod_index_map[1].mod_index =
        xkb_keymap_mod_get_index(xkb_keymap, XKB_MOD_NAME_CTRL);

    shell->xkb_mod_index_map[2].mod = RUT_MODIFIER_ALT_ON;
    shell->xkb_mod_index_map[2].mod_index =
        xkb_keymap_mod_get_index(xkb_keymap, XKB_MOD_NAME_ALT);

    shell->xkb_mod_index_map[3].mod = RUT_MODIFIER_CAPS_LOCK_ON;
    shell->xkb_mod_index_map[3].mod_index =
        xkb_keymap_mod_get_index(xkb_keymap, XKB_LED_NAME_CAPS);

    shell->xkb_mod_index_map[4].mod = RUT_MODIFIER_NUM_LOCK_ON;
    shell->xkb_mod_index_map[4].mod_index =
        xkb_keymap_mod_get_index(xkb_keymap, XKB_LED_NAME_NUM);

    _Static_assert(RUT_N_MODIFIERS == 5, "x11 shell expects up to 5 modifiers");

    return;

state_error:
    xkb_keymap_unref(xkb_keymap);
keymap_error:
    return;
}

static rut_shell_onscreen_t *
get_onscreen_for_xwindow(rut_shell_t *shell, Window xwindow)
{
    rut_shell_onscreen_t *onscreen;

    c_list_for_each(onscreen, &shell->onscreens, link) {
        if(cg_x11_onscreen_get_window_xid(onscreen->cg_onscreen) == xwindow)
            return onscreen;
    }

    return NULL;
}

static rut_shell_onscreen_t *
get_onscreen_for_xi2_event(rut_shell_t *shell, XIEvent *xi2event)
{
    XIDeviceEvent *xi2_dev_event;
    Window event_xwindow = None;

    switch (xi2event->evtype) {
    case XI_KeyPress:
    case XI_KeyRelease:
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_Motion:
    case XI_TouchBegin:
    case XI_TouchUpdate:
    case XI_TouchEnd:
        xi2_dev_event = (XIDeviceEvent *)xi2event;

        event_xwindow = xi2_dev_event->event;
        break;
    }

    if (!event_xwindow)
        return NULL;

    return get_onscreen_for_xwindow(shell, event_xwindow);
}

static void
append_text_event(rut_shell_t *shell,
                  rut_shell_onscreen_t *onscreen,
                  char *text)
{
    rut_input_event_t *event = c_slice_alloc0(sizeof(rut_input_event_t) +
                                              sizeof(rut_x11_event_t));
    rut_x11_event_t *rut_x11_event;

    event->type = RUT_INPUT_EVENT_TYPE_TEXT;
    event->onscreen = onscreen;
    event->native = event->data;

    rut_x11_event = (void *)event->data;
    rut_x11_event->text = text;

    rut_input_queue_append(shell->input_queue, event);
}

static void
maybe_append_text_for_pressed_keycode(rut_shell_t *shell,
                                      rut_shell_onscreen_t *onscreen,
                                      xkb_keycode_t keycode)
{
    int len;
    char *text;

    if (shell->xkb_compose_state) {
        xkb_keysym_t keysym = xkb_state_key_get_one_sym(shell->xkb_state,
                                                        keycode);
        enum xkb_compose_status status;

        xkb_compose_state_feed(shell->xkb_compose_state, keysym);
        status = xkb_compose_state_get_status(shell->xkb_compose_state);

        switch (status) {
        case XKB_COMPOSE_CANCELLED:
            xkb_compose_state_reset(shell->xkb_compose_state);
            return;
        case XKB_COMPOSE_COMPOSED:
            len = xkb_compose_state_get_utf8(shell->xkb_compose_state,
                                             NULL, 0);
            if (len > 0) {
                text = c_malloc(len + 1);

                xkb_compose_state_get_utf8(shell->xkb_compose_state,
                                           text, len + 1);
                append_text_event(shell, onscreen, text);
            }

            xkb_compose_state_reset(shell->xkb_compose_state);
        case XKB_COMPOSE_COMPOSING:
            return;
        case XKB_COMPOSE_NOTHING:
            break;
        }

        /* If we aren't composing then we want to fall through to
         * the same path as if compose support wasn't available
         * and just see if the keysym corresponds to a printable
         * character */
        c_return_if_fail(status == XKB_COMPOSE_NOTHING);
    }

    len = xkb_state_key_get_utf8(shell->xkb_state, keycode, NULL, 0);
    if (len > 0) {
        text = c_malloc(len + 1);
        xkb_state_key_get_utf8(shell->xkb_state, keycode, text, len + 1);
        append_text_event(shell, onscreen, text);
    }
}

static rut_modifier_state_t
modifier_state_from_xkb_state(rut_shell_t *shell)
{
    struct xkb_state *xkb_state = shell->xkb_state;
    rut_modifier_state_t state = 0;

    for (int i = 0; i < RUT_N_MODIFIERS; i++) {
        if (xkb_state_mod_index_is_active(xkb_state,
                                          shell->xkb_mod_index_map[i].mod_index,
                                          XKB_STATE_MODS_EFFECTIVE))
            state |= shell->xkb_mod_index_map[i].mod;
    }

    return state;
}

static void
set_xkb_modifier_state_from_xi2_dev_event(rut_shell_t *shell,
                                          XIDeviceEvent *xi2_dev_event)
{
    xkb_state_update_mask(shell->xkb_state,
                          xi2_dev_event->mods.base,
                          xi2_dev_event->mods.latched,
                          xi2_dev_event->mods.locked,
                          xi2_dev_event->group.base,
                          xi2_dev_event->group.latched,
                          xi2_dev_event->group.locked);
}

static void
handle_client_message(rut_shell_t *shell, XEvent *xevent)
{
    XClientMessageEvent *msg = &xevent->xclient;
    rut_shell_onscreen_t *onscreen =
        get_onscreen_for_xwindow(shell, msg->window);

    if (!onscreen) {
        c_warning("Ignoring spurious client message that couldn't be mapped to an onscreen window");
        return;
    }

    if (msg->message_type == XInternAtom(shell->xdpy, "WM_PROTOCOLS", False)) {
        Atom protocol = msg->data.l[0];

        if (protocol == XInternAtom(shell->xdpy, "WM_DELETE_WINDOW", False)) {

            /* FIXME: we should eventually support multiple windows and
             * we should be able close windows individually. */
            rut_shell_quit(shell);
        } else if (protocol == XInternAtom(shell->xdpy, "WM_TAKE_FOCUS", False)) {
            XSetInputFocus(shell->xdpy,
                           msg->window,
                           RevertToParent,
                           CurrentTime);
        } else if (protocol == XInternAtom(shell->xdpy, "_NET_WM_PING", False)) {
            msg->window = DefaultRootWindow(shell->xdpy);
            XSendEvent(shell->xdpy, DefaultRootWindow(shell->xdpy), False,
                       SubstructureRedirectMask | SubstructureNotifyMask, xevent);
        } else {
            char *name = XGetAtomName(shell->xdpy, protocol);
            c_warning("Unknown X client WM_PROTOCOLS message recieved (%s)\n",
                      name);
            XFree(name);
        }
    } else {
        char *name = XGetAtomName(shell->xdpy, xevent->xclient.message_type);
        c_warning("Unknown X client message recieved (%s)\n", name);
        XFree(name);
    }
}

static void
handle_property_notify(rut_shell_t *shell, XEvent *xevent)
{
    XPropertyEvent *event = &xevent->xproperty;
    Atom net_wm_state_atom;
    rut_shell_onscreen_t *onscreen =
        get_onscreen_for_xwindow(shell, event->window);

    if (!onscreen)
        return;

    net_wm_state_atom = XInternAtom(shell->xdpy, "_NET_WM_STATE", False);

    if (event->atom == net_wm_state_atom) {
        Atom actual_type;
        int actual_format;
        Atom *atoms;
        unsigned long n_atoms;
        unsigned long remaining;
        unsigned char *data;
        int status;

        status = XGetWindowProperty(shell->xdpy,
                                    event->window,
                                    net_wm_state_atom,
                                    0, /* offset */
                                    LONG_MAX,
                                    False, /* delete */
                                    XA_ATOM, /* expected type */
                                    &actual_type,
                                    &actual_format,
                                    &n_atoms,
                                    &remaining,
                                    &data);

        if (status == Success) {
            Atom net_wm_state_fullscreen =
                XInternAtom(shell->xdpy, "_NET_WM_STATE_FULLSCREEN", False);
            bool fullscreen = false;
            int i;

            c_warn_if_fail(remaining == 0);
            c_warn_if_fail(actual_type == XA_ATOM);
            c_warn_if_fail(actual_format == 32);

            atoms = (Atom *)data;
            for (i = 0; i < n_atoms; i++) {
                if (atoms[i] == net_wm_state_fullscreen)
                    fullscreen = true;
            }

            XFree(data);

            onscreen->fullscreen = fullscreen;
        }
    }
}

void
rut_x11_shell_handle_x11_event(rut_shell_t *shell, XEvent *xevent)
{
    rut_input_event_t *event = NULL;
    rut_x11_event_t *rut_x11_event;

    if (xevent->type == ClientMessage) {
        handle_client_message(shell, xevent);
        return;
    }

    if (xevent->type == PropertyNotify) {
        handle_property_notify(shell, xevent);
        return;
    }

    if (xevent->type == shell->xkb_event) {
        XkbEvent *xkbevent = (void *)xevent;

        switch(xkbevent->any.xkb_type) {
        case XkbNewKeyboardNotify:
            if (xkbevent->new_kbd.changed & XkbNKN_KeycodesMask)
                update_keyboard_state(shell);
        case XkbMapNotify:
            update_keyboard_state(shell);
            break;
        }

        return;
    }

    if (xevent->type == GenericEvent &&
        xevent->xgeneric.extension == shell->xi2_opcode)
    {
        XIEvent *xi2event;
        XIDeviceEvent *xi2_dev_event;

        XGetEventData(shell->xdpy, &xevent->xcookie);

        xi2event = xevent->xcookie.data;
        xi2_dev_event = xevent->xcookie.data;

        switch (xi2event->evtype) {
        case XI_KeyPress:
        case XI_KeyRelease:
            if (xi2_dev_event->deviceid != shell->xkb_core_device_id)
                return;
        case XI_ButtonPress:
        case XI_ButtonRelease:
        case XI_Motion:
        case XI_TouchBegin:
        case XI_TouchUpdate:
        case XI_TouchEnd:

            /* We queue input events to be handled on a per-frame
             * basis instead of dispatching them immediately...
             */

            event = c_slice_alloc0(sizeof(rut_input_event_t) +
                                   sizeof(rut_x11_event_t));
            event->onscreen = get_onscreen_for_xi2_event(shell, xi2event);
            event->native = event->data;

            rut_x11_event = (void *)event->data;
            rut_x11_event->xcookie = xevent->xcookie;

            break;
        default:
            XFreeEventData(shell->xdpy, &xevent->xcookie);
        }
    }

    if (event) {
        /* We can assume from above that this is an XInput2 event... */
        XIEvent *xi2event = xevent->xcookie.data;
        XIDeviceEvent *xi2_dev_event = xevent->xcookie.data;

        switch (xi2event->evtype) {
        case XI_KeyPress:
        case XI_KeyRelease:
            event->type = RUT_INPUT_EVENT_TYPE_KEY;
            break;
        case XI_ButtonPress:
        case XI_ButtonRelease:
        case XI_Motion:
        case XI_TouchBegin:
        case XI_TouchUpdate:
        case XI_TouchEnd:
            event->type = RUT_INPUT_EVENT_TYPE_MOTION;
            break;
        }

        set_xkb_modifier_state_from_xi2_dev_event(shell, xi2_dev_event);

        rut_x11_event->mod_state = modifier_state_from_xkb_state(shell);
        rut_x11_event->keysym = xkb_state_key_get_one_sym(shell->xkb_state,
                                                          xi2_dev_event->detail);

        rut_input_queue_append(shell->input_queue, event);

        if (xi2event->evtype == XI_KeyPress)
            maybe_append_text_for_pressed_keycode(shell,
                                                  event->onscreen,
                                                  xi2_dev_event->detail);

        /* FIXME: we need a separate status so we can trigger a new
         * frame, but if the input doesn't affect anything then we
         * want to avoid any actual rendering. */
        rut_shell_queue_redraw(shell);
    }
}

static void
rut_x11_free_input_event(rut_input_event_t *event)
{
    rut_shell_t *shell = event->onscreen->shell;
    rut_x11_event_t *x11_event = event->native;

    if (x11_event->xcookie.data)
        XFreeEventData(shell->xdpy, &x11_event->xcookie);

    if (x11_event->text)
        c_free(x11_event->text);

    c_slice_free1(sizeof(rut_input_event_t) + sizeof(rut_x11_event_t),
                  event);
}

static int64_t
xlib_prepare_cb (void *user_data)
{
    rut_shell_t *shell = user_data;

    return XPending(shell->xdpy) ? 0 : -1;
}

static void
xlib_dispatch_cb (void *user_data, int fd, int revents)
{
    rut_shell_t *shell = user_data;
    XEvent xevent;

    while (XPending (shell->xdpy)) {
        XNextEvent(shell->xdpy, &xevent);

        rut_x11_shell_handle_x11_event(shell, &xevent);
        cg_xlib_renderer_handle_event(shell->cg_renderer, &xevent);
    }
}

static bool
rut_x11_check_for_hmd(rut_shell_t *shell)
{
    Atom edid_atom = XInternAtom(shell->xdpy, "EDID", False);
    XRRScreenResources *resources;
    unsigned char *prop;
    int actual_format;
    Atom actual_type;
    unsigned long nitems;
    unsigned long bytes_after;
    int i;

    shell->hmd_output_id = -1;

    resources = XRRGetScreenResourcesCurrent(shell->xdpy,
                                             DefaultRootWindow(shell->xdpy));
    if (!resources)
        return false;

    for (i = 0; i < resources->noutput; i++) {
        XRROutputInfo *output = XRRGetOutputInfo(shell->xdpy,
                                                 resources,
                                                 resources->outputs[i]);

        if (output->connection != RR_Disconnected) {
            int output_id = resources->outputs[i];

            XRRGetOutputProperty(shell->xdpy, output_id, edid_atom,
                                 0, 100, False, False,
                                 AnyPropertyType,
                                 &actual_type, &actual_format,
                                 &nitems, &bytes_after, &prop);

            if (actual_type == XA_INTEGER && actual_format == 8) {
                MonitorInfo *info = decode_edid(prop);

                if (strcmp(info->manufacturer_code, "OVR") == 0)
                    shell->hmd_output_id = output_id;

                free(info);
            }
        }

      XRRFreeOutputInfo(output);

      if (shell->hmd_output_id >= 0)
          break;
    }

    return shell->hmd_output_id != -1;
}

static cg_onscreen_t *
rut_x11_allocate_onscreen(rut_shell_onscreen_t *onscreen)
{
    rut_shell_t *shell = onscreen->shell;
    cg_onscreen_t *cg_onscreen;
    cg_error_t *ignore = NULL;
    Window xwin;
    Atom window_type_atom = XInternAtom(shell->xdpy, "_NET_WM_WINDOW_TYPE", False);
    Atom normal_atom = XInternAtom(shell->xdpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    Atom net_wm_pid_atom = XInternAtom(shell->xdpy, "_NET_WM_PID", False);
    Atom wm_protocols[] = {
        XInternAtom(shell->xdpy, "WM_DELETE_WINDOW", False),
        XInternAtom(shell->xdpy, "WM_TAKE_FOCUS", False),
        XInternAtom(shell->xdpy, "_NET_WM_PING", False),
    };
    uint32_t pid;
    XIEventMask evmask;
    XSetWindowAttributes attribs;

    cg_onscreen = cg_onscreen_new(shell->cg_device,
                                  onscreen->width,
                                  onscreen->height);

    if (!cg_framebuffer_allocate(cg_onscreen, &ignore)) {
        cg_error_free(ignore);
        return NULL;
    }

    xwin = cg_x11_onscreen_get_window_xid(cg_onscreen);

    /* XXX: we are only calling this for the convenience that
     * it will set the WM_CLIENT_MACHINE property which is
     * a requirement before we can set _NET_WM_PID */
    XSetWMProperties(shell->xdpy, xwin,
                     NULL, /* window name */
                     NULL, /* icon name */
                     NULL, /* argv */
                     0, /* argc */
                     NULL, /* normal hints */
                     NULL, /* wm hints */
                     NULL); /* class hints */

    XSetWMProtocols(shell->xdpy, xwin, wm_protocols,
                    sizeof(wm_protocols) / sizeof(Atom));

    pid = getpid();
    XChangeProperty(shell->xdpy,
                    xwin,
                    net_wm_pid_atom,
                    XA_CARDINAL,
                    32, /* format */
                    PropModeReplace,
                    (unsigned char *)&pid,
                    1); /* n elements */

    XChangeProperty(shell->xdpy,
                    xwin,
                    window_type_atom,
                    XA_ATOM,
                    32, /* format */
                    PropModeReplace,
                    (unsigned char *)&normal_atom,
                    1); /* n elements */

    attribs.bit_gravity = NorthWestGravity;
    attribs.event_mask = (StructureNotifyMask | ExposureMask | /* needed by cogl */
                          PropertyChangeMask);
    /* Assuming we're creating a top-level window that might not be
     * re-parented by the window manager (e.g. while fullscreen)
     * then we don't want input events falling through to the
     * underlying desktop... */
    attribs.do_not_propagate_mask = (KeyPressMask | KeyReleaseMask |
                                     ButtonPressMask | ButtonReleaseMask |
                                     PointerMotionMask | ButtonMotionMask |
                                     Button1MotionMask | Button2MotionMask |
                                     Button3MotionMask | Button4MotionMask |
                                     Button5MotionMask);
    attribs.background_pixel = BlackPixel(shell->xdpy,
                                          DefaultScreen(shell->xdpy));

    XChangeWindowAttributes(shell->xdpy, xwin,
                            CWBitGravity | CWEventMask | CWDontPropagate | CWBackPixel,
                            &attribs);

    evmask.deviceid = XIAllDevices;
    evmask.mask_len = XIMaskLen(XI_LASTEVENT);
    evmask.mask = c_alloca(XIMaskLen(XI_LASTEVENT));
    memset(evmask.mask, 0, XIMaskLen(XI_LASTEVENT));

    XISetMask(evmask.mask, XI_KeyPress);
    XISetMask(evmask.mask, XI_KeyRelease);
    XISetMask(evmask.mask, XI_ButtonPress);
    XISetMask(evmask.mask, XI_ButtonRelease);
    XISetMask(evmask.mask, XI_Motion);
    XISetMask(evmask.mask, XI_Enter);
    XISetMask(evmask.mask, XI_Leave);

    XISelectEvents(shell->xdpy, xwin, &evmask, 1);

    return cg_onscreen;
}

void
rut_x11_onscreen_resize(rut_shell_onscreen_t *onscreen,
                        int width,
                        int height)
{
    rut_shell_t *shell = onscreen->shell;
    Window xwindow = cg_x11_onscreen_get_window_xid(onscreen->cg_onscreen);

    XResizeWindow(shell->xdpy, xwindow, width, height);
}

static void
rut_x11_onscreen_set_title(rut_shell_onscreen_t *onscreen,
                           const char *title)
{
    rut_shell_t *shell = onscreen->shell;
    Window xwindow = cg_x11_onscreen_get_window_xid(onscreen->cg_onscreen);
    Atom net_wm_name = XInternAtom(shell->xdpy, "_NET_WM_NAME", False);
    Atom utf8_string= XInternAtom(shell->xdpy, "UTF8_STRING", False);

    XStoreName(shell->xdpy, xwindow, title);

    XChangeProperty(shell->xdpy,
                    xwindow,
                    net_wm_name,
                    utf8_string,
                    8, /* format */
                    PropModeReplace,
                    (unsigned char *)title,
                    strlen(title));
}

static void
rut_x11_onscreen_set_cursor(rut_shell_onscreen_t *onscreen,
                            rut_cursor_t cursor)
{
    rut_shell_t *shell = onscreen->shell;
    Window xwindow = cg_x11_onscreen_get_window_xid(onscreen->cg_onscreen);
    unsigned int shape = XC_arrow;
    XColor zero = { .pixel = 0, .red = 0, .green = 0, .blue = 0 };
    Pixmap xpixmap;
    Cursor xcursor;

    switch(cursor) {
    case RUT_CURSOR_DEFAULT:
        XUndefineCursor(shell->xdpy, xwindow);
        return;
    case RUT_CURSOR_INVISIBLE:
        xpixmap = XCreatePixmap(shell->xdpy, xwindow, 1, 1, 1);
        xcursor = XCreatePixmapCursor(shell->xdpy,
                                      xpixmap, /* source */
                                      xpixmap, /* mask */
                                      &zero, /* fg */
                                      &zero, /* bg */
                                      1, 1); /* hotspot */
        XFreePixmap(shell->xdpy, xpixmap);
        XDefineCursor(shell->xdpy, xwindow, xcursor);
        XFreeCursor(shell->xdpy, xcursor);
        return;
    case RUT_CURSOR_ARROW:
        shape = XC_left_ptr;
        break;
    case RUT_CURSOR_IBEAM:
        shape = XC_xterm;
        break;
    case RUT_CURSOR_WAIT:
        shape = XC_watch;
        break;
    case RUT_CURSOR_CROSSHAIR:
        shape = XC_tcross;
        break;
    case RUT_CURSOR_SIZE_WE:
        shape = XC_sb_h_double_arrow;
        break;
    case RUT_CURSOR_SIZE_NS:
        shape = XC_sb_v_double_arrow;
        break;
    }

    xcursor = XCreateFontCursor(shell->xdpy, shape);
    XDefineCursor(shell->xdpy, xwindow, xcursor);
    XFreeCursor(shell->xdpy, xcursor);
}

void
rut_x11_onscreen_set_fullscreen(rut_shell_onscreen_t *onscreen,
                                bool fullscreen)
{
    rut_shell_t *shell = onscreen->shell;
    Window xwindow = cg_x11_onscreen_get_window_xid(onscreen->cg_onscreen);
    Atom net_wm_state_atom = XInternAtom(shell->xdpy, "_NET_WM_STATE", False);
    Atom fullscreen_atom = XInternAtom(shell->xdpy,
                                       "_NET_WM_STATE_FULLSCREEN", False);
    XEvent msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = ClientMessage;
    msg.xclient.window = xwindow;
    msg.xclient.message_type = net_wm_state_atom;
    msg.xclient.format = 32;
    msg.xclient.data.l[0] = fullscreen ? 1 : 0;
    msg.xclient.data.l[1] = fullscreen_atom;
    msg.xclient.data.l[2] = 0;

    XSendEvent(shell->xdpy, DefaultRootWindow(shell->xdpy), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &msg);
}

static void
free_keyboard_maps(rut_shell_t *shell)
{
    xkb_state_unref(shell->xkb_state);
    shell->xkb_state = NULL;
    xkb_keymap_unref(shell->xkb_keymap);
    shell->xkb_keymap = NULL;
}

bool
rut_x11_shell_init(rut_shell_t *shell)
{
    cg_error_t *error = NULL;
    XIEventMask evmask;
    const char *locale;
    struct xkb_compose_table *compose_table;
    Bool detectable_autorepeat_supported;

    shell->cg_renderer = cg_renderer_new();
    cg_xlib_renderer_set_event_retrieval_enabled(shell->cg_renderer, false);

    shell->cg_device = cg_device_new();

    cg_renderer_set_winsys_id(shell->cg_renderer, CG_WINSYS_ID_GLX);
    if (cg_renderer_connect(shell->cg_renderer, &error))
        cg_device_set_renderer(shell->cg_device, shell->cg_renderer);
    else {
        cg_error_free(error);
        c_warning("Failed to setup GLX renderer; "
                  "falling back to default\n");
    }

    cg_device_connect(shell->cg_device, &error);
    if (!shell->cg_device) {
        c_warning("Failed to create Cogl Context: %s", error->message);
        cg_error_free(error);
        goto error;
    }

    shell->xdpy = cg_xlib_renderer_get_display(shell->cg_renderer);

    shell->xcon = XGetXCBConnection(shell->xdpy);

    if (!XQueryExtension(shell->xdpy, "XInputExtension",
                         &shell->xi2_opcode,
                         &shell->xi2_event,
                         &shell->xi2_error))
    {
        c_warning("X11 missing required XInput2 extension");
        goto error;
    }

    shell->xi2_major = 2;
    shell->xi2_minor = 3;
    if (XIQueryVersion(shell->xdpy, &shell->xi2_major, &shell->xi2_minor)
        != Success)
    {
        c_warning("X11 XInput2 extension >= 2.3 required");
        goto error;
    }

    evmask.deviceid = XIAllDevices;
    evmask.mask_len = XIMaskLen(XI_LASTEVENT);
    evmask.mask = c_alloca(XIMaskLen(XI_LASTEVENT));
    memset(evmask.mask, 0, XIMaskLen(XI_LASTEVENT));

    XISetMask(evmask.mask, XI_HierarchyChanged);
    XISetMask(evmask.mask, XI_DeviceChanged);

    XISelectEvents(shell->xdpy, DefaultRootWindow(shell->xdpy), &evmask, 1);

    shell->xkb_major = XkbMajorVersion;
    shell->xkb_minor = XkbMinorVersion;
    if (!XkbQueryExtension(shell->xdpy,
                           &shell->xkb_opcode,
                           &shell->xkb_event,
                           &shell->xkb_error,
                           &shell->xkb_major,
                           &shell->xkb_minor))
    {
        c_warning("Missing XKB extension");
        goto error;
    }

    XkbSetDetectableAutoRepeat(shell->xdpy, True,
                               &detectable_autorepeat_supported);
    c_warn_if_fail(detectable_autorepeat_supported == True);

    shell->xkb_core_device_id = xkb_x11_get_core_keyboard_device_id(shell->xcon);

    shell->xkb_ctx = xkb_context_new(0);

    locale = getenv("LC_ALL");
    if (!locale)
        locale = getenv("LC_CTYPE");
    if (!locale)
        locale = getenv("LANG");
    if (!locale)
        locale = "C";

    compose_table = xkb_compose_table_new_from_locale(shell->xkb_ctx, locale, 0);
    if (compose_table)
        shell->xkb_compose_state = xkb_compose_state_new(compose_table, 0);

    XkbSelectEvents(shell->xdpy,
                    XkbUseCoreKbd, /* device id */
                    XkbNewKeyboardNotifyMask | XkbMapNotifyMask, /* update mask */
                    XkbNewKeyboardNotifyMask | XkbMapNotifyMask); /* values */

    update_keyboard_state(shell);

    rut_poll_shell_add_fd(shell,
                          ConnectionNumber(shell->xdpy),
                          RUT_POLL_FD_EVENT_IN,
                          xlib_prepare_cb,
                          xlib_dispatch_cb,
                          shell);

    shell->platform.type = RUT_SHELL_X11_PLATFORM;

    if (getenv("RIG_USE_HMD")) {
        if (!rut_x11_check_for_hmd(shell)) {
            c_warning("Failed to find a head mounted display");
        }
    }

    shell->platform.check_for_hmd = rut_x11_check_for_hmd;

    shell->platform.allocate_onscreen = rut_x11_allocate_onscreen;
    shell->platform.onscreen_resize = rut_x11_onscreen_resize;
    shell->platform.onscreen_set_title = rut_x11_onscreen_set_title;
    shell->platform.onscreen_set_cursor = rut_x11_onscreen_set_cursor;
    shell->platform.onscreen_set_fullscreen = rut_x11_onscreen_set_fullscreen;

    shell->platform.key_event_get_keysym = rut_x11_key_event_get_keysym;
    shell->platform.key_event_get_action = rut_x11_key_event_get_action;
    shell->platform.key_event_get_modifier_state = rut_x11_key_event_get_modifier_state;

    shell->platform.motion_event_get_action = rut_x11_motion_event_get_action;
    shell->platform.motion_event_get_button = rut_x11_motion_event_get_button;
    shell->platform.motion_event_get_button_state = rut_x11_motion_event_get_button_state;
    shell->platform.motion_event_get_modifier_state = rut_x11_motion_event_get_modifier_state;
    shell->platform.motion_event_get_transformed_xy = rut_x11_motion_event_get_transformed_xy;

    shell->platform.text_event_get_text = rut_x11_text_event_get_text;

    shell->platform.free_input_event = rut_x11_free_input_event;

    return true;

error:

    free_keyboard_maps(shell);

    if (shell->xdpy) {
        rut_poll_shell_remove_fd(shell, ConnectionNumber(shell->xdpy));
        shell->xdpy = NULL;
    }

    if (shell->cg_device) {
        cg_object_unref(shell->cg_device);
        shell->cg_device = NULL;
    }
    if (shell->cg_renderer) {
        cg_object_unref(shell->cg_renderer);
        shell->cg_renderer = NULL;
    }

    c_free(shell);

    return false;
}
